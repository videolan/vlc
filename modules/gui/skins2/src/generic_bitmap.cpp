/*****************************************************************************
 * generic_bitmap.cpp
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include "generic_bitmap.hpp"


BitmapImpl::BitmapImpl( intf_thread_t *pIntf, int width, int height ):
    GenericBitmap( pIntf ), m_width( width ), m_height( height ),
    m_pData( NULL )
{
    m_pData = new uint8_t[width * height * 4];
    memset( m_pData, 0, width * height * 4 );
}


BitmapImpl::~BitmapImpl()
{
    delete[] m_pData;
}


void BitmapImpl::drawBitmap( const GenericBitmap &rSource, int xSrc, int ySrc,
                             int xDest, int yDest, int width, int height )
{
    int srcWidth = rSource.getWidth();
    uint32_t *pSrc = (uint32_t*)rSource.getData() + ySrc * srcWidth + xSrc;
    uint32_t *pDest = (uint32_t*)m_pData + yDest * m_width + xDest ;
    for( int y = 0; y < height; y++ )
    {
        memcpy( pDest, pSrc, 4 * width );
        pSrc += srcWidth;
        pDest += m_width;
    }
}

