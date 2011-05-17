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
#include <vlc_keys.h>


// Maximum interval between clicks for a double-click (in microsec)
int X11Loop::m_dblClickDelay = 400000;

X11Loop::keymap_t X11Loop::m_keymap;

X11Loop::X11Loop( intf_thread_t *pIntf, X11Display &rDisplay ):
    OSLoop( pIntf ), m_rDisplay( rDisplay ), m_exit( false ),
    m_lastClickTime( 0 ), m_lastClickPosX( 0 ), m_lastClickPosY( 0 )
{
    if(m_keymap.empty()) {
        // Initialize the key map where VLC keys differ from X11 keys.
        m_keymap[XK_F1] = KEY_F1;
        m_keymap[XK_F2] = KEY_F2;
        m_keymap[XK_F3] = KEY_F3;
        m_keymap[XK_F4] = KEY_F4;
        m_keymap[XK_F5] = KEY_F5;
        m_keymap[XK_F6] = KEY_F6;
        m_keymap[XK_F7] = KEY_F7;
        m_keymap[XK_F8] = KEY_F8;
        m_keymap[XK_F9] = KEY_F9;
        m_keymap[XK_F10] = KEY_F10;
        m_keymap[XK_F11] = KEY_F11;
        m_keymap[XK_F12] = KEY_F12;
        m_keymap[XK_Return] = KEY_ENTER;
        m_keymap[XK_Escape] = KEY_ESC;
        m_keymap[XK_Left] = KEY_LEFT;
        m_keymap[XK_Right] = KEY_RIGHT;
        m_keymap[XK_Up] = KEY_UP;
        m_keymap[XK_Down] = KEY_DOWN;
        m_keymap[XK_Home] = KEY_HOME;
        m_keymap[XK_End] = KEY_END;
        m_keymap[XK_Prior] = KEY_PAGEUP;
        m_keymap[XK_Next] = KEY_PAGEDOWN;
        m_keymap[XK_Delete] = KEY_DELETE;
        m_keymap[XK_Insert] = KEY_INSERT;
    }
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
    delete pIntf->p_sys->p_osLoop;
    pIntf->p_sys->p_osLoop = NULL;
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


inline int X11Loop::X11ModToMod( unsigned state )
{
    int mod = EvtInput::kModNone;
    if( state & Mod1Mask )
        mod |= EvtInput::kModAlt;
    if( state & ControlMask )
        mod |= EvtInput::kModCtrl;
    if( state & ShiftMask )
        mod |= EvtInput::kModShift;
    return mod;
}


void X11Loop::handleX11Event()
{
    XEvent event;
    OSFactory *pOsFactory = OSFactory::instance( getIntf() );

    // Look for the next event in the queue
    XNextEvent( XDISPLAY, &event );

    if( event.xany.window == m_rDisplay.getMainWindow() )
    {
        if( event.type == ClientMessage )
        {
            Atom wm_protocols =
                XInternAtom( XDISPLAY, "WM_PROTOCOLS", False);
            Atom wm_delete =
                XInternAtom( XDISPLAY, "WM_DELETE_WINDOW", False);

            if( event.xclient.message_type == wm_protocols &&
                (Atom)event.xclient.data.l[0] == wm_delete )
            {
                msg_Dbg( getIntf(), "Received WM_DELETE_WINDOW message" );
                libvlc_Quit( getIntf()->p_libvlc );
            }
        }
        return;
    }

    // Find the window to which the event is sent
    GenericWindow *pWin =
        ((X11Factory*)pOsFactory)->m_windowMap[event.xany.window];

    if( !pWin )
    {
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

            int mod = X11ModToMod( event.xbutton.state );

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
                    if( event.type == ButtonPress )
                    {
                        EvtScroll evt( getIntf(), event.xbutton.x,
                                       event.xbutton.y, EvtScroll::kUp,
                                       mod );
                        pWin->processEvent( evt );
                    }
                    break;
                }
                case 5:
                {
                    // Scroll down
                    if( event.type == ButtonPress )
                    {
                        EvtScroll evt( getIntf(), event.xbutton.x,
                                       event.xbutton.y, EvtScroll::kDown,
                                       mod );
                        pWin->processEvent( evt );
                    }
                    break;
                }
            }
            break;
        }
        case KeyPress:
        case KeyRelease:
        {
            // Take the first keysym = lower case character, and translate.
            int key = keysymToVlcKey( XLookupKeysym( &event.xkey, 0 ) );

            EvtKey evt( getIntf(), key,
                        event.type==KeyRelease ? EvtKey::kUp : EvtKey::kDown,
                        X11ModToMod( event.xkey.state ) );
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
                msg_Err( getIntf(), "no associated D&D object" );
                return;
            }

            if( type == "XdndEnter" )
                pDnd->dndEnter( event.xclient.data.l );
            else if( type == "XdndPosition" )
                pDnd->dndPosition( event.xclient.data.l );
            else if( type == "XdndLeave" )
                pDnd->dndLeave( event.xclient.data.l );
            else if( type == "XdndDrop" )
                pDnd->dndDrop( event.xclient.data.l );
            break;
        }
    }
}

#endif
