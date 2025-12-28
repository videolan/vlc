/*****************************************************************************
 * macosx_factory.hpp
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

#ifndef MACOSX_FACTORY_HPP
#define MACOSX_FACTORY_HPP

#include "../src/os_factory.hpp"
#include "../src/generic_window.hpp"
#include <map>

class MacOSXTimerLoop;

/// Class used to instantiate macOS specific objects
class MacOSXFactory: public OSFactory
{
public:
    MacOSXFactory( intf_thread_t *pIntf );
    virtual ~MacOSXFactory();

    /// Initialization method
    virtual bool init();

    /// Instantiate an object OSGraphics
    virtual OSGraphics *createOSGraphics( int width, int height );

    /// Get the instance of the singleton OSLoop
    virtual OSLoop *getOSLoop();

    /// Destroy the instance of OSLoop
    virtual void destroyOSLoop();

    /// Instantiate an OSTimer with the given command
    virtual OSTimer *createOSTimer( CmdGeneric &rCmd );

    /// Minimize all the windows
    virtual void minimize();

    /// Restore the minimized windows
    virtual void restore();

    /// Add an icon in the system tray
    virtual void addInTray();

    /// Remove the icon from the system tray
    virtual void removeFromTray();

    /// Show the task in the task bar
    virtual void addInTaskBar();

    /// Remove the task from the task bar
    virtual void removeFromTaskBar();

    /// Instantiate an OSWindow object
    virtual OSWindow *createOSWindow( GenericWindow &rWindow,
                                      bool dragDrop, bool playOnDrop,
                                      OSWindow *pParent,
                                      GenericWindow::WindowType_t type );

    /// Instantiate an object OSTooltip
    virtual OSTooltip *createOSTooltip();

    /// Instantiate an object OSPopup
    virtual OSPopup *createOSPopup();

    /// Get the directory separator
    virtual const std::string &getDirSeparator() const { return m_dirSep; }

    /// Get the resource path
    virtual const std::list<std::string> &getResourcePath() const
        { return m_resourcePath; }

    /// Get the screen size
    virtual int getScreenWidth() const;
    virtual int getScreenHeight() const;

    /// Get Monitor Information
    virtual void getMonitorInfo( OSWindow *pWindow,
                                 int* x, int* y,
                                 int* width, int* height ) const;
    virtual void getMonitorInfo( int numScreen,
                                 int* x, int* y,
                                 int* width, int* height ) const;

    /// Get the work area (screen area without taskbars)
    virtual SkinsRect getWorkArea() const;

    /// Get the position of the mouse
    virtual void getMousePos( int &rXPos, int &rYPos ) const;

    /// Change the cursor
    virtual void changeCursor( CursorType_t type ) const;

    /// Delete a directory recursively
    virtual void rmDir( const std::string &rPath );

    /// Get the timer loop
    MacOSXTimerLoop *getTimerLoop() const { return m_pTimerLoop; }

    /// Map to find the GenericWindow* associated with an NSWindow
    std::map<void*, GenericWindow*> m_windowMap;

private:
    /// Timer loop
    MacOSXTimerLoop *m_pTimerLoop;
    /// Directory separator
    const std::string m_dirSep;
    /// Resource path
    std::list<std::string> m_resourcePath;
    /// Screen dimensions
    int m_screenWidth, m_screenHeight;
};

#endif
