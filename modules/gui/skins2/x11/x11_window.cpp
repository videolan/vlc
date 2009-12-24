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
                      X11Window *pParentWindow, GenericWindow::WindowType_t type ):
    OSWindow( pIntf ), m_rDisplay( rDisplay ), m_pParent( pParentWindow ),
    m_dragDrop( dragDrop ), m_type ( type )
{
    XSetWindowAttributes attr;
    unsigned long valuemask;
    string name_type;

    if( type == GenericWindow::FullscreenWindow )
    {
        m_wnd_parent = DefaultRootWindow( XDISPLAY );

        int i_screen = DefaultScreen( XDISPLAY );

        attr.event_mask = ExposureMask | StructureNotifyMask;
        attr.background_pixel = BlackPixel( XDISPLAY, i_screen );
        attr.backing_store = Always;
        valuemask = CWBackingStore | CWBackPixel | CWEventMask;

        if( NET_WM_STATE_FULLSCREEN == None )
        {
            attr.override_redirect = True;
            valuemask = valuemask | CWOverrideRedirect;
        }

        name_type = "Fullscreen";
    }
    else if( type == GenericWindow::VoutWindow )
    {
        m_wnd_parent = pParentWindow->m_wnd;

        int i_screen = DefaultScreen( XDISPLAY );

        attr.event_mask = ExposureMask | StructureNotifyMask;
        attr.backing_store = Always;
        attr.background_pixel = BlackPixel( XDISPLAY, i_screen );
        valuemask = CWBackingStore | CWBackPixel | CWEventMask;

        name_type = "VoutWindow";
    }
    else
    {
        m_wnd_parent = DefaultRootWindow( XDISPLAY );

        attr.event_mask = ExposureMask | StructureNotifyMask;
        valuemask = CWEventMask;

        name_type = "TopWindow";
    }

    // Create the window
    m_wnd = XCreateWindow( XDISPLAY, m_wnd_parent, -10, 0, 10, 10, 0, 0,
                           InputOutput, CopyFromParent, valuemask, &attr );

    // wait for X server to process the previous commands
    XSync( XDISPLAY, false );

    // Set the colormap for 8bpp mode
    if( XPIXELSIZE == 1 )
    {
        XSetWindowColormap( XDISPLAY, m_wnd, m_rDisplay.getColormap() );
    }

    // Select events received by the window
    long event_mask;
    if( type == GenericWindow::VoutWindow )
    {
        event_mask =  ExposureMask|KeyPressMask|
                      LeaveWindowMask|FocusChangeMask;
    }
    else
    {
        event_mask =  ExposureMask|KeyPressMask|
                      PointerMotionMask|ButtonPressMask|ButtonReleaseMask|
                      LeaveWindowMask|FocusChangeMask;
    }
    XSelectInput( XDISPLAY, m_wnd, event_mask );

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
    string name_window = "VLC (" + name_type + ")";
    XStoreName( XDISPLAY, m_wnd, name_window.c_str() );

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

    XReparentWindow( XDISPLAY, m_wnd, new_parent, x, y);
    if( w && h )
        XResizeWindow( XDISPLAY, m_wnd, w, h );

    m_wnd_parent = new_parent;
}


void X11Window::show() const
{
    // Map the window
    if( m_type == GenericWindow::VoutWindow )
    {
       XLowerWindow( XDISPLAY, m_wnd );
       XMapWindow( XDISPLAY, m_wnd );
    }
    else if( m_type == GenericWindow::FullscreenWindow )
    {
        XMapRaised( XDISPLAY, m_wnd );
        setFullscreen();
        // toggleOnTop( true );
    }
    else
    {
        XMapRaised( XDISPLAY, m_wnd );
    }
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
    if( NET_WM_WINDOW_OPACITY == None )
        return;

    if( 255==value )
        XDeleteProperty(XDISPLAY, m_wnd, NET_WM_WINDOW_OPACITY);
    else
    {
        uint32_t opacity = value * ((uint32_t)-1/255);
        XChangeProperty(XDISPLAY, m_wnd, NET_WM_WINDOW_OPACITY, XA_CARDINAL, 32,
                        PropModeReplace, (unsigned char *) &opacity, 1L);
    }
    XSync( XDISPLAY, False );
}


void X11Window::setFullscreen( ) const
{
    if( NET_WM_STATE_FULLSCREEN != None )
    {
        XClientMessageEvent event;
        memset( &event, 0, sizeof( XClientMessageEvent ) );

        event.type = ClientMessage;
        event.message_type = NET_WM_STATE;
        event.display = XDISPLAY;
        event.window = m_wnd;
        event.format = 32;
        event.data.l[ 0 ] = 1;
        event.data.l[ 1 ] = NET_WM_STATE_FULLSCREEN;
 
        XSendEvent( XDISPLAY,
                    DefaultRootWindow( XDISPLAY ),
                    False, SubstructureNotifyMask|SubstructureRedirectMask,
                    (XEvent*)&event );
    }
}


void X11Window::toggleOnTop( bool onTop ) const
{
    if( NET_WM_STAYS_ON_TOP != None )
    {
        /* Switch "on top" status */
        XClientMessageEvent event;
        memset( &event, 0, sizeof( XClientMessageEvent ) );

        event.type = ClientMessage;
        event.message_type = NET_WM_STATE;
        event.display = XDISPLAY;
        event.window = m_wnd;
        event.format = 32;
        event.data.l[ 0 ] = onTop; /* set property */
        event.data.l[ 1 ] = NET_WM_STAYS_ON_TOP;

        XSendEvent( XDISPLAY, DefaultRootWindow( XDISPLAY ),
                    False, SubstructureNotifyMask|SubstructureRedirectMask, (XEvent*)&event );
    }
    else if( NET_WM_STATE_ABOVE != None )
    {
        /* Switch "above" state */
        XClientMessageEvent event;
        memset( &event, 0, sizeof( XClientMessageEvent ) );

        event.type = ClientMessage;
        event.message_type = NET_WM_STATE;
        event.display = XDISPLAY;
        event.window = m_wnd;
        event.format = 32;
        event.data.l[ 0 ] = onTop; /* set property */
        event.data.l[ 1 ] = NET_WM_STATE_ABOVE;

        XSendEvent( XDISPLAY, DefaultRootWindow( XDISPLAY ),
                    False, SubstructureNotifyMask|SubstructureRedirectMask, (XEvent*)&event );
    }
}

#endif
