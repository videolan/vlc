/*****************************************************************************
 * vpar_blocks.h : video parser blocks management
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
 * macroblock_t : information on a macroblock
 *****************************************************************************/
typedef struct macroblock_s
{
    picture_t *             p_picture;
    int                     i_mb_x, i_mb_y;
    int                     i_structure;
    int                     i_l_x, i_l_y;    /* position of macroblock (lum) */
    int                     i_c_x, i_c_y; /* position of macroblock (chroma) */
    int                     i_structure;
    int                     i_chroma_nb_blocks;  /* nb of bks for a chr comp */

    /* IDCT information */
    elem_t                  ppi_blocks[12][64];                    /* blocks */
    f_idct_t                pf_idct[12];             /* sparse IDCT or not ? */
    int                     pi_sparse_pos[12];

    /* Motion compensation information */
    f_motion_t              pf_motion;    /* function to use for motion comp */
    f_chroma_motion_t       pf_chroma_motion;
    picture_t *             p_backw_top, p_backw_bot;
    picture_t *             p_forw_top, p_forw_bot;
    int                     i_field_select_backw_top, i_field_select_backw_bot;
    int                     i_field_select_forw_top, i_field_select_forw_bot;
    int                     pi_motion_vectors_backw_top[2];
    int                     pi_motion_vectors_backw_bot[2];
    int                     pi_motion_vectors_forw_top[2];
    int                     pi_motion_vectors_forw_bot[2];

    /* AddBlock information */
    f_addb_t                pf_addb[12];
    data_t *                p_data[12];    /* positions of blocks in picture */
    int                     i_lum_incr, i_chroma_incr;
} macroblock_t;

/*****************************************************************************
 * macroblock_parsing_t : parser context descriptor #3
 *****************************************************************************/
typedef struct
{
    int                     i_mb_type, i_motion_type, i_mv_count, i_mv_format;
    int                     i_coded_block_pattern;
    boolean_t               b_dct_type;
} macroblock_parsing_t;

/*****************************************************************************
 * LoadQuantizerScale
 *****************************************************************************
 * Quantizer scale factor (ISO/IEC 13818-2 7.4.2.2)
 *****************************************************************************/
static __inline__ void LoadQuantizerScale( vpar_thread_t * p_vpar )
{
    /* Quantization coefficient table */
    static unsigned char    ppi_quantizer_scale[3][32] =
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
