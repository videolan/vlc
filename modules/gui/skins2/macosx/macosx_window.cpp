/*****************************************************************************
 * macosx_window.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
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

#ifdef MACOSX_SKINS

#include "macosx_window.hpp"
#include "macosx_loop.hpp"
#include "../src/os_factory.hpp"


MacOSXWindow::MacOSXWindow( intf_thread_t *pIntf, GenericWindow &rWindow,
                            bool dragDrop, bool playOnDrop,
                            MacOSXWindow *pParentWindow ):
    OSWindow( pIntf ), m_pParent( pParentWindow ), m_dragDrop( dragDrop )
{
    // Create the window
    Rect rect;
    SetRect( &rect, 0, 0, 0, 0 );
    CreateNewWindow( kDocumentWindowClass, kWindowNoShadowAttribute |
                     kWindowNoTitleBarAttribute, &rect, &m_win );

    // Create the event handler for this window
    OSFactory *pOSFactory = OSFactory::instance( getIntf() );
    ((MacOSXLoop*)pOSFactory->getOSLoop())->registerWindow( rWindow, m_win );
}


MacOSXWindow::~MacOSXWindow()
{
    DisposeWindow( m_win );
}


void MacOSXWindow::show( int left, int top ) const
{
    ShowWindow( m_win );
}


void MacOSXWindow::hide() const
{
    HideWindow( m_win );
}


void MacOSXWindow::moveResize( int left, int top, int width, int height ) const
{
    MoveWindow( m_win, left, top, false );
    SizeWindow( m_win, width, height, true );
}


void MacOSXWindow::raise() const
{
    SelectWindow( m_win );
}


void MacOSXWindow::setOpacity( uint8_t value ) const
{
    SetWindowAlpha( m_win, (float)value / 255.0 );
}


void MacOSXWindow::toggleOnTop( bool onTop ) const
{
    // TODO
}

#endif
