/*****************************************************************************
 * macosx_graphics.cpp
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

#include "macosx_graphics.hpp"
#include "macosx_window.hpp"


MacOSXGraphics::MacOSXGraphics( intf_thread_t *pIntf, int width, int height ):
    OSGraphics( pIntf ), m_width( width ), m_height( height )
{
    // TODO
}


MacOSXGraphics::~MacOSXGraphics()
{
    // TODO
}


void MacOSXGraphics::clear()
{
    // TODO
}


void MacOSXGraphics::drawGraphics( const OSGraphics &rGraphics, int xSrc,
                                   int ySrc, int xDest, int yDest, int width,
                                   int height )
{
    // TODO
}


void MacOSXGraphics::drawBitmap( const GenericBitmap &rBitmap, int xSrc,
                                 int ySrc, int xDest, int yDest, int width,
                                 int height, bool blend )
{
    // TODO
}


void MacOSXGraphics::fillRect( int left, int top, int width, int height,
                               uint32_t color )
{
    // TODO
}


void MacOSXGraphics::drawRect( int left, int top, int width, int height,
                            uint32_t color )
{
    // TODO
}


void MacOSXGraphics::applyMaskToWindow( OSWindow &rWindow )
{
    // TODO
}


void MacOSXGraphics::copyToWindow( OSWindow &rWindow, int xSrc,  int ySrc,
                                int width, int height, int xDest, int yDest )
{
    // Get the graphics context
    WindowRef win = ((MacOSXWindow&)rWindow).getWindowRef();
    SetPortWindowPort( win );
    GrafPtr port = GetWindowPort( win );
    CGContextRef gc;
    QDBeginCGContext( port, &gc );

//    CGContextSetRGBFillColor( gc, 1, 0, 0, 1 );
//    CGContextFillRect( gc, CGRectMake( 0, 0, 50, 50 ));

    // Release the graphics context
    QDEndCGContext( port, &gc );
}


bool MacOSXGraphics::hit( int x, int y ) const
{
    // TODO
    return false;
}

#endif
