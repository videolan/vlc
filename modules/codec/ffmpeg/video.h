/*****************************************************************************
 * video.h: video decoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: video.h,v 1.3 2002/11/06 21:48:24 gbazin Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

typedef struct vdec_thread_s
{
    DECODER_THREAD_COMMON

    BITMAPINFOHEADER    *p_format;

    vout_thread_t       *p_vout; 

    /* for post processing */
    u32                 i_pp_mode; /* valid only with I420 and YV12 */
    postprocessing_t    *p_pp;

    /* for frame skipping algo */
    int b_hurry_up;
    int i_frame_error;
    int i_frame_skip;
    int i_frame_late;  /* how many decoded frames are late */

    /* for direct rendering */
    int b_direct_rendering;
    picture_t *p_lastpic;
    picture_t *p_secondlastpic;
} vdec_thread_t;


int      E_( InitThread_Video )   ( vdec_thread_t * );
void     E_( EndThread_Video )    ( vdec_thread_t * );
void     E_( DecodeThread_Video ) ( vdec_thread_t * );
