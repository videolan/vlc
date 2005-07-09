/*****************************************************************************
 * png_bitmap.cpp
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include <vlc/vlc.h>
#include "vlc_image.h"
#include "png_bitmap.hpp"

PngBitmap::PngBitmap( intf_thread_t *pIntf, image_handler_t *pImageHandler,
                      string fileName, uint32_t aColor ):
    GenericBitmap( pIntf ), m_width( 0 ), m_height( 0 )
{
    video_format_t fmt_in = {0}, fmt_out = {0};
    picture_t *pPic;

    fmt_out.i_chroma = VLC_FOURCC('R','V','3','2');

    pPic = image_ReadUrl( pImageHandler, fileName.c_str(), &fmt_in, &fmt_out );
    if( !pPic ) return;

    m_width = fmt_out.i_width;
    m_height = fmt_out.i_height;

    m_pData = new uint8_t[m_height * m_width * 4];

    // Compute the alpha layer
    uint8_t *pData = m_pData, *pSrc = pPic->p->p_pixels;
    for( int y = 0; y < m_height; y++ )
    {
        for( int x = 0; x < m_width; x++ )
        {
            uint32_t b = *(pData++) = *(pSrc++);
            uint32_t g = *(pData++) = *(pSrc++);
            uint32_t r = *(pData++) = *(pSrc++);
            *pData = *pSrc;

            // Transparent pixel ?
            if( aColor == (r<<16 | g<<8 | b) ) *pData = 0;
            pData++; pSrc++;
        }
        pSrc += pPic->p->i_pitch - m_width * 4;
    }

    pPic->pf_release( pPic );
    return;
}


PngBitmap::~PngBitmap()
{
    if( m_pData ) delete[] m_pData;
}


uint8_t *PngBitmap::getData() const
{
    if( m_pData == NULL )
    {
        msg_Warn( getIntf(), "PngBitmap::getData() returns NULL" );
    }
    return m_pData;
}
