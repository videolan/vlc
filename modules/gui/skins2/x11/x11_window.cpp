/*****************************************************************************
 * x11_window.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_window.cpp,v 1.1 2004/01/03 23:31:34 asmax Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifdef X11_SKINS

#include <X11/Xatom.h>

#include "../src/generic_window.hpp"
#include "x11_window.hpp"
#include "x11_display.hpp"
#include "x11_graphics.hpp"
#include "x11_dragdrop.hpp"
#include "x11_factory.hpp"


X11Window::X11Window( intf_thread_t *pIntf, GenericWindow &rWindow,
                      X11Display &rDisplay, bool dragDrop, bool playOnDrop ):
    OSWindow( pIntf ), m_rDisplay( rDisplay ), m_dragDrop( dragDrop )
{
    Window root = DefaultRootWindow( XDISPLAY );
    XSetWindowAttributes attr;

    // Create the window
    m_wnd = XCreateWindow( XDISPLAY, root, 0, 0, 1, 1, 0, 0,
                           InputOutput, CopyFromParent, 0, &attr );

    // Select events received by the window
    XSelectInput( XDISPLAY, m_wnd, ExposureMask|KeyPressMask|PointerMotionMask|
                  ButtonPressMask|ButtonReleaseMask|LeaveWindowMask|
                  FocusChangeMask );

    // Store a pointer on the generic window in a map
    X11Factory *pFactory = (X11Factory*)X11Factory::instance( getIntf() );
    pFactory->m_windowMap[m_wnd] = &rWindow;

    // Changing decorations
    struct {
        unsigned long flags;
        unsigned long functions;
        unsigned long decorations;
        long input_mode;
        unsigned long status;
    } motifWmHints;
    Atom hints_atom = XInternAtom( XDISPLAY, "_MOTIF_WM_HINTS", False );
    motifWmHints.flags = 2;    // MWM_HINTS_DECORATIONS;
    motifWmHints.decorations = 0;
    XChangeProperty( XDISPLAY, m_wnd, hints_atom, hints_atom, 32,
                     PropModeReplace, (unsigned char *)&motifWmHints,
                     sizeof( motifWmHints ) / sizeof( long ) );

    // Drag & drop
    if( m_dragDrop )
    {
        // Register the window as a drop target
        m_pDropTarget = new X11DragDrop( getIntf(), m_rDisplay, m_wnd,
                                         playOnDrop );

        Atom xdndAtom = XInternAtom( XDISPLAY, "XdndAware", False );
        char xdndVersion = 4;
        XChangeProperty( XDISPLAY, m_wnd, xdndAtom, XA_ATOM, 32,
                         PropModeReplace, (unsigned char *)&xdndVersion, 1 );

        // Store a pointer on the D&D object as a window property.
        storePointer( "DND_OBJECT", (void*)m_pDropTarget );
    }

    // Change the window title XXX
    XStoreName( XDISPLAY, m_wnd, "VLC" );
}


X11Window::~X11Window()
{
    X11Factory *pFactory = (X11Factory*)X11Factory::instance( getIntf() );
    pFactory->m_windowMap[m_wnd] = NULL;

    if( m_dragDrop )
    {
        delete m_pDropTarget;
    }
    XDestroyWindow( XDISPLAY, m_wnd );
    XSync( XDISPLAY, False );
}


void X11Window::show( int left, int top )
{
    // Map the window
    XMapRaised( XDISPLAY, m_wnd );
    XMoveWindow( XDISPLAY, m_wnd, left, top );
}


void X11Window::hide()
{
    // Unmap the window
    XUnmapWindow( XDISPLAY, m_wnd );
}


void X11Window::moveResize( int left, int top, int width, int height )
{
    XMoveResizeWindow( XDISPLAY, m_wnd, left, top, width, height );
}


void X11Window::raise()
{
    XRaiseWindow( XDISPLAY, m_wnd );
}


void X11Window::setOpacity( uint8_t value )
{
    // Sorry, the opacity cannot be changed :)
}


void X11Window::toggleOnTop( bool onTop )
{
    // XXX TODO
}


void X11Window::storePointer( const char *pName, void *pPtr )
{
    // We don't assume pointers are 32bits, so it's a bit tricky
    unsigned char data[sizeof(void*)];
    memcpy( data, &pPtr, sizeof(void*) );

    // Store the pointer on the generic window as a window property.
    Atom prop = XInternAtom( XDISPLAY, pName, False );
    Atom type = XInternAtom( XDISPLAY, "POINTER", False );
    XChangeProperty( XDISPLAY, m_wnd, prop, type, 8, PropModeReplace, data,
                     sizeof(GenericWindow*) );
}

#endif
