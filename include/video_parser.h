/*****************************************************************************
 * video_parser.h : video parser thread
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
 *  "vpar_headers.h"
 *****************************************************************************/

/*****************************************************************************
 * vpar_thread_t: video parser thread descriptor
 *****************************************************************************
 * ??
 *****************************************************************************/
typedef struct vpar_thread_s
{
    /* Thread properties and locks */
    boolean_t           b_die;                                 /* `die' flag */
    boolean_t           b_run;                                 /* `run' flag */
    boolean_t           b_error;                             /* `error' flag */
    boolean_t           b_active;                           /* `active' flag */
    vlc_thread_t        thread_id;                /* id for thread functions */

    /* Thread configuration */
    /* ?? */
 /*??*/
//    int *pi_status;
    

    /* Input properties */
    decoder_fifo_t      fifo;                              /* PES input fifo */

    /* The bit stream structure handles the PES stream at the bit level */
    bit_stream_t        bit_stream;

    /* Output properties */
    vout_thread_t *     p_vout;                       /* video output thread */
    int                 i_stream;                         /* video stream id */
    
    /* Decoder properties */
    struct vdec_thread_s *      p_vdec[NB_VDEC];
    video_fifo_t                vfifo;
    video_buffer_t              vbuffer;

    /* Parser properties */
    sequence_t              sequence;
    picture_parsing_t       picture;
    slice_parsing_t         slice;
    macroblock_parsing_t    mb;
    video_synchro_t         synchro;

    /* Lookup tables */
#ifdef MPEG2_COMPLIANT
    s16                     pi_crop_buf[65536];
    s16 *                   pi_crop;
#endif
    lookup_t                pl_mb_addr_inc[2048];    /* for macroblock
                                                        address increment */
    /* variable length codes for the structure dct_dc_size */
    lookup_t                pppl_dct_dc_size[2][2][32];  
    /* tables for macroblock types 0=P 1=B */
    lookup_t                pl_mb_type[2][64];
    /* table for coded_block_pattern */
    lookup_t                pl_coded_pattern[512];

#ifdef STATS
    /* Statistics */
    count_t         c_loops;                              /* number of loops */
    count_t         c_idle_loops;                    /* number of idle loops */
    count_t         c_sequences;                      /* number of sequences */
    count_t         c_pictures;                   /* number of pictures read */
    count_t         c_i_pictures;               /* number of I pictures read */
    count_t         c_p_pictures;               /* number of P pictures read */
    count_t         c_b_pictures;               /* number of B pictures read */
    count_t         c_decoded_pictures;        /* number of pictures decoded */
    count_t         c_decoded_i_pictures;    /* number of I pictures decoded */
    count_t         c_decoded_p_pictures;    /* number of P pictures decoded */
    count_t         c_decoded_b_pictures;    /* number of B pictures decoded */
#endif
} vpar_thread_t;

/* Chroma types */
#define CHROMA_420 1
#define CHROMA_422 2
#define CHROMA_444 3

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/* Thread management functions */
vpar_thread_t * vpar_CreateThread       ( /* video_cfg_t *p_cfg, */ input_thread_t *p_input /*,
                                          vout_thread_t *p_vout, int *pi_status */ );
void            vpar_DestroyThread      ( vpar_thread_t *p_vpar /*, int *pi_status */ );

/* Time management functions */
/* ?? */

/* Dynamic thread settings */
/* ?? */

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

    p_vpar->slice.i_quantizer_scale = ppi_quantizer_scale
           [(!p_vpar->sequence.b_mpeg2 << 1) | p_vpar->picture.b_q_scale_type]
           [GetBits( &p_vpar->bit_stream, 5 )];
}

