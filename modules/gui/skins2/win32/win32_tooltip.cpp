/*****************************************************************************
 * win32_tooltip.cpp
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

#include "win32_tooltip.hpp"
#include "win32_graphics.hpp"
#include "../src/generic_window.hpp"

Win32Tooltip::Win32Tooltip( intf_thread_t *pIntf, HINSTANCE hInst,
                            HWND hParentWindow ):
    OSTooltip( pIntf )
{
    // Create the window
    m_hWnd = CreateWindowEx( WS_EX_TOOLWINDOW,
        TEXT("SkinWindowClass"), TEXT("tooltip"), WS_POPUP | WS_DISABLED,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        hParentWindow, 0, hInst, NULL );

    if( !m_hWnd )
    {
        msg_Err( getIntf(), "createWindow failed" );
    }
}


Win32Tooltip::~Win32Tooltip()
{
    if( m_hWnd )
        DestroyWindow( m_hWnd );
}


void Win32Tooltip::show( int left, int top, OSGraphics &rText )
{
    // Source drawable
    HDC srcDC = ((Win32Graphics&)rText).getDC();
    int width = rText.getWidth();
    int height = rText.getHeight();

    // Set the window on top, resize it, and show it
    // SWP_NOACTIVATE is needed to make sure the underlying window
    // keeps the keyboard focus ( keys + mouse_wheel )
    SetWindowPos( m_hWnd, HWND_TOPMOST, left, top, width, height,
                  SWP_NOACTIVATE | SWP_SHOWWINDOW );

    HDC wndDC = GetDC( m_hWnd );
    BitBlt( wndDC, 0, 0, width, height, srcDC, 0, 0, SRCCOPY );
    ReleaseDC( m_hWnd, wndDC );
}


void Win32Tooltip::hide()
{
    ShowWindow( m_hWnd, SW_HIDE );
}


#endif

