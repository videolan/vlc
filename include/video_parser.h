/*******************************************************************************
 * video_parser.h : video parser thread
 * (c)1999 VideoLAN
 *******************************************************************************
 *******************************************************************************
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
 *******************************************************************************/

/*******************************************************************************
 * sequence_t : sequence descriptor
 *******************************************************************************
 * ??
 *******************************************************************************/
typedef struct sequence_s
{
    u16                 i_height, i_width;
    u16                 i_mb_height, i_mb_width;
    unsigned int        i_aspect_ratio;
    double              d_frame_rate;
    unsigned int        i_chroma_format;
    boolean_t           b_mpeg2;
    boolean_t           b_progressive;
    
    /* Parser context */
    picture_t *             p_forward, p_backward;
    pel_lookup_table_t *    p_frame_lum_lookup, p_field_lum_lookup;
    pel_lookup_table_t *    p_frame_chroma_lookup, p_field_chroma_lookup;
    quant_matrix_t          intra_quant, nonintra_quant;
    quant_matrix_t          chroma_intra_quant, chroma_nonintra_quant;
} sequence_t;

/*******************************************************************************
 * vpar_thread_t: video parser thread descriptor
 *******************************************************************************
 * ??
 *******************************************************************************/
typedef struct vpar_thread_s
{
    /* Thread properties and locks */
    boolean_t           b_die;                                   /* `die' flag */
    boolean_t           b_run;                                   /* `run' flag */
    boolean_t           b_error;                               /* `error' flag */
    boolean_t           b_active;                             /* `active' flag */
    vlc_thread_t        thread_id;                  /* id for thread functions */

    /* Thread configuration */
    /* ?? */
 /*??*/
//    int *pi_status;
    

    /* Input properties */
    decoder_fifo_t      fifo;                                /* PES input fifo */

    /* The bit stream structure handles the PES stream at the bit level */
    bit_stream_t        bit_stream;

    /* Output properties */
    vout_thread_t *     p_vout;                         /* video output thread */
    int                 i_stream;                           /* video stream id */
    
    /* Decoder properties */
    struct vdec_thread_s *      p_vdec[MAX_VDEC];
    video_fifo_t                vfifo;
    video_buffer_t              vbuffer;

    /* Parser properties */
    sequence_t          sequence;

#ifdef STATS
    /* Statistics */
    count_t         c_loops;                               /* number of loops */
    count_t         c_idle_loops;                     /* number of idle loops */
    count_t         c_sequences;                       /* number of sequences */
    count_t         c_pictures;                    /* number of pictures read */
    count_t         c_i_pictures;                /* number of I pictures read */
    count_t         c_p_pictures;                /* number of P pictures read */
    count_t         c_b_pictures;                /* number of B pictures read */    
    count_t         c_decoded_pictures;         /* number of pictures decoded */
    count_t         c_decoded_i_pictures;     /* number of I pictures decoded */
    count_t         c_decoded_p_pictures;     /* number of P pictures decoded */
    count_t         c_decoded_b_pictures;     /* number of B pictures decoded */
#endif
} vpar_thread_t;

/*******************************************************************************
 * Standard start codes
 *******************************************************************************/
#define PICTURE_START_CODE      0x100
#define SLICE_START_CODE_MIN    0x101
#define SLICE_START_CODE_MAX    0x1AF
#define USER_DATA_START_CODE    0x1B2
#define SEQUENCE_HEADER_CODE    0x1B3
#define SEQUENCE_ERROR_CODE     0x1B4
#define EXTENSION_START_CODE    0x1B5
#define SEQUENCE_END_CODE       0x1B7
#define GROUP_START_CODE        0x1B8

/*******************************************************************************
 * Prototypes
 *******************************************************************************/

/* Thread management functions */
vpar_thread_t * vpar_CreateThread       ( /* video_cfg_t *p_cfg, */ input_thread_t *p_input /*,
                                          vout_thread_t *p_vout, int *pi_status */ );
void            vpar_DestroyThread      ( vpar_thread_t *p_vpar /*, int *pi_status */ );

/* Time management functions */
/* ?? */

/* Dynamic thread settings */
/* ?? */
