/*****************************************************************************
 * png_bitmap.cpp
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

#include <png.h>
#include "png_bitmap.hpp"


PngBitmap::PngBitmap( intf_thread_t *pIntf, string fileName,
                      uint32_t aColor ):
    GenericBitmap( pIntf ), m_width( 0 ), m_height( 0 ), m_pData( NULL )
{
    // Open the PNG file
    FILE *pFile = fopen( fileName.c_str(), "rb" );
    if( pFile == NULL )
    {
        msg_Err( getIntf(), "Cannot open bitmap %s", fileName.c_str() );
        return;
    }

    // Create the PNG structures
    png_structp pReadStruct = png_create_read_struct( PNG_LIBPNG_VER_STRING,
        NULL, NULL, NULL );
    if ( pReadStruct == NULL )
    {
        msg_Err( getIntf(), "Failed to create PNG read struct" );
        return;
    }
    png_infop pInfo = png_create_info_struct( pReadStruct );
    if( pInfo == NULL )
    {
        png_destroy_read_struct( &pReadStruct, NULL, NULL );
        msg_Err( getIntf(), "Failed to create PNG info struct" );
        return;
    }
    png_infop pEndInfo = png_create_info_struct( pReadStruct );
    if( pEndInfo == NULL )
    {
        png_destroy_read_struct( &pReadStruct, NULL, NULL );
        msg_Err( getIntf(), "Failed to create PNG end info struct" );
        return;
    }

    // Initialize the PNG reader
    png_init_io( pReadStruct, pFile );

    // Read the image header
    png_read_info( pReadStruct, pInfo );
    m_width = png_get_image_width( pReadStruct, pInfo );
    m_height = png_get_image_height( pReadStruct, pInfo );
    int depth = png_get_bit_depth( pReadStruct, pInfo );
    int colorType = png_get_color_type( pReadStruct, pInfo );

    // Convert paletted images to RGB
    if( colorType == PNG_COLOR_TYPE_PALETTE )
    {
        png_set_palette_to_rgb( pReadStruct );
    }
    // Strip to 8 bits per channel
    if( depth == 16 )
    {
        png_set_strip_16( pReadStruct );
    }
    // 4 bytes per pixel
    if( !(colorType & PNG_COLOR_MASK_ALPHA ) )
    {
        png_set_filler( pReadStruct, 0xff, PNG_FILLER_AFTER );
    }
    // Invert colors
    if( colorType & PNG_COLOR_MASK_COLOR )
    {
        png_set_bgr( pReadStruct );
    }
    png_read_update_info( pReadStruct, pInfo );

    // Allocate memory for the buffers
    m_pData = new uint8_t[m_height * m_width * 4];
    uint8_t** pRows = new uint8_t*[m_height];
    for( int i = 0; i < m_height; i++ )
    {
        pRows[i] = m_pData + (i * m_width * 4);
    }

    // Read the image
    png_read_image( pReadStruct, pRows );
    png_read_end( pReadStruct, pEndInfo );

    // Compute the alpha layer
    uint8_t *pData = m_pData;
    for( int y = 0; y < m_height; y++ )
    {
        for( int x = 0; x < m_width; x++ )
        {
            uint32_t b = (uint32_t)*(pData++);
            uint32_t g = (uint32_t)*(pData++);
            uint32_t r = (uint32_t)*(pData++);
            // Transparent pixel ?
            if( aColor == (r<<16 | g<<8 | b) )
            {
                *(pData++) = 0;
            }
        }
    }

    // Free the structures
    png_destroy_read_struct( &pReadStruct, &pInfo, &pEndInfo );
    delete[] pRows;

    // Close the file
    fclose( pFile );
}


PngBitmap::~PngBitmap()
{
    if( m_pData )
    {
        delete[] m_pData;
    }
}


uint8_t *PngBitmap::getData() const
{
    if( m_pData == NULL )
    {
        msg_Warn( getIntf(), "PngBitmap::getData() returns NULL" );
    }
    return m_pData;
}
