/*****************************************************************************
 * vout_intf.c : video output interface
 *****************************************************************************
 * Copyright (C) 2000-2004 VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                                /* free() */

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "vlc_video.h"
#include "video_output.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* Object variables callbacks */
static int ZoomCallback( vlc_object_t *, char const *,
                         vlc_value_t, vlc_value_t, void * );
static int OnTopCallback( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );
static int FullscreenCallback( vlc_object_t *, char const *,
                               vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * vout_RequestWindow: Create/Get a video window if possible.
 *****************************************************************************
 * This function looks for the main interface and tries to request
 * a new video window. If it fails then the vout will still need to create the
 * window by itself.
 *****************************************************************************/
void *vout_RequestWindow( vout_thread_t *p_vout,
                          int *pi_x_hint, int *pi_y_hint,
                          unsigned int *pi_width_hint,
                          unsigned int *pi_height_hint )
{
    intf_thread_t *p_intf = NULL;
    vlc_list_t *p_list;
    void *p_window;
    vlc_value_t val;
    int i;

    /* Small kludge */
    if( !var_Type( p_vout, "aspect-ratio" ) ) vout_IntfInit( p_vout );

    /* Get requested coordinates */
    var_Get( p_vout, "video-x", &val );
    *pi_x_hint = val.i_int ;
    var_Get( p_vout, "video-y", &val );
    *pi_y_hint = val.i_int;

    *pi_width_hint = p_vout->i_window_width;
    *pi_height_hint = p_vout->i_window_height;

    /* Check whether someone provided us with a window ID */
    var_Get( p_vout->p_vlc, "drawable", &val );
    if( val.i_int ) return (void *)val.i_int;

    /* Find if the main interface supports embedding */
    p_list = vlc_list_find( p_vout, VLC_OBJECT_INTF, FIND_ANYWHERE );
    if( !p_list ) return NULL;

    for( i = 0; i < p_list->i_count; i++ )
    {
        p_intf = (intf_thread_t *)p_list->p_values[i].p_object;
        if( p_intf->b_block && p_intf->pf_request_window ) break;
        p_intf = NULL;
    }

    if( !p_intf )
    {
        vlc_list_release( p_list );
        return NULL;
    }

    vlc_object_yield( p_intf );
    vlc_list_release( p_list );

    p_window = p_intf->pf_request_window( p_intf, p_vout, pi_x_hint, pi_y_hint,
                                          pi_width_hint, pi_height_hint );

    if( !p_window ) vlc_object_release( p_intf );
    else p_vout->p_parent_intf = p_intf;

    return p_window;
}

void vout_ReleaseWindow( vout_thread_t *p_vout, void *p_window )
{
    intf_thread_t *p_intf = p_vout->p_parent_intf;

    if( !p_intf ) return;

    vlc_mutex_lock( &p_intf->object_lock );
    if( p_intf->b_dead )
    {
        vlc_mutex_unlock( &p_intf->object_lock );
        return;
    }

    if( !p_intf->pf_release_window )
    {
        msg_Err( p_vout, "no pf_release_window");
        vlc_mutex_unlock( &p_intf->object_lock );
        vlc_object_release( p_intf );
        return;
    }

    p_intf->pf_release_window( p_intf, p_window );

    p_vout->p_parent_intf = NULL;
    vlc_mutex_unlock( &p_intf->object_lock );
    vlc_object_release( p_intf );
}

int vout_ControlWindow( vout_thread_t *p_vout, void *p_window,
                        int i_query, va_list args )
{
    intf_thread_t *p_intf = p_vout->p_parent_intf;
    int i_ret;

    if( !p_intf ) return VLC_EGENERIC;

    vlc_mutex_lock( &p_intf->object_lock );
    if( p_intf->b_dead )
    {
        vlc_mutex_unlock( &p_intf->object_lock );
        return VLC_EGENERIC;
    }

    if( !p_intf->pf_control_window )
    {
        msg_Err( p_vout, "no pf_control_window");
        vlc_mutex_unlock( &p_intf->object_lock );
        return VLC_EGENERIC;
    }

    i_ret = p_intf->pf_control_window( p_intf, p_window, i_query, args );
    vlc_mutex_unlock( &p_intf->object_lock );
    return i_ret;
}

/*****************************************************************************
 * vout_IntfInit: called during the vout creation to initialise misc things.
 *****************************************************************************/
void vout_IntfInit( vout_thread_t *p_vout )
{
    vlc_value_t val, text, old_val;

    /* Create a few object variables we'll need later on */
    var_Create( p_vout, "aspect-ratio", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "width", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "height", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "align", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "video-x", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_vout, "video-y", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    var_Create( p_vout, "zoom", VLC_VAR_FLOAT | VLC_VAR_ISCOMMAND |
                VLC_VAR_HASCHOICE | VLC_VAR_DOINHERIT );

    text.psz_string = _("Zoom");
    var_Change( p_vout, "zoom", VLC_VAR_SETTEXT, &text, NULL );

    var_Get( p_vout, "zoom", &old_val );
    if( old_val.f_float == 0.25 ||
        old_val.f_float == 0.5 ||
        old_val.f_float == 1 ||
        old_val.f_float == 2 )
    {
        var_Change( p_vout, "zoom", VLC_VAR_DELCHOICE, &old_val, NULL );
    }

    val.f_float = 0.25; text.psz_string = _("1:4 Quarter");
    var_Change( p_vout, "zoom", VLC_VAR_ADDCHOICE, &val, &text );
    val.f_float = 0.5; text.psz_string = _("1:2 Half");
    var_Change( p_vout, "zoom", VLC_VAR_ADDCHOICE, &val, &text );
    val.f_float = 1; text.psz_string = _("1:1 Original");
    var_Change( p_vout, "zoom", VLC_VAR_ADDCHOICE, &val, &text );
    val.f_float = 2; text.psz_string = _("2:1 Double");
    var_Change( p_vout, "zoom", VLC_VAR_ADDCHOICE, &val, &text );

    var_Set( p_vout, "zoom", old_val );

    var_AddCallback( p_vout, "zoom", ZoomCallback, NULL );

    /* Add a variable to indicate if the window should be on top of others */
    var_Create( p_vout, "video-on-top", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    text.psz_string = _("Always on top");
    var_Change( p_vout, "video-on-top", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_vout, "video-on-top", OnTopCallback, NULL );

    /* Add a variable to indicate whether we want window decoration or not */
    var_Create( p_vout, "video-deco", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    /* Add a fullscreen variable */
    var_Create( p_vout, "fullscreen", VLC_VAR_BOOL );
    text.psz_string = _("Fullscreen");
    var_Change( p_vout, "fullscreen", VLC_VAR_SETTEXT, &text, NULL );
    var_Change( p_vout, "fullscreen", VLC_VAR_INHERITVALUE, &val, NULL );
    if( val.b_bool )
    {
        /* user requested fullscreen */
        p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
    }
    var_AddCallback( p_vout, "fullscreen", FullscreenCallback, NULL );

    /* Mouse coordinates */
    var_Create( p_vout, "mouse-x", VLC_VAR_INTEGER );
    var_Create( p_vout, "mouse-y", VLC_VAR_INTEGER );
    var_Create( p_vout, "mouse-button-down", VLC_VAR_INTEGER );
    var_Create( p_vout, "mouse-moved", VLC_VAR_BOOL );
    var_Create( p_vout, "mouse-clicked", VLC_VAR_INTEGER );

    var_Create( p_vout, "intf-change", VLC_VAR_BOOL );
    val.b_bool = VLC_TRUE;
    var_Set( p_vout, "intf-change", val );
}

/*****************************************************************************
 * vout_ControlDefault: default methods for video output control.
 *****************************************************************************/
int vout_vaControlDefault( vout_thread_t *p_vout, int i_query, va_list args )
{
    switch( i_query )
    {
    case VOUT_REPARENT:
    case VOUT_CLOSE:
        if( p_vout->p_parent_intf )
        {
            vlc_object_release( p_vout->p_parent_intf );
            p_vout->p_parent_intf = NULL;
        }
        return VLC_SUCCESS;
        break;

    default:
        msg_Dbg( p_vout, "control query not supported" );
        return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * Object variables callbacks
 *****************************************************************************/
static int ZoomCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_Control( p_vout, VOUT_SET_ZOOM, newval.f_float );
    return VLC_SUCCESS;
}

static int OnTopCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    playlist_t *p_playlist;
    vout_Control( p_vout, VOUT_SET_STAY_ON_TOP, newval.b_bool );

    p_playlist = (playlist_t *)vlc_object_find( p_this, VLC_OBJECT_PLAYLIST,
                                                 FIND_PARENT );
    if( p_playlist )
    {
        /* Modify playlist as well because the vout might have to be restarted */
        var_Create( p_playlist, "video-on-top", VLC_VAR_BOOL );
        var_Set( p_playlist, "video-on-top", newval );

        vlc_object_release( p_playlist );
    }
    return VLC_SUCCESS;
}

static int FullscreenCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    playlist_t *p_playlist;
    vlc_value_t val;

    p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;

    p_playlist = (playlist_t *)vlc_object_find( p_this, VLC_OBJECT_PLAYLIST,
                                                 FIND_PARENT );
    if( p_playlist )
    {
        /* Modify playlist as well because the vout might have to be restarted */
        var_Create( p_playlist, "fullscreen", VLC_VAR_BOOL );
        var_Set( p_playlist, "fullscreen", newval );

        vlc_object_release( p_playlist );
    }

    /* Disable "always on top" in fullscreen mode */
    var_Get( p_vout, "video-on-top", &val );
    if( newval.b_bool && val.b_bool )
    {
        val.b_bool = VLC_FALSE;
        vout_Control( p_vout, VOUT_SET_STAY_ON_TOP, val.b_bool );
    }
    else if( !newval.b_bool && val.b_bool )
    {
        vout_Control( p_vout, VOUT_SET_STAY_ON_TOP, val.b_bool );
    }

    val.b_bool = VLC_TRUE;
    var_Set( p_vout, "intf-change", val );
    return VLC_SUCCESS;
}
