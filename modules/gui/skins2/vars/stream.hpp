/*****************************************************************************
 * stream.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: time.hpp 6996 2004-03-07 12:55:32Z ipkiss $
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

#ifndef STREAM_HPP
#define STREAM_HPP

#include "../utils/var_text.hpp"
#include <string>

class UString;

/// Variable for VLC volume
class Stream: public VarText
{
    public:
        Stream( intf_thread_t *pIntf ): VarText( pIntf ) {}
        virtual ~Stream() {}

        virtual void set( const UString &name, bool updateVLC );

        virtual void set( const UString &name ) { set( name, true ); }

        /// Return current stream name
        virtual const string getAsStringName() const;
        /// Return current stream full name (i.e. including path)
        virtual const string getAsStringFullName() const;
};

#endif
