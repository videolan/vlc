/*****************************************************************************
 * time.cpp
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

#include "time.hpp"
#include <vlc_input.h>


inline bool StreamTime::havePosition() const {
    input_thread_t *p_input = getIntf()->p_sys->p_input;
    return p_input && ( var_GetFloat( p_input, "position" ) != 0.0 );
}


void StreamTime::set( float percentage, bool updateVLC )
{
    VarPercent::set( percentage );

    // Avoid looping forever...
    if( updateVLC && getIntf()->p_sys->p_input )
        var_SetFloat( getIntf()->p_sys->p_input, "position", percentage );
}


string StreamTime::getAsStringPercent() const
{
    int value = (int)(100. * get());
    // 0 <= value <= 100, so we need 4 chars
    char str[4];
    snprintf( str, 4, "%d", value );
    return string(str);
}


string StreamTime::formatTime( int seconds, bool bShortFormat ) const
{
    char psz_time[MSTRTIME_MAX_SIZE];
    if( bShortFormat && (seconds < 60 * 60) )
    {
        snprintf( psz_time, MSTRTIME_MAX_SIZE, "%02d:%02d",
                  (int) (seconds / 60 % 60),
                  (int) (seconds % 60) );
    }
    else
    {
        snprintf( psz_time, MSTRTIME_MAX_SIZE, "%d:%02d:%02d",
                  (int) (seconds / (60 * 60)),
                  (int) (seconds / 60 % 60),
                  (int) (seconds % 60) );
    }
    return string(psz_time);
}


string StreamTime::getAsStringCurrTime( bool bShortFormat ) const
{
    if( !havePosition() )
        return "-:--:--";

    mtime_t time = var_GetTime( getIntf()->p_sys->p_input, "time" );
    return formatTime( time / 1000000, bShortFormat );
}


string StreamTime::getAsStringTimeLeft( bool bShortFormat ) const
{
    if( !havePosition() )
        return "-:--:--";

    mtime_t time = var_GetTime( getIntf()->p_sys->p_input, "time" ),
        duration = var_GetTime( getIntf()->p_sys->p_input, "length" );

    return formatTime( (duration - time) / 1000000, bShortFormat );
}


string StreamTime::getAsStringDuration( bool bShortFormat ) const
{
    if( !havePosition() )
        return "-:--:--";

    mtime_t time = var_GetTime( getIntf()->p_sys->p_input, "length" );
    return formatTime( time / 1000000, bShortFormat );
}
