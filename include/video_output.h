/*******************************************************************************
 * video_output.h : video output thread
 * (c)1999 VideoLAN
 *******************************************************************************
 * This module describes the programming interface for video output threads.
 * It includes functions allowing to open a new thread, send pictures to a
 * thread, and destroy a previously oppenned video output thread.
 *******************************************************************************/

/*******************************************************************************
 * vout_thread_t: video output thread descriptor
 *******************************************************************************
 * Any independant video output device, such as an X11 window or a GGI device,
 * is represented by a video output thread, and described using following 
 * structure.
 *******************************************************************************/
typedef struct vout_thread_s
{
    /* Thread properties and locks */
    boolean_t           b_die;                                   /* `die' flag */
    boolean_t           b_error;                               /* `error' flag */
    boolean_t           b_active;                             /* `active' flag */
    pthread_t           thread_id;                 /* id for pthread functions */
    pthread_mutex_t     lock;                                   /* thread lock */
    int *               pi_status;                    /* temporary status flag */

    /* Common display properties */
    boolean_t           b_info;              /* print additionnal informations */    
    boolean_t           b_grayscale;             /* color or grayscale display */    
    int                 i_width;                /* current output method width */
    int                 i_height;              /* current output method height */
    int                 i_bytes_per_line;/* bytes per line (including virtual) */    
    int                 i_screen_depth;                      /* bits per pixel */
    int                 i_bytes_per_pixel;                /* real screen depth */
    float               f_x_ratio;                 /* horizontal display ratio */
    float               f_y_ratio;                   /* vertical display ratio */
    float               f_gamma;                                      /* gamma */    

    /* Changed properties values - some of them are treated directly by the
     * thread, the over may be ignored or handled by vout_SysManage */
    boolean_t           b_gamma_change;              /* gamma change indicator */    
    int                 i_new_width;                              /* new width */    
    int                 i_new_height;                            /* new height */    

#ifdef STATS    
    /* Statistics - these numbers are not supposed to be accurate */
    count_t             c_loops;                            /* number of loops */
    count_t             c_idle_loops;                  /* number of idle loops */
    count_t             c_fps_samples;                       /* picture counts */    
    mtime_t             fps_sample[ VOUT_FPS_SAMPLES ];   /* FPS samples dates */
#endif

#ifdef DEBUG_VIDEO
    /* Video debugging informations */
    mtime_t             picture_render_time;    /* last picture rendering time */
#endif

    /* Output method */
    p_vout_sys_t        p_sys;                         /* system output method */

    /* Video heap */
    picture_t           p_picture[VOUT_MAX_PICTURES];              /* pictures */

    /* YUV translation tables - they have to be casted to the appropriate width 
     * on use. All tables are allocated in the same memory block, based at
     * p_trans_base, and shifted depending of the output thread configuration */
    byte_t *            p_trans_base;       /* base for all translation tables */    
    void *              p_trans_red;
    void *              p_trans_green;
    void *              p_trans_blue;
    void *              p_trans_gray;    

    /* YUV translation tables, for optimized C YUV transform ?? */
} vout_thread_t;

/*******************************************************************************
 * Prototypes
 *******************************************************************************/
vout_thread_t * vout_CreateThread               ( 
#ifdef VIDEO_X11
                                                  char *psz_display, Window root_window, 
#endif
                                                  int i_width, int i_height, int *pi_status
                                                );

void            vout_DestroyThread              ( vout_thread_t *p_vout, int *pi_status );

picture_t *     vout_CreatePicture              ( vout_thread_t *p_vout, int i_type, 
						  int i_width, int i_height, int i_bytes_per_line );
void            vout_DestroyPicture             ( vout_thread_t *p_vout, picture_t *p_pic );
void            vout_DisplayPicture             ( vout_thread_t *p_vout, picture_t *p_pic );
void            vout_LinkPicture                ( vout_thread_t *p_vout, picture_t *p_pic );
void            vout_UnlinkPicture              ( vout_thread_t *p_vout, picture_t *p_pic );

















