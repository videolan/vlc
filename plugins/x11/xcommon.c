/*****************************************************************************
 * xcommon.c: Functions common to the X11 and XVideo plugins
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: xcommon.c,v 1.19 2002/02/24 20:51:10 gbazin Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          David Kennedy <dkennedy@tinytoad.com>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include <videolan/vlc.h>

#ifdef HAVE_MACHINE_PARAM_H
    /* BSD */
#   include <machine/param.h>
#   include <sys/types.h>                                  /* typedef ushort */
#   include <sys/ipc.h>
#endif

#ifndef WIN32
#   include <netinet/in.h>                            /* BSD: struct in_addr */
#endif

#ifdef HAVE_SYS_SHM_H
#   include <sys/shm.h>                                /* shmget(), shmctl() */
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#ifdef HAVE_SYS_SHM_H
#   include <X11/extensions/XShm.h>
#endif
#ifdef DPMSINFO_IN_DPMS_H
#   include <X11/extensions/dpms.h>
#endif

#ifdef MODULE_NAME_IS_xvideo
#   include <X11/extensions/Xv.h>
#   include <X11/extensions/Xvlib.h>
#endif

#include "video.h"
#include "video_output.h"
#include "xcommon.h"

#include "interface.h"
#include "netutils.h"                                 /* network_ChannelJoin */

#include "stream_control.h"                 /* needed by input_ext-intf.h... */
#include "input_ext-intf.h"

#ifdef MODULE_NAME_IS_xvideo
#   define IMAGE_TYPE     XvImage
#   define EXTRA_ARGS     int i_xvport, int i_chroma
#   define EXTRA_ARGS_SHM int i_xvport, int i_chroma, XShmSegmentInfo *p_shm
#   define DATA_SIZE(p)   (p)->data_size
#   define IMAGE_FREE     XFree      /* There is nothing like XvDestroyImage */
#else
#   define IMAGE_TYPE     XImage
#   define EXTRA_ARGS     Visual *p_visual, int i_depth, int i_bytes_per_pixel
#   define EXTRA_ARGS_SHM Visual *p_visual, int i_depth, XShmSegmentInfo *p_shm
#   define DATA_SIZE(p)   ((p)->bytes_per_line * (p)->height)
#   define IMAGE_FREE     XDestroyImage
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  vout_Create    ( vout_thread_t * );
static void vout_Destroy   ( vout_thread_t * );
static void vout_Render    ( vout_thread_t *, picture_t * );
static void vout_Display   ( vout_thread_t *, picture_t * );
static int  vout_Manage    ( vout_thread_t * );
static int  vout_Init      ( vout_thread_t * );
static void vout_End       ( vout_thread_t * );

static int  InitDisplay    ( vout_thread_t * );

static int  CreateWindow   ( vout_thread_t * );
static void DestroyWindow  ( vout_thread_t * );

static int  NewPicture     ( vout_thread_t *, picture_t * );
static void FreePicture    ( vout_thread_t *, picture_t * );

static IMAGE_TYPE *CreateImage    ( Display *, EXTRA_ARGS, int, int );
#ifdef HAVE_SYS_SHM_H
static IMAGE_TYPE *CreateShmImage ( Display *, EXTRA_ARGS_SHM, int, int );
#endif

static void ToggleFullScreen      ( vout_thread_t * );

static void EnableXScreenSaver    ( vout_thread_t * );
static void DisableXScreenSaver   ( vout_thread_t * );

static void CreateCursor   ( vout_thread_t * );
static void DestroyCursor  ( vout_thread_t * );
static void ToggleCursor   ( vout_thread_t * );

#ifdef MODULE_NAME_IS_xvideo
static int  XVideoGetPort         ( Display *, u32, u32 * );
static void XVideoReleasePort     ( Display *, int );
#endif

/*****************************************************************************
 * vout_sys_t: video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the X11 and XVideo specific properties of an output thread.
 *****************************************************************************/
typedef struct vout_sys_s
{
    /* Internal settings and properties */
    Display *           p_display;                        /* display pointer */

    Visual *            p_visual;                          /* visual pointer */
    int                 i_screen;                           /* screen number */
    Window              window;                               /* root window */
    GC                  gc;              /* graphic context instance handler */

#ifdef HAVE_SYS_SHM_H
    boolean_t           b_shm;               /* shared memory extension flag */
#endif

#ifdef MODULE_NAME_IS_xvideo
    Window              yuv_window;   /* sub-window for displaying yuv video
                                                                        data */
    int                 i_xvport;
#else
    Colormap            colormap;               /* colormap used (8bpp only) */

    int                 i_screen_depth;
    int                 i_bytes_per_pixel;
    int                 i_bytes_per_line;
    int                 i_red_mask;
    int                 i_green_mask;
    int                 i_blue_mask;
#endif

    /* X11 generic properties */
    Atom                wm_protocols;
    Atom                wm_delete_window;

    int                 i_width;                     /* width of main window */
    int                 i_height;                   /* height of main window */

    /* Backup of window position and size before fullscreen switch */
    int                 i_width_backup;
    int                 i_height_backup;
    int                 i_xpos_backup;
    int                 i_ypos_backup;
    int                 i_width_backup_2;
    int                 i_height_backup_2;
    int                 i_xpos_backup_2;
    int                 i_ypos_backup_2;

    /* Screen saver properties */
    int                 i_ss_timeout;                             /* timeout */
    int                 i_ss_interval;           /* interval between changes */
    int                 i_ss_blanking;                      /* blanking mode */
    int                 i_ss_exposure;                      /* exposure mode */
#ifdef DPMSINFO_IN_DPMS_H
    BOOL                b_ss_dpms;                              /* DPMS mode */
#endif

    /* Mouse pointer properties */
    boolean_t           b_mouse_pointer_visible;
    mtime_t             i_time_mouse_last_moved; /* used to auto-hide pointer*/
    Cursor              blank_cursor;                   /* the hidden cursor */
    Pixmap              cursor_pixmap;

} vout_sys_t;

/*****************************************************************************
 * picture_sys_t: direct buffer method descriptor
 *****************************************************************************
 * This structure is part of the picture descriptor, it describes the
 * XVideo specific properties of a direct buffer.
 *****************************************************************************/
typedef struct picture_sys_s
{
    IMAGE_TYPE *        p_image;

#ifdef HAVE_SYS_SHM_H
    XShmSegmentInfo     shminfo;       /* shared memory zone information */
#endif

} picture_sys_t;

/*****************************************************************************
 * mwmhints_t: window manager hints
 *****************************************************************************
 * Fullscreen needs to be able to hide the wm decorations so we provide
 * this structure to make it easier.
 *****************************************************************************/
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define PROP_MWM_HINTS_ELEMENTS 5
typedef struct mwmhints_s
{
    u32 flags;
    u32 functions;
    u32 decorations;
    s32 input_mode;
    u32 status;
} mwmhints_t;

/*****************************************************************************
 * Chroma defines
 *****************************************************************************/
#ifdef MODULE_NAME_IS_xvideo
#   define MAX_DIRECTBUFFERS 10
#else
#   define MAX_DIRECTBUFFERS 2
#endif

/*****************************************************************************
 * Seeking function TODO: put this in a generic location !
 *****************************************************************************/
static __inline__ void vout_Seek( off_t i_seek )
{
    off_t i_tell;

    vlc_mutex_lock( &p_input_bank->lock );
    if( p_input_bank->pp_input[0] != NULL )
    {
#define S p_input_bank->pp_input[0]->stream
        i_tell = S.p_selected_area->i_tell + i_seek * (off_t)50 * S.i_mux_rate;

        i_tell = ( i_tell <= 0 /*S.p_selected_area->i_start*/ )
                   ? 0 /*S.p_selected_area->i_start*/
                   : ( i_tell >= S.p_selected_area->i_size )
                       ? S.p_selected_area->i_size
                       : i_tell;

        input_Seek( p_input_bank->pp_input[0], i_tell );
#undef S
    }
    vlc_mutex_unlock( &p_input_bank->lock );
}

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( vout_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->functions.vout.pf_create     = vout_Create;
    p_function_list->functions.vout.pf_init       = vout_Init;
    p_function_list->functions.vout.pf_end        = vout_End;
    p_function_list->functions.vout.pf_destroy    = vout_Destroy;
    p_function_list->functions.vout.pf_manage     = vout_Manage;
    p_function_list->functions.vout.pf_render     = vout_Render;
    p_function_list->functions.vout.pf_display    = vout_Display;
}

/*****************************************************************************
 * vout_Create: allocate X11 video thread output method
 *****************************************************************************
 * This function allocate and initialize a X11 vout method. It uses some of the
 * vout properties to choose the window size, and change them according to the
 * actual properties of the display.
 *****************************************************************************/
static int vout_Create( vout_thread_t *p_vout )
{
    char *psz_display;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        intf_ErrMsg( "vout error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    /* Open display, unsing the VOUT_DISPLAY_VAR config variable or the DISPLAY
     * environment variable */
    psz_display = config_GetPszVariable( VOUT_DISPLAY_VAR );
    p_vout->p_sys->p_display = XOpenDisplay( psz_display );

    if( p_vout->p_sys->p_display == NULL )                          /* error */
    {
        intf_ErrMsg( "vout error: cannot open display %s",
                     XDisplayName( psz_display ) );
        free( p_vout->p_sys );
        if( psz_display ) free( psz_display );
        return( 1 );
    }
    if( psz_display ) free( psz_display );

    p_vout->p_sys->i_screen = DefaultScreen( p_vout->p_sys->p_display );

#ifdef MODULE_NAME_IS_xvideo
    /* Check that we have access to an XVideo port providing this chroma */
    p_vout->p_sys->i_xvport = XVideoGetPort( p_vout->p_sys->p_display,
                                             p_vout->render.i_chroma,
                                             &p_vout->output.i_chroma );
    if( p_vout->p_sys->i_xvport < 0 )
    {
        /* It failed, but it's not completely lost ! We try to open an
         * XVideo port for an YUY2 picture. We'll need to do an YUV
         * conversion, but at least it has got scaling. */
        p_vout->p_sys->i_xvport = XVideoGetPort( p_vout->p_sys->p_display,
                                                 FOURCC_YUY2,
                                                 &p_vout->output.i_chroma );
        if( p_vout->p_sys->i_xvport < 0 )
        {
            /* It failed, but it's not completely lost ! We try to open an
             * XVideo port for a simple 16bpp RGB picture. We'll need to do
             * an YUV conversion, but at least it has got scaling. */
            p_vout->p_sys->i_xvport = XVideoGetPort( p_vout->p_sys->p_display,
                                                     FOURCC_RV16,
                                                     &p_vout->output.i_chroma );
            if( p_vout->p_sys->i_xvport < 0 )
            {
                XCloseDisplay( p_vout->p_sys->p_display );
                free( p_vout->p_sys );
                return 1;
            }
        }
    }
#endif

    /* Create blank cursor (for mouse cursor autohiding) */
    p_vout->p_sys->b_mouse_pointer_visible = 1;
    CreateCursor( p_vout );

    /* Spawn base window - this window will include the video output window,
     * but also command buttons, subtitles and other indicators */
    if( CreateWindow( p_vout ) )
    {
        intf_ErrMsg( "vout error: cannot create X11 window" );
        DestroyCursor( p_vout );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return( 1 );
    }

    /* Open and initialize device. */
    if( InitDisplay( p_vout ) )
    {
        intf_ErrMsg( "vout error: cannot initialize X11 display" );
        DestroyCursor( p_vout );
        DestroyWindow( p_vout );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return( 1 );
    }

    /* Disable screen saver and return */
    DisableXScreenSaver( p_vout );

    return( 0 );
}

/*****************************************************************************
 * vout_Destroy: destroy X11 video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_CreateOutputMethod
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    /* Restore cursor if it was blanked */
    if( !p_vout->p_sys->b_mouse_pointer_visible )
    {
        ToggleCursor( p_vout );
    }

#ifdef MODULE_NAME_IS_xvideo   
    XVideoReleasePort( p_vout->p_sys->p_display, p_vout->p_sys->i_xvport );
#else
#if 0
    /* Destroy colormap */
    if( p_vout->p_sys->i_screen_depth == 8 )
    {
        XFreeColormap( p_vout->p_sys->p_display, p_vout->p_sys->colormap );
    }
#endif
#endif

    DestroyCursor( p_vout );
    EnableXScreenSaver( p_vout );
    DestroyWindow( p_vout );

    XCloseDisplay( p_vout->p_sys->p_display );

    /* Destroy structure */
    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_Init: initialize X11 video thread output method
 *****************************************************************************
 * This function create the XImages needed by the output thread. It is called
 * at the beginning of the thread, but also each time the window is resized.
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;

    I_OUTPUTPICTURES = 0;

#ifdef MODULE_NAME_IS_xvideo
    /* Initialize the output structure; we already found an XVideo port,
     * and the corresponding chroma we will be using. Since we can
     * arbitrary scale, stick to the coordinates and aspect. */
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

#else
    /* Initialize the output structure: RGB with square pixels, whatever
     * the input format is, since it's the only format we know */
    switch( p_vout->p_sys->i_screen_depth )
    {
        case 8: /* FIXME: set the palette */
            p_vout->output.i_chroma = FOURCC_BI_RGB; break;
        case 15:
            p_vout->output.i_chroma = FOURCC_RV15; break;
        case 16:
            p_vout->output.i_chroma = FOURCC_RV16; break;
        case 24:
            p_vout->output.i_chroma = FOURCC_BI_BITFIELDS; break;
        case 32:
            p_vout->output.i_chroma = FOURCC_BI_BITFIELDS; break;
        default:
            intf_ErrMsg( "vout error: unknown screen depth" );
            return( 0 );
    }

    p_vout->output.i_width = p_vout->p_sys->i_width;
    p_vout->output.i_height = p_vout->p_sys->i_height;

    /* Assume we have square pixels */
    p_vout->output.i_aspect = p_vout->p_sys->i_width
                               * VOUT_ASPECT_FACTOR / p_vout->p_sys->i_height;
#endif

    /* Try to initialize up to MAX_DIRECTBUFFERS direct buffers */
    while( I_OUTPUTPICTURES < MAX_DIRECTBUFFERS )
    {
        p_pic = NULL;

        /* Find an empty picture slot */
        for( i_index = 0 ; i_index < VOUT_MAX_PICTURES ; i_index++ )
        {
            if( p_vout->p_picture[ i_index ].i_status == FREE_PICTURE )
            {
                p_pic = p_vout->p_picture + i_index;
                break;
            }
        }

        /* Allocate the picture */
        if( p_pic == NULL || NewPicture( p_vout, p_pic ) )
        {
            break;
        }

        p_pic->i_status = DESTROYED_PICTURE;
        p_pic->i_type   = DIRECT_PICTURE;

        PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

        I_OUTPUTPICTURES++;
    }

    return( 0 );
}

/*****************************************************************************
 * vout_Render: render previously calculated output
 *****************************************************************************/
static void vout_Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    ;
}

 /*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function sends the currently rendered image to X11 server.
 * (The Xv extension takes care of "double-buffering".)
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    int i_width, i_height, i_x, i_y;

    vout_PlacePicture( p_vout, p_vout->p_sys->i_width, p_vout->p_sys->i_height,
                       &i_x, &i_y, &i_width, &i_height );

#ifdef HAVE_SYS_SHM_H
    if( p_vout->p_sys->b_shm )
    {
        /* Display rendered image using shared memory extension */
#   ifdef MODULE_NAME_IS_xvideo
        XvShmPutImage( p_vout->p_sys->p_display, p_vout->p_sys->i_xvport,
                       p_vout->p_sys->yuv_window, p_vout->p_sys->gc,
                       p_pic->p_sys->p_image, 0 /*src_x*/, 0 /*src_y*/,
                       p_vout->output.i_width, p_vout->output.i_height,
                       0 /*dest_x*/, 0 /*dest_y*/, i_width, i_height,
                       False /* Don't put True here or you'll waste your CPU */ );
#   else
        XShmPutImage( p_vout->p_sys->p_display, p_vout->p_sys->window,
                      p_vout->p_sys->gc, p_pic->p_sys->p_image,
                      0 /*src_x*/, 0 /*src_y*/, 0 /*dest_x*/, 0 /*dest_y*/,
                      p_vout->output.i_width, p_vout->output.i_height,
                      False /* Don't put True here ! */ );
#   endif
    }
    else
#endif /* HAVE_SYS_SHM_H */
    {
        /* Use standard XPutImage -- this is gonna be slow ! */
#ifdef MODULE_NAME_IS_xvideo
        XvPutImage( p_vout->p_sys->p_display, p_vout->p_sys->i_xvport,
                    p_vout->p_sys->yuv_window, p_vout->p_sys->gc,
                    p_pic->p_sys->p_image, 0 /*src_x*/, 0 /*src_y*/,
                    p_vout->output.i_width, p_vout->output.i_height,
                    0 /*dest_x*/, 0 /*dest_y*/, i_width, i_height );
#else
        XPutImage( p_vout->p_sys->p_display, p_vout->p_sys->window,
                   p_vout->p_sys->gc, p_pic->p_sys->p_image,
                   0 /*src_x*/, 0 /*src_y*/, 0 /*dest_x*/, 0 /*dest_y*/,
                   p_vout->output.i_width, p_vout->output.i_height );
#endif
    }

    /* Make sure the command is sent now - do NOT use XFlush !*/
    XSync( p_vout->p_sys->p_display, False );
}

/*****************************************************************************
 * vout_Manage: handle X11 events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * X11 events and allows window resizing. It returns a non null value on
 * error.
 *****************************************************************************/
static int vout_Manage( vout_thread_t *p_vout )
{
    XEvent      xevent;                                         /* X11 event */
    boolean_t   b_resized;                        /* window has been resized */
    char        i_key;                                    /* ISO Latin-1 key */
    KeySym      x_key_symbol;

    /* Handle X11 events: ConfigureNotify events are parsed to know if the
     * output window's size changed, MapNotify and UnmapNotify to know if the
     * window is mapped (and if the display is useful), and ClientMessages
     * to intercept window destruction requests */

    b_resized = 0;
    while( XCheckWindowEvent( p_vout->p_sys->p_display, p_vout->p_sys->window,
                              StructureNotifyMask | KeyPressMask |
                              ButtonPressMask | ButtonReleaseMask | 
                              PointerMotionMask | Button1MotionMask , &xevent )
           == True )
    {
        /* ConfigureNotify event: prepare  */
        if( xevent.type == ConfigureNotify )
        {
            if( (xevent.xconfigure.width != p_vout->p_sys->i_width)
                 || (xevent.xconfigure.height != p_vout->p_sys->i_height) )
            {
                /* Update dimensions */
                b_resized = 1;
                p_vout->i_changes |= VOUT_SIZE_CHANGE;
                p_vout->p_sys->i_width = xevent.xconfigure.width;
                p_vout->p_sys->i_height = xevent.xconfigure.height;
            }
        }
        /* MapNotify event: change window status and disable screen saver */
        else if( xevent.type == MapNotify)
        {
            if( (p_vout != NULL) && !p_vout->b_active )
            {
                DisableXScreenSaver( p_vout );
                p_vout->b_active = 1;
            }
        }
        /* UnmapNotify event: change window status and enable screen saver */
        else if( xevent.type == UnmapNotify )
        {
            if( (p_vout != NULL) && p_vout->b_active )
            {
                EnableXScreenSaver( p_vout );
                p_vout->b_active = 0;
            }
        }
        /* Keyboard event */
        else if( xevent.type == KeyPress )
        {
            /* We may have keys like F1 trough F12, ESC ... */
            x_key_symbol = XKeycodeToKeysym( p_vout->p_sys->p_display,
                                             xevent.xkey.keycode, 0 );
            switch( x_key_symbol )
            {
                 case XK_Escape:
                     p_main->p_intf->b_die = 1;
                     break;
                 case XK_Menu:
                     p_main->p_intf->b_menu_change = 1;
                     break;
                 case XK_Left:
                     vout_Seek( -5 );
                     break;
                 case XK_Right:
                     vout_Seek( 5 );
                     break;
                 case XK_Up:
                     vout_Seek( 60 );
                     break;
                 case XK_Down:
                     vout_Seek( -60 );
                     break;
                 case XK_Home:
                     input_Seek( p_input_bank->pp_input[0],
                     p_input_bank->pp_input[0]->stream.p_selected_area->i_start );
                     break;
                 case XK_End:
                     input_Seek( p_input_bank->pp_input[0],
                     p_input_bank->pp_input[0]->stream.p_selected_area->i_size );
                     break;
                 case XK_Page_Up:
                     vout_Seek( 900 );
                     break;
                 case XK_Page_Down:
                     vout_Seek( -900 );
                     break;
                 case XK_space:
                     input_SetStatus( p_input_bank->pp_input[0],
                                      INPUT_STATUS_PAUSE );
                     break;

                 default:
                     /* "Normal Keys"
                      * The reason why I use this instead of XK_0 is that 
                      * with XLookupString, we don't have to care about
                      * keymaps. */

                    if( XLookupString( &xevent.xkey, &i_key, 1, NULL, NULL ) )
                    {
                        /* FIXME: handle stuff here */
                        switch( i_key )
                        {
                        case 'q':
                        case 'Q':
                            p_main->p_intf->b_die = 1;
                            break;
                        case 'f':
                        case 'F':
                            p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
                            break;

                        case '0': network_ChannelJoin( 0 ); break;
                        case '1': network_ChannelJoin( 1 ); break;
                        case '2': network_ChannelJoin( 2 ); break;
                        case '3': network_ChannelJoin( 3 ); break;
                        case '4': network_ChannelJoin( 4 ); break;
                        case '5': network_ChannelJoin( 5 ); break;
                        case '6': network_ChannelJoin( 6 ); break;
                        case '7': network_ChannelJoin( 7 ); break;
                        case '8': network_ChannelJoin( 8 ); break;
                        case '9': network_ChannelJoin( 9 ); break;

                        default:
                            break;
                        }
                    }
                break;
            }
        }
        /* Mouse click */
        else if( xevent.type == ButtonPress )
        {
            switch( ((XButtonEvent *)&xevent)->button )
            {
                case Button1:
                    /* In this part we will eventually manage
                     * clicks for DVD navigation for instance. For the
                     * moment just pause the stream. */
                    input_SetStatus( p_input_bank->pp_input[0],
                                     INPUT_STATUS_PAUSE );
                    break;

                case Button4:
                    vout_Seek( 15 );
                    break;

                case Button5:
                    vout_Seek( -15 );
                    break;
            }
        }
        /* Mouse release */
        else if( xevent.type == ButtonRelease )
        {
            switch( ((XButtonEvent *)&xevent)->button )
            {
                case Button3:
                    /* FIXME: need locking ! */
                    p_main->p_intf->b_menu_change = 1;
                    break;
            }
        }
        /* Mouse move */
        else if( xevent.type == MotionNotify )
        {
            p_vout->p_sys->i_time_mouse_last_moved = mdate();
            if( ! p_vout->p_sys->b_mouse_pointer_visible )
            {
                ToggleCursor( p_vout ); 
            }
        }
        /* Reparent move -- XXX: why are we getting this ? */
        else if( xevent.type == ReparentNotify )
        {
            ;
        }
        /* Other event */
        else
        {
            intf_WarnMsg( 3, "vout: unhandled event %d received", xevent.type );
        }
    }

#ifdef MODULE_NAME_IS_xvideo
    /* Handle events for YUV video output sub-window */
    while( XCheckWindowEvent( p_vout->p_sys->p_display,
                              p_vout->p_sys->yuv_window,
                              ExposureMask, &xevent ) == True )
    {
        /* Window exposed (only handled if stream playback is paused) */
        if( xevent.type == Expose )
        {
            if( ((XExposeEvent *)&xevent)->count == 0 )
            {
                /* (if this is the last a collection of expose events...) */
                if( p_input_bank->pp_input[0] != NULL )
                {
                    if( PAUSE_S ==
                            p_input_bank->pp_input[0]->stream.control.i_status )
                    {
/*                        XVideoDisplay( p_vout )*/;
                    }
                }
            }
        }
    }
#endif

    /* ClientMessage event - only WM_PROTOCOLS with WM_DELETE_WINDOW data
     * are handled - according to the man pages, the format is always 32
     * in this case */
    while( XCheckTypedEvent( p_vout->p_sys->p_display,
                             ClientMessage, &xevent ) )
    {
        if( (xevent.xclient.message_type == p_vout->p_sys->wm_protocols)
            && (xevent.xclient.data.l[0] == p_vout->p_sys->wm_delete_window ) )
        {
            p_main->p_intf->b_die = 1;
        }
    }

    /*
     * Fullscreen Change
     */
    if ( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        ToggleFullScreen( p_vout );
        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;

    }


#ifdef MODULE_NAME_IS_x11
    /*
     * Handle vout window resizing
     */
#if 0
    if( b_resized )
    {
        /* If interface window has been resized, change vout size */
        p_vout->i_width =  p_vout->p_sys->i_width;
        p_vout->i_height = p_vout->p_sys->i_height;
        p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }
    else if( (p_vout->i_width  != p_vout->p_sys->i_width) ||
             (p_vout->i_height != p_vout->p_sys->i_height) )
    {
        /* If video output size has changed, change interface window size */
        p_vout->p_sys->i_width =    p_vout->i_width;
        p_vout->p_sys->i_height =   p_vout->i_height;
        XResizeWindow( p_vout->p_sys->p_display, p_vout->p_sys->window,
                       p_vout->p_sys->i_width, p_vout->p_sys->i_height );
    }
    /*
     * Color/Grayscale or gamma change: in 8bpp, just change the colormap
     */
    if( (p_vout->i_changes & VOUT_GRAYSCALE_CHANGE)
        && (p_vout->i_screen_depth == 8) )
    {
        /* FIXME: clear flags ?? */
    }

    /*
     * Size change
     */
    if( p_vout->i_changes & VOUT_SIZE_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_SIZE_CHANGE;

        /* Resize window */
        XResizeWindow( p_vout->p_sys->p_display, p_vout->p_sys->window,
                       p_vout->i_width, p_vout->i_height );

        /* Destroy XImages to change their size */
        vout_End( p_vout );

        /* Recreate XImages. If SysInit failed, the thread can't go on. */
        if( vout_Init( p_vout ) )
        {
            intf_ErrMsg( "vout error: cannot resize display" );
            return( 1 );
       }

        /* Tell the video output thread that it will need to rebuild YUV
         * tables. This is needed since conversion buffer size may have
         * changed */
        p_vout->i_changes |= VOUT_YUV_CHANGE;
        intf_Msg( "vout: video display resized (%dx%d)",
                  p_vout->i_width, p_vout->i_height);
    }
#endif /* #if 0 */
#else
    /*
     * Size change
     *
     * (Needs to be placed after VOUT_FULLSREEN_CHANGE because we can activate
     *  the size flag inside the fullscreen routine)
     */
    if( p_vout->i_changes & VOUT_SIZE_CHANGE )
    {
        int i_width, i_height, i_x, i_y;

        p_vout->i_changes &= ~VOUT_SIZE_CHANGE;

        intf_WarnMsg( 3, "vout: video display resized (%dx%d)",
                      p_vout->p_sys->i_width,
                      p_vout->p_sys->i_height );
 
        vout_PlacePicture( p_vout, p_vout->p_sys->i_width,
                           p_vout->p_sys->i_height,
                           &i_x, &i_y, &i_width, &i_height );

        XResizeWindow( p_vout->p_sys->p_display, p_vout->p_sys->yuv_window,
                       i_width, i_height );
        
        XMoveWindow( p_vout->p_sys->p_display, p_vout->p_sys->yuv_window,
                     i_x, i_y );

    }
#endif

    /* Autohide Cursour */
    if( mdate() - p_vout->p_sys->i_time_mouse_last_moved > 2000000 )
    {
        /* Hide the mouse automatically */
        if( p_vout->p_sys->b_mouse_pointer_visible )
        {
            ToggleCursor( p_vout ); 
        }
    }

    return 0;
}

/*****************************************************************************
 * vout_End: terminate X11 video thread output method
 *****************************************************************************
 * Destroy the X11 XImages created by vout_Init. It is called at the end of
 * the thread, but also each time the window is resized.
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
    int i_index;

    /* Free the direct buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
        FreePicture( p_vout, PP_OUTPUTPICTURE[ i_index ] );
    }
}

/* following functions are local */

/*****************************************************************************
 * CreateWindow: open and set-up X11 main window
 *****************************************************************************/
static int CreateWindow( vout_thread_t *p_vout )
{
    XSizeHints              xsize_hints;
    XSetWindowAttributes    xwindow_attributes;
    XGCValues               xgcvalues;
    XEvent                  xevent;

    boolean_t               b_expose;
    boolean_t               b_configure_notify;
    boolean_t               b_map_notify;

    /* Set main window's size */
    if( p_vout->render.i_height * p_vout->render.i_aspect
        >= p_vout->render.i_width * VOUT_ASPECT_FACTOR )
    {
        p_vout->p_sys->i_width = p_vout->render.i_height
          * p_vout->render.i_aspect / VOUT_ASPECT_FACTOR;
        p_vout->p_sys->i_height = p_vout->render.i_height;
    }
    else
    {
        p_vout->p_sys->i_width = p_vout->render.i_width;
        p_vout->p_sys->i_height = p_vout->render.i_width
          * VOUT_ASPECT_FACTOR / p_vout->render.i_aspect;
    }

#if 0
    if( p_vout->p_sys->i_width <= 300 && p_vout->p_sys->i_height <= 300 )
    {
        p_vout->p_sys->i_width <<= 1;
        p_vout->p_sys->i_height <<= 1;
    }
    else if( p_vout->p_sys->i_width <= 400
             && p_vout->p_sys->i_height <= 400 )
    {
        p_vout->p_sys->i_width += p_vout->p_sys->i_width >> 1;
        p_vout->p_sys->i_height += p_vout->p_sys->i_height >> 1;
    }
#endif

    /* Prepare window manager hints and properties */
    xsize_hints.base_width          = p_vout->p_sys->i_width;
    xsize_hints.base_height         = p_vout->p_sys->i_height;
    xsize_hints.flags               = PSize;
    p_vout->p_sys->wm_protocols     = XInternAtom( p_vout->p_sys->p_display,
                                                   "WM_PROTOCOLS", True );
    p_vout->p_sys->wm_delete_window = XInternAtom( p_vout->p_sys->p_display,
                                                   "WM_DELETE_WINDOW", True );

    /* Prepare window attributes */
    xwindow_attributes.backing_store = Always;       /* save the hidden part */
    xwindow_attributes.background_pixel = BlackPixel(p_vout->p_sys->p_display,
                                                     p_vout->p_sys->i_screen);
    xwindow_attributes.event_mask = ExposureMask | StructureNotifyMask;
    

    /* Create the window and set hints - the window must receive
     * ConfigureNotify events, and until it is displayed, Expose and
     * MapNotify events. */

    p_vout->p_sys->window =
        XCreateWindow( p_vout->p_sys->p_display,
                       DefaultRootWindow( p_vout->p_sys->p_display ),
                       0, 0,
                       p_vout->p_sys->i_width,
                       p_vout->p_sys->i_height,
                       0,
                       0, InputOutput, 0,
                       CWBackingStore | CWBackPixel | CWEventMask,
                       &xwindow_attributes );

    /* Set window manager hints and properties: size hints, command,
     * window's name, and accepted protocols */
    XSetWMNormalHints( p_vout->p_sys->p_display, p_vout->p_sys->window,
                       &xsize_hints );
    XSetCommand( p_vout->p_sys->p_display, p_vout->p_sys->window,
                 p_main->ppsz_argv, p_main->i_argc );
    XStoreName( p_vout->p_sys->p_display, p_vout->p_sys->window,
#ifdef MODULE_NAME_IS_x11
                VOUT_TITLE " (X11 output)"
#else
                VOUT_TITLE " (XVideo output)"
#endif
              );

    if( (p_vout->p_sys->wm_protocols == None)        /* use WM_DELETE_WINDOW */
        || (p_vout->p_sys->wm_delete_window == None)
        || !XSetWMProtocols( p_vout->p_sys->p_display, p_vout->p_sys->window,
                             &p_vout->p_sys->wm_delete_window, 1 ) )
    {
        /* WM_DELETE_WINDOW is not supported by window manager */
        intf_Msg( "vout error: missing or bad window manager" );
    } 

    /* Creation of a graphic context that doesn't generate a GraphicsExpose
     * event when using functions like XCopyArea */
    xgcvalues.graphics_exposures = False;
    p_vout->p_sys->gc = XCreateGC( p_vout->p_sys->p_display,
                                   p_vout->p_sys->window,
                                   GCGraphicsExposures, &xgcvalues);

    /* Send orders to server, and wait until window is displayed - three
     * events must be received: a MapNotify event, an Expose event allowing
     * drawing in the window, and a ConfigureNotify to get the window
     * dimensions. Once those events have been received, only ConfigureNotify
     * events need to be received. */
    b_expose = 0;
    b_configure_notify = 0;
    b_map_notify = 0;
    XMapWindow( p_vout->p_sys->p_display, p_vout->p_sys->window);
    do
    {
        XNextEvent( p_vout->p_sys->p_display, &xevent);
        if( (xevent.type == Expose)
            && (xevent.xexpose.window == p_vout->p_sys->window) )
        {
            b_expose = 1;
        }
        else if( (xevent.type == MapNotify)
                 && (xevent.xmap.window == p_vout->p_sys->window) )
        {
            b_map_notify = 1;
        }
        else if( (xevent.type == ConfigureNotify)
                 && (xevent.xconfigure.window == p_vout->p_sys->window) )
        {
            b_configure_notify = 1;
            p_vout->p_sys->i_width = xevent.xconfigure.width;
            p_vout->p_sys->i_height = xevent.xconfigure.height;
        }
    } while( !( b_expose && b_configure_notify && b_map_notify ) );

    XSelectInput( p_vout->p_sys->p_display, p_vout->p_sys->window,
                  StructureNotifyMask | KeyPressMask |
                  ButtonPressMask | ButtonReleaseMask | 
                  PointerMotionMask );

#ifdef MODULE_NAME_IS_x11
    if( XDefaultDepth(p_vout->p_sys->p_display, p_vout->p_sys->i_screen) == 8 )
    {
        /* Allocate a new palette */
        p_vout->p_sys->colormap =
            XCreateColormap( p_vout->p_sys->p_display,
                             DefaultRootWindow( p_vout->p_sys->p_display ),
                             DefaultVisual( p_vout->p_sys->p_display,
                                            p_vout->p_sys->i_screen ),
                             AllocAll );

        xwindow_attributes.colormap = p_vout->p_sys->colormap;
        XChangeWindowAttributes( p_vout->p_sys->p_display,
                                 p_vout->p_sys->window,
                                 CWColormap, &xwindow_attributes );
    }

#else
    /* Create YUV output sub-window. */
    p_vout->p_sys->yuv_window = XCreateSimpleWindow( p_vout->p_sys->p_display,
                         p_vout->p_sys->window, 0, 0,
                         p_vout->p_sys->i_width,
                         p_vout->p_sys->i_height,
                         0,
                         BlackPixel( p_vout->p_sys->p_display,
                                         p_vout->p_sys->i_screen ),
                         WhitePixel( p_vout->p_sys->p_display,
                                         p_vout->p_sys->i_screen ) );
    
    XSetWindowBackground( p_vout->p_sys->p_display, p_vout->p_sys->yuv_window,
             BlackPixel(p_vout->p_sys->p_display, p_vout->p_sys->i_screen ) );
    
    XMapWindow( p_vout->p_sys->p_display, p_vout->p_sys->yuv_window );
    XSelectInput( p_vout->p_sys->p_display, p_vout->p_sys->yuv_window,
                  ExposureMask );
#endif

    /* If the cursor was formerly blank than blank it again */
    if( !p_vout->p_sys->b_mouse_pointer_visible )
    {
        ToggleCursor( p_vout );
        ToggleCursor( p_vout );
    }

    /* Do NOT use XFlush here ! */
    XSync( p_vout->p_sys->p_display, False );

    /* At this stage, the window is open, displayed, and ready to
     * receive data */

    return( 0 );
}

/*****************************************************************************
 * DestroyWindow: destroy the window
 *****************************************************************************
 *
 *****************************************************************************/
static void DestroyWindow( vout_thread_t *p_vout )
{
    /* Do NOT use XFlush here ! */
    XSync( p_vout->p_sys->p_display, False );

#ifdef MODULE_NAME_IS_xvideo
    XDestroyWindow( p_vout->p_sys->p_display, p_vout->p_sys->yuv_window );
#endif

    XUnmapWindow( p_vout->p_sys->p_display, p_vout->p_sys->window );
    XFreeGC( p_vout->p_sys->p_display, p_vout->p_sys->gc );
    XDestroyWindow( p_vout->p_sys->p_display, p_vout->p_sys->window );
}

/*****************************************************************************
 * NewPicture: allocate a picture
 *****************************************************************************
 * Returns 0 on success, -1 otherwise
 *****************************************************************************/
static int NewPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    /* We know the chroma, allocate a buffer which will be used
     * directly by the decoder */
    p_pic->p_sys = malloc( sizeof( picture_sys_t ) );

    if( p_pic->p_sys == NULL )
    {
        return -1;
    }

#ifdef HAVE_SYS_SHM_H
    if( p_vout->p_sys->b_shm )
    {
        /* Create image using XShm extension */
        p_pic->p_sys->p_image =
            CreateShmImage( p_vout->p_sys->p_display,
#   ifdef MODULE_NAME_IS_xvideo
                            p_vout->p_sys->i_xvport,
                            p_vout->output.i_chroma,
#   else
                            p_vout->p_sys->p_visual,
                            p_vout->p_sys->i_screen_depth,
#   endif
                            &p_pic->p_sys->shminfo,
                            p_vout->output.i_width, p_vout->output.i_height );
    }
    else
#endif /* HAVE_SYS_SHM_H */
    {
        /* Create image without XShm extension */
        p_pic->p_sys->p_image =
            CreateImage( p_vout->p_sys->p_display,
#ifdef MODULE_NAME_IS_xvideo
                         p_vout->p_sys->i_xvport,
                         p_vout->output.i_chroma,
#else
                         p_vout->p_sys->p_visual,
                         p_vout->p_sys->i_screen_depth, 
                         p_vout->p_sys->i_bytes_per_pixel,
#endif
                         p_vout->output.i_width, p_vout->output.i_height );
    }

    if( p_pic->p_sys->p_image == NULL )
    {
        free( p_pic->p_sys );
        return -1;
    }

    switch( p_vout->output.i_chroma )
    {
#ifdef MODULE_NAME_IS_xvideo
        case FOURCC_I420:

            p_pic->Y_PIXELS = p_pic->p_sys->p_image->data
                               + p_pic->p_sys->p_image->offsets[0];
            p_pic->p[Y_PLANE].i_lines = p_vout->output.i_height;
            p_pic->p[Y_PLANE].i_pitch = p_pic->p_sys->p_image->pitches[0];
            p_pic->p[Y_PLANE].i_pixel_bytes = 1;
            p_pic->p[Y_PLANE].b_margin = 0;

            p_pic->U_PIXELS = p_pic->p_sys->p_image->data
                               + p_pic->p_sys->p_image->offsets[1];
            p_pic->p[U_PLANE].i_lines = p_vout->output.i_height / 2;
            p_pic->p[U_PLANE].i_pitch = p_pic->p_sys->p_image->pitches[1];
            p_pic->p[U_PLANE].i_pixel_bytes = 1;
            p_pic->p[U_PLANE].b_margin = 0;

            p_pic->V_PIXELS = p_pic->p_sys->p_image->data
                               + p_pic->p_sys->p_image->offsets[2];
            p_pic->p[V_PLANE].i_lines = p_vout->output.i_height / 2;
            p_pic->p[V_PLANE].i_pitch = p_pic->p_sys->p_image->pitches[2];
            p_pic->p[V_PLANE].i_pixel_bytes = 1;
            p_pic->p[V_PLANE].b_margin = 0;

            p_pic->i_planes = 3;
            break;

        case FOURCC_YV12:

            p_pic->Y_PIXELS = p_pic->p_sys->p_image->data
                               + p_pic->p_sys->p_image->offsets[0];
            p_pic->p[Y_PLANE].i_lines = p_vout->output.i_height;
            p_pic->p[Y_PLANE].i_pitch = p_pic->p_sys->p_image->pitches[0];
            p_pic->p[Y_PLANE].i_pixel_bytes = 1;
            p_pic->p[Y_PLANE].b_margin = 0;

            p_pic->U_PIXELS = p_pic->p_sys->p_image->data
                               + p_pic->p_sys->p_image->offsets[2];
            p_pic->p[U_PLANE].i_lines = p_vout->output.i_height / 2;
            p_pic->p[U_PLANE].i_pitch = p_pic->p_sys->p_image->pitches[2];
            p_pic->p[U_PLANE].i_pixel_bytes = 1;
            p_pic->p[U_PLANE].b_margin = 0;

            p_pic->V_PIXELS = p_pic->p_sys->p_image->data
                               + p_pic->p_sys->p_image->offsets[1];
            p_pic->p[V_PLANE].i_lines = p_vout->output.i_height / 2;
            p_pic->p[V_PLANE].i_pitch = p_pic->p_sys->p_image->pitches[1];
            p_pic->p[V_PLANE].i_pixel_bytes = 1;
            p_pic->p[V_PLANE].b_margin = 0;

            p_pic->i_planes = 3;
            break;

        case FOURCC_Y211:

            p_pic->p->p_pixels = p_pic->p_sys->p_image->data
                                  + p_pic->p_sys->p_image->offsets[0];
            p_pic->p->i_lines = p_vout->output.i_height;
            /* XXX: this just looks so plain wrong... check it out ! */
            p_pic->p->i_pitch = p_pic->p_sys->p_image->pitches[0] / 4;
            p_pic->p->i_pixel_bytes = 4;
            p_pic->p->b_margin = 0;

            p_pic->i_planes = 1;
            break;

        case FOURCC_YUY2:
        case FOURCC_UYVY:

            p_pic->p->p_pixels = p_pic->p_sys->p_image->data
                                  + p_pic->p_sys->p_image->offsets[0];
            p_pic->p->i_lines = p_vout->output.i_height;
            p_pic->p->i_pitch = p_pic->p_sys->p_image->pitches[0];
            p_pic->p->i_pixel_bytes = 4;
            p_pic->p->b_margin = 0;

            p_pic->i_planes = 1;
            break;

        case FOURCC_RV15:

            p_pic->p->p_pixels = p_pic->p_sys->p_image->data
                                  + p_pic->p_sys->p_image->offsets[0];
            p_pic->p->i_lines = p_vout->output.i_height;
            p_pic->p->i_pitch = p_pic->p_sys->p_image->pitches[0];
            p_pic->p->i_pixel_bytes = 2;
            p_pic->p->b_margin = 0;

            p_pic->p->i_red_mask   = 0x001f;
            p_pic->p->i_green_mask = 0x07e0;
            p_pic->p->i_blue_mask  = 0xf800;

            p_pic->i_planes = 1;
            break;

        case FOURCC_RV16:

            p_pic->p->p_pixels = p_pic->p_sys->p_image->data
                                  + p_pic->p_sys->p_image->offsets[0];
            p_pic->p->i_lines = p_vout->output.i_height;
            p_pic->p->i_pitch = p_pic->p_sys->p_image->pitches[0];
            p_pic->p->i_pixel_bytes = 2;
            p_pic->p->b_margin = 0;

            p_pic->p->i_red_mask   = 0x001f;
            p_pic->p->i_green_mask = 0x03e0;
            p_pic->p->i_blue_mask  = 0x7c00;

            p_pic->i_planes = 1;
            break;

#else
        case FOURCC_RV16:
        case FOURCC_RV15:

            p_pic->p->p_pixels = p_pic->p_sys->p_image->data
                                  + p_pic->p_sys->p_image->xoffset;
            p_pic->p->i_lines = p_pic->p_sys->p_image->height;
            p_pic->p->i_pitch = p_pic->p_sys->p_image->bytes_per_line;
            p_pic->p->i_pixel_bytes = p_pic->p_sys->p_image->depth;

            if( p_pic->p->i_pitch == 2 * p_pic->p_sys->p_image->width )
            {
                p_pic->p->b_margin = 0;
            }
            else
            {
                p_pic->p->b_margin = 1;
                p_pic->p->b_hidden = 1;
                p_pic->p->i_visible_bytes = 2 * p_pic->p_sys->p_image->width;
            }

            p_pic->p->i_red_mask   = p_pic->p_sys->p_image->red_mask;
            p_pic->p->i_green_mask = p_pic->p_sys->p_image->green_mask;
            p_pic->p->i_blue_mask  = p_pic->p_sys->p_image->blue_mask;

            p_pic->i_planes = 1;

            break;

        case FOURCC_BI_BITFIELDS:

            p_pic->p->p_pixels = p_pic->p_sys->p_image->data
                                  + p_pic->p_sys->p_image->xoffset;
            p_pic->p->i_lines = p_pic->p_sys->p_image->height;
            p_pic->p->i_pitch = p_pic->p_sys->p_image->bytes_per_line;
            p_pic->p->i_pixel_bytes = p_pic->p_sys->p_image->depth;

            if( p_pic->p->i_pitch == 4 * p_pic->p_sys->p_image->width )
            {
                p_pic->p->b_margin = 0;
            }
            else
            {
                p_pic->p->b_margin = 1;
                p_pic->p->b_hidden = 1;
                p_pic->p->i_visible_bytes = 4 * p_pic->p_sys->p_image->width;
            }

            p_pic->p->i_red_mask   = p_pic->p_sys->p_image->red_mask;
            p_pic->p->i_green_mask = p_pic->p_sys->p_image->green_mask;
            p_pic->p->i_blue_mask  = p_pic->p_sys->p_image->blue_mask;

            p_pic->i_planes = 1;

            break;
#endif

        default:
            /* Unknown chroma, tell the guy to get lost */
            IMAGE_FREE( p_pic->p_sys->p_image );
            free( p_pic->p_sys );
            intf_ErrMsg( "vout error: never heard of chroma 0x%.8x (%4.4s)",
                         p_vout->output.i_chroma,
                         (char*)&p_vout->output.i_chroma );
            p_pic->i_planes = 0;
            return -1;
    }

    return 0;
}

/*****************************************************************************
 * FreePicture: destroy a picture allocated with NewPicture
 *****************************************************************************
 * Destroy XImage AND associated data. If using Shm, detach shared memory
 * segment from server and process, then free it. The XDestroyImage manpage
 * says that both the image structure _and_ the data pointed to by the
 * image structure are freed, so no need to free p_image->data.
 *****************************************************************************/
static void FreePicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    /* The order of operations is correct */
#ifdef HAVE_SYS_SHM_H
    if( p_vout->p_sys->b_shm )
    {
        XShmDetach( p_vout->p_sys->p_display, &p_pic->p_sys->shminfo );
        IMAGE_FREE( p_pic->p_sys->p_image );

        shmctl( p_pic->p_sys->shminfo.shmid, IPC_RMID, 0 );
        if( shmdt( p_pic->p_sys->shminfo.shmaddr ) )
        {
            intf_ErrMsg( "vout error: cannot detach shared memory (%s)",
                         strerror(errno) );
        }
    }
    else
#endif
    {
        IMAGE_FREE( p_pic->p_sys->p_image );
    }

    /* Do NOT use XFlush here ! */
    XSync( p_vout->p_sys->p_display, False );

    free( p_pic->p_sys );
}

/*****************************************************************************
 * ToggleFullScreen: Enable or disable full screen mode
 *****************************************************************************
 * This function will switch between fullscreen and window mode.
 *
 *****************************************************************************/
static void ToggleFullScreen ( vout_thread_t *p_vout )
{
    Atom prop;
    mwmhints_t mwmhints;
    int i_xpos, i_ypos, i_width, i_height;
    XEvent xevent;
#ifdef ALTERNATE_FULLSCREEN
    XSetWindowAttributes attributes;
#endif

    p_vout->b_fullscreen = !p_vout->b_fullscreen;

    if( p_vout->b_fullscreen )
    {
        Window next_parent, parent, *p_dummy, dummy1;
        unsigned int dummy2, dummy3;

        intf_WarnMsg( 3, "vout: entering fullscreen mode" );

        /* Save current window coordinates so they can be restored when
         * we exit from fullscreen mode. This is the tricky part because
         * this heavily depends on the behaviour of the window manager.
         * When you use XMoveWindow some window managers will adjust the top
         * of the window to the coordinates you gave, but others will instead
         * adjust the top of the client area to the coordinates
         * (don't forget windows have decorations). */

        /* First, get the position and size of the client area */
        XGetGeometry( p_vout->p_sys->p_display,
                      p_vout->p_sys->window,
                      &dummy1,
                      &dummy2,
                      &dummy3,
                      &p_vout->p_sys->i_width_backup_2,
                      &p_vout->p_sys->i_height_backup_2,
                      &dummy2, &dummy3 );
        XTranslateCoordinates( p_vout->p_sys->p_display,
                               p_vout->p_sys->window,
                               DefaultRootWindow( p_vout->p_sys->p_display ),
                               0,
                               0,
                               &p_vout->p_sys->i_xpos_backup_2,
                               &p_vout->p_sys->i_ypos_backup_2,
                               &dummy1 );

        /* Then try to get the position and size of the whole window */

        /* find the real parent of our window (created by the window manager),
         * the one which is a direct child of the root window */
        next_parent = parent = p_vout->p_sys->window;
        while( next_parent != DefaultRootWindow( p_vout->p_sys->p_display ) )
        {
            parent = next_parent;
            XQueryTree( p_vout->p_sys->p_display,
                        parent,
                        &dummy1,
                        &next_parent,
                        &p_dummy,
                        &dummy2 );
            XFree((void *)p_dummy);
        }

        XGetGeometry( p_vout->p_sys->p_display,
                      p_vout->p_sys->window,
                      &dummy1,
                      &dummy2,
                      &dummy3,
                      &p_vout->p_sys->i_width_backup,
                      &p_vout->p_sys->i_height_backup,
                      &dummy2, &dummy3 );

        XTranslateCoordinates( p_vout->p_sys->p_display,
                               parent,
                               DefaultRootWindow( p_vout->p_sys->p_display ),
                               0,
                               0,
                               &p_vout->p_sys->i_xpos_backup,
                               &p_vout->p_sys->i_ypos_backup,
                               &dummy1 );

        /* fullscreen window size and position */
        i_xpos = 0;
        i_ypos = 0;
        i_width = DisplayWidth( p_vout->p_sys->p_display,
                                p_vout->p_sys->i_screen );
        i_height = DisplayHeight( p_vout->p_sys->p_display,
                                  p_vout->p_sys->i_screen );

#if 0
        /* Being a transient window allows us to really be fullscreen (display
         * over the taskbar for instance) but then we end-up with the same
         * result as with the brute force method */
        XSetTransientForHint( p_vout->p_sys->p_display,
                              p_vout->p_sys->window, None );
#endif
    }
    else
    {
        intf_WarnMsg( 3, "vout: leaving fullscreen mode" );

        i_xpos = p_vout->p_sys->i_xpos_backup;
        i_ypos = p_vout->p_sys->i_ypos_backup;
        i_width = p_vout->p_sys->i_width_backup;
        i_height = p_vout->p_sys->i_height_backup;
    }

    /* To my knowledge there are two ways to create a borderless window.
     * There's the generic way which is to tell x to bypass the window manager,
     * but this creates problems with the focus of other applications.
     * The other way is to use the motif property "_MOTIF_WM_HINTS" which
     * luckily seems to be supported by most window managers.
     */
#ifndef ALTERNATE_FULLSCREEN
    mwmhints.flags = MWM_HINTS_DECORATIONS;
    mwmhints.decorations = !p_vout->b_fullscreen;

    prop = XInternAtom( p_vout->p_sys->p_display, "_MOTIF_WM_HINTS",
                        False );
    XChangeProperty( p_vout->p_sys->p_display, p_vout->p_sys->window,
                     prop, prop, 32, PropModeReplace,
                     (unsigned char *)&mwmhints,
                     PROP_MWM_HINTS_ELEMENTS );

#else
    /* brute force way to remove decorations */
    attributes.override_redirect = p_vout->b_fullscreen;
    XChangeWindowAttributes( p_vout->p_sys->p_display,
                             p_vout->p_sys->window,
                             CWOverrideRedirect,
                             &attributes);
#endif

    /* We need to unmap and remap the window if we want the window 
     * manager to take our changes into effect */
    XUnmapWindow( p_vout->p_sys->p_display, p_vout->p_sys->window);

    XWindowEvent( p_vout->p_sys->p_display, p_vout->p_sys->window,
                  StructureNotifyMask, &xevent );
    while( xevent.type != UnmapNotify )
        XWindowEvent( p_vout->p_sys->p_display, p_vout->p_sys->window,
                      StructureNotifyMask, &xevent );

    XMapRaised( p_vout->p_sys->p_display, p_vout->p_sys->window);

    while( xevent.type != MapNotify )
        XWindowEvent( p_vout->p_sys->p_display, p_vout->p_sys->window,
                      StructureNotifyMask, &xevent );

    XMoveResizeWindow( p_vout->p_sys->p_display,
                       p_vout->p_sys->window,
                       i_xpos,
                       i_ypos,
                       i_width,
                       i_height );

    /* Purge all ConfigureNotify events, this is needed to fix a bug where we
     * would lose the original size of the window */
    while( xevent.type != ConfigureNotify )
        XWindowEvent( p_vout->p_sys->p_display, p_vout->p_sys->window,
                      StructureNotifyMask, &xevent );
    while( XCheckWindowEvent( p_vout->p_sys->p_display, p_vout->p_sys->window,
                              StructureNotifyMask, &xevent ) );


    /* We need to check that the window was really restored where we wanted */
    if( !p_vout->b_fullscreen )
    {
        Window dummy1;
        unsigned int dummy2, dummy3, dummy4, dummy5;

        /* Check the position */
        XTranslateCoordinates( p_vout->p_sys->p_display,
                               p_vout->p_sys->window,
                               DefaultRootWindow( p_vout->p_sys->p_display ),
                               0,
                               0,
                               &dummy2,
                               &dummy3,
                               &dummy1 );
        if( dummy2 != p_vout->p_sys->i_xpos_backup_2 ||
            dummy3 != p_vout->p_sys->i_ypos_backup_2 )
        {
            /* Ok it didn't work... second try */

            XMoveWindow( p_vout->p_sys->p_display,
                         p_vout->p_sys->window,
                         p_vout->p_sys->i_xpos_backup_2,
                         p_vout->p_sys->i_ypos_backup_2 );
            
            /* Purge all ConfigureNotify events, this is needed to fix a bug
             * where we would lose the original size of the window */
            XWindowEvent( p_vout->p_sys->p_display, p_vout->p_sys->window,
                          StructureNotifyMask, &xevent );
            while( xevent.type != ConfigureNotify )
                XWindowEvent( p_vout->p_sys->p_display, p_vout->p_sys->window,
                              StructureNotifyMask, &xevent );
            while( XCheckWindowEvent( p_vout->p_sys->p_display,
                                      p_vout->p_sys->window,
                                      StructureNotifyMask, &xevent ) );
        }

        /* Check the size */
        XGetGeometry( p_vout->p_sys->p_display,
                      p_vout->p_sys->window,
                      &dummy1,
                      &dummy2,
                      &dummy3,
                      &dummy4,
                      &dummy5,
                      &dummy2, &dummy3 );

        if( dummy4 != p_vout->p_sys->i_width_backup_2 ||
            dummy5 != p_vout->p_sys->i_height_backup_2 )
        {
            /* Ok it didn't work... third try */

            XResizeWindow( p_vout->p_sys->p_display,
                         p_vout->p_sys->window,
                         p_vout->p_sys->i_width_backup_2,
                         p_vout->p_sys->i_height_backup_2 );
            
            /* Purge all ConfigureNotify events, this is needed to fix a bug
             * where we would lose the original size of the window */
            XWindowEvent( p_vout->p_sys->p_display, p_vout->p_sys->window,
                          StructureNotifyMask, &xevent );
            while( xevent.type != ConfigureNotify )
                XWindowEvent( p_vout->p_sys->p_display, p_vout->p_sys->window,
                              StructureNotifyMask, &xevent );
            while( XCheckWindowEvent( p_vout->p_sys->p_display,
                                      p_vout->p_sys->window,
                                      StructureNotifyMask, &xevent ) );
        }
    }

#ifdef ALTERNATE_FULLSCREEN
    XSetInputFocus(p_vout->p_sys->p_display,
                   p_vout->p_sys->window,
                   RevertToParent,
                   CurrentTime);
#endif

    /* signal that the size needs to be updated */
    p_vout->p_sys->i_width = i_width;
    p_vout->p_sys->i_height = i_height;
    p_vout->i_changes |= VOUT_SIZE_CHANGE;

}

/*****************************************************************************
 * EnableXScreenSaver: enable screen saver
 *****************************************************************************
 * This function enables the screen saver on a display after it has been
 * disabled by XDisableScreenSaver.
 * FIXME: what happens if multiple vlc sessions are running at the same
 *        time ???
 *****************************************************************************/
static void EnableXScreenSaver( vout_thread_t *p_vout )
{
#ifdef DPMSINFO_IN_DPMS_H
    int dummy;
#endif

    XSetScreenSaver( p_vout->p_sys->p_display, p_vout->p_sys->i_ss_timeout,
                     p_vout->p_sys->i_ss_interval,
                     p_vout->p_sys->i_ss_blanking,
                     p_vout->p_sys->i_ss_exposure );

    /* Restore DPMS settings */
#ifdef DPMSINFO_IN_DPMS_H
    if( DPMSQueryExtension( p_vout->p_sys->p_display, &dummy, &dummy ) )
    {
        if( p_vout->p_sys->b_ss_dpms )
        {
            DPMSEnable( p_vout->p_sys->p_display );
        }
    }
#endif
}

/*****************************************************************************
 * DisableXScreenSaver: disable screen saver
 *****************************************************************************
 * See XEnableXScreenSaver
 *****************************************************************************/
static void DisableXScreenSaver( vout_thread_t *p_vout )
{
#ifdef DPMSINFO_IN_DPMS_H
    int dummy;
#endif

    /* Save screen saver informations */
    XGetScreenSaver( p_vout->p_sys->p_display, &p_vout->p_sys->i_ss_timeout,
                     &p_vout->p_sys->i_ss_interval,
                     &p_vout->p_sys->i_ss_blanking,
                     &p_vout->p_sys->i_ss_exposure );

    /* Disable screen saver */
    XSetScreenSaver( p_vout->p_sys->p_display, 0,
                     p_vout->p_sys->i_ss_interval,
                     p_vout->p_sys->i_ss_blanking,
                     p_vout->p_sys->i_ss_exposure );

    /* Disable DPMS */
#ifdef DPMSINFO_IN_DPMS_H
    if( DPMSQueryExtension( p_vout->p_sys->p_display, &dummy, &dummy ) )
    {
        CARD16 unused;
        /* Save DPMS current state */
        DPMSInfo( p_vout->p_sys->p_display, &unused,
                  &p_vout->p_sys->b_ss_dpms );
        DPMSDisable( p_vout->p_sys->p_display );
   }
#endif
}

/*****************************************************************************
 * CreateCursor: create a blank mouse pointer
 *****************************************************************************/
static void CreateCursor( vout_thread_t *p_vout )
{
    XColor cursor_color;

    p_vout->p_sys->cursor_pixmap =
        XCreatePixmap( p_vout->p_sys->p_display,
                       DefaultRootWindow( p_vout->p_sys->p_display ),
                       1, 1, 1 );

    XParseColor( p_vout->p_sys->p_display,
                 XCreateColormap( p_vout->p_sys->p_display,
                                  DefaultRootWindow(
                                                    p_vout->p_sys->p_display ),
                                  DefaultVisual(
                                                p_vout->p_sys->p_display,
                                                p_vout->p_sys->i_screen ),
                                  AllocNone ),
                 "black", &cursor_color );

    p_vout->p_sys->blank_cursor =
        XCreatePixmapCursor( p_vout->p_sys->p_display,
                             p_vout->p_sys->cursor_pixmap,
                             p_vout->p_sys->cursor_pixmap,
                             &cursor_color, &cursor_color, 1, 1 );
}

/*****************************************************************************
 * DestroyCursor: destroy the blank mouse pointer
 *****************************************************************************/
static void DestroyCursor( vout_thread_t *p_vout )
{
    XFreePixmap( p_vout->p_sys->p_display, p_vout->p_sys->cursor_pixmap );
}

/*****************************************************************************
 * ToggleCursor: hide or show the mouse pointer
 *****************************************************************************
 * This function hides the X pointer if it is visible by setting the pointer
 * sprite to a blank one. To show it again, we disable the sprite.
 *****************************************************************************/
static void ToggleCursor( vout_thread_t *p_vout )
{
    if( p_vout->p_sys->b_mouse_pointer_visible )
    {
        XDefineCursor( p_vout->p_sys->p_display,
                       p_vout->p_sys->window,
                       p_vout->p_sys->blank_cursor );
        p_vout->p_sys->b_mouse_pointer_visible = 0;
    }
    else
    {
        XUndefineCursor( p_vout->p_sys->p_display, p_vout->p_sys->window );
        p_vout->p_sys->b_mouse_pointer_visible = 1;
    }
}

#ifdef MODULE_NAME_IS_xvideo
/*****************************************************************************
 * XVideoGetPort: get YUV12 port
 *****************************************************************************/
static int XVideoGetPort( Display *dpy, u32 i_chroma, u32 *pi_newchroma )
{
    XvAdaptorInfo *p_adaptor;
    unsigned int i;
    int i_adaptor, i_num_adaptors, i_requested_adaptor;
    int i_selected_port;

    switch( XvQueryExtension( dpy, &i, &i, &i, &i, &i ) )
    {
        case Success:
            break;

        case XvBadExtension:
            intf_WarnMsg( 3, "vout error: XvBadExtension" );
            return( -1 );

        case XvBadAlloc:
            intf_WarnMsg( 3, "vout error: XvBadAlloc" );
            return( -1 );

        default:
            intf_WarnMsg( 3, "vout error: XvQueryExtension failed" );
            return( -1 );
    }

    switch( XvQueryAdaptors( dpy, DefaultRootWindow( dpy ),
                             &i_num_adaptors, &p_adaptor ) )
    {
        case Success:
            break;

        case XvBadExtension:
            intf_WarnMsg( 3, "vout error: XvBadExtension for XvQueryAdaptors" );
            return( -1 );

        case XvBadAlloc:
            intf_WarnMsg( 3, "vout error: XvBadAlloc for XvQueryAdaptors" );
            return( -1 );

        default:
            intf_WarnMsg( 3, "vout error: XvQueryAdaptors failed" );
            return( -1 );
    }

    i_selected_port = -1;
    i_requested_adaptor = config_GetIntVariable( XVADAPTOR_VAR );

    for( i_adaptor = 0; i_adaptor < i_num_adaptors; ++i_adaptor )
    {
        XvImageFormatValues *p_formats;
        int i_format, i_num_formats;
        int i_port;

        /* If we requested an adaptor and it's not this one, we aren't
         * interested */
        if( i_requested_adaptor != -1 && i_adaptor != i_requested_adaptor )
        {
            continue;
        }

        /* If the adaptor doesn't have the required properties, skip it */
        if( !( p_adaptor[ i_adaptor ].type & XvInputMask ) ||
            !( p_adaptor[ i_adaptor ].type & XvImageMask ) )
        {
            continue;
        }

        /* Check that adaptor supports our requested format... */
        p_formats = XvListImageFormats( dpy, p_adaptor[i_adaptor].base_id,
                                        &i_num_formats );

        for( i_format = 0;
             i_format < i_num_formats && ( i_selected_port == -1 );
             i_format++ )
        {
            /* Code removed, we can get this through xvinfo anyway */
#if 0
            XvEncodingInfo  *p_enc;
            int             i_enc, i_num_encodings;
            XvAttribute     *p_attr;
            int             i_attr, i_num_attributes;
#endif

            /* If this is not the format we want, or at least a
             * similar one, forget it */
            if( !vout_ChromaCmp( p_formats[ i_format ].id, i_chroma ) )
            {
                continue;
            }

            /* Look for the first available port supporting this format */
            for( i_port = p_adaptor[i_adaptor].base_id;
                 ( i_port < p_adaptor[i_adaptor].base_id
                             + p_adaptor[i_adaptor].num_ports )
                   && ( i_selected_port == -1 );
                 i_port++ )
            {
                if( XvGrabPort( dpy, i_port, CurrentTime ) == Success )
                {
                    i_selected_port = i_port;
                    *pi_newchroma = p_formats[ i_format ].id;
                }
            }

            /* If no free port was found, forget it */
            if( i_selected_port == -1 )
            {
                continue;
            }

            /* If we found a port, print information about it */
            intf_WarnMsg( 3, "vout: found adaptor %i, port %i, "
                             "image format 0x%x (%4.4s) %s",
                             i_adaptor, i_selected_port,
                             p_formats[ i_format ].id,
                             (char *)&p_formats[ i_format ].id,
                             ( p_formats[ i_format ].format
                                == XvPacked ) ? "packed" : "planar" );

#if 0
            intf_WarnMsg( 10, " encoding list:" );

            if( XvQueryEncodings( dpy, i_selected_port,
                                  &i_num_encodings, &p_enc )
                 != Success )
            {
                intf_WarnMsg( 10, "  XvQueryEncodings failed" );
                continue;
            }

            for( i_enc = 0; i_enc < i_num_encodings; i_enc++ )
            {
                intf_WarnMsg( 10, "  id=%ld, name=%s, size=%ldx%ld,"
                                  " numerator=%d, denominator=%d",
                              p_enc[i_enc].encoding_id, p_enc[i_enc].name,
                              p_enc[i_enc].width, p_enc[i_enc].height,
                              p_enc[i_enc].rate.numerator,
                              p_enc[i_enc].rate.denominator );
            }

            if( p_enc != NULL )
            {
                XvFreeEncodingInfo( p_enc );
            }

            intf_WarnMsg( 10, " attribute list:" );
            p_attr = XvQueryPortAttributes( dpy, i_selected_port,
                                            &i_num_attributes );
            for( i_attr = 0; i_attr < i_num_attributes; i_attr++ )
            {
                intf_WarnMsg( 10,
                      "  name=%s, flags=[%s%s ], min=%i, max=%i",
                      p_attr[i_attr].name,
                      (p_attr[i_attr].flags & XvGettable) ? " get" : "",
                      (p_attr[i_attr].flags & XvSettable) ? " set" : "",
                      p_attr[i_attr].min_value, p_attr[i_attr].max_value );
            }

            if( p_attr != NULL )
            {
                XFree( p_attr );
            }
#endif
        }

        if( p_formats != NULL )
        {
            XFree( p_formats );
        }

    }

    if( i_num_adaptors > 0 )
    {
        XvFreeAdaptorInfo( p_adaptor );
    }

    if( i_selected_port == -1 )
    {
        if( i_requested_adaptor == -1 )
        {
            intf_WarnMsg( 3, "vout: no free XVideo port found for format "
                             "0x%.8x (%4.4s)", i_chroma, (char*)&i_chroma );
        }
        else
        {
            intf_WarnMsg( 3, "vout: XVideo adaptor %i does not have a free "
                             "XVideo port for format 0x%.8x (%4.4s)",
                             i_requested_adaptor, i_chroma, (char*)&i_chroma );
        }
    }

    return( i_selected_port );
}

/*****************************************************************************
 * XVideoReleasePort: release YUV12 port
 *****************************************************************************/
static void XVideoReleasePort( Display *dpy, int i_port )
{
    XvUngrabPort( dpy, i_port, CurrentTime );
}
#endif

/*****************************************************************************
 * InitDisplay: open and initialize X11 device
 *****************************************************************************
 * Create a window according to video output given size, and set other
 * properties according to the display properties.
 *****************************************************************************/
static int InitDisplay( vout_thread_t *p_vout )
{
#ifdef MODULE_NAME_IS_x11
    XPixmapFormatValues *       p_formats;                 /* pixmap formats */
    XVisualInfo *               p_xvisual;           /* visuals informations */
    XVisualInfo                 xvisual_template;         /* visual template */
    int                         i_count;                       /* array size */
#endif

#ifdef HAVE_SYS_SHM_H
#   ifdef SYS_DARWIN
    /* FIXME : As of 2001-03-16, XFree4 for MacOS X does not support Xshm. */
    p_vout->p_sys->b_shm = 0;
#   else
    p_vout->p_sys->b_shm = ( XShmQueryExtension( p_vout->p_sys->p_display )
                              == True );
#   endif
    if( !p_vout->p_sys->b_shm )
#endif
    {
        intf_WarnMsg( 1, "vout warning: XShm video extension is unavailable" );
    }

#ifdef MODULE_NAME_IS_xvideo
    /* XXX The brightness and contrast values should be read from environment
     * XXX variables... */
#if 0
    XVideoSetAttribute( p_vout, "XV_BRIGHTNESS", 0.5 );
    XVideoSetAttribute( p_vout, "XV_CONTRAST",   0.5 );
#endif
#endif

#ifdef MODULE_NAME_IS_x11
    /* Initialize structure */
    p_vout->p_sys->i_screen = DefaultScreen( p_vout->p_sys->p_display );

    /* Get screen depth */
    p_vout->p_sys->i_screen_depth = XDefaultDepth( p_vout->p_sys->p_display,
                                                   p_vout->p_sys->i_screen );
    switch( p_vout->p_sys->i_screen_depth )
    {
    case 8:
        /*
         * Screen depth is 8bpp. Use PseudoColor visual with private colormap.
         */
        xvisual_template.screen =   p_vout->p_sys->i_screen;
        xvisual_template.class =    DirectColor;
        p_xvisual = XGetVisualInfo( p_vout->p_sys->p_display,
                                    VisualScreenMask | VisualClassMask,
                                    &xvisual_template, &i_count );
        if( p_xvisual == NULL )
        {
            intf_ErrMsg( "vout error: no PseudoColor visual available" );
            return( 1 );
        }
        p_vout->p_sys->i_bytes_per_pixel = 1;
        break;
    case 15:
    case 16:
    case 24:
    default:
        /*
         * Screen depth is higher than 8bpp. TrueColor visual is used.
         */
        xvisual_template.screen =   p_vout->p_sys->i_screen;
        xvisual_template.class =    TrueColor;
        p_xvisual = XGetVisualInfo( p_vout->p_sys->p_display,
                                    VisualScreenMask | VisualClassMask,
                                    &xvisual_template, &i_count );
        if( p_xvisual == NULL )
        {
            intf_ErrMsg( "vout error: no TrueColor visual available" );
            return( 1 );
        }
        p_vout->p_sys->i_red_mask =        p_xvisual->red_mask;
        p_vout->p_sys->i_green_mask =      p_xvisual->green_mask;
        p_vout->p_sys->i_blue_mask =       p_xvisual->blue_mask;

        /* There is no difference yet between 3 and 4 Bpp. The only way
         * to find the actual number of bytes per pixel is to list supported
         * pixmap formats. */
        p_formats = XListPixmapFormats( p_vout->p_sys->p_display, &i_count );
        p_vout->p_sys->i_bytes_per_pixel = 0;

        for( ; i_count-- ; p_formats++ )
        {
            /* Under XFree4.0, the list contains pixmap formats available
             * through all video depths ; so we have to check against current
             * depth. */
            if( p_formats->depth == p_vout->p_sys->i_screen_depth )
            {
                if( p_formats->bits_per_pixel / 8
                        > p_vout->p_sys->i_bytes_per_pixel )
                {
                    p_vout->p_sys->i_bytes_per_pixel = p_formats->bits_per_pixel / 8;
                }
            }
        }
        break;
    }
    p_vout->p_sys->p_visual = p_xvisual->visual;
    XFree( p_xvisual );
#endif

    return( 0 );
}

#ifdef HAVE_SYS_SHM_H
/*****************************************************************************
 * CreateShmImage: create an XImage or XvImage using shared memory extension
 *****************************************************************************
 * Prepare an XImage or XvImage for display function.
 * The order of the operations respects the recommandations of the mit-shm
 * document by J.Corbet and K.Packard. Most of the parameters were copied from
 * there. See http://ftp.xfree86.org/pub/XFree86/4.0/doc/mit-shm.TXT
 *****************************************************************************/
static IMAGE_TYPE * CreateShmImage( Display* p_display, EXTRA_ARGS_SHM,
                                    int i_width, int i_height )
{
    IMAGE_TYPE *p_image;

    /* Create XImage / XvImage */
#ifdef MODULE_NAME_IS_xvideo
    p_image = XvShmCreateImage( p_display, i_xvport, i_chroma, 0,
                                i_width, i_height, p_shm );
#else
    p_image = XShmCreateImage( p_display, p_visual, i_depth, ZPixmap, 0,
                               p_shm, i_width, i_height );
#endif
    if( p_image == NULL )
    {
        intf_ErrMsg( "vout error: image creation failed." );
        return( NULL );
    }

    /* Allocate shared memory segment - 0776 set the access permission
     * rights (like umask), they are not yet supported by all X servers */
    p_shm->shmid = shmget( IPC_PRIVATE, DATA_SIZE(p_image), IPC_CREAT | 0776 );
    if( p_shm->shmid < 0 )
    {
        intf_ErrMsg( "vout error: cannot allocate shared image data (%s)",
                     strerror( errno ) );
        IMAGE_FREE( p_image );
        return( NULL );
    }

    /* Attach shared memory segment to process (read/write) */
    p_shm->shmaddr = p_image->data = shmat( p_shm->shmid, 0, 0 );
    if(! p_shm->shmaddr )
    {
        intf_ErrMsg( "vout error: cannot attach shared memory (%s)",
                    strerror(errno));
        IMAGE_FREE( p_image );
        shmctl( p_shm->shmid, IPC_RMID, 0 );
        return( NULL );
    }

    /* Read-only data. We won't be using XShmGetImage */
    p_shm->readOnly = True;

    /* Attach shared memory segment to X server */
    if( XShmAttach( p_display, p_shm ) == False )
    {
        intf_ErrMsg( "vout error: cannot attach shared memory to X server" );
        IMAGE_FREE( p_image );
        shmctl( p_shm->shmid, IPC_RMID, 0 );
        shmdt( p_shm->shmaddr );
        return( NULL );
    }

    /* Send image to X server. This instruction is required, since having
     * built a Shm XImage and not using it causes an error on XCloseDisplay,
     * and remember NOT to use XFlush ! */
    XSync( p_display, False );

#if 0
    /* Mark the shm segment to be removed when there are no more
     * attachements, so it is automatic on process exit or after shmdt */
    shmctl( p_shm->shmid, IPC_RMID, 0 );
#endif

    return( p_image );
}
#endif

/*****************************************************************************
 * CreateImage: create an XImage or XvImage
 *****************************************************************************
 * Create a simple image used as a buffer.
 *****************************************************************************/
static IMAGE_TYPE * CreateImage( Display *p_display, EXTRA_ARGS,
                                 int i_width, int i_height )
{
    byte_t *    p_data;                           /* image data storage zone */
    IMAGE_TYPE *p_image;
#ifdef MODULE_NAME_IS_x11
    int         i_quantum;                     /* XImage quantum (see below) */
    int         i_bytes_per_line;
#endif

    /* Allocate memory for image */
#ifdef MODULE_NAME_IS_xvideo
    p_data = (byte_t *) malloc( i_width * i_height * 2 ); /* XXX */
#else
    i_bytes_per_line = i_width * i_bytes_per_pixel;
    p_data = (byte_t *) malloc( i_bytes_per_line * i_height );
#endif
    if( !p_data )
    {
        intf_ErrMsg( "vout error: %s", strerror(ENOMEM));
        return( NULL );
    }

#ifdef MODULE_NAME_IS_x11
    /* Optimize the quantum of a scanline regarding its size - the quantum is
       a diviser of the number of bits between the start of two scanlines. */
    if( i_bytes_per_line & 0xf )
    {
        i_quantum = 0x8;
    }
    else if( i_bytes_per_line & 0x10 )
    {
        i_quantum = 0x10;
    }
    else
    {
        i_quantum = 0x20;
    }
#endif

    /* Create XImage. p_data will be automatically freed */
#ifdef MODULE_NAME_IS_xvideo
    p_image = XvCreateImage( p_display, i_xvport, i_chroma,
                             p_data, i_width, i_height );
#else
    p_image = XCreateImage( p_display, p_visual, i_depth, ZPixmap, 0,
                            p_data, i_width, i_height, i_quantum, 0 );
#endif
    if( p_image == NULL )
    {
        intf_ErrMsg( "vout error: XCreateImage() failed" );
        free( p_data );
        return( NULL );
    }

    return p_image;
}

