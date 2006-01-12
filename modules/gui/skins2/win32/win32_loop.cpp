/*****************************************************************************
 * win32_loop.cpp
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef WIN32_SKINS

#include "win32_factory.hpp"
#include "win32_loop.hpp"
#include "../src/generic_window.hpp"
#include "../events/evt_key.hpp"
#include "../events/evt_leave.hpp"
#include "../events/evt_menu.hpp"
#include "../events/evt_motion.hpp"
#include "../events/evt_mouse.hpp"
#include "../events/evt_refresh.hpp"
#include "../events/evt_scroll.hpp"
#include "vlc_keys.h"


// XXX: Cygwin (at least) doesn't define these macros. Too bad...
#ifndef GET_X_LPARAM
    #define GET_X_LPARAM(a) ((int16_t)(a))
    #define GET_Y_LPARAM(a) ((int16_t)((a)>>16))
#endif


Win32Loop::Win32Loop( intf_thread_t *pIntf ): OSLoop( pIntf )
{
    // Initialize the map
    virtKeyToVlcKey[VK_F1] = KEY_F1;
    virtKeyToVlcKey[VK_F2] = KEY_F2;
    virtKeyToVlcKey[VK_F3] = KEY_F3;
    virtKeyToVlcKey[VK_F4] = KEY_F4;
    virtKeyToVlcKey[VK_F5] = KEY_F5;
    virtKeyToVlcKey[VK_F6] = KEY_F6;
    virtKeyToVlcKey[VK_F7] = KEY_F7;
    virtKeyToVlcKey[VK_F8] = KEY_F8;
    virtKeyToVlcKey[VK_F9] = KEY_F9;
    virtKeyToVlcKey[VK_F10] = KEY_F10;
    virtKeyToVlcKey[VK_F11] = KEY_F11;
    virtKeyToVlcKey[VK_F12] = KEY_F12;
    virtKeyToVlcKey[VK_RETURN] = KEY_ENTER;
    virtKeyToVlcKey[VK_SPACE] = KEY_SPACE;
    virtKeyToVlcKey[VK_ESCAPE] = KEY_ESC;
    virtKeyToVlcKey[VK_LEFT] = KEY_LEFT;
    virtKeyToVlcKey[VK_RIGHT] = KEY_RIGHT;
    virtKeyToVlcKey[VK_UP] = KEY_UP;
    virtKeyToVlcKey[VK_DOWN] = KEY_DOWN;
    virtKeyToVlcKey[VK_HOME] = KEY_HOME;
    virtKeyToVlcKey[VK_END] = KEY_END;
    virtKeyToVlcKey[VK_PRIOR] = KEY_PAGEUP;
    virtKeyToVlcKey[VK_NEXT] = KEY_PAGEDOWN;
}


Win32Loop::~Win32Loop()
{
}


OSLoop *Win32Loop::instance( intf_thread_t *pIntf )
{
    if( pIntf->p_sys->p_osLoop == NULL )
    {
        OSLoop *pOsLoop = new Win32Loop( pIntf );
        pIntf->p_sys->p_osLoop = pOsLoop;
    }
    return pIntf->p_sys->p_osLoop;
}


void Win32Loop::destroy( intf_thread_t *pIntf )
{
    if( pIntf->p_sys->p_osLoop )
    {
        delete pIntf->p_sys->p_osLoop;
        pIntf->p_sys->p_osLoop = NULL;
    }
}


void Win32Loop::run()
{
    MSG msg;

    // Compute windows message list
    while( GetMessage( &msg, NULL, 0, 0 ) )
    {
        Win32Factory *pFactory =
            (Win32Factory*)Win32Factory::instance( getIntf() );
        GenericWindow *pWin = pFactory->m_windowMap[msg.hwnd];
        if( pWin == NULL )
        {
            // We are probably getting a message for a tooltip (which has no
            // associated GenericWindow), for a timer, or for the parent window
            DispatchMessage( &msg );
            continue;
        }

        GenericWindow &win = *pWin;
        switch( msg.message )
        {
            case WM_PAINT:
            {
                PAINTSTRUCT Infos;
                BeginPaint( msg.hwnd, &Infos );
                EvtRefresh evt( getIntf(),
                                Infos.rcPaint.left,
                                Infos.rcPaint.top,
                                Infos.rcPaint.right - Infos.rcPaint.left + 1,
                                Infos.rcPaint.bottom - Infos.rcPaint.top + 1 );
                EndPaint( msg.hwnd, &Infos );
                win.processEvent( evt );
                break;
            }
            case WM_COMMAND:
            {
                EvtMenu evt( getIntf(), LOWORD( msg.wParam ) );
                win.processEvent( evt );
                break;
            }
            case WM_MOUSEMOVE:
            {
                // Needed to generate WM_MOUSELEAVE events
                TRACKMOUSEEVENT TrackEvent;
                TrackEvent.cbSize      = sizeof( TRACKMOUSEEVENT );
                TrackEvent.dwFlags     = TME_LEAVE;
                TrackEvent.hwndTrack   = msg.hwnd;
                TrackEvent.dwHoverTime = 1;
                TrackMouseEvent( &TrackEvent );

                // Compute the absolute position of the mouse
                int x = GET_X_LPARAM( msg.lParam ) + win.getLeft();
                int y = GET_Y_LPARAM( msg.lParam ) + win.getTop();
                EvtMotion evt( getIntf(), x, y );
                win.processEvent( evt );
                break;
            }
            case WM_MOUSELEAVE:
            {
                EvtLeave evt( getIntf() );
                win.processEvent( evt );
                break;
            }
            case WM_MOUSEWHEEL:
            {
                int x = GET_X_LPARAM( msg.lParam ) - win.getLeft();
                int y = GET_Y_LPARAM( msg.lParam ) - win.getTop();
                int mod = getMod( msg.wParam );
                if( GET_WHEEL_DELTA_WPARAM( msg.wParam ) > 0 )
                {
                    EvtScroll evt( getIntf(), x, y, EvtScroll::kUp, mod );
                    win.processEvent( evt );
                }
                else
                {
                    EvtScroll evt( getIntf(), x, y, EvtScroll::kDown, mod );
                    win.processEvent( evt );
                }
                break;
            }
            case WM_LBUTTONDOWN:
            {
                SetCapture( msg.hwnd );
                EvtMouse evt( getIntf(), GET_X_LPARAM( msg.lParam ),
                              GET_Y_LPARAM( msg.lParam ), EvtMouse::kLeft,
                              EvtMouse::kDown, getMod( msg.wParam ) );
                win.processEvent( evt );
                break;
            }
            case WM_RBUTTONDOWN:
            {
                SetCapture( msg.hwnd );
                EvtMouse evt( getIntf(), GET_X_LPARAM( msg.lParam ),
                              GET_Y_LPARAM( msg.lParam ), EvtMouse::kRight,
                              EvtMouse::kDown, getMod( msg.wParam ) );
                win.processEvent( evt );
                break;
            }
            case WM_LBUTTONUP:
            {
                ReleaseCapture();
                EvtMouse evt( getIntf(), GET_X_LPARAM( msg.lParam ),
                              GET_Y_LPARAM( msg.lParam ), EvtMouse::kLeft,
                              EvtMouse::kUp, getMod( msg.wParam ) );
                win.processEvent( evt );
                break;
            }
            case WM_RBUTTONUP:
            {
                ReleaseCapture();
                EvtMouse evt( getIntf(), GET_X_LPARAM( msg.lParam ),
                              GET_Y_LPARAM( msg.lParam ), EvtMouse::kRight,
                              EvtMouse::kUp, getMod( msg.wParam ) );
                win.processEvent( evt );
                break;
            }
            case WM_LBUTTONDBLCLK:
            {
                ReleaseCapture();
                EvtMouse evt( getIntf(), GET_X_LPARAM( msg.lParam ),
                              GET_Y_LPARAM( msg.lParam ), EvtMouse::kLeft,
                              EvtMouse::kDblClick, getMod( msg.wParam ) );
                win.processEvent( evt );
                break;
            }
            case WM_RBUTTONDBLCLK:
            {
                ReleaseCapture();
                EvtMouse evt( getIntf(), GET_X_LPARAM( msg.lParam ),
                              GET_Y_LPARAM( msg.lParam ), EvtMouse::kRight,
                              EvtMouse::kDblClick, getMod( msg.wParam ) );
                win.processEvent( evt );
                break;
            }
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYUP:
            {
                // The key events are first processed here and not translated
                // into WM_CHAR events because we need to know the status of
                // the modifier keys.

                // Get VLC key code from the virtual key code
                int key = virtKeyToVlcKey[msg.wParam];
                if( !key )
                {
                    // This appears to be a "normal" (ascii) key
                    key = tolower( MapVirtualKey( msg.wParam, 2 ) );
                }

                if( key )
                {
                    // Get the modifier
                    int mod = 0;
                    if( GetKeyState( VK_CONTROL ) & 0x8000 )
                    {
                        mod |= EvtInput::kModCtrl;
                    }
                    if( GetKeyState( VK_SHIFT ) & 0x8000 )
                    {
                        mod |= EvtInput::kModShift;
                    }
                    if( GetKeyState( VK_MENU ) & 0x8000 )
                    {
                        mod |= EvtInput::kModAlt;
                    }

                    // Get the state
                    EvtKey::ActionType_t state;
                    if( msg.message == WM_KEYDOWN ||
                        msg.message == WM_SYSKEYDOWN )
                    {
                        state = EvtKey::kDown;
                    }
                    else
                    {
                        state = EvtKey::kUp;
                    }

                    EvtKey evt( getIntf(), key, state, mod );
                    win.processEvent( evt );
                }
                break;
            }
            default:
                TranslateMessage( &msg );
                DispatchMessage( &msg );
        }
    }
}


int Win32Loop::getMod( WPARAM wParam ) const
{
    int mod = EvtInput::kModNone;
    if( wParam & MK_CONTROL )
        mod |= EvtInput::kModCtrl;
    if( wParam & MK_SHIFT )
        mod |= EvtInput::kModShift;

    return mod;
}


void Win32Loop::exit()
{
    PostQuitMessage(0);
}

#endif
