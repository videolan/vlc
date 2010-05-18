/*****************************************************************************
 * art_bitmap.cpp
 *****************************************************************************
 * Copyright (C) 2010 the VideoLAN team
 * $Id$
 *
 * Author: Erwan Tulou      <erwan10@vidoelan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "art_bitmap.hpp"
#include <vlc_image.h>


// static variables
intf_thread_t*    ArtBitmap::m_pIntf = NULL;
image_handler_t*  ArtBitmap::m_pImageHandler = NULL;
list<ArtBitmap*>  ArtBitmap::m_listBitmap;

ArtBitmap::ArtBitmap( string uriName ):
    FileBitmap( m_pIntf, m_pImageHandler, uriName, -1 ), m_uriName( uriName )
{
}

void ArtBitmap::initArtBitmap( intf_thread_t* pIntf )
{
    if( m_pIntf )
        return;

    // retain reference to skins interface
    m_pIntf = pIntf;

    // initialize handler
    m_pImageHandler = image_HandlerCreate( pIntf );

    if( !m_pImageHandler )
        msg_Err( m_pIntf, "initialization of art bitmaps failed" );
}


ArtBitmap* ArtBitmap::getArtBitmap( string uriName )
{
    if( !m_pImageHandler )
        return NULL;

    // check whether art is already loaded
    list<ArtBitmap*>::const_iterator it;
    for( it = m_listBitmap.begin(); it != m_listBitmap.end(); ++it )
    {
        if( (*it)->getUriName() == uriName )
            return *it;
    }

    // create and retain a new ArtBitmap since uri if not yet known
    ArtBitmap* pArt = new ArtBitmap( uriName );
    if( pArt && pArt->getWidth() && pArt->getHeight() )
    {
        m_listBitmap.push_back( pArt );
        return pArt;
    }
    else
    {
        delete pArt;
        return NULL;
    }
}

void ArtBitmap::freeArtBitmap( )
{
    m_pIntf = NULL;

    if( m_pImageHandler )
    {
        image_HandlerDelete( m_pImageHandler );
        m_pImageHandler = NULL;
    }

    m_listBitmap.clear();
}
