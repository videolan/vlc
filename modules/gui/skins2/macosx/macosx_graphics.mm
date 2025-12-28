/*****************************************************************************
 * macosx_graphics.mm
 *****************************************************************************
 * Copyright (C) 2024 the VideoLAN team
 *
 * Authors: VLC contributors
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>

#include "macosx_graphics.hpp"
#include "macosx_window.hpp"
#include "../src/generic_bitmap.hpp"

#include <cstring>


MacOSXGraphics::MacOSXGraphics( intf_thread_t *pIntf, int width, int height ):
    OSGraphics( pIntf ), m_width( width ), m_height( height )
{
    // Allocate BGRA pixel buffer
    m_bytesPerRow = width * 4;
    m_pData = new uint8_t[m_bytesPerRow * height];
    memset( m_pData, 0, m_bytesPerRow * height );
}


MacOSXGraphics::~MacOSXGraphics()
{
    delete[] m_pData;
}


void MacOSXGraphics::clear( int xDest, int yDest, int width, int height )
{
    if( width == -1 )
        width = m_width;
    if( height == -1 )
        height = m_height;

    // Clamp to valid range
    if( xDest < 0 )
    {
        width += xDest;
        xDest = 0;
    }
    if( yDest < 0 )
    {
        height += yDest;
        yDest = 0;
    }
    if( xDest + width > m_width )
        width = m_width - xDest;
    if( yDest + height > m_height )
        height = m_height - yDest;

    if( width <= 0 || height <= 0 )
        return;

    // Clear the specified region to transparent
    for( int y = yDest; y < yDest + height; y++ )
    {
        memset( m_pData + y * m_bytesPerRow + xDest * 4, 0, width * 4 );
    }
}


void MacOSXGraphics::drawGraphics( const OSGraphics &rGraphics,
                                    int xSrc, int ySrc,
                                    int xDest, int yDest,
                                    int width, int height )
{
    const MacOSXGraphics &rSrc = static_cast<const MacOSXGraphics&>(rGraphics);

    if( width == -1 )
        width = rSrc.getWidth();
    if( height == -1 )
        height = rSrc.getHeight();

    int srcWidth = rSrc.getWidth();
    int srcHeight = rSrc.getHeight();

    if( !checkBoundaries( xSrc, ySrc, srcWidth, srcHeight,
                          xDest, yDest, width, height ) )
        return;

    const uint8_t *pSrcData = rSrc.getData();

    // Copy pixel data with alpha blending
    for( int y = 0; y < height; y++ )
    {
        for( int x = 0; x < width; x++ )
        {
            int srcOffset = (ySrc + y) * rSrc.m_bytesPerRow + (xSrc + x) * 4;
            int dstOffset = (yDest + y) * m_bytesPerRow + (xDest + x) * 4;

            // Simple copy (source over destination)
            uint8_t srcAlpha = pSrcData[srcOffset + 3];
            if( srcAlpha == 255 )
            {
                // Fully opaque - just copy
                memcpy( m_pData + dstOffset, pSrcData + srcOffset, 4 );
            }
            else if( srcAlpha > 0 )
            {
                // Alpha blend
                uint8_t dstAlpha = m_pData[dstOffset + 3];
                uint8_t outAlpha = srcAlpha + (dstAlpha * (255 - srcAlpha)) / 255;

                if( outAlpha > 0 )
                {
                    for( int c = 0; c < 3; c++ )
                    {
                        m_pData[dstOffset + c] = (uint8_t)(
                            (pSrcData[srcOffset + c] * srcAlpha +
                             m_pData[dstOffset + c] * dstAlpha * (255 - srcAlpha) / 255) / outAlpha
                        );
                    }
                    m_pData[dstOffset + 3] = outAlpha;
                }
            }
        }
    }
}


void MacOSXGraphics::drawBitmap( const GenericBitmap &rBitmap,
                                  int xSrc, int ySrc,
                                  int xDest, int yDest,
                                  int width, int height,
                                  bool blend )
{
    int srcWidth = rBitmap.getWidth();
    int srcHeight = rBitmap.getHeight();

    if( width == -1 )
        width = srcWidth;
    if( height == -1 )
        height = srcHeight;

    if( !checkBoundaries( xSrc, ySrc, srcWidth, srcHeight,
                          xDest, yDest, width, height ) )
        return;

    // Get bitmap data
    uint8_t *pBmpData = rBitmap.getData();
    if( !pBmpData )
        return;

    int bmpBytesPerRow = srcWidth * 4;

    // Copy/blend pixel data
    for( int y = 0; y < height; y++ )
    {
        for( int x = 0; x < width; x++ )
        {
            int srcOffset = (ySrc + y) * bmpBytesPerRow + (xSrc + x) * 4;
            int dstOffset = (yDest + y) * m_bytesPerRow + (xDest + x) * 4;

            uint8_t srcAlpha = pBmpData[srcOffset + 3];

            if( !blend || srcAlpha == 255 )
            {
                // Direct copy
                m_pData[dstOffset + 0] = pBmpData[srcOffset + 0]; // B
                m_pData[dstOffset + 1] = pBmpData[srcOffset + 1]; // G
                m_pData[dstOffset + 2] = pBmpData[srcOffset + 2]; // R
                m_pData[dstOffset + 3] = srcAlpha;
            }
            else if( srcAlpha > 0 )
            {
                // Alpha blend
                uint8_t dstAlpha = m_pData[dstOffset + 3];
                uint8_t outAlpha = srcAlpha + (dstAlpha * (255 - srcAlpha)) / 255;

                if( outAlpha > 0 )
                {
                    for( int c = 0; c < 3; c++ )
                    {
                        m_pData[dstOffset + c] = (uint8_t)(
                            (pBmpData[srcOffset + c] * srcAlpha +
                             m_pData[dstOffset + c] * dstAlpha * (255 - srcAlpha) / 255) / outAlpha
                        );
                    }
                    m_pData[dstOffset + 3] = outAlpha;
                }
            }
        }
    }
}


void MacOSXGraphics::fillRect( int left, int top, int width, int height,
                                uint32_t color )
{
    // Clamp to valid range
    if( left < 0 )
    {
        width += left;
        left = 0;
    }
    if( top < 0 )
    {
        height += top;
        top = 0;
    }
    if( left + width > m_width )
        width = m_width - left;
    if( top + height > m_height )
        height = m_height - top;

    if( width <= 0 || height <= 0 )
        return;

    // Extract color components (color is #RRGGBB)
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    // Fill the rectangle
    for( int y = top; y < top + height; y++ )
    {
        for( int x = left; x < left + width; x++ )
        {
            int offset = y * m_bytesPerRow + x * 4;
            m_pData[offset + 0] = b;
            m_pData[offset + 1] = g;
            m_pData[offset + 2] = r;
            m_pData[offset + 3] = 255;
        }
    }
}


void MacOSXGraphics::drawRect( int left, int top, int width, int height,
                                uint32_t color )
{
    // Extract color components
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    // Draw the rectangle outline
    // Top and bottom edges
    for( int x = left; x < left + width && x < m_width; x++ )
    {
        if( x >= 0 )
        {
            if( top >= 0 && top < m_height )
            {
                int offset = top * m_bytesPerRow + x * 4;
                m_pData[offset + 0] = b;
                m_pData[offset + 1] = g;
                m_pData[offset + 2] = r;
                m_pData[offset + 3] = 255;
            }
            if( top + height - 1 >= 0 && top + height - 1 < m_height )
            {
                int offset = (top + height - 1) * m_bytesPerRow + x * 4;
                m_pData[offset + 0] = b;
                m_pData[offset + 1] = g;
                m_pData[offset + 2] = r;
                m_pData[offset + 3] = 255;
            }
        }
    }
    // Left and right edges
    for( int y = top; y < top + height && y < m_height; y++ )
    {
        if( y >= 0 )
        {
            if( left >= 0 && left < m_width )
            {
                int offset = y * m_bytesPerRow + left * 4;
                m_pData[offset + 0] = b;
                m_pData[offset + 1] = g;
                m_pData[offset + 2] = r;
                m_pData[offset + 3] = 255;
            }
            if( left + width - 1 >= 0 && left + width - 1 < m_width )
            {
                int offset = y * m_bytesPerRow + (left + width - 1) * 4;
                m_pData[offset + 0] = b;
                m_pData[offset + 1] = g;
                m_pData[offset + 2] = r;
                m_pData[offset + 3] = 255;
            }
        }
    }
}


void MacOSXGraphics::applyMaskToWindow( OSWindow &rWindow )
{
    // macOS handles transparency through the alpha channel directly
    // The window's opaque property should be set to NO
    // This is handled in copyToWindow
}


void MacOSXGraphics::copyToWindow( OSWindow &rWindow, int xSrc, int ySrc,
                                    int width, int height,
                                    int xDest, int yDest )
{
    @autoreleasepool {
        MacOSXWindow &rMacWindow = static_cast<MacOSXWindow&>(rWindow);
        NSWindow *nsWindow = rMacWindow.getNSWindow();

        if( !nsWindow )
            return;

        // Create CGImage from our pixel data
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        CGContextRef context = CGBitmapContextCreate(
            m_pData,
            m_width,
            m_height,
            8,
            m_bytesPerRow,
            colorSpace,
            kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little
        );

        if( context )
        {
            CGImageRef image = CGBitmapContextCreateImage( context );
            if( image )
            {
                // Create subimage if needed
                CGImageRef subImage = image;
                if( xSrc != 0 || ySrc != 0 || width != m_width || height != m_height )
                {
                    CGRect subRect = CGRectMake( xSrc, ySrc, width, height );
                    subImage = CGImageCreateWithImageInRect( image, subRect );
                }

                // Update the window's content view
                NSView *contentView = [nsWindow contentView];
                if( [contentView isKindOfClass:[NSImageView class]] )
                {
                    NSImage *nsImage = [[NSImage alloc] initWithCGImage:
                        (subImage ? subImage : image)
                        size:NSMakeSize(width, height)];
                    [(NSImageView *)contentView setImage:nsImage];
                }
                else
                {
                    // Create a layer-backed view for better performance
                    [contentView setWantsLayer:YES];
                    contentView.layer.contents = (__bridge id)(subImage ? subImage : image);
                }

                if( subImage != image )
                    CGImageRelease( subImage );
                CGImageRelease( image );
            }
            CGContextRelease( context );
        }
        CGColorSpaceRelease( colorSpace );
    }
}


bool MacOSXGraphics::hit( int x, int y ) const
{
    if( x < 0 || y < 0 || x >= m_width || y >= m_height )
        return false;

    // Check if the pixel has non-zero alpha
    int offset = y * m_bytesPerRow + x * 4;
    return m_pData[offset + 3] > 0;
}


void *MacOSXGraphics::getImage() const
{
    @autoreleasepool {
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        CGContextRef context = CGBitmapContextCreate(
            const_cast<uint8_t*>(m_pData),
            m_width,
            m_height,
            8,
            m_bytesPerRow,
            colorSpace,
            kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little
        );

        CGImageRef image = NULL;
        if( context )
        {
            image = CGBitmapContextCreateImage( context );
            CGContextRelease( context );
        }
        CGColorSpaceRelease( colorSpace );

        return image;
    }
}


bool MacOSXGraphics::checkBoundaries( int x_src, int y_src, int w_src, int h_src,
                                       int& x_target, int& y_target,
                                       int& w_target, int& h_target )
{
    // Check for valid source
    if( x_src < 0 || y_src < 0 )
        return false;

    // Adjust for source boundaries
    if( x_src + w_target > w_src )
        w_target = w_src - x_src;
    if( y_src + h_target > h_src )
        h_target = h_src - y_src;

    // Adjust for negative destination
    if( x_target < 0 )
    {
        w_target += x_target;
        x_target = 0;
    }
    if( y_target < 0 )
    {
        h_target += y_target;
        y_target = 0;
    }

    // Adjust for destination boundaries
    if( x_target + w_target > m_width )
        w_target = m_width - x_target;
    if( y_target + h_target > m_height )
        h_target = m_height - y_target;

    return w_target > 0 && h_target > 0;
}
