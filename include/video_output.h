/*******************************************************************************
 * video_output.h : video output thread
 * (c)1999 VideoLAN
 *******************************************************************************
 * This module describes the programming interface for video output threads.
 * It includes functions allowing to open a new thread, send pictures to a
 * thread, and destroy a previously oppenned video output thread.
 *******************************************************************************/

/*******************************************************************************
 * vout_yuv_convert_t: YUV convertion function
 *******************************************************************************
 * This is the prototype common to all convertion functions. The type of p_pic
 * will change depending of the screen depth treated.
 * Parameters:
 *      p_vout                          video output thread
 *      p_pic                           picture address 
 *      p_y, p_u, p_v                   Y,U,V samples addresses
 *      i_width, i_height               Y samples extension
 *      i_pic_width, i_pic_height       picture extension
 *      i_pic_line_width                picture total line width
 *      i_matrix_coefficients           matrix coefficients
 * Picture width and source dimensions must be multiples of 16.
 *******************************************************************************/
typedef void (vout_yuv_convert_t)( p_vout_thread_t p_vout, void *p_pic,
                                   yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                   int i_width, int i_height,
                                   int i_pic_width, int i_pic_height, int i_pic_line_width,
                                   int i_matrix_coefficients );

/*******************************************************************************
 * vout_yuv_t: pre-calculated YUV convertion tables
 *******************************************************************************
 * These tables are used by convertion and scaling functions.
 *******************************************************************************/
typedef struct vout_yuv_s
{
    /* Convertion functions */
    vout_yuv_convert_t *        p_Convert420;          /* YUV 4:2:0 converter */
    vout_yuv_convert_t *        p_Convert422;          /* YUV 4:2:2 converter */
    vout_yuv_convert_t *        p_Convert444;          /* YUV 4:4:4 converter */

    /* Pre-calculated convertion tables */
    void *              p_base;             /* base for all convertion tables */    
    union
    {
        u8 *            p_gray8;                         /* gray 8 bits table */
        u16 *           p_gray16;                       /* gray 16 bits table */
        u32 *           p_gray32;                       /* gray 32 bits table */
        u8 *            p_rgb8;                           /* RGB 8 bits table */
        u16 *           p_rgb16;                         /* RGB 16 bits table */
        u32 *           p_rgb32;                         /* RGB 32 bits table */
    } yuv;

    /* Temporary convertion buffer and offset array */
    void *              p_buffer;                        /* convertion buffer */
    int *               p_offset;                             /* offset array */    
} vout_yuv_t;

/*******************************************************************************
 * vout_buffer_t: rendering buffer
 *******************************************************************************
 * This structure store informations about a buffer. Buffers are not completely
 * cleared between displays, and modified areas needs to be stored.
 *******************************************************************************/
typedef struct vout_buffer_s
{     
    /* Picture area */
    int         i_pic_x, i_pic_y;                        /* picture position  */
    int         i_pic_width, i_pic_height;               /* picture extension */
    
    /* Other areas - only vertical extensions of areas are stored */
    int         i_areas;                                   /* number of areas */
    int         pi_area_begin[VOUT_MAX_AREAS];           /* beginning of area */
    int         pi_area_end[VOUT_MAX_AREAS];                   /* end of area */
    
    /* Picture data */
    byte_t *    p_data;                                     /* memory address */
} vout_buffer_t;

/*******************************************************************************
 * vout_thread_t: video output thread descriptor
 *******************************************************************************
 * Any independant video output device, such as an X11 window or a GGI device,
 * is represented by a video output thread, and described using following 
 * structure.
 *******************************************************************************/
typedef struct vout_thread_s
{
    /* Thread properties and lock */
    boolean_t           b_die;                                  /* `die' flag */
    boolean_t           b_error;                              /* `error' flag */
    boolean_t           b_active;                            /* `active' flag */
    vlc_thread_t        thread_id;                /* id for pthread functions */
    vlc_mutex_t         picture_lock;                    /* picture heap lock */
    vlc_mutex_t         subpicture_lock;              /* subpicture heap lock */
    vlc_mutex_t         change_lock;                    /* thread change lock */
    int *               pi_status;                   /* temporary status flag */
    p_vout_sys_t        p_sys;                        /* system output method */

    /* Current display properties */    
    u16                 i_changes;              /* changes made to the thread */
    int                 i_width;               /* current output method width */
    int                 i_height;             /* current output method height */
    int                 i_bytes_per_line;   /* bytes per line (incl. virtual) */
    int                 i_screen_depth;   /* significant bpp: 8, 15, 16 or 24 */
    int                 i_bytes_per_pixel; /* real screen depth: 1, 2, 3 or 4 */
    float               f_gamma;                                     /* gamma */

    /* Color masks and shifts in RGB mode - masks are set by system 
     * initialization, shifts are calculated. A pixel color value can be 
     * obtained using the formula ((value >> rshift) << lshift) */
    u32                 i_red_mask;                               /* red mask */
    u32                 i_green_mask;                           /* green mask */
    u32                 i_blue_mask;                             /* blue mask */
    int                 i_red_lshift, i_red_rshift;             /* red shifts */
    int                 i_green_lshift, i_green_rshift;       /* green shifts */
    int                 i_blue_lshift, i_blue_rshift;          /* blue shifts */

    /* Useful pre-calculated pixel values - these are not supposed to be 
     * accurate values, but rather values looking nice, given their usage. */
    u32                 i_white_pixel;                               /* white */
    u32                 i_black_pixel;                               /* black */
    u32                 i_gray_pixel;                                 /* gray */
    u32                 i_blue_pixel;                                 /* blue */
    
    /* Palette */
    u8                  lookup[1377];       /* lookup table for 8 bpp palette */

    /* Pictures and rendering properties */
    boolean_t           b_grayscale;            /* color or grayscale display */
    boolean_t           b_info;             /* print additionnal informations */
    boolean_t           b_interface;                      /* render interface */
    boolean_t           b_scale;                     /* allow picture scaling */

    /* Idle screens management */
    mtime_t             last_display_date;      /* last non idle display date */
    mtime_t             last_idle_date;             /* last idle display date */

#ifdef STATS    
    /* Statistics - these numbers are not supposed to be accurate, but are a
     * good indication of the thread status */
    mtime_t             render_time;              /* last picture render time */
    count_t             c_fps_samples;                      /* picture counts */
    mtime_t             p_fps_sample[ VOUT_FPS_SAMPLES ];/* FPS samples dates */
#endif

    /* Rendering buffers */
    int                 i_buffer_index;                       /* buffer index */
    vout_buffer_t       p_buffer[2];                    /* buffers properties */

    /* Videos heap and translation tables */
    picture_t           p_picture[VOUT_MAX_PICTURES];             /* pictures */
    subpicture_t        p_subpicture[VOUT_MAX_PICTURES];       /* subpictures */
    vout_yuv_t          yuv;                            /* translation tables */

    /* Bitmap fonts */
    p_vout_font_t       p_default_font;                       /* default font */
    p_vout_font_t       p_large_font;                           /* large font */
} vout_thread_t;

/* Flags for changes - these flags are set in the i_changes field when another
 * thread changed a variable */
#define VOUT_INFO_CHANGE        0x0001                      /* b_info changed */
#define VOUT_GRAYSCALE_CHANGE   0x0002                 /* b_grayscale changed */
#define VOUT_INTF_CHANGE        0x0004                 /* b_interface changed */
#define VOUT_SCALE_CHANGE       0x0008                     /* b_scale changed */
#define VOUT_SIZE_CHANGE        0x0200                        /* size changed */
#define VOUT_DEPTH_CHANGE       0x0400                       /* depth changed */
#define VOUT_GAMMA_CHANGE       0x0010                       /* gamma changed */
#define VOUT_YUV_CHANGE         0x0800                   /* change yuv tables */
#define VOUT_NODISPLAY_CHANGE   0xff00     /* changes which forbidden display */

/*******************************************************************************
 * Macros
 *******************************************************************************/

/* RGB2PIXEL: assemble RGB components to a pixel value, returns a u32 */
#define RGB2PIXEL( p_vout, i_red, i_green, i_blue )                            \
    (((((u32)i_red)   >> p_vout->i_red_rshift)   << p_vout->i_red_lshift)   |  \
     ((((u32)i_green) >> p_vout->i_green_rshift) << p_vout->i_green_lshift) |  \
     ((((u32)i_blue)  >> p_vout->i_blue_rshift)  << p_vout->i_blue_lshift))


/*******************************************************************************
 * Prototypes
 *******************************************************************************/
vout_thread_t * vout_CreateThread       ( char *psz_display, int i_root_window, 
                                          int i_width, int i_height, int *pi_status );
void            vout_DestroyThread      ( vout_thread_t *p_vout, int *pi_status );
picture_t *     vout_CreatePicture      ( vout_thread_t *p_vout, int i_type, 
                                          int i_width, int i_height );
void            vout_DestroyPicture     ( vout_thread_t *p_vout, picture_t *p_pic );
void            vout_DisplayPicture     ( vout_thread_t *p_vout, picture_t *p_pic );
void            vout_DatePicture        ( vout_thread_t *p_vout, picture_t *p_pic, mtime_t date );
void            vout_LinkPicture        ( vout_thread_t *p_vout, picture_t *p_pic );
void            vout_UnlinkPicture      ( vout_thread_t *p_vout, picture_t *p_pic );
subpicture_t *  vout_CreateSubPicture   ( vout_thread_t *p_vout, int i_type, int i_size );
void            vout_DestroySubPicture  ( vout_thread_t *p_vout, subpicture_t *p_subpic );
void            vout_DisplaySubPicture  ( vout_thread_t *p_vout, subpicture_t *p_subpic );

void            vout_SetBuffers         ( vout_thread_t *p_vout, void *p_buf1, void *p_buf2 );

