/*****************************************************************************
 * vdec_ext-plugins.h : structures from the video decoder exported to plug-ins
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: vdec_ext-plugins.h,v 1.8 2001/12/09 17:01:35 sam Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
 * macroblock_t : information on a macroblock passed to the video_decoder
 *                thread
 *****************************************************************************/
typedef struct idct_inner_s
{
    /* Should be kept aligned ! */
    dctelem_t *             pi_block;                               /* block */
    void                ( * pf_idct )   ( dctelem_t *, yuv_data_t *, int,
                                          void *, int );
                                        /* sparse IDCT or not, add or copy ? */
    int                     i_sparse_pos;                  /* position of the
                                                            * non-NULL coeff */
} idct_inner_t;

typedef struct motion_inner_s
{
    boolean_t               b_average;                          /* 0 == copy */
    int                     i_x_pred, i_y_pred;            /* motion vectors */
    yuv_data_t *            pp_source[3];
    int                     i_dest_offset, i_src_offset;
    int                     i_stride, i_height;
    boolean_t               b_second_half;
} motion_inner_t;

typedef struct macroblock_s
{
    int                     i_mb_modes;

    /* IDCT information */
    idct_inner_t            p_idcts[12];
    u16                     i_coded_block_pattern;
                                                 /* which blocks are coded ? */
    yuv_data_t *            p_y_data;
    yuv_data_t *            p_u_data;
    yuv_data_t *            p_v_data;
                                                    /* pointers to the position
                                                     * in the final picture  */

    /* Motion compensation information */
    motion_inner_t          p_motions[8];
    int                     i_nb_motions;
    pixel_data_t *          pp_dest[3];

} macroblock_t;

/* Macroblock Modes */
#define MB_INTRA                        1
#define MB_PATTERN                      2
#define MB_MOTION_BACKWARD              4
#define MB_MOTION_FORWARD               8
#define MB_QUANT                        16
#define DCT_TYPE_INTERLACED             32

/*****************************************************************************
 * vdec_thread_t: video decoder thread descriptor
 *****************************************************************************/
typedef struct vdec_thread_s
{
    vlc_thread_t        thread_id;                /* id for thread functions */
    boolean_t           b_die;

    /* IDCT iformations */
    void *              p_idct_data;

    /* Input properties */
    struct vdec_pool_s * p_pool;
} vdec_thread_t;

