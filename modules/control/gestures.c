/*****************************************************************************
 * gestures.c: control vlc with mouse gestures
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
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
#include <vlc_aout.h>
#include <vlc_playlist.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif

/*****************************************************************************
 * intf_sys_t: description and status of interface
 *****************************************************************************/
struct intf_sys_t
{
    vlc_object_t *      p_vout;
    bool          b_got_gesture;
    bool          b_button_pressed;
    int                 i_mouse_x, i_mouse_y;
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

int  Open   ( vlc_object_t * );
void Close  ( vlc_object_t * );
static int  InitThread     ( intf_thread_t *p_intf );
static void EndThread      ( intf_thread_t *p_intf );
static int  MouseEvent     ( vlc_object_t *, char const *,
                             vlc_value_t, vlc_value_t, void * );

/* Exported functions */
static void RunIntf        ( intf_thread_t *p_intf );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define THRESHOLD_TEXT N_( "Motion threshold (10-100)" )
#define THRESHOLD_LONGTEXT N_( \
    "Amount of movement required for a mouse gesture to be recorded." )

#define BUTTON_TEXT N_( "Trigger button" )
#define BUTTON_LONGTEXT N_( \
    "Trigger button for mouse gestures." )

static const char *const button_list[] = { "left", "middle", "right" };
static const char *const button_list_text[] =
                                   { N_("Left"), N_("Middle"), N_("Right") };

vlc_module_begin ()
    set_shortname( N_("Gestures"))
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    add_integer( "gestures-threshold", 30, NULL,
                 THRESHOLD_TEXT, THRESHOLD_LONGTEXT, true )
    add_string( "gestures-button", "right", NULL,
                BUTTON_TEXT, BUTTON_LONGTEXT, false )
        change_string_list( button_list, button_list_text, 0 )
    set_description( N_("Mouse gestures control interface") )

    set_capability( "interface", 0 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * OpenIntf: initialize interface
 *****************************************************************************/
int Open ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        return( 1 );
    };

    p_intf->pf_run = RunIntf;

    return( 0 );
}

/*****************************************************************************
 * gesture: return a subpattern within a pattern
 *****************************************************************************/
static int gesture( int i_pattern, int i_num )
{
    return ( i_pattern >> ( i_num * 4 ) ) & 0xF;
}

/*****************************************************************************
 * input_from_playlist: don't forget to release the return value
 *  Also this function should really be available from core.
 *****************************************************************************/
static input_thread_t * input_from_playlist ( playlist_t *p_playlist )
{
    return playlist_CurrentInput( p_playlist );
}

/*****************************************************************************
 * CloseIntf: destroy dummy interface
 *****************************************************************************/
void Close ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Destroy structure */
    free( p_intf->p_sys );
}


/*****************************************************************************
 * RunIntf: main loop
 *****************************************************************************/
static void RunIntf( intf_thread_t *p_intf )
{
    playlist_t * p_playlist = NULL;
    int canc = vlc_savecancel();

    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->p_sys->p_vout = NULL;
    vlc_mutex_unlock( &p_intf->change_lock );

    if( InitThread( p_intf ) < 0 )
    {
        msg_Err( p_intf, "can't initialize interface thread" );
        return;
    }
    msg_Dbg( p_intf, "interface thread initialized" );

    /* Main loop */
    while( vlc_object_alive( p_intf ) )
    {
        vlc_mutex_lock( &p_intf->change_lock );

        /*
         * mouse cursor
         */
        if( p_intf->p_sys->b_got_gesture )
        {
            vlc_value_t val;
            int i_interval = 0;
            /* Do something */
            /* If you modify this, please try to follow this convention:
               Start with LEFT, RIGHT for playback related commands
               and UP, DOWN, for other commands */
            switch( p_intf->p_sys->i_pattern )
            {
            case LEFT:
                i_interval = config_GetInt( p_intf , "short-jump-size" );
                if ( i_interval > 0 ) {
                    val.i_time = ( (mtime_t)( -i_interval ) * 1000000L);
                    var_Set( p_intf, "time-offset", val );
                }
                msg_Dbg(p_intf, "Go backward in the movie!");
                break;
            case RIGHT:
                i_interval = config_GetInt( p_intf , "short-jump-size" );
                if ( i_interval > 0 ) {
                    val.i_time = ( (mtime_t)( i_interval ) * 1000000L);
                    var_Set( p_intf, "time-offset", val );
                }
                msg_Dbg(p_intf, "Go forward in the movie!");
                break;
            case GESTURE(LEFT,UP,NONE,NONE):
                /*FIXME BF*/
                msg_Dbg(p_intf, "Going slower.");
                break;
            case GESTURE(RIGHT,UP,NONE,NONE):
                /*FIXME FF*/
                msg_Dbg(p_intf, "Going faster.");
                break;
            case GESTURE(LEFT,RIGHT,NONE,NONE):
            case GESTURE(RIGHT,LEFT,NONE,NONE):
                {
                    input_thread_t * p_input;
                    p_playlist = pl_Hold( p_intf );

                    p_input = input_from_playlist( p_playlist );
                    vlc_object_release( p_playlist );
 
                    if( !p_input )
                        break;
 
                    val.i_int = PLAYING_S;
                    if( p_input )
                    {
                        var_Get( p_input, "state", &val);
                        if( val.i_int == PAUSE_S )
                        {
                            val.i_int = PLAYING_S;
                        }
                        else
                        {
                            val.i_int = PAUSE_S;
                        }
                        var_Set( p_input, "state", val);
                    }
                    msg_Dbg(p_intf, "Play/Pause");
                    vlc_object_release( p_input );
                }
                break;
            case GESTURE(LEFT,DOWN,NONE,NONE):
                p_playlist = pl_Hold( p_intf );

                playlist_Prev( p_playlist );
                vlc_object_release( p_playlist );
                break;
            case GESTURE(RIGHT,DOWN,NONE,NONE):
                p_playlist = pl_Hold( p_intf );

                playlist_Next( p_playlist );
                vlc_object_release( p_playlist );
                break;
            case UP:
                {
                    audio_volume_t i_newvol;
                    aout_VolumeUp( p_intf, 1, &i_newvol );
                    msg_Dbg(p_intf, "Louder");
                }
                break;
            case DOWN:
                {
                    audio_volume_t i_newvol;
                    aout_VolumeDown( p_intf, 1, &i_newvol );
                    msg_Dbg(p_intf, "Quieter");
                }
                break;
            case GESTURE(UP,DOWN,NONE,NONE):
            case GESTURE(DOWN,UP,NONE,NONE):
                {
                    audio_volume_t i_newvol = -1;
                    aout_VolumeMute( p_intf, &i_newvol );
                    msg_Dbg(p_intf, "Mute sound");
                }
                break;
            case GESTURE(UP,RIGHT,NONE,NONE):
                {
                   input_thread_t * p_input;
                   vlc_value_t val, list, list2;
                   int i_count, i;

                    p_playlist = pl_Hold( p_intf );

                    p_input = input_from_playlist( p_playlist );

                    vlc_object_release( p_playlist );

                    if( !p_input )
                        break;

                   var_Get( p_input, "audio-es", &val );
                   var_Change( p_input, "audio-es", VLC_VAR_GETCHOICES,
                               &list, &list2 );
                   i_count = list.p_list->i_count;
                   if( i_count <= 1 )
                   {
                       vlc_object_release( p_input );
                       break;
                   }
                   for( i = 0; i < i_count; i++ )
                   {
                       if( val.i_int == list.p_list->p_values[i].i_int )
                       {
                           break;
                       }
                   }
                   /* value of audio-es was not in choices list */
                   if( i == i_count )
                   {
                       msg_Warn( p_input,
                               "invalid current audio track, selecting 0" );
                       var_Set( p_input, "audio-es",
                               list.p_list->p_values[0] );
                       i = 0;
                   }
                   else if( i == i_count - 1 )
                   {
                       var_Set( p_input, "audio-es",
                               list.p_list->p_values[1] );
                       i = 1;
                   }
                   else
                   {
                       var_Set( p_input, "audio-es",
                               list.p_list->p_values[i+1] );
                       i++;
                   }
                   vlc_object_release( p_input );
                }
                break;
            case GESTURE(DOWN,RIGHT,NONE,NONE):
                {
                    input_thread_t * p_input;
                    vlc_value_t val, list, list2;
                    int i_count, i;

                    p_playlist = pl_Hold( p_intf );

                    p_input = input_from_playlist( p_playlist );
                    vlc_object_release( p_playlist );

                    if( !p_input )
                        break;

                    var_Get( p_input, "spu-es", &val );

                    var_Change( p_input, "spu-es", VLC_VAR_GETCHOICES,
                            &list, &list2 );
                    i_count = list.p_list->i_count;
                    if( i_count <= 1 )
                    {
                        vlc_object_release( p_input );
                        break;
                    }
                    for( i = 0; i < i_count; i++ )
                    {
                        if( val.i_int == list.p_list->p_values[i].i_int )
                        {
                            break;
                        }
                    }
                    /* value of spu-es was not in choices list */
                    if( i == i_count )
                    {
                        msg_Warn( p_input,
                                "invalid current subtitle track, selecting 0" );
                        var_Set( p_input, "spu-es", list.p_list->p_values[0] );
                        i = 0;
                    }
                    else if( i == i_count - 1 )
                    {
                        var_Set( p_input, "spu-es", list.p_list->p_values[0] );
                        i = 0;
                    }
                    else
                    {
                        var_Set( p_input, "spu-es",
                                list.p_list->p_values[i+1] );
                        i = i + 1;
                    }
                    vlc_object_release( p_input );
                }
                break;
            case GESTURE(UP,LEFT,NONE,NONE):
                if (p_intf->p_sys->p_vout )
                {
                    ((vout_thread_t *)p_intf->p_sys->p_vout)->i_changes |=
                        VOUT_FULLSCREEN_CHANGE;
                }
                break;
            case GESTURE(DOWN,LEFT,NONE,NONE):
                /* FIXME: Should close the vout!"*/
                libvlc_Quit( p_intf->p_libvlc );
                break;
            case GESTURE(DOWN,LEFT,UP,RIGHT):
            case GESTURE(UP,RIGHT,DOWN,LEFT):
                msg_Dbg(p_intf, "a square was drawn!" );
                break;
            default:
                break;
            }
            p_intf->p_sys->i_num_gestures = 0;
            p_intf->p_sys->i_pattern = 0;
            p_intf->p_sys->b_got_gesture = false;
        }

        /*
         * video output
         */
        if( p_intf->p_sys->p_vout && !vlc_object_alive (p_intf->p_sys->p_vout) )
        {
            var_DelCallback( p_intf->p_sys->p_vout, "mouse-moved",
                             MouseEvent, p_intf );
            var_DelCallback( p_intf->p_sys->p_vout, "mouse-button-down",
                             MouseEvent, p_intf );
            vlc_object_release( p_intf->p_sys->p_vout );
            p_intf->p_sys->p_vout = NULL;
        }

        if( p_intf->p_sys->p_vout == NULL )
        {
            p_intf->p_sys->p_vout = vlc_object_find( p_intf,
                                      VLC_OBJECT_VOUT, FIND_ANYWHERE );
            if( p_intf->p_sys->p_vout )
            {
                var_AddCallback( p_intf->p_sys->p_vout, "mouse-moved",
                                 MouseEvent, p_intf );
                var_AddCallback( p_intf->p_sys->p_vout, "mouse-button-down",
                                 MouseEvent, p_intf );
            }
        }

        vlc_mutex_unlock( &p_intf->change_lock );

        /* Wait a bit */
        msleep( INTF_IDLE_SLEEP );
    }

    EndThread( p_intf );
    vlc_restorecancel( canc );
}

/*****************************************************************************
 * InitThread:
 *****************************************************************************/
static int InitThread( intf_thread_t * p_intf )
{
    char *psz_button;
    /* we might need some locking here */
    if( vlc_object_alive( p_intf ) )
    {
        /* p_intf->change_lock locking strategy:
         * - Every access to p_intf->p_sys are locked threw p_intf->change_lock
         * - make sure there won't be  cross increment/decrement ref count
         *   of p_intf->p_sys members p_intf->change_lock should be locked
         *   during those operations */
        vlc_mutex_lock( &p_intf->change_lock );

        p_intf->p_sys->b_got_gesture = false;
        p_intf->p_sys->b_button_pressed = false;
        p_intf->p_sys->i_threshold =
                     config_GetInt( p_intf, "gestures-threshold" );
        psz_button = config_GetPsz( p_intf, "gestures-button" );
        if ( !strcmp( psz_button, "left" ) )
        {
            p_intf->p_sys->i_button_mask = 1;
        }
        else if ( !strcmp( psz_button, "middle" ) )
        {
            p_intf->p_sys->i_button_mask = 2;
        }
        else if ( !strcmp( psz_button, "right" ) )
        {
            p_intf->p_sys->i_button_mask = 4;
        }
        free( psz_button );

        p_intf->p_sys->i_pattern = 0;
        p_intf->p_sys->i_num_gestures = 0;
        vlc_mutex_unlock( &p_intf->change_lock );

        return 0;
    }
    else
    {
        return -1;
    }
}

/*****************************************************************************
 * EndThread:
 *****************************************************************************/
static void EndThread( intf_thread_t * p_intf )
{
    vlc_mutex_lock( &p_intf->change_lock );

    if( p_intf->p_sys->p_vout )
    {
        var_DelCallback( p_intf->p_sys->p_vout, "mouse-moved",
                         MouseEvent, p_intf );
        var_DelCallback( p_intf->p_sys->p_vout, "mouse-button-down",
                         MouseEvent, p_intf );
        vlc_object_release( p_intf->p_sys->p_vout );
    }

    vlc_mutex_unlock( &p_intf->change_lock );
}

/*****************************************************************************
 * MouseEvent: callback for mouse events
 *****************************************************************************/
static int MouseEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);
    vlc_value_t val;
    int pattern = 0;

    signed int i_horizontal, i_vertical;
    intf_thread_t *p_intf = (intf_thread_t *)p_data;

    vlc_mutex_lock( &p_intf->change_lock );

    /* don't process new gestures before the last events are processed */
    if( p_intf->p_sys->b_got_gesture )
    {
        vlc_mutex_unlock( &p_intf->change_lock );
        return VLC_SUCCESS;
    }

    if( !strcmp(psz_var, "mouse-moved" ) && p_intf->p_sys->b_button_pressed )
    {
        var_Get( p_intf->p_sys->p_vout, "mouse-x", &val );
        p_intf->p_sys->i_mouse_x = val.i_int;
        var_Get( p_intf->p_sys->p_vout, "mouse-y", &val );
        p_intf->p_sys->i_mouse_y = val.i_int;
        i_horizontal = p_intf->p_sys->i_mouse_x -
            p_intf->p_sys->i_last_x;
        i_horizontal = i_horizontal / p_intf->p_sys->i_threshold;
        i_vertical = p_intf->p_sys->i_mouse_y
            - p_intf->p_sys->i_last_y;
        i_vertical = i_vertical / p_intf->p_sys->i_threshold;

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
            p_intf->p_sys->i_last_y = p_intf->p_sys->i_mouse_y;
            p_intf->p_sys->i_last_x = p_intf->p_sys->i_mouse_x;
            if( gesture( p_intf->p_sys->i_pattern,
                         p_intf->p_sys->i_num_gestures - 1 ) != pattern )
            {
                p_intf->p_sys->i_pattern |=
                    pattern << ( p_intf->p_sys->i_num_gestures * 4 );
                p_intf->p_sys->i_num_gestures++;
            }
        }

    }
    if( !strcmp( psz_var, "mouse-button-down" )
        && newval.i_int & p_intf->p_sys->i_button_mask
        && !p_intf->p_sys->b_button_pressed )
    {
        p_intf->p_sys->b_button_pressed = true;
        var_Get( p_intf->p_sys->p_vout, "mouse-x", &val );
        p_intf->p_sys->i_last_x = val.i_int;
        var_Get( p_intf->p_sys->p_vout, "mouse-y", &val );
        p_intf->p_sys->i_last_y = val.i_int;
    }
    if( !strcmp( psz_var, "mouse-button-down" )
        && !( newval.i_int & p_intf->p_sys->i_button_mask )
        && p_intf->p_sys->b_button_pressed )
    {
        p_intf->p_sys->b_button_pressed = false;
        p_intf->p_sys->b_got_gesture = true;
    }

    vlc_mutex_unlock( &p_intf->change_lock );

    return VLC_SUCCESS;
}
