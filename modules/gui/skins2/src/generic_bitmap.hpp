/*****************************************************************************
 * generic_bitmap.hpp
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

#ifndef GENERIC_BITMAP_HPP
#define GENERIC_BITMAP_HPP

#include "skin_common.hpp"
#include "../utils/pointer.hpp"
#include "../utils/position.hpp"


/// Generic interface for bitmaps
class GenericBitmap: public SkinObject, public Box
{
    public:
        virtual ~GenericBitmap() {}

        /// Get a linear buffer containing the image data.
        /// Each pixel is stored in 4 bytes in the order B,G,R,A
        virtual uint8_t *getData() const = 0;

        /// Get the number of frames in the bitmap
        int getNbFrames() const { return m_nbFrames; }

        /// Get the number of frames per second (for animated bitmaps)
        int getFrameRate() const { return m_frameRate; }

    protected:
        GenericBitmap( intf_thread_t *pIntf, int nbFrames = 1, int fps = 0);

    private:
        /// Number of frames
        int m_nbFrames;
        /// Frame rate
        int m_frameRate;
};


/// Basic bitmap implementation
class BitmapImpl: public GenericBitmap
{
    public:
        /// Create an empty bitmap of the given size
        BitmapImpl( intf_thread_t *pIntf, int width, int height,
                    int nbFrames = 1, int fps = 0 );
        ~BitmapImpl();

        /// Get the width of the bitmap
        virtual int getWidth() const { return m_width; }

        /// Get the heighth of the bitmap
        virtual int getHeight() const { return m_height; }

        /// Get a linear buffer containing the image data.
        /// Each pixel is stored in 4 bytes in the order B,G,R,A
        virtual uint8_t *getData() const { return m_pData; }

        // Copy a region of another bitmap on this bitmap
        bool drawBitmap( const GenericBitmap &rSource, int xSrc, int ySrc,
                         int xDest, int yDest, int width, int height );

    private:
        /// Size of the bitmap.
        int m_width, m_height;
        /// Buffer containing the image data.
        uint8_t *m_pData;
};


typedef CountedPtr<GenericBitmap> GenericBitmapPtr;

#endif
