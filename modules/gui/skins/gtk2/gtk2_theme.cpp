/*****************************************************************************
 * gtk2_theme.cpp: GTK2 implementation of the Theme class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: gtk2_theme.cpp,v 1.12 2003/04/15 20:33:58 karibu Exp $
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

#if !defined WIN32

//--- GTK2 -----------------------------------------------------------------
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "os_api.h"
#include "banks.h"
#include "window.h"
#include "os_window.h"
#include "event.h"
#include "os_event.h"
#include "theme.h"
#include "os_theme.h"
#include "dialog.h"
#include "os_dialog.h"
#include "vlcproc.h"
#include "skin_common.h"


//---------------------------------------------------------------------------
void SkinManage( intf_thread_t *p_intf );


//---------------------------------------------------------------------------
// THEME
//---------------------------------------------------------------------------
GTK2Theme::GTK2Theme( intf_thread_t *_p_intf ) : Theme( _p_intf )
{
/*
    // Get instance handle
    hinst = GetModuleHandle( NULL );

    // Create window class
    WNDCLASS SkinWindow;

    SkinWindow.style = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;
    SkinWindow.lpfnWndProc = (WNDPROC) GTK2Proc;
    SkinWindow.lpszClassName = "SkinWindow";
    SkinWindow.lpszMenuName = NULL;
    SkinWindow.cbClsExtra = 0;
    SkinWindow.cbWndExtra = 0;
    SkinWindow.hbrBackground = HBRUSH (COLOR_WINDOW);
    SkinWindow.hCursor = LoadCursor( NULL , IDC_ARROW );
    SkinWindow.hIcon = LoadIcon( hinst, "VLC_ICON" );
    SkinWindow.hInstance = hinst;

    if( !RegisterClass( &SkinWindow ) )
    {
        WNDCLASS wndclass;

        // Check why it failed. If it's because the class already exists
        // then fine, otherwise return with an error.
        if( !GetClassInfo( hinst, "SkinWindow", &wndclass ) )
        {
            msg_Err( p_intf, "Cannot register window class" );
            return;
        }
    }
*/
    //Initialize value
    ParentWindow = NULL;

}

//---------------------------------------------------------------------------
GTK2Theme::~GTK2Theme()
{/*
    // Unregister the window class if needed
    WNDCLASS wndclass;
    if( GetClassInfo( hinst, "SkinWindow", &wndclass ) )
    {
        UnregisterClass( "SkinWindow", hinst );
    }
    if( GetClassInfo( hinst, "LogWindow", &wndclass ) )
    {
        UnregisterClass( "LogWindow", hinst );
    }
    if( GetClassInfo( hinst, "ParentWindow", &wndclass ) )
    {
        UnregisterClass( "ParentWindow", hinst );
    }

    // Delete tray icon if exists
    if( ShowInTray )
    {
        Shell_NotifyIcon( NIM_DELETE, &TrayIcon );
    }

    // Destroy parent window
    if( ParentWindow )
    {
        DestroyWindow( ParentWindow );
    }*/
}
//---------------------------------------------------------------------------
void GTK2Theme::OnLoadTheme()
{/*
    // Create window class
    WNDCLASS ParentClass;
    ParentClass.style = CS_VREDRAW|CS_HREDRAW|CS_DBLCLKS;
    ParentClass.lpfnWndProc = (WNDPROC) GTK2Proc;
    ParentClass.lpszClassName = "ParentWindow";
    ParentClass.lpszMenuName = NULL;
    ParentClass.cbClsExtra = 0;
    ParentClass.cbWndExtra = 0;
    ParentClass.hbrBackground = HBRUSH (COLOR_WINDOW);
    ParentClass.hCursor = LoadCursor( NULL , IDC_ARROW );
    ParentClass.hIcon = LoadIcon( hinst, "VLC_ICON" );
    ParentClass.hInstance = hinst;

    // register class and check it
    if( !RegisterClass( &ParentClass ) )
    {
        WNDCLASS wndclass;

        // Check why it failed. If it's because the class already exists
        // then fine, otherwise return with an error.
        if( !GetClassInfo( hinst, "ParentWindow", &wndclass ) )
        {
            msg_Err( p_intf, "Cannot register window class" );
            return;
        }
    }

    // Create Window
    ParentWindow = CreateWindowEx( WS_EX_LAYERED|WS_EX_TOOLWINDOW,
        "ParentWindow", "VLC Media Player",
        WS_SYSMENU,
        0, 0, 0, 0, 0, 0, hinst, NULL );

    // Store with it a pointer to the interface thread
    SetWindowLongPtr( ParentWindow, GWLP_USERDATA, (LONG_PTR)p_intf );
    ShowWindow( ParentWindow, SW_SHOW );

    // System tray icon
    TrayIcon.cbSize = sizeof( PNOTIFYICONDATA );
    TrayIcon.hWnd = ParentWindow;
    TrayIcon.uID = 42;
    TrayIcon.uFlags = NIF_ICON|NIF_TIP|NIF_MESSAGE;
    TrayIcon.uCallbackMessage = WM_RBUTTONDOWN;
    TrayIcon.hIcon = LoadIcon( hinst, "VLC_ICON" );
    strcpy( TrayIcon.szTip, "VLC Media Player" );

    // Remove default entries from system menu popup
    SysMenu = GetSystemMenu( ParentWindow, false );
    RemoveMenu( SysMenu, SC_RESTORE,  MF_BYCOMMAND );
    RemoveMenu( SysMenu, SC_MOVE,     MF_BYCOMMAND );
    RemoveMenu( SysMenu, SC_SIZE,     MF_BYCOMMAND );
    RemoveMenu( SysMenu, SC_MINIMIZE, MF_BYCOMMAND );
    RemoveMenu( SysMenu, SC_MAXIMIZE, MF_BYCOMMAND );
    RemoveMenu( SysMenu, SC_CLOSE,    MF_BYCOMMAND );
    RemoveMenu( SysMenu, 0,           MF_BYPOSITION );

    // The create menu
    CreateSystemMenu();
*/
    // Set the parent window attributes
    GdkWindowAttr attr;
    attr.title = "VLC Media Player";
    attr.event_mask = GDK_ALL_EVENTS_MASK;
    attr.x = 0;
    attr.y = 0;
    attr.width = 0;
    attr.height = 0;
    attr.window_type = GDK_WINDOW_TOPLEVEL;
    attr.wclass = GDK_INPUT_ONLY;
    attr.override_redirect = FALSE;
    
    gint mask = GDK_WA_TITLE|GDK_WA_X|GDK_WA_Y|GDK_WA_NOREDIR;
    
    // Create the parent window
    ParentWindow = gdk_window_new( NULL, &attr, mask);

//    gdk_window_show( ParentWindow );
}
//---------------------------------------------------------------------------
void GTK2Theme::AddSystemMenu( string name, Event *event )
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
void GTK2Theme::ChangeClientWindowName( string name )
{/*
    SetWindowText( ParentWindow, name.c_str() );*/
}
//---------------------------------------------------------------------------
void GTK2Theme::AddWindow( string name, int x, int y, bool visible,
    int fadetime, int alpha, int movealpha, bool dragdrop )
{/*
    HWND hwnd;

    hwnd = CreateWindowEx( WS_EX_LAYERED|WS_EX_TOOLWINDOW,
        "SkinWindow", name.c_str(), WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT,
        0, 0, ParentWindow, 0, hinst, NULL );

    if( !hwnd )
    {
        msg_Err( p_intf, "CreateWindow failed" );
        return;
    }

    SetWindowLongPtr( hwnd, GWLP_USERDATA, (LONG_PTR)p_intf );

    WindowList.push_back( (Window *)new OSWindow( p_intf, hwnd, x, y, visible,
        fadetime, alpha, movealpha, dragdrop ) ) ;*/

    GdkWindowAttr attr;
    attr.title = (gchar *)name.c_str();
    attr.event_mask = GDK_ALL_EVENTS_MASK;
    attr.width = 0;
    attr.height = 0;
    attr.window_type = GDK_WINDOW_TOPLEVEL;
    attr.wclass = GDK_INPUT_OUTPUT;
    attr.override_redirect = TRUE;

    gint mask = GDK_WA_NOREDIR;

    // Create the window
    GdkWindow *gwnd = gdk_window_new( NULL, &attr, mask );
    if( !gwnd )
    {
        msg_Err( p_intf, "CreateWindow failed" );
        return;
    }

    gdk_window_show( gwnd );

    WindowList.push_back( (Window *)new OSWindow( p_intf, gwnd, x, y, visible,
        fadetime, alpha, movealpha, dragdrop, name ) ) ;

}
//---------------------------------------------------------------------------
/*HWND GTK2Theme::GetLogHandle()
{
    if( Log != NULL )
        return ( (GTK2LogWindow *)Log )->GetWindow();
    else
        return NULL;
}*/
//---------------------------------------------------------------------------
void GTK2Theme::ChangeTray()
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
void GTK2Theme::ChangeTaskbar()
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
