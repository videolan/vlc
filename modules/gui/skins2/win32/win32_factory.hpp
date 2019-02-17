/*****************************************************************************
 * win32_factory.hpp
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef WIN32_FACTORY_HPP
#define WIN32_FACTORY_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <windows.h>
#include <shellapi.h>
// #include <wingdi.h>
#include "../src/os_factory.hpp"
#include "../src/generic_window.hpp"

#include <map>


/// Class used to instanciate Win32 specific objects
class Win32Factory: public OSFactory
{
public:
    Win32Factory( intf_thread_t *pIntf );
    virtual ~Win32Factory();

    /// Initialization method
    virtual bool init();

    /// Instantiate an object OSGraphics
    virtual OSGraphics *createOSGraphics( int width, int height );

    /// Get the instance of the singleton OSLoop
    virtual OSLoop *getOSLoop();

    /// Destroy the instance of OSLoop
    virtual void destroyOSLoop();

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

    /// Instantiate an OSTimer with the given command
    virtual OSTimer *createOSTimer( CmdGeneric &rCmd );

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

    /// Map to find the GenericWindow associated with a Win32Window
    std::map<HWND, GenericWindow*> m_windowMap;

    HWND getParentWindow() { return m_hParentWindow; }

    /// Callback function (Windows Procedure)
    static LRESULT CALLBACK Win32Proc( HWND hwnd, UINT uMsg,
                                       WPARAM wParam, LPARAM lParam );

    /// Callback (enumerate multiple screens)
    static BOOL CALLBACK MonitorEnumProc( HMONITOR hMonitor, HDC hdcMonitor,
                                          LPRECT lprcMonitor, LPARAM dwData );
private:
    /// Handle of the instance
    HINSTANCE m_hInst;
    /// Handle of the parent window
    HWND m_hParentWindow;
    /// Structure for the system tray
    NOTIFYICONDATA m_trayIcon;
    /// Handle on msimg32.dll (for TransparentBlt)
    HINSTANCE m_hMsimg32;
    /// Handle on user32.dll (for SetLayeredWindowAttributes)
    HINSTANCE m_hUser32;
    /// Directory separator
    const std::string m_dirSep;
    /// Resource path
    std::list<std::string> m_resourcePath;
    /// Monitors detected
    std::list<HMONITOR> m_monitorList;
};


#endif
