/*****************************************************************************
 * win32_timer.cpp
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

#ifdef WIN32_SKINS

#include "win32_timer.hpp"
#include "../commands/cmd_generic.hpp"


void CALLBACK CallbackTimer( HWND hwnd, UINT uMsg,
                             UINT_PTR idEvent, DWORD dwTime )
{
    Win32Timer *pTimer = (Win32Timer*)idEvent;
    if( pTimer != NULL )
    {
        pTimer->execute();
    }
}


Win32Timer::Win32Timer( intf_thread_t *pIntf, CmdGeneric &rCmd, HWND hWnd ):
    OSTimer( pIntf ), m_rCommand( rCmd ), m_hWnd( hWnd )
{
}


Win32Timer::~Win32Timer()
{
    stop();
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

    // Restart the timer if needed
    if( ! m_oneShot )
    {
        start( m_interval, m_oneShot );
    }
}

#endif
