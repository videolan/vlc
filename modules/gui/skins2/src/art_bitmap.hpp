/*****************************************************************************
 * art_bitmap.hpp
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef ART_BITMAP_HPP
#define ART_BITMAP_HPP

#include "file_bitmap.hpp"
#include <string>
#include <list>


/// Class for art bitmaps
class ArtBitmap: public FileBitmap
{
public:

    static ArtBitmap* getArtBitmap( string uriName );
    static void initArtBitmap( intf_thread_t* pIntf );
    static void freeArtBitmap( );

    string getUriName() { return m_uriName; }

protected:

    /// Constructor/destructor
    ArtBitmap( string uriName );
    virtual ~ArtBitmap() {}

    /// skins2 interface
    static intf_thread_t *m_pIntf;

    /// Image handler (used to load art files)
    static image_handler_t *m_pImageHandler;

    // keep a cache of art already open
    static list<ArtBitmap*> m_listBitmap;

private:

    // uriName
    string m_uriName;

};


#endif
