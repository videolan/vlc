/*****************************************************************************
 * win32_factory.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: win32_factory.hpp,v 1.1 2004/01/03 23:31:34 asmax Exp $
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

#ifndef WIN32_FACTORY_HPP
#define WIN32_FACTORY_HPP

#include <windows.h>
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

        /// Instantiate an object OSGraphics.
        virtual OSGraphics *createOSGraphics( int width, int height );

        /// Get the instance of the singleton OSLoop.
        virtual OSLoop *getOSLoop();

        /// Destroy the instance of OSLoop.
        virtual void destroyOSLoop();

        /// Instantiate an OSTimer with the given callback
        virtual OSTimer *createOSTimer( const Callback &rCallback );

        /// Instantiate an OSWindow object
        virtual OSWindow *createOSWindow( GenericWindow &rWindow,
                                          bool dragDrop, bool playOnDrop );

        /// Instantiate an object OSTooltip.
        virtual OSTooltip *createOSTooltip();

        /// Get the directory separator
        virtual const string getDirSeparator() const;

        /// Get the screen size
        virtual int getScreenWidth() const;
        virtual int getScreenHeight() const;

        /// Get the work area (screen area without taskbars)
        virtual Rect getWorkArea() const;

        /// Get the position of the mouse
        virtual void getMousePos( int &rXPos, int &rYPos ) const;

        /// Delete a directory recursively
        virtual void rmDir( const string &rPath );

        /// Map to find the GenericWindow associated with a Win32Window
        map<HWND, GenericWindow*> m_windowMap;


        /// Functions dynamically loaded from the dll, because they don't exist
        /// on win9x
        // We dynamically load msimg32.dll to get a pointer to TransparentBlt()
        BOOL (WINAPI *TransparentBlt)( HDC, int, int, int, int,
                                       HDC, int, int, int, int, UINT );
        // Idem for user32.dll and SetLayeredWindowAttributes()
        BOOL (WINAPI *SetLayeredWindowAttributes)( HWND, COLORREF,
                                                   BYTE, DWORD );

    private:
        /// Handle of the instance
        HINSTANCE m_hInst;
        /// Handle of the parent window
        HWND m_hParentWindow;
        /// Handle on msimg32.dll (for TransparentBlt)
        HINSTANCE m_hMsimg32;
        /// Handle on user32.dll (for SetLayeredWindowAttributes)
        HINSTANCE m_hUser32;
};


#endif
