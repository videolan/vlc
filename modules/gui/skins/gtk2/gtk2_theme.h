/*****************************************************************************
 * gtk2_theme.h: GTK2 implementation of the Theme class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: gtk2_theme.h,v 1.1 2003/04/12 21:43:27 asmax Exp $
 *
 * Authors: Cyril Deguet     <asmax@videolan.org>
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


#ifndef VLC_SKIN_GTK2_THEME
#define VLC_SKIN_GTK2_THEME

//--- GENERAL ---------------------------------------------------------------
#include <string>
using namespace std;

//--- GTK2 -----------------------------------------------------------------
//#include <shellapi.h>

//---------------------------------------------------------------------------
struct intf_thread_t;
class Window;
class EventBank;
class BitmapBank;
class FontBank;
class LogWindow;

//---------------------------------------------------------------------------
class GTK2Theme : public Theme
{
    protected:
        // Handles
/*        HINSTANCE hinst;
        HWND ParentWindow;

        // System tray icon
        NOTIFYICONDATA TrayIcon;
        HMENU SysMenu;
*/
    public:
        // Constructor
        GTK2Theme( intf_thread_t *_p_intf );
        virtual void OnLoadTheme();

        // Destructor
        virtual ~GTK2Theme();
/*
        // Specific windows methods
        HINSTANCE getInstance()       { return hinst; }
        HWND      GetLogHandle();
        HWND      GetParentWindow()   { return ParentWindow; }
*/
        // !!!
        virtual void AddWindow( string name, int x, int y, bool visible,
            int fadetime, int alpha, int movealpha, bool dragdrop );
        virtual void ChangeClientWindowName( string name );

        // Taskbar && system tray
        virtual void AddSystemMenu( string name, Event *event );
        virtual void ChangeTray();
        virtual void ChangeTaskbar();
//        HMENU GetSysMenu() { return SysMenu; }
};
//---------------------------------------------------------------------------


#endif

