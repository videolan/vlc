/*****************************************************************************
 * time.cpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id$
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
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

#include <stdio.h>  // snprintf

#include "stream.hpp"
#include "../utils/ustring.hpp"
#include "../src/os_factory.hpp"
#include <vlc/input.h>


void Stream::set( const UString &name, bool updateVLC )
{
    VarText::set( name );

    // Avoid looping forever...
    if( updateVLC )
    {
        // We have nothing to do here, until we decide that the user
        // can change a stream name on the fly...
    }
}


const string Stream::getAsStringName() const
{
    string fullName = getAsStringFullName();

    // XXX: This should be done in VLC core, not here...
    // Remove path information if any
    OSFactory *pFactory = OSFactory::instance( getIntf() );
    string::size_type pos = fullName.rfind( pFactory->getDirSeparator() );
    if( pos != string::npos )
    {
        fullName = fullName.substr( pos + 1, fullName.size() - pos + 1 );
    }

    return fullName;
}


const string Stream::getAsStringFullName() const
{
    string ret;

    // XXX: we are not using geIntf()->p_sys->p_input direclty here, because it
    // is not updated by VlcProc yet. Anyway, we shouldn't need to do that,
    // because VLC core should provide getter functions for the stream name...
    if( getIntf()->p_sys->p_playlist->p_input == NULL )
    {
        ret = "";
    }
    else
    {
#warning "FIXME!"
        ret = getIntf()->p_sys->p_playlist->p_input->input.p_item->psz_uri;
    }

    return ret;
}

