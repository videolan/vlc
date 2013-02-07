/*****************************************************************************
 * gestures.c: control vlc with mouse gestures
 *****************************************************************************
 * Copyright (C) 2004-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_vout.h>
#include <vlc_playlist.h>
#include <assert.h>

/*****************************************************************************
 * intf_sys_t: description and status of interface
 *****************************************************************************/
struct intf_sys_t
{
    vlc_mutex_t         lock;
    input_thread_t     *p_input;
    vout_thread_t      *p_vout;
    bool                b_button_pressed;
    int                 i_last_x, i_last_y;
    unsigned int        i_pattern;
    int                 i_num_gestures;
    int                 i_threshold;
    int                 i_button_mask;
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
#define UP 1
#define DOWN 2
#define LEFT 3
#define RIGHT 4
#define NONE 0
#define GESTURE( a, b, c, d ) (a | ( b << 4 ) | ( c << 8 ) | ( d << 12 ))

static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define THRESHOLD_TEXT N_( "Motion threshold (10-100)" )
#define THRESHOLD_LONGTEXT N_( \
    "Amount of movement required for a mouse gesture to be recorded." )

#define BUTTON_TEXT N_( "Trigger button" )
#define BUTTON_LONGTEXT N_( \
    "Trigger button for mouse gestures." )

#define BUTTON_DEFAULT "left"

static const char *const button_list[] = { "left", "middle", "right" };
static const char *const button_list_text[] =
                                   { N_("Left"), N_("Middle"), N_("Right") };

vlc_module_begin ()
    set_shortname( N_("Gestures"))
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    add_integer( "gestures-threshold", 30,
                 THRESHOLD_TEXT, THRESHOLD_LONGTEXT, true )
    add_string( "gestures-button", BUTTON_DEFAULT,
                BUTTON_TEXT, BUTTON_LONGTEXT, false )
        change_string_list( button_list, button_list_text )
    set_description( N_("Mouse gestures control interface") )

    set_capability( "interface", 0 )
    set_callbacks( Open, Close )
vlc_module_end ()

static int PlaylistEvent( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );
static int InputEvent( vlc_object_t *, char const *,
                       vlc_value_t, vlc_value_t, void * );
static int MovedEvent( vlc_object_t *, char const *,
                       vlc_value_t, vlc_value_t, void * );
static int ButtonEvent( vlc_object_t *, char const *,
                        vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * OpenIntf: initialize interface
 *****************************************************************************/
static int Open ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Allocate instance and initialize some members */
    intf_sys_t *p_sys = p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( unlikely(p_sys == NULL) )
        return VLC_ENOMEM;

    // Configure the module
    vlc_mutex_init( &p_sys->lock );
    p_sys->p_input = NULL;
    p_sys->p_vout = NULL;
    p_sys->b_button_pressed = false;
    p_sys->i_threshold = var_InheritInteger( p_intf, "gestures-threshold" );

    // Choose the tight button to use
    char *psz_button = var_InheritString( p_intf, "gestures-button" );
    if( psz_button && !strcmp( psz_button, "left" ) )
        p_sys->i_button_mask = 1;
    else if( psz_button && !strcmp( psz_button, "middle" ) )
        p_sys->i_button_mask = 2;
    else // psz_button == "right"
        p_sys->i_button_mask = 4;
    free( psz_button );

    p_sys->i_pattern = 0;
    p_sys->i_num_gestures = 0;

    var_AddCallback( pl_Get(p_intf), "input-current", PlaylistEvent, p_intf );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * gesture: return a subpattern within a pattern
 *****************************************************************************/
static int gesture( int i_pattern, int i_num )
{
    return ( i_pattern >> ( i_num * 4 ) ) & 0xF;
}

/*****************************************************************************
 * CloseIntf: destroy dummy interface
 *****************************************************************************/
static void Close ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    /* Destroy the callbacks (the order matters!) */
    var_DelCallback( pl_Get(p_intf), "input-current", PlaylistEvent, p_intf );

    if( p_sys->p_input )
    {
        var_DelCallback( p_sys->p_input, "intf-event", InputEvent, p_intf );
        vlc_object_release( p_sys->p_input );
    }

    if( p_sys->p_vout )
    {
        var_DelCallback( p_sys->p_vout, "mouse-moved", MovedEvent, p_intf );
        var_DelCallback( p_sys->p_vout, "mouse-button-down",
                         ButtonEvent, p_intf );
        vlc_object_release( p_sys->p_vout );
    }

    /* Destroy structure */
    vlc_mutex_destroy( &p_sys->lock );
    free( p_sys );
}

static void ProcessGesture( intf_thread_t *p_intf )
{
    intf_sys_t *p_sys = p_intf->p_sys;
    playlist_t *p_playlist = pl_Get( p_intf );

    /* Do something */
    /* If you modify this, please try to follow this convention:
       Start with LEFT, RIGHT for playback related commands
       and UP, DOWN, for other commands */
    switch( p_sys->i_pattern )
    {
        case LEFT:
        {
            msg_Dbg( p_intf, "Go backward in the movie!" );

            input_thread_t *p_input = playlist_CurrentInput( p_playlist );
            if( p_input == NULL )
                break;

            int it = var_InheritInteger( p_intf , "short-jump-size" );
            if( it > 0 )
                var_SetTime( p_input, "time-offset", -CLOCK_FREQ * it );
            vlc_object_release( p_input );
            break;
        }

        case RIGHT:
        {
            msg_Dbg( p_intf, "Go forward in the movie!" );

            input_thread_t *p_input = playlist_CurrentInput( p_playlist );
            if( p_input == NULL )
                break;

            int it = var_InheritInteger( p_intf , "short-jump-size" );
            if( it > 0 )
                var_SetTime( p_input, "time-offset", CLOCK_FREQ * it );
            vlc_object_release( p_input );
            break;
        }

        case GESTURE(LEFT,UP,NONE,NONE):
            msg_Dbg( p_intf, "Going slower." );
            var_TriggerCallback( p_playlist, "rate-slower" );
            break;

        case GESTURE(RIGHT,UP,NONE,NONE):
            msg_Dbg( p_intf, "Going faster." );
            var_TriggerCallback( p_playlist, "rate-faster" );
            break;

        case GESTURE(LEFT,RIGHT,NONE,NONE):
        case GESTURE(RIGHT,LEFT,NONE,NONE):
        {
            msg_Dbg( p_intf, "Play/Pause" );

            input_thread_t *p_input = playlist_CurrentInput( p_playlist );
            if( p_input == NULL )
                break;

            int i_state = var_GetInteger( p_input, "state" );
            i_state = (i_state == PLAYING_S) ? PAUSE_S : PLAYING_S;
            var_SetInteger( p_input, "state", i_state );
            vlc_object_release( p_input );
            break;
        }

        case GESTURE(LEFT,DOWN,NONE,NONE):
            playlist_Prev( p_playlist );
            break;

        case GESTURE(RIGHT,DOWN,NONE,NONE):
            playlist_Next( p_playlist );
            break;

        case UP:
            msg_Dbg(p_intf, "Louder");
            playlist_VolumeUp( p_playlist, 1, NULL );
            break;

        case DOWN:
            msg_Dbg(p_intf, "Quieter");
            playlist_VolumeDown( p_playlist, 1, NULL );
            break;

        case GESTURE(UP,DOWN,NONE,NONE):
        case GESTURE(DOWN,UP,NONE,NONE):
            msg_Dbg( p_intf, "Mute sound" );
            playlist_MuteToggle( p_playlist );
            break;

        case GESTURE(UP,RIGHT,NONE,NONE):
        {
            input_thread_t *p_input = playlist_CurrentInput( p_playlist );
            if( p_input == NULL )
                break;

            vlc_value_t list, list2;
            var_Change( p_input, "audio-es", VLC_VAR_GETCHOICES,
                        &list, &list2 );

            if( list.p_list->i_count > 1 )
            {
                int i_audio_es = var_GetInteger( p_input, "audio-es" );
                int i;

                for( i = 0; i < list.p_list->i_count; i++ )
                     if( i_audio_es == list.p_list->p_values[i].i_int )
                         break;
                /* value of audio-es was not in choices list */
                if( i == list.p_list->i_count )
                {
                    msg_Warn( p_input,
                              "invalid current audio track, selecting 0" );
                    i = 0;
                }
                else if( i == list.p_list->i_count - 1 )
                    i = 1;
                else
                    i++;
                var_SetInteger( p_input, "audio-es",
                                list.p_list->p_values[i].i_int );
            }
            var_FreeList( &list, &list2 );
            vlc_object_release( p_input );
            break;
        }

        case GESTURE(DOWN,RIGHT,NONE,NONE):
        {
            input_thread_t *p_input = playlist_CurrentInput( p_playlist );
            if( p_input == NULL )
                break;

            vlc_value_t list, list2;
            var_Change( p_input, "spu-es", VLC_VAR_GETCHOICES,
                        &list, &list2 );

            if( list.p_list->i_count > 1 )
            {
                int i_audio_es = var_GetInteger( p_input, "spu-es" );
                int i;

                for( i = 0; i < list.p_list->i_count; i++ )
                     if( i_audio_es == list.p_list->p_values[i].i_int )
                         break;
                /* value of audio-es was not in choices list */
                if( i == list.p_list->i_count )
                {
                    msg_Warn( p_input,
                              "invalid current subtitle track, selecting 0" );
                    i = 0;
                }
                else if( i == list.p_list->i_count - 1 )
                    i = 1;
                else
                    i++;
                var_SetInteger( p_input, "audio-es",
                                list.p_list->p_values[i].i_int );
            }
            var_FreeList( &list, &list2 );
            vlc_object_release( p_input );
            break;
        }

        case GESTURE(UP,LEFT,NONE,NONE):
        {
            bool val = var_ToggleBool( pl_Get( p_intf ), "fullscreen" );
            if( p_sys->p_vout )
                var_SetBool( p_sys->p_vout, "fullscreen", val );
            break;
        }

        case GESTURE(DOWN,LEFT,NONE,NONE):
            /* FIXME: Should close the vout!"*/
            libvlc_Quit( p_intf->p_libvlc );
            break;

        case GESTURE(DOWN,LEFT,UP,RIGHT):
        case GESTURE(UP,RIGHT,DOWN,LEFT):
            msg_Dbg( p_intf, "a square was drawn!" );
            break;
    }

    p_sys->i_num_gestures = 0;
    p_sys->i_pattern = 0;
}

static int MovedEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_data;
    intf_sys_t    *p_sys = p_intf->p_sys;

    (void) p_this; (void) psz_var; (void) oldval;

    vlc_mutex_lock( &p_sys->lock );
    if( p_sys->b_button_pressed )
    {
        int i_horizontal = newval.coords.x - p_sys->i_last_x;
        int i_vertical = newval.coords.y - p_sys->i_last_y;
        int pattern = 0;

        i_horizontal = i_horizontal / p_sys->i_threshold;
        i_vertical = i_vertical / p_sys->i_threshold;

        if( i_horizontal < 0 )
        {
            msg_Dbg( p_intf, "left gesture (%d)", i_horizontal );
            pattern = LEFT;
        }
        else if( i_horizontal > 0 )
        {
            msg_Dbg( p_intf, "right gesture (%d)", i_horizontal );
            pattern = RIGHT;
        }
        if( i_vertical < 0 )
        {
            msg_Dbg( p_intf, "up gesture (%d)", i_vertical );
            pattern = UP;
        }
        else if( i_vertical > 0 )
        {
            msg_Dbg( p_intf, "down gesture (%d)", i_vertical );
            pattern = DOWN;
        }

        if( pattern )
        {
            p_sys->i_last_x = newval.coords.x;
            p_sys->i_last_y = newval.coords.y;
            if( gesture( p_sys->i_pattern, p_sys->i_num_gestures - 1 )
                    != pattern )
            {
                p_sys->i_pattern |= pattern << ( p_sys->i_num_gestures * 4 );
                p_sys->i_num_gestures++;
            }
        }

    }
    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}

static int ButtonEvent( vlc_object_t *p_this, char const *psz_var,
                        vlc_value_t oldval, vlc_value_t val, void *p_data )
{
    intf_thread_t *p_intf = p_data;
    intf_sys_t *p_sys = p_intf->p_sys;

    (void) psz_var; (void) oldval;

    vlc_mutex_lock( &p_sys->lock );
    if( val.i_int & p_sys->i_button_mask )
    {
        if( !p_sys->b_button_pressed )
        {
            p_sys->b_button_pressed = true;
            var_GetCoords( p_this, "mouse-moved",
                           &p_sys->i_last_x, &p_sys->i_last_y );
        }
    }
    else
    {
        if( p_sys->b_button_pressed )
        {
            p_sys->b_button_pressed = false;
            ProcessGesture( p_intf );
        }
    }
    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}

static int InputEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t val, void *p_data )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    intf_thread_t *p_intf = p_data;
    intf_sys_t *p_sys = p_intf->p_sys;

    (void) psz_var; (void) oldval;

    switch( val.i_int )
    {
      case INPUT_EVENT_DEAD:
        vlc_object_release( p_input );
        p_sys->p_input = NULL; /* FIXME: locking!! */
        break;

      case INPUT_EVENT_VOUT:
        /* intf-event is serialized against itself and is the sole user of
         * p_sys->p_vout. So there is no need to acquire the lock currently. */
        if( p_sys->p_vout != NULL )
        {   /* /!\ Beware of lock inversion with var_DelCallback() /!\ */
            var_DelCallback( p_sys->p_vout, "mouse-moved", MovedEvent,
                             p_intf );
            var_DelCallback( p_sys->p_vout, "mouse-button-down", ButtonEvent,
                             p_intf );
            vlc_object_release( p_sys->p_vout );
        }

        p_sys->p_vout = input_GetVout( p_input );
        if( p_sys->p_vout != NULL )
        {
            var_AddCallback( p_sys->p_vout, "mouse-moved", MovedEvent,
                             p_intf );
            var_AddCallback( p_sys->p_vout, "mouse-button-down", ButtonEvent,
                             p_intf );
        }
        break;
    }
    return VLC_SUCCESS;
}

static int PlaylistEvent( vlc_object_t *p_this, char const *psz_var,
                          vlc_value_t oldval, vlc_value_t val, void *p_data )
{
    intf_thread_t *p_intf = p_data;
    intf_sys_t *p_sys = p_intf->p_sys;
    input_thread_t *p_input = val.p_address;

    (void) p_this; (void) psz_var; (void) oldval;

    var_AddCallback( p_input, "intf-event", InputEvent, p_intf );
    assert( p_sys->p_input == NULL );
    p_sys->p_input = vlc_object_hold( p_input );
    return VLC_SUCCESS;
}
