/*******************************************************************************
 * video_output.h : video output thread
 * (c)1999 VideoLAN
 *******************************************************************************
 * This module describes the programming interface for video output threads.
 * It includes functions allowing to open a new thread, send pictures to a
 * thread, and destroy a previously oppenned video output thread.
 *******************************************************************************
 * Requires:
 *  <pthread.h>
 *  "config.h"
 *  "common.h"
 *  "mtime.h"
 *  "video.h"
 *******************************************************************************/

/* ?? this over-complicated API and code should be re-designed, with a simple
 * video-stream associated to a window (each window designed to be openned in
 * a parent one and probably without border), and have an api looking like
 * vout_CreateWindow
 * vout_DestroyWindow
 * vout_AddPicture
 * vout_RemovePicture
 * vout_ReservePicture
 * vout_AddReservedPicture
 * vout_Clear
 * vout_Refresh
 * 
 * the overlay/transparent, permanent and such stuff should disapear.
 */

/*******************************************************************************
 * vout_stream_t: video stream descriptor
 *******************************************************************************
 * Each video stream has a set of properties, stored in this structure. It is
 * part of vout_thread_t and is not supposed to be used anywhere else.
 *******************************************************************************/
typedef struct
{
    int                 i_status;                        /* is stream active ? */
    picture_t *         p_next_picture;        /* next picture to be displayed */

#ifdef STATS
    /* Statistics */
    count_t             c_pictures;          /* total number of pictures added */
    count_t             c_rendered_pictures;    /* number of rendered pictures */
#endif
} vout_stream_t;

/* Video stream status */
#define VOUT_INACTIVE_STREAM    0                /* stream is inactive (empty) */
#define VOUT_ACTIVE_STREAM      1                          /* stream is active */
#define VOUT_ENDING_STREAM      2       /* stream will be destroyed when empty */
#define VOUT_DESTROYED_STREAM   3                  /* stream must be destroyed */

/*******************************************************************************
 * vout_thread_t: video output thread descriptor
 *******************************************************************************
 * Any independant video output device, such as an X11 window, is represented
 * by a video output thread, and described using following structure.
 *******************************************************************************/
typedef struct vout_thread_s
{
    /* Thread properties and locks */
    boolean_t           b_die;                                   /* `die' flag */
    boolean_t           b_error;                               /* `error' flag */
    boolean_t           b_active;                             /* `active' flag */
    pthread_t           thread_id;                 /* id for pthread functions */
    pthread_mutex_t     streams_lock;             /* streams modification lock */
    pthread_mutex_t     pictures_lock;           /* pictures modification lock */
    int *               pi_status;                    /* temporary status flag */

    /* Common display properties */
    int                 i_width;                /* current output method width */
    int                 i_height;              /* current output method height */
    int                 i_screen_depth;                      /* bits per pixel */
    int                 i_bytes_per_pixel;                /* real screen depth */

    /* Output method */
    struct vout_x11_s * p_x11;                            /* X11 output method */

    /* Video heap */
    int                 i_max_pictures;                   /* heap maximal size */
    int                 i_pictures;                       /* current heap size */
    picture_t *         p_picture;                                 /* pictures */

    /* Streams data */
    vout_stream_t       p_stream[VOUT_MAX_STREAMS];            /* streams data */

#ifdef STATS    
    /* Statistics */
    count_t         c_loops;                               /* number of loops */
    count_t         c_idle_loops;                     /* number of idle loops */
    count_t         c_pictures;           /* number of pictures added to heap */
    count_t         c_rendered_pictures;       /* number of pictures rendered */
#endif

    /* Rendering functions - these functions are of vout_render_blank_t and 
     * vout_render_line_t, but are not declared here using these types since
     * they require vout_thread_t to be defined */
    void (* RenderRGBBlank)         ( struct vout_thread_s *p_vout, pixel_t pixel,
                                      int i_x, int i_y, int i_width, int i_height );
    void (* RenderPixelBlank)       ( struct vout_thread_s *p_vout, pixel_t pixel,
                                      int i_x, int i_y, int i_width, int i_height );
    void (* RenderRGBLine)          ( struct vout_thread_s *p_vout, picture_t *p_pic,
                                      int i_x, int i_y, int i_pic_x, int i_pic_y, 
                                      int i_width, int i_line_width, int i_ratio ); 
    void (* RenderPixelLine)        ( struct vout_thread_s *p_vout, picture_t *p_pic,
                                      int i_x, int i_y, int i_pic_x, int i_pic_y, 
                                      int i_width, int i_line_width, int i_ratio ); 
    void (* RenderRGBMaskLine)      ( struct vout_thread_s *p_vout, picture_t *p_pic,
                                      int i_x, int i_y, int i_pic_x, int i_pic_y, 
                                      int i_width, int i_line_width, int i_ratio ); 
    void (* RenderPixelMaskLine)    ( struct vout_thread_s *p_vout, picture_t *p_pic,
                                      int i_x, int i_y, int i_pic_x, int i_pic_y, 
                                      int i_width, int i_line_width, int i_ratio ); 
    /* ?? add YUV types */
} vout_thread_t;

/*******************************************************************************
 * vout_render_blank_t: blank rendering function
 * vout_render_line_t: rectangle rendering functions
 *******************************************************************************
 * All rendering functions should be of these types - for blank pictures
 * (pictures with uniform color), blank rendering functions are called once. For
 * other pictures, each function is called once for each picture line. Note that
 * the part of the picture sent to the rendering functions is in the output
 * window, since the clipping is done before. 
 *  p_vout is the calling thread
 *  pixel is the color or pixel value of the rectange to be drawn
 *  p_pic is the picture to be rendered
 *  i_x, i_y is the absolute position in output window
 *  i_pic_x is the first pixel to be drawn in the picture
 *  i_pic_y is the line of the picture to be drawn
 *  i_width is the width of the area to be rendered in the picture (not in the
 *      output window), except for blank pictures, where it is the absolute size
 *      of the area to be rendered
 *  i_height is the height og the area to be rendered
 *  i_line_width is the number of time the line must be copied
 *  i_ratio is the horizontal display ratio
 *******************************************************************************/
typedef void (vout_render_blank_t)( vout_thread_t *p_vout, pixel_t pixel,
                                      int i_x, int i_y, int i_width, int i_height );
typedef void (vout_render_line_t) ( vout_thread_t *p_vout, picture_t *p_pic,
                                      int i_x, int i_y, int i_pic_x, int i_pic_y, 
                                      int i_width, int i_line_width, int i_ratio );

/*******************************************************************************
 * Prototypes
 *******************************************************************************/
vout_thread_t * vout_CreateThread               ( video_cfg_t *p_cfg, int *pi_status );
void            vout_DestroyThread              ( vout_thread_t *p_vout, int *pi_status );

picture_t *     vout_DisplayPicture             ( vout_thread_t *p_vout, picture_t *p_pic );
picture_t *     vout_DisplayPictureCopy         ( vout_thread_t *p_vout, picture_t *p_pic );
picture_t *     vout_DisplayPictureReplicate    ( vout_thread_t *p_vout, picture_t *p_pic );
picture_t *     vout_DisplayReservedPicture     ( vout_thread_t *p_vout, picture_t *p_pic );

picture_t *     vout_CreateReservedPicture      ( vout_thread_t *p_vout, video_cfg_t *p_cfg );
picture_t *     vout_ReservePicture             ( vout_thread_t *p_vout, picture_t *p_pic );
picture_t *     vout_ReservePictureCopy         ( vout_thread_t *p_vout, picture_t *p_pic );
picture_t *     vout_ReservePictureReplicate    ( vout_thread_t *p_vout, picture_t *p_pic );
void            vout_RemovePicture              ( vout_thread_t *p_vout, picture_t *p_pic );

void            vout_RefreshPermanentPicture    ( vout_thread_t *p_vout, picture_t *p_pic, 
                                                  mtime_t displa_date );

void            vout_LinkPicture                ( vout_thread_t *p_vout, picture_t *p_pic );
void            vout_UnlinkPicture              ( vout_thread_t *p_vout, picture_t *p_pic );

int             vout_CreateStream               ( vout_thread_t *p_vout );
void            vout_EndStream                  ( vout_thread_t *p_vout, int i_stream );
void            vout_DestroyStream              ( vout_thread_t *p_vout, int i_stream );

#ifdef DEBUG
void            vout_PrintHeap                  ( vout_thread_t *p_vout, char *psz_str );
#endif







