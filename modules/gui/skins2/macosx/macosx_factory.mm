/*****************************************************************************
 * macosx_factory.mm
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

#include "macosx_factory.hpp"
#include "macosx_graphics.hpp"
#include "macosx_loop.hpp"
#include "macosx_timer.hpp"
#include "macosx_window.hpp"
#include "macosx_tooltip.hpp"
#include "macosx_popup.hpp"
#include "../src/generic_window.hpp"

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>


MacOSXFactory::MacOSXFactory( intf_thread_t *pIntf ):
    OSFactory( pIntf ), m_pTimerLoop( NULL ), m_dirSep( "/" )
{
    // Initialize the resource path
    m_resourcePath.push_back( "share/skins2" );

    // Get user's home directory for skins
    const char *home = getenv( "HOME" );
    if( home )
    {
        std::string userSkins = std::string(home) + "/Library/Application Support/org.videolan.vlc/skins2";
        m_resourcePath.push_back( userSkins );
    }

    // Add application bundle resource path
    @autoreleasepool {
        NSBundle *mainBundle = [NSBundle mainBundle];
        if( mainBundle )
        {
            NSString *resourcePath = [mainBundle resourcePath];
            if( resourcePath )
            {
                std::string bundlePath = [resourcePath UTF8String];
                bundlePath += "/share/skins2";
                m_resourcePath.push_back( bundlePath );
            }
        }
    }
}


MacOSXFactory::~MacOSXFactory()
{
    delete m_pTimerLoop;
}


bool MacOSXFactory::init()
{
    @autoreleasepool {
        // Get screen dimensions
        NSScreen *mainScreen = [NSScreen mainScreen];
        NSRect frame = [mainScreen frame];
        m_screenWidth = (int)frame.size.width;
        m_screenHeight = (int)frame.size.height;

        // Create timer loop
        m_pTimerLoop = new MacOSXTimerLoop( getIntf() );

        return true;
    }
}


OSGraphics *MacOSXFactory::createOSGraphics( int width, int height )
{
    return new MacOSXGraphics( getIntf(), width, height );
}


OSLoop *MacOSXFactory::getOSLoop()
{
    return MacOSXLoop::instance( getIntf() );
}


void MacOSXFactory::destroyOSLoop()
{
    MacOSXLoop::destroy( getIntf() );
}


void MacOSXFactory::minimize()
{
    @autoreleasepool {
        // Minimize all application windows
        for( NSWindow *window in [NSApp windows] )
        {
            if( [window isVisible] && ![window isMiniaturized] )
            {
                [window miniaturize:nil];
            }
        }
    }
}


void MacOSXFactory::restore()
{
    @autoreleasepool {
        // Restore all minimized windows
        for( NSWindow *window in [NSApp windows] )
        {
            if( [window isMiniaturized] )
            {
                [window deminiaturize:nil];
            }
        }
    }
}


void MacOSXFactory::addInTray()
{
    // macOS uses the Dock icon and menu bar status items
    // This is a simplified implementation
    msg_Dbg( getIntf(), "addInTray() called - macOS uses Dock" );
}


void MacOSXFactory::removeFromTray()
{
    msg_Dbg( getIntf(), "removeFromTray() called - macOS uses Dock" );
}


void MacOSXFactory::addInTaskBar()
{
    @autoreleasepool {
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    }
}


void MacOSXFactory::removeFromTaskBar()
{
    @autoreleasepool {
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    }
}


OSTimer *MacOSXFactory::createOSTimer( CmdGeneric &rCmd )
{
    return new MacOSXTimer( getIntf(), rCmd, m_pTimerLoop );
}


OSWindow *MacOSXFactory::createOSWindow( GenericWindow &rWindow,
                                          bool dragDrop, bool playOnDrop,
                                          OSWindow *pParent,
                                          GenericWindow::WindowType_t type )
{
    return new MacOSXWindow( getIntf(), rWindow, dragDrop, playOnDrop,
                              static_cast<MacOSXWindow*>(pParent), type );
}


OSTooltip *MacOSXFactory::createOSTooltip()
{
    return new MacOSXTooltip( getIntf() );
}


OSPopup *MacOSXFactory::createOSPopup()
{
    return new MacOSXPopup( getIntf() );
}


int MacOSXFactory::getScreenWidth() const
{
    return m_screenWidth;
}


int MacOSXFactory::getScreenHeight() const
{
    return m_screenHeight;
}


void MacOSXFactory::getMonitorInfo( OSWindow *pWindow,
                                     int* x, int* y,
                                     int* width, int* height ) const
{
    @autoreleasepool {
        MacOSXWindow *pMacWindow = static_cast<MacOSXWindow*>(pWindow);
        NSWindow *nsWindow = pMacWindow ? pMacWindow->getNSWindow() : nil;
        NSScreen *screen = nsWindow ? [nsWindow screen] : [NSScreen mainScreen];

        if( !screen )
            screen = [NSScreen mainScreen];

        NSRect frame = [screen frame];
        *x = (int)frame.origin.x;
        *y = (int)(m_screenHeight - frame.origin.y - frame.size.height);
        *width = (int)frame.size.width;
        *height = (int)frame.size.height;
    }
}


void MacOSXFactory::getMonitorInfo( int numScreen,
                                     int* x, int* y,
                                     int* width, int* height ) const
{
    @autoreleasepool {
        NSArray *screens = [NSScreen screens];
        NSScreen *screen;

        if( numScreen >= 0 && numScreen < (int)[screens count] )
            screen = [screens objectAtIndex:numScreen];
        else
            screen = [NSScreen mainScreen];

        NSRect frame = [screen frame];
        *x = (int)frame.origin.x;
        *y = (int)(m_screenHeight - frame.origin.y - frame.size.height);
        *width = (int)frame.size.width;
        *height = (int)frame.size.height;
    }
}


SkinsRect MacOSXFactory::getWorkArea() const
{
    @autoreleasepool {
        NSScreen *mainScreen = [NSScreen mainScreen];
        NSRect visibleFrame = [mainScreen visibleFrame];
        NSRect fullFrame = [mainScreen frame];

        // Convert from Cocoa coordinates (origin at bottom-left)
        // to skin coordinates (origin at top-left)
        int x = (int)visibleFrame.origin.x;
        int y = (int)(fullFrame.size.height - visibleFrame.origin.y - visibleFrame.size.height);
        int w = (int)visibleFrame.size.width;
        int h = (int)visibleFrame.size.height;

        return SkinsRect( x, y, x + w, y + h );
    }
}


void MacOSXFactory::getMousePos( int &rXPos, int &rYPos ) const
{
    @autoreleasepool {
        NSPoint mouseLocation = [NSEvent mouseLocation];
        rXPos = (int)mouseLocation.x;
        // Convert from Cocoa coordinates
        rYPos = (int)(m_screenHeight - mouseLocation.y);
    }
}


void MacOSXFactory::changeCursor( CursorType_t type ) const
{
    @autoreleasepool {
        NSCursor *cursor = nil;

        switch( type )
        {
            case kDefaultArrow:
                cursor = [NSCursor arrowCursor];
                break;
            case kResizeNS:
                cursor = [NSCursor resizeUpDownCursor];
                break;
            case kResizeWE:
                cursor = [NSCursor resizeLeftRightCursor];
                break;
            case kResizeNWSE:
                // macOS doesn't have a direct equivalent, use crosshair
                cursor = [NSCursor crosshairCursor];
                break;
            case kResizeNESW:
                cursor = [NSCursor crosshairCursor];
                break;
            case kNoCursor:
                [NSCursor hide];
                return;
            default:
                cursor = [NSCursor arrowCursor];
                break;
        }

        [NSCursor unhide];
        [cursor set];
    }
}


void MacOSXFactory::rmDir( const std::string &rPath )
{
    @autoreleasepool {
        NSFileManager *fileManager = [NSFileManager defaultManager];
        NSString *path = [NSString stringWithUTF8String:rPath.c_str()];
        NSError *error = nil;
        [fileManager removeItemAtPath:path error:&error];
        if( error )
        {
            msg_Warn( getIntf(), "Failed to remove directory: %s",
                     [[error localizedDescription] UTF8String] );
        }
    }
}
