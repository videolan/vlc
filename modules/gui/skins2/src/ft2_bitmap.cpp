/*****************************************************************************
 * ft2_bitmap.cpp
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

#include "ft2_bitmap.hpp"


FT2Bitmap::FT2Bitmap( intf_thread_t *pIntf, int width, int height ):
    GenericBitmap( pIntf ), m_width( width ), m_height( height )
{
    // Allocate memory for the buffer
    m_pData = new uint8_t[m_height * m_width * 4];
    memset( m_pData, 0, m_height * m_width * 4 );
}


FT2Bitmap::~FT2Bitmap()
{
    delete[] m_pData;
}


void FT2Bitmap::draw( const FT_Bitmap &rBitmap, int left, int top,
                      uint32_t color )
{
    uint8_t *pBuf = rBitmap.buffer;

    // Calculate colors
    uint8_t blue = color & 0xff;
    uint8_t green = (color >> 8) & 0xff;
    uint8_t red = (color >> 16) & 0xff;

    for( int y = top; y < top + rBitmap.rows; y++ )
    {
        uint8_t *pData = m_pData + 4 * (m_width * y + left);
        for( int x = left; x < left + rBitmap.width; x++ )
        {
            // The buffer in FT_Bitmap contains alpha values
            uint8_t val = *(pBuf++);
            *(pData++) = (blue * val) >> 8;
            *(pData++) = (green * val) >> 8;
            *(pData++) = (red * val) >> 8;
            *(pData++) = val;
        }
    }
}


