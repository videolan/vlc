/*****************************************************************************
 * time.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: time.hpp,v 1.2 2004/01/11 17:12:17 asmax Exp $
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

#ifndef TIME_HPP
#define TIME_HPP

#include "../utils/var_percent.hpp"
#include <string>

/// Variable for VLC volume
class Time: public VarPercent
{
    public:
        Time( intf_thread_t *pIntf ): VarPercent( pIntf ) {}
        virtual ~Time() {}

        virtual void set( float percentage, bool updateVLC );

        virtual void set( float percentage ) { set( percentage, true ); }

        /// Return a string containing a value from 0 to 100
        virtual string getAsStringPercent() const;
        /// Return a string formatted as a time display (h:mm:ss)
        virtual string getAsStringTime() const;
};

#endif
