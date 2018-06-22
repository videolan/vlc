/*****************************************************************************
 * vout_control.h : Vout control function definitions
 *****************************************************************************
 * Copyright (C) 2008 VLC authors and VideoLAN
 * Copyright (C) 2008 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar < fenrir _AT_ videolan _DOT_ org >
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef LIBVLC_VOUT_CONTROL_H
#define LIBVLC_VOUT_CONTROL_H 1

typedef struct vout_window_mouse_event_t vout_window_mouse_event_t;

/**
 * This function will (un)pause the display of pictures.
 * It is thread safe
 */
void vout_ChangePause( vout_thread_t *, bool b_paused, vlc_tick_t i_date );

/**
 * This function will apply an offset on subtitle subpicture.
 */
void spu_OffsetSubtitleDate( spu_t *p_spu, vlc_tick_t i_duration );

/**
 * This function will return and reset internal statistics.
 */
void vout_GetResetStatistic( vout_thread_t *p_vout, unsigned *pi_displayed,
                             unsigned *pi_lost );

/**
 * This function will ensure that all ready/displayed pictures have at most
 * the provided date.
 */
void vout_Flush( vout_thread_t *p_vout, vlc_tick_t i_date );

/*
 * Cancel the vout, if cancel is true, it won't return any pictures after this
 * call.
 */
void vout_Cancel( vout_thread_t *p_vout, bool b_canceled );

/**
 * This function will force to display the next picture while paused
 */
void vout_NextPicture( vout_thread_t *p_vout, vlc_tick_t *pi_duration );

/**
 * This function will ask the display of the input title
 */
void vout_DisplayTitle( vout_thread_t *p_vout, const char *psz_title );

void vout_WindowMouseEvent( vout_thread_t *p_vout,
                            const vout_window_mouse_event_t *mouse );

/**
 * This function will return true if no more pictures are to be displayed.
 */
bool vout_IsEmpty( vout_thread_t *p_vout );

#endif
