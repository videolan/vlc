/*****************************************************************************
 * video_widgets.c : OSD widgets manipulation functions
 *****************************************************************************
 * Copyright (C) 2004-2005 the VideoLAN team
 * $Id$
 *
 * Author: Yoann Peronneau <yoann@videolan.org>
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
#include <assert.h>

#include <vlc_common.h>
#include <vlc_vout.h>
#include <vlc_osd.h>

#include <vlc_filter.h>

/* TODO remove access to private vout data */
#include "vout_internal.h"

/*****************************************************************************
 * Displays an OSD slider.
 * Types are: OSD_HOR_SLIDER and OSD_VERT_SLIDER.
 *****************************************************************************/
void vout_OSDSlider( vout_thread_t *p_vout, int i_channel, int i_position,
                     short i_type )
{
    if( !var_InheritBool( p_vout, "osd" ) || i_position < 0 )
        return;

    osd_Slider( VLC_OBJECT( p_vout ), vout_GetSpu( p_vout ),
                p_vout->p->fmt_render.i_width,
                p_vout->p->fmt_render.i_height,
                p_vout->p->fmt_in.i_x_offset,
                p_vout->p->fmt_in.i_height - p_vout->p->fmt_in.i_visible_height
                                        - p_vout->p->fmt_in.i_y_offset,
                i_channel, i_position, i_type );
}

/*****************************************************************************
 * Displays an OSD icon.
 * Types are: OSD_PLAY_ICON, OSD_PAUSE_ICON, OSD_SPEAKER_ICON, OSD_MUTE_ICON
 *****************************************************************************/
void vout_OSDIcon( vout_thread_t *p_vout, int i_channel, short i_type )
{
    if( !var_InheritBool( p_vout, "osd" ) )
        return;
    osd_Icon( VLC_OBJECT( p_vout ),
              vout_GetSpu( p_vout ),
              p_vout->p->fmt_render.i_width,
              p_vout->p->fmt_render.i_height,
              p_vout->p->fmt_in.i_width - p_vout->p->fmt_in.i_visible_width
                                     - p_vout->p->fmt_in.i_x_offset,
              p_vout->p->fmt_in.i_y_offset,
              i_channel, i_type );
}

