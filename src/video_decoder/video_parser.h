/*****************************************************************************
 * video_parser.h : video parser thread
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: video_parser.h,v 1.11 2001/07/18 14:21:00 massiot Exp $
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
extern u8       pi_default_intra_quant[64];
extern u8       pi_default_nonintra_quant[64];
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
void vpar_InitScanTable( struct vpar_thread_s * p_vpar );

typedef void (*f_picture_data_t)( struct vpar_thread_s * p_vpar,
                                  int i_mb_base );
#define PROTO_PICD( FUNCNAME )                                              \
void FUNCNAME( struct vpar_thread_s * p_vpar, int i_mb_base );

PROTO_PICD( vpar_PictureDataGENERIC )
#if (VPAR_OPTIM_LEVEL > 0)
PROTO_PICD( vpar_PictureData1I )
PROTO_PICD( vpar_PictureData1P )
PROTO_PICD( vpar_PictureData1B )
PROTO_PICD( vpar_PictureData1D )
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
    /* ISO/CEI 11172-2 backward compatibility */
    boolean_t           pb_full_pel_vector[2];
    int                 i_forward_f_code, i_backward_f_code;

    /* Values from the picture_coding_extension. Please refer to ISO/IEC
     * 13818-2. */
    int                 ppi_f_code[2][2];
    int                 i_intra_dc_precision;
    boolean_t           b_frame_pred_frame_dct, b_q_scale_type;
    boolean_t           b_intra_vlc_format;
    boolean_t           b_alternate_scan, b_progressive;
    boolean_t           b_top_field_first, b_concealment_mv;
    boolean_t           b_repeat_first_field;
    /* Relative to the current field */
    int                 i_coding_type, i_structure;
    boolean_t           b_frame_structure; /* i_structure == FRAME_STRUCTURE */

    picture_t *         p_picture;               /* picture buffer from vout */
    int                 i_current_structure;   /* current parsed structure of
                                                * p_picture (second field ?) */
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

#ifdef STATS
    unsigned int    i_trashed_pic, i_not_chosen_pic, i_pic;
#endif
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
    vdec_config_t *         p_config;

    /* Output properties */
    vout_thread_t *         p_vout;                   /* video output thread */

    /* Decoder properties */
    vdec_pool_t             pool;

    /* Parser properties */
    sequence_t              sequence;
    picture_parsing_t       picture;
    macroblock_parsing_t    mb;
    video_synchro_t         synchro;

    /*
     * Lookup tables
     */
    /* table for macroblock address increment */
    lookup_t                pl_mb_addr_inc[2048];
    /* tables for macroblock types 0=P 1=B */
    lookup_t                ppl_mb_type[2][64];
    /* table for coded_block_pattern */
    lookup_t *              pl_coded_pattern;
    /* variable length codes for the structure dct_dc_size for intra blocks */
    lookup_t *              pppl_dct_dc_size[2][2];
    /* Structure to store the tables B14 & B15 (ISO/IEC 13818-2 B.4) */
    dct_lookup_t            ppl_dct_coef[2][16384];
    /* Scan table */
    u8                      ppi_scan[2][64];
    /* Default quantization matrices */
    u8                      pi_default_intra_quant[64];
    u8                      pi_default_nonintra_quant[64];

    /* Motion compensation plug-in used and shortcuts */
    struct module_s *       p_motion_module;
    void ( * pppf_motion[4][2][4] )     ( struct macroblock_s * );
    void ( * ppf_motion_skipped[4][4] ) ( struct macroblock_s * );

    /* IDCT plugin used and shortcuts */
    struct module_s *           p_idct_module;
    void ( * pf_sparse_idct ) ( void *, dctelem_t*, int );
    void ( * pf_idct )        ( void *, dctelem_t*, int );
    void ( * pf_norm_scan )   ( u8 ppi_scan[2][64] );
    void ( * pf_decode_mb_c ) ( struct vdec_thread_s *, struct macroblock_s * );
    void ( * pf_decode_mb_bw )( struct vdec_thread_s *, struct macroblock_s * );

#ifdef STATS
    /* Statistics */
    count_t         c_loops;                              /* number of loops */
    count_t         c_sequences;                      /* number of sequences */
    count_t         pc_pictures[4]; /* number of (coding_type) pictures read */
    count_t         pc_decoded_pictures[4];       /* number of (coding_type)
                                                   *        pictures decoded */
    count_t         pc_malformed_pictures[4];  /* number of pictures trashed
                                                * during parsing             */
#endif
} vpar_thread_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/* Thread management functions - temporary ! */
vlc_thread_t vpar_CreateThread       ( vdec_config_t * );

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
    static u8   ppi_quantizer_scale[3][32] =
    {
        /* MPEG-2 */
        {
            /* q_scale_type */
            /* linear */
            0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,
            32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62
        },
        {
            /* non-linear */
            0, 1, 2, 3, 4, 5, 6, 7, 8, 10,12,14,16,18,20, 22,
            24,28,32,36,40,44,48,52,56,64,72,80,88,96,104,112
        },
        /* MPEG-1 */
        {
            0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
            16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
        }
    };

    p_vpar->mb.i_quantizer_scale = ppi_quantizer_scale
           [(!p_vpar->sequence.b_mpeg2 << 1) | p_vpar->picture.b_q_scale_type]
           [GetBits( &p_vpar->bit_stream, 5 )];
}

