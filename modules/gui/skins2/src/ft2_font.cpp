/*****************************************************************************
 * ft2_font.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
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

#include "ft2_font.hpp"
#include "ft2_bitmap.hpp"
#include "../utils/ustring.hpp"

#ifdef HAVE_FRIBIDI
#include <fribidi/fribidi.h>
#endif


FT2Font::FT2Font( intf_thread_t *pIntf, const string &rName, int size ):
    GenericFont( pIntf ), m_name( rName ), m_buffer( NULL ), m_size( size ),
    m_lib( NULL ), m_face( NULL ), m_dotGlyph( NULL )
{
}


FT2Font::~FT2Font()
{
    if( m_dotGlyph )
    {
        FT_Done_Glyph( m_dotGlyph );
    }
    if( m_face )
    {
        FT_Done_Face( m_face );
    }
    if( m_lib )
    {
        FT_Done_FreeType( m_lib );
    }
    if( m_buffer )
    {
        free( m_buffer );
    }
}


bool FT2Font::init()
{
    int err;

    // Initalise libfreetype
    if( FT_Init_FreeType( &m_lib ) )
    {
        msg_Err( getIntf(), "Failed to initalize libfreetype" );
        return false;
    }

    // Open the font
    FILE *file = fopen( m_name.c_str(), "rb" );
    if( file )
    {
        msg_Dbg( getIntf(), "Loading font %s", m_name.c_str() );
    }
    else
    {
        msg_Dbg( getIntf(), "Unable to open the font %s", m_name.c_str() );
        return false;
    }
    // Get the file size
    fseek( file, 0, SEEK_END );
    int size = ftell( file );
    rewind( file );
    // Allocate the buffer
    m_buffer = malloc( size );
    if( !m_buffer )
    {
        msg_Err( getIntf(), "Not enough memory for the font %s",
                 m_name.c_str() );
        return false;
    }
    // Read the font data
    fread( m_buffer, size, 1, file );
    fclose( file );

    // Load the font from the buffer
    err = FT_New_Memory_Face( m_lib, (const FT_Byte*)m_buffer, size, 0,
                              &m_face );
    if ( err == FT_Err_Unknown_File_Format )
    {
        msg_Err( getIntf(), "Unsupported font format (%s)", m_name.c_str() );
        return false;
    }
    else if ( err )
    {
        msg_Err( getIntf(), "Error opening font (%s)", m_name.c_str() );
        return false;
    }

    // Select the charset
    if( FT_Select_Charmap( m_face, ft_encoding_unicode ) )
    {
        msg_Err( getIntf(), "Font has no UNICODE table (%s)", m_name.c_str() );
        return false;
    }

    // Set the pixel size
    if( FT_Set_Pixel_Sizes( m_face, 0, m_size ) )
    {
        msg_Warn( getIntf(), "Cannot set a pixel size of %d (%s)", m_size,
                  m_name.c_str() );
    }

    // Get the font metrucs
    m_height = m_face->size->metrics.height >> 6;
    m_ascender = m_face->size->metrics.ascender >> 6;
    m_descender = m_face->size->metrics.descender >> 6;

    // Render the '.' symbol and compute its size
    m_dotIndex = FT_Get_Char_Index( m_face, '.' );
    FT_Load_Glyph( m_face, m_dotIndex, FT_LOAD_DEFAULT );
    FT_Get_Glyph( m_face->glyph, &m_dotGlyph );
    FT_BBox dotSize;
    FT_Glyph_Get_CBox( m_dotGlyph, ft_glyph_bbox_pixels, &dotSize );
    m_dotWidth = dotSize.xMax - dotSize.xMin;
    m_dotAdvance = m_face->glyph->advance.x >> 6;
    FT_Glyph_To_Bitmap( &m_dotGlyph, ft_render_mode_normal, NULL, 1 );

    return true;
}


GenericBitmap *FT2Font::drawString( const UString &rString, uint32_t color,
                                    int maxWidth ) const
{
    int err;
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
        fribidi_log2vis( (FriBidiChar*)pString, len, &baseDir,
                         (FriBidiChar*)pFribidiString, 0, 0, 0 );
        pString = pFribidiString;
    }
#endif

    // Array of glyph bitmaps and position
    FT_Glyph *glyphs = new FT_Glyph[len];
    int *pos = new int[len];

    // Does the font support kerning ?
    FT_Bool useKerning = FT_HAS_KERNING( m_face );
    int previous = 0;

    // Index of the last glyph when the text is truncated with trailing ...
    int maxIndex = 0;
    // Position of the first trailing dot
    int firstDotX = 0;

    // First, render all the glyphs
    for( n = 0; n < len; n++ )
    {
        code = *(pString++);
        // Load the glyph
        int glyphIndex = FT_Get_Char_Index( m_face, code );
        err = FT_Load_Glyph( m_face, glyphIndex, FT_LOAD_DEFAULT );
        err = FT_Get_Glyph( m_face->glyph, &glyphs[n] );

        // Retrieve kerning distance and move pen position
        if( useKerning && previous && glyphIndex )
        {
            FT_Vector delta;
            FT_Get_Kerning( m_face, previous, glyphIndex,
                            ft_kerning_default, &delta );
            penX += delta.x >> 6;
        }

        // Get the glyph size
        FT_BBox glyphSize;
        FT_Glyph_Get_CBox( glyphs[n], ft_glyph_bbox_pixels, &glyphSize );

        // Render the glyph
        err = FT_Glyph_To_Bitmap( &glyphs[n], ft_render_mode_normal, NULL, 1 );

        pos[n] = penX;
        width1 = penX + glyphSize.xMax - glyphSize.xMin;
        yMin = __MIN( yMin, glyphSize.yMin );
        yMax = __MAX( yMax, glyphSize.yMax );

        // Next position
        penX += m_face->glyph->advance.x >> 6;

        // Save glyph index
        previous = glyphIndex;

        if( maxWidth != -1 )
        {
            // Check if the truncated text with the '...' fit in the maxWidth
            int curX = penX;
            if( useKerning )
            {
                FT_Vector delta;
                FT_Get_Kerning( m_face, glyphIndex, m_dotIndex,
                                ft_kerning_default, &delta );
                curX += delta.x >> 6;
            }
            if( curX + 2 * m_dotAdvance + m_dotWidth < maxWidth )
            {
                width2 = curX + 2 * m_dotAdvance + m_dotWidth;
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

        // Free the glyph
        FT_Done_Glyph( glyphs[n] );
    }
    // Draw the trailing dots if the text is truncated
    if( maxIndex < len )
    {
        int penX = firstDotX;
        FT_BitmapGlyphRec *pBmpGlyph = (FT_BitmapGlyphRec*)m_dotGlyph;
        for( n = 0; n < 3; n++ )
        {
            // Draw the glyph on the bitmap
            pBmp->draw( pBmpGlyph->bitmap, penX, yMax - pBmpGlyph->top,
                        color );
            penX += m_dotAdvance;
        }
    }

    delete [] glyphs;
    delete [] pos;

    return pBmp;
}
