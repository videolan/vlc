/*****************************************************************************
 * vdec_common.h : structures from the video decoder exported to plug-ins
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: vdec_ext-plugins.h,v 1.1 2001/07/17 09:48:07 massiot Exp $
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
 * Function pointers
 *****************************************************************************/
typedef void (*f_motion_t)( struct macroblock_s * );

/*****************************************************************************
 * macroblock_t : information on a macroblock passed to the video_decoder
 *                thread
 *****************************************************************************/
typedef struct macroblock_s
{
    picture_t *             p_picture;          /* current frame in progress */

    int                     i_mb_type;                    /* macroblock type */
    int                     i_coded_block_pattern;
                                                 /* which blocks are coded ? */
    int                     i_chroma_nb_blocks;         /* number of blocks for
                                                         * chroma components */

    /* IDCT information */
    dctelem_t               ppi_blocks[12][64];                    /* blocks */
    void ( * pf_idct[12] )  ( void *, dctelem_t*, int );
                                                     /* sparse IDCT or not ? */
    int                     pi_sparse_pos[12];             /* position of the
                                                            * non-NULL coeff */

    /* Motion compensation information */
    f_motion_t              pf_motion;    /* function to use for motion comp */
    picture_t *             p_backward;          /* backward reference frame */
    picture_t *             p_forward;            /* forward reference frame */
    int                     ppi_field_select[2][2];      /* field to use to
                                                          * form predictions */
    int                     pppi_motion_vectors[2][2][2];  /* motion vectors */
    int                     ppi_dmv[2][2];    /* differential motion vectors */
                            /* coordinates of the block in the picture */
    int                     i_l_x, i_c_x;
    int                     i_motion_l_y;
    int                     i_motion_c_y;
    int                     i_l_stride;         /* number of yuv_data_t to
                                                 * ignore when changing line */
    int                     i_c_stride;                  /* idem, for chroma */
    boolean_t               b_P_second;  /* Second field of a P picture ?
                                          * (used to determine the predicting
                                          * frame)                           */
    boolean_t               b_motion_field;  /* Field we are predicting
                                              * (top field or bottom field) */

    /* AddBlock information */
    yuv_data_t *            p_data[12];              /* pointer to the position
                                                      * in the final picture */
    int                     i_addb_l_stride, i_addb_c_stride;
                                 /* nb of coeffs to jump when changing lines */
} macroblock_t;

/* Macroblock types */
#define MB_INTRA                        1
#define MB_PATTERN                      2
#define MB_MOTION_BACKWARD              4
#define MB_MOTION_FORWARD               8
#define MB_QUANT                        16

/* Motion types */
#define MOTION_FIELD                    1
#define MOTION_FRAME                    2
#define MOTION_16X8                     2
#define MOTION_DMV                      3

/* Structures */
#define TOP_FIELD               1
#define BOTTOM_FIELD            2
#define FRAME_STRUCTURE         3

/*****************************************************************************
 * vdec_thread_t: video decoder thread descriptor
 *****************************************************************************/
typedef struct vdec_thread_s
{
    /* Thread properties and locks */
    boolean_t           b_die;                                 /* `die' flag */
    boolean_t           b_run;                                 /* `run' flag */
    boolean_t           b_error;                             /* `error' flag */
    boolean_t           b_active;                           /* `active' flag */
    vlc_thread_t        thread_id;                /* id for thread functions */

    /* IDCT iformations */
    void *			p_idct_data;
    dctelem_t              p_pre_idct[64*64];

    /* Macroblock copy functions */
    void ( * pf_decode_init ) ( struct vdec_thread_s * );
    void ( * pf_decode_mb_c ) ( struct vdec_thread_s *, struct macroblock_s * );
    void ( * pf_decode_mb_bw )( struct vdec_thread_s *, struct macroblock_s * );

    /* Input properties */
    struct vpar_thread_s * p_vpar;                    /* video_parser thread */

} vdec_thread_t;

