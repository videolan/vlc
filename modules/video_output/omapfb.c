/*****************************************************************************
* omapfb.c : omap framebuffer plugin for vlc
*****************************************************************************
* Copyright (C) 2008-2009 the VideoLAN team
* $Id$
*
* Authors: Antoine Lejeune <phytos @ videolan.org>
*          Based on fb.c and work of Siarhei Siamashka on mplayer for Maemo
*          Needs a recent omapfb.h to compile
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
*****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>                                                 /* ENOMEM */
#include <fcntl.h>                                                 /* open() */
#include <unistd.h>                                               /* close() */

#include <sys/ioctl.h>
#include <sys/mman.h>                                              /* mmap() */

#include <linux/fb.h>
#include <asm/arch-omap/omapfb.h>

/* Embedded window handling */
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout.h>
#include <vlc_vout_window.h>
#include <vlc_playlist.h>

/*****************************************************************************
* Local prototypes
*****************************************************************************/
static int  Create           ( vlc_object_t * );
static void Destroy          ( vlc_object_t * );

static int  Init             ( vout_thread_t * );
static void End              ( vout_thread_t * );
static int  Manage           ( vout_thread_t * );
static void DisplayVideo     ( vout_thread_t *, picture_t * );
static int  Control          ( vout_thread_t *, int, va_list );

static void FreePicture      ( vout_thread_t *, picture_t * );

static int  OpenDisplay      ( vout_thread_t * );
static void CloseDisplay     ( vout_thread_t * );
static void UpdateScreen     ( vout_thread_t *,
                               int, int, int, int, int, int, int );

static int  InitWindow       ( vout_thread_t * );
static void CreateWindow     ( vout_sys_t * );
static void ToggleFullScreen ( vout_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FB_DEV_VAR "omapfbdev"

#define DEVICE_TEXT N_("OMAP Framebuffer device")
#define DEVICE_LONGTEXT N_( \
    "OMAP Framebuffer device to use for rendering (usually /dev/fb0).")

#define CHROMA_TEXT N_("Chroma used.")
#define CHROMA_LONGTEXT N_( \
    "Force use of a specific chroma for output. Default is Y420 (specific to N770/N8xx hardware)." )

#define OVERLAY_TEXT N_("Embed the overlay")
#define OVERLAY_LONGTEXT N_( \
    "Embed the framebuffer overlay into a X11 window" )

vlc_module_begin();
    set_shortname( "OMAP framebuffer" );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VOUT );
    add_file( FB_DEV_VAR, "/dev/fb0", NULL, DEVICE_TEXT, DEVICE_LONGTEXT,
              false )
    add_string( "omap-chroma", NULL, NULL, CHROMA_TEXT, CHROMA_LONGTEXT,
                true )
    add_bool( "omap-embedded", true, NULL, OVERLAY_TEXT, OVERLAY_LONGTEXT,
                 true )
    set_description( N_("OMAP framebuffer video output") );
    set_capability( "video output", 200 );
    set_callbacks( Create, Destroy );
vlc_module_end();

/*****************************************************************************
 * omap_window_t: simple structure with the geometry of a window
 *****************************************************************************/
struct omap_window_t
{
    uint32_t i_x;
    uint32_t i_y;
    uint32_t i_width;
    uint32_t i_height;
};

/*****************************************************************************
* vout_sys_t: video output omap framebuffer method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the FB specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    /* Framebuffer information */
    int                         i_fd;                       /* device handle */
    struct fb_var_screeninfo    fb_vinfo;        /* current mode information */
    struct fb_fix_screeninfo    fb_finfo;
    struct omapfb_caps          caps;
    bool                        b_tearsync;

    /* Window information */
    struct omap_window_t output_window;    /* Size of the real output window */
    struct omap_window_t main_window;  /* Size of the area we got to display */
    struct omap_window_t embedded_window;     /* Size of the embedded window */

    /* Video information */
    uint32_t             i_video_width;                       /* video width */
    uint32_t             i_video_height;                     /* video height */
    vlc_fourcc_t         i_chroma;
    int                  i_color_format;                   /* OMAPFB_COLOR_* */
    bool                 b_video_enabled;        /* Video must be displayed? */
    picture_t           *p_output_picture;

    /* Video memory */
    uint8_t    *p_video;                                      /* base adress */
    uint8_t    *p_center;                                   /* output adress */
    size_t      i_page_size;                                    /* page size */
    int         i_bytes_per_pixel;                /* Bytes used by one pixel */
    int         i_line_len;                   /* Length of one line in bytes */

    /* X11 */
    Display   *p_display;
    vout_window_t *owner_window;
    Window     window;
    mtime_t    i_time_button_last_pressed;         /* To detect double click */

    /* Dummy memory */
    int        i_null_fd;
    uint8_t   *p_null;
};

/*****************************************************************************
 * Create: allocates omapfb video thread output method
 *****************************************************************************
 * This function allocates and initializes a FB vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_sys_t    *p_sys;

    if( p_vout->fmt_in.i_chroma != VLC_CODEC_I420 &&
        p_vout->fmt_in.i_chroma != VLC_CODEC_YV12 )
        return VLC_EGENERIC;

    /* Allocate instance and initialize some members */
    p_vout->p_sys = p_sys = calloc( 1, sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
        return VLC_ENOMEM;

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_render = NULL;
    p_vout->pf_display = DisplayVideo;
    p_vout->pf_control = Control;
    p_sys->b_video_enabled = true;

    if( OpenDisplay( p_vout ) )
    {
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

    if( InitWindow( p_vout ) )
    {
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy omapfb video thread output method
 *****************************************************************************
 * Terminate an output method created by Create
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    CloseDisplay( p_vout );

    if( p_vout->p_sys->owner_window )
    {
        vout_window_Delete( p_vout->p_sys->owner_window );
        XCloseDisplay( p_vout->p_sys->p_display );
    }

    /* Destroy structure */
    free( p_vout->p_sys );
}


/*****************************************************************************
 * Init: initialize omap framebuffer video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = (vout_sys_t *)p_vout->p_sys;

    // We want to keep the same aspect
    p_vout->fmt_out.i_aspect = p_vout->output.i_aspect = p_vout->render.i_aspect;
    // We ask where the video should be displayed in the video area
    vout_PlacePicture( p_vout, p_sys->main_window.i_width,
                       p_sys->main_window.i_height,
                       &p_sys->output_window.i_x,
                       &p_sys->output_window.i_y,
                       &p_sys->output_window.i_width,
                       &p_sys->output_window.i_height );
    p_sys->output_window.i_x = ( p_sys->output_window.i_x +
                                 p_sys->main_window.i_x ) & ~1;
    p_sys->output_window.i_y = ( p_sys->output_window.i_y +
                                 p_sys->main_window.i_y ) & ~1;

    // Hardware upscaling better than software
    if( p_vout->fmt_render.i_width <= p_sys->main_window.i_width &&
        p_vout->fmt_render.i_height <= p_sys->main_window.i_height )
    {
        p_sys->i_video_width =
        p_vout->output.i_width =
        p_vout->fmt_out.i_width =
        p_vout->fmt_out.i_visible_width = p_vout->fmt_render.i_width;
        p_sys->i_video_height =
        p_vout->output.i_height =
        p_vout->fmt_out.i_height =
        p_vout->fmt_out.i_visible_height = p_vout->fmt_render.i_height;
    }
    else
    {
        p_sys->i_video_width =
        p_vout->output.i_width =
        p_vout->fmt_out.i_width =
        p_vout->fmt_out.i_visible_width = p_sys->output_window.i_width;
        p_sys->i_video_height =
        p_vout->output.i_height =
        p_vout->fmt_out.i_height =
        p_vout->fmt_out.i_visible_height = p_sys->output_window.i_height;
    }

    p_vout->output.i_chroma =
    p_vout->fmt_out.i_chroma = VLC_CODEC_I420;
    p_sys->i_color_format = OMAPFB_COLOR_YUV420;

    // place in the framebuffer where we have to write
    p_sys->p_center = p_sys->p_video + p_sys->output_window.i_x*p_sys->i_bytes_per_pixel
                      + p_sys->output_window.i_y*p_sys->i_line_len;

    // We get and set a direct render vlc picture
    I_OUTPUTPICTURES = 0;
    picture_t *p_pic = NULL;
    int i_index;

    for( i_index = 0 ; i_index < VOUT_MAX_PICTURES ; i_index++ )
    {
        if( p_vout->p_picture[ i_index ].i_status == FREE_PICTURE )
        {
            p_pic = p_vout->p_picture + i_index;
            break;
        }
    }

    /* Allocate the picture */
    if( p_pic == NULL )
    {
        return VLC_EGENERIC;
    }

    p_sys->p_output_picture = p_pic;
    p_pic->p->p_pixels = p_vout->p_sys->p_center;
    p_pic->p->i_pixel_pitch = p_vout->p_sys->i_bytes_per_pixel;
    p_pic->p->i_lines = p_sys->i_video_height;
    p_pic->p->i_visible_lines = p_sys->i_video_height;
    p_pic->p->i_pitch = p_sys->i_line_len;
    p_pic->p->i_visible_pitch = p_sys->i_line_len;
    p_pic->i_planes = 1;
    p_pic->i_status = DESTROYED_PICTURE;
    p_pic->i_type   = DIRECT_PICTURE;

    PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

    I_OUTPUTPICTURES++;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate omap framebuffer video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    /* Clear the screen */
    UpdateScreen( p_vout, 0, 0,
                  p_vout->p_sys->fb_vinfo.xres,
                  p_vout->p_sys->fb_vinfo.yres,
                  p_vout->p_sys->fb_vinfo.xres,
                  p_vout->p_sys->fb_vinfo.yres,
                  OMAPFB_COLOR_RGB565 );
}

/*****************************************************************************
 * Control: control facility for the vout
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    return VLC_EGENERIC;
}

/*****************************************************************************
* FreePicture: Destroy the picture and make it free again
******************************************************************************/
static void FreePicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    p_pic->p->p_pixels = NULL;
    p_pic->i_status = FREE_PICTURE;
}

/*****************************************************************************
 * Manage: handle omapfb events
 *****************************************************************************
 * This function should be called regularly by video output thread.
 *****************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
    XEvent xevent;

    while( XPending( p_vout->p_sys->p_display ) )
    {
        XNextEvent( p_vout->p_sys->p_display, &xevent );

        if( xevent.type == ButtonPress &&
            ((XButtonEvent *)&xevent)->button == Button1 )
        {
            /* detect double-clicks */
            if( ( ((XButtonEvent *)&xevent)->time -
                    p_vout->p_sys->i_time_button_last_pressed ) < 300 )
            {
                p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
            }

            p_vout->p_sys->i_time_button_last_pressed =
                        ((XButtonEvent *)&xevent)->time;
        }
        else if( ( xevent.type == VisibilityNotify &&
                 xevent.xvisibility.state != VisibilityUnobscured ) ||
                 xevent.type == UnmapNotify )
        {
            UpdateScreen( p_vout, 0, 0,
                          p_vout->p_sys->fb_vinfo.xres,
                          p_vout->p_sys->fb_vinfo.yres,
                          p_vout->p_sys->fb_vinfo.xres,
                          p_vout->p_sys->fb_vinfo.yres,
                          OMAPFB_COLOR_RGB565 );
            p_vout->p_sys->b_video_enabled = false;
            p_vout->p_sys->p_output_picture->p->p_pixels =
                p_vout->p_sys->p_null;
         }
    }

    if( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        /* Update the object variable and trigger callback */
        p_vout->b_fullscreen = !p_vout->b_fullscreen;
        var_SetBool( p_vout, "fullscreen", p_vout->b_fullscreen );

        if( p_vout->p_sys->owner_window )
            vout_window_SetFullscreen( p_vout->p_sys->owner_window,
                                       p_vout->b_fullscreen );
        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
    }

    if( p_vout->i_changes & VOUT_SIZE_CHANGE )
    {
        FreePicture( p_vout, p_vout->p_sys->p_output_picture );
        if( Init( p_vout ) )
        {
            msg_Err( p_vout, "cannot reinit framebuffer screen" );
            return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * DisplayVideo: displays previously rendered output
 *****************************************************************************
 * This function update the screen resulting in the display of the last
 * rendered picture.
 *****************************************************************************/
static void DisplayVideo( vout_thread_t *p_vout, picture_t *p_pic )
{
    VLC_UNUSED( p_pic );

    if( !p_vout->p_sys->b_video_enabled )
        return;

    UpdateScreen( p_vout,
                  p_vout->p_sys->output_window.i_x,
                  p_vout->p_sys->output_window.i_y,
                  p_vout->p_sys->i_video_width,
                  p_vout->p_sys->i_video_height,
                  p_vout->p_sys->output_window.i_width,
                  p_vout->p_sys->output_window.i_height,
                  p_vout->p_sys->i_color_format );

    // Wait for the window to be fully displayed
    ioctl( p_vout->p_sys->i_fd, OMAPFB_SYNC_GFX);
}

/*****************************************************************************
 * OpenDisplay: initialize framebuffer
 *****************************************************************************/
static int OpenDisplay( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = (vout_sys_t *) p_vout->p_sys;
    char *psz_device;                             /* framebuffer device path */

    /* Open framebuffer device */
    if( !(psz_device = var_CreateGetNonEmptyString( p_vout, FB_DEV_VAR )) )
    {
        msg_Err( p_vout, "don't know which fb device to open" );
        return VLC_EGENERIC;
    }

    p_sys->i_fd = open( psz_device, O_RDWR );
    if( p_sys->i_fd == -1 )
    {
        msg_Err( p_vout, "cannot open %s (%m)", psz_device );
        free( psz_device );
        return VLC_EGENERIC;
    }
    free( psz_device );

    // Get caps, try older interface if needed
    if( ioctl( p_sys->i_fd, OMAPFB_GET_CAPS, &p_sys->caps ) != 0 )
    {
        if( ioctl( p_sys->i_fd, OMAP_IOR( 42, unsigned long ), &p_sys->caps ) != 0 )
        {
            msg_Err( p_vout, "OMAPFB_GET_CAPS ioctl failed" );
            close( p_sys->i_fd );
            return VLC_EGENERIC;
        }
    }

    if( ( p_sys->caps.ctrl & OMAPFB_CAPS_TEARSYNC ) != 0 )
        p_sys->b_tearsync = true;

    if( ioctl( p_sys->i_fd, FBIOGET_VSCREENINFO, &p_sys->fb_vinfo ) )
    {
        msg_Err( p_vout, "Can't get VSCREENINFO: %m" );
        close( p_sys->i_fd );
        return VLC_EGENERIC;
    }
    if( ioctl( p_sys->i_fd, FBIOGET_FSCREENINFO, &p_sys->fb_finfo ) )
    {
        msg_Err( p_vout, "Can't get FSCREENINFO: %m" );
        close( p_sys->i_fd );
        return VLC_EGENERIC;
    }

    p_sys->i_bytes_per_pixel = 2;
    p_sys->i_page_size = p_sys->fb_finfo.smem_len;
    p_sys->i_line_len = p_sys->fb_finfo.line_length;

    if( (p_sys->p_video = (uint8_t *)mmap( 0, p_sys->i_page_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                                            p_sys->i_fd, 0 )) == MAP_FAILED )
    {
        msg_Err( p_vout, "Can't mmap: %m" );
        close( p_sys->i_fd );
        return VLC_EGENERIC;
    }

    p_sys->p_display = XOpenDisplay( NULL );

    /* Open /dev/null and map it */
    p_sys->i_null_fd = open( "/dev/zero", O_RDWR );
    if( p_sys->i_null_fd == -1 )
    {
        msg_Err( p_vout, "cannot open /dev/zero (%m)" );
        munmap( p_sys->p_video, p_sys->i_page_size );
        close( p_sys->i_fd );
        return VLC_EGENERIC;
    }

    if( (p_sys->p_null = (uint8_t *)mmap( 0, p_sys->i_page_size, PROT_READ | PROT_WRITE,
                                          MAP_PRIVATE, p_sys->i_null_fd, 0 )) == MAP_FAILED )
    {
        msg_Err( p_vout, "Can't mmap 2: %m" );
        munmap( p_sys->p_video, p_sys->i_page_size );
        close( p_sys->i_null_fd );
        close( p_sys->i_fd );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseDisplay: terminate FB video thread output method
 *****************************************************************************/
static void CloseDisplay( vout_thread_t *p_vout )
{
    munmap( p_vout->p_sys->p_video, p_vout->p_sys->i_page_size );
    munmap( p_vout->p_sys->p_null,  p_vout->p_sys->i_page_size );

    close( p_vout->p_sys->i_fd );
    close( p_vout->p_sys->i_null_fd );
}

/*****************************************************************************
 * UpdateScreen: update the screen of the omapfb
 *****************************************************************************/
static void UpdateScreen( vout_thread_t *p_vout, int i_x, int i_y,
                          int i_width, int i_height,
                          int i_out_width, int i_out_height, int i_format )
{
    struct omapfb_update_window update;
    update.x = i_x;
    update.y = i_y;
    update.width = i_width;
    update.height = i_height;
    update.out_x = i_x;
    update.out_y = i_y;
    update.out_width = i_out_width;
    update.out_height = i_out_height;
    update.format = i_format;
    if( p_vout->p_sys->b_tearsync )
        update.format |= OMAPFB_FORMAT_FLAG_TEARSYNC;
    ioctl( p_vout->p_sys->i_fd, OMAPFB_UPDATE_WINDOW, &update );
}

/*****************************************************************************
 * InitWindow: get embedded window and init X11
 *****************************************************************************/
static int InitWindow( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = (vout_sys_t *)p_vout->p_sys;

    if( var_CreateGetBool( p_vout, "omap-embedded" ) )
    {
        p_sys->p_display = XOpenDisplay( NULL );

        // Request window from interface
        vout_window_cfg_t wnd_cfg;

        memset( &wnd_cfg, 0, sizeof(wnd_cfg) );
        wnd_cfg.type   = VOUT_WINDOW_TYPE_XID;
        wnd_cfg.x      = p_sys->embedded_window.i_x;
        wnd_cfg.y      = p_sys->embedded_window.i_y;
        wnd_cfg.width  = p_sys->embedded_window.i_width;
        wnd_cfg.height = p_sys->embedded_window.i_height;

        p_sys->owner_window = vout_window_New( VLC_OBJECT(p_vout), NULL, &wnd_cfg );
        p_sys->main_window = p_sys->embedded_window;

        // We have to create a new window to get some events
        CreateWindow( p_sys );
    }
    else
    {
        // No embedding, fullscreen framebuffer overlay with no events handling
        p_sys->main_window.i_x = p_sys->main_window.i_y = 0;
        p_sys->main_window.i_width = p_sys->fb_vinfo.xres;
        p_sys->main_window.i_height = p_sys->fb_vinfo.yres;
        p_vout->b_fullscreen = true;
        var_SetBool( p_vout, "fullscreen", p_vout->b_fullscreen );
    }

    return VLC_SUCCESS;
}

static void CreateWindow( vout_sys_t *p_sys )
{
    XSetWindowAttributes    xwindow_attributes;
    xwindow_attributes.backing_store = Always;
    xwindow_attributes.background_pixel =
        BlackPixel( p_sys->p_display, DefaultScreen(p_sys->p_display) );
    xwindow_attributes.event_mask = ExposureMask | StructureNotifyMask
                                  | VisibilityChangeMask;
    p_sys->window = XCreateWindow( p_sys->p_display,
                                   p_sys->owner_window->handle.xid,
                                   0, 0,
                                   p_sys->main_window.i_width,
                                   p_sys->main_window.i_height,
                                   0,
                                   0, InputOutput, 0,
                                   CWBackingStore | CWBackPixel | CWEventMask,
                                   &xwindow_attributes );

    XMapWindow( p_sys->p_display, p_sys->window );
    XSelectInput( p_sys->p_display, p_sys->owner_window->handle.xid,
                  StructureNotifyMask );
}

