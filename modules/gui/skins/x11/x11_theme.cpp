/*****************************************************************************
 * x11_theme.cpp: X11 implementation of the Theme class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_theme.cpp,v 1.13 2003/06/11 21:46:57 asmax Exp $
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

#ifdef X11_SKINS

//--- X11 -------------------------------------------------------------------
#include <X11/Xlib.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "../os_api.h"
#include "../src/banks.h"
#include "../src/window.h"
#include "../os_window.h"
#include "../src/event.h"
#include "../os_event.h"
#include "../src/theme.h"
#include "../os_theme.h"
#include "../src/vlcproc.h"
#include "../src/skin_common.h"


//---------------------------------------------------------------------------
void SkinManage( intf_thread_t *p_intf );


//---------------------------------------------------------------------------
// THEME
//---------------------------------------------------------------------------
X11Theme::X11Theme( intf_thread_t *_p_intf ) : Theme( _p_intf )
{
    //Initialize values
    p_intf = _p_intf;
    display = p_intf->p_sys->display;
}

//---------------------------------------------------------------------------
X11Theme::~X11Theme()
{
}
//---------------------------------------------------------------------------
void X11Theme::OnLoadTheme()
{
/*    // The create menu
    CreateSystemMenu();
*/
}
//---------------------------------------------------------------------------
void X11Theme::AddSystemMenu( string name, Event *event )
{/*
    if( name == "SEPARATOR" )
    {
        AppendMenu( SysMenu, MF_SEPARATOR, 0, NULL );
    }
    else
    {
        AppendMenu( SysMenu, MF_STRING, (unsigned int)event,
                    (char *)name.c_str() );
    }*/
}
//---------------------------------------------------------------------------
void X11Theme::ChangeClientWindowName( string name )
{
    XLOCK;
    XStoreName( display, p_intf->p_sys->mainWin, name.c_str() );
    XUNLOCK;
}
//---------------------------------------------------------------------------
void X11Theme::AddWindow( string name, int x, int y, bool visible,
    int fadetime, int alpha, int movealpha, bool dragdrop )
{
    // Create the window
    Window root = DefaultRootWindow( display );
    XSetWindowAttributes attr;
    XLOCK;
    Window wnd = XCreateWindow( display, root, 0, 0, 1, 1, 0, 0, InputOutput,
                                CopyFromParent, 0, &attr );
    XSelectInput( display, wnd, ExposureMask|StructureNotifyMask|
                  KeyPressMask|KeyReleaseMask|ButtonPressMask|
                  ButtonReleaseMask|PointerMotionMask|EnterWindowMask|
                  LeaveWindowMask);
    XUNLOCK;

    // Changing decorations
    struct {
        unsigned long flags;
        unsigned long functions;
        unsigned long decorations;
        long input_mode;
        unsigned long status;
    } motifWmHints;

    Atom hints_atom = XInternAtom( display, "_MOTIF_WM_HINTS", False );

    motifWmHints.flags = 2;    // MWM_HINTS_DECORATIONS;
    motifWmHints.decorations = 0;
    XLOCK;
    XChangeProperty( display, wnd, hints_atom, hints_atom, 32, 
                     PropModeReplace, (unsigned char *)&motifWmHints, 
                     sizeof( motifWmHints ) / sizeof( long ) );

    // Change the window title
    XStoreName( display, wnd, name.c_str() );
    XUNLOCK;

    WindowList.push_back( (SkinWindow *)new OSWindow( p_intf, wnd, x, y, 
        visible, fadetime, alpha, movealpha, dragdrop, name ) ) ;
}
//---------------------------------------------------------------------------
void X11Theme::ChangeTray()
{/*
    if( ShowInTray )
    {
        Shell_NotifyIcon( NIM_DELETE, &TrayIcon );
        ShowInTray = false;
    }
    else
    {
        Shell_NotifyIcon( NIM_ADD, &TrayIcon );
        ShowInTray = true;
    }*/
}
//---------------------------------------------------------------------------
void X11Theme::ChangeTaskbar()
{/*
    if( ShowInTaskbar )
    {
        ShowWindow( ParentWindow, SW_HIDE );
        SetWindowLongPtr( ParentWindow, GWL_EXSTYLE,
                          WS_EX_LAYERED|WS_EX_TOOLWINDOW );
        ShowWindow( ParentWindow, SW_SHOW );
        ShowInTaskbar = false;
    }
    else
    {
        ShowWindow( ParentWindow, SW_HIDE );
        SetWindowLongPtr( ParentWindow, GWL_EXSTYLE,
                          WS_EX_LAYERED|WS_EX_APPWINDOW );
        ShowWindow( ParentWindow, SW_SHOW );
        ShowInTaskbar = true;
    }*/
}
//---------------------------------------------------------------------------

#endif
