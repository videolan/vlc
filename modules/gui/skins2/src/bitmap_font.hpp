/*****************************************************************************
 * bitmap_font.hpp
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef BITMAP_FONT_HPP
#define BITMAP_FONT_HPP

#include "generic_font.hpp"
#include <string>

class GenericBitmap;


/// Class to handle bitmap fonts
class BitmapFont: public GenericFont
{
public:
    BitmapFont( intf_thread_t *pIntf, const GenericBitmap &rBitmap,
                const string &rType );
    virtual ~BitmapFont() { }

    virtual bool init() { return true; }

    /// Render a string on a bitmap.
    /// If maxWidth != -1, the text is truncated with '...'
    virtual GenericBitmap *drawString( const UString &rString,
        uint32_t color, int maxWidth = -1 ) const;

    /// Get the font size
    virtual int getSize() const { return m_height; }

private:
    /// Description of a glyph
    struct Glyph_t
    {
        Glyph_t(): m_xPos( -1 ), m_yPos( 0 ) { }
        int m_xPos, m_yPos;
    };

    /// Bitmap
    const GenericBitmap &m_rBitmap;
    /// Glyph size
    int m_width, m_height;
    /// Horizontal advance between two characters
    int m_advance;
    /// Horizontal advance for non-displayable characters
    int m_skip;
    /// Character table
    Glyph_t m_table[256];
};

#endif
