/*****************************************************************************
 * video_parser.h : video parser thread
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: video_parser.h,v 1.1 2001/11/13 12:09:18 henri Exp $
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

/*
 * Block parsing structures
 */

/*****************************************************************************
 * macroblock_parsing_t : macroblock context & predictors
 *****************************************************************************/
typedef struct motion_s
{
    u8 *                pppi_ref[2][3];
    int                 ppi_pmv[2][2];
    int                 pi_f_code[2];
} motion_t;

typedef struct macroblock_parsing_s
{
    int                 i_offset;

    motion_t            b_motion;
    motion_t            f_motion;

    /* Predictor for DC coefficients in intra blocks */
    u16                 pi_dc_dct_pred[3];
    u8                  i_quantizer_scale;        /* scale of the quantization
                                                   * matrices                */
} macroblock_parsing_t;

/*****************************************************************************
 * Constants
 *****************************************************************************/
extern u8       pi_default_intra_quant[64];
extern u8       pi_default_nonintra_quant[64];
extern u8       pi_scan[2][64];

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
void vpar_InitScanTable( struct vpar_thread_s * p_vpar );

typedef void (*f_picture_data_t)( struct vpar_thread_s * p_vpar );
#define PROTO_PICD( FUNCNAME )                                              \
void FUNCNAME( struct vpar_thread_s * p_vpar );

PROTO_PICD( vpar_PictureDataGENERIC )
#if (VPAR_OPTIM_LEVEL > 0)
PROTO_PICD( vpar_PictureData2IF )
PROTO_PICD( vpar_PictureData2PF )
PROTO_PICD( vpar_PictureData2BF )
#endif
#if (VPAR_OPTIM_LEVEL > 1)
PROTO_PICD( vpar_PictureData2IT )
PROTO_PICD( vpar_PictureData2PT )
PROTO_PICD( vpar_PictureData2BT )
PROTO_PICD( vpar_PictureData2IB )
PROTO_PICD( vpar_PictureData2PB )
PROTO_PICD( vpar_PictureData2BB )
PROTO_PICD( vpar_PictureData1I )
PROTO_PICD( vpar_PictureData1P )
PROTO_PICD( vpar_PictureData1B )
PROTO_PICD( vpar_PictureData1D )
#endif


/*
 * Headers parsing structures
 */

/*****************************************************************************
 * quant_matrix_t : Quantization Matrix
 *****************************************************************************/
typedef struct quant_matrix_s
{
    u8 *        pi_matrix;
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
    int                 i_chroma_format, i_scalable_mode;
    int                 i_chroma_nb_blocks;
    boolean_t           b_chroma_h_subsampled, b_chroma_v_subsampled;
    int                 i_frame_rate;  /* theoritical frame rate in fps*1001 */
    boolean_t           b_mpeg2;                                    /* guess */
    boolean_t           b_progressive;              /* progressive (ie.
                                                     * non-interlaced) frame */
    quant_matrix_t      intra_quant, nonintra_quant;
    quant_matrix_t      chroma_intra_quant, chroma_nonintra_quant;
                                            /* current quantization matrices */

    /* Parser context */
    picture_t *         p_forward;        /* current forward reference frame */
    picture_t *         p_backward;      /* current backward reference frame */
    mtime_t             next_pts, next_dts;
    int                 i_current_rate;
    boolean_t           b_expect_discontinuity; /* reset the frame predictors
                                                 * after the current frame   */

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
    /* Values from the picture_coding_extension. */
    int                 ppi_f_code[2][2];
    int                 i_intra_dc_precision;
    boolean_t           b_frame_pred_frame_dct, b_q_scale_type;
    boolean_t           b_intra_vlc_format;
    boolean_t           b_progressive;
    u8 *                pi_scan;
    boolean_t           b_top_field_first, b_concealment_mv;
    boolean_t           b_repeat_first_field;
    /* Relative to the current field */
    int                 i_coding_type, i_structure;
    boolean_t           b_frame_structure; /* i_structure == FRAME_STRUCTURE */
    boolean_t           b_current_field;         /* i_structure == TOP_FIELD */
    boolean_t           b_second_field;

    picture_t *         p_picture;               /* picture buffer from vout */
    int                 i_current_structure;   /* current parsed structure of
                                                * p_picture (second field ?) */
    int                 i_field_width;
    boolean_t           b_error;            /* parsing error, try to recover */
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
#define CHROMA_NONE 0
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


/*
 * Synchronization management
 */

/*****************************************************************************
 * video_synchro_t : timers for the video synchro
 *****************************************************************************/
#define MAX_PIC_AVERAGE         8

/* Read the discussion on top of vpar_synchro.c for more information. */
typedef struct video_synchro_s
{
    /* synchro algorithm */
    int             i_type;

    /* date of the beginning of the decoding of the current picture */
    mtime_t         decoding_start;

    /* stream properties */
    unsigned int    i_n_p, i_n_b;

    /* decoding values */
    mtime_t         p_tau[4];                  /* average decoding durations */
    unsigned int    pi_meaningful[4];            /* number of durations read */
    /* and p_vout->render_time (read with p_vout->change_lock) */

    /* stream context */
    unsigned int    i_eta_p, i_eta_b;
    boolean_t       b_dropped_last;            /* for special synchros below */
    mtime_t         backward_pts, current_pts;
    int             i_current_period;   /* period to add to the next picture */
    int             i_backward_period;  /* period to add after the next
                                         * reference picture
                                         * (backward_period * period / 2) */

    /* statistics */
    unsigned int    i_trashed_pic, i_not_chosen_pic, i_pic;
} video_synchro_t;

/* Synchro algorithms */
#define VPAR_SYNCHRO_DEFAULT    0
#define VPAR_SYNCHRO_I          1
#define VPAR_SYNCHRO_Iplus      2
#define VPAR_SYNCHRO_IP         3
#define VPAR_SYNCHRO_IPplus     4
#define VPAR_SYNCHRO_IPB        5

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
void vpar_SynchroInit           ( struct vpar_thread_s * p_vpar );
boolean_t vpar_SynchroChoose    ( struct vpar_thread_s * p_vpar,
                                  int i_coding_type, int i_structure );
void vpar_SynchroTrash          ( struct vpar_thread_s * p_vpar,
                                  int i_coding_type, int i_structure );
void vpar_SynchroDecode         ( struct vpar_thread_s * p_vpar,
                                  int i_coding_type, int i_structure );
void vpar_SynchroEnd            ( struct vpar_thread_s * p_vpar,
                                  int i_coding_type, int i_structure,
                                  int i_garbage );
mtime_t vpar_SynchroDate        ( struct vpar_thread_s * p_vpar );
void vpar_SynchroNewPicture( struct vpar_thread_s * p_vpar, int i_coding_type,
                             int i_repeat_field );


/*
 * Video parser structures
 */

/*****************************************************************************
 * vpar_thread_t: video parser thread descriptor
 *****************************************************************************/
typedef struct vpar_thread_s
{
    bit_stream_t            bit_stream;

    /* Thread properties and locks */
    vlc_thread_t            thread_id;            /* id for thread functions */

    /* Input properties */
    decoder_fifo_t *        p_fifo;                        /* PES input fifo */
    decoder_config_t *      p_config;

    /* Output properties */
    vout_thread_t *         p_vout;                   /* video output thread */

    /* Decoder properties */
    vdec_pool_t             pool;

    /* Parser properties */
    sequence_t              sequence;
    picture_parsing_t       picture;
    macroblock_parsing_t    mb;
    video_synchro_t         synchro;

    /* Scan table */
    u8                      ppi_scan[2][64];
    /* Default quantization matrices */
    u8                      pi_default_intra_quant[64];
    u8                      pi_default_nonintra_quant[64];

    /* Motion compensation plug-in used and shortcuts */
    struct module_s *       p_motion_module;

    /* IDCT plug-in used and shortcuts */
    struct module_s *       p_idct_module;
    void ( * pf_sparse_idct_add )( dctelem_t *, yuv_data_t *, int,
                                 void *, int );
    void ( * pf_idct_add )     ( dctelem_t *, yuv_data_t *, int,
                                 void *, int );
    void ( * pf_sparse_idct_copy )( dctelem_t *, yuv_data_t *, int,
                                  void *, int );
    void ( * pf_idct_copy )    ( dctelem_t *, yuv_data_t *, int,
                                 void *, int );

    void ( * pf_norm_scan ) ( u8 ppi_scan[2][64] );

    /* Statistics */
    count_t         c_loops;                              /* number of loops */
    count_t         c_sequences;                      /* number of sequences */
    count_t         pc_pictures[4]; /* number of (coding_type) pictures read */
    count_t         pc_decoded_pictures[4];       /* number of (coding_type)
                                                   *        pictures decoded */
    count_t         pc_malformed_pictures[4];  /* number of pictures trashed
                                                * during parsing             */
} vpar_thread_t;

/*****************************************************************************
 * NextStartCode : Find the next start code
 *****************************************************************************/
static __inline__ void NextStartCode( bit_stream_t * p_bit_stream )
{
    /* Re-align the buffer on an 8-bit boundary */
    RealignBits( p_bit_stream );

    while( ShowBits( p_bit_stream, 24 ) != 0x01L
            && !p_bit_stream->p_decoder_fifo->b_die )
    {
        RemoveBits( p_bit_stream, 8 );
    }
}

/*****************************************************************************
 * LoadQuantizerScale
 *****************************************************************************
 * Quantizer scale factor (ISO/IEC 13818-2 7.4.2.2)
 *****************************************************************************/
static __inline__ void LoadQuantizerScale( struct vpar_thread_s * p_vpar )
{
    /* Quantization coefficient table */
    static u8   pi_non_linear_quantizer_scale[32] =
    {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 10,12,14,16,18,20, 22,
        24,28,32,36,40,44,48,52,56,64,72,80,88,96,104,112
    };
    int         i_q_scale_code = GetBits( &p_vpar->bit_stream, 5 );

    if( p_vpar->picture.b_q_scale_type )
    {
        p_vpar->mb.i_quantizer_scale =
                        pi_non_linear_quantizer_scale[i_q_scale_code];
    }
    else
    {
        p_vpar->mb.i_quantizer_scale = i_q_scale_code << 1;
    }
}

