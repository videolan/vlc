/*****************************************************************************
 * os2_graphics.cpp
 *****************************************************************************
 * Copyright (C) 2003, 2013 the VideoLAN team
 *
 * Authors: Cyril Deguet      <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
 *          KO Myung-Hun      <komh@chollian.net>
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

#ifdef OS2_SKINS

#include "os2_factory.hpp"
#include "os2_graphics.hpp"
#include "os2_window.hpp"
#include "../src/generic_bitmap.hpp"

// kLIBC does not declare this function
extern "C" BOOL APIENTRY WinSetClipRegion( HWND, HRGN );

OS2Graphics::OS2Graphics( intf_thread_t *pIntf, int width, int height ):
    OSGraphics( pIntf ), m_width( width ), m_height( height )
{
    m_hdc = DevOpenDC( 0, OD_MEMORY, "*", 0L, NULL, NULLHANDLE );

    SIZEL sizl = { 0, 0 };
    m_hps = GpiCreatePS( 0, m_hdc, &sizl,
                         PU_PELS | GPIF_DEFAULT | GPIT_MICRO | GPIA_ASSOC );

    // Set the color table into RGB mode
    GpiCreateLogColorTable( m_hps, 0, LCOLF_RGB, 0, 0, NULL );

    BITMAPINFOHEADER bmp;
    bmp.cbFix     = sizeof( bmp );
    bmp.cx        = width;
    bmp.cy        = height;
    bmp.cPlanes   = 1;
    bmp.cBitCount = 24;

    m_hbm = GpiCreateBitmap( m_hps, ( PBITMAPINFOHEADER2 )&bmp, 0L,
                             NULL, NULL);

    GpiSetBitmap( m_hps, m_hbm );

    // Create the mask
    RECTL rcl = { 0, 0, 0, 0 };
    m_mask = GpiCreateRegion( m_hps, 1, &rcl );
}


OS2Graphics::~OS2Graphics()
{
    GpiDestroyRegion( m_hps, m_mask );

    GpiSetBitmap( m_hps, NULLHANDLE );
    GpiDeleteBitmap( m_hbm );

    GpiDestroyPS( m_hps );

    DevCloseDC( m_hdc );
}


void OS2Graphics::clear( int xDest, int yDest, int width, int height )
{
    if( width <= 0 || height <= 0 )
    {
        // Clear the transparency mask
        GpiDestroyRegion( m_hps, m_mask );
        RECTL rcl = { 0, 0, 0, 0 };
        m_mask = GpiCreateRegion( m_hps, 1, &rcl );
    }
    else
    {
        RECTL rcl = { xDest, invertRectY( yDest + height ),
                      xDest + width, invertRectY( yDest )};
        HRGN mask = GpiCreateRegion( m_hps, 1, &rcl );
        GpiCombineRegion( m_hps, m_mask, m_mask, mask, CRGN_DIFF );
        GpiDestroyRegion( m_hps, mask );
    }
}


void OS2Graphics::drawBitmap( const GenericBitmap &rBitmap,
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

    BITMAPINFOHEADER bmp;
    bmp.cbFix     = sizeof( bmp );
    bmp.cx        = width;
    bmp.cy        = height;
    bmp.cPlanes   = 1;
    bmp.cBitCount = 24;

    // pointer to bitmap bits
    uint8_t *pBits = ( uint8_t * )malloc((( bmp.cBitCount * bmp.cx + 31 ) /
                                         32 ) * bmp.cPlanes * 4 );

    HDC hdc = DevOpenDC( 0, OD_MEMORY, "*", 0L, NULL, NULLHANDLE );

    SIZEL sizl = { 0, 0 };
    HPS hps = GpiCreatePS( 0, hdc, &sizl,
                           PU_PELS | GPIF_DEFAULT | GPIT_MICRO | GPIA_ASSOC );

    HBITMAP hbm = GpiCreateBitmap( hps, ( PBITMAPINFOHEADER2 )&bmp, 0L,
                                   NULL, NULL);

    GpiSetBitmap( hps, hbm );

    // Mask for transparency
    RECTL rcl = { 0, 0, 0, 0 };
    HRGN  mask = GpiCreateRegion( m_hps, 1, &rcl );

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
            uint8_t b = *( pBmpData++ );
            uint8_t g = *( pBmpData++ );
            uint8_t r = *( pBmpData++ );
            uint8_t a = *( pBmpData++ );

            // Draw the pixel
            pBits[ x * 3     ] = b;
            pBits[ x * 3 + 1 ] = g;
            pBits[ x * 3 + 2 ] = r;

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
                    addSegmentInRegion( mask, visibleSegmentStart, x, y,
                                        height );
                }
                wasVisible = false;
            }
        }

        // Transfer bitmap bits to hps
        GpiSetBitmapBits( hps, invertPointY( y, height ), 1, ( PBYTE )pBits,
                          ( PBITMAPINFO2 )&bmp );

        if( wasVisible )
        {
            // End of a visible segment: add it to the mask
            addSegmentInRegion( mask, visibleSegmentStart, width, y, height );
        }
        // Skip uninteresting bytes at the end of the line
        pBmpData += 4 * (rBitmap.getWidth() - width - xSrc);
    }

    // Apply the mask to the internal DC
    POINTL ptl = { xDest, invertRectY( yDest + height )};
    GpiOffsetRegion( m_hps, mask, &ptl );

    GpiSetClipRegion( m_hps, mask, NULL );

    POINTL aptl[] = {{ xDest, invertRectY( yDest + height )},
                     { xDest + width, invertRectY( yDest )},
                     { 0, 0 }};
    GpiBitBlt( m_hps, hps, 3, aptl, ROP_SRCCOPY, BBO_IGNORE );

    // Release a clip region handle
    GpiSetClipRegion( m_hps, NULLHANDLE, NULL );

    // Add the bitmap mask to the global graphics mask
    GpiCombineRegion( m_hps, m_mask, m_mask, mask, CRGN_OR );

    // Do cleanup
    GpiDestroyRegion( m_hps, mask );
    GpiSetBitmap( hps, NULLHANDLE );
    GpiDeleteBitmap( hbm );
    GpiDestroyPS( hps );
    DevCloseDC( hdc );
    free( pBits );
}


void OS2Graphics::drawGraphics( const OSGraphics &rGraphics, int xSrc,
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

    // To invert Y
    int h = rGraphics.getHeight();

    // Create the mask for transparency
    RECTL rcl = { xSrc, invertRectY( ySrc + height, h ),
                  xSrc + width, invertRectY( ySrc, h )};
    HRGN  mask = GpiCreateRegion( m_hps, 1, &rcl );
    GpiCombineRegion( m_hps, mask, ((OS2Graphics&)rGraphics).getMask(), mask,
                      CRGN_AND );

    POINTL ptl = { xDest - xSrc,
                   invertRectY( yDest + height ) -
                   invertRectY( ySrc + height, h )};
    GpiOffsetRegion( m_hps, mask, &ptl );

    // Copy the image
    HPS hpsSrc = ((OS2Graphics&)rGraphics).getPS();

    GpiSetClipRegion( m_hps, mask, NULL );

    POINTL aptl[] = {{ xDest, invertRectY( yDest + height )},
                     { xDest + width, invertRectY( yDest )},
                     { xSrc, invertRectY( ySrc + height, h )}};
    GpiBitBlt( m_hps, hpsSrc, 3, aptl, ROP_SRCCOPY, BBO_IGNORE );

    // Release a clip region handle
    GpiSetClipRegion( m_hps, NULLHANDLE, NULL );

    // Add the source mask to the mask of the graphics
    GpiCombineRegion( m_hps, m_mask, mask, m_mask, CRGN_OR );
    GpiDestroyRegion( m_hps, mask );
}


void OS2Graphics::fillRect( int left, int top, int width, int height,
                            uint32_t color )
{
    // Update the mask with the rectangle area
    int bottom = invertRectY( top + height );
    RECTL rcl = { left, bottom, left + width, bottom + height };
    HRGN  newMask = GpiCreateRegion( m_hps, 1, &rcl );
    GpiCombineRegion( m_hps, m_mask, m_mask, newMask, CRGN_OR );

    GpiSetClipRegion( m_hps, m_mask, NULL );
    GpiDestroyRegion( m_hps, newMask );

    // Draw the rectangle
    WinSetRect( 0, &rcl, left, bottom, left + width, bottom + height );
    WinFillRect( m_hps, &rcl, color );

    // Release a clip region handle
    GpiSetClipRegion( m_hps, NULLHANDLE, NULL );
}


void OS2Graphics::drawRect( int left, int top, int width, int height,
                            uint32_t color )
{
    // Update the mask with the rectangle
    int bottom = invertRectY( top + height );
    RECTL rcl = { left, bottom, left + width, bottom + 1 };
    HRGN  l1 = GpiCreateRegion( m_hps, 1, &rcl );

    WinSetRect( 0, &rcl, left + width - 1, bottom, left + width,
                bottom + height );
    HRGN l2 = GpiCreateRegion( m_hps, 1, &rcl );

    WinSetRect( 0, &rcl, left, bottom + height - 1, left + width,
                bottom + height );
    HRGN l3 = GpiCreateRegion( m_hps, 1, &rcl );

    WinSetRect( 0, &rcl, left, bottom, left + 1, bottom + height );
    HRGN l4 = GpiCreateRegion( m_hps, 1, &rcl );

    GpiCombineRegion( m_hps, m_mask, m_mask, l1, CRGN_OR );
    GpiCombineRegion( m_hps, m_mask, m_mask, l2, CRGN_OR );
    GpiCombineRegion( m_hps, m_mask, m_mask, l3, CRGN_OR );
    GpiCombineRegion( m_hps, m_mask, m_mask, l4, CRGN_OR );

    GpiDestroyRegion( m_hps, l1 );
    GpiDestroyRegion( m_hps, l2 );
    GpiDestroyRegion( m_hps, l3 );
    GpiDestroyRegion( m_hps, l4 );

    GpiSetClipRegion( m_hps, m_mask, NULL );

    // Set color
    GpiSetColor( m_hps, color );

    // Draw the rectangle
    POINTL ptl = { left, bottom };
    GpiMove( m_hps, &ptl );

    ptl.x = left + width - 1;
    ptl.y = bottom;
    GpiLine( m_hps, &ptl );

    ptl.x = left + width - 1;
    ptl.y = bottom + height - 1;
    GpiLine( m_hps, &ptl );

    ptl.x = left;
    ptl.y = bottom + height - 1;
    GpiLine( m_hps, &ptl );

    ptl.x = left;
    ptl.y = bottom;
    GpiLine( m_hps, &ptl );

    // Release a clip region handle
    GpiSetClipRegion( m_hps, NULLHANDLE, NULL );
}


void OS2Graphics::applyMaskToWindow( OSWindow &rWindow )
{
    // Get window handle
    HWND hWnd = ((OS2Window&)rWindow).getHandle();

    // Apply the mask
    RECTL rcl = { 0, 0, 0, 0 };
    HRGN mask = GpiCreateRegion( m_hps, 1, &rcl );
    GpiCombineRegion( m_hps, mask, m_mask, NULLHANDLE, CRGN_COPY );
    //WinSetClipRegion( hWnd, mask );
    GpiDestroyRegion( m_hps, mask );
}


void OS2Graphics::copyToWindow( OSWindow &rWindow, int xSrc, int ySrc,
                                int width, int height, int xDest, int yDest )
{
    // Initialize painting
    HWND hWnd = ((OS2Window&)rWindow).getHandle();
    HPS hpsWnd = WinGetPS( hWnd );
    HPS hpsSrc = m_hps;

    RECTL rcl;
    WinQueryWindowRect( hWnd, &rcl );

    // Draw image on window
    POINTL aptl[] = {{ xDest, invertRectY( yDest + height, rcl.yTop )},
                     { xDest + width, invertRectY( yDest, rcl.yTop )},
                     { xSrc, invertRectY( ySrc + height )}};
    GpiBitBlt( hpsWnd, hpsSrc, 3, aptl, ROP_SRCCOPY, BBO_IGNORE );

    // Release window device context
    WinReleasePS( hpsWnd );
}


bool OS2Graphics::hit( int x, int y ) const
{
    POINTL ptl = { x, invertPointY( y )};
    return GpiPtInRegion( m_hps, m_mask, &ptl ) == PRGN_INSIDE;
}


void OS2Graphics::addSegmentInRegion( HRGN &rMask, int start,
                                      int end, int line, int height )
{
    RECTL rcl = { start, invertRectY( line + 1, height ),
                  end, invertRectY( line, height )};
    HRGN buffer = GpiCreateRegion( m_hps, 1, &rcl );
    GpiCombineRegion( m_hps, rMask, buffer, rMask, CRGN_OR );
    GpiDestroyRegion( m_hps, buffer );
}


bool OS2Graphics::checkBoundaries( int x_src, int y_src,
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
