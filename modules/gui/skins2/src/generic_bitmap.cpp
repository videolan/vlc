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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "generic_bitmap.hpp"
#include "os_factory.hpp"


GenericBitmap::GenericBitmap( intf_thread_t *pIntf,
                              int nbFrames, int fps, int nbLoops ):
    SkinObject( pIntf ), m_nbFrames( nbFrames ),
    m_frameRate( fps ), m_nbLoops( nbLoops ), m_pGraphics( NULL )
{
}

const OSGraphics *GenericBitmap::getGraphics() const
{
    if( m_pGraphics )
        return m_pGraphics;

    OSFactory *pOsFactory = OSFactory::instance( getIntf() );

    int width = getWidth();
    int height = getHeight();
    if( width > 0 && height > 0 )
    {
        m_pGraphics = pOsFactory->createOSGraphics( width, height );
        m_pGraphics->drawBitmap( *this, 0, 0 );
        return m_pGraphics;
    }

    msg_Err( getIntf(), "failed to create a graphics, please report" );
    return NULL;
}


BitmapImpl::BitmapImpl( intf_thread_t *pIntf, int width, int height,
                        int nbFrames, int fps, int nbLoops ):
    GenericBitmap( pIntf, nbFrames, fps, nbLoops ), m_width( width ),
    m_height( height ), m_pData( NULL )
{
    m_pData = new uint8_t[width * height * 4];
    memset( m_pData, 0, width * height * 4 );
}


BitmapImpl::~BitmapImpl()
{
    delete[] m_pData;
}


bool BitmapImpl::drawBitmap( const GenericBitmap &rSource, int xSrc, int ySrc,
                             int xDest, int yDest, int width, int height )
{
    int srcWidth = rSource.getWidth();
    uint32_t *pSrc = (uint32_t*)rSource.getData() + ySrc * srcWidth + xSrc;
    if( !pSrc )
    {
        return false;
    }
    if( xSrc < 0 || xSrc + width > srcWidth ||
        ySrc < 0 || ySrc + height > rSource.getHeight() )
    {
        msg_Warn( getIntf(), "drawBitmap: source rect too small, ignoring" );
        return false;
    }
    if( xDest < 0 || xDest + width > m_width ||
        yDest < 0 || yDest + height > m_height )
    {
        msg_Warn( getIntf(), "drawBitmap: dest rect too small, ignoring" );
        return false;
    }

    uint32_t *pDest = (uint32_t*)m_pData + yDest * m_width + xDest ;
    for( int y = 0; y < height; y++ )
    {
        memcpy( pDest, pSrc, 4 * width );
        pSrc += srcWidth;
        pDest += m_width;
    }
    return true;
}

