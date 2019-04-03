/*****************************************************************************
 * win32_factory.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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

#ifdef WIN32_SKINS

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <windows.h>
#include <winuser.h>
#include <wingdi.h>
#include <shellapi.h>

#include "win32_factory.hpp"
#include "win32_graphics.hpp"
#include "win32_timer.hpp"
#include "win32_window.hpp"
#include "win32_tooltip.hpp"
#include "win32_popup.hpp"
#include "win32_loop.hpp"
#include "../src/theme.hpp"
#include "../src/window_manager.hpp"
#include "../src/generic_window.hpp"
#include "../commands/cmd_dialogs.hpp"
#include "../commands/cmd_minimize.hpp"

// Custom message for the notifications of the system tray
#define MY_WM_TRAYACTION (WM_APP + 1)


LRESULT CALLBACK Win32Factory::Win32Proc( HWND hwnd, UINT uMsg,
                                          WPARAM wParam, LPARAM lParam )
{
    // Get pointer to thread info: should only work with the parent window
    intf_thread_t *p_intf = (intf_thread_t *)GetWindowLongPtr( hwnd,
        GWLP_USERDATA );

    // If doesn't exist, treat windows message normally
    if( p_intf == NULL || p_intf->p_sys->p_osFactory == NULL )
    {
        return DefWindowProc( hwnd, uMsg, wParam, lParam );
    }

    Win32Factory *pFactory = (Win32Factory*)Win32Factory::instance( p_intf );
    GenericWindow *pWin = pFactory->m_windowMap[hwnd];

    if( hwnd == pFactory->getParentWindow() )
    {
        if( uMsg == WM_SYSCOMMAND )
        {
            // If closing parent window
            if( (wParam & 0xFFF0) == SC_CLOSE )
            {
                libvlc_Quit( vlc_object_instance(p_intf) );
                return 0;
            }
            else if( (wParam & 0xFFF0) == SC_MINIMIZE )
            {
                pFactory->minimize();
                return 0;
            }
            else if( (wParam & 0xFFF0) == SC_RESTORE )
            {
                pFactory->restore();
                return 0;
            }
            else
            {
                msg_Dbg( p_intf, "WM_SYSCOMMAND %i", (wParam  & 0xFFF0) );
            }
        }
        // Handle systray notifications
        else if( uMsg == MY_WM_TRAYACTION )
        {
            if( (UINT)lParam == WM_LBUTTONDOWN )
            {
                p_intf->p_sys->p_theme->getWindowManager().raiseAll();
                CmdDlgHidePopupMenu aCmdPopup( p_intf );
                aCmdPopup.execute();
                return 0;
            }
            else if( (UINT)lParam == WM_RBUTTONDOWN )
            {
                CmdDlgShowPopupMenu aCmdPopup( p_intf );
                aCmdPopup.execute();
                return 0;
            }
            else if( (UINT)lParam == WM_LBUTTONDBLCLK )
            {
                CmdRestore aCmdRestore( p_intf );
                aCmdRestore.execute();
                return 0;
            }
        }
    }
    else if( pWin )
    {
        Win32Loop* pLoop =
            (Win32Loop*) OSFactory::instance( p_intf )->getOSLoop();
        if( pLoop )
            return pLoop->processEvent( hwnd, uMsg, wParam, lParam );
    }

    // If hwnd does not match any window or message not processed
    return DefWindowProc( hwnd, uMsg, wParam, lParam );
}


BOOL CALLBACK Win32Factory::MonitorEnumProc( HMONITOR hMonitor, HDC hdcMonitor,
                                             LPRECT lprcMonitor, LPARAM dwData )
{
    (void)hdcMonitor; (void)lprcMonitor;
    std::list<HMONITOR>* pList = (std::list<HMONITOR>*)dwData;
    pList->push_back( hMonitor );

    return TRUE;
}

Win32Factory::Win32Factory( intf_thread_t *pIntf ):
    OSFactory( pIntf ), m_hParentWindow( NULL ),
    m_dirSep( "\\" )
{
    // see init()
}


bool Win32Factory::init()
{
    LPCTSTR vlc_name = TEXT("VLC Media Player");
    LPCTSTR vlc_icon = TEXT("VLC_ICON");
    LPCTSTR vlc_class = TEXT("SkinWindowClass");

    // Get instance handle
    m_hInst = GetModuleHandle( NULL );
    if( m_hInst == NULL )
    {
        msg_Err( getIntf(), "Cannot get module handle" );
    }

    // Create window class
    WNDCLASS skinWindowClass;
    skinWindowClass.style = CS_DBLCLKS;
    skinWindowClass.lpfnWndProc = (WNDPROC)Win32Factory::Win32Proc;
    skinWindowClass.lpszClassName = vlc_class;
    skinWindowClass.lpszMenuName = NULL;
    skinWindowClass.cbClsExtra = 0;
    skinWindowClass.cbWndExtra = 0;
    skinWindowClass.hbrBackground = NULL;
    skinWindowClass.hCursor = LoadCursor( NULL, IDC_ARROW );
    skinWindowClass.hIcon = LoadIcon( m_hInst, vlc_icon );
    skinWindowClass.hInstance = m_hInst;

    // Register class and check it
    if( !RegisterClass( &skinWindowClass ) )
    {
        WNDCLASS wndclass;

        // Check why it failed. If it's because the class already exists
        // then fine, otherwise return with an error.
        if( !GetClassInfo( m_hInst, vlc_class, &wndclass ) )
        {
            msg_Err( getIntf(), "cannot register window class" );
            return false;
        }
    }

    // Create Window
    m_hParentWindow = CreateWindowEx( WS_EX_TOOLWINDOW, vlc_class,
        vlc_name, WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX,
        -200, -200, 0, 0, 0, 0, m_hInst, 0 );
    if( m_hParentWindow == NULL )
    {
        msg_Err( getIntf(), "cannot create parent window" );
        return false;
    }

    // Store with it a pointer to the interface thread
    SetWindowLongPtr( m_hParentWindow, GWLP_USERDATA, (LONG_PTR)getIntf() );

    // We do it this way otherwise CreateWindowEx will fail
    // if WS_EX_LAYERED is not supported
    SetWindowLongPtr( m_hParentWindow, GWL_EXSTYLE,
                      GetWindowLongPtr( m_hParentWindow, GWL_EXSTYLE ) |
                      WS_EX_LAYERED );

    ShowWindow( m_hParentWindow, SW_SHOW );

    // Initialize the systray icon
    m_trayIcon.cbSize = sizeof( NOTIFYICONDATA );
    m_trayIcon.hWnd = m_hParentWindow;
    m_trayIcon.uID = 42;
    m_trayIcon.uFlags = NIF_ICON|NIF_TIP|NIF_MESSAGE;
    m_trayIcon.uCallbackMessage = MY_WM_TRAYACTION;
    m_trayIcon.hIcon = LoadIcon( m_hInst, vlc_icon );
    wcscpy( m_trayIcon.szTip, vlc_name );

    // Show the systray icon if needed
    if( var_InheritBool( getIntf(), "skins2-systray" ) )
    {
        addInTray();
    }

    // Show the task in the task bar if needed
    if( var_InheritBool( getIntf(), "skins2-taskbar" ) )
    {
        addInTaskBar();
    }

    // Initialize the OLE library (for drag & drop)
    OleInitialize( NULL );

    // Initialize the resource path
    char *datadir = config_GetUserDir( VLC_USERDATA_DIR );
    m_resourcePath.push_back( (std::string)datadir + "\\skins" );
    free( datadir );
    datadir = config_GetSysPath(VLC_PKG_DATA_DIR, NULL);
    m_resourcePath.push_back( (std::string)datadir + "\\skins" );
    m_resourcePath.push_back( (std::string)datadir + "\\skins2" );
    m_resourcePath.push_back( (std::string)datadir + "\\share\\skins" );
    m_resourcePath.push_back( (std::string)datadir + "\\share\\skins2" );
    free( datadir );

    // Enumerate all monitors available
    EnumDisplayMonitors( NULL, NULL, MonitorEnumProc, (LPARAM)&m_monitorList );
    int num = 0;
    for( std::list<HMONITOR>::iterator it = m_monitorList.begin();
         it != m_monitorList.end(); ++it, num++ )
    {
        MONITORINFO mi;
        mi.cbSize = sizeof( MONITORINFO );
        if( GetMonitorInfo( *it, &mi ) )
        {
            msg_Dbg( getIntf(), "monitor #%i, %ldx%ld at +%ld+%ld", num,
                        mi.rcMonitor.right - mi.rcMonitor.left,
                        mi.rcMonitor.bottom - mi.rcMonitor.top,
                        mi.rcMonitor.left,
                        mi.rcMonitor.top );
        }
    }

    // All went well
    return true;
}


Win32Factory::~Win32Factory()
{
    // Uninitialize the OLE library
    OleUninitialize();

    // Remove the systray icon
    removeFromTray();

    if( m_hParentWindow ) DestroyWindow( m_hParentWindow );
}


OSGraphics *Win32Factory::createOSGraphics( int width, int height )
{
    return new Win32Graphics( getIntf(), width, height );
}


OSLoop *Win32Factory::getOSLoop()
{
    return Win32Loop::instance( getIntf() );
}


void Win32Factory::destroyOSLoop()
{
    Win32Loop::destroy( getIntf() );
}

void Win32Factory::minimize()
{
    /* Make sure no tooltip is visible first */
    getIntf()->p_sys->p_theme->getWindowManager().hideTooltip();

    ShowWindow( m_hParentWindow, SW_MINIMIZE );
}

void Win32Factory::restore()
{
    ShowWindow( m_hParentWindow, SW_RESTORE );
}

void Win32Factory::addInTray()
{
    Shell_NotifyIcon( NIM_ADD, &m_trayIcon );
}

void Win32Factory::removeFromTray()
{
    Shell_NotifyIcon( NIM_DELETE, &m_trayIcon );
}

void Win32Factory::addInTaskBar()
{
    ShowWindow( m_hParentWindow, SW_HIDE );
    SetWindowLongPtr( m_hParentWindow, GWL_EXSTYLE,
                      WS_EX_LAYERED|WS_EX_APPWINDOW );
    ShowWindow( m_hParentWindow, SW_SHOW );
}

void Win32Factory::removeFromTaskBar()
{
    ShowWindow( m_hParentWindow, SW_HIDE );
    SetWindowLongPtr( m_hParentWindow, GWL_EXSTYLE,
                      WS_EX_LAYERED|WS_EX_TOOLWINDOW );
    ShowWindow( m_hParentWindow, SW_SHOW );
}

OSTimer *Win32Factory::createOSTimer( CmdGeneric &rCmd )
{
    return new Win32Timer( getIntf(), rCmd, m_hParentWindow );
}


OSWindow *Win32Factory::createOSWindow( GenericWindow &rWindow, bool dragDrop,
                                        bool playOnDrop, OSWindow *pParent,
                                        GenericWindow::WindowType_t type )
{
    return new Win32Window( getIntf(), rWindow, m_hInst, m_hParentWindow,
                            dragDrop, playOnDrop, (Win32Window*)pParent, type );
}


OSTooltip *Win32Factory::createOSTooltip()
{
    return new Win32Tooltip( getIntf(), m_hInst, m_hParentWindow );
}


OSPopup *Win32Factory::createOSPopup()
{
    // XXX FIXME: this way of getting the handle really sucks!
    // In fact, the clean way would be to have in Builder::addPopup() a call
    // to pPopup->associateToWindow() (to be written)... but the problem is
    // that there is no way to access the OS-dependent window handle from a
    // GenericWindow (we cannot even access the OSWindow).
    if( m_windowMap.begin() == m_windowMap.end() )
    {
        msg_Err( getIntf(), "no window has been created before the popup!" );
        return NULL;
    }

    return new Win32Popup( getIntf(), m_windowMap.begin()->first );
}


int Win32Factory::getScreenWidth() const
{
    return GetSystemMetrics(SM_CXSCREEN);
}


int Win32Factory::getScreenHeight() const
{
    return GetSystemMetrics(SM_CYSCREEN);
}


void Win32Factory::getMonitorInfo( OSWindow *pWindow,
                                   int* p_x, int* p_y,
                                   int* p_width, int* p_height ) const
{
    Win32Window *pWin = (Win32Window*)pWindow;
    HWND wnd = pWin->getHandle();
    HMONITOR hmon = MonitorFromWindow( wnd, MONITOR_DEFAULTTONEAREST );
    MONITORINFO mi;
    mi.cbSize = sizeof( MONITORINFO );
    if( hmon && GetMonitorInfo( hmon, &mi ) )
    {
        *p_x = mi.rcMonitor.left;
        *p_y = mi.rcMonitor.top;
        *p_width = mi.rcMonitor.right - mi.rcMonitor.left;
        *p_height = mi.rcMonitor.bottom - mi.rcMonitor.top;
    }
    else
    {
        *p_x = 0;
        *p_y = 0;
        *p_width = getScreenWidth();
        *p_height = getScreenHeight();
    }
}


void Win32Factory::getMonitorInfo( int numScreen, int* p_x, int* p_y,
                                   int* p_width, int* p_height ) const
{
    HMONITOR hmon = NULL;
    std::list<HMONITOR>::const_iterator it = m_monitorList.begin();
    for( int i = 0; it != m_monitorList.end(); ++it, i++ )
    {
        if( i == numScreen )
        {
            hmon = *it;
            break;
        }
    }
    MONITORINFO mi;
    mi.cbSize = sizeof( MONITORINFO );
    if( hmon && GetMonitorInfo( hmon, &mi ) )
    {
        *p_x = mi.rcMonitor.left;
        *p_y = mi.rcMonitor.top;
        *p_width = mi.rcMonitor.right - mi.rcMonitor.left;
        *p_height = mi.rcMonitor.bottom - mi.rcMonitor.top;
    }
    else
    {
        *p_x = 0;
        *p_y = 0;
        *p_width = getScreenWidth();
        *p_height = getScreenHeight();
    }
}


SkinsRect Win32Factory::getWorkArea() const
{
    RECT r;
    SystemParametersInfo( SPI_GETWORKAREA, 0, &r, 0 );
    // Fill a Rect object
    return  SkinsRect( r.left, r.top, r.right, r.bottom );
}


void Win32Factory::getMousePos( int &rXPos, int &rYPos ) const
{
    POINT mousePos;
    GetCursorPos( &mousePos );
    rXPos = mousePos.x;
    rYPos = mousePos.y;
}


void Win32Factory::changeCursor( CursorType_t type ) const
{
    LPCTSTR id;
    switch( type )
    {
    case kDefaultArrow: id = IDC_ARROW;    break;
    case kResizeNWSE:   id = IDC_SIZENWSE; break;
    case kResizeNS:     id = IDC_SIZENS;   break;
    case kResizeWE:     id = IDC_SIZEWE;   break;
    case kResizeNESW:   id = IDC_SIZENESW; break;
    case kNoCursor:
    default: id = 0;
    }

    HCURSOR hCurs = (type == kNoCursor) ? NULL : LoadCursor( NULL, id );
    SetCursor( hCurs );
}


void Win32Factory::rmDir( const std::string &rPath )
{
    LPWSTR dir_temp = ToWide( rPath.c_str() );
    size_t len = wcslen( dir_temp );

    LPWSTR dir = (wchar_t *)vlc_alloc( len + 2, sizeof (wchar_t) );
    wcsncpy( dir, dir_temp, len + 2);

    SHFILEOPSTRUCTW file_op = {
        NULL,
        FO_DELETE,
        dir,
        NULL,
        FOF_NOCONFIRMATION |
        FOF_NOERRORUI |
        FOF_SILENT,
        false,
        NULL,
        L"" };

     SHFileOperationW(&file_op);

     free(dir_temp);
     free(dir);
}

#endif
