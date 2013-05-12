/*****************************************************************************
 * os2_loop.cpp
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

#include "os2_factory.hpp"
#include "os2_loop.hpp"
#include "../src/generic_window.hpp"
#include "../events/evt_key.hpp"
#include "../events/evt_leave.hpp"
#include "../events/evt_menu.hpp"
#include "../events/evt_motion.hpp"
#include "../events/evt_mouse.hpp"
#include "../events/evt_refresh.hpp"
#include "../events/evt_scroll.hpp"
#include <vlc_keys.h>


#define GET_X_MP( mp ) ( SHORT1FROMMP( mp ))
#define GET_Y_MP( mp ) (( rcl.yTop - 1 ) - SHORT2FROMMP( mp )) /* Invert Y */

#ifndef WM_MOUSELEAVE
#define WM_MOUSELEAVE   0x41F
#endif

OS2Loop::OS2Loop( intf_thread_t *pIntf ): OSLoop( pIntf )
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
    virtKeyToVlcKey[VK_ENTER] = KEY_ENTER;
    virtKeyToVlcKey[VK_NEWLINE] = KEY_ENTER;
    virtKeyToVlcKey[VK_SPACE] = ' ';
    virtKeyToVlcKey[VK_ESC] = KEY_ESC;
    virtKeyToVlcKey[VK_LEFT] = KEY_LEFT;
    virtKeyToVlcKey[VK_RIGHT] = KEY_RIGHT;
    virtKeyToVlcKey[VK_UP] = KEY_UP;
    virtKeyToVlcKey[VK_DOWN] = KEY_DOWN;
    virtKeyToVlcKey[VK_INSERT] = KEY_INSERT;
    virtKeyToVlcKey[VK_DELETE] = KEY_DELETE;
    virtKeyToVlcKey[VK_HOME] = KEY_HOME;
    virtKeyToVlcKey[VK_END] = KEY_END;
    virtKeyToVlcKey[VK_PAGEUP] = KEY_PAGEUP;
    virtKeyToVlcKey[VK_PAGEDOWN] = KEY_PAGEDOWN;
}


OS2Loop::~OS2Loop()
{
}


OSLoop *OS2Loop::instance( intf_thread_t *pIntf )
{
    if( pIntf->p_sys->p_osLoop == NULL )
    {
        OSLoop *pOsLoop = new OS2Loop( pIntf );
        pIntf->p_sys->p_osLoop = pOsLoop;
    }
    return pIntf->p_sys->p_osLoop;
}


void OS2Loop::destroy( intf_thread_t *pIntf )
{
    delete pIntf->p_sys->p_osLoop;
    pIntf->p_sys->p_osLoop = NULL;
}


void OS2Loop::run()
{
    QMSG qm;

    // Compute windows message list
    while( WinGetMsg( 0, &qm, NULLHANDLE, 0, 0 ))
        WinDispatchMsg( 0, &qm );
}


MRESULT EXPENTRY OS2Loop::processEvent( HWND hwnd, ULONG msg,
                                        MPARAM mp1, MPARAM mp2 )
{
    OS2Factory *pFactory =
        (OS2Factory*)OS2Factory::instance( getIntf() );
    GenericWindow *pWin = pFactory->m_windowMap[hwnd];

    // To invert Y
    RECTL rcl;
    WinQueryWindowRect( hwnd, &rcl );

    GenericWindow &win = *pWin;
    switch( msg )
    {
        case WM_PAINT:
        {
            HPS   hps;
            RECTL rclPaint;

            hps = WinBeginPaint( hwnd, NULLHANDLE, &rclPaint );
            EvtRefresh evt( getIntf(),
                            rclPaint.xLeft,
                            // Find top and invert it
                            ( rcl.yTop - 1 ) - ( rclPaint.yTop - 1 ),
                            rclPaint.xRight - rclPaint.xLeft + 1,
                            rclPaint.yTop - rclPaint.yBottom + 1 );
            win.processEvent( evt );
            WinEndPaint( hps );
            return 0;
        }
        case WM_COMMAND:
        {
            EvtMenu evt( getIntf(), SHORT1FROMMP( mp1 ));
            win.processEvent( evt );
            return 0;
        }
        case WM_MOUSEMOVE:
        {
            pFactory->changeCursor( pFactory->getCursorType());

            // Compute the absolute position of the mouse
            POINTL ptl;
            WinQueryPointerPos( HWND_DESKTOP, &ptl );
            int x = ptl.x;
            int y = ( pFactory->getScreenHeight() - 1 ) - ptl.y;
            EvtMotion evt( getIntf(), x, y );
            win.processEvent( evt );

            return MRFROMLONG( TRUE );
        }
        case WM_MOUSELEAVE:
        {
            EvtLeave evt( getIntf() );
            win.processEvent( evt );
            return MRFROMLONG( TRUE );
        }
        case WM_BUTTON1DOWN:
        {
            WinSetCapture( HWND_DESKTOP, hwnd );
            EvtMouse evt( getIntf(), GET_X_MP( mp1 ),
                          GET_Y_MP( mp1 ), EvtMouse::kLeft,
                          EvtMouse::kDown, getMod( mp2 ) );
            win.processEvent( evt );
            return MRFROMLONG( TRUE );
        }
        case WM_BUTTON2DOWN:
        {
            WinSetCapture( HWND_DESKTOP, hwnd );
            EvtMouse evt( getIntf(), GET_X_MP( mp1 ),
                          GET_Y_MP( mp1 ), EvtMouse::kRight,
                          EvtMouse::kDown, getMod( mp2 ) );
            win.processEvent( evt );
            return MRFROMLONG( TRUE );
        }
        case WM_BUTTON1UP:
        {
            WinSetCapture( HWND_DESKTOP, NULLHANDLE );
            EvtMouse evt( getIntf(), GET_X_MP( mp1 ),
                          GET_Y_MP( mp1 ), EvtMouse::kLeft,
                          EvtMouse::kUp, getMod( mp2 ) );
            win.processEvent( evt );
            return MRFROMLONG( TRUE );
        }
        case WM_BUTTON2UP:
        {
            WinSetCapture( HWND_DESKTOP, NULLHANDLE );
            EvtMouse evt( getIntf(), GET_X_MP( mp1 ),
                          GET_Y_MP( mp1 ), EvtMouse::kRight,
                          EvtMouse::kUp, getMod( mp2 ) );
            win.processEvent( evt );
            return MRFROMLONG( TRUE );
        }
        case WM_BUTTON1DBLCLK:
        {
            WinSetCapture( HWND_DESKTOP, NULLHANDLE );
            EvtMouse evt( getIntf(), GET_X_MP( mp1 ),
                          GET_Y_MP( mp1 ), EvtMouse::kLeft,
                          EvtMouse::kDblClick, getMod( mp2 ) );
            win.processEvent( evt );
            return MRFROMLONG( TRUE );
        }
        case WM_BUTTON2DBLCLK:
        {
            WinSetCapture( HWND_DESKTOP, NULLHANDLE );
            EvtMouse evt( getIntf(), GET_X_MP( mp1 ),
                          GET_Y_MP( mp1 ), EvtMouse::kRight,
                          EvtMouse::kDblClick, getMod( mp2 ) );
            win.processEvent( evt );
            return MRFROMLONG( TRUE );
        }
        case WM_CHAR:
        {
            // The key events are first processed here and not translated
            // into WM_CHAR events because we need to know the status of
            // the modifier keys.

            USHORT fsFlags = SHORT1FROMMP( mp1 );
            USHORT usCh    = SHORT1FROMMP( mp2 );
            USHORT usVk    = SHORT2FROMMP( mp2 );

            // Get VLC key code from the virtual key code
            int key = ( fsFlags & KC_VIRTUALKEY ) ?
                            virtKeyToVlcKey[ usVk ] : 0;
            if( !key )
            {
                // This appears to be a "normal" (ascii) key
                key = tolower( usCh );
            }

            if( key )
            {
                // Get the modifier
                int mod = 0;
                if( fsFlags & KC_CTRL )
                {
                    mod |= EvtInput::kModCtrl;
                }
                if( fsFlags & KC_SHIFT )
                {
                    mod |= EvtInput::kModShift;
                }
                if( fsFlags & KC_ALT )
                {
                    mod |= EvtInput::kModAlt;
                }

                // Get the state
                EvtKey::ActionType_t state;
                if( fsFlags & KC_KEYUP )
                {
                    state = EvtKey::kUp;
                }
                else
                {
                    state = EvtKey::kDown;
                }

                EvtKey evt( getIntf(), key, state, mod );
                win.processEvent( evt );
            }
            return MRFROMLONG( TRUE );
        }
        default:
            break;
    }
    return WinDefWindowProc( hwnd, msg, mp1, mp2 );
}


int OS2Loop::getMod( MPARAM mp ) const
{
    int mod = EvtInput::kModNone;
    if( SHORT2FROMMP( mp ) & KC_CTRL )
        mod |= EvtInput::kModCtrl;
    if( SHORT2FROMMP( mp ) & KC_SHIFT )
        mod |= EvtInput::kModShift;

    return mod;
}


void OS2Loop::exit()
{
    WinPostQueueMsg( HMQ_CURRENT, WM_QUIT, 0, 0 );
}

#endif
