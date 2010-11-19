/*****************************************************************************
 * win32_loop.hpp
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

#ifndef WIN32_LOOP_HPP
#define WIN32_LOOP_HPP

#include "../events/evt_mouse.hpp"
#include "../src/os_loop.hpp"
#include <map>


class GenericWindow;

/// Main event loop for Win32
class Win32Loop: public OSLoop
{
public:
    /// Get the instance of Win32Loop
    static OSLoop *instance( intf_thread_t *pIntf );

    /// Destroy the instance of Win32Loop
    static void destroy( intf_thread_t *pIntf );

    /// Enter the event loop
    virtual void run();

    /// Exit the main loop
    virtual void exit();

    /// called by the window procedure callback
    virtual LRESULT CALLBACK processEvent( HWND hwnd, UINT msg,
                                           WPARAM wParam, LPARAM lParam );

private:
    // Private because it is a singleton
    Win32Loop( intf_thread_t *pIntf );
    virtual ~Win32Loop();

    /// Map associating special (i.e. non ascii) virtual key codes with
    /// internal vlc key codes
    map<int, int> virtKeyToVlcKey;

    /// Helper function to find the modifier in a Windows message
    int getMod( WPARAM wParam ) const;
};

#endif
