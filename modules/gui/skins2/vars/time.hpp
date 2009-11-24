/*****************************************************************************
 * time.hpp
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef TIME_HPP
#define TIME_HPP

#include "../utils/var_percent.hpp"
#include <string>

/// Variable for VLC stream time
class StreamTime: public VarPercent
{
public:
    StreamTime( intf_thread_t *pIntf ): VarPercent( pIntf ) { }
    virtual ~StreamTime() { }

    virtual void set( float percentage, bool updateVLC );

    virtual void set( float percentage ) { set( percentage, true ); }

    /// Return a string containing a value from 0 to 100
    virtual string getAsStringPercent() const;
    /// Return the current time formatted as a time display (h:mm:ss)
    virtual string getAsStringCurrTime( bool bShortFormat = false ) const;
    /// Return the time left formatted as a time display (h:mm:ss)
    virtual string getAsStringTimeLeft( bool bShortFormat = false ) const;
    /// Return the duration formatted as a time display (h:mm:ss)
    virtual string getAsStringDuration( bool bShortFormat = false ) const;

private:
    /// Convert a number of seconds into "h:mm:ss" format
    string formatTime( int seconds, bool bShortFormat ) const;
    /// Return true when there is a non-null input and its position is not 0.0.
    bool havePosition() const;
};

#endif
