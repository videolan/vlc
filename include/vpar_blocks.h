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
    int                     i_structure;
    int                     i_l_x, i_l_y;    /* position of macroblock (lum) */
    int                     i_c_x, i_c_y; /* position of macroblock (chroma) */
    int                     i_chroma_nb_blocks;  /* nb of bks for a chr comp */
    int                     i_l_stride;           /* number of data_t to ignore
					           * when changing lines     */
    int                     i_c_stride;                  /* idem, for chroma */

    /* IDCT information */
    elem_t                  ppi_blocks[12][64];                    /* blocks */
    f_idct_t                pf_idct[12];             /* sparse IDCT or not ? */
    int                     pi_sparse_pos[12];

    /* Motion compensation information */
    f_motion_t              pf_motion;    /* function to use for motion comp */
    f_chroma_motion_t       pf_chroma_motion;
    picture_t *             p_backw_top;
    picture_t *             p_backw_bot;
    picture_t *             p_forw_top;
    picture_t *             p_forw_bot;
    int                     ppi_field_select[2][2];
    int                     pppi_motion_vectors[2][2][2];
    int                     pi_dm_vector[2];
   
    /* AddBlock information */
    f_addb_t                pf_addb[12];      /* pointer to the Add function */
    data_t *                p_data[12];              /* pointer to the position
					              * in the final picture */
    int                     i_addb_l_stride, i_addb_c_stride;
} macroblock_t;

/*****************************************************************************
 * macroblock_parsing_t : parser context descriptor #3
 *****************************************************************************/
typedef struct
{
    int                     i_mb_type, i_motion_type, i_mv_count, i_mv_format;
    boolean_t               b_dmv;
    /* AddressIncrement information */
    int                     i_addr_inc;

    /* Macroblock Type */
    int                     i_coded_block_pattern;
    boolean_t               b_dct_type;

    int                     i_l_x, i_l_y, i_c_x, i_c_y;
} macroblock_parsing_t;

/******************************************************************************
 * lookup_t : entry type for lookup tables                                    *
 ******************************************************************************/

typedef struct lookup_s
{
    int    i_value;
    int    i_length;
} lookup_t;

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
#define SCAN_ZIGZAG                         0
#define SCAN_ALT                            1

/*****************************************************************************
 * Constants
 *****************************************************************************/
extern int *    pi_default_intra_quant;
extern int *    pi_default_nonintra_quant;
extern u8       pi_scan[2][64];

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
void vpar_InitCrop( struct vpar_thread_s* p_vpar );
int vpar_CodedPattern420( struct vpar_thread_s* p_vpar );
int vpar_CodedPattern422( struct vpar_thread_s* p_vpar );
int vpar_CodedPattern444( struct vpar_thread_s* p_vpar );
int  vpar_IMBType( struct vpar_thread_s* p_vpar );
int  vpar_PMBType( struct vpar_thread_s* p_vpar );
int  vpar_BMBType( struct vpar_thread_s* p_vpar );
int  vpar_DMBType( struct vpar_thread_s* p_vpar );
