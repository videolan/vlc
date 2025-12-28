/*****************************************************************************
 * macosx_timer.mm
 *****************************************************************************
 * Copyright (C) 2024 the VideoLAN team
 *
 * Authors: VLC contributors
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "macosx_timer.hpp"
#include "../commands/cmd_generic.hpp"

#include <algorithm>


MacOSXTimer::MacOSXTimer( intf_thread_t *pIntf, CmdGeneric &rCmd,
                          MacOSXTimerLoop *pTimerLoop ):
    OSTimer( pIntf ), m_rCommand( rCmd ), m_pTimerLoop( pTimerLoop ),
    m_interval( 0 ), m_nextDate( 0 ), m_oneShot( false )
{
}


MacOSXTimer::~MacOSXTimer()
{
    stop();
}


void MacOSXTimer::start( int delay, bool oneShot )
{
    // Stop any existing timer
    stop();

    m_interval = VLC_TICK_FROM_MS( delay );
    m_oneShot = oneShot;
    m_nextDate = vlc_tick_now() + m_interval;

    // Register with the timer loop
    if( m_pTimerLoop )
    {
        m_pTimerLoop->addTimer( *this );
    }
}


void MacOSXTimer::stop()
{
    if( m_pTimerLoop )
    {
        m_pTimerLoop->removeTimer( *this );
    }
    m_nextDate = 0;
}


bool MacOSXTimer::execute()
{
    // Execute the command
    m_rCommand.execute();

    // Update for next execution
    if( m_oneShot )
    {
        return false;  // Remove the timer
    }

    m_nextDate = vlc_tick_now() + m_interval;
    return true;  // Keep the timer
}


MacOSXTimerLoop::MacOSXTimerLoop( intf_thread_t *pIntf ):
    SkinObject( pIntf )
{
}


MacOSXTimerLoop::~MacOSXTimerLoop()
{
    m_timers.clear();
}


void MacOSXTimerLoop::addTimer( MacOSXTimer &rTimer )
{
    m_timers.push_back( &rTimer );
}


void MacOSXTimerLoop::removeTimer( MacOSXTimer &rTimer )
{
    m_timers.remove( &rTimer );
}


void MacOSXTimerLoop::checkTimers()
{
    vlc_tick_t now = vlc_tick_now();

    // Create a copy of the list to iterate safely
    std::list<MacOSXTimer*> timersCopy = m_timers;

    for( auto it = timersCopy.begin(); it != timersCopy.end(); ++it )
    {
        MacOSXTimer *pTimer = *it;
        if( pTimer->getNextDate() <= now )
        {
            if( !pTimer->execute() )
            {
                // Timer requested to be removed
                m_timers.remove( pTimer );
            }
        }
    }
}
