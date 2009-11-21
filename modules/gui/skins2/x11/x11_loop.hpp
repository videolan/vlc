/*****************************************************************************
 * x11_loop.hpp
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

#ifndef X11_LOOP_HPP
#define X11_LOOP_HPP

#include <X11/Xlib.h>
#include "../src/os_loop.hpp"

#include <map>

class X11Display;
class GenericWindow;

/// Main event loop for X11 (singleton)
class X11Loop: public OSLoop
{
public:
    /// Get the instance of X11Loop
    static OSLoop *instance( intf_thread_t *pIntf, X11Display &rDisplay );

    /// Destroy the instance of X11Loop
    static void destroy( intf_thread_t *pIntf );

    /// Enter the event loop
    virtual void run();

    /// Exit the main loop
    virtual void exit();

private:
    /// X11 Display
    X11Display &m_rDisplay;
    /// Flag set on exit
    bool m_exit;
    /// Date and position of the last left-click
    mtime_t m_lastClickTime;
    int m_lastClickPosX, m_lastClickPosY;
    /// Maximum interval between clicks for a double-click (in microsec)
    static int m_dblClickDelay;
    /// Map associating special (i.e. non ascii) virtual key codes with
    /// internal vlc key codes
    typedef std::map<KeySym, int> keymap_t;
    static keymap_t m_keymap;
    /// Translate X11 KeySyms to VLC key codes.
    static int keysymToVlcKey( KeySym keysym )
    {
        keymap_t::const_iterator i=m_keymap.find(keysym);
        return i!=m_keymap.end() ? (i->second) : keysym;
    }
    static int X11ModToMod( unsigned state );

    // Private because it's a singleton
    X11Loop( intf_thread_t *pIntf, X11Display &rDisplay );
    virtual ~X11Loop();

    /// Handle the next X11 event
    void handleX11Event();
};

#endif
