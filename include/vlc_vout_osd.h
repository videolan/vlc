/*****************************************************************************
 * vlc_vout_osd.h: vout OSD
 *****************************************************************************
 * Copyright (C) 1999-2010 the VideoLAN team
 * Copyright (C) 2004-2005 M2X
 * $Id$
 *
 * Authors: Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
 *          Gildas Bazin <gbazin@videolan.org>
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

#ifndef VLC_VOUT_OSD_H
#define VLC_VOUT_OSD_H 1

#include <vlc_spu.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * OSD menu position and picture type defines
 */
#define OSD_HOR_SLIDER 1
#define OSD_VERT_SLIDER 2

#define OSD_PLAY_ICON 1
#define OSD_PAUSE_ICON 2
#define OSD_SPEAKER_ICON 3
#define OSD_MUTE_ICON 4

/**********************************************************************
 * Vout text and widget overlays
 **********************************************************************/
VLC_EXPORT( int, vout_OSDEpg, ( vout_thread_t *, input_item_t * ) );

/**
 * Write an informative message at the default location,
 * for the default duration and only if the OSD option is enabled.
 * \param p_caller The object that called the function.
 * \param i_channel Subpicture channel
 * \param psz_format printf style formatting
 **/
VLC_EXPORT( void,  vout_OSDMessage, ( vout_thread_t *, int, const char *, ... ) LIBVLC_FORMAT( 3, 4 ) );

/**
 * Display a slider on the video output.
 * \param p_this    The object that called the function.
 * \param i_channel Subpicture channel
 * \param i_postion Current position in the slider
 * \param i_type    Types are: OSD_HOR_SLIDER and OSD_VERT_SLIDER.
 * @see vlc_osd.h
 */
VLC_EXPORT( void, vout_OSDSlider, ( vout_thread_t *, int, int , short ) );

/**
 * Display an Icon on the video output.
 * \param p_this    The object that called the function.
 * \param i_channel Subpicture channel
 * \param i_type    Types are: OSD_PLAY_ICON, OSD_PAUSE_ICON, OSD_SPEAKER_ICON, OSD_MUTE_ICON
 * @see vlc_osd.h
 */
VLC_EXPORT( void, vout_OSDIcon, ( vout_thread_t *, int, short ) );

#ifdef __cplusplus
}
#endif

#endif /* VLC_VOUT_OSD_H */

