/*****************************************************************************
 * win32_theme.h: Win32 implementation of the Theme class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: win32_theme.h,v 1.3 2003/04/20 20:28:39 ipkiss Exp $
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

#ifndef VLC_SKIN_WIN32_THEME
#define VLC_SKIN_WIN32_THEME

//--- GENERAL ---------------------------------------------------------------
#include <string>
using namespace std;

//--- WIN32 -----------------------------------------------------------------
#include <shellapi.h>

//---------------------------------------------------------------------------
struct intf_thread_t;
class Window;
class EventBank;
class BitmapBank;
class FontBank;

//---------------------------------------------------------------------------
class Win32Theme : public Theme
{
    protected:
        // Handles
        HINSTANCE hinst;
        HWND ParentWindow;

        // System tray icon
        NOTIFYICONDATA TrayIcon;
        HMENU SysMenu;

    public:
        // Constructor
        Win32Theme( intf_thread_t *_p_intf );
        virtual void OnLoadTheme();

        // Destructor
        virtual ~Win32Theme();

        // Specific windows methods
        HINSTANCE getInstance()       { return hinst; }
        HWND      GetParentWindow()   { return ParentWindow; }

        // !!!
        virtual void AddWindow( string name, int x, int y, bool visible,
            int fadetime, int alpha, int movealpha, bool dragdrop );
        virtual void ChangeClientWindowName( string name );

        // Taskbar && system tray
        virtual void AddSystemMenu( string name, Event *event );
        virtual void ChangeTray();
        virtual void ChangeTaskbar();
        HMENU GetSysMenu() { return SysMenu; }
};
//---------------------------------------------------------------------------


#endif

#endif
