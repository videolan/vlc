/*****************************************************************************
 * os_factory.hpp
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

#ifndef OS_FACTORY_HPP
#define OS_FACTORY_HPP

#include "skin_common.hpp"
#include "../utils/position.hpp"
#include <string>
#include <list>

#include "../src/generic_window.hpp"
class CmdGeneric;
class OSBitmap;
class OSGraphics;
class OSLoop;
class OSWindow;
class OSTooltip;
class OSPopup;
class OSTimer;


/// Abstract factory used to instantiate OS specific objects.
class OSFactory: public SkinObject
{
public:
    enum CursorType_t
    {
        kDefaultArrow,
        kResizeNS,
        kResizeWE,
        kResizeNWSE,
        kResizeNESW,
        kNoCursor,
    };

    /**
     * Initialization method overloaded in derived classes.
     * It must return false if the init failed.
     */
    virtual bool init() { return true; }

    /**
     * Get the right instance of OSFactory.
     * Returns NULL if initialization of the concrete factory failed.
     */
    static OSFactory *instance( intf_thread_t *pIntf );

    /// Delete the instance of OSFactory
    static void destroy( intf_thread_t *pIntf );

    /// Instantiate an object OSGraphics
    virtual OSGraphics *createOSGraphics( int width, int height ) = 0;

    /// Get the instance of the singleton OSLoop
    virtual OSLoop *getOSLoop() = 0;

    /// Destroy the instance of OSLoop
    virtual void destroyOSLoop() = 0;

    /// Minimize all the windows
    virtual void minimize() = 0;

    /// Restore the minimized windows
    virtual void restore() = 0;

    /// Add an icon in the system tray
    virtual void addInTray() = 0;

    /// Remove the icon from the system tray
    virtual void removeFromTray() = 0;

    /// Show the task in the task bar
    virtual void addInTaskBar() = 0;

    /// Remove the task from the task bar
    virtual void removeFromTaskBar() = 0;

    /// Instantiate an OSTimer with the given command
    virtual OSTimer *createOSTimer( CmdGeneric &rCmd ) = 0;

    /// Instantiate an object OSWindow
    virtual OSWindow *createOSWindow( GenericWindow &rWindow,
                                      bool dragDrop, bool playOnDrop,
                                      OSWindow *pParent, GenericWindow::WindowType_t ) = 0;

    /// Instantiate an object OSTooltip
    virtual OSTooltip *createOSTooltip() = 0;

    /// Instantiate an object OSPopup
    virtual OSPopup *createOSPopup() = 0;

    /// Get the directory separator
    virtual const std::string &getDirSeparator() const = 0;

    /// Get the resource path
    virtual const std::list<std::string> &getResourcePath() const = 0;

    /// Get the screen size
    virtual int getScreenWidth() const = 0;
    virtual int getScreenHeight() const = 0;

    /// Get Monitor Information for a given Window
    virtual void getMonitorInfo( OSWindow *pWindow,
                                 int* x, int* y,
                                 int* width, int* height ) const = 0;

    /// Get Monitor Information (screens numbered from 0 upwards.)
    virtual void getMonitorInfo( int num, int* x, int* y,
                                 int* width, int* height ) const = 0;

    /// Get the work area (screen area without taskbars)
    virtual SkinsRect getWorkArea() const = 0;

    /// Get the position of the mouse
    virtual void getMousePos( int &rXPos, int &rYPos ) const = 0;

    /// Change the cursor
    virtual void changeCursor( CursorType_t type ) const = 0;

    /// Delete a directory recursively
    virtual void rmDir( const std::string &rPath ) = 0;

protected:
    // Protected because it's a singleton
    OSFactory( intf_thread_t* pIntf ): SkinObject( pIntf ) { }
    virtual ~OSFactory() { }
};


#endif
