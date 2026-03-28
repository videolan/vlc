/*****************************************************************************
 * macosx_loop.hpp
 *****************************************************************************
 * Copyright (C) 2026 the VideoLAN team
 *
 * Authors: Fletcher Holt <fletcherholt649@gmail.com>
 *          Felix Paul Kühne <fkuehne@videolan.org>
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

#import <Cocoa/Cocoa.h>
#include <dispatch/dispatch.h>
#include <map>

class GenericWindow;

/// Main event loop for macOS (singleton)
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

private:
    MacOSXLoop( intf_thread_t *pIntf );
    virtual ~MacOSXLoop();

    bool m_exit;
    dispatch_semaphore_t m_exitSemaphore;
    NSTimer *m_pTimer;
    id m_pMonitor;

    /// Date and position of the last left-click
    vlc_tick_t m_lastClickTime;
    int m_lastClickPosX, m_lastClickPosY;

    /// Maximum interval between clicks for a double-click (in microsec)
    static int m_dblClickDelay;

    /// Handle an NSEvent
    void handleEvent( void *pEvent );

    /// Convert NSEvent modifiers to VLC key modifiers
    static int cocoaModToMod( unsigned int flags );

    /// Convert a unicode character to VLC key code
    static int cocoaCharToVlcKey( unichar c );
};

#endif
