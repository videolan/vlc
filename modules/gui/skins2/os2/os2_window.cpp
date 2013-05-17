/*****************************************************************************
 * os2_window.cpp
 *****************************************************************************
 * Copyright (C) 2003, 2013 the VideoLAN team
 *
 * Authors: Cyril Deguet      <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
 *          KO Myung-Hun      <komh@chollian.net>
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

#ifdef OS2_SKINS

#include "../src/generic_window.hpp"
#include "../src/vlcproc.hpp"
#include "../src/vout_manager.hpp"
#include "os2_window.hpp"
#include "os2_dragdrop.hpp"
#include "os2_factory.hpp"


OS2Window::OS2Window( intf_thread_t *pIntf, GenericWindow &rWindow,
                      HMODULE hInst, HWND hParentWindow,
                      bool dragDrop, bool playOnDrop,
                      OS2Window *pParentWindow,
                      GenericWindow::WindowType_t type ):
    OSWindow( pIntf ), m_dragDrop( dragDrop ), m_isLayered( false ),
    m_pParent( pParentWindow ), m_type ( type )
{
    (void)hParentWindow;
    OS2Factory *pFactory = (OS2Factory*)OS2Factory::instance( getIntf() );

    PCSZ  vlc_name  = "VlC Media Player";
    PCSZ  vlc_class = "SkinWindowClass";
    ULONG flFrame = 0;

    // Create the window
    if( type == GenericWindow::VoutWindow )
    {
        // Child window (for vout)
        m_hWnd_parent = pParentWindow->getHandle();
        m_hWnd = WinCreateStdWindow( m_hWnd_parent,
                                     WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                     &flFrame,
                                     vlc_class,
                                     vlc_name,
                                     0,
                                     hInst,
                                     0,
                                     &m_hWndClient );
    }
    else if( type == GenericWindow::FullscreenWindow )
    {
        // top-level window
        m_hWnd = WinCreateStdWindow( HWND_DESKTOP,
                                     WS_CLIPCHILDREN,
                                     &flFrame,
                                     vlc_class,
                                     vlc_name,
                                     0,
                                     hInst,
                                     0,
                                     &m_hWndClient );

        WinSetOwner( m_hWnd, pFactory->getParentWindow());
    }
    else if( type == GenericWindow::FscWindow )
    {
        VoutManager* pVoutManager = VoutManager::instance( getIntf() );
        GenericWindow* pParent =
           (GenericWindow*)pVoutManager->getVoutMainWindow();

        m_hWnd_parent = (HWND)pParent->getOSHandle();

        // top-level window
        m_hWnd = WinCreateStdWindow( HWND_DESKTOP,
                                     WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                     &flFrame,
                                     vlc_class,
                                     vlc_name,
                                     0,
                                     hInst,
                                     0,
                                     &m_hWndClient );

        WinSetOwner( m_hWnd, m_hWnd_parent );
    }
    else
    {
        // top-level window (owned by the root window)
        HWND hWnd_owner = pFactory->getParentWindow();
        m_hWnd = WinCreateStdWindow( HWND_DESKTOP,
                                     WS_CLIPCHILDREN,
                                     &flFrame,
                                     vlc_class,
                                     vlc_name,
                                     0,
                                     hInst,
                                     0,
                                     &m_hWndClient );

        WinSetOwner( m_hWnd, hWnd_owner );
    }

    if( !m_hWnd )
    {
        msg_Err( getIntf(), "CreateWindow failed" );
        return;
    }

    // Store with it a pointer to the interface thread
    WinSetWindowPtr( m_hWndClient, 0, getIntf());

    // Store a pointer to the GenericWindow in a map
    pFactory->m_windowMap[m_hWndClient] = &rWindow;

    // Drag & drop
    if( m_dragDrop )
    {
        // TODO
    }
}


OS2Window::~OS2Window()
{
    OS2Factory *pFactory = (OS2Factory*)OS2Factory::instance( getIntf() );
    pFactory->m_windowMap[m_hWndClient] = NULL;

    if( m_hWnd )
    {
        if( m_dragDrop )
        {
            // TODO
        }

        WinDestroyWindow( m_hWnd );
    }
}


void OS2Window::reparent( void* OSHandle, int x, int y, int w, int h )
{
    HWND hwndParent = ( HWND )OSHandle;

    // Reparent the window
    if( !WinSetParent( m_hWnd, hwndParent, TRUE ) )
        msg_Err( getIntf(), "SetParent failed (%lx)", WinGetLastError( 0 ));
    RECTL rclParent;
    WinQueryWindowRect( hwndParent, &rclParent );
    // Find bottom and invert it
    y = ( rclParent.yTop - 1 ) - ( y + h - 1 );
    WinSetWindowPos( m_hWnd, NULLHANDLE, x, y, w, h, SWP_MOVE | SWP_SIZE );
}


bool OS2Window::invalidateRect( int x, int y, int w, int h ) const
{
    RECTL rcl;
    WinQueryWindowRect( m_hWnd, &rcl );

    // Find bottom and invert it
    y = ( rcl.yTop - 1 ) - ( y + h - 1 );
    WinSetRect( 0, &rcl, x, y, x + w, y + h );
    WinInvalidateRect( m_hWndClient, &rcl, TRUE );
    WinUpdateWindow( m_hWndClient );

    return true;
}


void OS2Window::show() const
{
    if( m_type == GenericWindow::VoutWindow )
    {
        WinSetWindowPos( m_hWnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_ZORDER );
    }
    else if( m_type == GenericWindow::FullscreenWindow )
    {
        WinSetWindowPos( m_hWnd, HWND_TOP, 0, 0, 0, 0, SWP_ZORDER );
    }

    WinSetWindowPos( m_hWnd, NULLHANDLE, 0, 0, 0, 0,
                     SWP_SHOW | SWP_ACTIVATE );
}


void OS2Window::hide() const
{
    WinShowWindow( m_hWnd, FALSE );
}


void OS2Window::moveResize( int left, int top, int width, int height ) const
{
    RECTL rclParent;
    WinQueryWindowRect( WinQueryWindow( m_hWnd, QW_PARENT ), &rclParent );
    // Find bottom and invert it
    int bottom = ( rclParent.yTop - 1 ) - ( top + height - 1 );
    WinSetWindowPos( m_hWnd, NULLHANDLE, left, bottom , width, height,
                     SWP_MOVE | SWP_SIZE );
}


void OS2Window::raise() const
{
    WinSetWindowPos( m_hWnd, HWND_TOP, 0, 0, 0, 0, SWP_ZORDER | SWP_SHOW );
}


void OS2Window::setOpacity( uint8_t value ) const
{
    // Not supported
}


void OS2Window::toggleOnTop( bool onTop ) const
{
    // Just bring a window to the top
    WinSetWindowPos( m_hWnd, HWND_TOP, 0, 0, 0, 0, onTop ? SWP_ZORDER : 0 );
}


#endif
