/*****************************************************************************
 * x11_timer.cpp
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef X11_SKINS

#include <unistd.h>
#include <poll.h>

#include "x11_timer.hpp"
#include "x11_factory.hpp"
#include "../commands/cmd_generic.hpp"


X11Timer::X11Timer( intf_thread_t *pIntf, CmdGeneric &rCmd ):
    OSTimer( pIntf ), m_rCommand( rCmd )
{
    // Get the instance of timer loop
    X11Factory *m_pOsFactory = (X11Factory*)(OSFactory::instance( pIntf ) );
    m_pTimerLoop = m_pOsFactory->getTimerLoop();
}


X11Timer::~X11Timer()
{
    stop();
}


void X11Timer::start( int delay, bool oneShot )
{
    m_interval = 1000LL * (vlc_tick_t)delay;
    m_oneShot = oneShot;
    m_nextDate = vlc_tick_now() + m_interval;
    m_pTimerLoop->addTimer( *this );
}


void X11Timer::stop()
{
    m_pTimerLoop->removeTimer( *this );
}


vlc_tick_t X11Timer::getNextDate() const
{
    return m_nextDate;
}


bool X11Timer::execute()
{
    m_nextDate += m_interval;
    // Execute the callback
    m_rCommand.execute();

    return !m_oneShot;
}


X11TimerLoop::X11TimerLoop( intf_thread_t *pIntf, int connectionNumber ):
    SkinObject( pIntf ), m_connectionNumber( connectionNumber )
{
}


X11TimerLoop::~X11TimerLoop()
{
}


void X11TimerLoop::addTimer( X11Timer &rTimer )
{
    m_timers.push_back( &rTimer );
}


void X11TimerLoop::removeTimer( X11Timer &rTimer )
{
    m_timers.remove( &rTimer );
}


void X11TimerLoop::waitNextTimer()
{
    vlc_tick_t curDate = vlc_tick_now();
    vlc_tick_t nextDate = INT64_MAX;

    X11Timer *nextTimer = NULL;

    // Find the next timer to execute
    std::list<X11Timer*>::const_iterator timer;
    for( timer = m_timers.begin(); timer != m_timers.end(); ++timer )
    {
        vlc_tick_t timerDate = (*timer)->getNextDate();
        if( timerDate < nextDate )
        {
            nextTimer = *timer;
            nextDate = timerDate;
        }
    }

    if( nextTimer == NULL )
    {
        this->sleep( 1000 );
    }
    else
    {
        if( nextDate > curDate )
        {
            if( this->sleep( (nextDate - curDate ) / 1000 ) )
            {
                // The sleep has been interrupted: stop here
                return;
            }
        }
        // Execute the timer callback
        if( ! nextTimer->execute() )
        {
            // Remove the timer if execute() returned false
            m_timers.remove( nextTimer );
        }
    }
}


bool X11TimerLoop::sleep( int delay )
{
    struct pollfd ufd;
    memset( &ufd, 0, sizeof (ufd) );
    ufd.fd = m_connectionNumber;
    ufd.events = POLLIN;

    // Wait for an X11 event, or timeout
    return poll( &ufd, 1, delay ) > 0;
}


#endif
