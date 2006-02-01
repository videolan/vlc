/*****************************************************************************
 * x11_loop.cpp
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

#ifdef X11_SKINS

#include <X11/keysym.h>
#include "x11_loop.hpp"
#include "x11_display.hpp"
#include "x11_dragdrop.hpp"
#include "x11_factory.hpp"
#include "x11_timer.hpp"
#include "../src/generic_window.hpp"
#include "../src/theme.hpp"
#include "../src/window_manager.hpp"
#include "../events/evt_focus.hpp"
#include "../events/evt_key.hpp"
#include "../events/evt_mouse.hpp"
#include "../events/evt_motion.hpp"
#include "../events/evt_leave.hpp"
#include "../events/evt_refresh.hpp"
#include "../events/evt_scroll.hpp"
#include "../commands/async_queue.hpp"
#include "../utils/var_bool.hpp"
#include "vlc_keys.h"


// Maximum interval between clicks for a double-click (in microsec)
int X11Loop::m_dblClickDelay = 400000;


X11Loop::X11Loop( intf_thread_t *pIntf, X11Display &rDisplay ):
    OSLoop( pIntf ), m_rDisplay( rDisplay ), m_exit( false ),
    m_lastClickTime( 0 ), m_lastClickPosX( 0 ), m_lastClickPosY( 0 )
{
    // Initialize the key map
    keysymToVlcKey[XK_F1] = KEY_F1;
    keysymToVlcKey[XK_F2] = KEY_F2;
    keysymToVlcKey[XK_F3] = KEY_F3;
    keysymToVlcKey[XK_F4] = KEY_F4;
    keysymToVlcKey[XK_F5] = KEY_F5;
    keysymToVlcKey[XK_F6] = KEY_F6;
    keysymToVlcKey[XK_F7] = KEY_F7;
    keysymToVlcKey[XK_F8] = KEY_F8;
    keysymToVlcKey[XK_F9] = KEY_F9;
    keysymToVlcKey[XK_F10] = KEY_F10;
    keysymToVlcKey[XK_F11] = KEY_F11;
    keysymToVlcKey[XK_F12] = KEY_F12;
    keysymToVlcKey[XK_Return] = KEY_ENTER;
    keysymToVlcKey[XK_space] = KEY_SPACE;
    keysymToVlcKey[XK_Escape] = KEY_ESC;
    keysymToVlcKey[XK_Left] = KEY_LEFT;
    keysymToVlcKey[XK_Right] = KEY_RIGHT;
    keysymToVlcKey[XK_Up] = KEY_UP;
    keysymToVlcKey[XK_Down] = KEY_DOWN;
    keysymToVlcKey[XK_Home] = KEY_HOME;
    keysymToVlcKey[XK_End] = KEY_END;
    keysymToVlcKey[XK_Prior] = KEY_PAGEUP;
    keysymToVlcKey[XK_Next] = KEY_PAGEDOWN;
}


X11Loop::~X11Loop()
{
}


OSLoop *X11Loop::instance( intf_thread_t *pIntf, X11Display &rDisplay )
{
    if( pIntf->p_sys->p_osLoop == NULL )
    {
        OSLoop *pOsLoop = new X11Loop( pIntf, rDisplay );
        pIntf->p_sys->p_osLoop = pOsLoop;
    }
    return pIntf->p_sys->p_osLoop;
}


void X11Loop::destroy( intf_thread_t *pIntf )
{
    if( pIntf->p_sys->p_osLoop )
    {
        delete pIntf->p_sys->p_osLoop;
        pIntf->p_sys->p_osLoop = NULL;
    }
}


void X11Loop::run()
{
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );
    X11TimerLoop *pTimerLoop = ((X11Factory*)pOsFactory)->getTimerLoop();

    // Main event loop
    while( ! m_exit )
    {
        int nPending;

        // Number of pending events in the queue
        nPending = XPending( XDISPLAY );

        while( ! m_exit && nPending > 0 )
        {
            // Handle the next X11 event
            handleX11Event();

            // Number of pending events in the queue
            nPending = XPending( XDISPLAY );
        }

        // Wait for the next timer and execute it
        // The sleep is interrupted if an X11 event is received
        if( !m_exit )
        {
            pTimerLoop->waitNextTimer();
        }
    }
}


void X11Loop::exit()
{
    m_exit = true;
}


void X11Loop::handleX11Event()
{
    XEvent event;
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );

    // Look for the next event in the queue
    XNextEvent( XDISPLAY, &event );

    if( event.xany.window == m_rDisplay.getMainWindow() )
    {
        if( event.type == MapNotify )
        {
            // When the "parent" window is mapped, show all the visible
            // windows, as it is not automatic, unfortunately
            Theme *pTheme = getIntf()->p_sys->p_theme;
            if( pTheme )
            {
                pTheme->getWindowManager().synchVisibility();
            }
        }
        return;
    }

    // Find the window to which the event is sent
    GenericWindow *pWin =
        ((X11Factory*)pOsFactory)->m_windowMap[event.xany.window];

    if( !pWin )
    {
        msg_Dbg( getIntf(), "No associated generic window !!" );
        return;
    }

    // Send the right event object to the window
    switch( event.type )
    {
        case Expose:
        {
            EvtRefresh evt( getIntf(), event.xexpose.x,
                            event.xexpose.y, event.xexpose.width,
                            event.xexpose.height );
            pWin->processEvent( evt );
            break;
        }
        case FocusIn:
        {
            EvtFocus evt( getIntf(), true );
            pWin->processEvent( evt );
            break;
        }
        case FocusOut:
        {
            EvtFocus evt( getIntf(), false );
            pWin->processEvent( evt );
            break;
        }

        case MotionNotify:
        {
            // Don't trust the position in the event, it is
            // out of date. Get the actual current position instead
            int x, y;
            pOsFactory->getMousePos( x, y );
            EvtMotion evt( getIntf(), x, y );
            pWin->processEvent( evt );
            break;
        }
        case LeaveNotify:
        {
            EvtLeave evt( getIntf() );
            pWin->processEvent( evt );
            break;
        }
        case ButtonPress:
        case ButtonRelease:
        {
            EvtMouse::ActionType_t action = EvtMouse::kDown;
            switch( event.type )
            {
                case ButtonPress:
                    action = EvtMouse::kDown;
                    break;
                case ButtonRelease:
                    action = EvtMouse::kUp;
                    break;
            }

            // Get the modifiers
            int mod = EvtInput::kModNone;
            if( event.xbutton.state & Mod1Mask )
            {
                mod |= EvtInput::kModAlt;
            }
            if( event.xbutton.state & ControlMask )
            {
                mod |= EvtInput::kModCtrl;
            }
            if( event.xbutton.state & ShiftMask )
            {
                mod |= EvtInput::kModShift;
            }

            // Check for double clicks
            if( event.type == ButtonPress &&
                event.xbutton.button == 1 )
            {
                mtime_t time = mdate();
                int x, y;
                pOsFactory->getMousePos( x, y );
                if( time - m_lastClickTime < m_dblClickDelay &&
                    x == m_lastClickPosX && y == m_lastClickPosY )
                {
                    m_lastClickTime = 0;
                    action = EvtMouse::kDblClick;
                }
                else
                {
                    m_lastClickTime = time;
                    m_lastClickPosX = x;
                    m_lastClickPosY = y;
                }
            }

            switch( event.xbutton.button )
            {
                case 1:
                {
                    EvtMouse evt( getIntf(), event.xbutton.x,
                                  event.xbutton.y, EvtMouse::kLeft,
                                  action, mod );
                    pWin->processEvent( evt );
                    break;
                }
                case 2:
                {
                    EvtMouse evt( getIntf(), event.xbutton.x,
                                  event.xbutton.y, EvtMouse::kMiddle,
                                  action, mod );
                    pWin->processEvent( evt );
                    break;
                }
                case 3:
                {
                    EvtMouse evt( getIntf(), event.xbutton.x,
                                  event.xbutton.y, EvtMouse::kRight,
                                  action, mod );
                    pWin->processEvent( evt );
                    break;
                }
                case 4:
                {
                    // Scroll up
                    EvtScroll evt( getIntf(), event.xbutton.x,
                                   event.xbutton.y, EvtScroll::kUp,
                                   mod );
                    pWin->processEvent( evt );
                    break;
                }
                case 5:
                {
                    // Scroll down
                    EvtScroll evt( getIntf(), event.xbutton.x,
                                   event.xbutton.y, EvtScroll::kDown,
                                   mod );
                    pWin->processEvent( evt );
                    break;
                }
            }
            break;
        }
        case KeyPress:
        case KeyRelease:
        {
            EvtKey::ActionType_t action = EvtKey::kDown;
            int mod = EvtInput::kModNone;
            // Get the modifiers
            if( event.xkey.state & Mod1Mask )
            {
                mod |= EvtInput::kModAlt;
            }
            if( event.xkey.state & ControlMask )
            {
                mod |= EvtInput::kModCtrl;
            }
            if( event.xkey.state & ShiftMask )
            {
                mod |= EvtInput::kModShift;
            }

            // Take the first keysym = lower case character
            KeySym keysym = XLookupKeysym( &event.xkey, 0 );

            // Get VLC key code from the keysym
            int key = keysymToVlcKey[keysym];
            if( !key )
            {
                // Normal key
                key = keysym;
            }

            switch( event.type )
            {
                case KeyPress:
                    action = EvtKey::kDown;
                    break;
                case KeyRelease:
                    action = EvtKey::kUp;
                    break;
            }
            EvtKey evt( getIntf(), key, action, mod );
            pWin->processEvent( evt );
            break;
        }

        case ClientMessage:
        {
            // Get the message type
            string type = XGetAtomName( XDISPLAY, event.xclient.message_type );

            // Find the DnD object for this window
            X11DragDrop *pDnd =
                ((X11Factory*)pOsFactory)->m_dndMap[event.xany.window];
            if( !pDnd )
            {
                msg_Err( getIntf(), "No associated D&D object !!" );
                return;
            }

            if( type == "XdndEnter" )
            {
                pDnd->dndEnter( event.xclient.data.l );
            }
            else if( type == "XdndPosition" )
            {
                pDnd->dndPosition( event.xclient.data.l );
            }
            else if( type == "XdndLeave" )
            {
                pDnd->dndLeave( event.xclient.data.l );
            }
            else if( type == "XdndDrop" )
            {
                pDnd->dndDrop( event.xclient.data.l );
            }
            break;
        }
    }
}

#endif
