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
typedef struct picture_s
{
    /* Type and flags - should NOT be modified except by the vout thread */
    int             i_type;                                    /* picture type */
    int             i_status;                                 /* picture flags */
    int             i_matrix_coefficients;       /* in YUV type, encoding type */    

    /* Picture management properties - these properties can be modified using
     * the video output thread API, but should ne be written directly */
    int             i_refcount;                      /* link reference counter */
    mtime_t         date;                                      /* display date */
    
    /* Picture static properties - those properties are fixed at initialization
     * and should NOT be modified */
    int             i_width;                                  /* picture width */
    int             i_height;                                /* picture height */
    int             i_chroma_width;                            /* chroma width */

    /* Picture dynamic properties - those properties can be changed by the 
     * decoder */
    int             i_display_horizontal_offset;     /* ISO/IEC 13818-2 6.3.12 */
    int             i_display_vertical_offset;       /* ISO/IEC 13818-2 6.3.12 */
    int             i_display_width;                   /* useful picture width */
    int             i_display_height;                 /* useful picture height */
    int             i_aspect_ratio;                            /* aspect ratio */  
    
    /* Macroblock counter - the decoder use it to verify if it has
     * decoded all the macroblocks of the picture */
    int             i_deccount;
    vlc_mutex_t     lock_deccount;
    
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
#define RESERVED_DATED_PICTURE  2     /* picture is waiting for DisplayPicture */
#define RESERVED_DISP_PICTURE   3      /* picture is waiting for a DatePixture */
#define READY_PICTURE           4              /* picture is ready for display */
#define DISPLAYED_PICTURE       5  /* picture has been displayed but is linked */
#define DESTROYED_PICTURE       6     /* picture is allocated but no more used */

/* Aspect ratios (ISO/IEC 13818-2 section 6.3.3, table 6-3) */
#define AR_SQUARE_PICTURE       1                             /* square pixels */
#define AR_3_4_PICTURE          2                          /* 3:4 picture (TV) */
#define AR_16_9_PICTURE         3                /* 16:9 picture (wide screen) */
#define AR_221_1_PICTURE        4                    /* 2.21:1 picture (movie) */

/*******************************************************************************
 * subpicture_t: video sub picture unit                                            
 *******************************************************************************
 * Any sub picture unit destined to be displayed by a video output thread should
 * be stored in this structure from it's creation to it's effective display.
 * Subtitle type and flags should only be modified by the output thread. Note
 * that an empty subtitle MUST have its flags set to 0.
 *******************************************************************************/
typedef struct subpicture_s
{
    /* Type and flags - should NOT be modified except by the vout thread */
    int             i_type;                                        /* spu type */
    int             i_status;                                     /* spu flags */

    /* Other properties */
    mtime_t         begin_date;                   /* beginning of display date */
    mtime_t         end_date;                           /* end of display date */

    /* Display properties - these properties are only indicative and may be
     * changed by the video output thread */
    int             i_x;                     /* offset from alignment position */
    int             i_y;                     /* offset from alignment position */
    int             i_width;                                  /* picture width */
    int             i_height;                                /* picture height */
    int             i_horizontal_align;                /* horizontal alignment */
    int             i_vertical_align;                    /* vertical alignment */

    /* Sub picture unit data - data can always be freely modified. p_data itself 
     * (the pointer) should NEVER be modified. */
    void *          p_data;                                        /* spu data */    
} subpicture_t;

/* Subpicture type */
#define EMPTY_SUBPICTURE        0      /* subtitle slot is empty and available */
#define RLE_SUBPICTURE          100                    /* RLE encoded subtitle */
#define TEXT_SUBPICTURE         200                 /* iso8859-1 text subtitle */

/* Subpicture status */
#define FREE_SUBPICTURE         0      /* subpicture is free and not allocated */
#define RESERVED_SUBPICTURE     1      /* subpicture is allocated and reserved */
#define READY_SUBPICTURE        2           /* subpicture is ready for display */
#define DESTROYED_SUBPICTURE    3  /* subpicture is allocated but no more used */

/* Alignment types */
#define RIGHT_ALIGN             10                  /* x is absolute for right */
#define LEFT_ALIGN              11                   /* x is absolute for left */
#define RIGHT_RALIGN            12       /* x is relative for right from right */
#define LEFT_RALIGN             13         /* x is relative for left from left */

#define CENTER_ALIGN            20             /* x, y are absolute for center */
#define CENTER_RALIGN           21 /* x, y are relative for center from center */

#define BOTTOM_ALIGN            30                 /* y is absolute for bottom */
#define TOP_ALIGN               31                    /* y is absolute for top */
#define BOTTOM_RALIGN           32     /* y is relative for bottom from bottom */
#define TOP_RALIGN              33           /* y is relative for top from top */
#define SUBTITLE_RALIGN         34   /* y is relative for center from subtitle */


