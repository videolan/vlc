/*****************************************************************************
 * scaled_bitmap.cpp
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

#include "scaled_bitmap.hpp"


ScaledBitmap::ScaledBitmap( intf_thread_t *pIntf, const GenericBitmap &rBitmap,
                            int width, int height ):
    GenericBitmap( pIntf ), m_width( width ), m_height( height )
{
    // XXX We should check that width and height are positive...

    // Allocate memory for the buffer
    m_pData = new uint8_t[m_height * m_width * 4];

    int srcWidth = rBitmap.getWidth();
    int srcHeight = rBitmap.getHeight();
    uint32_t *pSrcData = (uint32_t*)rBitmap.getData();
    uint32_t *pDestData = (uint32_t*)m_pData;

    // Algorithm for horizontal enlargement
    if( width > srcWidth )
    {
        // Decision variables for Bresenham algorithm
        int incX1 = 2 * (srcWidth-1);
        int incX2 = incX1 - 2 * (width-1);

        for( int y = 0; y < height; y++ )
        {
            int dX = incX1 - (width-1);
            uint32_t yOffset = ((y * srcHeight) / height) * srcWidth;
            pSrcData = ((uint32_t*)rBitmap.getData()) + yOffset;

            for( int x = 0; x < width; x++ )
            {
                *(pDestData++) = *pSrcData;

                if( dX <= 0 )
                {
                    dX += incX1;
                }
                else
                {
                    dX += incX2;
                    pSrcData++;
                }
            }
        }
    }
    // Algorithm for horizontal reduction
    else
    {
        // Decision variables for Bresenham algorithm
        int incX1 = 2 * (width-1);
        int incX2 = incX1 - 2 * (srcWidth-1);

        for( int y = 0; y < height; y++ )
        {
            int dX = incX1 - (srcWidth-1);
            uint32_t yOffset = ((y * srcHeight) / height) * srcWidth;
            pSrcData = ((uint32_t*)rBitmap.getData()) + yOffset;

            if (width == 1)
            {
                *(pDestData++) = *pSrcData;
            }
            else for( int x = 0; x < width; x++ )
            {
                *(pDestData++) = *(pSrcData++);

                while( dX <= 0 )
                {
                    dX += incX1;
                    pSrcData++;
                }
                dX += incX2;
            }
        }

    }
}


ScaledBitmap::~ScaledBitmap()
{
    delete[] m_pData;
}

