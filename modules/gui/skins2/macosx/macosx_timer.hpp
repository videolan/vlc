/*****************************************************************************
 * macosx_timer.hpp
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

#ifndef MACOSX_TIMER_HPP
#define MACOSX_TIMER_HPP

#include "../src/os_timer.hpp"

#include <list>

class CmdGeneric;
class MacOSXTimerLoop;

/// macOS specific timer implementation
class MacOSXTimer: public OSTimer
{
public:
    MacOSXTimer( intf_thread_t *pIntf, CmdGeneric &rCmd, MacOSXTimerLoop *pTimerLoop );
    virtual ~MacOSXTimer();

    /// (Re)start the timer with the given delay (in ms).
    /// If oneShot is true, stop it after the first execution of the callback.
    virtual void start( int delay, bool oneShot );

    /// Stop the timer
    virtual void stop();

    /// Get the next date at which the timer should fire
    vlc_tick_t getNextDate() const { return m_nextDate; }

    /// Execute the callback
    /// Returns false if the timer must be removed after
    bool execute();

private:
    /// Command to execute
    CmdGeneric &m_rCommand;
    /// Timer loop
    MacOSXTimerLoop *m_pTimerLoop;
    /// Interval between executions (in ticks)
    vlc_tick_t m_interval;
    /// Next date at which the timer must be executed
    vlc_tick_t m_nextDate;
    /// Flag to indicate one-shot timer
    bool m_oneShot;
};


/// Class to manage a set of timers
class MacOSXTimerLoop: public SkinObject
{
public:
    MacOSXTimerLoop( intf_thread_t *pIntf );
    virtual ~MacOSXTimerLoop();

    /// Add a timer to the manager
    void addTimer( MacOSXTimer &rTimer );

    /// Remove a timer from the manager
    void removeTimer( MacOSXTimer &rTimer );

    /// Check and execute timers that have expired
    void checkTimers();

private:
    /// List of active timers
    std::list<MacOSXTimer*> m_timers;
};

#endif
