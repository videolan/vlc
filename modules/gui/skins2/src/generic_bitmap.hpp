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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef GENERIC_BITMAP_HPP
#define GENERIC_BITMAP_HPP

#include "skin_common.hpp"
#include "../utils/pointer.hpp"


/// Generic interface for bitmaps
class GenericBitmap: public SkinObject
{
    public:
        virtual ~GenericBitmap() {}

        /// Get the width of the bitmap
        virtual int getWidth() const = 0;

        /// Get the heighth of the bitmap
        virtual int getHeight() const = 0;

        /// Get a linear buffer containing the image data.
        /// Each pixel is stored in 4 bytes in the order B,G,R,A
        virtual uint8_t *getData() const = 0;

    protected:
        GenericBitmap( intf_thread_t *pIntf ): SkinObject( pIntf ) {}
};


/// Basic bitmap implementation
class BitmapImpl: public GenericBitmap
{
    public:
        /// Create an empty bitmap of the given size
        BitmapImpl( intf_thread_t *pIntf, int width, int height );
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
