/*****************************************************************************
 * vout_x11.c: X11 video output display method
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: vout_x11.c,v 1.21 2001/04/27 19:29:11 massiot Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
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

#define MODULE_NAME x11
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

/*****************************************************************************
 * vout_sys_t: video output X11 method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the X11 specific properties of an output thread. X11 video
 * output is performed through regular resizable windows. Windows can be
 * dynamically resized to adapt to the size of the streams.
 *****************************************************************************/
typedef struct vout_sys_s
{
    /* User settings */
    boolean_t           b_shm;               /* shared memory extension flag */

    /* Internal settings and properties */
    Display *           p_display;                        /* display pointer */
    Visual *            p_visual;                          /* visual pointer */
    int                 i_screen;                           /* screen number */
    Window              window;                               /* root window */
    GC                  gc;              /* graphic context instance handler */
    Colormap            colormap;               /* colormap used (8bpp only) */

    /* Display buffers and shared memory information */
    XImage *            p_ximage[2];                       /* XImage pointer */
    XShmSegmentInfo     shm_info[2];       /* shared memory zone information */

    /* X11 generic properties */
    Atom                wm_protocols;
    Atom                wm_delete_window;

    int                 i_width;                     /* width of main window */
    int                 i_height;                   /* height of main window */

    /* Screen saver properties */
    int                 i_ss_timeout;                             /* timeout */
    int                 i_ss_interval;           /* interval between changes */
    int                 i_ss_blanking;                      /* blanking mode */
    int                 i_ss_exposure;                      /* exposure mode */

    /* Auto-hide cursor */
    mtime_t     i_lastmoved;
    
    /* Mouse pointer properties */
    boolean_t           b_mouse;         /* is the mouse pointer displayed ? */

    /* Displaying fullscreen */
    boolean_t           b_fullscreen;

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
static int  vout_Probe     ( probedata_t *p_data );
static int  vout_Create    ( struct vout_thread_s * );
static int  vout_Init      ( struct vout_thread_s * );
static void vout_End       ( struct vout_thread_s * );
static void vout_Destroy   ( struct vout_thread_s * );
static int  vout_Manage    ( struct vout_thread_s * );
static void vout_Display   ( struct vout_thread_s * );
static void vout_SetPalette( struct vout_thread_s *, u16*, u16*, u16*, u16* );

static int  X11CreateWindow     ( vout_thread_t *p_vout );
static int  X11InitDisplay      ( vout_thread_t *p_vout, char *psz_display );

static int  X11CreateImage      ( vout_thread_t *p_vout, XImage **pp_ximage );
static void X11DestroyImage     ( XImage *p_ximage );
static int  X11CreateShmImage   ( vout_thread_t *p_vout, XImage **pp_ximage,
                                  XShmSegmentInfo *p_shm_info );
static void X11DestroyShmImage  ( vout_thread_t *p_vout, XImage *p_ximage,
                                  XShmSegmentInfo *p_shm_info );

static void X11TogglePointer            ( vout_thread_t *p_vout );
static void X11EnableScreenSaver        ( vout_thread_t *p_vout );
static void X11DisableScreenSaver       ( vout_thread_t *p_vout );

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
 * This function tries to initialize SDL and returns a score to the
 * plugin manager so that it can select the best plugin.
 *****************************************************************************/
static int vout_Probe( probedata_t *p_data )
{
    if( TestMethod( VOUT_METHOD_VAR, "x11" ) )
    {
        return( 999 );
    }

    return( 50 );
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

    p_vout->p_sys->b_fullscreen
        = main_GetIntVariable( VOUT_FULLSCREEN_VAR, VOUT_FULLSCREEN_DEFAULT );

    /* Spawn base window - this window will include the video output window,
     * but also command buttons, subtitles and other indicators */

    if( X11CreateWindow( p_vout ) )
    {
        intf_ErrMsg( "vout error: cannot create X11 window" );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return( 1 );
    }

    /* Open and initialize device. This function issues its own error messages.
     * Since XLib is usually not thread-safe, we can't use the same display
     * pointer than the interface or another thread. However, the root window
     * id is still valid. */
    if( X11InitDisplay( p_vout, psz_display ) )
    {
        intf_ErrMsg( "vout error: cannot initialize X11 display" );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return( 1 );
    }

    p_vout->p_sys->b_mouse = 1;

    /* Disable screen saver and return */
    X11DisableScreenSaver( p_vout );

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize X11 video thread output method
 *****************************************************************************
 * This function create the XImages needed by the output thread. It is called
 * at the beginning of the thread, but also each time the window is resized.
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    int i_err;

#ifdef SYS_DARWIN1_3
    /* FIXME : As of 2001-03-16, XFree4 for MacOS X does not support Xshm. */
    p_vout->p_sys->b_shm = 0;
#endif

    /* Create XImages using XShm extension - on failure, fall back to regular
     * way (and destroy the first image if it was created successfully) */
    if( p_vout->p_sys->b_shm )
    {
        /* Create first image */
        i_err = X11CreateShmImage( p_vout, &p_vout->p_sys->p_ximage[0],
                                   &p_vout->p_sys->shm_info[0] );
        if( !i_err )                         /* first image has been created */
        {
            /* Create second image */
            if( X11CreateShmImage( p_vout, &p_vout->p_sys->p_ximage[1],
                                   &p_vout->p_sys->shm_info[1] ) )
            {                             /* error creating the second image */
                X11DestroyShmImage( p_vout, p_vout->p_sys->p_ximage[0],
                                    &p_vout->p_sys->shm_info[0] );
                i_err = 1;
            }
        }
        if( i_err )                                      /* an error occured */
        {
            intf_Msg( "vout: XShm video extension unavailable" );
            p_vout->p_sys->b_shm = 0;
        }
    }

    /* Create XImages without XShm extension */
    if( !p_vout->p_sys->b_shm )
    {
        if( X11CreateImage( p_vout, &p_vout->p_sys->p_ximage[0] ) )
        {
            intf_ErrMsg( "vout error: cannot create images" );
            p_vout->p_sys->p_ximage[0] = NULL;
            p_vout->p_sys->p_ximage[1] = NULL;
            return( 1 );
        }
        if( X11CreateImage( p_vout, &p_vout->p_sys->p_ximage[1] ) )
        {
            intf_ErrMsg( "vout error: cannot create images" );
            X11DestroyImage( p_vout->p_sys->p_ximage[0] );
            p_vout->p_sys->p_ximage[0] = NULL;
            p_vout->p_sys->p_ximage[1] = NULL;
            return( 1 );
        }
    }

    /* Set bytes per line and initialize buffers */
    p_vout->i_bytes_per_line = p_vout->p_sys->p_ximage[0]->bytes_per_line;
    vout_SetBuffers( p_vout, p_vout->p_sys->p_ximage[ 0 ]->data,
                     p_vout->p_sys->p_ximage[ 1 ]->data );

    /* Set date for autohiding cursor */
    p_vout->p_sys->i_lastmoved = mdate();
    
    return( 0 );
}

/*****************************************************************************
 * vout_End: terminate X11 video thread output method
 *****************************************************************************
 * Destroy the X11 XImages created by vout_Init. It is called at the end of
 * the thread, but also each time the window is resized.
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
    if( p_vout->p_sys->b_shm )                             /* Shm XImages... */
    {
        X11DestroyShmImage( p_vout, p_vout->p_sys->p_ximage[0],
                            &p_vout->p_sys->shm_info[0] );
        X11DestroyShmImage( p_vout, p_vout->p_sys->p_ximage[1],
                            &p_vout->p_sys->shm_info[1] );
    }
    else                                          /* ...or regular XImages */
    {
        X11DestroyImage( p_vout->p_sys->p_ximage[0] );
        X11DestroyImage( p_vout->p_sys->p_ximage[1] );
    }
}

/*****************************************************************************
 * vout_Destroy: destroy X11 video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_CreateOutputMethod
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    /* Enable screen saver */
    X11EnableScreenSaver( p_vout );

    /* Destroy colormap */
    if( p_vout->i_screen_depth == 8 )
    {
        XFreeColormap( p_vout->p_sys->p_display, p_vout->p_sys->colormap );
    }

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
 *****************************************************************************/
static int vout_Manage( vout_thread_t *p_vout )
{
    XEvent      xevent;                                         /* X11 event */
    boolean_t   b_resized;                        /* window has been resized */
    boolean_t   b_gofullscreen;                    /* user wants full-screen */
    char        i_key;                                    /* ISO Latin-1 key */
    KeySym      x_key_symbol;

    /* Handle X11 events: ConfigureNotify events are parsed to know if the
     * output window's size changed, MapNotify and UnmapNotify to know if the
     * window is mapped (and if the display is useful), and ClientMessages
     * to intercept window destruction requests */
    b_resized = 0;
    b_gofullscreen = 0;

    while( XCheckWindowEvent( p_vout->p_sys->p_display, p_vout->p_sys->window,
                              StructureNotifyMask | KeyPressMask |
                              ButtonPressMask | ButtonReleaseMask | 
                              PointerMotionMask | Button1MotionMask , &xevent )
           == True )
    {
        /* ConfigureNotify event: prepare  */
        if( (xevent.type == ConfigureNotify)
            && ((xevent.xconfigure.width != p_vout->p_sys->i_width)
                || (xevent.xconfigure.height != p_vout->p_sys->i_height)) )
        {
            /* Update dimensions */
            b_resized = 1;
            p_vout->p_sys->i_width = xevent.xconfigure.width;
            p_vout->p_sys->i_height = xevent.xconfigure.height;
        }
        /* MapNotify event: change window status and disable screen saver */
        else if( xevent.type == MapNotify)
        {
            if( (p_vout != NULL) && !p_vout->b_active )
            {
                X11DisableScreenSaver( p_vout );
                p_vout->b_active = 1;
            }
        }
        /* UnmapNotify event: change window status and enable screen saver */
        else if( xevent.type == UnmapNotify )
        {
            if( (p_vout != NULL) && p_vout->b_active )
            {
                X11EnableScreenSaver( p_vout );
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
                        /* FIXME: handle stuff here */
                        switch( i_key )
                        {
                        case 'q':
                        case 'Q':
                            p_main->p_intf->b_die = 1;
                            break;
                        case 'f':
                        case 'F':
                            b_gofullscreen = 1;
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
                               intf_DbgMsg( "vout: unhandled key '%c' (%i)", 
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
                X11TogglePointer( p_vout ); 
            }
        }
        /* Other event */
        else
        {
            intf_WarnMsg( 1, "vout: unhandled event %d received", xevent.type );
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
            intf_DbgMsg( "vout: unhandled ClientMessage received" );
        }
    }

    if ( b_gofullscreen )
    {
        char *psz_display;
        /* Open display, unsing 'vlc_display' or DISPLAY environment variable */
        psz_display = XDisplayName( main_GetPszVariable( VOUT_DISPLAY_VAR, NULL ) );

        intf_DbgMsg( "vout: changing full-screen status" );

        p_vout->p_sys->b_fullscreen = !p_vout->p_sys->b_fullscreen;

        /* Get rid of the old window */
        XUnmapWindow( p_vout->p_sys->p_display, p_vout->p_sys->window );
        XFreeGC( p_vout->p_sys->p_display, p_vout->p_sys->gc );

        /* And create a new one */
        if( X11CreateWindow( p_vout ) )
        {
            intf_ErrMsg( "vout error: cannot create X11 window" );
            XCloseDisplay( p_vout->p_sys->p_display );

            free( p_vout->p_sys );
            return( 1 );
        }

        if( X11InitDisplay( p_vout, psz_display ) )
        {
            intf_ErrMsg( "vout error: cannot initialize X11 display" );
            XCloseDisplay( p_vout->p_sys->p_display );
            free( p_vout->p_sys );
            return( 1 );
        }
        /* We've changed the size, update it */
        p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }

    /*
     * Handle vout window resizing
     */
    if( b_resized )
    {
        /* If interface window has been resized, change vout size */
        intf_DbgMsg( "vout: resizing output window" );
        p_vout->i_width =  p_vout->p_sys->i_width;
        p_vout->i_height = p_vout->p_sys->i_height;
        p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }
    else if( (p_vout->i_width  != p_vout->p_sys->i_width) ||
             (p_vout->i_height != p_vout->p_sys->i_height) )
    {
        /* If video output size has changed, change interface window size */
        intf_DbgMsg( "vout: resizing output window" );
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
        intf_DbgMsg( "vout info: resizing window" );
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

    /* Autohide Cursour */
    if( mdate() - p_vout->p_sys->i_lastmoved > 2000000 )
    {
        /* Hide the mouse automatically */
        if( p_vout->p_sys->b_mouse )
        {
            X11TogglePointer( p_vout ); 
        }
    }

    
    return 0;
}

/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to X11 server, wait until
 * it is displayed and switch the two rendering buffer, preparing next frame.
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout )
{
    if( p_vout->p_sys->b_shm)                                /* XShm is used */
    {
        /* Display rendered image using shared memory extension */
        XShmPutImage(p_vout->p_sys->p_display, p_vout->p_sys->window, p_vout->p_sys->gc,
                     p_vout->p_sys->p_ximage[ p_vout->i_buffer_index ],
                     0, 0, 0, 0,
                     p_vout->p_sys->p_ximage[ p_vout->i_buffer_index ]->width,
                     p_vout->p_sys->p_ximage[ p_vout->i_buffer_index ]->height, True);

        /* Send the order to the X server */
        XSync(p_vout->p_sys->p_display, False);
    }
    else                                /* regular X11 capabilities are used */
    {
        XPutImage(p_vout->p_sys->p_display, p_vout->p_sys->window, p_vout->p_sys->gc,
                  p_vout->p_sys->p_ximage[ p_vout->i_buffer_index ],
                  0, 0, 0, 0,
                  p_vout->p_sys->p_ximage[ p_vout->i_buffer_index ]->width,
                  p_vout->p_sys->p_ximage[ p_vout->i_buffer_index ]->height);

        /* Send the order to the X server */
        XSync(p_vout->p_sys->p_display, False);
    }
}

/*****************************************************************************
 * vout_SetPalette: sets an 8 bpp palette
 *****************************************************************************
 * This function sets the palette given as an argument. It does not return
 * anything, but could later send information on which colors it was unable
 * to set.
 *****************************************************************************/
static void vout_SetPalette( p_vout_thread_t p_vout,
                             u16 *red, u16 *green, u16 *blue, u16 *transp )
{
    int i, j;
    XColor p_colors[255];

    intf_DbgMsg( "vout: Palette change called" );

    /* allocate palette */
    for( i = 0, j = 255; i < 255; i++, j-- )
    {
        /* kludge: colors are indexed reversely because color 255 seems
         * to be reserved for black even if we try to set it to white */
        p_colors[ i ].pixel = j;
        p_colors[ i ].pad   = 0;
        p_colors[ i ].flags = DoRed | DoGreen | DoBlue;
        p_colors[ i ].red   = red[ j ];
        p_colors[ i ].blue  = blue[ j ];
        p_colors[ i ].green = green[ j ];
    }

    XStoreColors( p_vout->p_sys->p_display,
                  p_vout->p_sys->colormap, p_colors, 256 );
}

/* following functions are local */

/*****************************************************************************
 * X11CreateWindow: open and set-up X11 main window
 *****************************************************************************/
static int X11CreateWindow( vout_thread_t *p_vout )
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

    /* If we're full screen, we're full screen! */
    if( p_vout->p_sys->b_fullscreen ) 
    {
        p_vout->p_sys->i_width = DisplayWidth( p_vout->p_sys->p_display, 
                                               p_vout->p_sys->i_screen );
        p_vout->p_sys->i_height =  DisplayHeight( p_vout->p_sys->p_display, 
                                                  p_vout->p_sys->i_screen ); 
        p_vout->i_width =  p_vout->p_sys->i_width;
        p_vout->i_height = p_vout->p_sys->i_height;
    }
    else
    {
        /* Set main window's size */
        p_vout->p_sys->i_width =  main_GetIntVariable( VOUT_WIDTH_VAR,
                                                       VOUT_WIDTH_DEFAULT );
        p_vout->p_sys->i_height = main_GetIntVariable( VOUT_HEIGHT_VAR,
                                                       VOUT_HEIGHT_DEFAULT );
        p_vout->i_width =  p_vout->p_sys->i_width;
        p_vout->i_height = p_vout->p_sys->i_height;
    }

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
    xwindow_attributes.background_pixel = BlackPixel( p_vout->p_sys->p_display,
                                                      p_vout->p_sys->i_screen );
    xwindow_attributes.event_mask = ExposureMask | StructureNotifyMask;
    

    /* Create the window and set hints - the window must receive ConfigureNotify
     * events, and, until it is displayed, Expose and MapNotify events. */

    p_vout->p_sys->window =
        XCreateWindow( p_vout->p_sys->p_display,
                       DefaultRootWindow( p_vout->p_sys->p_display ),
                       0, 0,
                       p_vout->p_sys->i_width, p_vout->p_sys->i_height, 0,
                       0, InputOutput, 0,
                       CWBackingStore | CWBackPixel | CWEventMask,
                       &xwindow_attributes );

    if ( p_vout->p_sys->b_fullscreen )
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
                VOUT_TITLE " (X11 output)" );

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

    if( p_vout->p_sys->b_fullscreen )
    {
        XSetInputFocus( p_vout->p_sys->p_display, p_vout->p_sys->window,
                        RevertToNone, CurrentTime );
        XMoveWindow( p_vout->p_sys->p_display, p_vout->p_sys->window, 0, 0 );
    }

    /* At this stage, the window is open, displayed, and ready to
     * receive data */

    return( 0 );
}

/*****************************************************************************
 * X11InitDisplay: open and initialize X11 device
 *****************************************************************************
 * Create a window according to video output given size, and set other
 * properties according to the display properties.
 *****************************************************************************/
static int X11InitDisplay( vout_thread_t *p_vout, char *psz_display )
{
    XPixmapFormatValues *       p_formats;                 /* pixmap formats */
    XVisualInfo *               p_xvisual;           /* visuals informations */
    XVisualInfo                 xvisual_template;         /* visual template */
    int                         i_count;                       /* array size */



    /* Initialize structure */
    p_vout->p_sys->i_screen = DefaultScreen( p_vout->p_sys->p_display );
    p_vout->p_sys->b_shm    = ( XShmQueryExtension( p_vout->p_sys->p_display )
                                 == True );
    if( !p_vout->p_sys->b_shm )
    {
        intf_Msg( "vout: XShm video extension is not available" );
    }

    /* Get screen depth */
    p_vout->i_screen_depth = XDefaultDepth( p_vout->p_sys->p_display,
                                            p_vout->p_sys->i_screen );
    switch( p_vout->i_screen_depth )
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
        p_vout->i_bytes_per_pixel = 1;
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
        p_vout->i_red_mask =        p_xvisual->red_mask;
        p_vout->i_green_mask =      p_xvisual->green_mask;
        p_vout->i_blue_mask =       p_xvisual->blue_mask;

        /* There is no difference yet between 3 and 4 Bpp. The only way
         * to find the actual number of bytes per pixel is to list supported
         * pixmap formats. */
        p_formats = XListPixmapFormats( p_vout->p_sys->p_display, &i_count );
        p_vout->i_bytes_per_pixel = 0;

        for( ; i_count-- ; p_formats++ )
        {
            /* Under XFree4.0, the list contains pixmap formats available
             * through all video depths ; so we have to check against current
             * depth. */
            if( p_formats->depth == p_vout->i_screen_depth )
            {
                if( p_formats->bits_per_pixel / 8
                        > p_vout->i_bytes_per_pixel )
                {
                    p_vout->i_bytes_per_pixel = p_formats->bits_per_pixel / 8;
                }
            }
        }
        break;
    }
    p_vout->p_sys->p_visual = p_xvisual->visual;
    XFree( p_xvisual );

    return( 0 );
}

/*****************************************************************************
 * X11CreateImage: create an XImage
 *****************************************************************************
 * Create a simple XImage used as a buffer.
 *****************************************************************************/
static int X11CreateImage( vout_thread_t *p_vout, XImage **pp_ximage )
{
    byte_t *    pb_data;                          /* image data storage zone */
    int         i_quantum;                     /* XImage quantum (see below) */

    /* Allocate memory for image */
    p_vout->i_bytes_per_line = p_vout->i_width * p_vout->i_bytes_per_pixel;
    pb_data = (byte_t *) malloc( p_vout->i_bytes_per_line * p_vout->i_height );
    if( !pb_data )                                                  /* error */
    {
        intf_ErrMsg( "vout error: %s", strerror(ENOMEM));
        return( 1 );
    }

    /* Optimize the quantum of a scanline regarding its size - the quantum is
       a diviser of the number of bits between the start of two scanlines. */
    if( !(( p_vout->i_bytes_per_line ) % 32) )
    {
        i_quantum = 32;
    }
    else
    {
        if( !(( p_vout->i_bytes_per_line ) % 16) )
        {
            i_quantum = 16;
        }
        else
        {
            i_quantum = 8;
        }
    }

    /* Create XImage */
    *pp_ximage = XCreateImage( p_vout->p_sys->p_display,
                               p_vout->p_sys->p_visual, p_vout->i_screen_depth,
                               ZPixmap, 0, pb_data,
                               p_vout->i_width, p_vout->i_height, i_quantum, 0);
    if(! *pp_ximage )                                               /* error */
    {
        intf_ErrMsg( "vout error: XCreateImage() failed" );
        free( pb_data );
        return( 1 );
    }

    return 0;
}

/*****************************************************************************
 * X11CreateShmImage: create an XImage using shared memory extension
 *****************************************************************************
 * Prepare an XImage for DisplayX11ShmImage function.
 * The order of the operations respects the recommandations of the mit-shm
 * document by J.Corbet and K.Packard. Most of the parameters were copied from
 * there.
 *****************************************************************************/
static int X11CreateShmImage( vout_thread_t *p_vout, XImage **pp_ximage,
                              XShmSegmentInfo *p_shm_info)
{
    /* Create XImage */
    *pp_ximage =
        XShmCreateImage( p_vout->p_sys->p_display, p_vout->p_sys->p_visual,
                         p_vout->i_screen_depth, ZPixmap, 0,
                         p_shm_info, p_vout->i_width, p_vout->i_height );
    if(! *pp_ximage )                                               /* error */
    {
        intf_ErrMsg( "vout error: XShmCreateImage() failed" );
        return( 1 );
    }

    /* Allocate shared memory segment - 0777 set the access permission
     * rights (like umask), they are not yet supported by X servers */
    p_shm_info->shmid =
        shmget( IPC_PRIVATE, (*pp_ximage)->bytes_per_line
                                 * (*pp_ximage)->height, IPC_CREAT | 0777);
    if( p_shm_info->shmid < 0)                                      /* error */
    {
        intf_ErrMsg( "vout error: cannot allocate shared image data (%s)",
                    strerror(errno));
        XDestroyImage( *pp_ximage );
        return( 1 );
    }

    /* Attach shared memory segment to process (read/write) */
    p_shm_info->shmaddr = (*pp_ximage)->data = shmat(p_shm_info->shmid, 0, 0);
    if(! p_shm_info->shmaddr )
    {                                                               /* error */
        intf_ErrMsg( "vout error: cannot attach shared memory (%s)",
                    strerror(errno));
        shmctl( p_shm_info->shmid, IPC_RMID, 0 );      /* free shared memory */
        XDestroyImage( *pp_ximage );
        return( 1 );
    }

    /* Mark the shm segment to be removed when there will be no more
     * attachements, so it is automatic on process exit or after shmdt */
    shmctl( p_shm_info->shmid, IPC_RMID, 0 );

    /* Attach shared memory segment to X server (read only) */
    p_shm_info->readOnly = True;
    if( XShmAttach( p_vout->p_sys->p_display, p_shm_info )
         == False )                                                 /* error */
    {
        intf_ErrMsg( "vout error: cannot attach shared memory to X11 server" );
        shmdt( p_shm_info->shmaddr );   /* detach shared memory from process
                                         * and automatic free */
        XDestroyImage( *pp_ximage );
        return( 1 );
    }

    /* Send image to X server. This instruction is required, since having
     * built a Shm XImage and not using it causes an error on XCloseDisplay */
    XFlush( p_vout->p_sys->p_display );
    return( 0 );
}

/*****************************************************************************
 * X11DestroyImage: destroy an XImage
 *****************************************************************************
 * Destroy XImage AND associated data. If pointer is NULL, the image won't be
 * destroyed (see vout_ManageOutputMethod())
 *****************************************************************************/
static void X11DestroyImage( XImage *p_ximage )
{
    if( p_ximage != NULL )
    {
        XDestroyImage( p_ximage );                     /* no free() required */
    }
}

/*****************************************************************************
 * X11DestroyShmImage
 *****************************************************************************
 * Destroy XImage AND associated data. Detach shared memory segment from
 * server and process, then free it. If pointer is NULL, the image won't be
 * destroyed (see vout_ManageOutputMethod())
 *****************************************************************************/
static void X11DestroyShmImage( vout_thread_t *p_vout, XImage *p_ximage,
                                XShmSegmentInfo *p_shm_info )
{
    /* If pointer is NULL, do nothing */
    if( p_ximage == NULL )
    {
        return;
    }

    XShmDetach( p_vout->p_sys->p_display, p_shm_info );/* detach from server */
    XDestroyImage( p_ximage );

    if( shmdt( p_shm_info->shmaddr ) )  /* detach shared memory from process */
    {                                   /* also automatic freeing...         */
        intf_ErrMsg( "vout error: cannot detach shared memory (%s)",
                     strerror(errno) );
    }
}


/* WAZAAAAAAAAAAA */

/*****************************************************************************
 * X11EnableScreenSaver: enable screen saver
 *****************************************************************************
 * This function enable the screen saver on a display after it had been
 * disabled by XDisableScreenSaver. Both functions use a counter mechanism to
 * know wether the screen saver can be activated or not: if n successive calls
 * are made to XDisableScreenSaver, n successive calls to XEnableScreenSaver
 * will be required before the screen saver could effectively be activated.
 *****************************************************************************/
void X11EnableScreenSaver( vout_thread_t *p_vout )
{
    intf_DbgMsg( "vout: enabling screen saver" );
    XSetScreenSaver( p_vout->p_sys->p_display, p_vout->p_sys->i_ss_timeout,
                     p_vout->p_sys->i_ss_interval,
                     p_vout->p_sys->i_ss_blanking,
                     p_vout->p_sys->i_ss_exposure );
}

/*****************************************************************************
 * X11DisableScreenSaver: disable screen saver
 *****************************************************************************
 * See XEnableScreenSaver
 *****************************************************************************/
void X11DisableScreenSaver( vout_thread_t *p_vout )
{
    /* Save screen saver informations */
    XGetScreenSaver( p_vout->p_sys->p_display, &p_vout->p_sys->i_ss_timeout,
                     &p_vout->p_sys->i_ss_interval,
                     &p_vout->p_sys->i_ss_blanking,
                     &p_vout->p_sys->i_ss_exposure );

    /* Disable screen saver */
    intf_DbgMsg( "vout: disabling screen saver" );
    XSetScreenSaver( p_vout->p_sys->p_display, 0,
                     p_vout->p_sys->i_ss_interval,
                     p_vout->p_sys->i_ss_blanking,
                     p_vout->p_sys->i_ss_exposure );
}

/*****************************************************************************
 * X11TogglePointer: hide or show the mouse pointer
 *****************************************************************************
 * This function hides the X pointer if it is visible by putting it at
 * coordinates (32,32) and setting the pointer sprite to a blank one. To
 * show it again, we disable the sprite and restore the original coordinates.
 *****************************************************************************/
void X11TogglePointer( vout_thread_t *p_vout )
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

