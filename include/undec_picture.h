/*******************************************************************************
 * undec_picture.h: undecoded pictures type
 * (c)1999 VideoLAN
 *******************************************************************************
 * This header is required by all modules which have to handle pictures. It
 * includes all common video types and constants.
 *******************************************************************************
 * Requires:
 *  "config.h"
 *  "common.h"
 *  "video.h"
 *******************************************************************************/

/*******************************************************************************
 * macroblock_info_t : information on a macroblock
 *******************************************************************************/
typedef struct
{
    int                     i_mb_type;
    int                     i_motion_type;
    int                     i_dct_type;
    (void *)                p_idct_function[12](coeff_t * p_block);
    
    int                     ppp_motion_vectors[2][2][2];
    int                     pi_field_select[2][2];
} macroblock_info_t;

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

/*******************************************************************************
 * undec_link_t : link to an undecoded picture
 *******************************************************************************/
typedef struct undec_link_s
{
    struct undec_picture_s *    p_undec;
    picture_t **                pp_frame;
} undec_link_t

/*******************************************************************************
 * undec_picture_t: undecoded picture
 *******************************************************************************
 * Any picture destined to be decoded by a video decoder thread should be 
 * stored in this structure from it's creation to it's effective display.
 *******************************************************************************/
typedef struct undec_picture_s
{
	/* Picture data */
    picture_t *                 p_picture;

    int                         i_coding_type;
    boolean_t                   b_mpeg2;
    int                         i_mb_height, i_mb_width;
    int                         i_structure;
    mtime_t                     i_pts;

    macroblock_info_t *         p_mb_info;

    picture_t *                 p_backward_ref;
    picture_t *                 p_forward_ref;
    
    undec_link_t                pp_referencing_undec[MAX_REFERENCING_UNDEC];
} undec_picture_t;


/* Pictures types */
#define I_CODING_TYPE           1
#define P_CODING_TYPE           2
#define B_CODING_TYPE           3
#define D_CODING_TYPE           4 /* MPEG-1 ONLY */
/* other values are reserved */

/* Structures */
#define TOP_FIRST               1
#define BOTTOM_FIRST            2
#define FRAME_STRUCTURE         3

/*******************************************************************************
 * pel_lookup_table_t : lookup table for pixels
 *******************************************************************************/
#ifdef BIG_PICTURES
#   define PEL_P                u32
#else
#   define PEL_P                u16
#endif

typedef struct pel_lookup_table_s {
    PEL_P *                     pi_pel;

    /* When the size of the picture changes, this structure is freed, so we
     * keep a reference count. */
    int                         i_refcount;
    vlc_mutex_t                 lock;
} pel_lookup_table_t;

#define LINK_LOOKUP(p_l) \
    vlc_mutex_lock( (p_l)->lock ); \
    (p_l)->i_refcount++; \
    vlc_mutex_unlock( (p_l)->lock );

#define UNLINK_LOOKUP(p_l) \
    vlc_mutex_lock( (p_l)->lock ); \
    (p_l)->i_refcount--; \
    if( (p_l)->i_refcount <= 0 ) \
    { \
        vlc_mutex_unlock( (p_l)->lock ); \
        free( p_l ); \
    } \
    vlc_mutex_unlock( (p_l)->lock );
