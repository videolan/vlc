/*****************************************************************************
 * vpar_headers.h : video parser : headers parsing
 * (c)1999 VideoLAN
 *****************************************************************************
 *****************************************************************************
 * Requires:
 *  "config.h"
 *  "common.h"
 *  "mtime.h"
 *  "vlc_thread.h"
 *  "input.h"
 *  "video.h"
 *  "video_output.h"
 *  "decoder_fifo.h"
 *  "video_fifo.h"
 *****************************************************************************/
 
/*****************************************************************************
 * Function pointers
 *****************************************************************************/
struct vpar_thread_s;

typedef void    (*f_slice_header_t)( struct vpar_thread_s*, int*, int, u32);
typedef void    (*f_chroma_pattern_t)( struct vpar_thread_s* );
typedef int     (*f_macroblock_type_t)( struct vpar_thread_s* );

/*****************************************************************************
 * quant_matrix_t : Quantization Matrix
 *****************************************************************************/
typedef struct quant_matrix_s
{
    int *       pi_matrix;
    boolean_t   b_allocated;
                          /* Has the matrix been allocated by vpar_headers ? */
} quant_matrix_t;

extern int *    pi_default_intra_quant;
extern int *    pi_default_nonintra_quant;

/*****************************************************************************
 * sequence_t : sequence descriptor
 *****************************************************************************/
typedef struct sequence_s
{
    u32                 i_height, i_width, i_size;
    u32                 i_mb_height, i_mb_width, i_mb_size;
    unsigned int        i_aspect_ratio;
    double              d_frame_rate;
    boolean_t           b_mpeg2;
    boolean_t           b_progressive;
    unsigned int        i_scalable_mode;
    f_slice_header_t    pf_slice_header;
    quant_matrix_t      intra_quant, nonintra_quant;
    quant_matrix_t      chroma_intra_quant, chroma_nonintra_quant;
    void                (*pf_decode_mv)( struct vpar_thread_s *, int );
    f_chroma_pattern_t  pf_decode_pattern;

    /* Chromatic information */
    unsigned int        i_chroma_format;
    int                 i_chroma_nb_blocks;
    u32                 i_chroma_width;
    u32                 i_chroma_mb_width, i_chroma_mb_height;

    /* Parser context */
    picture_t *         p_forward;
    picture_t *         p_backward;

    /* Copyright extension */
    boolean_t               b_copyright_flag;     /* Whether the following
                                                     information is significant
                                                     or not. */
    u8                      i_copyright_id;
    boolean_t               b_original;
    u64                     i_copyright_nb;
} sequence_t;

/*****************************************************************************
 * picture_parsing_t : parser context descriptor
 *****************************************************************************/
typedef struct picture_parsing_s
{
    boolean_t           b_full_pel_forward_vector, b_full_pel_backward_vector;
    int                 i_forward_f_code, i_backward_f_code;

    int                 ppi_f_code[2][2];
    int                 i_intra_dc_precision;
    boolean_t           b_frame_pred_frame_dct, b_q_scale_type;
    boolean_t           b_intra_vlc_format;
    boolean_t           b_alternate_scan, b_progressive_frame;
    boolean_t           b_top_field_first, b_concealment_mv;
    boolean_t           b_repeat_first_field;
    int                 i_l_stride, i_c_stride;

    /* Used for second field management */
    int                 i_current_structure;

    picture_t *         p_picture;
    macroblock_t *      pp_mb[MAX_MB];

    /* Relative to the current field */
    int                 i_coding_type, i_structure;
    boolean_t           b_frame_structure;
    f_macroblock_type_t pf_macroblock_type;

    boolean_t           b_error;
} picture_parsing_t;

/*****************************************************************************
 * slice_parsing_t : parser context descriptor #2
 *****************************************************************************/
typedef struct slice_parsing_s
{
    unsigned char       i_quantizer_scale;
    int                 pi_dc_dct_pred[3];          /* ISO/IEC 13818-2 7.2.1 */
    int                 pppi_pmv[2][2][2];  /* Motion vect predictors, 7.6.3 */
} slice_parsing_t;

/*****************************************************************************
 * Standard codes
 *****************************************************************************/
#define PICTURE_START_CODE      0x100
#define SLICE_START_CODE_MIN    0x101
#define SLICE_START_CODE_MAX    0x1AF
#define USER_DATA_START_CODE    0x1B2
#define SEQUENCE_HEADER_CODE    0x1B3
#define SEQUENCE_ERROR_CODE     0x1B4
#define EXTENSION_START_CODE    0x1B5
#define SEQUENCE_END_CODE       0x1B7
#define GROUP_START_CODE        0x1B8

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
