/*****************************************************************************
 * x11_theme.cpp: X11 implementation of the Theme class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_theme.cpp,v 1.2 2003/05/18 17:48:05 asmax Exp $
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
    ParentWindow = 0;
    display = p_intf->p_sys->display;
}

//---------------------------------------------------------------------------
X11Theme::~X11Theme()
{/*
    // Unregister the window class if needed
    WNDCLASS wndclass;
    if( GetClassInfo( hinst, "SkinWindow", &wndclass ) )
    {
        UnregisterClass( "SkinWindow", hinst );
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
*/
    // Destroy parent window
/*    if( ParentWindow )
    {
        gdk_window_destroy( ParentWindow );
    }*/
}
//---------------------------------------------------------------------------
void X11Theme::OnLoadTheme()
{
/*    // The create menu
    CreateSystemMenu();
*/
    // Set the parent window attributes
/*    GdkWindowAttr attr;
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
    if( !ParentWindow )
    {
        msg_Err( p_intf, "gdk_window_new failed" );
        return;
    }*/

/*    Window root = DefaultRootWindow( display );
    ParentWindow = XCreateSimpleWindow( display, root, 0, 0, 100, 100, 0, 0, 0 );
    XSelectInput( display, ParentWindow, ExposureMask|
            KeyPressMask|KeyReleaseMask|ButtonPressMask|PointerMotionMask|
            PointerMotionHintMask| EnterWindowMask|LeaveWindowMask);

    XMapRaised( display, ParentWindow );
    XFlush( display );*/
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
{/*
    SetWindowText( ParentWindow, name.c_str() );*/
}
//---------------------------------------------------------------------------
void X11Theme::AddWindow( string name, int x, int y, bool visible,
    int fadetime, int alpha, int movealpha, bool dragdrop )
{
/*    GdkWindowAttr attr;
    attr.title = (gchar *)name.c_str();
    attr.event_mask = GDK_ALL_EVENTS_MASK;
    attr.width = 0;
    attr.height = 0;
    attr.window_type = GDK_WINDOW_TOPLEVEL;
    attr.wclass = GDK_INPUT_OUTPUT;
    attr.override_redirect = FALSE;

    gint mask = GDK_WA_NOREDIR;

    // Create the window
    GdkWindow *gwnd = gdk_window_new( NULL, &attr, mask );
    if( !gwnd )
    {
        msg_Err( p_intf, "gdk_window_new failed" );
        return;
    }

    gdk_window_set_decorations( gwnd, (GdkWMDecoration)0 );

    gdk_window_show( gwnd );*/

    // Create the window
    Window root = DefaultRootWindow( display );
    Window wnd = XCreateSimpleWindow( display, root, 0, 0, 1, 1, 0, 0, 0 );
    XSelectInput( display, wnd, ExposureMask|
            KeyPressMask|KeyReleaseMask|ButtonPressMask|PointerMotionMask|
            PointerMotionHintMask| EnterWindowMask|LeaveWindowMask);

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

    XChangeProperty( display, wnd, hints_atom, hints_atom, 32, 
                     PropModeReplace, (unsigned char *)&motifWmHints, 
                     sizeof( motifWmHints ) / sizeof( long ) );

    // Display the window
    XMapRaised( display, wnd );
    XFlush( display );

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
