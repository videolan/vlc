/*****************************************************************************
 * art_manager.hpp
 *****************************************************************************
 * Copyright (C) 2010 the VideoLAN team
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef ART_MANAGER_HPP
#define ART_MANAGER_HPP

#include "file_bitmap.hpp"
#include <string>
#include <list>


/// Class for art bitmaps
class ArtBitmap: public FileBitmap
{
public:

    std::string getUriName() { return m_uriName; }

    /// Constructor/destructor
    ArtBitmap( intf_thread_t *pIntf, image_handler_t *pImageHandler,
               std::string uriName ) :
        FileBitmap( pIntf, pImageHandler, uriName, -1 ),
        m_uriName( uriName ) {}
    virtual ~ArtBitmap() {}

private:
    /// uriName
    std::string m_uriName;
};


/// Singleton object for handling art
class ArtManager: public SkinObject
{
public:
    /// Get the instance of ArtManager
    /// Returns NULL if the initialization of the object failed
    static ArtManager *instance( intf_thread_t *pIntf );

    /// Delete the instance of ArtManager
    static void destroy( intf_thread_t *pIntf );

    /// Retrieve for the art file from uri name
    ArtBitmap* getArtBitmap( std::string uriName );

protected:
    // Protected because it is a singleton
    ArtManager( intf_thread_t *pIntf );
    virtual ~ArtManager();

private:
    /// Image handler (used to load art files)
    image_handler_t *m_pImageHandler;

    // keep a cache of art already open
    std::list<ArtBitmap*> m_listBitmap;
};

#endif
