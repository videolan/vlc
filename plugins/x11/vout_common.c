/*****************************************************************************
 * vout_common.c: Functions common to the X11 and XVideo plugins
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: vout_common.c,v 1.3 2001/12/13 12:47:17 sam Exp $
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

#define x11 12
#define xvideo 42
#if ( MODULE_NAME == x11 )
#   define MODULE_NAME_IS_x11 1
#elif ( MODULE_NAME == xvideo )
#   define MODULE_NAME_IS_xvideo 1
#   include <X11/extensions/Xv.h>
#   include <X11/extensions/Xvlib.h>
#   include <X11/extensions/dpms.h>
#endif
#undef x11
#undef xvideo

#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "video.h"
#include "video_output.h"
#include "vout_common.h"

#include "interface.h"
#include "netutils.h"                                 /* network_ChannelJoin */

#include "stream_control.h"                 /* needed by input_ext-intf.h... */
#include "input_ext-intf.h"

#include "modules.h"
#include "modules_export.h"

/*****************************************************************************
 * vout_Manage: handle X11 events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * X11 events and allows window resizing. It returns a non null value on
 * error.
 *****************************************************************************/
static __inline__ void vout_Seek( int i_seek )
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

int _M( vout_Manage ) ( vout_thread_t *p_vout )
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
                _M( XCommonDisableScreenSaver ) ( p_vout );
                p_vout->b_active = 1;
            }
        }
        /* UnmapNotify event: change window status and enable screen saver */
        else if( xevent.type == UnmapNotify )
        {
            if( (p_vout != NULL) && p_vout->b_active )
            {
                _M( XCommonEnableScreenSaver ) ( p_vout );
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
                     p_main->p_intf->p_input->stream.p_selected_area->i_start );
                     break;
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
                            intf_DbgMsg( "vout: unhandled key '%c' (%i)", 
                                         (char)i_key, i_key );
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
                _M( XCommonToggleMousePointer ) ( p_vout ); 
            }
        }
        /* Other event */
        else
        {
            intf_WarnMsg( 3, "vout: unhandled event %d received", xevent.type );
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
            {
                /* (if this is the last a collection of expose events...) */
                if( p_main->p_intf->p_input != NULL )
                {
                    if( PAUSE_S ==
                            p_main->p_intf->p_input->stream.control.i_status )
                    {
/*                        XVideoDisplay( p_vout )*/;
                    }
                }
            }
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

    if ( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;

        p_vout->b_fullscreen = !p_vout->b_fullscreen;

        /* Get rid of the old window */
        _M( XCommonDestroyWindow ) ( p_vout );

        /* And create a new one */
        if( _M( XCommonCreateWindow ) ( p_vout ) )
        {
            intf_ErrMsg( "vout error: cannot create X11 window" );
            XCloseDisplay( p_vout->p_sys->p_display );

            free( p_vout->p_sys );
            return( 1 );
        }

    }

#ifdef MODULE_NAME_IS_x11
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
#else
    /*
     * Size change
     */
    if( p_vout->i_changes & VOUT_SIZE_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_SIZE_CHANGE;

        intf_WarnMsg( 3, "vout: video display resized (%dx%d)",
                      p_vout->p_sys->i_width,
                      p_vout->p_sys->i_height );
    }
#endif

    /* Autohide Cursour */
    if( mdate() - p_vout->p_sys->i_time_mouse_last_moved > 2000000 )
    {
        /* Hide the mouse automatically */
        if( p_vout->p_sys->b_mouse_pointer_visible )
        {
            _M( XCommonToggleMousePointer ) ( p_vout ); 
        }
    }

    return 0;
}

/*****************************************************************************
 * XCommonCreateWindow: open and set-up X11 main window
 *****************************************************************************/
int _M( XCommonCreateWindow ) ( vout_thread_t *p_vout )
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
    if( p_vout->b_fullscreen ) 
    {
        p_vout->p_sys->i_width =
           DisplayWidth( p_vout->p_sys->p_display, p_vout->p_sys->i_screen );
        p_vout->p_sys->i_height =
           DisplayHeight( p_vout->p_sys->p_display, p_vout->p_sys->i_screen ); 
    }
    else
    {
        /* Set main window's size */
        p_vout->p_sys->i_width = p_vout->render.i_width;
        p_vout->p_sys->i_height = p_vout->render.i_height;
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
                       p_vout->p_sys->i_width,
                       p_vout->p_sys->i_height,
#ifdef MODULE_NAME_IS_x11
                       /* XXX - what's this ? */
                       0,
#else
                       1,
#endif
                       0, InputOutput, 0,
                       CWBackingStore | CWBackPixel | CWEventMask,
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

    if( p_vout->b_fullscreen )
    {
        XSetInputFocus( p_vout->p_sys->p_display, p_vout->p_sys->window,
                        RevertToNone, CurrentTime );
        XMoveWindow( p_vout->p_sys->p_display, p_vout->p_sys->window, 0, 0 );
    }

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
#endif

    /* If the cursor was formerly blank than blank it again */
    if( !p_vout->p_sys->b_mouse_pointer_visible )
    {
        _M( XCommonToggleMousePointer ) ( p_vout );
        _M( XCommonToggleMousePointer ) ( p_vout );
    }

    XSync( p_vout->p_sys->p_display, False );

    /* At this stage, the window is open, displayed, and ready to
     * receive data */

    return( 0 );
}

void _M( XCommonDestroyWindow ) ( vout_thread_t *p_vout )
{
    XSync( p_vout->p_sys->p_display, False );

#ifdef MODULE_NAME_IS_xvideo
    XFreeGC( p_vout->p_sys->p_display, p_vout->p_sys->yuv_gc );
    XDestroyWindow( p_vout->p_sys->p_display, p_vout->p_sys->yuv_window );
#endif

    XUnmapWindow( p_vout->p_sys->p_display, p_vout->p_sys->window );
    XFreeGC( p_vout->p_sys->p_display, p_vout->p_sys->gc );
    XDestroyWindow( p_vout->p_sys->p_display, p_vout->p_sys->window );
}

/*****************************************************************************
 * XCommonEnableScreenSaver: enable screen saver
 *****************************************************************************
 * This function enable the screen saver on a display after it had been
 * disabled by XDisableScreenSaver. Both functions use a counter mechanism to
 * know wether the screen saver can be activated or not: if n successive calls
 * are made to XDisableScreenSaver, n successive calls to XEnableScreenSaver
 * will be required before the screen saver could effectively be activated.
 *****************************************************************************/
void _M( XCommonEnableScreenSaver ) ( vout_thread_t *p_vout )
{
    intf_DbgMsg( "vout: enabling screen saver" );
    XSetScreenSaver( p_vout->p_sys->p_display, p_vout->p_sys->i_ss_timeout,
                     p_vout->p_sys->i_ss_interval,
                     p_vout->p_sys->i_ss_blanking,
                     p_vout->p_sys->i_ss_exposure );
}

/*****************************************************************************
 * XCommonDisableScreenSaver: disable screen saver
 *****************************************************************************
 * See XEnableScreenSaver
 *****************************************************************************/
void _M( XCommonDisableScreenSaver ) ( vout_thread_t *p_vout )
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

#ifdef MODULE_NAME_IS_xvideo
    DPMSDisable( p_vout->p_sys->p_display );
#endif
}

/*****************************************************************************
 * XCommonToggleMousePointer: hide or show the mouse pointer
 *****************************************************************************
 * This function hides the X pointer if it is visible by putting it at
 * coordinates (32,32) and setting the pointer sprite to a blank one. To
 * show it again, we disable the sprite and restore the original coordinates.
 *****************************************************************************/
void _M( XCommonToggleMousePointer ) ( vout_thread_t *p_vout )
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

