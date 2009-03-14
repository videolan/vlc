/*****************************************************************************
 * x11_window.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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

#ifdef X11_SKINS

#include <X11/Xatom.h>

#include "../src/generic_window.hpp"
#include "../src/vlcproc.hpp"
#include "x11_window.hpp"
#include "x11_display.hpp"
#include "x11_graphics.hpp"
#include "x11_dragdrop.hpp"
#include "x11_factory.hpp"


X11Window::X11Window( intf_thread_t *pIntf, GenericWindow &rWindow,
                      X11Display &rDisplay, bool dragDrop, bool playOnDrop,
                      X11Window *pParentWindow ):
    OSWindow( pIntf ), m_rDisplay( rDisplay ), m_pParent( pParentWindow ),
    m_dragDrop( dragDrop )
{
    if (pParentWindow)
    {
        m_wnd_parent = pParentWindow->m_wnd;
    }
    else
    {
        m_wnd_parent = DefaultRootWindow( XDISPLAY );
    }
    XSetWindowAttributes attr;
    attr.event_mask = ExposureMask | StructureNotifyMask;

    // Create the window
    m_wnd = XCreateWindow( XDISPLAY, m_wnd_parent, -10, 0, 1, 1, 0, 0,
                           InputOutput, CopyFromParent, CWEventMask, &attr );

    // Make sure window is created before returning
    XMapWindow( XDISPLAY, m_wnd );
    bool b_map_notify = false;
    do
    {
        XEvent xevent;
        XWindowEvent( XDISPLAY, m_wnd, SubstructureNotifyMask |
                      StructureNotifyMask, &xevent);
        if( (xevent.type == MapNotify)
             && (xevent.xmap.window == m_wnd ) )
        {
            b_map_notify = true;
        }
    } while( ! b_map_notify );
    XUnmapWindow( XDISPLAY, m_wnd );

    // Set the colormap for 8bpp mode
    if( XPIXELSIZE == 1 )
    {
        XSetWindowColormap( XDISPLAY, m_wnd, m_rDisplay.getColormap() );
    }

    // Select events received by the window
    XSelectInput( XDISPLAY, m_wnd, ExposureMask|KeyPressMask|
                  PointerMotionMask|ButtonPressMask|ButtonReleaseMask|
                  LeaveWindowMask|FocusChangeMask );

    // Store a pointer on the generic window in a map
    X11Factory *pFactory = (X11Factory*)X11Factory::instance( getIntf() );
    pFactory->m_windowMap[m_wnd] = &rWindow;

    // Changing decorations
    struct {
        unsigned long flags;
        unsigned long functions;
        unsigned long decorations;
        signed   long input_mode;
        unsigned long status;
    } motifWmHints;
    Atom hints_atom = XInternAtom( XDISPLAY, "_MOTIF_WM_HINTS", False );
    motifWmHints.flags = 2;    // MWM_HINTS_DECORATIONS;
    motifWmHints.decorations = 0;
    XChangeProperty( XDISPLAY, m_wnd, hints_atom, hints_atom, 32,
                     PropModeReplace, (unsigned char *)&motifWmHints,
                     sizeof( motifWmHints ) / sizeof( uint32_t ) );

    // Drag & drop
    if( m_dragDrop )
    {
        // Create a Dnd object for this window
        m_pDropTarget = new X11DragDrop( getIntf(), m_rDisplay, m_wnd,
                                         playOnDrop );

        // Register the window as a drop target
        Atom xdndAtom = XInternAtom( XDISPLAY, "XdndAware", False );
        char xdndVersion = 4;
        XChangeProperty( XDISPLAY, m_wnd, xdndAtom, XA_ATOM, 32,
                         PropModeReplace, (unsigned char *)&xdndVersion, 1 );

        // Store a pointer to be used in X11Loop
        pFactory->m_dndMap[m_wnd] = m_pDropTarget;
    }

    // Change the window title
    XStoreName( XDISPLAY, m_wnd, "VLC" );

    // Associate the window to the main "parent" window
    XSetTransientForHint( XDISPLAY, m_wnd, m_rDisplay.getMainWindow() );

}


X11Window::~X11Window()
{
    X11Factory *pFactory = (X11Factory*)X11Factory::instance( getIntf() );
    pFactory->m_windowMap[m_wnd] = NULL;
    pFactory->m_dndMap[m_wnd] = NULL;

    if( m_dragDrop )
    {
        delete m_pDropTarget;
    }
    XDestroyWindow( XDISPLAY, m_wnd );
    XSync( XDISPLAY, False );
}

void X11Window::reparent( void* OSHandle, int x, int y, int w, int h )
{
    // Reparent the window
    Window new_parent =
           OSHandle ? (Window) OSHandle : DefaultRootWindow( XDISPLAY );

    if( w && h )
        XResizeWindow( XDISPLAY, m_wnd, w, h );

    XReparentWindow( XDISPLAY, m_wnd, new_parent, x, y);
    m_wnd_parent = new_parent;
}


void X11Window::show( int left, int top ) const
{
    // Map the window
    XMapRaised( XDISPLAY, m_wnd );
    XMoveWindow( XDISPLAY, m_wnd, left, top );
}


void X11Window::hide() const
{
    // Unmap the window
    XUnmapWindow( XDISPLAY, m_wnd );
}


void X11Window::moveResize( int left, int top, int width, int height ) const
{
    if( width && height )
        XMoveResizeWindow( XDISPLAY, m_wnd, left, top, width, height );
    else
        XMoveWindow( XDISPLAY, m_wnd, left, top );
}


void X11Window::raise() const
{
    XRaiseWindow( XDISPLAY, m_wnd );
}


void X11Window::setOpacity( uint8_t value ) const
{
    // Sorry, the opacity cannot be changed :)
}


void X11Window::toggleOnTop( bool onTop ) const
{
    int i_ret, i_format;
    unsigned long i, i_items, i_bytesafter;
    Atom net_wm_supported, net_wm_state, net_wm_state_on_top,net_wm_state_above;
    union { Atom *p_atom; unsigned char *p_char; } p_args;

    p_args.p_atom = NULL;

    net_wm_supported = XInternAtom( XDISPLAY, "_NET_SUPPORTED", False );

    i_ret = XGetWindowProperty( XDISPLAY, DefaultRootWindow( XDISPLAY ),
                                net_wm_supported,
                                0, 16384, False, AnyPropertyType,
                                &net_wm_supported,
                                &i_format, &i_items, &i_bytesafter,
                                (unsigned char **)&p_args );

    if( i_ret != Success || i_items == 0 ) return; /* Not supported */

    net_wm_state = XInternAtom( XDISPLAY, "_NET_WM_STATE", False );
    net_wm_state_on_top = XInternAtom( XDISPLAY, "_NET_WM_STATE_STAYS_ON_TOP",
                                       False );

    for( i = 0; i < i_items; i++ )
    {
        if( p_args.p_atom[i] == net_wm_state_on_top ) break;
    }

    if( i == i_items )
    { /* use _NET_WM_STATE_ABOVE if window manager
       * doesn't handle _NET_WM_STATE_STAYS_ON_TOP */

        net_wm_state_above = XInternAtom( XDISPLAY, "_NET_WM_STATE_ABOVE",
                                          False);
        for( i = 0; i < i_items; i++ )
        {
            if( p_args.p_atom[i] == net_wm_state_above ) break;
        }
 
        XFree( p_args.p_atom );
        if( i == i_items )
            return; /* Not supported */

        /* Switch "on top" status */
        XClientMessageEvent event;
        memset( &event, 0, sizeof( XClientMessageEvent ) );

        event.type = ClientMessage;
        event.message_type = net_wm_state;
        event.display = XDISPLAY;
        event.window = m_wnd;
        event.format = 32;
        event.data.l[ 0 ] = onTop; /* set property */
        event.data.l[ 1 ] = net_wm_state_above;

        XSendEvent( XDISPLAY, DefaultRootWindow( XDISPLAY ),
                    False, SubstructureRedirectMask, (XEvent*)&event );
        return;
    }

    XFree( p_args.p_atom );

    /* Switch "on top" status */
    XClientMessageEvent event;
    memset( &event, 0, sizeof( XClientMessageEvent ) );

    event.type = ClientMessage;
    event.message_type = net_wm_state;
    event.display = XDISPLAY;
    event.window = m_wnd;
    event.format = 32;
    event.data.l[ 0 ] = onTop; /* set property */
    event.data.l[ 1 ] = net_wm_state_on_top;

    XSendEvent( XDISPLAY, DefaultRootWindow( XDISPLAY ),
                False, SubstructureRedirectMask, (XEvent*)&event );
}

#endif
