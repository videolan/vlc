/*******************************************************************************
 * video_output.h : video output thread
 * (c)1999 VideoLAN
 *******************************************************************************
 * This module describes the programming interface for video output threads.
 * It includes functions allowing to open a new thread, send pictures to a
 * thread, and destroy a previously oppenned video output thread.
 *******************************************************************************/

/*******************************************************************************
 * vout_tables_t: pre-calculated convertion tables
 *******************************************************************************
 * These tables are used by convertion and scaling functions.
 *******************************************************************************/
typedef struct vout_tables_s
{
    void *              p_base;             /* base for all translation tables */    
    union 
    {        
        struct { u16 *p_red, *p_green, *p_blue; } rgb16;   /* color 15, 16 bpp */
        struct { u32 *p_red, *p_green, *p_blue; } rgb32;   /* color 24, 32 bpp */
        struct { u16 *p_gray; }                   gray16;   /* gray 15, 16 bpp */
        struct { u32 *p_gray; }                   gray32;   /* gray 24, 32 bpp */
    } yuv;    
} vout_tables_t;

/*******************************************************************************
 * vout_convert_t: convertion function
 *******************************************************************************
 * This is the prototype common to all convertion functions. The type of p_pic
 * will change depending of the screen depth treated.
 * Parameters:
 *      p_vout                  video output thread
 *      p_pic                   picture address (start address in picture)
 *      p_y, p_u, p_v           Y,U,V samples addresses
 *      i_width                 Y samples width
 *      i_height                Y samples height
 *      i_eol                   number of Y samples to reach the next line 
 *      i_pic_eol               number or pixels to reach the next line
 *      i_scale                 if non 0, vertical scaling is 1 - 1/i_scale
 *      i_matrix_coefficients   matrix coefficients
 * Conditions:
 *      start x + i_width                        <  picture width
 *      start y + i_height * (scaling factor)    <  picture height
 *      i_width % 16                             == 0
 *******************************************************************************/
typedef void (vout_convert_t)( p_vout_thread_t p_vout, void *p_pic,
                               yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                               int i_width, int i_height, int i_eol, int i_pic_eol,
                               int i_scale, int i_matrix_coefficients );

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
    boolean_t           b_die;                                   /* `die' flag */
    boolean_t           b_error;                               /* `error' flag */
    boolean_t           b_active;                             /* `active' flag */
    vlc_thread_t        thread_id;                 /* id for pthread functions */
    vlc_mutex_t         picture_lock;                     /* picture heap lock */
    vlc_mutex_t         subtitle_lock;                   /* subtitle heap lock */   
    vlc_mutex_t         change_lock;                     /* thread change lock */    
    int *               pi_status;                    /* temporary status flag */
    p_vout_sys_t        p_sys;                         /* system output method */

    /* Current display properties */    
    boolean_t           b_grayscale;             /* color or grayscale display */   
    int                 i_width;                /* current output method width */
    int                 i_height;              /* current output method height */
    int                 i_bytes_per_line;/* bytes per line (including virtual) */
    int                 i_screen_depth;                      /* bits per pixel */
    int                 i_bytes_per_pixel;                /* real screen depth */
    float               f_gamma;                                      /* gamma */

    /* Pictures and rendering properties */
    boolean_t           b_info;              /* print additionnal informations */

#ifdef STATS    
    /* Statistics - these numbers are not supposed to be accurate, but are a
     * good indication of the thread status */
    mtime_t             render_time;               /* last picture render time */
    count_t             c_fps_samples;                       /* picture counts */    
    mtime_t             fps_sample[ VOUT_FPS_SAMPLES ];   /* FPS samples dates */
#endif

    /* Running properties */
    u16                 i_changes;               /* changes made to the thread */    
    mtime_t             last_picture_date;        /* last picture display date */
    mtime_t             last_display_date;         /* last screen display date */    

    /* Videos heap and translation tables */
    picture_t           p_picture[VOUT_MAX_PICTURES];              /* pictures */
    subtitle_t          p_subtitle[VOUT_MAX_PICTURES];            /* subtitles */    
    vout_tables_t       tables;                          /* translation tables */
    vout_convert_t *    p_ConvertYUV420;                /* YUV 4:2:0 converter */
    vout_convert_t *    p_ConvertYUV422;                /* YUV 4:2:2 converter */
    vout_convert_t *    p_ConvertYUV444;                /* YUV 4:4:4 converter */

    /* Bitmap fonts */
    p_vout_font_t       p_default_font;                        /* default font */    
    p_vout_font_t       p_large_font;                            /* large font */    
} vout_thread_t;

/* Flags for changes - these flags are set in the i_changes field when another
 * thread changed a variable */
#define VOUT_INFO_CHANGE        0x0001                       /* b_info changed */
#define VOUT_GRAYSCALE_CHANGE   0x0002                  /* b_grayscale changed */
#define VOUT_SIZE_CHANGE        0x0008                         /* size changed */
#define VOUT_DEPTH_CHANGE       0x0010                        /* depth changed */
#define VOUT_GAMMA_CHANGE       0x0080                        /* gamma changed */
#define VOUT_NODISPLAY_CHANGE   0xffff      /* changes which forbidden display */

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
subtitle_t *    vout_CreateSubtitle     ( vout_thread_t *p_vout, int i_type, int i_size );
void            vout_DestroySubtitle    ( vout_thread_t *p_vout, subtitle_t *p_sub );
void            vout_DisplaySubtitle    ( vout_thread_t *p_vout, subtitle_t *p_sub );









