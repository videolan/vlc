/*****************************************************************************
 * vout_intf.c : video output interface
 *****************************************************************************
 * Copyright (C) 2000-2004 VideoLAN
 * $Id: video_output.c 6961 2004-03-05 17:34:23Z sam $
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

#include "vlc_video.h"
#include "video_output.h"
#include "vlc_interface.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* Object variables callbacks */
static int ZoomCallback( vlc_object_t *, char const *,
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
    intf_thread_t *p_intf;
    void *p_window;
    vlc_value_t val;

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

    /* Find the main interface */
    p_intf = vlc_object_find( p_vout, VLC_OBJECT_INTF, FIND_ANYWHERE );
    if( !p_intf ) return NULL;

    if( !p_intf->pf_request_window )
    {
        vlc_object_release( p_intf );
        return NULL;
    }

    p_window = p_intf->pf_request_window( p_intf, pi_x_hint, pi_y_hint,
                                          pi_width_hint, pi_height_hint );
    vlc_object_release( p_intf );

    return p_window;
}

void vout_ReleaseWindow( vout_thread_t *p_vout, void *p_window )
{
    intf_thread_t *p_intf;

    /* Find the main interface */
    p_intf = vlc_object_find( p_vout, VLC_OBJECT_INTF, FIND_ANYWHERE );
    if( !p_intf ) return;

    if( !p_intf->pf_release_window )
    {
        msg_Err( p_vout, "no pf_release_window");
        vlc_object_release( p_intf );
        return;
    }

    p_intf->pf_release_window( p_intf, p_window );
    vlc_object_release( p_intf );
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

    val.f_float = 0.25; text.psz_string = _("1:4 Quater");
    var_Change( p_vout, "zoom", VLC_VAR_ADDCHOICE, &val, &text );
    val.f_float = 0.5; text.psz_string = _("1:2 Half");
    var_Change( p_vout, "zoom", VLC_VAR_ADDCHOICE, &val, &text );
    val.f_float = 1; text.psz_string = _("1:1 Original");
    var_Change( p_vout, "zoom", VLC_VAR_ADDCHOICE, &val, &text );
    val.f_float = 2; text.psz_string = _("2:1 Double");
    var_Change( p_vout, "zoom", VLC_VAR_ADDCHOICE, &val, &text );

    var_Set( p_vout, "zoom", old_val );

    var_AddCallback( p_vout, "zoom", ZoomCallback, NULL );
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
