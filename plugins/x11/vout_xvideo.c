/*****************************************************************************
 * vout_xvideo.c: Xvideo video output display method
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000, 2001 VideoLAN
 * $Id: vout_xvideo.c,v 1.7 2001/04/15 04:46:41 sam Exp $
 *
 * Authors: Shane Harper <shanegh@optusnet.com.au>
 *          Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
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

#define MODULE_NAME xvideo
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#ifdef HAVE_MACHINE_PARAM_H
/* BSD */
#include <machine/param.h>
#include <sys/types.h>                                     /* typedef ushort */
#include <sys/ipc.h>
#endif

#include <sys/shm.h>                                   /* shmget(), shmctl() */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"
#include "modules.h"

#include "video.h"
#include "video_output.h"

#include "interface.h"
#include "intf_msg.h"

#include "netutils.h"                                 /* network_ChannelJoin */

#include "main.h"


#define GUID_YUV12_PLANAR 0x32315659


/*****************************************************************************
 * vout_sys_t: video output X11 method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the XVideo specific properties of an output thread.
 *****************************************************************************/
typedef struct vout_sys_s
{
    /* User settings */
#if 0
    /* this plugin (currently) requires the SHM Ext... */
    boolean_t           b_shm;               /* shared memory extension flag */
#endif

    /* Internal settings and properties */
    Display *           p_display;                        /* display pointer */
    int                 i_screen;                           /* screen number */
    Window              window;                               /* root window */
    GC                  gc;              /* graphic context instance handler */
    int                 xv_port;

    /* Display buffers and shared memory information */
    /* Note: only 1 buffer... Xv ext does double buffering. */
    XvImage *           p_xvimage;
    int                 i_image_width;
    int                 i_image_height;
                                /* i_image_width & i_image_height reflect the
                                 * size of the XvImage. They are used by
                                 * vout_Display() to check if the image to be
                                 * displayed can use the current XvImage. */
    XShmSegmentInfo     shm_info;       /* shared memory zone information */

    /* X11 generic properties */
    Atom                wm_protocols;
    Atom                wm_delete_window;

    int                 i_window_width;              /* width of main window */
    int                 i_window_height;            /* height of main window */


    /* Screen saver properties */
    int                 i_ss_timeout;                             /* timeout */
    int                 i_ss_interval;           /* interval between changes */
    int                 i_ss_blanking;                      /* blanking mode */
    int                 i_ss_exposure;                      /* exposure mode */
    
    /* Auto-hide cursor */
    mtime_t     i_lastmoved;
    
    /* Mouse pointer properties */
    boolean_t           b_mouse;         /* is the mouse pointer displayed ? */

} vout_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  vout_Probe     ( probedata_t * );
static int  vout_Create    ( vout_thread_t * );
static int  vout_Init      ( vout_thread_t * );
static void vout_End       ( vout_thread_t * );
static void vout_Destroy   ( vout_thread_t * );
static int  vout_Manage    ( vout_thread_t * );
static void vout_Display   ( vout_thread_t * );
static void vout_SetPalette( vout_thread_t *, u16 *, u16 *, u16 *, u16 * );

static int  XVideoCreateWindow       ( vout_thread_t * );
static int  XVideoUpdateImgSizeIfRequired( vout_thread_t *p_vout );
static int  XVideoCreateShmImage     ( Display* dpy, int xv_port,
                                       XvImage **pp_xvimage,
                                       XShmSegmentInfo *p_shm_info,
                                       int i_width, int i_height );
static void XVideoDestroyShmImage    ( vout_thread_t *, XvImage *,
                                       XShmSegmentInfo * );
static void XVideoTogglePointer      ( vout_thread_t * );
static void XVideoEnableScreenSaver  ( vout_thread_t * );
static void XVideoDisableScreenSaver ( vout_thread_t * );
/*static void XVideoSetAttribute       ( vout_thread_t *, char *, float );*/

static int  XVideoCheckForXv         ( Display * );
static int  XVideoGetPort            ( Display * );
static void XVideoOutputCoords       ( const picture_t *, const boolean_t,
                                       const int, const int,
                                       int *, int *, int *, int * );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( vout_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = vout_Probe;
    p_function_list->functions.vout.pf_create     = vout_Create;
    p_function_list->functions.vout.pf_init       = vout_Init;
    p_function_list->functions.vout.pf_end        = vout_End;
    p_function_list->functions.vout.pf_destroy    = vout_Destroy;
    p_function_list->functions.vout.pf_manage     = vout_Manage;
    p_function_list->functions.vout.pf_display    = vout_Display;
    p_function_list->functions.vout.pf_setpalette = vout_SetPalette;
}

/*****************************************************************************
 * vout_Probe: probe the video driver and return a score
 *****************************************************************************
 * This returns a score to the plugin manager so that it can select the best
 * plugin.
 *****************************************************************************/
static int vout_Probe( probedata_t *p_data )
{
    if( TestMethod( VOUT_METHOD_VAR, "xvideo" ) )
    {
        return( 999 );
    }

    return( 90 );
}

/*****************************************************************************
 * vout_Create: allocate XVideo video thread output method
 *****************************************************************************
 * This function allocate and initialize a XVideo vout method. It uses some of
 * the vout properties to choose the window size, and change them according to
 * the actual properties of the display.
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

    /* Open display, unsing 'vlc_display' or DISPLAY environment variable */
    psz_display = XDisplayName( main_GetPszVariable( VOUT_DISPLAY_VAR, NULL ) );
    p_vout->p_sys->p_display = XOpenDisplay( psz_display );

    if( p_vout->p_sys->p_display == NULL )                          /* error */
    {
        intf_ErrMsg( "vout error: cannot open display %s", psz_display );
        free( p_vout->p_sys );
        return( 1 );
    }
    p_vout->p_sys->i_screen = DefaultScreen( p_vout->p_sys->p_display );

    if( !XVideoCheckForXv( p_vout->p_sys->p_display ) )
    {
        intf_ErrMsg( "vout error: no XVideo extension" );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return( 1 );
    }

    /* Spawn base window - this window will include the video output window,
     * but also command buttons, subtitles and other indicators */
    if( XVideoCreateWindow( p_vout ) )
    {
        intf_ErrMsg( "vout error: cannot create XVideo window" );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return( 1 );
    }

    if( (p_vout->p_sys->xv_port = XVideoGetPort( p_vout->p_sys->p_display ))<0 )
        return 1;
    intf_DbgMsg( 1, "Using xv port %d" , p_vout->p_sys->xv_port );

#if 0
    /* XXX The brightness and contrast values should be read from environment
     * XXX variables... */
    XVideoSetAttribute( p_vout, "XV_BRIGHTNESS", 0.5 );
    XVideoSetAttribute( p_vout, "XV_CONTRAST",   0.5 );
#endif

    p_vout->p_sys->b_mouse = 1;

    /* Disable screen saver and return */
    XVideoDisableScreenSaver( p_vout );

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize XVideo video thread output method
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
#ifdef SYS_DARWIN1_3
    /* FIXME : As of 2001-03-16, XFree4 for MacOS X does not support Xshm. */
    p_vout->p_sys->b_shm = 0;
#endif
    p_vout->b_need_render = 0;
    p_vout->p_sys->i_image_width = p_vout->p_sys->i_image_height = 0;

    return( 0 );
}

/*****************************************************************************
 * vout_End: terminate XVideo video thread output method
 *****************************************************************************
 * Destroy the XvImage. It is called at the end of the thread, but also each
 * time the image is resized.
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
    XVideoDestroyShmImage( p_vout, p_vout->p_sys->p_xvimage,
                           &p_vout->p_sys->shm_info );
}

/*****************************************************************************
 * vout_Destroy: destroy XVideo video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_CreateOutputMethod
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    /* Enable screen saver */
    XVideoEnableScreenSaver( p_vout );

    /* Destroy window */
    XUnmapWindow( p_vout->p_sys->p_display, p_vout->p_sys->window );
    XFreeGC( p_vout->p_sys->p_display, p_vout->p_sys->gc );
    XDestroyWindow( p_vout->p_sys->p_display, p_vout->p_sys->window );

    XCloseDisplay( p_vout->p_sys->p_display );

    /* Destroy structure */
    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_Manage: handle X11 events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * X11 events and allows window resizing. It returns a non null value on
 * error.
 *
 * XXX  Should "factor-out" common code in this and the "same" fn in the x11
 * XXX  plugin!
 *****************************************************************************/
static int vout_Manage( vout_thread_t *p_vout )
{
    XEvent      xevent;                                         /* X11 event */
    char        i_key;                                    /* ISO Latin-1 key */
    KeySym      x_key_symbol;

    /* Handle X11 events: ConfigureNotify events are parsed to know if the
     * output window's size changed, MapNotify and UnmapNotify to know if the
     * window is mapped (and if the display is useful), and ClientMessages
     * to intercept window destruction requests */
    while( XCheckWindowEvent( p_vout->p_sys->p_display, p_vout->p_sys->window,
                              StructureNotifyMask | KeyPressMask |
                              ButtonPressMask | ButtonReleaseMask | 
                              PointerMotionMask, &xevent )
           == True )
    {
        /* ConfigureNotify event: prepare  */
        if( (xevent.type == ConfigureNotify)
            /*&& ((xevent.xconfigure.width != p_vout->p_sys->i_window_width)
                || (xevent.xconfigure.height != p_vout->p_sys->i_window_height))*/ )
        {
            /* Update dimensions */
            p_vout->p_sys->i_window_width = xevent.xconfigure.width;
            p_vout->p_sys->i_window_height = xevent.xconfigure.height;
        }
        /* MapNotify event: change window status and disable screen saver */
        else if( xevent.type == MapNotify)
        {
            if( (p_vout != NULL) && !p_vout->b_active )
            {
                XVideoDisableScreenSaver( p_vout );
                p_vout->b_active = 1;
            }
        }
        /* UnmapNotify event: change window status and enable screen saver */
        else if( xevent.type == UnmapNotify )
        {
            if( (p_vout != NULL) && p_vout->b_active )
            {
                XVideoEnableScreenSaver( p_vout );
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
                 default:
                     /* "Normal Keys"
                      * The reason why I use this instead of XK_0 is that 
                      * with XLookupString, we don't have to care about
                      * keymaps. */

                    if( XLookupString( &xevent.xkey, &i_key, 1, NULL, NULL ) )
                    {
                        switch( i_key )
                        {
                        case 'q':
                        case 'Q':
                            p_main->p_intf->b_die = 1;
                            break;
                        case '0':
                            network_ChannelJoin( 0 );
                            break;
                        case '1':
                            network_ChannelJoin( 1 );
                            break;
                        case '2':
                            network_ChannelJoin( 2 );
                            break;
                        case '3':
                            network_ChannelJoin( 3 );
                            break;
                        case '4':
                            network_ChannelJoin( 4 );
                            break;
                        case '5':
                            network_ChannelJoin( 5 );
                            break;
                        case '6':
                            network_ChannelJoin( 6 );
                            break;
                        case '7':
                            network_ChannelJoin( 7 );
                            break;
                        case '8':
                            network_ChannelJoin( 8 );
                            break;
                        case '9':
                            network_ChannelJoin( 9 );
                            break;
                        default:
                            if( intf_ProcessKey( p_main->p_intf, 
                                                 (char )i_key ) )
                            {
                               intf_DbgMsg( "unhandled key '%c' (%i)", 
                                            (char)i_key, i_key );
                            }
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
                    /* in this part we will eventually manage
                     * clicks for DVD navigation for instance */
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
            p_vout->p_sys->i_lastmoved = mdate();
            if( ! p_vout->p_sys->b_mouse )
            {
                XVideoTogglePointer( p_vout ); 
            }
        }
        
#ifdef DEBUG
        /* Other event */
        else
        {
            intf_DbgMsg( "%p -> unhandled event type %d received",
                         p_vout, xevent.type );
        }
#endif
    }

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
        else
        {
            intf_DbgMsg( "%p -> unhandled ClientMessage received", p_vout );
        }
    }

    if( (p_vout->i_changes & VOUT_GRAYSCALE_CHANGE))
    {
        /* FIXME: clear flags ?? */
    }

    /*
     * Size change
     */
    if( p_vout->i_changes & VOUT_SIZE_CHANGE )
    {
        intf_DbgMsg( "vout: resizing window" );
        p_vout->i_changes &= ~VOUT_SIZE_CHANGE;
        /* Noting to do here...
         * vout_Display() detects size changes of the image to be displayed and
         * re-creates the XvImage.*/
        intf_Msg( "vout: video display resized (%dx%d)",
                  p_vout->i_width, p_vout->i_height );
    }

    /* Autohide Cursor */
    if( p_vout->p_sys->b_mouse &&
        mdate() - p_vout->p_sys->i_lastmoved > 2000000 )
            XVideoTogglePointer( p_vout ); 
    
    return 0;
}

/*****************************************************************************
 * XVideoUpdateImgSizeIfRequired 
 *****************************************************************************
 * This function checks to see if the image to be displayed is of a different
 * size to the last image displayed. If so, the old shm block must be
 * destroyed and a new one created.
 * Note: the "image size" is the size of the image to be passed to the Xv
 * extension (which is probably different to the size of the output window).
 *****************************************************************************/
static int XVideoUpdateImgSizeIfRequired( vout_thread_t *p_vout )
{
    int i_img_width         = p_vout->p_rendered_pic->i_width;
    int i_img_height        = p_vout->p_rendered_pic->i_height;

    if( p_vout->p_sys->i_image_width != i_img_width
            || p_vout->p_sys->i_image_height != i_img_height )
    {
        p_vout->p_sys->i_image_width  = i_img_width;
        p_vout->p_sys->i_image_height = i_img_height;

        /* Destroy XvImage to change its size */
        vout_End( p_vout );
            /* Note: vout_End does nothing if no XvImage to destroy. */

        /* Create XvImage using XShm extension */
        if( XVideoCreateShmImage( p_vout->p_sys->p_display,
                                  p_vout->p_sys->xv_port,
                                  &p_vout->p_sys->p_xvimage,
                                  &p_vout->p_sys->shm_info,
                                  i_img_width, i_img_height ) )
        {
            intf_ErrMsg( "vout: failed to create xvimage." );
            p_vout->p_sys->i_image_width = 0;
            return( 1 );
        }

        /* Set bytes per line and initialize buffers */
        p_vout->i_bytes_per_line =
            (p_vout->p_sys->p_xvimage->data_size) /
            (p_vout->p_sys->p_xvimage->height);

        /* vout_SetBuffers( p_vout, p_vout->p_sys->p_xvimage->data ); */
    }
    return( 0 );
}

/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function sends the currently rendered image to X11 server.
 * (The Xv extension takes care of "double-buffering".)
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout )
{
    boolean_t b_draw = 1;
    int i_size = p_vout->p_rendered_pic->i_width *
                   p_vout->p_rendered_pic->i_height;

    if( XVideoUpdateImgSizeIfRequired( p_vout ) )
        return;

    switch( p_vout->p_rendered_pic->i_type )
    {
    case YUV_422_PICTURE:
        intf_ErrMsg( "vout error: YUV_422_PICTURE not (yet) supported" );
        b_draw = 0;
        break;

    case YUV_444_PICTURE:
        intf_ErrMsg( "vout error: YUV_444_PICTURE not (yet) supported" );
        b_draw = 0;
        break;

    case YUV_420_PICTURE:
        memcpy( p_vout->p_sys->p_xvimage->data,
                p_vout->p_rendered_pic->p_y, i_size );
        memcpy( p_vout->p_sys->p_xvimage->data + ( i_size ),
                p_vout->p_rendered_pic->p_v, i_size / 4 );
        memcpy( p_vout->p_sys->p_xvimage->data + ( i_size ) + ( i_size / 4 ),
                p_vout->p_rendered_pic->p_u, i_size / 4 );
        break;
    }

    if( b_draw )
    {
        int     i_dummy,
                i_window_width = p_vout->p_sys->i_window_width,
                i_window_height = p_vout->p_sys->i_window_height,
                i_dest_width, i_dest_height, i_dest_x, i_dest_y;
        Window  window;

#if 1
        /* If I change the line above to "#if 0" I find on resizing the window
         * that blue rectangles (used to specify where part of the YUV overlay
         * used to be drawn) may remain around the edge of the video output. */
        XGetGeometry( p_vout->p_sys->p_display, p_vout->p_sys->window,
                      &window, &i_dummy, &i_dummy,
                      &i_window_width, &i_window_height, &i_dummy, &i_dummy );
#endif

        XVideoOutputCoords( p_vout->p_rendered_pic, p_vout->b_scale,
                            i_window_width, i_window_height,
                            &i_dest_x, &i_dest_y,
                            &i_dest_width, &i_dest_height);
  
        XvShmPutImage( p_vout->p_sys->p_display, p_vout->p_sys->xv_port,
                       p_vout->p_sys->window, p_vout->p_sys->gc,
                       p_vout->p_sys->p_xvimage,
                       0 /*src_x*/, 0 /*src_y*/,
                       p_vout->p_rendered_pic->i_width,
                       p_vout->p_rendered_pic->i_height,
                       i_dest_x, i_dest_y, i_dest_width, i_dest_height,
                       True );
    }
}

static void vout_SetPalette( p_vout_thread_t p_vout,
                             u16 *red, u16 *green, u16 *blue, u16 *transp )
{
    return;
}

/* following functions are local */

/*****************************************************************************
 * XVideoCheckForXv: check for the XVideo extension
 *****************************************************************************/
static int XVideoCheckForXv( Display *dpy )
{
    unsigned int i;

    switch( XvQueryExtension( dpy, &i, &i, &i, &i, &i ) )
    {
        case Success:
            return( 1 );

        case XvBadExtension:
            intf_ErrMsg( "vout error: XvBadExtension" );
            return( 0 );

        case XvBadAlloc:
            intf_ErrMsg( "vout error: XvBadAlloc" );
            return( 0 );

        default:
            intf_ErrMsg( "vout error: XvQueryExtension failed" );
            return( 0 );
    }
}

/*****************************************************************************
 * XVideoCreateWindow: open and set-up XVideo main window
 *****************************************************************************/
static int XVideoCreateWindow( vout_thread_t *p_vout )
{
    XSizeHints              xsize_hints;
    XSetWindowAttributes    xwindow_attributes;
    XGCValues               xgcvalues;
    XEvent                  xevent;
    boolean_t               b_expose;
    boolean_t               b_configure_notify;
    boolean_t               b_map_notify;

    /* Set main window's size */
    p_vout->p_sys->i_window_width =  main_GetIntVariable( VOUT_WIDTH_VAR,
                                                   VOUT_WIDTH_DEFAULT );
    p_vout->p_sys->i_window_height = main_GetIntVariable( VOUT_HEIGHT_VAR,
                                                   VOUT_HEIGHT_DEFAULT );

    /* Prepare window manager hints and properties */
    xsize_hints.base_width          = p_vout->p_sys->i_window_width;
    xsize_hints.base_height         = p_vout->p_sys->i_window_height;
    xsize_hints.flags               = PSize;
    p_vout->p_sys->wm_protocols     = XInternAtom( p_vout->p_sys->p_display,
                                                   "WM_PROTOCOLS", True );
    p_vout->p_sys->wm_delete_window = XInternAtom( p_vout->p_sys->p_display,
                                                   "WM_DELETE_WINDOW", True );

    /* Prepare window attributes */
    xwindow_attributes.backing_store = Always;       /* save the hidden part */
    xwindow_attributes.background_pixel = BlackPixel( p_vout->p_sys->p_display,
                                                      p_vout->p_sys->i_screen );

    xwindow_attributes.event_mask = ExposureMask | StructureNotifyMask;

    /* Create the window and set hints - the window must receive ConfigureNotify
     * events, and, until it is displayed, Expose and MapNotify events. */
    p_vout->p_sys->window =
            XCreateWindow( p_vout->p_sys->p_display,
                           DefaultRootWindow( p_vout->p_sys->p_display ),
                           0, 0,
                           p_vout->p_sys->i_window_width,
                           p_vout->p_sys->i_window_height, 1,
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
                VOUT_TITLE " (XVideo output)" );

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
            p_vout->p_sys->i_window_width = xevent.xconfigure.width;
            p_vout->p_sys->i_window_height = xevent.xconfigure.height;
        }
    } while( !( b_expose && b_configure_notify && b_map_notify ) );

    XSelectInput( p_vout->p_sys->p_display, p_vout->p_sys->window,
                  StructureNotifyMask | KeyPressMask |
                  ButtonPressMask | ButtonReleaseMask | 
                  PointerMotionMask );

    /* At this stage, the window is open, displayed, and ready to
     * receive data */
    return( 0 );
}

/*****************************************************************************
 * XVideoCreateShmImage: create an XvImage using shared memory extension
 *****************************************************************************
 * Prepare an XvImage for display function.
 * The order of the operations respects the recommandations of the mit-shm
 * document by J.Corbet and K.Packard. Most of the parameters were copied from
 * there.
 *****************************************************************************/
static int XVideoCreateShmImage( Display* dpy, int xv_port,
                                    XvImage **pp_xvimage,
                                    XShmSegmentInfo *p_shm_info,
                                    int i_width, int i_height )
{
    *pp_xvimage = XvShmCreateImage( dpy, xv_port,
                                    GUID_YUV12_PLANAR, 0,
                                    i_width, i_height,
                                    p_shm_info );
    if( !(*pp_xvimage) )
    {
        intf_ErrMsg( "vout error: XvShmCreateImage failed." );
        return( -1 );
    }

    p_shm_info->shmid    = shmget( IPC_PRIVATE, (*pp_xvimage)->data_size,
                                   IPC_CREAT | 0777 );
    if( p_shm_info->shmid < 0)                                      /* error */
    {
        intf_ErrMsg( "vout error: cannot allocate shared image data (%s)",
                    strerror(errno));
        return( 1 );
    }

    p_shm_info->shmaddr  = (*pp_xvimage)->data = shmat( p_shm_info->shmid,
                                                        0, 0 );
    p_shm_info->readOnly = False;

    /* Mark the shm segment to be removed when there will be no more
     * attachements, so it is automatic on process exit or after shmdt */
    shmctl( p_shm_info->shmid, IPC_RMID, 0 );

    if( !XShmAttach( dpy, p_shm_info ) )
    {
        intf_ErrMsg( "vout error: XShmAttach failed" );
        shmdt( p_shm_info->shmaddr );
        return( -1 );
    }

    /* Send image to X server. This instruction is required, since having
     * built a Shm XImage and not using it causes an error on XCloseDisplay */
    XFlush( dpy );

    return( 0 );
}

/*****************************************************************************
 * XVideoDestroyShmImage
 *****************************************************************************
 * Destroy XImage AND associated data. Detach shared memory segment from
 * server and process, then free it. If pointer is NULL, the image won't be
 * destroyed (see vout_ManageOutputMethod())
 *****************************************************************************/
static void XVideoDestroyShmImage( vout_thread_t *p_vout, XvImage *p_xvimage,
                                   XShmSegmentInfo *p_shm_info )
{
    /* If pointer is NULL, do nothing */
    if( p_xvimage == NULL )
    {
        return;
    }

    XShmDetach( p_vout->p_sys->p_display, p_shm_info );/* detach from server */
#if 0
    XDestroyImage( p_ximage ); /* XXX */
#endif

    if( shmdt( p_shm_info->shmaddr ) )  /* detach shared memory from process */
    {                                   /* also automatic freeing...         */
        intf_ErrMsg( "vout error: cannot detach shared memory (%s)",
                     strerror(errno) );
    }
}

/*****************************************************************************
 * XVideoEnableScreenSaver: enable screen saver
 *****************************************************************************
 * This function enable the screen saver on a display after it had been
 * disabled by XDisableScreenSaver. Both functions use a counter mechanism to
 * know wether the screen saver can be activated or not: if n successive calls
 * are made to XDisableScreenSaver, n successive calls to XEnableScreenSaver
 * will be required before the screen saver could effectively be activated.
 *****************************************************************************/
void XVideoEnableScreenSaver( vout_thread_t *p_vout )
{
    intf_DbgMsg( "intf: enabling screen saver" );
    XSetScreenSaver( p_vout->p_sys->p_display, p_vout->p_sys->i_ss_timeout,
                     p_vout->p_sys->i_ss_interval,
                     p_vout->p_sys->i_ss_blanking,
                     p_vout->p_sys->i_ss_exposure );
}

/*****************************************************************************
 * XVideoDisableScreenSaver: disable screen saver
 *****************************************************************************
 * See XEnableScreenSaver
 *****************************************************************************/
void XVideoDisableScreenSaver( vout_thread_t *p_vout )
{
    /* Save screen saver informations */
    XGetScreenSaver( p_vout->p_sys->p_display, &p_vout->p_sys->i_ss_timeout,
                     &p_vout->p_sys->i_ss_interval,
                     &p_vout->p_sys->i_ss_blanking,
                     &p_vout->p_sys->i_ss_exposure );

    /* Disable screen saver */
    intf_DbgMsg( "intf: disabling screen saver" );
    XSetScreenSaver( p_vout->p_sys->p_display, 0,
                     p_vout->p_sys->i_ss_interval,
                     p_vout->p_sys->i_ss_blanking,
                     p_vout->p_sys->i_ss_exposure );
}

/*****************************************************************************
 * XVideoTogglePointer: hide or show the mouse pointer
 *****************************************************************************
 * This function hides the X pointer if it is visible by putting it at
 * coordinates (32,32) and setting the pointer sprite to a blank one. To
 * show it again, we disable the sprite and restore the original coordinates.
 *****************************************************************************/
void XVideoTogglePointer( vout_thread_t *p_vout )
{
    static Cursor cursor;
    static boolean_t b_cursor = 0;

    if( p_vout->p_sys->b_mouse )
    {
        p_vout->p_sys->b_mouse = 0;

        if( !b_cursor )
        {
            XColor color;
            Pixmap blank = XCreatePixmap( p_vout->p_sys->p_display,
                               DefaultRootWindow(p_vout->p_sys->p_display),
                               1, 1, 1 );

            XParseColor( p_vout->p_sys->p_display,
                         XCreateColormap( p_vout->p_sys->p_display,
                                          DefaultRootWindow(
                                                  p_vout->p_sys->p_display ),
                                          DefaultVisual(
                                                  p_vout->p_sys->p_display,
                                                  p_vout->p_sys->i_screen ),
                                          AllocNone ),
                         "black", &color );

            cursor = XCreatePixmapCursor( p_vout->p_sys->p_display,
                           blank, blank, &color, &color, 1, 1 );

            b_cursor = 1;
        }
        XDefineCursor( p_vout->p_sys->p_display,
                       p_vout->p_sys->window, cursor );
    }
    else
    {
        p_vout->p_sys->b_mouse = 1;

        XUndefineCursor( p_vout->p_sys->p_display, p_vout->p_sys->window );
    }
}

/* This based on some code in SetBufferPicture... At the moment it's only
 * used by the xvideo plugin, but others may want to use it. */
static void XVideoOutputCoords( const picture_t *p_pic, const boolean_t scale,
                                const int win_w, const int win_h,
                                int *dx, int *dy, int *w, int *h)
{
    if( !scale )
    {
        *w = p_pic->i_width; *h = p_pic->i_height;
    }
    else
    {
        *w = win_w;
        switch( p_pic->i_aspect_ratio )
        {
        case AR_3_4_PICTURE:        *h = win_w * 3 / 4;      break;
        case AR_16_9_PICTURE:       *h = win_w * 9 / 16;     break;
        case AR_221_1_PICTURE:      *h = win_w * 100 / 221;  break;
        case AR_SQUARE_PICTURE:
                default:            *h = win_w; break;
        }

        if( *h > win_h )
        {
            *h = win_h;
            switch( p_pic->i_aspect_ratio )
            {
            case AR_3_4_PICTURE:    *w = win_h * 4 / 3;      break;
            case AR_16_9_PICTURE:   *w = win_h * 16 / 9;     break;
            case AR_221_1_PICTURE:  *w = win_h * 221 / 100;  break;
            case AR_SQUARE_PICTURE:
                    default:        *w = win_h; break;
            }
        }
    }

    /* Set picture position */
    *dx = (win_w - *w) / 2;
    *dy = (win_h - *h) / 2;
}


static int XVideoGetPort( Display *dpy )
{
    int            i, i_adaptors;
    int            xv_port = -1;
    XvAdaptorInfo *adaptor_info;

    switch( XvQueryAdaptors( dpy, DefaultRootWindow( dpy ),
                             &i_adaptors, &adaptor_info ) )
    {
        case Success:
            break;

        case XvBadExtension:
            intf_ErrMsg( "vout error: XvBadExtension for XvQueryAdaptors" );
            return( -1 );

        case XvBadAlloc:
            intf_ErrMsg( "vout error: XvBadAlloc for XvQueryAdaptors" );
            return( -1 );

        default:
            intf_ErrMsg( "vout error: XvQueryAdaptors failed" );
            return( -1 );
    }

    for( i=0; i < i_adaptors && xv_port == -1; ++i )
        if( ( adaptor_info[ i ].type & XvInputMask ) &&
            ( adaptor_info[ i ].type & XvImageMask ) )
        {
            /* check that port supports YUV12 planar format... */
            int port = adaptor_info[ i ].base_id;
            int i_num_formats, i;
            XvImageFormatValues *imageFormats;

            imageFormats = XvListImageFormats( dpy, port, &i_num_formats );

            for( i=0; i < i_num_formats && xv_port == -1; ++i )
                if( imageFormats[ i ].id == GUID_YUV12_PLANAR )
                    xv_port = port;

            if( xv_port == -1 )
                intf_WarnMsg( 3, "vout: XVideo image input port %d "
                        "does not support the YUV12 planar format which is "
                        "currently required by the xvideo output plugin.",
                        port );

            if( imageFormats )
                XFree( imageFormats );
        }

    if( i_adaptors > 0 )
        XvFreeAdaptorInfo(adaptor_info);

    if( xv_port == -1 )
        intf_ErrMsg( "vout error: didn't find a suitable Xvideo image input port." );

    return( xv_port );
}


#if 0
/*****************************************************************************
 * XVideoSetAttribute
 *****************************************************************************
 * This function can be used to set attributes, e.g. XV_BRIGHTNESS and
 * XV_CONTRAST. "f_value" should be in the range of 0 to 1.
 *****************************************************************************/
static void XVideoSetAttribute( vout_thread_t *p_vout,
                                char *attr_name, float f_value )
{
    int             i_attrib;
    XvAttribute    *p_attrib;
    Display        *p_dpy   = p_vout->p_sys->p_display;
    int             xv_port = p_vout->p_sys->xv_port;

    p_attrib = XvQueryPortAttributes( p_dpy, xv_port, &i_attrib );

    do
    {
        i_attrib--;

        if( i_attrib >= 0 && !strcmp( p_attrib[ i_attrib ].name, attr_name ) )
        {
            int i_sv = f_value * ( p_attrib[ i_attrib ].max_value
                                    - p_attrib[ i_attrib ].min_value + 1 )
                        + p_attrib[ i_attrib ].min_value;

            XvSetPortAttribute( p_dpy, xv_port,
                            XInternAtom( p_dpy, attr_name, False ), i_sv );
            break;
        }

    } while( i_attrib > 0 );

    if( p_attrib )
        XFree( p_attrib );
}
#endif
