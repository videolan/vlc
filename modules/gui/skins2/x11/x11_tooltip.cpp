/*****************************************************************************
 * x11_tooltip.cpp
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

#include "../src/generic_window.hpp"
#include "x11_display.hpp"
#include "x11_graphics.hpp"
#include "x11_tooltip.hpp"

X11Tooltip::X11Tooltip( intf_thread_t *pIntf,
                                   X11Display &rDisplay ):
    OSTooltip( pIntf ), m_rDisplay( rDisplay )
{
    Window root = DefaultRootWindow( XDISPLAY );
    XSetWindowAttributes attr;
    // Bypass the window manager
    attr.override_redirect = True;

    // Create the window
    m_wnd = XCreateWindow( XDISPLAY, root, 0, 0, 1, 1, 0, 0,
                           InputOutput, CopyFromParent, CWOverrideRedirect,
                           &attr );

    // Set the colormap for 8bpp mode
    if( XPIXELSIZE == 1 )
    {
        XSetWindowColormap( XDISPLAY, m_wnd, m_rDisplay.getColormap() );
    }
}


X11Tooltip::~X11Tooltip()
{
    XDestroyWindow( XDISPLAY, m_wnd );
}


void X11Tooltip::show( int left, int top, OSGraphics &rText )
{
    // Source drawable
    Drawable src = ((X11Graphics&)rText).getDrawable();
    int width = rText.getWidth();
    int height = rText.getHeight();

    XMoveResizeWindow( XDISPLAY, m_wnd, left, top, width, height );
    // Show the window
    XMapRaised( XDISPLAY, m_wnd );
    // Move it again if the window manager forgets the position
    XMoveWindow( XDISPLAY, m_wnd, left, top );
    XCopyArea( XDISPLAY, src, m_wnd, XGC, 0, 0, width, height, 0, 0 );
}


void X11Tooltip::hide()
{
    // Unmap the window
    XUnmapWindow( XDISPLAY, m_wnd );
}


#endif
