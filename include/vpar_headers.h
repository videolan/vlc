/*****************************************************************************
 * vpar_headers.h : video parser : headers parsing
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
 *  "video_fifo.h"
 *****************************************************************************/

/*****************************************************************************
 * quant_matrix_t : Quantization Matrix
 *****************************************************************************/
typedef struct quant_matrix_s
{
    int *       pi_matrix;
    boolean_t   b_allocated;
                          /* Has the matrix been allocated by vpar_headers ? */
} quant_matrix_t;

/*****************************************************************************
 * sequence_t : sequence descriptor
 *****************************************************************************
 * This structure should only be changed when reading the sequence header,
 * or exceptionnally some extension structures (like quant_matrix).
 *****************************************************************************/
typedef struct sequence_s
{
    u32                 i_height, i_width;      /* height and width of the lum
                                                 * comp of the picture       */
    u32                 i_size;       /* total number of pel of the lum comp */
    u32                 i_mb_height, i_mb_width, i_mb_size;
                                            /* the same, in macroblock units */
    unsigned int        i_aspect_ratio;        /* height/width display ratio */
    unsigned int        i_matrix_coefficients;/* coeffs of the YUV transform */
    int                 i_frame_rate;  /* theoritical frame rate in fps*1001 */
    boolean_t           b_mpeg2;                                    /* guess */
    boolean_t           b_progressive;              /* progressive (ie.
                                                     * non-interlaced) frame */
    unsigned int        i_scalable_mode; /* scalability ; unsupported, but
                                          * modifies the syntax of the binary
                                          * stream.                          */
    quant_matrix_t      intra_quant, nonintra_quant;
    quant_matrix_t      chroma_intra_quant, chroma_nonintra_quant;
                                            /* current quantization matrices */

    /* Chromatic information */
    unsigned int        i_chroma_format;               /* see CHROMA_* below */
    int                 i_chroma_nb_blocks;       /* number of chroma blocks */
    u32                 i_chroma_width;/* width of a line of the chroma comp */
    u32                 i_chroma_mb_width, i_chroma_mb_height;
                                 /* size of a macroblock in the chroma buffer
                                  * (eg. 8x8 or 8x16 or 16x16)               */

    /* Parser context */
    picture_t *         p_forward;        /* current forward reference frame */
    picture_t *         p_backward;      /* current backward reference frame */

    /* Copyright extension */
    boolean_t           b_copyright_flag;     /* Whether the following
                                                 information is significant
                                                 or not. */
    u8                  i_copyright_id;
    boolean_t           b_original;
    u64                 i_copyright_nb;
} sequence_t;

/*****************************************************************************
 * picture_parsing_t : parser context descriptor
 *****************************************************************************
 * This structure should only be changed when reading the picture header.
 *****************************************************************************/
typedef struct picture_parsing_s
{
    /* ISO/CEI 11172-2 backward compatibility */
    boolean_t           pb_full_pel_vector[2];
    int                 i_forward_f_code, i_backward_f_code;

    /* Values from the picture_coding_extension. Please refer to ISO/IEC
     * 13818-2. */
    int                 ppi_f_code[2][2];
    int                 i_intra_dc_precision;
    boolean_t           b_frame_pred_frame_dct, b_q_scale_type;
    boolean_t           b_intra_vlc_format;
    boolean_t           b_alternate_scan, b_progressive_frame;
    boolean_t           b_top_field_first, b_concealment_mv;
    boolean_t           b_repeat_first_field;
    /* Relative to the current field */
    int                 i_coding_type, i_structure;
    boolean_t           b_frame_structure; /* i_structure == FRAME_STRUCTURE */

    picture_t *         p_picture;               /* picture buffer from vout */
    int                 i_current_structure;   /* current parsed structure of
                                                * p_picture (second field ?) */
#ifdef VDEC_SMP
    macroblock_t *      pp_mb[MAX_MB];         /* macroblock buffer to
                                                * send to the vdec thread(s) */
#endif
    boolean_t           b_error;            /* parsing error, try to recover */

    int                 i_l_stride, i_c_stride;
                                    /* number of coeffs to jump when changing
                                     * lines (different with field pictures) */
} picture_parsing_t;

/*****************************************************************************
 * Standard codes
 *****************************************************************************/
#define PICTURE_START_CODE      0x100L
#define SLICE_START_CODE_MIN    0x101L
#define SLICE_START_CODE_MAX    0x1AFL
#define USER_DATA_START_CODE    0x1B2L
#define SEQUENCE_HEADER_CODE    0x1B3L
#define SEQUENCE_ERROR_CODE     0x1B4L
#define EXTENSION_START_CODE    0x1B5L
#define SEQUENCE_END_CODE       0x1B7L
#define GROUP_START_CODE        0x1B8L

/* extension start code IDs */
#define SEQUENCE_EXTENSION_ID                    1
#define SEQUENCE_DISPLAY_EXTENSION_ID            2
#define QUANT_MATRIX_EXTENSION_ID                3
#define COPYRIGHT_EXTENSION_ID                   4
#define SEQUENCE_SCALABLE_EXTENSION_ID           5
#define PICTURE_DISPLAY_EXTENSION_ID             7
#define PICTURE_CODING_EXTENSION_ID              8
#define PICTURE_SPATIAL_SCALABLE_EXTENSION_ID    9
#define PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID  10

/* scalable modes */
#define SC_NONE     0
#define SC_DP       1
#define SC_SPAT     2
#define SC_SNR      3
#define SC_TEMP     4

/* Chroma types */
#define CHROMA_420 1
#define CHROMA_422 2
#define CHROMA_444 3

/* Pictures types */
#define I_CODING_TYPE           1
#define P_CODING_TYPE           2
#define B_CODING_TYPE           3
#define D_CODING_TYPE           4 /* MPEG-1 ONLY */
/* other values are reserved */

/* Structures */
#define TOP_FIELD               1
#define BOTTOM_FIELD            2
#define FRAME_STRUCTURE         3

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int vpar_NextSequenceHeader( struct vpar_thread_s * p_vpar );
int vpar_ParseHeader( struct vpar_thread_s * p_vpar );
