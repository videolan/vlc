/*****************************************************************************
 * win32_window.h: Win32 implementation of the Window class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: win32_window.h,v 1.3 2003/04/21 21:51:16 asmax Exp $
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
 *          Emmanuel Puig    <karibu@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/

#ifdef WIN32

#ifndef VLC_SKIN_WIN32_WIN
#define VLC_SKIN_WIN32_WIN

//--- WIN32 -----------------------------------------------------------------
#include <commctrl.h>

//---------------------------------------------------------------------------
class Graphics;
class Event;

//---------------------------------------------------------------------------
class Win32Window : public SkinWindow
{
    private:
        // General parameters
        HWND hWnd;
        LPPOINT CursorPos;
        LPPOINT WindowPos;

        // Tooltip texts
        HWND ToolTipWindow;
        TOOLINFO ToolTipInfo;

        // Drag & drop
        LPDROPTARGET DropTarget;

    public:
        // Cosntructors
        Win32Window( intf_thread_t *_p_intf, HWND hwnd, int x, int y,
            bool visible, int transition, int normalalpha, int movealpha,
            bool dragdrop );

        // Destructors
        virtual ~Win32Window();

        // Event processing
        virtual bool ProcessOSEvent( Event *evt );

        // Window graphic aspect
        virtual void OSShow( bool show );
        virtual void RefreshFromImage( int x, int y, int w, int h );
        virtual void SetTransparency( int Value = -1 );
        virtual void WindowManualMove();
        virtual void WindowManualMoveInit();

        // Window methods
        virtual void Move( int left, int top );
        virtual void Size( int width, int height );

        // Specific win32 methods
        HWND GetHandle() {return hWnd; };

        // Tooltip texts
        virtual void ChangeToolTipText( string text );
};
//---------------------------------------------------------------------------

#endif

#endif
