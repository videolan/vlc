/*****************************************************************************
 * video_x11.h: X11 video output display method
 * (c)1999 VideoLAN
 *****************************************************************************
 * The X11 method for video output thread. The functions declared here should
 * not be needed by any other module than video_output.c.
 *****************************************************************************
 * Required headers:
 * <X11/Xlib.h>
 * <X11/Xutil.h>
 * <X11/extensions/XShm.h>
 * "config.h"
 * "common.h"
 * "mtime.h"
 * "video.h"
 * "video_output.h"
 *****************************************************************************/

/*****************************************************************************
 * vout_x11_t: video output X11 method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the X11 specific properties of an output thread. X11 video
 * output is performed through regular resizable windows. Windows can be
 * dynamically resized to adapt to the size of the streams.
 *****************************************************************************/
typedef struct vout_x11_s
{
    /* User settings */
    boolean_t           b_shm_ext;           /* shared memory extension flag */

    /* Thread configuration - these properties are copied from a video_cfg_t
     * structure to be used in second step of initialization */
    char *              psz_display;                         /* display name */
    char *              psz_title;                           /* window title */

    /* Internal settings and properties */
    Display *           p_display;                        /* display pointer */
    int                 i_screen;                           /* screen number */
    Window              window;                   /* window instance handler */
    GC                  gc;              /* graphic context instance handler */

    /* Window manager hints and atoms */
    Atom                wm_protocols;                   /* WM_PROTOCOLS atom */
    Atom                wm_delete_window;           /* WM_DELETE_WINDOW atom */

    /* Color maps and translation tables - some of those tables are shifted,
     * see x11.c for more informations. */
    u8 *                trans_16bpp_red;         /* red (16 bpp) (SHIFTED !) */
    u8 *                trans_16bpp_green;     /* green (16 bpp) (SHIFTED !) */
    u8 *                trans_16bpp_blue;       /* blue (16 bpp) (SHIFTED !) */

    /* ?? colormaps ? */
    boolean_t           b_private_colormap;        /* private color map flag */
    Colormap            private_colormap;               /* private color map */

    /* Display buffers and shared memory information */
    int                 i_buffer_index;                      /* buffer index */
    XImage *            p_ximage[2];                       /* XImage pointer */
    XShmSegmentInfo     shm_info[2];       /* shared memory zone information */

    int                 i_completion_type;                             /* ?? */
} vout_x11_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int     vout_X11AllocOutputMethod   ( vout_thread_t *p_vout, video_cfg_t *p_cfg );
void    vout_X11FreeOutputMethod    ( vout_thread_t *p_vout );
int     vout_X11CreateOutputMethod  ( vout_thread_t *p_vout );
void    vout_X11DestroyOutputMethod ( vout_thread_t *p_vout );
int     vout_X11ManageOutputMethod  ( vout_thread_t *p_vout );
void    vout_X11DisplayOutput       ( vout_thread_t *p_vout );


