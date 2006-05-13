/*****************************************************************************
 * win32_factory.hpp
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef WIN32_FACTORY_HPP
#define WIN32_FACTORY_HPP

#ifndef _WIN32_WINNT
#   define _WIN32_WINNT 0x0500
#endif

#include <windows.h>
#include <shellapi.h>
#include "../src/os_factory.hpp"
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

        ///
        virtual void minimize();

        /// Instantiate an OSTimer with the given command
        virtual OSTimer *createOSTimer( CmdGeneric &rCmd );

        /// Instantiate an OSWindow object
        virtual OSWindow *createOSWindow( GenericWindow &rWindow,
                                          bool dragDrop, bool playOnDrop,
                                          OSWindow *pParent );

        /// Instantiate an object OSTooltip
        virtual OSTooltip *createOSTooltip();

        /// Instantiate an object OSPopup
        virtual OSPopup *createOSPopup();

        /// Get the directory separator
        virtual const string &getDirSeparator() const { return m_dirSep; }

        /// Get the resource path
        virtual const list<string> &getResourcePath() const
            { return m_resourcePath; }

        /// Get the screen size
        virtual int getScreenWidth() const;
        virtual int getScreenHeight() const;

        /// Get the work area (screen area without taskbars)
        virtual Rect getWorkArea() const;

        /// Get the position of the mouse
        virtual void getMousePos( int &rXPos, int &rYPos ) const;

        /// Change the cursor
        virtual void changeCursor( CursorType_t type ) const;

        /// Delete a directory recursively
        virtual void rmDir( const string &rPath );

        /// Map to find the GenericWindow associated with a Win32Window
        map<HWND, GenericWindow*> m_windowMap;

        /// Functions dynamically loaded from the dll, because they don't exist
        /// on Win9x/NT4
        // We dynamically load msimg32.dll to get a pointer to TransparentBlt()
        BOOL (WINAPI *TransparentBlt)( HDC, int, int, int, int,
                                       HDC, int, int, int, int, UINT );
        BOOL (WINAPI *AlphaBlend)( HDC, int, int, int, int,
                                   HDC, int, int, int, int, BLENDFUNCTION );

        // Idem for user32.dll and SetLayeredWindowAttributes()
        BOOL (WINAPI *SetLayeredWindowAttributes)( HWND, COLORREF,
                                                   BYTE, DWORD );

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
        const string m_dirSep;
        /// Resource path
        list<string> m_resourcePath;
};


#endif
