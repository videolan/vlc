/*****************************************************************************
 * x11_theme.h: X11 implementation of the Theme class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_theme.h,v 1.2 2003/06/01 16:39:49 asmax Exp $
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


#ifndef VLC_SKIN_X11_THEME
#define VLC_SKIN_X11_THEME

//--- GENERAL ---------------------------------------------------------------
#include <string>
using namespace std;

//--- X11 -------------------------------------------------------------------
#include <X11/Xlib.h>

//---------------------------------------------------------------------------
struct intf_thread_t;
class SkinWindow;
class EventBank;
class BitmapBank;
class FontBank;

//---------------------------------------------------------------------------
class X11Theme : public Theme
{
    protected:
        intf_thread_t *p_intf;
        // Handles
        Display *display;
/*
        // System tray icon
        NOTIFYICONDATA TrayIcon;
        HMENU SysMenu;
*/
    public:
        // Constructor
        X11Theme( intf_thread_t *_p_intf );
        virtual void OnLoadTheme();

        // Destructor
        virtual ~X11Theme();

        // Specific methods
        Display * GetDisplay()   { return display; }

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

