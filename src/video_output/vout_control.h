/*****************************************************************************
 * vout_control.h : Vout control function definitions
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * Copyright (C) 2008 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar < fenrir _AT_ videolan _DOT_ org >
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


#if defined(__PLUGIN__) || defined(__BUILTIN__) || !defined(__LIBVLC__)
# error This header file can only be included from LibVLC.
#endif

#ifndef _VOUT_CONTROL_H
#define _VOUT_CONTROL_H 1

/* DO NOT use vout_CountPictureAvailable unless your are in src/input/decoder.c (no exception) */
int vout_CountPictureAvailable( vout_thread_t * );

/**
 * This function will (un)pause the display of pictures.
 * It is thread safe
 */
void vout_ChangePause( vout_thread_t *, bool b_paused, mtime_t i_date );

/**
 * This function will apply an offset on subtitle subpicture.
 */
void spu_OffsetSubtitleDate( spu_t *p_spu, mtime_t i_duration );

/**
 * This function will return and reset internal statistics.
 */
void vout_GetResetStatistic( vout_thread_t *p_vout, int *pi_displayed, int *pi_lost );

/**
 * This function will ensure that all ready/displayed pciture have at most
 * the provided dat
 */
void vout_Flush( vout_thread_t *p_vout, mtime_t i_date );

/**
 * This function will try to detect if pictures are being leaked. If so it
 * will release them.
 *
 * XXX This function is there to workaround bugs in decoder
 */
void vout_FixLeaks( vout_thread_t *p_vout, bool b_forced );

/**
 * This functions will drop a picture retreived by vout_CreatePicture.
 */
void vout_DropPicture( vout_thread_t *p_vout, picture_t * );

/**
 * This function will force to display the next picture while paused
 */
void vout_NextPicture( vout_thread_t *p_vout, mtime_t *pi_duration );

/**
 * This function will ask the display of the input title
 */
void vout_DisplayTitle( vout_thread_t *p_vout, const char *psz_title );

#endif

