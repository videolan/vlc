/*****************************************************************************
 * win32_timer.cpp
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

#ifdef WIN32_SKINS

#include "win32_timer.hpp"
#include "../commands/cmd_generic.hpp"


void CALLBACK CallbackTimer( HWND hwnd, UINT uMsg,
                             UINT_PTR idEvent, DWORD dwTime )
{
    (void)hwnd; (void)uMsg; (void)dwTime;
    Win32Timer *pTimer = (Win32Timer*)idEvent;
    pTimer->execute();
}


Win32Timer::Win32Timer( intf_thread_t *pIntf, CmdGeneric &rCmd, HWND hWnd ):
    OSTimer( pIntf ), m_rCommand( rCmd ), m_hWnd( hWnd )
{
}


Win32Timer::~Win32Timer()
{
    stop();

    // discard possible WM_TIMER messages for this timer
    // already in the message queue and not yet dispatched
    MSG msg;
    while( !PeekMessage( &msg, m_hWnd, WM_TIMER, WM_TIMER, PM_REMOVE ) )
    {
        if( (Win32Timer*)msg.wParam != this )
            PostMessage( m_hWnd, WM_TIMER, msg.wParam, msg.lParam );
    }
}


void Win32Timer::start( int delay, bool oneShot )
{
    m_interval = delay;
    m_oneShot = oneShot;
    SetTimer( m_hWnd, (UINT_PTR)this, m_interval, (TIMERPROC)CallbackTimer );
}


void Win32Timer::stop()
{
    KillTimer( m_hWnd, (UINT_PTR)this );
}


void Win32Timer::execute()
{
    // Execute the callback
    m_rCommand.execute();

    // Stop the timer if requested
    if( m_oneShot )
        stop();
}

#endif
