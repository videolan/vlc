/*****************************************************************************
 * MacOSX_loop.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
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

#ifndef MACOSX_LOOP_HPP
#define MACOSX_LOOP_HPP

#include "../src/os_loop.hpp"
#include <Carbon/Carbon.h>

class MacOSXDisplay;
class GenericWindow;

/// Main event loop for MacOSX (singleton)
class MacOSXLoop: public OSLoop
{
public:
    /// Get the instance of MacOSXLoop
    static OSLoop *instance( intf_thread_t *pIntf );

    /// Destroy the instance of MacOSXLoop
    static void destroy( intf_thread_t *pIntf );

    /// Enter the event loop
    virtual void run();

    /// Exit the main loop
    virtual void exit();

    // Handle a window event
    void registerWindow( GenericWindow &rGenWin, WindowRef win );

private:
    // Private because it's a singleton
    MacOSXLoop( intf_thread_t *pIntf );
    virtual ~MacOSXLoop();
    // Flag set to exit the loop
    bool m_exit;
};

#endif
