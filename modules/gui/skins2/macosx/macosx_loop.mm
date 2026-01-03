/*****************************************************************************
 * macosx_loop.mm
 *****************************************************************************
 * Copyright (C) 2024 the VideoLAN team
 *
 * Authors: VLC contributors
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

#include "macosx_loop.hpp"
#include "macosx_factory.hpp"
#include "macosx_window.hpp"
#include "../src/os_factory.hpp"
#include "../src/generic_window.hpp"
#include "../events/evt_key.hpp"
#include "../events/evt_mouse.hpp"
#include "../events/evt_scroll.hpp"
#include "../events/evt_motion.hpp"
#include "../events/evt_leave.hpp"
#include "../events/evt_focus.hpp"
#include "../events/evt_refresh.hpp"

#include <vlc_actions.h>


// Double-click delay in microseconds
int MacOSXLoop::m_dblClickDelay = 400000;


MacOSXLoop::MacOSXLoop( intf_thread_t *pIntf ):
    OSLoop( pIntf ), m_exit( false ),
    m_lastClickTime( 0 ), m_lastClickPosX( 0 ), m_lastClickPosY( 0 )
{
    @autoreleasepool {
        NSScreen *mainScreen = [NSScreen mainScreen];
        m_screenHeight = (int)[mainScreen frame].size.height;
    }
}


MacOSXLoop::~MacOSXLoop()
{
}


OSLoop *MacOSXLoop::instance( intf_thread_t *pIntf )
{
    if( !pIntf->p_sys->p_osLoop )
    {
        pIntf->p_sys->p_osLoop.reset( new MacOSXLoop( pIntf ) );
    }
    return pIntf->p_sys->p_osLoop.get();
}


void MacOSXLoop::destroy( intf_thread_t *pIntf )
{
    pIntf->p_sys->p_osLoop.reset();
}


void MacOSXLoop::run()
{
    @autoreleasepool {
        // Main event loop
        while( !m_exit )
        {
            @autoreleasepool {
                NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                    untilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]
                                                       inMode:NSDefaultRunLoopMode
                                                      dequeue:YES];
                if( event )
                {
                    handleEvent( (__bridge void *)event );
                    [NSApp sendEvent:event];
                }

                // Process timers
                MacOSXFactory *pFactory = static_cast<MacOSXFactory*>(
                    OSFactory::instance( getIntf() ) );
                if( pFactory && pFactory->getTimerLoop() )
                {
                    pFactory->getTimerLoop()->checkTimers();
                }
            }
        }
    }
}


void MacOSXLoop::exit()
{
    m_exit = true;
}


void MacOSXLoop::handleEvent( void *pEvent )
{
    @autoreleasepool {
        NSEvent *event = (__bridge NSEvent *)pEvent;

        // Get the window
        NSWindow *nsWindow = [event window];
        if( !nsWindow )
            return;

        // Find the associated GenericWindow
        MacOSXFactory *pFactory = static_cast<MacOSXFactory*>(
            OSFactory::instance( getIntf() ) );
        if( !pFactory )
            return;

        auto it = pFactory->m_windowMap.find( (__bridge void *)nsWindow );
        if( it == pFactory->m_windowMap.end() )
            return;

        GenericWindow *pWin = it->second;
        if( !pWin )
            return;

        // Get mouse position in window coordinates
        NSPoint locationInWindow = [event locationInWindow];
        NSView *contentView = [nsWindow contentView];
        NSRect bounds = [contentView bounds];

        // Convert to skins coordinates (origin top-left)
        int x = (int)locationInWindow.x;
        int y = (int)(bounds.size.height - locationInWindow.y);

        // Handle the event
        NSEventType eventType = [event type];

        switch( eventType )
        {
            case NSEventTypeLeftMouseDown:
            {
                vlc_tick_t time = vlc_tick_now();
                int xPos = x;
                int yPos = y;

                // Check for double-click
                EvtMouse::ActionType action = EvtMouse::kDown;
                if( time - m_lastClickTime < m_dblClickDelay &&
                    xPos == m_lastClickPosX && yPos == m_lastClickPosY )
                {
                    action = EvtMouse::kDblClick;
                    m_lastClickTime = 0;
                }
                else
                {
                    m_lastClickTime = time;
                    m_lastClickPosX = xPos;
                    m_lastClickPosY = yPos;
                }

                EvtMouse evt( getIntf(), xPos, yPos, EvtMouse::kLeft, action );
                pWin->processEvent( evt );
                break;
            }

            case NSEventTypeLeftMouseUp:
            {
                EvtMouse evt( getIntf(), x, y, EvtMouse::kLeft, EvtMouse::kUp );
                pWin->processEvent( evt );
                break;
            }

            case NSEventTypeRightMouseDown:
            {
                EvtMouse evt( getIntf(), x, y, EvtMouse::kRight, EvtMouse::kDown );
                pWin->processEvent( evt );
                break;
            }

            case NSEventTypeRightMouseUp:
            {
                EvtMouse evt( getIntf(), x, y, EvtMouse::kRight, EvtMouse::kUp );
                pWin->processEvent( evt );
                break;
            }

            case NSEventTypeOtherMouseDown:
            {
                EvtMouse evt( getIntf(), x, y, EvtMouse::kMiddle, EvtMouse::kDown );
                pWin->processEvent( evt );
                break;
            }

            case NSEventTypeOtherMouseUp:
            {
                EvtMouse evt( getIntf(), x, y, EvtMouse::kMiddle, EvtMouse::kUp );
                pWin->processEvent( evt );
                break;
            }

            case NSEventTypeMouseMoved:
            case NSEventTypeLeftMouseDragged:
            case NSEventTypeRightMouseDragged:
            case NSEventTypeOtherMouseDragged:
            {
                EvtMotion evt( getIntf(), x, y );
                pWin->processEvent( evt );
                break;
            }

            case NSEventTypeMouseEntered:
            {
                EvtMotion evt( getIntf(), x, y );
                pWin->processEvent( evt );
                break;
            }

            case NSEventTypeMouseExited:
            {
                EvtLeave evt( getIntf() );
                pWin->processEvent( evt );
                break;
            }

            case NSEventTypeScrollWheel:
            {
                CGFloat deltaY = [event deltaY];
                int direction = (deltaY > 0) ? EvtScroll::kUp : EvtScroll::kDown;
                int mod = cocoaModToMod( [event modifierFlags] );
                EvtScroll evt( getIntf(), x, y, direction, mod );
                pWin->processEvent( evt );
                break;
            }

            case NSEventTypeKeyDown:
            {
                int mod = cocoaModToMod( [event modifierFlags] );
                int key = cocoaKeyToVlcKey( [event keyCode], [event modifierFlags] );

                // Also handle character input
                NSString *chars = [event charactersIgnoringModifiers];
                if( [chars length] > 0 )
                {
                    unichar c = [chars characterAtIndex:0];
                    if( c >= 32 && c < 127 )
                    {
                        key = c;
                    }
                }

                EvtKey evt( getIntf(), key, EvtKey::kDown, mod );
                pWin->processEvent( evt );
                break;
            }

            case NSEventTypeKeyUp:
            {
                int mod = cocoaModToMod( [event modifierFlags] );
                int key = cocoaKeyToVlcKey( [event keyCode], [event modifierFlags] );

                NSString *chars = [event charactersIgnoringModifiers];
                if( [chars length] > 0 )
                {
                    unichar c = [chars characterAtIndex:0];
                    if( c >= 32 && c < 127 )
                    {
                        key = c;
                    }
                }

                EvtKey evt( getIntf(), key, EvtKey::kUp, mod );
                pWin->processEvent( evt );
                break;
            }

            default:
                break;
        }
    }
}


int MacOSXLoop::cocoaModToMod( unsigned int flags )
{
    int mod = 0;

    if( flags & NSEventModifierFlagShift )
        mod |= KEY_MODIFIER_SHIFT;
    if( flags & NSEventModifierFlagControl )
        mod |= KEY_MODIFIER_CTRL;
    if( flags & NSEventModifierFlagOption )
        mod |= KEY_MODIFIER_ALT;
    if( flags & NSEventModifierFlagCommand )
        mod |= KEY_MODIFIER_META;

    return mod;
}


int MacOSXLoop::cocoaKeyToVlcKey( unsigned short keyCode, unsigned int flags )
{
    // Map macOS key codes to VLC key codes
    switch( keyCode )
    {
        case kVK_Return:        return KEY_ENTER;
        case kVK_Tab:           return KEY_TAB;
        case kVK_Delete:        return KEY_BACKSPACE;
        case kVK_ForwardDelete: return KEY_DELETE;
        case kVK_Escape:        return KEY_ESC;
        case kVK_Space:         return ' ';

        case kVK_UpArrow:       return KEY_UP;
        case kVK_DownArrow:     return KEY_DOWN;
        case kVK_LeftArrow:     return KEY_LEFT;
        case kVK_RightArrow:    return KEY_RIGHT;

        case kVK_Home:          return KEY_HOME;
        case kVK_End:           return KEY_END;
        case kVK_PageUp:        return KEY_PAGEUP;
        case kVK_PageDown:      return KEY_PAGEDOWN;

        case kVK_F1:            return KEY_F1;
        case kVK_F2:            return KEY_F2;
        case kVK_F3:            return KEY_F3;
        case kVK_F4:            return KEY_F4;
        case kVK_F5:            return KEY_F5;
        case kVK_F6:            return KEY_F6;
        case kVK_F7:            return KEY_F7;
        case kVK_F8:            return KEY_F8;
        case kVK_F9:            return KEY_F9;
        case kVK_F10:           return KEY_F10;
        case kVK_F11:           return KEY_F11;
        case kVK_F12:           return KEY_F12;

        default:                return 0;
    }
}
