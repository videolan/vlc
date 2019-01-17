/*****************************************************************************
 * ft2_font.hpp
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef FT2_FONT_HPP
#define FT2_FONT_HPP

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include <string>
#include <map>

#include "generic_font.hpp"

class UString;


/// Freetype2 font
class FT2Font: public GenericFont
{
public:
    FT2Font( intf_thread_t *pIntf, const std::string &rName, int size );
    virtual ~FT2Font();

    /// Initialize the object. Returns false if it failed
    virtual bool init();

    /// Render a string on a bitmap.
    /// If maxWidth != -1, the text is truncated with '...'
    virtual GenericBitmap *drawString( const UString &rString,
        uint32_t color, int maxWidth = -1 ) const;

    /// Get the text height
    virtual int getSize() const { return m_height; }

private:
    typedef struct
    {
        FT_Glyph m_glyph;
        FT_BBox m_size;
        int m_index;
        int m_advance;
    } Glyph_t;
    typedef std::map<uint32_t,Glyph_t> GlyphMap_t;

    /// File name
    const std::string m_name;
    /// Buffer to store the font
    char *m_buffer;
    /// Pixel size of the font
    int m_size;
    /// Handle to FT library
    FT_Library m_lib;
    /// Font face
    FT_Face m_face;
    /// Font metrics
    int m_height, m_ascender, m_descender;
    /// Glyph cache
    mutable GlyphMap_t m_glyphCache;

    /// Get the glyph corresponding to the given code
    Glyph_t &getGlyph( uint32_t code ) const;
    bool error( unsigned err, const char *msg );
};


#endif
