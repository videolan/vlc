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
 * pixel_t: universal pixel value descriptor
 *******************************************************************************
 * This type and associated macros and functions are provided as an universal
 * way of storing colors/pixels parameters. For pixels, it represents the
 * actual value of the pixel. For RGB values, it is a 24 bits RGB encoded
 * value. For masks, it is 0 or 1...
 *******************************************************************************/
typedef u32 pixel_t;

#define RGBVALUE2RED( value )   (  (value)        & 0x0000ff )
#define RGBVALUE2GREEN( value ) ( ((value) >> 8)  & 0x0000ff )
#define RGBVALUE2BLUE( value )  ( ((value) >> 16) & 0x0000ff )

/*******************************************************************************
 * picture_t: video picture                                            
 *******************************************************************************
 * Any picture destined to be displayed by a video output thread should be 
 * stored in this structure from it's creation to it's effective display.
 * Two forms of pictures exists: independant pictures, which can be manipulated
 * freely, although usage of graphic library is recommanded, and heap pictures.
 * Extreme care should be taken when manipulating heap pictures, since any error
 * could cause a segmentation fault in the video output thread. The rule is:
 * once a picture is in the video heap, only it's data can be written. All other
 * fields should only be read or modified using interface functions.
 * Note that for all pictures, some properties should never be modified, except
 * by the video output thread itself, once the picture has been created !
 *******************************************************************************/
typedef struct
{
    /* Type and flags - should NOT be modified except by the vout thread */
    int             i_type;                                    /* picture type */
    int             i_flags;                                  /* picture flags */
    
    /* Picture properties - those properties are fixed at initialization and 
     * should NOT be modified */
    int             i_width;                                  /* picture width */
    int             i_height;                                /* picture height */
    int             i_bpp;                            /* padded bits per pixel */
    int             i_bytes_per_line;        /* total number of bytes per line */
    
    /* Picture properties - those properties can be modified is the picture is
     * independant, or in a heap but permanent or reserved */
    int             i_x;                 /* x position offset in output window */
    int             i_y;                 /* y position offset in output window */
    int             i_h_align;                         /* horizontal alignment */
    int             i_v_align;                           /* vertical alignment */
    int             i_h_ratio;                     /* horizontal display ratio */
    int             i_v_ratio;                       /* vertical display ratio */
    int             i_level;                     /* overlay hierarchical level */

    /* Link reference counter - it can be modified using vout_Link and 
     * vout_Unlink functions, or directly if the picture is independant */
    int             i_refcount;                      /* link reference counter */

    /* Video properties - those properties should not be modified once 
     * the picture is in a heap, but can be freely modified if it is 
     * independant */
    int             i_stream;                               /* video stream id */
    mtime_t         date;                                      /* display date */
    mtime_t         duration;                 /* duration for overlay pictures */

    /* Picture data - data can always be freely modified, although special care
     * should be taken for permanent pictures to avoid flickering - p_data 
     * itself (the pointer) should NEVER be modified */   
    pixel_t         pixel;                   /* pixel value, for mask pictures */
    byte_t *        p_data;                                    /* picture data */
} picture_t;

/* Pictures types */
#define EMPTY_PICTURE           0             /* picture is waiting to be used */
#define RGB_BLANK_PICTURE       10       /* blank picture (rgb color, no data) */
#define PIXEL_BLANK_PICTURE     11     /* blank picture (pixel color, no data) */
#define RGB_PICTURE             20           /* picture is 24 bits rgb encoded */
#define PIXEL_PICTURE           30                 /* picture is pixel encoded */
#define RGB_MASK_PICTURE        40              /* picture is a 1 bpp rgb mask */
#define PIXEL_MASK_PICTURE      41            /* picture is a 1 bpp pixel mask */

/* ?? */
#define YUV_444_PICTURE         100                  /* chroma 444 YUV picture */
#define YUV_422_PICTURE         101                  /* chroma 422 YUV picture */
#define YUV_420_PICTURE         102                  /* chroma 420 YUV picture */

/* Pictures properties (flags) */
#define RESERVED_PICTURE        (1 << 0)  /* picture is not ready but reserved */
#define PERMANENT_PICTURE       (1 << 1)               /* picture is permanent */
#define DISPLAYED_PICTURE       (1 << 2)         /* picture has been displayed */
#define OWNER_PICTURE           (1 << 3)              /* picture owns its data */
#define DISPLAY_PICTURE	        (1 << 4)          /* picture will be displayed */
#define DESTROY_PICTURE	        (1 << 5)          /* picture will be destroyed */
#define TRANSPARENT_PICTURE     (1 << 8)             /* picture is transparent */
#define OVERLAY_PICTURE	        (1 << 9)       /* picture overlays another one */

/* Alignments - this field describe how the position of the picture will
 * be calculated */
#define ALIGN_LEFT              -1                             /* left-aligned */
#define ALIGN_TOP               -1                               /* up-aligned */
#define ALIGN_ENTER             0                                  /* centered */
#define ALIGN_RIGHT             1                             /* right-aligned */
#define ALIGN_BOTTOM            1                            /* bottom-aligned */
#define ALIGN_H_DEFAULT         ALIGN_LEFT     /* default horizontal alignment */
#define ALIGN_V_DEFAULT 	    ALIGN_TOP        /* default vertical alignment */

/* Display ratios - this field describe how the image will be resized before
 * being displayed */
#define DISPLAY_RATIO_HALF      -1                            /* 1:2 half size */
#define DISPLAY_RATIO_NORMAL    0                           /* 1:1 normal size */
#define DISPLAY_RATIO_DOUBLE    1                           /* 2:1 double size */
/* ?? add other ratios (TRIPLE, THIRD), TV, automatic, ... */

/*******************************************************************************
 * video_cfg_t: video object configuration structure
 *******************************************************************************
 * This structure is passed as a parameter to many initialization function of
 * the vout and vdec modules. It includes many fields describing potential 
 * properties of a new object. The 'i_properties' field allow to set only a 
 * subset of the required properties, asking the called function to use default 
 * settings for the other ones.
 *******************************************************************************/
typedef struct video_cfg_s
{
    u64         i_properties;                               /* used properties */

    /* Size properties */
    int         i_width;                              /* image or window width */
    int         i_height;                            /* image or window height */
    int         i_size;                                           /* heap size */

    /* X11 properties */
    char *      psz_display;                                   /* display name */
    char *      psz_title;                                     /* window title */
    boolean_t   b_shm_ext;                        /* try to use XShm extension */

    /* Pictures properties */
    int         i_type;                                        /* picture type */
    int         i_flags;                                      /* picture flags */
    int         i_bpp;                                /* padded bits per pixel */
    int         i_x;                     /* x position offset in output window */
    int         i_y;                     /* y position offset in output window */
    int         i_h_align;                             /* horizontal alignment */
    int         i_v_align;                               /* vertical alignment */
    int         i_h_ratio;                         /* horizontal display ratio */
    int         i_v_ratio;                           /* vertical display ratio */
    int         i_level;                         /* overlay hierarchical level */
    int         i_refcount;                          /* link reference counter */
    int         i_stream;                                   /* video stream id */
    mtime_t     date;                                  /* picture display date */
    mtime_t     duration;                     /* duration for overlay pictures */
    pixel_t     pixel;                       /* pixel value, for mask pictures */
    byte_t *    p_data;                                        /* picture data */
} video_cfg_t;

/* Properties flags (see picture_t and other video structures for 
 * explanations) */
#define VIDEO_CFG_WIDTH	    (1 << 0)
#define VIDEO_CFG_HEIGHT    (1 << 1)
#define VIDEO_CFG_SIZE      (1 << 2)

#define VIDEO_CFG_DISPLAY   (1 << 4)
#define VIDEO_CFG_TITLE     (1 << 5)
#define VIDEO_CFG_SHM_EXT   (1 << 6)

#define VIDEO_CFG_TYPE      (1 << 8)
#define VIDEO_CFG_FLAGS     (1 << 9)
#define VIDEO_CFG_BPP       (1 << 10)
#define VIDEO_CFG_POSITION  (1 << 11)                      /* both i_x and i_y */
#define VIDEO_CFG_ALIGN     (1 << 12)          /* both i_h_align and i_v_align */
#define VIDEO_CFG_RATIO     (1 << 13)          /* both i_h_ratio and i_y_ratio */
#define VIDEO_CFG_LEVEL     (1 << 14)
#define VIDEO_CFG_REFCOUNT  (1 << 15)
#define VIDEO_CFG_STREAM    (1 << 16)
#define VIDEO_CFG_DATE      (1 << 17)
#define VIDEO_CFG_DURATION  (1 << 18)
#define VIDEO_CFG_PIXEL     (1 << 19)
#define VIDEO_CFG_DATA      (1 << 20)
