/*******************************************************************************
 * video.h: common video definitions
 * (c)1999 VideoLAN
 *******************************************************************************
 * This header is required by all modules which have to handle pictures. It
 * includes all common video types and constants.
 *******************************************************************************
 * Requires:
 *  "config.h"
 *  "common.h"
 *  "mtime.h"
 *******************************************************************************/

/*******************************************************************************
 * yuv_data_t: type for storing one Y, U or V sample.
 *******************************************************************************/
typedef u8 yuv_data_t;

/*******************************************************************************
 * picture_t: video picture                                            
 *******************************************************************************
 * Any picture destined to be displayed by a video output thread should be 
 * stored in this structure from it's creation to it's effective display.
 * Picture type and flags should only be modified by the output thread. Note
 * that an empty picture MUST have its flags set to 0.
 *******************************************************************************/
typedef struct
{
    /* Type and flags - should NOT be modified except by the vout thread */
    int             i_type;                                    /* picture type */
    int             i_status;                                 /* picture flags */
    int             i_matrix_coefficients;       /* in YUV type, encoding type */    
    
    /* Picture properties - those properties are fixed at initialization and 
     * should NOT be modified. Note that for YUV pictures, i_bytes_per_line is
     * the number of bytes for ONE of the Y, U or V pictures, and therefore the
     * number of bytes in the picture is 3 * i_height * i_bytes_per_line */
    int             i_width;                                  /* picture width */
    int             i_height;                                /* picture height */
    int             i_bytes_per_line;        /* total number of bytes per line */
    
    /* Link reference counter - it can be modified using vout_Link and 
     * vout_Unlink functions, or directly if the picture is independant */
    int             i_refcount;                      /* link reference counter */

    /* Macroblock counter - the decoder use it to verify if it has
     * decoded all the macroblocks of the picture */
    int             i_deccount;
    vlc_mutex_t     lock_deccount;
    
    /* Video properties - those properties should not be modified once 
     * the picture is in a heap, but can be freely modified if it is 
     * independant */
    mtime_t         date;                                      /* display date */

    /* Picture data - data can always be freely modified. p_data itself 
     * (the pointer) should NEVER be modified. In YUV format, the p_y, p_u and
     * p_v data pointers refers to different areas of p_data, and should not
     * be freed */   
    void *          p_data;                                    /* picture data */
    yuv_data_t *    p_y;          /* pointer to beginning of Y image in p_data */
    yuv_data_t *    p_u;          /* pointer to beginning of U image in p_data */
    yuv_data_t *    p_v;          /* pointer to beginning of V image in p_data */
} picture_t;


/* Pictures types */
#define EMPTY_PICTURE           0       /* picture slot is empty and available */
#define YUV_420_PICTURE         100                       /* 4:2:0 YUV picture */
#define YUV_422_PICTURE         101                       /* 4:2:2 YUV picture */
#define YUV_444_PICTURE         102                       /* 4:4:4 YUV picture */

/* Pictures status */
#define FREE_PICTURE            0         /* picture is free and not allocated */
#define RESERVED_PICTURE        1         /* picture is allocated and reserved */
#define READY_PICTURE           2              /* picture is ready for display */
#define DISPLAYED_PICTURE       3  /* picture has been displayed but is linked */
#define DESTROYED_PICTURE       4     /* picture is allocated but no more used */








