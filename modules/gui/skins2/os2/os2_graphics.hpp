/*****************************************************************************
 * os2_graphics.hpp
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef OS2_GRAPHICS_HPP
#define OS2_GRAPHICS_HPP

#include "../src/os_graphics.hpp"


class GenericBitmap;

/// OS2 implementation of OSGraphics
class OS2Graphics: public OSGraphics
{
public:
    OS2Graphics( intf_thread_t *pIntf, int width, int height );
    virtual ~OS2Graphics();

    /// Clear the graphics
    virtual void clear( int xDest = 0, int yDest = 0,
                        int width = -1, int height = -1 );

    /// Render a bitmap on this graphics
    virtual void drawBitmap( const GenericBitmap &rBitmap, int xSrc = 0,
                             int ySrc = 0, int xDest = 0, int yDest = 0,
                             int width = -1, int height = -1,
                             bool blend = false );

    /// Draw another graphics on this one
    virtual void drawGraphics( const OSGraphics &rGraphics, int xSrc = 0,
                               int ySrc = 0, int xDest = 0, int yDest = 0,
                               int width = -1, int height = -1 );

    /// Draw an empty rectangle on the grahics (color is #RRGGBB)
    virtual void drawRect( int left, int top, int width, int height,
                           uint32_t color );


    /// Draw a filled rectangle on the grahics (color is #RRGGBB)
    virtual void fillRect( int left, int top, int width, int height,
                           uint32_t color );

    /// Set the shape of a window with the mask of this graphics.
    virtual void applyMaskToWindow( OSWindow &rWindow );

    /// Copy the graphics on a window
    virtual void copyToWindow( OSWindow &rWindow, int xSrc,
                               int ySrc, int width, int height,
                               int xDest, int yDest );

    /// Tell whether the pixel at the given position is visible
    virtual bool hit( int x, int y ) const;

    /// Getters for the size
    virtual int getWidth() const { return m_width; }
    virtual int getHeight() const { return m_height; }

    /// Get the device context handler
    virtual HDC getPS() const { return m_hps; }

    /// Get the mask
    virtual HRGN getMask() const { return m_mask; }

private:
    /// Size of the image
    int m_width, m_height;

    /// Device context
    HDC m_hdc;

    /// Presentaiton space
    HPS m_hps;

    /// Bitmap handle
    HBITMAP m_hbm;

    /// Transparency mask
    HRGN m_mask;

    /// Add a segment in a region
    void addSegmentInRegion( HRGN &rMask, int start, int end, int line,
                             int height );

    /// check boundaries for graphics and bitmaps
    bool checkBoundaries( int x_src, int y_src, int w_src, int h_src,
                          int& x_target, int& y_target,
                          int& w_target, int& h_target );

    /// invert Y of a point against the given height
    int invertPointY( int y, int height ) const
    {
        return ( height - 1 ) - y;
    }

    /// invert Y of a point against the m_height
    int invertPointY( int y ) const
    {
        return ( m_height - 1 ) - y;
    }

    /// invert Y of a rectangular against the given height
    int invertRectY( int y, int height ) const
    {
        // bottom is inclusive, top is exclusive
        // the following is the short of ( height - 1 ) - ( y - 1 )
        return height - y;
    }

    /// invert Y of a rectangular against the m_height
    int invertRectY( int y ) const
    {
        // bottom is inclusive, top is exclusive
        // the following is the short of ( m_height - 1 ) - ( y - 1 )
        return m_height - y;
    }
};


#endif
