/*****************************************************************************
 * vpar_blocks.h : video parser blocks management
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: vpar_blocks.h,v 1.1 2000/12/21 17:19:52 massiot Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Jean-Marc Dressler <polux@via.ecp.fr>
 *          Stéphane Borel <stef@via.ecp.fr>
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
 *  "video_fifo.h"
 *****************************************************************************/

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
    f_idct_t                pf_idct[12];             /* sparse IDCT or not ? */
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

/*****************************************************************************
 * macroblock_parsing_t : macroblock context & predictors
 *****************************************************************************/
typedef struct
{
    unsigned char       i_quantizer_scale;        /* scale of the quantization
                                                   * matrices                */
    int                 pi_dc_dct_pred[3];          /* ISO/IEC 13818-2 7.2.1 */
    int                 pppi_pmv[2][2][2];  /* Motion vect predictors, 7.6.3 */
    int                 i_motion_dir;/* Used for the next skipped macroblock */

    /* Context used to optimize block parsing */
    int                 i_motion_type, i_mv_count, i_mv_format;
    boolean_t           b_dmv, b_dct_type;

    /* Coordinates of the upper-left pixel of the macroblock, in lum and
     * chroma */
    int                 i_l_x, i_l_y, i_c_x, i_c_y;
} macroblock_parsing_t;

/*****************************************************************************
 * lookup_t : entry type for lookup tables                                   *
 *****************************************************************************/
typedef struct lookup_s
{
    int    i_value;
    int    i_length;
} lookup_t;

/*****************************************************************************
 * ac_lookup_t : special entry type for lookup tables about ac coefficients
 *****************************************************************************/
typedef struct dct_lookup_s
{
    char   i_run;
    char   i_level;
    char   i_length;
} dct_lookup_t;

/*****************************************************************************
 * Standard codes
 *****************************************************************************/
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

/* Macroblock Address Increment types */
#define MB_ADDRINC_ESCAPE               8
#define MB_ADDRINC_STUFFING             15

/* Error constant for lookup tables */
#define MB_ERROR                        (-1)

/* Scan */
#define SCAN_ZIGZAG                     0
#define SCAN_ALT                        1

/* Constant for block decoding */
#define DCT_EOB                         64
#define DCT_ESCAPE                      65

/*****************************************************************************
 * Constants
 *****************************************************************************/
extern int      pi_default_intra_quant[];
extern int      pi_default_nonintra_quant[];
extern u8       pi_scan[2][64];

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
void vpar_InitCrop( struct vpar_thread_s* p_vpar );
void vpar_InitMbAddrInc( struct vpar_thread_s * p_vpar );
void vpar_InitPMBType( struct vpar_thread_s * p_vpar );
void vpar_InitBMBType( struct vpar_thread_s * p_vpar );
void vpar_InitCodedPattern( struct vpar_thread_s * p_vpar );
void vpar_InitDCTTables( struct vpar_thread_s * p_vpar );
void vpar_PictureData( struct vpar_thread_s * p_vpar, int i_mb_base );
