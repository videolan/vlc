/*****************************************************************************
 * os_timer.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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

#ifndef OS_TIMER_HPP
#define OS_TIMER_HPP

#include "skin_common.hpp"


// Base class for OS-specific timers
class OSTimer: public SkinObject
{
public:
    virtual ~OSTimer() { }

    /// (Re)start the timer with the given delay (in ms). If oneShot is
    /// true, stop it after the first execution of the callback.
    virtual void start( int delay, bool oneShot ) = 0;

    /// Stop the timer
    virtual void stop() = 0;

protected:
    OSTimer( intf_thread_t *pIntf ): SkinObject( pIntf ) { }
};


#endif
