/*****************************************************************************
 * x11_graphics.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>

#include "x11_display.hpp"
#include "x11_graphics.hpp"
#include "x11_window.hpp"
#include "../src/generic_bitmap.hpp"
#include "../utils/position.hpp"


X11Graphics::X11Graphics( intf_thread_t *pIntf, X11Display &rDisplay,
                          int width, int height ):
    OSGraphics( pIntf ), m_rDisplay( rDisplay ), m_width( width ),
    m_height( height )
{
    // Get the display paramaters
    int screen = DefaultScreen( XDISPLAY );
    int depth = DefaultDepth( XDISPLAY, screen );

    // X11 doesn't accept that !
    if( width == 0 || height == 0 )
    {
        // Avoid a X11 Bad Value error
        width = height = 1;
        msg_Err( getIntf(), "invalid image size (null width or height)" );
    }

    // Create a pixmap
    m_pixmap = XCreatePixmap( XDISPLAY, DefaultRootWindow( XDISPLAY ),
                              width, height, depth);

    // Create the transparency mask (everything is transparent initially)
    m_mask = XCreateRegion();

    // Create a Graphics Context that does not generate GraphicsExpose events
    XGCValues xgcvalues;
    xgcvalues.graphics_exposures = False;
    m_gc = XCreateGC( XDISPLAY, m_pixmap, GCGraphicsExposures, &xgcvalues );
}


X11Graphics::~X11Graphics()
{
    XFreeGC( XDISPLAY, m_gc );
    XDestroyRegion( m_mask );
    XFreePixmap( XDISPLAY, m_pixmap );
}


void X11Graphics::clear( int xDest, int yDest, int width, int height )
{
    if( width <= 0 || height <= 0 )
    {
        // Clear the transparency mask completely
        XDestroyRegion( m_mask );
        m_mask = XCreateRegion();
    }
    else
    {
        // remove this area from the mask
        XRectangle rect;
        rect.x = xDest;
        rect.y = yDest;
        rect.width = width;
        rect.height = height;
        Region regMask = XCreateRegion();
        XUnionRectWithRegion( &rect, regMask, regMask );
        XSubtractRegion( m_mask, regMask, m_mask );
        XDestroyRegion( regMask );
    }
}


void X11Graphics::drawGraphics( const OSGraphics &rGraphics, int xSrc,
                                int ySrc, int xDest, int yDest, int width,
                                int height )
{
    const X11Graphics& rGraph = (X11Graphics&)rGraphics;

    // check and adapt to source if needed
    if( !checkBoundaries( 0, 0, rGraph.getWidth(), rGraph.getHeight(),
                          xSrc, ySrc, width, height ) )
    {
        msg_Err( getIntf(), "nothing to draw from graphics source" );
        return;
    }

    // check destination
    if( !checkBoundaries( 0, 0, m_width, m_height,
                          xDest, yDest, width, height ) )
    {
        msg_Err( getIntf(), "out of reach destination! pls, debug your skin" );
        return;
    }

    // Source drawable
    Drawable src = rGraph.getDrawable();

    // Create the mask for transparency
    Region voidMask = XCreateRegion();
    XRectangle rect;
    rect.x = xSrc;
    rect.y = ySrc;
    rect.width = width;
    rect.height = height;
    Region clipMask = XCreateRegion();
    XUnionRectWithRegion( &rect, voidMask, clipMask );
    Region mask = XCreateRegion();
    XIntersectRegion( rGraph.getMask(), clipMask, mask );
    XDestroyRegion( clipMask );
    XDestroyRegion( voidMask );
    XOffsetRegion( mask, xDest - xSrc, yDest - ySrc );

    // Copy the pixmap
    XSetRegion( XDISPLAY, m_gc, mask );
    XCopyArea( XDISPLAY, src, m_pixmap, m_gc, xSrc, ySrc, width, height,
               xDest, yDest );

    // Add the source mask to the mask of the graphics
    Region newMask = XCreateRegion();
    XUnionRegion( m_mask, mask, newMask );
    XDestroyRegion( mask );
    XDestroyRegion( m_mask );
    m_mask = newMask;
}


void X11Graphics::drawBitmap( const GenericBitmap &rBitmap, int xSrc,
                              int ySrc, int xDest, int yDest, int width,
                              int height, bool blend )
{
    // check and adapt to source if needed
    if( !checkBoundaries( 0, 0, rBitmap.getWidth(), rBitmap.getHeight(),
                          xSrc, ySrc, width, height ) )
    {
        msg_Err( getIntf(), "empty source! pls, debug your skin" );
        return;
    }

    // check destination
    if( !checkBoundaries( 0, 0, m_width, m_height,
                          xDest, yDest, width, height ) )
    {
        msg_Err( getIntf(), "out of reach destination! pls, debug your skin" );
        return;
    }

    // Get a buffer on the image data
    uint8_t *pBmpData = rBitmap.getData();
    if( pBmpData == NULL )
    {
        // Nothing to draw
        return;
    }

    // Force pending XCopyArea to be sent to the X Server
    // before issuing an XGetImage.
    XSync( XDISPLAY, False );

    // Get the image from the pixmap
    XImage *pImage = XGetImage( XDISPLAY, m_pixmap, xDest, yDest, width,
                                height, AllPlanes, ZPixmap );
    if( pImage == NULL )
    {
        msg_Dbg( getIntf(), "XGetImage returned NULL" );
        return;
    }
    char *pData = pImage->data;

    // Get the padding of this image
    int pad = pImage->bitmap_pad >> 3;
    int shift = ( pad - ( (width * XPIXELSIZE) % pad ) ) % pad;

    // Mask for transparency
    Region mask = XCreateRegion();

    // Get a pointer on the right X11Display::makePixel method
    X11Display::MakePixelFunc_t makePixelFunc = ( blend ?
        m_rDisplay.getBlendPixel() : m_rDisplay.getPutPixel() );

    // Skip the first lines of the image
    pBmpData += 4 * ySrc * rBitmap.getWidth();

    // Copy the bitmap on the image and compute the mask
    for( int y = 0; y < height; y++ )
    {
        // Skip uninteresting bytes at the beginning of the line
        pBmpData += 4 * xSrc;
        // Flag to say whether the previous pixel on the line was visible
        bool wasVisible = false;
        // Beginning of the current visible segment on the line
        int visibleSegmentStart = 0;
        for( int x = 0; x < width; x++ )
        {
            uint8_t b = *(pBmpData++);
            uint8_t g = *(pBmpData++);
            uint8_t r = *(pBmpData++);
            uint8_t a = *(pBmpData++);
            // Draw the pixel
            (m_rDisplay.*makePixelFunc)( (uint8_t*)pData, r, g, b, a );
            pData += XPIXELSIZE;
            if( a > 0 )
            {
                // Pixel is visible
                if( ! wasVisible )
                {
                    // Beginning of a visible segment
                    visibleSegmentStart = x;
                }
                wasVisible = true;
            }
            else
            {
                // Pixel is transparent
                if( wasVisible )
                {
                    // End of a visible segment: add it to the mask
                    addHSegmentInRegion( mask, visibleSegmentStart, x, y );
                }
                wasVisible = false;
            }
        }
        if( wasVisible )
        {
            // End of a visible segment: add it to the mask
            addHSegmentInRegion( mask, visibleSegmentStart, width, y );
        }
        pData += shift;
        // Skip uninteresting bytes at the end of the line
        pBmpData += 4 * (rBitmap.getWidth() - width - xSrc);
    }

    // Apply the mask to the graphics context
    XOffsetRegion( mask, xDest, yDest );
    XSetRegion( XDISPLAY, m_gc, mask );
    // Copy the image on the pixmap
    XPutImage( XDISPLAY, m_pixmap, m_gc, pImage, 0, 0, xDest, yDest, width,
               height);
    XDestroyImage( pImage );

    // Add the bitmap mask to the global graphics mask
    Region newMask = XCreateRegion();
    XUnionRegion( mask, m_mask, newMask );
    XDestroyRegion( m_mask );
    m_mask = newMask;

    XDestroyRegion( mask );
}


void X11Graphics::fillRect( int left, int top, int width, int height,
                            uint32_t color )
{
    // Update the mask with the rectangle area
    Region newMask = XCreateRegion();
    XRectangle rect;
    rect.x = left;
    rect.y = top;
    rect.width = width;
    rect.height = height;
    XUnionRectWithRegion( &rect, m_mask, newMask );
    XDestroyRegion( m_mask );
    m_mask = newMask;

    // Draw the rectangle
    XGCValues gcVal;
    gcVal.foreground = m_rDisplay.getPixelValue( color >> 16, color >> 8, color );
    XChangeGC( XDISPLAY, m_gc, GCForeground,  &gcVal );
    XSetRegion( XDISPLAY, m_gc, m_mask );
    XFillRectangle( XDISPLAY, m_pixmap, m_gc, left, top, width, height );
}


void X11Graphics::drawRect( int left, int top, int width, int height,
                            uint32_t color )
{
    // Update the mask with the rectangle
    addHSegmentInRegion( m_mask, left, left + width, top );
    addHSegmentInRegion( m_mask, left, left + width, top + height );
    addVSegmentInRegion( m_mask, top, top + height, left );
    addVSegmentInRegion( m_mask, top, top + height, left + width );

    // Draw the rectangle
    XGCValues gcVal;
    gcVal.foreground = m_rDisplay.getPixelValue( color >> 16, color >> 8, color );
    XChangeGC( XDISPLAY, m_gc, GCForeground,  &gcVal );
    XSetRegion( XDISPLAY, m_gc, m_mask );
    XDrawRectangle( XDISPLAY, m_pixmap, m_gc, left, top, width - 1, height - 1 );
}


void X11Graphics::applyMaskToWindow( OSWindow &rWindow )
{
    // Get the target window
    Window win = ((X11Window&)rWindow).getDrawable();

    // ensure the window size is right
    XResizeWindow( XDISPLAY, win, m_width, m_height );

    // Change the shape of the window
    XShapeCombineRegion( XDISPLAY, win, ShapeBounding, 0, 0, m_mask,
                         ShapeSet );
}


void X11Graphics::copyToWindow( OSWindow &rWindow, int xSrc,  int ySrc,
                                int width, int height, int xDest, int yDest )
{
    // Destination window
    Drawable dest = ((X11Window&)rWindow).getDrawable();

    XCopyArea( XDISPLAY, m_pixmap, dest, XGC, xSrc, ySrc, width, height,
               xDest, yDest );
}


bool X11Graphics::hit( int x, int y ) const
{
    return XPointInRegion( m_mask, x, y );
}


inline void X11Graphics::addHSegmentInRegion( Region &rMask, int xStart,
                                              int xEnd, int y )
{
    XRectangle rect;
    rect.x = xStart;
    rect.y = y;
    rect.width = xEnd - xStart;
    rect.height = 1;
    Region newMask = XCreateRegion();
    XUnionRectWithRegion( &rect, rMask, newMask );
    XDestroyRegion( rMask );
    rMask = newMask;
}


inline void X11Graphics::addVSegmentInRegion( Region &rMask, int yStart,
                                              int yEnd, int x )
{
    XRectangle rect;
    rect.x = x;
    rect.y = yStart;
    rect.width = 1;
    rect.height = yEnd - yStart;
    Region newMask = XCreateRegion();
    XUnionRectWithRegion( &rect, rMask, newMask );
    XDestroyRegion( rMask );
    rMask = newMask;
}

bool X11Graphics::checkBoundaries( int x_src, int y_src,
                                   int w_src, int h_src,
                                   int& x_target, int& y_target,
                                   int& w_target, int& h_target )
{
    // set valid width and height
    w_target = (w_target > 0) ? w_target : w_src;
    h_target = (h_target > 0) ? h_target : h_src;

    // clip source if needed
    rect srcRegion( x_src, y_src, w_src, h_src );
    rect targetRegion( x_target, y_target, w_target, h_target );
    rect inter;
    if( rect::intersect( srcRegion, targetRegion, &inter ) )
    {
        x_target = inter.x;
        y_target = inter.y;
        w_target = inter.width;
        h_target = inter.height;
        return true;
    }
    return false;
}

#endif
