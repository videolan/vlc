/*****************************************************************************
 * gtk2_window.cpp: GTK2 implementation of the Window class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: gtk2_window.cpp,v 1.5 2003/04/13 22:55:15 asmax Exp $
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

//--- GENERAL ---------------------------------------------------------------
//#include <math.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- GTK2 ------------------------------------------------------------------
#include <gdk/gdk.h>

//--- SKIN ------------------------------------------------------------------
#include "os_api.h"
#include "anchor.h"
#include "generic.h"
#include "window.h"
#include "os_window.h"
#include "event.h"
#include "os_event.h"
#include "graphics.h"
#include "os_graphics.h"
#include "skin_common.h"
#include "theme.h"



//---------------------------------------------------------------------------
// Fading API
//---------------------------------------------------------------------------
/*#define LWA_COLORKEY  0x00000001
#define LWA_ALPHA     0x00000002
typedef BOOL (WINAPI *SLWA)(HWND, COLORREF, BYTE, DWORD);
HMODULE hModule = LoadLibrary( "user32.dll" );
SLWA SetLayeredWindowAttributes =
    (SLWA)GetProcAddress( hModule, "SetLayeredWindowAttributes" );
*/

//---------------------------------------------------------------------------
// Skinable Window
//---------------------------------------------------------------------------
GTK2Window::GTK2Window( intf_thread_t *p_intf, GdkWindow *gwnd, int x, int y,
    bool visible, int transition, int normalalpha, int movealpha,
    bool dragdrop )
    : Window( p_intf, x, y, visible, transition, normalalpha, movealpha,
              dragdrop )
{
    // Set handles
    gWnd           = gwnd;
/*
    // Set position parameters
    CursorPos    = new POINT;
    WindowPos    = new POINT;

    // Create Tool Tip Window
    ToolTipWindow = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        hWnd, 0, GetModuleHandle( NULL ), 0);

    // Create Tool Tip infos
    ToolTipInfo.cbSize = sizeof(TOOLINFO);
    ToolTipInfo.uFlags = TTF_SUBCLASS|TTF_IDISHWND;
    ToolTipInfo.hwnd = hWnd;
    ToolTipInfo.hinst = GetModuleHandle( NULL );
    ToolTipInfo.uId = (unsigned int)hWnd;
    ToolTipInfo.lpszText = NULL;
    ToolTipInfo.rect.left = ToolTipInfo.rect.top = 0;
        ToolTipInfo.rect.right = ToolTipInfo.rect.bottom = 0;

    SendMessage( ToolTipWindow, TTM_ADDTOOL, 0,
                    (LPARAM)(LPTOOLINFO) &ToolTipInfo );

    // Drag & drop
    if( DragDrop )
    {
        // Initialize the OLE library
        OleInitialize( NULL );
        DropTarget = (LPDROPTARGET) new GTK2DropObject();
        // register the listview as a drop target
        RegisterDragDrop( hWnd, DropTarget );
    }
*/
    // Create Tool Tip window
/*    GdkWindowAttr attr;
    attr.event_mask = GDK_ALL_EVENTS_MASK;
    attr.width = 100;
    attr.height = 100;
    attr.window_type = GDK_WINDOW_CHILD;
    attr.wclass = GDK_INPUT_OUTPUT;
    gint mask = 0;
    ToolTipWindow = gdk_window_new( gwnd, &attr, mask);*/

}
//---------------------------------------------------------------------------
GTK2Window::~GTK2Window()
{
/*    delete CursorPos;
    delete WindowPos;

    if( hWnd != NULL )
    {
        DestroyWindow( hWnd );
    }
    if( ToolTipWindow != NULL )
    {
        DestroyWindow( ToolTipWindow );
    }
    if( DragDrop )
    {
        // Remove the listview from the list of drop targets
        RevokeDragDrop( hWnd );
        DropTarget->Release();
        // Uninitialize the OLE library
        OleUninitialize();
    }
*/
}
//---------------------------------------------------------------------------
void GTK2Window::OSShow( bool show )
{
    if( show )
    {
        gdk_window_show( gWnd );
    }
    else
    {
        gdk_window_hide( gWnd );
    }
}
//---------------------------------------------------------------------------
bool GTK2Window::ProcessOSEvent( Event *evt )
{
/*    unsigned int msg = evt->GetMessage();
    unsigned int p1  = evt->GetParam1();
    int          p2  = evt->GetParam2();

    switch( msg )
    {
        case WM_PAINT:
            HDC DC;
            PAINTSTRUCT Infos;
            DC = BeginPaint( hWnd , &Infos );
            EndPaint( hWnd , &Infos );
            RefreshFromImage( 0, 0, Width, Height );
            return true;

        case WM_MOUSEMOVE:
            TRACKMOUSEEVENT TrackEvent;
            TrackEvent.cbSize      = sizeof( TRACKMOUSEEVENT );
            TrackEvent.dwFlags     = TME_LEAVE;
            TrackEvent.hwndTrack   = hWnd;
            TrackEvent.dwHoverTime = 1;
            TrackMouseEvent( &TrackEvent );
            if( p1 == MK_LBUTTON )
                MouseMove( LOWORD( p2 ), HIWORD( p2 ), 1 );
            else if( p1 == MK_RBUTTON )
                MouseMove( LOWORD( p2 ), HIWORD( p2 ), 2 );
            else
                MouseMove( LOWORD( p2 ), HIWORD( p2 ), 0 );

            return true;

        case WM_LBUTTONDOWN:
            SetCapture( hWnd );
            MouseDown( LOWORD( p2 ), HIWORD( p2 ), 1 );
            return true;

        case WM_LBUTTONUP:
            ReleaseCapture();
            MouseUp( LOWORD( p2 ), HIWORD( p2 ), 1 );
            return true;

        case WM_RBUTTONDOWN:
            MouseDown( LOWORD( p2 ), HIWORD( p2 ), 2 );
            return true;

        case WM_RBUTTONUP:
            MouseUp( LOWORD( p2 ), HIWORD( p2 ), 2 );
            return true;

        case WM_LBUTTONDBLCLK:
            MouseDblClick( LOWORD( p2 ), HIWORD( p2 ), 1 );
            return true;

        case WM_MOUSELEAVE:
            OSAPI_PostMessage( this, WINDOW_LEAVE, 0, 0 );
            return true;

        default:
            return false;
    }*/
}
//---------------------------------------------------------------------------
void GTK2Window::SetTransparency( int Value )
{
/*    if( Value > -1 )
        Alpha = Value;
    SetLayeredWindowAttributes( hWnd, 0, Alpha, LWA_ALPHA | LWA_COLORKEY );
    UpdateWindow( hWnd );*/
}
//---------------------------------------------------------------------------
void GTK2Window::RefreshFromImage( int x, int y, int w, int h )
{
    // Initialize painting
/*    HDC DC = GetWindowDC( hWnd );

    // Draw image on window
    BitBlt( DC, x, y, w, h, ( (GTK2Graphics *)Image )->GetImageHandle(),
            x, y, SRCCOPY );

    // Release window device context
    ReleaseDC( hWnd, DC );
*/
}
//---------------------------------------------------------------------------
void GTK2Window::WindowManualMove()
{
/*    // Get mouse cursor position
    LPPOINT NewPos = new POINT;
    GetCursorPos( NewPos );

    // Move window and chek for magnetism
    p_intf->p_sys->p_theme->MoveSkinMagnet( this,
        WindowPos->x + NewPos->x - CursorPos->x,
        WindowPos->y + NewPos->y - CursorPos->y );

    // Free memory
    delete[] NewPos;
*/
}
//---------------------------------------------------------------------------
void GTK2Window::WindowManualMoveInit()
{
/*    GetCursorPos( CursorPos );
    WindowPos->x = Left;
    WindowPos->y = Top;*/
}
//---------------------------------------------------------------------------
void GTK2Window::Move( int left, int top )
{
/*    Left = left;
    Top  = top;
    //SetWindowPos( hWnd, HWND_TOP, Left, Top, Width, Height,
    //              SWP_NOSIZE|SWP_NOREDRAW|SWP_NOZORDER );
    MoveWindow( hWnd, Left, Top, Width, Height, false );*/
}
//---------------------------------------------------------------------------
void GTK2Window::Size( int width, int height )
{
    Width  = width;
    Height = height;
/*    SetWindowPos( hWnd, HWND_TOP, Left, Top, Width, Height,
                  SWP_NOMOVE|SWP_NOREDRAW|SWP_NOZORDER );*/
}
//---------------------------------------------------------------------------
void GTK2Window::ChangeToolTipText( string text )
{
/*    if( text == "none" )
    {
        if( ToolTipText != "none" )
        {
            ToolTipText = "none";
            ToolTipInfo.lpszText = NULL;
            SendMessage( ToolTipWindow, TTM_ACTIVATE, 0 , 0 );
        }
    }
    else
    {
        if( text != ToolTipText )
        {
            ToolTipText = text;
            ToolTipInfo.lpszText = (char *)ToolTipText.c_str();
            SendMessage( ToolTipWindow, TTM_ACTIVATE, 1 , 0 );
            SendMessage( ToolTipWindow, TTM_UPDATETIPTEXT, 0,
                             (LPARAM)(LPTOOLINFO)&ToolTipInfo );
        }
    }
*/
}
//---------------------------------------------------------------------------

#endif
