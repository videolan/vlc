/*****************************************************************************
 * bitmap_font.cpp
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "bitmap_font.hpp"
#include "generic_bitmap.hpp"
#include "../utils/ustring.hpp"


BitmapFont::BitmapFont( intf_thread_t *pIntf, const GenericBitmap &rBitmap,
                        const std::string &rType ):
    GenericFont( pIntf ), m_rBitmap( rBitmap )
{
    int i;

    // Build the character table
    if( rType == "digits" )
    {
        m_width = 9;
        m_height = 13;
        m_advance = 12;
        m_skip = 6;
        for( i = 0; i <= 9; i++ )
        {
            m_table['0'+i].m_xPos = i * m_width;
        }
        m_table[(size_t)' '].m_xPos = 10 * m_width;
        m_table[(size_t)'-'].m_xPos = 11 * m_width;
    }
    else if( rType == "text" )
    {
        m_width = 5;
        m_height = 6;
        m_advance = 5;
        m_skip = 5;
        for( i = 0; i < 26; i++ )
        {
            m_table['A'+i].m_xPos = m_table['a'+i].m_xPos = i * m_width;
        }
        m_table[(size_t)'"'].m_xPos = 26 * m_width;
        m_table[(size_t)'@'].m_xPos = 27 * m_width;
        m_table[(size_t)' '].m_xPos = 29 * m_width;
        for( i = 0; i <= 9; i++ )
        {
            m_table['0'+i].m_xPos = i * m_width;
            m_table['0'+i].m_yPos = m_height;
        }
        static const char specialChars[] = {'.', ':', '(', ')', '-', '\'',
            '!', '_', '+', '\\', '/', '[', ']', '^', '&', '%', ',', '=', '$',
            '#'};
        for( i = 0; i < 19; i++ )
        {
            m_table[(size_t)specialChars[i]].m_xPos = (11 + i) * m_width;
            m_table[(size_t)specialChars[i]].m_yPos = m_height;
        }
        m_table[(size_t)'?'].m_xPos = 4 * m_width;
        m_table[(size_t)'*'].m_xPos = 5 * m_width;
        m_table[(size_t)'?'].m_yPos = m_table[(size_t)'*'].m_yPos = 2 * m_height;
    }
}


GenericBitmap *BitmapFont::drawString( const UString &rString,
                                       uint32_t color, int maxWidth ) const
{
    (void)color; (void)maxWidth;

    uint32_t *pString = (uint32_t*)rString.u_str();
    // Compute the text width
    int width = 0;
    for( uint32_t *ptr = pString; *ptr; ptr++ )
    {
        uint32_t c = *ptr;
        if( c < 256 && m_table[c].m_xPos != -1 )
        {
            width += m_advance;
        }
        else
        {
            width += m_skip;
        }
    }
    // Create a bitmap
    BitmapImpl *pBmp = new BitmapImpl( getIntf(), width, m_height );
    int xDest = 0;
    while( *pString )
    {
        uint32_t c = *(pString++);
        if( c < 256 && m_table[c].m_xPos != -1 )
        {
            bool res = pBmp->drawBitmap( m_rBitmap, m_table[c].m_xPos,
                                         m_table[c].m_yPos, xDest, 0,
                                         m_width, m_height );
            if ( !res )
                msg_Warn( getIntf(), "BitmapFont::drawString: ignoring char" );
            xDest += m_advance;
        }
        else
        {
            xDest += m_skip;
        }
    }
    return pBmp;
}

