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
    int                 i_width;                /* current output method width */
    int                 i_height;              /* current output method height */
    int                 i_screen_depth;                      /* bits per pixel */
    int                 i_bytes_per_pixel;                /* real screen depth */
    float               f_x_ratio;                 /* horizontal display ratio */
    float               f_y_ratio;                   /* vertical display ratio */

#ifdef STATS    
    /* Statistics */
    count_t         c_loops;                               /* number of loops */
    count_t         c_idle_loops;                     /* number of idle loops */
    count_t         c_pictures;           /* number of pictures added to heap */
#endif

    /* Output method */
    p_vout_sys_t        p_sys;                         /* system output method */

    /* Video heap */
    int                 i_pictures;                       /* current heap size */
    picture_t           p_picture[VOUT_MAX_PICTURES];              /* pictures */

    /* YUV translation tables, for 15,16 and 24/32 bpp displays. 16 bits and 32
     * bits pointers points on the same data.
     * CAUTION: these tables are translated: their origin is -384 */
    u16 *           pi_trans16_red;
    u16 *           pi_trans16_green;
    u16 *           pi_trans16_blue;
    u32 *           pi_trans32_red;
    u32 *           pi_trans32_green;
    u32 *           pi_trans32_blue;          
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

















