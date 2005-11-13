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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include <stdio.h>  // snprintf

#include "time.hpp"
#include <vlc/input.h>


void StreamTime::set( float percentage, bool updateVLC )
{
    VarPercent::set( percentage );

    // Avoid looping forever...
    if( updateVLC && getIntf()->p_sys->p_input )
    {
        vlc_value_t pos;
        pos.f_float = percentage;

        var_Set( getIntf()->p_sys->p_input, "position", pos );
    }
}


const string StreamTime::getAsStringPercent() const
{
    int value = (int)(100. * get());
    // 0 <= value <= 100, so we need 4 chars
    char *str = new char[4];
    snprintf( str, 4, "%d", value );
    string ret = str;
    delete[] str;

    return ret;
}


const string StreamTime::getAsStringCurrTime( bool bShortFormat ) const
{
    if( getIntf()->p_sys->p_input == NULL )
    {
        return "-:--:--";
    }

    vlc_value_t pos;
    var_Get( getIntf()->p_sys->p_input, "position", &pos );
    if( pos.f_float == 0.0 )
    {
        return "-:--:--";
    }

    vlc_value_t time;
    var_Get( getIntf()->p_sys->p_input, "time", &time );

    return formatTime( time.i_time / 1000000, bShortFormat );
}


const string StreamTime::getAsStringTimeLeft( bool bShortFormat ) const
{
    if( getIntf()->p_sys->p_input == NULL )
    {
        return "-:--:--";
    }

    vlc_value_t pos;
    var_Get( getIntf()->p_sys->p_input, "position", &pos );
    if( pos.f_float == 0.0 )
    {
        return "-:--:--";
    }

    vlc_value_t time, duration;
    var_Get( getIntf()->p_sys->p_input, "time", &time );
    var_Get( getIntf()->p_sys->p_input, "length", &duration );

    return formatTime( (duration.i_time - time.i_time) / 1000000,
                       bShortFormat );
}


const string StreamTime::getAsStringDuration( bool bShortFormat ) const
{
    if( getIntf()->p_sys->p_input == NULL )
    {
        return "-:--:--";
    }

    vlc_value_t pos;
    var_Get( getIntf()->p_sys->p_input, "position", &pos );
    if( pos.f_float == 0.0 )
    {
        return "-:--:--";
    }

    vlc_value_t time;
    var_Get( getIntf()->p_sys->p_input, "length", &time );

    return formatTime( time.i_time / 1000000, bShortFormat );
}


const string StreamTime::formatTime( int seconds, bool bShortFormat ) const
{
    char *psz_time = new char[MSTRTIME_MAX_SIZE];
    if( bShortFormat && (seconds < 60 * 60) )
    {
        snprintf( psz_time, MSTRTIME_MAX_SIZE, "  %02d:%02d",
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

    string ret = psz_time;
    delete[] psz_time;

    return ret;
}
