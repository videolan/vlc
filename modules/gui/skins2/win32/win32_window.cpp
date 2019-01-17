/*****************************************************************************
 * win32_window.cpp
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

#include "../src/generic_window.hpp"
#include "../src/vlcproc.hpp"
#include "../src/vout_manager.hpp"
#include "win32_window.hpp"
#include "win32_dragdrop.hpp"
#include "win32_factory.hpp"


/// Fading API
#ifndef LWA_COLORKEY
#   define LWA_COLORKEY  0x00000001
#   define LWA_ALPHA     0x00000002
#endif


// XXX layered windows are supposed to work only with at least win2k
#ifndef WS_EX_LAYERED
#   define WS_EX_LAYERED 0x00080000
#endif

Win32Window::Win32Window( intf_thread_t *pIntf, GenericWindow &rWindow,
                          HINSTANCE hInst, HWND hParentWindow,
                          bool dragDrop, bool playOnDrop,
                          Win32Window *pParentWindow,
                          GenericWindow::WindowType_t type ):
    OSWindow( pIntf ), m_dragDrop( dragDrop ), m_isLayered( false ),
    m_pParent( pParentWindow ), m_type ( type )
{
    (void)hParentWindow;
    Win32Factory *pFactory = (Win32Factory*)Win32Factory::instance( getIntf() );

    LPCTSTR vlc_name =  TEXT("VlC Media Player");
    LPCTSTR vlc_class = TEXT("SkinWindowClass");

    // Create the window
    if( type == GenericWindow::VoutWindow )
    {
        // Child window (for vout)
        m_hWnd_parent = pParentWindow->getHandle();
        m_hWnd = CreateWindowEx( WS_EX_TOOLWINDOW | WS_EX_NOPARENTNOTIFY,
                     vlc_class, vlc_name,
                     WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                     0, 0, 0, 0, m_hWnd_parent, 0, hInst, NULL );
    }
    else if( type == GenericWindow::FullscreenWindow )
    {
        // top-level window
        m_hWnd = CreateWindowEx( WS_EX_APPWINDOW, vlc_class,
                                 vlc_name, WS_POPUP | WS_CLIPCHILDREN,
                                 0, 0, 0, 0, NULL, 0, hInst, NULL );

        // Store with it a pointer to the interface thread
        SetWindowLongPtr( m_hWnd, GWLP_USERDATA, (LONG_PTR)getIntf() );
    }
    else if( type == GenericWindow::FscWindow )
    {
        VoutManager* pVoutManager = VoutManager::instance( getIntf() );
        GenericWindow* pParent =
           (GenericWindow*)pVoutManager->getVoutMainWindow();

        Win32Window *pWin = (Win32Window*)pParent->getOSWindow();
        m_hWnd_parent = pWin->getHandle();

        // top-level window
        m_hWnd = CreateWindowEx( WS_EX_APPWINDOW, vlc_class, vlc_name,
                                 WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                 0, 0, 0, 0, m_hWnd_parent, 0, hInst, NULL );

        // Store with it a pointer to the interface thread
        SetWindowLongPtr( m_hWnd, GWLP_USERDATA, (LONG_PTR)getIntf() );
    }
    else
    {
        // top-level window (owned by the root window)
        HWND hWnd_owner = pFactory->getParentWindow();
        m_hWnd = CreateWindowEx( 0, vlc_class, vlc_name,
                                 WS_POPUP | WS_CLIPCHILDREN,
                                 0, 0, 0, 0, hWnd_owner, 0, hInst, NULL );

        // Store with it a pointer to the interface thread
        SetWindowLongPtr( m_hWnd, GWLP_USERDATA, (LONG_PTR)getIntf() );
    }

    if( !m_hWnd )
    {
        msg_Err( getIntf(), "CreateWindow failed" );
        return;
    }

    // Store a pointer to the GenericWindow in a map
    pFactory->m_windowMap[m_hWnd] = &rWindow;

    // Drag & drop
    if( m_dragDrop )
    {
        m_pDropTarget = (LPDROPTARGET)
            new Win32DragDrop( getIntf(), playOnDrop, &rWindow );
        // Register the window as a drop target
        RegisterDragDrop( m_hWnd, m_pDropTarget );
    }
}


Win32Window::~Win32Window()
{
    Win32Factory *pFactory = (Win32Factory*)Win32Factory::instance( getIntf() );
    pFactory->m_windowMap[m_hWnd] = NULL;

    if( m_hWnd )
    {
        if( m_dragDrop )
        {
            // Remove the window from the list of drop targets
            RevokeDragDrop( m_hWnd );
            m_pDropTarget->Release();
        }

        DestroyWindow( m_hWnd );
    }
}


void Win32Window::reparent( OSWindow* parent, int x, int y, int w, int h )
{
    Win32Window *pParentWin = (Win32Window*)parent;
    // Reparent the window
    if( !SetParent( m_hWnd, pParentWin->getHandle() ) )
        msg_Err( getIntf(), "SetParent failed (%lu)", GetLastError() );
    MoveWindow( m_hWnd, x, y, w, h, TRUE );
}


bool Win32Window::invalidateRect( int x, int y, int w, int h) const
{
    RECT rect = { x, y, x + w , y + h };
    InvalidateRect( m_hWnd, &rect, FALSE );
    UpdateWindow( m_hWnd );

    return true;
}


void Win32Window::show() const
{

    if( m_type == GenericWindow::VoutWindow )
    {
        SetWindowPos( m_hWnd, HWND_BOTTOM, 0, 0, 0, 0,
                              SWP_NOMOVE | SWP_NOSIZE );
    }
    else if( m_type == GenericWindow::FullscreenWindow )
    {
        SetWindowPos( m_hWnd, HWND_TOPMOST, 0, 0, 0, 0,
                              SWP_NOMOVE | SWP_NOSIZE );
    }

    ShowWindow( m_hWnd, SW_SHOW );
}


void Win32Window::hide() const
{
    ShowWindow( m_hWnd, SW_HIDE );
}


void Win32Window::moveResize( int left, int top, int width, int height ) const
{
    MoveWindow( m_hWnd, left, top, width, height, TRUE );
}


void Win32Window::raise() const
{
//     SetWindowPos( m_hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
    SetForegroundWindow( m_hWnd );
}


void Win32Window::setOpacity( uint8_t value ) const
{
    if( !m_isLayered )
    {
        // add the WS_EX_LAYERED attribute.
        SetWindowLongPtr( m_hWnd, GWL_EXSTYLE,
            GetWindowLongPtr( m_hWnd, GWL_EXSTYLE ) | WS_EX_LAYERED );

        m_isLayered = true;
    }

    // Change the opacity
    SetLayeredWindowAttributes( m_hWnd, 0, value, LWA_ALPHA );
}


void Win32Window::toggleOnTop( bool onTop ) const
{
    SetWindowPos( m_hWnd, onTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                  0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE );
}


#endif
