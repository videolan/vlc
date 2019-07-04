/*****************************************************************************
 * ft2_font.cpp
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

#include <errno.h>
#include "ft2_font.hpp"
#include "ft2_bitmap.hpp"
#include "ft2_err.h"
#include "../utils/ustring.hpp"

#ifdef HAVE_FRIBIDI
# include <fribidi.h>
#endif


FT2Font::FT2Font( intf_thread_t *pIntf, const std::string &rName, int size ):
    GenericFont( pIntf ), m_name( rName ), m_buffer( NULL ), m_size( size ),
    m_lib( NULL ), m_face( NULL )
{
}


FT2Font::~FT2Font()
{
    GlyphMap_t::iterator iter;
    for( iter = m_glyphCache.begin(); iter != m_glyphCache.end(); ++iter )
        FT_Done_Glyph( (*iter).second.m_glyph );
    if( m_face ) FT_Done_Face( m_face );
    if( m_lib )  FT_Done_FreeType( m_lib );
    delete[] m_buffer;
}


bool FT2Font::init()
{
    unsigned err;

    if( ( err = FT_Init_FreeType( &m_lib ) ) )
    {
        msg_Err( getIntf(), "failed to initialize freetype (%s)",
                 ft2_strerror( err ) );
        return false;
    }

    FILE *file = vlc_fopen( m_name.c_str(), "rb" );
    if( !file )
    {
        msg_Dbg( getIntf(), "failed to open font %s (%s)",
                 m_name.c_str(), strerror(errno) );
        return false;
    }
    msg_Dbg( getIntf(), "loading font %s", m_name.c_str() );

    fseek( file, 0, SEEK_END );
    long size = ftell( file );
    rewind( file );

    if( -1==size )
    {
        msg_Dbg( getIntf(), "fseek loading font %s (%s)",
                 m_name.c_str(), strerror(errno) );
        fclose( file );
        return false;
    }

    m_buffer = new (std::nothrow) char[size];
    if( !m_buffer )
    {
        fclose( file );
        return false;
    }

    if( fread( m_buffer, size, 1, file ) != 1 )
    {
        msg_Err( getIntf(), "unexpected result for read" );
        fclose( file );
        return false;
    }
    fclose( file );

    err = FT_New_Memory_Face( m_lib, (const FT_Byte*)m_buffer, size, 0,
                              &m_face );
    if ( err == FT_Err_Unknown_File_Format )
    {
        msg_Err( getIntf(), "unsupported font format (%s)", m_name.c_str() );
        return false;
    }
    else if ( err )
    {
        msg_Err( getIntf(), "error opening font %s (%s)",
                 m_name.c_str(), ft2_strerror(err) );
        return false;
    }

    // Select the charset
    if( ( err = FT_Select_Charmap( m_face, ft_encoding_unicode ) ) )
    {
        msg_Err( getIntf(), "font %s has no UNICODE table (%s)",
                 m_name.c_str(), ft2_strerror(err) );
        return false;
    }

    // Set the pixel size
    if( ( err = FT_Set_Pixel_Sizes( m_face, 0, m_size ) ) )
    {
        msg_Warn( getIntf(), "cannot set a pixel size of %d for %s (%s)",
                  m_size, m_name.c_str(), ft2_strerror(err) );
    }

    // Get the font metrucs
    m_height = m_face->size->metrics.height >> 6;
    m_ascender = m_face->size->metrics.ascender >> 6;
    m_descender = m_face->size->metrics.descender >> 6;

    return true;
}


GenericBitmap *FT2Font::drawString( const UString &rString, uint32_t color,
                                    int maxWidth ) const
{
    uint32_t code;
    int n;
    int penX = 0;
    int width1 = 0, width2 = 0;
    int yMin = 0, yMax = 0;
    uint32_t *pString = (uint32_t*)rString.u_str();

    // Check if freetype has been initialized
    if( !m_face )
    {
        return NULL;
    }

    // Get the length of the string
    int len = rString.length();

    // Use fribidi if available
#ifdef HAVE_FRIBIDI
    uint32_t *pFribidiString = NULL;
    if( len > 0 )
    {
        pFribidiString = new uint32_t[len+1];
        FriBidiCharType baseDir = FRIBIDI_TYPE_ON;
        FriBidiLevel level = fribidi_log2vis(
                        (FriBidiChar*)pString, len, &baseDir,
                        (FriBidiChar*)pFribidiString, 0, 0, 0 );
        (void)level;
        pString = pFribidiString;
    }
#endif

    // Array of glyph bitmaps and position
    FT_BitmapGlyphRec **glyphs = new FT_BitmapGlyphRec*[len];
    int *pos = new int[len];

    // Does the font support kerning ?
    FT_Bool useKerning = FT_HAS_KERNING( m_face );
    int previous = 0;

    // Index of the last glyph when the text is truncated with trailing ...
    int maxIndex = 0;
    // Position of the first trailing dot
    int firstDotX = 0;
    /// Get the dot glyph
    Glyph_t &dotGlyph = getGlyph( '.' );

    // First, render all the glyphs
    for( n = 0; n < len; n++ )
    {
        code = *(pString++);
        // Get the glyph for this character
        Glyph_t &glyph = getGlyph( code );
        glyphs[n] = (FT_BitmapGlyphRec*)(glyph.m_glyph);

        // Retrieve kerning distance and move pen position
        if( useKerning && previous && glyph.m_index )
        {
            FT_Vector delta;
            FT_Get_Kerning( m_face, previous, glyph.m_index,
                            ft_kerning_default, &delta );
            penX += delta.x >> 6;
        }

        pos[n] = penX;
        width1 = penX + glyph.m_size.xMax - glyph.m_size.xMin;
        yMin = __MIN( yMin, glyph.m_size.yMin );
        yMax = __MAX( yMax, glyph.m_size.yMax );

        // Next position
        penX += glyph.m_advance;

        // Save glyph index
        previous = glyph.m_index;

        if( maxWidth != -1 )
        {
            // Check if the truncated text with the '...' fit in the maxWidth
            int curX = penX;
            if( useKerning )
            {
                FT_Vector delta;
                FT_Get_Kerning( m_face, glyph.m_index, dotGlyph.m_index,
                                ft_kerning_default, &delta );
                curX += delta.x >> 6;
            }
            int dotWidth = 2 * dotGlyph.m_advance +
                dotGlyph.m_size.xMax - dotGlyph.m_size.xMin;
            if( curX + dotWidth < maxWidth )
            {
                width2 = curX + dotWidth;
                maxIndex++;
                firstDotX = curX;
            }
        }
        else
        {
            // No check
            width2 = width1;
            maxIndex++;
        }

        // Stop here if the text is too large
        if( maxWidth != -1 && width1 > maxWidth )
        {
            break;
        }
    }

#ifdef HAVE_FRIBIDI
    if( len > 0 )
    {
        delete[] pFribidiString;
    }
#endif

    // Adjust the size for vertical padding
    yMax = __MAX( yMax, m_ascender );
    yMin = __MIN( yMin, m_descender );

    // Create the bitmap
    FT2Bitmap *pBmp = new FT2Bitmap( getIntf(), __MIN( width1, width2 ),
                                     yMax - yMin );

    // Draw the glyphs on the bitmap
    for( n = 0; n < maxIndex; n++ )
    {
        FT_BitmapGlyphRec *pBmpGlyph = (FT_BitmapGlyphRec*)glyphs[n];
        // Draw the glyph on the bitmap
        pBmp->draw( pBmpGlyph->bitmap, pos[n], yMax - pBmpGlyph->top, color );
    }
    // Draw the trailing dots if the text is truncated
    if( maxIndex < len )
    {
        int penX = firstDotX;
        FT_BitmapGlyphRec *pBmpGlyph = (FT_BitmapGlyphRec*)dotGlyph.m_glyph;
        for( n = 0; n < 3; n++ )
        {
            // Draw the glyph on the bitmap
            pBmp->draw( pBmpGlyph->bitmap, penX, yMax - pBmpGlyph->top,
                        color );
            penX += dotGlyph.m_advance;
        }
    }

    delete [] glyphs;
    delete [] pos;

    return pBmp;
}


FT2Font::Glyph_t &FT2Font::getGlyph( uint32_t code ) const
{
    // Try to find the glyph in the cache
    GlyphMap_t::iterator iter = m_glyphCache.find( code );
    if( iter != m_glyphCache.end() )
    {
        return (*iter).second;
    }
    else
    {
        // Add a new glyph in the cache
        Glyph_t &glyph = m_glyphCache[code];

        // Load and render the glyph
        glyph.m_index = FT_Get_Char_Index( m_face, code );
        FT_Load_Glyph( m_face, glyph.m_index, FT_LOAD_DEFAULT );
        FT_Get_Glyph( m_face->glyph, &glyph.m_glyph );
        FT_Glyph_Get_CBox( glyph.m_glyph, ft_glyph_bbox_pixels,
                           &glyph.m_size );
        glyph.m_advance = m_face->glyph->advance.x >> 6;
        FT_Glyph_To_Bitmap( &glyph.m_glyph, ft_render_mode_normal, NULL, 1 );
        return glyph;
    }
}

