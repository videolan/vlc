/*****************************************************************************
 * vout_xvideo.c: Xvideo video output display method
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: vout_xvideo.c,v 1.36.2.3 2001/12/20 16:46:40 massiot Exp $
 *
 * Authors: Shane Harper <shanegh@optusnet.com.au>
 *          Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          David Kennedy <dkennedy@tinytoad.com>
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

#ifndef WIN32
#include <netinet/in.h>                               /* BSD: struct in_addr */
#endif

#include <sys/shm.h>                                   /* shmget(), shmctl() */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include <X11/extensions/dpms.h>

#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "video.h"
#include "video_output.h"

#include "interface.h"

#include "netutils.h"                                 /* network_ChannelJoin */

#include "stream_control.h"                 /* needed by input_ext-intf.h... */
#include "input_ext-intf.h"

#include "modules.h"
#include "modules_export.h"

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
    Window              yuv_window;   /* sub-window for displaying yuv video
                                                                        data */
    GC                  yuv_gc;
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
    
    /* Mouse pointer properties */
    boolean_t           b_mouse_pointer_visible;
    mtime_t             i_time_mouse_last_moved; /* used to auto-hide pointer*/
    Cursor              blank_cursor;                   /* the hidden cursor */
    Pixmap              cursor_pixmap;

} vout_sys_t;

/* Fullscreen needs to be able to hide the wm decorations */
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
static void XVideoDestroyWindow      ( vout_thread_t *p_vout );
static int  XVideoUpdateImgSizeIfRequired( vout_thread_t *p_vout );
static int  XVideoCreateShmImage     ( Display* dpy, int xv_port,
                                       XvImage **pp_xvimage,
                                       XShmSegmentInfo *p_shm_info,
                                       int i_width, int i_height );
static void XVideoDestroyShmImage    ( vout_thread_t *, XvImage *,
                                       XShmSegmentInfo * );
static void X11ToggleMousePointer    ( vout_thread_t * );
static void XVideoEnableScreenSaver  ( vout_thread_t * );
static void XVideoDisableScreenSaver ( vout_thread_t * );
/*static void XVideoSetAttribute       ( vout_thread_t *, char *, float );*/

static int  XVideoCheckForXv         ( Display * );
static int  XVideoGetPort            ( Display * );
static void XVideoOutputCoords       ( const picture_t *, const boolean_t,
                                       const int, const int,
                                       int *, int *, int *, int * );
static void XVideoDisplay            ( vout_thread_t * );

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
    Display *p_display;                                   /* display pointer */
    char    *psz_display;

    /* Open display, unsing 'vlc_display' or DISPLAY environment variable */
    psz_display = XDisplayName( main_GetPszVariable(VOUT_DISPLAY_VAR, NULL) );
    p_display = XOpenDisplay( psz_display );
    if( p_display == NULL )                                         /* error */
    {
        intf_WarnMsg( 3, "vout: Xvideo cannot open display %s", psz_display );
        intf_WarnMsg( 3, "vout: Xvideo not supported" );
        return( 0 );
    }
    
    if( !XVideoCheckForXv( p_display ) )
    {
        intf_WarnMsg( 3, "vout: Xvideo not supported" );
        XCloseDisplay( p_display );
        return( 0 );
    }

    if( XVideoGetPort( p_display ) < 0 )
    {
        intf_WarnMsg( 3, "vout: Xvideo not supported" );
        XCloseDisplay( p_display );
        return( 0 );
    }

    /* Clean-up everyting */
    XCloseDisplay( p_display );

    if( TestMethod( VOUT_METHOD_VAR, "xvideo" ) )
    {
        return( 999 );
    }

    return( 150 );
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
    XColor cursor_color;

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

    p_vout->b_fullscreen
        = main_GetIntVariable( VOUT_FULLSCREEN_VAR, VOUT_FULLSCREEN_DEFAULT );
    
    if( !XVideoCheckForXv( p_vout->p_sys->p_display ) )
    {
        intf_ErrMsg( "vout error: no XVideo extension" );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return( 1 );
    }

    /* Check we have access to a video port */
    if( (p_vout->p_sys->xv_port = XVideoGetPort(p_vout->p_sys->p_display)) <0 )
    {
        intf_ErrMsg( "vout error: cannot get XVideo port" );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return 1;
    }
    intf_DbgMsg( "Using xv port %d" , p_vout->p_sys->xv_port );

    /* Create blank cursor (for mouse cursor autohiding) */
    p_vout->p_sys->b_mouse_pointer_visible = 1;
    p_vout->p_sys->cursor_pixmap = XCreatePixmap( p_vout->p_sys->p_display,
                                                  DefaultRootWindow(
                                                     p_vout->p_sys->p_display),
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
    
    p_vout->p_sys->blank_cursor = XCreatePixmapCursor(
                                      p_vout->p_sys->p_display,
                                      p_vout->p_sys->cursor_pixmap,
                                      p_vout->p_sys->cursor_pixmap,
                                      &cursor_color,
                                      &cursor_color, 1, 1 );    

    /* Spawn base window - this window will include the video output window,
     * but also command buttons, subtitles and other indicators */
    if( XVideoCreateWindow( p_vout ) )
    {
        intf_ErrMsg( "vout error: cannot create XVideo window" );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return( 1 );
    }

    /* p_vout->pf_setbuffers( p_vout, NULL, NULL ); */

#if 0
    /* XXX The brightness and contrast values should be read from environment
     * XXX variables... */
    XVideoSetAttribute( p_vout, "XV_BRIGHTNESS", 0.5 );
    XVideoSetAttribute( p_vout, "XV_CONTRAST",   0.5 );
#endif

    /* Disable screen saver and return */
    XVideoDisableScreenSaver( p_vout );

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize XVideo video thread output method
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
#ifdef SYS_DARWIN
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
 * Terminate an output method created by vout_Create
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    /* Restore cursor if it was blanked */
    if( !p_vout->p_sys->b_mouse_pointer_visible )
        X11ToggleMousePointer( p_vout );

    /* Destroy blank cursor pixmap */
    XFreePixmap( p_vout->p_sys->p_display, p_vout->p_sys->cursor_pixmap );

    XVideoEnableScreenSaver( p_vout );
    XVideoDestroyWindow( p_vout );
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
static __inline__ void vout_Seek( off_t i_seek )
{
    int i_tell = p_main->p_intf->p_input->stream.p_selected_area->i_tell;

    i_tell += i_seek * 50 * p_main->p_intf->p_input->stream.i_mux_rate;

    if( i_tell < p_main->p_intf->p_input->stream.p_selected_area->i_start )
    {
        i_tell = p_main->p_intf->p_input->stream.p_selected_area->i_start;
    }
    else if( i_tell > p_main->p_intf->p_input->stream.p_selected_area->i_size )
    {
        i_tell = p_main->p_intf->p_input->stream.p_selected_area->i_size;
    }

    input_Seek( p_main->p_intf->p_input, i_tell );
}

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
//            p_vout->i_changes |= VOUT_SIZE_CHANGE;
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
                     input_Seek( p_main->p_intf->p_input,
                     p_main->p_intf->p_input->stream.p_selected_area->i_start );                     break;
                 case XK_End:
                     input_Seek( p_main->p_intf->p_input,
                     p_main->p_intf->p_input->stream.p_selected_area->i_size );
                     break;
                 case XK_Page_Up:
                     vout_Seek( 900 );
                     break;
                 case XK_Page_Down:
                     vout_Seek( -900 );
                     break;
                 case XK_space:
                     input_SetStatus( p_main->p_intf->p_input,
                                      INPUT_STATUS_PAUSE );
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
                        case 'f':
                        case 'F':
                            p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
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
                    /* In this part we will eventually manage
                     * clicks for DVD navigation for instance. For the
                     * moment just pause the stream. */
                    input_SetStatus( p_main->p_intf->p_input,
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
            if( !p_vout->p_sys->b_mouse_pointer_visible )
                X11ToggleMousePointer( p_vout ); 
        }
        /* Other event */
        else
        {
            intf_WarnMsg( 3, "%p -> unhandled event type %d received",
                         p_vout, xevent.type );
        }
    }

    /* Handle events for YUV video output sub-window */
    while( XCheckWindowEvent( p_vout->p_sys->p_display,
                              p_vout->p_sys->yuv_window,
                              ExposureMask, &xevent ) == True )
    {
        /* Window exposed (only handled if stream playback is paused) */
        if( xevent.type == Expose )
        {
            if( ((XExposeEvent *)&xevent)->count == 0 )
                /* (if this is the last a collection of expose events...) */
                if( p_main->p_intf->p_input )
                    if( PAUSE_S ==
                            p_main->p_intf->p_input->stream.control.i_status )
                        XVideoDisplay( p_vout );
        }
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

    if ( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        intf_DbgMsg( "vout: changing full-screen status" );

        p_vout->b_fullscreen = !p_vout->b_fullscreen;

        /* Get rid of the old window */
        XVideoDestroyWindow( p_vout );

        /* And create a new one */
        if( XVideoCreateWindow( p_vout ) )
        {
            intf_ErrMsg( "vout error: cannot create X11 window" );
            XCloseDisplay( p_vout->p_sys->p_display );

            free( p_vout->p_sys );
            return( 1 );
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

        p_vout->i_width = p_vout->p_sys->i_window_width;
        p_vout->i_height = p_vout->p_sys->i_window_height;

        intf_WarnMsg( 3, "vout: video display resized (%dx%d)",
                      p_vout->i_width, p_vout->i_height );
    }

    /* Autohide Cursor */
    if( p_vout->p_sys->b_mouse_pointer_visible &&
        mdate() - p_vout->p_sys->i_time_mouse_last_moved > 2000000 )
    {
        X11ToggleMousePointer( p_vout );
    }
    
    return 0;
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
    {
        return;
    }

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
        p_main->fast_memcpy( p_vout->p_sys->p_xvimage->data,
                             p_vout->p_rendered_pic->p_y, i_size );
        p_main->fast_memcpy( p_vout->p_sys->p_xvimage->data + ( i_size ),
                             p_vout->p_rendered_pic->p_v, i_size / 4 );
        p_main->fast_memcpy( p_vout->p_sys->p_xvimage->data
                                 + ( i_size ) + ( i_size / 4 ),
                             p_vout->p_rendered_pic->p_u, i_size / 4 );
        break;
    }

    if( b_draw )
    {
        XVideoDisplay( p_vout );
    }
}

static void vout_SetPalette( p_vout_thread_t p_vout,
                             u16 *red, u16 *green, u16 *blue, u16 *transp )
{
    return;
}

/* following functions are local */

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
        if( p_vout->p_sys->i_image_width != 0
             && p_vout->p_sys->i_image_height != 0 )
        {
            /* Destroy XvImage to change its size */
            vout_End( p_vout );
        }

        p_vout->p_sys->i_image_width  = i_img_width;
        p_vout->p_sys->i_image_height = i_img_height;

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

    }

    return( 0 );
}

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
            intf_WarnMsg( 3, "vout error: XvBadExtension" );
            return( 0 );

        case XvBadAlloc:
            intf_WarnMsg( 3, "vout error: XvBadAlloc" );
            return( 0 );

        default:
            intf_WarnMsg( 3, "vout error: XvQueryExtension failed" );
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
    Atom                    prop;
    mwmhints_t              mwmhints;
    
    boolean_t               b_expose;
    boolean_t               b_configure_notify;
    boolean_t               b_map_notify;


    /* Set main window's size */
    /* If we're full screen, we're full screen! */
    if( p_vout->b_fullscreen )
    {
        p_vout->p_sys->i_window_width = DisplayWidth( p_vout->p_sys->p_display,
                                                      p_vout->p_sys->i_screen );
        p_vout->p_sys->i_window_height =  DisplayHeight( p_vout->p_sys->p_display,
                                                         p_vout->p_sys->i_screen );
    }
    else
    {
        p_vout->p_sys->i_window_width =  p_vout->i_width;
        p_vout->p_sys->i_window_height = p_vout->i_height;
    }

    /* Prepare window manager hints and properties */
    xsize_hints.base_width          = p_vout->p_sys->i_window_width;
    xsize_hints.base_height         = p_vout->p_sys->i_window_height;
    xsize_hints.flags               = PSize;
    p_vout->p_sys->wm_protocols     = XInternAtom( p_vout->p_sys->p_display,
                                                   "WM_PROTOCOLS", True );
    p_vout->p_sys->wm_delete_window = XInternAtom( p_vout->p_sys->p_display,
                                                   "WM_DELETE_WINDOW", True );

    /* Prepare window attributes */
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
                           CWBackPixel | CWEventMask,
                           &xwindow_attributes );

    if ( p_vout->b_fullscreen )
    {
        prop = XInternAtom(p_vout->p_sys->p_display, "_MOTIF_WM_HINTS", False);
        mwmhints.flags = MWM_HINTS_DECORATIONS;
        mwmhints.decorations = 0;
        XChangeProperty( p_vout->p_sys->p_display, p_vout->p_sys->window,
                         prop, prop, 32, PropModeReplace,
                         (unsigned char *)&mwmhints, PROP_MWM_HINTS_ELEMENTS );

        XSetTransientForHint( p_vout->p_sys->p_display,
                              p_vout->p_sys->window, None );
        XRaiseWindow( p_vout->p_sys->p_display, p_vout->p_sys->window );
    }

    
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

    if( p_vout->b_fullscreen )
    {
        XSetInputFocus( p_vout->p_sys->p_display, p_vout->p_sys->window,
                        RevertToNone, CurrentTime );
        XMoveWindow( p_vout->p_sys->p_display, p_vout->p_sys->window, 0, 0 );
    }

    /* Create YUV output sub-window. */
    p_vout->p_sys->yuv_window=XCreateSimpleWindow( p_vout->p_sys->p_display,
                         p_vout->p_sys->window, 0, 0, 1, 1, 0,
                         BlackPixel( p_vout->p_sys->p_display,
                                         p_vout->p_sys->i_screen ),
                         WhitePixel( p_vout->p_sys->p_display,
                                         p_vout->p_sys->i_screen ) );
 
    p_vout->p_sys->yuv_gc = XCreateGC( p_vout->p_sys->p_display,
                                        p_vout->p_sys->yuv_window,
                                        GCGraphicsExposures, &xgcvalues );

    XSetWindowBackground( p_vout->p_sys->p_display, p_vout->p_sys->yuv_window,
             BlackPixel(p_vout->p_sys->p_display, p_vout->p_sys->i_screen ) );

    XMapWindow( p_vout->p_sys->p_display, p_vout->p_sys->yuv_window );
    XSelectInput( p_vout->p_sys->p_display, p_vout->p_sys->yuv_window,
                  ExposureMask );

    /* If the cursor was formerly blank than blank it again */
    if( !p_vout->p_sys->b_mouse_pointer_visible )
    {
        X11ToggleMousePointer( p_vout );
        X11ToggleMousePointer( p_vout );
    }

    XSync( p_vout->p_sys->p_display, False );

    return( 0 );
}

static void XVideoDestroyWindow( vout_thread_t *p_vout )
{
    XSync( p_vout->p_sys->p_display, False );

    XFreeGC( p_vout->p_sys->p_display, p_vout->p_sys->yuv_gc );
    XDestroyWindow( p_vout->p_sys->p_display, p_vout->p_sys->yuv_window );

    XUnmapWindow( p_vout->p_sys->p_display, p_vout->p_sys->window );
    XFreeGC( p_vout->p_sys->p_display, p_vout->p_sys->gc );
    XDestroyWindow( p_vout->p_sys->p_display, p_vout->p_sys->window );
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

    if( !XShmAttach( dpy, p_shm_info ) )
    {
        intf_ErrMsg( "vout error: XShmAttach failed" );
        shmctl( p_shm_info->shmid, IPC_RMID, 0 );
        shmdt( p_shm_info->shmaddr );
        return( -1 );
    }

    /* Send image to X server. This instruction is required, since having
     * built a Shm XImage and not using it causes an error on XCloseDisplay */
    XSync( dpy, False );

    /* Mark the shm segment to be removed when there will be no more
     * attachements, so it is automatic on process exit or after shmdt */
    shmctl( p_shm_info->shmid, IPC_RMID, 0 );

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

    XSync( p_vout->p_sys->p_display, False );
    XShmDetach( p_vout->p_sys->p_display, p_shm_info );/* detach from server */
#if 0
    XDestroyImage( p_ximage ); /* XXX */
#endif
    XFree( p_xvimage );

    if( shmdt( p_shm_info->shmaddr ) )  /* detach shared memory from process */
    {
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

    DPMSEnable( p_vout->p_sys->p_display );
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

    DPMSDisable( p_vout->p_sys->p_display );
}

/*****************************************************************************
 * X11ToggleMousePointer: hide or show the mouse pointer
 *****************************************************************************
 * This function hides the X pointer if requested.
 *****************************************************************************/
void X11ToggleMousePointer( vout_thread_t *p_vout )
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

/* This based on some code in SetBufferPicture... At the moment it's only
 * used by the xvideo plugin, but others may want to use it. */
static void XVideoOutputCoords( const picture_t *p_pic, const boolean_t scale,
                                const int win_w, const int win_h,
                                int *dx, int *dy, int *w, int *h )
{
}


/*****************************************************************************
 * XVideoGetPort: get YUV12 port
 *****************************************************************************
 * 
 *****************************************************************************/
static int XVideoGetPort( Display *dpy )
{
    XvAdaptorInfo *p_adaptor;
    int i_adaptor, i_num_adaptors, i_requested_adaptor;
    int i_selected_port;

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
    i_requested_adaptor = main_GetIntVariable( VOUT_XVADAPTOR_VAR, -1 );

    /* No special xv port has been requested so try all of them */
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

        /* Check that port supports YUV12 planar format... */
        p_formats = XvListImageFormats( dpy, p_adaptor[i_adaptor].base_id,
                                        &i_num_formats );

        for( i_format = 0; i_format < i_num_formats; i_format++ )
        {
            XvEncodingInfo  *p_enc;
            int             i_enc, i_num_encodings;
            XvAttribute     *p_attr;
            int             i_attr, i_num_attributes;

            /* If this is not the format we want, forget it */
            if( p_formats[ i_format ].id != GUID_YUV12_PLANAR )
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
                }
            }

            /* If no free port was found, forget it */
            if( i_selected_port == -1 )
            {
                continue;
            }

            /* If we found a port, print information about it */
            intf_WarnMsg( 3, "vout: GetXVideoPort found adaptor %i, port %i",
                             i_adaptor, i_selected_port );
            intf_WarnMsg( 3, "  image format 0x%x (%4.4s) %s supported",
                             p_formats[ i_format ].id,
                             (char *)&p_formats[ i_format ].id,
                             ( p_formats[ i_format ].format
                                == XvPacked ) ? "packed" : "planar" );

            intf_WarnMsg( 4, " encoding list:" );

            if( XvQueryEncodings( dpy, i_selected_port,
                                  &i_num_encodings, &p_enc )
                 != Success )
            {
                intf_WarnMsg( 4, "  XvQueryEncodings failed" );
                continue;
            }

            for( i_enc = 0; i_enc < i_num_encodings; i_enc++ )
            {
                intf_WarnMsg( 4, "  id=%ld, name=%s, size=%ldx%ld,"
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

            intf_WarnMsg( 4, " attribute list:" );
            p_attr = XvQueryPortAttributes( dpy, i_selected_port,
                                            &i_num_attributes );
            for( i_attr = 0; i_attr < i_num_attributes; i_attr++ )
            {
                intf_WarnMsg( 4,
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
            intf_WarnMsg( 3, "vout: no free XVideo port found for YV12" );
        }
        else
        {
            intf_WarnMsg( 3, "vout: XVideo adaptor %i does not have a free "
                             "XVideo port for YV12", i_requested_adaptor );
        }
    }

    return( i_selected_port );
}


/*****************************************************************************
 * XVideoDisplay: display image
 *****************************************************************************
 * This function displays the image stored in p_vout->p_sys->p_xvimage.
 * The image is scaled to fit in the output window (and to have the correct
 * aspect ratio).
 *****************************************************************************/
static void XVideoDisplay( vout_thread_t *p_vout )
{
    int         i_dest_width, i_dest_height;
    int         i_dest_x, i_dest_y;

    if( !p_vout->p_sys->p_xvimage || !p_vout->p_rendered_pic )
    {
        return;
    }

    i_dest_height = p_vout->p_sys->i_window_height >
                        p_vout->p_rendered_pic->i_height
                  ? p_vout->p_sys->i_window_height
                  : p_vout->p_rendered_pic->i_height;
    i_dest_width = p_vout->p_sys->i_window_width >
                        p_vout->p_rendered_pic->i_width
                 ? p_vout->p_sys->i_window_width
                 : p_vout->p_rendered_pic->i_width;
        
    if( p_vout->b_scale )
    {
        int   i_ratio = 900 * i_dest_width / i_dest_height;
        
        switch( p_vout->p_rendered_pic->i_aspect_ratio )
        {
            case AR_3_4_PICTURE:
                if( i_ratio < 1200 )
                {
                    i_dest_width = i_dest_height * 4 / 3;
                }
                else
                {
                    i_dest_height = i_dest_width * 3 / 4;
                }
                i_ratio = 1200;
                break;

            case AR_16_9_PICTURE:
                if( i_ratio < 1600 )
                {
                    i_dest_width = i_dest_height * 16 / 9;
                }
                else
                {
                    i_dest_height = i_dest_width * 9 / 16;
                }
                i_ratio = 1600;
                break;

            case AR_221_1_PICTURE:
                if( i_ratio < 1989 )
                {
                    i_dest_width = i_dest_height * 221 / 100;
                }
                else
                {
                    i_dest_height = i_dest_width * 100 / 221;
                }
                i_ratio = 1989;
                break;

            case AR_SQUARE_PICTURE:
            default:
                if( i_ratio < 900 )
                {
                    i_dest_width = i_dest_height * p_vout->p_rendered_pic->i_width / p_vout->p_rendered_pic->i_height;
                }
                else
                {
                    i_dest_height = i_dest_width * p_vout->p_rendered_pic->i_height / p_vout->p_rendered_pic->i_width;
                }
                i_ratio = 900;
                break;
        }

        if( i_dest_width >
            DisplayWidth( p_vout->p_sys->p_display, p_vout->p_sys->i_screen ) )
        {
            i_dest_width = DisplayWidth( p_vout->p_sys->p_display,
                                         p_vout->p_sys->i_screen );
            i_dest_height = 900 * i_dest_width / i_ratio;
        }
        else if( i_dest_height >
            DisplayHeight( p_vout->p_sys->p_display, p_vout->p_sys->i_screen ) )
        {
            i_dest_height = DisplayHeight( p_vout->p_sys->p_display,
                                           p_vout->p_sys->i_screen );
            i_dest_width = i_ratio * i_dest_height / 900;
        }
    }

    XvShmPutImage( p_vout->p_sys->p_display, p_vout->p_sys->xv_port,
                   p_vout->p_sys->yuv_window, p_vout->p_sys->gc,
                   p_vout->p_sys->p_xvimage,
                   0 /*src_x*/, 0 /*src_y*/,
                   p_vout->p_rendered_pic->i_width,
                   p_vout->p_rendered_pic->i_height,
                   0 /*dest_x*/, 0 /*dest_y*/, i_dest_width, i_dest_height,
                   False );
    
    /* YUV window */
    XResizeWindow( p_vout->p_sys->p_display, p_vout->p_sys->yuv_window,
                   i_dest_width, i_dest_height );

    /* Root window */
    if( ( ( i_dest_width != p_vout->p_sys->i_window_width ) ||
          ( i_dest_height != p_vout->p_sys->i_window_height ) ) &&
        ! p_vout->b_fullscreen )
    {
        p_vout->p_sys->i_window_width = i_dest_width;
        p_vout->p_sys->i_window_height = i_dest_height;
//        p_vout->i_changes |= VOUT_SIZE_CHANGE;
        XResizeWindow( p_vout->p_sys->p_display, p_vout->p_sys->window,
                       i_dest_width, i_dest_height );
    }
    
    /* Set picture position */
    i_dest_x = (p_vout->p_sys->i_window_width - i_dest_width) / 2;
    i_dest_y = (p_vout->p_sys->i_window_height - i_dest_height) / 2;
    
    XMoveWindow( p_vout->p_sys->p_display, p_vout->p_sys->yuv_window,
                 i_dest_x, i_dest_y );
    
    /* Send the order to the X server */
    XSync( p_vout->p_sys->p_display, False );
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

