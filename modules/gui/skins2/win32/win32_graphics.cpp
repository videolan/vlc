/*****************************************************************************
 * win32_graphics.cpp
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

#ifdef WIN32_SKINS

#define WINVER 0x500

#include "win32_factory.hpp"
#include "win32_graphics.hpp"
#include "win32_window.hpp"
#include "../src/generic_bitmap.hpp"

#ifndef AC_SRC_ALPHA
#define AC_SRC_ALPHA 1
#endif

Win32Graphics::Win32Graphics( intf_thread_t *pIntf, int width, int height ):
    OSGraphics( pIntf ), m_width( width ), m_height( height ), m_hDC( NULL )
{
    HBITMAP hBmp;
    HDC hDC = GetDC( NULL );
    hBmp = CreateCompatibleBitmap( hDC, m_width, m_height );
    ReleaseDC( NULL, hDC );

    m_hDC = CreateCompatibleDC( NULL );
    SelectObject( m_hDC, hBmp );
    DeleteObject( hBmp );

    // Create the mask
    m_mask = CreateRectRgn( 0, 0, 0, 0 );
}


Win32Graphics::~Win32Graphics()
{
    DeleteDC( m_hDC );
    DeleteObject( m_mask );
}


void Win32Graphics::clear( int xDest, int yDest, int width, int height )
{
    if( width <= 0 || height <= 0 )
    {
        // Clear the transparency mask
        DeleteObject( m_mask );
        m_mask = CreateRectRgn( 0, 0, 0, 0 );
    }
    else
    {
        HRGN mask = CreateRectRgn( xDest, yDest,
                                   xDest + width, yDest + height );
        CombineRgn( m_mask, m_mask, mask, RGN_DIFF );
        DeleteObject( mask );
    }
}


void Win32Graphics::drawBitmap( const GenericBitmap &rBitmap,
                                int xSrc, int ySrc, int xDest, int yDest,
                                int width, int height, bool blend )
{
    (void)blend;

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

    void *pBits;     // pointer to DIB section
    // Fill a BITMAPINFO structure
    BITMAPINFO bmpInfo;
    memset( &bmpInfo, 0, sizeof( bmpInfo ) );
    bmpInfo.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
    bmpInfo.bmiHeader.biWidth = width;
    bmpInfo.bmiHeader.biHeight = -height;
    bmpInfo.bmiHeader.biPlanes = 1;
    bmpInfo.bmiHeader.biBitCount = 32;
    bmpInfo.bmiHeader.biCompression = BI_RGB;
    bmpInfo.bmiHeader.biSizeImage = width * height * 4;

    // Create a DIB (Device Independent Bitmap) and associate it with
    // a temporary DC
    HDC hDC = CreateCompatibleDC( m_hDC );
    HBITMAP hBmp = CreateDIBSection( hDC, &bmpInfo, DIB_RGB_COLORS,
                                     &pBits, NULL, 0 );
    SelectObject( hDC, hBmp );

    // Mask for transparency
    HRGN mask = CreateRectRgn( 0, 0, 0, 0 );

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
            ((UINT32 *)pBits)[x + y * width] =
                (a << 24) | (r << 16) | (g << 8) | b;

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
                    addSegmentInRegion( mask, visibleSegmentStart, x, y );
                }
                wasVisible = false;
            }
        }
        if( wasVisible )
        {
            // End of a visible segment: add it to the mask
            addSegmentInRegion( mask, visibleSegmentStart, width, y );
        }
        // Skip uninteresting bytes at the end of the line
        pBmpData += 4 * (rBitmap.getWidth() - width - xSrc);
    }

    // Apply the mask to the internal DC
    OffsetRgn( mask, xDest, yDest );
    SelectClipRgn( m_hDC, mask );

    BLENDFUNCTION bf;      // structure for alpha blending
    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 0xff;  // don't use constant alpha
    bf.AlphaFormat = AC_SRC_ALPHA;

    // Blend the image onto the internal DC
    if( !AlphaBlend( m_hDC, xDest, yDest, width, height, hDC, 0, 0,
                     width, height, bf ) )
    {
        msg_Err( getIntf(), "AlphaBlend() failed" );
    }

    // Add the bitmap mask to the global graphics mask
    CombineRgn( m_mask, m_mask, mask, RGN_OR );

    // Do cleanup
    DeleteObject( hBmp );
    DeleteObject( mask );
    DeleteDC( hDC );
}


void Win32Graphics::drawGraphics( const OSGraphics &rGraphics, int xSrc,
                                  int ySrc, int xDest, int yDest, int width,
                                  int height )
{
    // check and adapt to source if needed
    if( !checkBoundaries( 0, 0, rGraphics.getWidth(), rGraphics.getHeight(),
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

    // Create the mask for transparency
    HRGN mask = CreateRectRgn( xSrc, ySrc, xSrc + width, ySrc + height );
    CombineRgn( mask, ((Win32Graphics&)rGraphics).getMask(), mask, RGN_AND );
    OffsetRgn( mask, xDest - xSrc, yDest - ySrc );

    // Copy the image
    HDC srcDC = ((Win32Graphics&)rGraphics).getDC();
    SelectClipRgn( m_hDC, mask );
    BitBlt( m_hDC, xDest, yDest, width, height, srcDC, xSrc, ySrc, SRCCOPY );

    // Add the source mask to the mask of the graphics
    CombineRgn( m_mask, mask, m_mask, RGN_OR );
    DeleteObject( mask );
}


void Win32Graphics::fillRect( int left, int top, int width, int height,
                              uint32_t color )
{
    // Update the mask with the rectangle area
    HRGN newMask = CreateRectRgn( left, top, left + width, top + height );
    CombineRgn( m_mask, m_mask, newMask, RGN_OR );
    SelectClipRgn( m_hDC, m_mask );
    DeleteObject( newMask );

    // Create a brush with the color
    int red = (color & 0xff0000) >> 16;
    int green = (color & 0xff00) >> 8;
    int blue = color & 0xff;
    HBRUSH hBrush = CreateSolidBrush( RGB( red, green, blue ) );

    // Draw the rectangle
    RECT r;
    r.left = left;
    r.top = top;
    r.right = left + width;
    r.bottom = top + height;
    FillRect( m_hDC, &r, hBrush );
    DeleteObject( hBrush );
}


void Win32Graphics::drawRect( int left, int top, int width, int height,
                              uint32_t color )
{
    // Update the mask with the rectangle
    HRGN l1 = CreateRectRgn( left, top, left + width, top + 1 );
    HRGN l2 = CreateRectRgn( left + width - 1, top,
                             left + width, top + height );
    HRGN l3 = CreateRectRgn( left, top + height - 1,
                             left + width, top + height );
    HRGN l4 = CreateRectRgn( left, top, left + 1, top + height );
    CombineRgn( m_mask, m_mask, l1, RGN_OR );
    CombineRgn( m_mask, m_mask, l2, RGN_OR );
    CombineRgn( m_mask, m_mask, l3, RGN_OR );
    CombineRgn( m_mask, m_mask, l4, RGN_OR );
    DeleteObject( l1 );
    DeleteObject( l2 );
    DeleteObject( l3 );
    DeleteObject( l4 );

    SelectClipRgn( m_hDC, m_mask );

    // Create a pen with the color
    int red = (color & 0xff0000) >> 16;
    int green = (color & 0xff00) >> 8;
    int blue = color & 0xff;
    HPEN hPen = CreatePen( PS_SOLID, 0, RGB( red, green, blue ) );
    SelectObject( m_hDC, hPen );

    // Draw the rectangle
    MoveToEx( m_hDC, left, top, NULL );
    LineTo( m_hDC, left + width - 1, top );
    LineTo( m_hDC, left + width - 1, top + height - 1 );
    LineTo( m_hDC, left, top + height - 1 );
    LineTo( m_hDC, left, top );

    // Delete the pen
    DeleteObject( hPen );
}


void Win32Graphics::applyMaskToWindow( OSWindow &rWindow )
{
    // Get window handle
    HWND hWnd = ((Win32Window&)rWindow).getHandle();

    // Apply the mask
    // We need to copy the mask, because SetWindowRgn modifies it in our back
    HRGN mask = CreateRectRgn( 0, 0, 0, 0 );
    CombineRgn( mask, m_mask, NULL, RGN_COPY );
    SetWindowRgn( hWnd, mask, TRUE );
}


void Win32Graphics::copyToWindow( OSWindow &rWindow, int xSrc, int ySrc,
                                  int width, int height, int xDest, int yDest )
{
    // Initialize painting
    HWND hWnd = ((Win32Window&)rWindow).getHandle();
    HDC wndDC = GetDC( hWnd );
    HDC srcDC = m_hDC;

    // Draw image on window
    BitBlt( wndDC, xDest, yDest, width, height, srcDC, xSrc, ySrc, SRCCOPY );

    // Release window device context
    ReleaseDC( hWnd, wndDC );
}


bool Win32Graphics::hit( int x, int y ) const
{
    return PtInRegion( m_mask, x, y ) != 0;
}


void Win32Graphics::addSegmentInRegion( HRGN &rMask, int start,
                                        int end, int line )
{
    HRGN buffer = CreateRectRgn( start, line, end, line + 1 );
    CombineRgn( rMask, buffer, rMask, RGN_OR );
    DeleteObject( buffer );
}


bool Win32Graphics::checkBoundaries( int x_src, int y_src,
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
