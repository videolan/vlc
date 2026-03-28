/*****************************************************************************
 * macosx_loop.mm
 *****************************************************************************
 * Copyright (C) 2026 the VideoLAN team
 *
 * Authors: Fletcher Holt <fletcherholt649@gmail.com>
 *          Felix Paul Kühne <fkuehne@videolan.org>
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
#include <CoreFoundation/CoreFoundation.h>
#include "macosx_loop.hpp"
#include "macosx_factory.hpp"
#include "macosx_timer.hpp"
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
#include "../src/dialogs.hpp"
#include "../commands/cmd_dialogs.hpp"
#include "../commands/cmd_fullscreen.hpp"
#include "../commands/async_queue.hpp"

/// Helper class to dispatch menu actions to skins2 commands
@interface VLCSkinsMenuHandler : NSObject
{
    intf_thread_t *m_pIntf;
}
- (instancetype)initWithIntf:(intf_thread_t *)pIntf;
- (void)openFile:(id)sender;
- (void)openDirectory:(id)sender;
- (void)toggleFullscreen:(id)sender;
@end

@implementation VLCSkinsMenuHandler

- (instancetype)initWithIntf:(intf_thread_t *)pIntf
{
    self = [super init];
    if( self )
        m_pIntf = pIntf;
    return self;
}

- (void)openFile:(id)sender
{
    (void)sender;
    Dialogs *dlg = Dialogs::instance( m_pIntf );
    if( dlg )
        dlg->showFileSimple( true );
}

- (void)openDirectory:(id)sender
{
    (void)sender;
    Dialogs *dlg = Dialogs::instance( m_pIntf );
    if( dlg )
        dlg->showDirectory( true );
}

- (void)toggleFullscreen:(id)sender
{
    (void)sender;
    CmdFullscreen *pCmd = new CmdFullscreen( m_pIntf );
    AsyncQueue *pQueue = AsyncQueue::instance( m_pIntf );
    if( pQueue )
        pQueue->push( CmdGenericPtr( pCmd ) );
}

@end

static inline NSString *_NS( const char *s )
{
    return s ? [NSString stringWithUTF8String:vlc_gettext( s )] : @"";
}

// Double-click delay in microseconds
int MacOSXLoop::m_dblClickDelay = 400000;


MacOSXLoop::MacOSXLoop( intf_thread_t *pIntf ):
    OSLoop( pIntf ), m_exit( false ),
    m_lastClickTime( 0 ), m_lastClickPosX( 0 ), m_lastClickPosY( 0 )
{
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
    m_exitSemaphore = dispatch_semaphore_create( 0 );

    CFRunLoopPerformBlock(CFRunLoopGetMain(), kCFRunLoopDefaultMode, ^{
        @autoreleasepool {
            // Monitor events to dispatch to skins
            m_pMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskAny
                                                              handler:^NSEvent *(NSEvent *event) {
                handleEvent( (__bridge void *)event );
                return event;
            }];

            // Periodically check skins timers.
            // Use NSRunLoopCommonModes so the timer also fires during
            // mouse drags (NSEventTrackingRunLoopMode), which is needed
            // for the AsyncQueue to flush resize commands while dragging.
            m_pTimer = [NSTimer timerWithTimeInterval:0.01
                                              repeats:YES
                                                block:^(NSTimer *timer) {
                @autoreleasepool {
                    if( m_exit )
                    {
                        [timer invalidate];
                        [NSEvent removeMonitor:m_pMonitor];
                        m_pMonitor = nil;
                        return;
                    }

                    MacOSXFactory *pFactory = static_cast<MacOSXFactory*>(
                        OSFactory::instance( getIntf() ) );
                    if( pFactory && pFactory->getTimerLoop() )
                    {
                        pFactory->getTimerLoop()->checkTimers();
                    }
                }
            }];
            [[NSRunLoop currentRunLoop] addTimer:m_pTimer
                                         forMode:NSRunLoopCommonModes];

            // Set up the menu bar after finishLaunching.
            // First, disable window restoration BEFORE finishLaunching
            // to prevent AppKit from restoring windows belonging to a
            // different interface (e.g. the native macOS one).
            if( [NSApp respondsToSelector:@selector(disableRelaunchOnLogin)] )
                [NSApp disableRelaunchOnLogin];
            [[NSUserDefaults standardUserDefaults] setBool:NO
                                                    forKey:@"NSQuitAlwaysKeepsWindows"];
            [NSApp finishLaunching];

            VLCSkinsMenuHandler *menuHandler =
                [[VLCSkinsMenuHandler alloc] initWithIntf:getIntf()];

            NSMenu *menuBar = [[NSMenu alloc] init];

            // Application menu
            NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
            NSMenu *appMenu = [[NSMenu alloc] initWithTitle:@"VLC"];
            [appMenu addItemWithTitle:_NS("About")
                               action:@selector(orderFrontStandardAboutPanel:)
                        keyEquivalent:@""];
            [appMenu addItem:[NSMenuItem separatorItem]];
            [appMenu addItemWithTitle:_NS("Hide")
                               action:@selector(hide:)
                        keyEquivalent:@"h"];
            NSMenuItem *hideOthers = [appMenu addItemWithTitle:_NS("Hide Others")
                               action:@selector(hideOtherApplications:)
                        keyEquivalent:@"h"];
            [hideOthers setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagOption];
            [appMenu addItemWithTitle:_NS("Show All")
                               action:@selector(unhideAllApplications:)
                        keyEquivalent:@""];
            [appMenu addItem:[NSMenuItem separatorItem]];
            [appMenu addItemWithTitle:_NS("Quit")
                               action:@selector(terminate:)
                        keyEquivalent:@"q"];
            [appMenuItem setSubmenu:appMenu];
            [menuBar addItem:appMenuItem];

            // File menu
            NSMenuItem *fileMenuItem = [[NSMenuItem alloc] init];
            NSMenu *fileMenu = [[NSMenu alloc] initWithTitle:_NS("File")];
            NSMenuItem *openFile = [fileMenu addItemWithTitle:_NS("Open File...")
                                action:@selector(openFile:)
                         keyEquivalent:@"o"];
            [openFile setTarget:menuHandler];
            NSMenuItem *openDir = [fileMenu addItemWithTitle:_NS("Open Folder")
                                action:@selector(openDirectory:)
                         keyEquivalent:@"d"];
            [openDir setTarget:menuHandler];
            [fileMenuItem setSubmenu:fileMenu];
            [menuBar addItem:fileMenuItem];

            // Window menu
            NSMenuItem *windowMenuItem = [[NSMenuItem alloc] init];
            NSMenu *windowMenu = [[NSMenu alloc] initWithTitle:_NS("Window")];
            [windowMenu addItemWithTitle:_NS("Minimize")
                                  action:@selector(performMiniaturize:)
                           keyEquivalent:@"m"];
            [windowMenu addItemWithTitle:_NS("Close")
                                  action:@selector(performClose:)
                           keyEquivalent:@"w"];
            [windowMenu addItem:[NSMenuItem separatorItem]];
            NSMenuItem *fsItem = [windowMenu addItemWithTitle:_NS("Toggle Fullscreen mode")
                                  action:@selector(toggleFullscreen:)
                           keyEquivalent:@"f"];
            [fsItem setTarget:menuHandler];
            [windowMenuItem setSubmenu:windowMenu];
            [menuBar addItem:windowMenuItem];
            [NSApp setWindowsMenu:windowMenu];

            [NSApp setMainMenu:menuBar];
            [NSApp activateIgnoringOtherApps:YES];

            // Run the event loop (finishLaunching already called above)
            [NSApp run];

            // Clean up before signaling — the timer and monitor must
            // be invalidated now because the skins2 thread will proceed
            // to destroy the interface as soon as the semaphore is signaled.
            [m_pTimer invalidate];
            m_pTimer = nil;
            [NSEvent removeMonitor:m_pMonitor];
            m_pMonitor = nil;
        }

        // [NSApp run] returned — signal the skins2 thread
        dispatch_semaphore_signal( m_exitSemaphore );
    });
    CFRunLoopWakeUp(CFRunLoopGetMain());

    // Block until [NSApp run] exits
    dispatch_semaphore_wait( m_exitSemaphore, DISPATCH_TIME_FOREVER );
}


void MacOSXLoop::exit()
{
    m_exit = true;
    dispatch_async(dispatch_get_main_queue(), ^{
        [NSApp stop:nil];
        // Post a dummy event to ensure [NSApp run] returns
        [NSApp postEvent:[NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                            location:NSZeroPoint
                                       modifierFlags:0
                                           timestamp:0
                                        windowNumber:0
                                             context:nil
                                             subtype:0
                                               data1:0
                                               data2:0]
                 atStart:YES];
    });
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

        // Compute both window-relative and screen-absolute positions.
        // EvtMouse uses window-relative coords (for hit testing),
        // EvtMotion uses screen-absolute coords (for window dragging),
        // matching the X11 and Win32 conventions.
        NSPoint locationInWindow = [event locationInWindow];
        NSView *contentView = [nsWindow contentView];
        NSRect bounds = [contentView bounds];

        int x = locationInWindow.x;
        int y = bounds.size.height - locationInWindow.y;

        // Use the global mouse position for screen-absolute coords,
        // like X11's getMousePos().  This avoids depending on the
        // NSWindow frame which may lag behind after async moveResize.
        NSPoint mouseLocation = [NSEvent mouseLocation];
        CGFloat screenHeight = [NSScreen mainScreen].frame.size.height;
        int xAbs = mouseLocation.x;
        int yAbs = screenHeight - mouseLocation.y;

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
                EvtMouse::ActionType_t action = EvtMouse::kDown;
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
                EvtMotion evt( getIntf(), xAbs, yAbs );
                pWin->processEvent( evt );
                break;
            }

            case NSEventTypeMouseEntered:
            {
                EvtMotion evt( getIntf(), xAbs, yAbs );
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
                EvtScroll::Direction_t direction = (deltaY > 0) ? EvtScroll::kUp : EvtScroll::kDown;
                int mod = cocoaModToMod( [event modifierFlags] );
                EvtScroll evt( getIntf(), x, y, direction, mod );
                pWin->processEvent( evt );
                break;
            }

            case NSEventTypeKeyDown:
            case NSEventTypeKeyUp:
            {
                int mod = cocoaModToMod( [event modifierFlags] );
                int key = 0;

                NSString *chars = [event charactersIgnoringModifiers];
                if( [chars length] > 0 )
                {
                    unichar c = [chars characterAtIndex:0];
                    key = cocoaCharToVlcKey( c );
                }

                EvtKey::ActionType_t action = (eventType == NSEventTypeKeyDown)
                    ? EvtKey::kDown : EvtKey::kUp;
                EvtKey evt( getIntf(), key, action, mod );
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


// Mirrors CocoaKeyToVLC() in modules/gui/macosx/extensions/NSString+Helpers.m
int MacOSXLoop::cocoaCharToVlcKey( unichar c )
{
    if( c >= 32 && c < 127 )
        return c;

    switch( c )
    {
        case NSCarriageReturnCharacter: return KEY_ENTER;
        case NSEnterCharacter:          return KEY_ENTER;
        case NSTabCharacter:            return KEY_TAB;
        case NSBackspaceCharacter:      return KEY_BACKSPACE;
        case NSDeleteCharacter:         return KEY_DELETE;
        case 0x1B:                      return KEY_ESC;

        case NSUpArrowFunctionKey:      return KEY_UP;
        case NSDownArrowFunctionKey:    return KEY_DOWN;
        case NSLeftArrowFunctionKey:    return KEY_LEFT;
        case NSRightArrowFunctionKey:   return KEY_RIGHT;

        case NSHomeFunctionKey:         return KEY_HOME;
        case NSEndFunctionKey:          return KEY_END;
        case NSPageUpFunctionKey:       return KEY_PAGEUP;
        case NSPageDownFunctionKey:     return KEY_PAGEDOWN;
        case NSInsertFunctionKey:       return KEY_INSERT;
        case NSMenuFunctionKey:         return KEY_MENU;

        case NSF1FunctionKey:           return KEY_F1;
        case NSF2FunctionKey:           return KEY_F2;
        case NSF3FunctionKey:           return KEY_F3;
        case NSF4FunctionKey:           return KEY_F4;
        case NSF5FunctionKey:           return KEY_F5;
        case NSF6FunctionKey:           return KEY_F6;
        case NSF7FunctionKey:           return KEY_F7;
        case NSF8FunctionKey:           return KEY_F8;
        case NSF9FunctionKey:           return KEY_F9;
        case NSF10FunctionKey:          return KEY_F10;
        case NSF11FunctionKey:          return KEY_F11;
        case NSF12FunctionKey:          return KEY_F12;

        default:                        return 0;
    }
}
