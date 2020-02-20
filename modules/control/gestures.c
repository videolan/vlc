/*****************************************************************************
 * gestures.c: control vlc with mouse gestures
 *****************************************************************************
 * Copyright (C) 2004-2009 the VideoLAN team
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

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_vout.h>
#include <vlc_player.h>
#include <vlc_playlist.h>
#include <vlc_vector.h>
#include <assert.h>

/*****************************************************************************
 * intf_sys_t: description and status of interface
 *****************************************************************************/

typedef struct VLC_VECTOR(vout_thread_t *) vout_vector;
struct intf_sys_t
{
    vlc_playlist_t         *playlist;
    vlc_player_listener_id *player_listener;
    vlc_mutex_t             lock;
    vout_vector             vout_vector;
    bool                    b_button_pressed;
    int                     i_last_x, i_last_y;
    unsigned int            i_pattern;
    unsigned int            i_num_gestures;
    int                     i_threshold;
    int                     i_button_mask;
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

static void player_on_vout_changed(vlc_player_t *player,
                                   enum vlc_player_vout_action action,
                                   vout_thread_t *vout,
                                   enum vlc_vout_order order,
                                   vlc_es_id_t *es_id,
                                   void *data);
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
    p_sys->playlist = vlc_intf_GetMainPlaylist(p_intf);
    vlc_mutex_init( &p_sys->lock );
    vlc_vector_init(&p_sys->vout_vector);

    static const struct vlc_player_cbs cbs =
    {
        .on_vout_changed = player_on_vout_changed,
    };
    vlc_player_t *player = vlc_playlist_GetPlayer(p_sys->playlist);
    vlc_player_Lock(player);
    p_sys->player_listener = vlc_player_AddListener(player, &cbs, p_intf);
    vlc_player_Unlock(player);
    if (!p_sys->player_listener)
        goto error;

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

    return VLC_SUCCESS;

error:
    vlc_vector_clear(&p_sys->vout_vector);
    free(p_sys);
    return VLC_EGENERIC;
}

/*****************************************************************************
 * gesture: return a subpattern within a pattern
 *****************************************************************************/
static inline unsigned gesture( unsigned i_pattern, unsigned i_num )
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

    vlc_player_t *player = vlc_playlist_GetPlayer(p_sys->playlist);
    vlc_player_Lock(player);
    vlc_player_RemoveListener(player, p_sys->player_listener);
    vlc_player_Unlock(player);

    /* Destroy the callbacks (the order matters!) */
    vout_thread_t *vout;
    vlc_vector_foreach(vout, &p_sys->vout_vector)
    {
        var_DelCallback(vout, "mouse-moved", MovedEvent, p_intf);
        var_DelCallback(vout, "mouse-button-down", ButtonEvent, p_intf);
        vout_Release(vout);
    }
    vlc_vector_clear(&p_sys->vout_vector);

    /* Destroy structure */
    free( p_sys );
}

static void ProcessGesture( intf_thread_t *p_intf )
{
    intf_sys_t *p_sys = p_intf->p_sys;
    vlc_playlist_t *playlist = p_sys->playlist;
    vlc_player_t *player = vlc_playlist_GetPlayer(playlist);

    /* Do something */
    /* If you modify this, please try to follow this convention:
       Start with LEFT, RIGHT for playback related commands
       and UP, DOWN, for other commands */
    vlc_playlist_Lock(playlist);
    switch( p_sys->i_pattern )
    {
        case LEFT:
        case RIGHT:
        {
            msg_Dbg( p_intf, "Go %s in the movie!",
                     p_sys->i_pattern == LEFT ? "backward" : "forward" );
            int it = var_InheritInteger( p_intf , "short-jump-size" );
            if( it > 0 )
            {
                vlc_tick_t jump = p_sys->i_pattern == LEFT ? -it : it;
                vlc_player_JumpTime(player, vlc_tick_from_sec(jump));
            }
            break;
        }

        case GESTURE(LEFT,UP,NONE,NONE):
            msg_Dbg( p_intf, "Going slower." );
            vlc_player_DecrementRate(player);
            break;

        case GESTURE(RIGHT,UP,NONE,NONE):
            msg_Dbg( p_intf, "Going faster." );
            vlc_player_IncrementRate(player);
            break;

        case GESTURE(LEFT,RIGHT,NONE,NONE):
        case GESTURE(RIGHT,LEFT,NONE,NONE):
        {
            msg_Dbg( p_intf, "Play/Pause" );
            vlc_player_TogglePause(player);
            break;
        }

        case GESTURE(LEFT,DOWN,NONE,NONE):
            vlc_playlist_Prev(playlist);
            break;

        case GESTURE(RIGHT,DOWN,NONE,NONE):
            vlc_playlist_Next(playlist);
            break;

        case UP:
            msg_Dbg(p_intf, "Louder");
            vlc_player_aout_IncrementVolume(player, 1, NULL);
            break;

        case DOWN:
            msg_Dbg(p_intf, "Quieter");
            vlc_player_aout_DecrementVolume(player, 1, NULL);
            break;

        case GESTURE(UP,DOWN,NONE,NONE):
        case GESTURE(DOWN,UP,NONE,NONE):
            msg_Dbg( p_intf, "Mute sound" );
            vlc_player_aout_ToggleMute(player);
            break;

        case GESTURE(UP,RIGHT,NONE,NONE):
        case GESTURE(DOWN,RIGHT,NONE,NONE):
        {
            enum es_format_category_e cat =
                p_sys->i_pattern == GESTURE(UP,RIGHT,NONE,NONE) ?
                AUDIO_ES : SPU_ES;
            vlc_player_SelectNextTrack(player, cat);
            break;
        }

        case GESTURE(UP,LEFT,NONE,NONE):
        {
            vlc_player_vout_ToggleFullscreen(player);
            break;
        }

        case GESTURE(DOWN,LEFT,NONE,NONE):
            vlc_playlist_Unlock(playlist);
            /* FIXME: Should close the vout!"*/
            libvlc_Quit( vlc_object_instance(p_intf) );
            break;

        case GESTURE(DOWN,LEFT,UP,RIGHT):
        case GESTURE(UP,RIGHT,DOWN,LEFT):
            msg_Dbg( p_intf, "a square was drawn!" );
            break;
    }
    vlc_playlist_Unlock(playlist);

    p_sys->i_num_gestures = 0;
    p_sys->i_pattern = 0;
}

static int MovedEvent(vlc_object_t *this, char const *psz_var,
                      vlc_value_t oldval, vlc_value_t newval, void *data)
{
    VLC_UNUSED(this); VLC_UNUSED(psz_var); VLC_UNUSED(oldval);

    intf_thread_t *intf = data;
    intf_sys_t *sys = intf->p_sys;

    vlc_mutex_lock(&sys->lock);
    if (sys->b_button_pressed)
    {
        unsigned int pattern = 0;
        int xdelta = newval.coords.x - sys->i_last_x;
        xdelta /= sys->i_threshold;
        int ydelta = newval.coords.y - sys->i_last_y;
        ydelta /= sys->i_threshold;

        char const *dir;
        unsigned int delta;
        if (abs(xdelta) > abs(ydelta))
        {
            pattern = xdelta < 0 ? LEFT : RIGHT;
            dir = xdelta < 0 ? "left" : "right";
            delta = abs(xdelta);
        }
        else if (abs(ydelta) > 0)
        {
            pattern = ydelta < 0 ? UP : DOWN;
            dir = ydelta < 0 ? "up" : "down";
            delta = abs(ydelta);
        }

        if (pattern)
        {
            sys->i_last_x = newval.coords.x;
            sys->i_last_y = newval.coords.y;
            if (sys->i_num_gestures > 0 &&
                gesture(sys->i_pattern, sys->i_num_gestures - 1) != pattern)
            {
                sys->i_pattern |= pattern << (sys->i_num_gestures * 4);
                sys->i_num_gestures++;
            }
            else if (sys->i_num_gestures == 0)
            {
                sys->i_pattern = pattern;
                sys->i_num_gestures = 1;
            }
            msg_Dbg(intf, "%s gesture (%u)", dir, delta);
        }
    }
    vlc_mutex_unlock(&sys->lock);
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

static void
player_on_vout_changed(vlc_player_t *player,
                       enum vlc_player_vout_action action,
                       vout_thread_t *vout,
                       enum vlc_vout_order order,
                       vlc_es_id_t *es_id, void *data)
{
    VLC_UNUSED(player); VLC_UNUSED(order);
    intf_thread_t *intf = data;
    intf_sys_t *sys = intf->p_sys;

    if (vlc_es_id_GetCat(es_id) != VIDEO_ES)
        return;

    switch (action)
    {
        case VLC_PLAYER_VOUT_STARTED:
            if (vlc_vector_push(&sys->vout_vector, vout))
            {
                vout_Hold(vout);
                var_AddCallback(vout, "mouse-moved", MovedEvent, intf);
                var_AddCallback(vout, "mouse-button-down", ButtonEvent, intf);
            }
            break;
        case VLC_PLAYER_VOUT_STOPPED:
            for (size_t i = 0; i < sys->vout_vector.size; ++i)
            {
                vout_thread_t *it = sys->vout_vector.data[i];
                if (it == vout)
                {
                    vlc_vector_remove(&sys->vout_vector, i);
                    var_DelCallback(vout, "mouse-moved", MovedEvent, intf);
                    var_DelCallback(vout, "mouse-button-down", ButtonEvent, intf);
                    vout_Release(vout);
                    break;
                }
            }
            break;
        default:
            vlc_assert_unreachable();
    }
}
