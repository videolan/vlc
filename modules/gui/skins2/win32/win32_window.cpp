/*****************************************************************************
 * win32_window.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifdef WIN32_SKINS

#include "../src/generic_window.hpp"
#include "../src/vlcproc.hpp"
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
                          Win32Window *pParentWindow ):
    OSWindow( pIntf ), m_dragDrop( dragDrop ), m_isLayered( false ),
    m_pParent( pParentWindow )
{
    // Create the window
    if( pParentWindow )
    {
        // Child window (for vout)
        HWND hParent = pParentWindow->getHandle();
        m_hWnd = CreateWindowEx( WS_EX_TOOLWINDOW, "SkinWindowClass",
            "default name", WS_CHILD, CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT, hParent, 0, hInst, NULL );
    }
    else
    {
        // Normal window
        m_hWnd = CreateWindowEx( WS_EX_TOOLWINDOW, "SkinWindowClass",
            "default name", WS_POPUP | WS_CLIPCHILDREN,
            CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT, hParentWindow, 0, hInst, NULL );
    }

    if( !m_hWnd )
    {
        msg_Err( getIntf(), "CreateWindow failed" );
        return;
    }

    // Store a pointer to the GenericWindow in a map
    Win32Factory *pFactory = (Win32Factory*)Win32Factory::instance( getIntf() );
    pFactory->m_windowMap[m_hWnd] = &rWindow;

    // Drag & drop
    if( m_dragDrop )
    {
        m_pDropTarget = (LPDROPTARGET) new Win32DragDrop( getIntf(),
                                                          playOnDrop );
        // Register the window as a drop target
        RegisterDragDrop( m_hWnd, m_pDropTarget );
    }

    // Set this window as a vout
    if( m_pParent )
    {
        VlcProc::instance( getIntf() )->registerVoutWindow( (void*)m_hWnd );
    }
}


Win32Window::~Win32Window()
{
    if( m_pParent )
    {
        VlcProc::instance( getIntf() )->unregisterVoutWindow( (void*)m_hWnd );
    }

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


void Win32Window::show( int left, int top ) const
{
    ShowWindow( m_hWnd, SW_SHOW );
}


void Win32Window::hide() const
{
    ShowWindow( m_hWnd, SW_HIDE );
}


void Win32Window::moveResize( int left, int top, int width, int height ) const
{
    MoveWindow( m_hWnd, left, top, width, height, true );
}


void Win32Window::raise() const
{
//     SetWindowPos( m_hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
    SetForegroundWindow( m_hWnd );
}


void Win32Window::setOpacity( uint8_t value ) const
{
    Win32Factory *pFactory = (Win32Factory*)Win32Factory::instance( getIntf() );

    if( value == 255 )
    {
        // If the window is opaque, we remove the WS_EX_LAYERED attribute
        // which slows down resizing for nothing
        if( m_isLayered )
        {
            SetWindowLongPtr( m_hWnd, GWL_EXSTYLE,
                GetWindowLong( m_hWnd, GWL_EXSTYLE ) & ~WS_EX_LAYERED );

            // Redraw the window, otherwise we may end up with a grey rectangle
            // for some strange reason
            RedrawWindow(m_hWnd, NULL, NULL,
                RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);

            m_isLayered = false;
        }
    }
    else
    {
        if( pFactory->SetLayeredWindowAttributes )
        {
            if( ! m_isLayered )
            {
                // (Re)Add the WS_EX_LAYERED attribute.
                // Resizing will be very slow, now :)
                SetWindowLongPtr( m_hWnd, GWL_EXSTYLE,
                    GetWindowLong( m_hWnd, GWL_EXSTYLE ) | WS_EX_LAYERED );

                // Redraw the window, otherwise we may end up with a grey
                // rectangle for some strange reason
                RedrawWindow(m_hWnd, NULL, NULL,
                    RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);

                m_isLayered = true;
            }

            // Change the opacity
            pFactory->SetLayeredWindowAttributes(
                m_hWnd, 0, value, LWA_ALPHA|LWA_COLORKEY );
        }
    }
}


void Win32Window::toggleOnTop( bool onTop ) const
{
    if( onTop )
    {
        // Set the window on top
        SetWindowPos( m_hWnd, HWND_TOPMOST, 0, 0, 0, 0,
                      SWP_NOSIZE | SWP_NOMOVE );
    }
    else
    {
        // Set the window not on top
        SetWindowPos( m_hWnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                      SWP_NOSIZE | SWP_NOMOVE );
    }
}


#endif
