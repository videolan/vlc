/*****************************************************************************
 * video.h: video decoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: video.h,v 1.7 2003/04/17 10:58:30 fenrir Exp $
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

    AVFrame        *p_ff_pic;
    BITMAPINFOHEADER    *p_format;

    vout_thread_t       *p_vout;

    /* for post processing */
#ifdef LIBAVCODEC_PP
    pp_context_t        *pp_context;
    pp_mode_t           *pp_mode;
#else
    uint32_t            i_pp_mode; /* valid only with I420 and YV12 */
    postprocessing_t    *p_pp;
#endif

    /* for frame skipping algo */
    int b_hurry_up;
    int i_frame_error;
    int i_frame_skip;
    int i_frame_late;   /* how many decoded frames are late */

    int i_frame_count;  /* to emulate pts */

    /* for direct rendering */
    int b_direct_rendering;
} vdec_thread_t;


int      E_( InitThread_Video )   ( vdec_thread_t * );
void     E_( EndThread_Video )    ( vdec_thread_t * );
void     E_( DecodeThread_Video ) ( vdec_thread_t * );
