/*****************************************************************************
 * bitmap_font.hpp
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef BITMAP_FONT_HPP
#define BITMAP_FONT_HPP

#include "generic_font.hpp"

class GenericBitmap;


/// Class to handle bitmap fonts
class BitmapFont: public GenericFont
{
    public:
        BitmapFont( intf_thread_t *pIntf, const GenericBitmap &rBitmap );
        virtual ~BitmapFont() {}

        virtual bool init() { return true; }

        /// Render a string on a bitmap.
        /// If maxWidth != -1, the text is truncated with '...'
        virtual GenericBitmap *drawString( const UString &rString,
            uint32_t color, int maxWidth = -1 ) const;

        /// Get the font size
        virtual int getSize() const { return 12; }

    private:
        /// Bitmap
        const GenericBitmap &m_rBitmap;
        /// Glyph size
        int m_width, m_height;
};

#endif
