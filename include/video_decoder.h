/*****************************************************************************
 * video_decoder.h : video decoder thread
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
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
 * Requires:
 *  "config.h"
 *  "common.h"
 *  "mtime.h"
 *  "threads.h"
 *  "input.h"
 *  "video.h"
 *  "video_output.h"
 *  "decoder_fifo.h"
 *****************************************************************************/

/*****************************************************************************
 * vdec_thread_t: video decoder thread descriptor
 *****************************************************************************
 * XXX??
 *****************************************************************************/
typedef struct vdec_thread_s
{
    /* Thread properties and locks */
    boolean_t           b_die;                                 /* `die' flag */
    boolean_t           b_run;                                 /* `run' flag */
    boolean_t           b_error;                             /* `error' flag */
    boolean_t           b_active;                           /* `active' flag */
    vlc_thread_t        thread_id;                /* id for thread functions */

    /* Thread configuration */
    /* XXX?? */
//    int *pi_status;

    /* idct iformations */
    dctelem_t              p_pre_idct[64*64];

    /* Input properties */
    struct vpar_thread_s *    p_vpar;                 /* video_parser thread */

    /* Lookup tables */
//#ifdef MPEG2_COMPLIANT
    u8              pi_crop_buf[VDEC_CROPRANGE];
    u8 *            pi_crop;
//#endif

#ifdef STATS
    /* Statistics */
    count_t         c_loops;                              /* number of loops */
    count_t         c_idle_loops;                    /* number of idle loops */
    count_t         c_decoded_pictures;        /* number of pictures decoded */
    count_t         c_decoded_i_pictures;    /* number of I pictures decoded */
    count_t         c_decoded_p_pictures;    /* number of P pictures decoded */
    count_t         c_decoded_b_pictures;    /* number of B pictures decoded */
#endif
} vdec_thread_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
struct vpar_thread_s;
struct macroblock_s;

/* Thread management functions */
#ifndef VDEC_SMP
int             vdec_InitThread         ( struct vdec_thread_s *p_vdec );
void            vdec_DecodeMacroblock   ( struct vdec_thread_s *p_vdec,
                                          struct macroblock_s *p_mb );
#endif
vdec_thread_t * vdec_CreateThread       ( struct vpar_thread_s *p_vpar /*,
                                          int *pi_status */ );
void            vdec_DestroyThread      ( vdec_thread_t *p_vdec /*,
                                          int *pi_status */ );

