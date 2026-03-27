/*****************************************************************************
 * macosx_window.mm
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

#include "macosx_window.hpp"
#include "macosx_dragdrop.hpp"
#include "macosx_factory.hpp"
#include "macosx_graphics.hpp"
#include "../src/os_factory.hpp"
#include "../events/evt_focus.hpp"

// Forward declaration of the window delegate protocol handler
@class VLCSkinsWindowDelegate;

/// Custom NSWindow subclass for VLC Skins
@interface VLCSkinsWindow : NSWindow
{
    MacOSXWindow *m_pOwner;
    VLCSkinsWindowDelegate *m_delegate;
    const MacOSXGraphics *m_pHitTestGraphics;
}
- (instancetype)initWithOwner:(MacOSXWindow *)owner
                  contentRect:(NSRect)contentRect
                    styleMask:(NSWindowStyleMask)style
                      backing:(NSBackingStoreType)backingStoreType
                        defer:(BOOL)flag;
- (MacOSXWindow *)owner;
- (void)setHitTestGraphics:(const MacOSXGraphics *)pGraphics;
@end

@interface VLCSkinsWindowDelegate : NSObject <NSWindowDelegate>
{
    MacOSXWindow *m_pOwner;
}
- (instancetype)initWithOwner:(MacOSXWindow *)owner;
@end

@interface VLCSkinsContentView : NSImageView
{
    const MacOSXGraphics *m_pGraphics;
    NSTrackingArea *m_trackingArea;
}
- (void)setHitTestGraphics:(const MacOSXGraphics *)pGraphics;
@end

@implementation VLCSkinsContentView

- (void)setHitTestGraphics:(const MacOSXGraphics *)pGraphics
{
    m_pGraphics = pGraphics;
}

- (void)updateTrackingAreas
{
    [super updateTrackingAreas];
    if( m_trackingArea )
        [self removeTrackingArea:m_trackingArea];

    m_trackingArea = [[NSTrackingArea alloc]
        initWithRect:[self bounds]
             options:NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
                     NSTrackingActiveAlways | NSTrackingInVisibleRect
               owner:[self window]
            userInfo:nil];
    [self addTrackingArea:m_trackingArea];
}

- (NSView *)hitTest:(NSPoint)point
{
    if( m_pGraphics )
    {
        NSPoint local = [self convertPoint:point fromView:[self superview]];
        int x = local.x;
        int y = [self bounds].size.height - local.y;

        if( !m_pGraphics->hit( x, y ) )
            return nil;
    }
    return [super hitTest:point];
}

@end

@implementation VLCSkinsWindowDelegate

- (instancetype)initWithOwner:(MacOSXWindow *)owner
{
    self = [super init];
    if( self )
    {
        m_pOwner = owner;
    }
    return self;
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    if( m_pOwner )
    {
        EvtFocus evt( m_pOwner->getIntf(), true );
        m_pOwner->getGenericWindow().processEvent( evt );
    }
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    if( m_pOwner )
    {
        EvtFocus evt( m_pOwner->getIntf(), false );
        m_pOwner->getGenericWindow().processEvent( evt );
    }
}

- (BOOL)windowShouldClose:(NSWindow *)sender
{
    // Let the skins system handle closing
    return NO;
}

@end

@implementation VLCSkinsWindow

- (instancetype)initWithOwner:(MacOSXWindow *)owner
                  contentRect:(NSRect)contentRect
                    styleMask:(NSWindowStyleMask)style
                      backing:(NSBackingStoreType)backingStoreType
                        defer:(BOOL)flag
{
    self = [super initWithContentRect:contentRect
                            styleMask:style
                              backing:backingStoreType
                                defer:flag];
    if( self )
    {
        m_pOwner = owner;
        m_delegate = [[VLCSkinsWindowDelegate alloc] initWithOwner:owner];
        [self setDelegate:m_delegate];

        // Configure for skinned appearance
        [self setOpaque:NO];
        [self setBackgroundColor:[NSColor clearColor]];
        [self setHasShadow:YES];
        [self setAcceptsMouseMovedEvents:YES];

        // Create content view
        NSRect bounds = NSMakeRect( 0, 0, contentRect.size.width, contentRect.size.height );
        VLCSkinsContentView *contentView = [[VLCSkinsContentView alloc] initWithFrame:bounds];
        [contentView setImageScaling:NSImageScaleNone];
        [self setContentView:contentView];
        m_pHitTestGraphics = NULL;
    }
    return self;
}

- (MacOSXWindow *)owner
{
    return m_pOwner;
}

- (void)setHitTestGraphics:(const MacOSXGraphics *)pGraphics
{
    m_pHitTestGraphics = pGraphics;
    VLCSkinsContentView *view = (VLCSkinsContentView *)[self contentView];
    [view setHitTestGraphics:pGraphics];
}

- (BOOL)canBecomeKeyWindow
{
    return YES;
}

- (BOOL)canBecomeMainWindow
{
    return YES;
}

- (void)sendEvent:(NSEvent *)event
{
    // Forward events to the skins event loop
    [super sendEvent:event];
}

@end


MacOSXWindow::MacOSXWindow( intf_thread_t *pIntf, GenericWindow &rWindow,
                            bool dragDrop, bool playOnDrop,
                            MacOSXWindow *pParentWindow,
                            GenericWindow::WindowType_t type ):
    OSWindow( pIntf ), m_rWindow( rWindow ), m_pWindow( nil ),
    m_pParent( pParentWindow ), m_type( type ), m_pDropTarget( NULL )
{
    @autoreleasepool {
        // Determine window style
        NSWindowStyleMask styleMask;
        NSRect contentRect = NSMakeRect( 0, 0, 100, 100 );

        switch( type )
        {
            case GenericWindow::FullscreenWindow:
                styleMask = NSWindowStyleMaskBorderless;
                contentRect = [mainScreen frame];
                break;

            case GenericWindow::ToolbarWindow:
            case GenericWindow::TopWindow:
            default:
                styleMask = NSWindowStyleMaskBorderless;
                break;
        }

        // Create the window
        m_pWindow = [[VLCSkinsWindow alloc]
            initWithOwner:this
              contentRect:contentRect
                styleMask:styleMask
                  backing:NSBackingStoreBuffered
                    defer:YES];

        if( m_pWindow )
        {
            // Register with factory
            MacOSXFactory *pFactory = static_cast<MacOSXFactory*>(
                OSFactory::instance( pIntf ) );
            if( pFactory )
            {
                pFactory->m_windowMap[(__bridge void *)m_pWindow] = &rWindow;
            }

            // Set up drag & drop if requested
            if( dragDrop )
            {
                m_pDropTarget = new MacOSXDragDrop( pIntf, m_pWindow, playOnDrop, &rWindow );
            }

            // Set window level based on type
            if( type == GenericWindow::FullscreenWindow )
            {
                [m_pWindow setLevel:NSScreenSaverWindowLevel];
            }
            else if( type == GenericWindow::ToolbarWindow )
            {
                [m_pWindow setLevel:NSFloatingWindowLevel];
            }

            // Set parent window
            if( m_pParent && m_pParent->getNSWindow() )
            {
                [m_pParent->getNSWindow() addChildWindow:m_pWindow
                                                 ordered:NSWindowAbove];
            }
        }
    }
}


MacOSXWindow::~MacOSXWindow()
{
    @autoreleasepool {
        // Unregister from factory
        MacOSXFactory *pFactory = static_cast<MacOSXFactory*>(
            OSFactory::instance( getIntf() ) );
        if( pFactory && m_pWindow )
        {
            pFactory->m_windowMap.erase( (__bridge void *)m_pWindow );
        }

        delete m_pDropTarget;

        if( m_pWindow )
        {
            if( m_pParent && m_pParent->getNSWindow() )
            {
                [m_pParent->getNSWindow() removeChildWindow:m_pWindow];
            }
            [m_pWindow close];
            m_pWindow = nil;
        }
    }
}


void MacOSXWindow::show() const
{
    @autoreleasepool {
        if( m_pWindow )
        {
            [m_pWindow makeKeyAndOrderFront:nil];
        }
    }
}


void MacOSXWindow::hide() const
{
    @autoreleasepool {
        if( m_pWindow )
        {
            [m_pWindow orderOut:nil];
        }
    }
}


void MacOSXWindow::moveResize( int left, int top, int width, int height ) const
{
    @autoreleasepool {
        if( m_pWindow )
        {
            // Convert from skins coordinates (origin top-left)
            // to Cocoa coordinates (origin bottom-left)
            NSRect frame = NSMakeRect( left, [NSScreen mainScreen].frame.size.height - top - height,
                                       width, height );
            [m_pWindow setFrame:frame display:YES];
        }
    }
}


void MacOSXWindow::raise() const
{
    @autoreleasepool {
        if( m_pWindow )
        {
            [m_pWindow orderFront:nil];
        }
    }
}


void MacOSXWindow::setOpacity( uint8_t value ) const
{
    @autoreleasepool {
        if( m_pWindow )
        {
            CGFloat alpha = (CGFloat)value / 255.0;
            [m_pWindow setAlphaValue:alpha];
        }
    }
}


void MacOSXWindow::toggleOnTop( bool onTop ) const
{
    @autoreleasepool {
        if( m_pWindow )
        {
            if( onTop )
            {
                [m_pWindow setLevel:NSFloatingWindowLevel];
            }
            else
            {
                [m_pWindow setLevel:NSNormalWindowLevel];
            }
        }
    }
}


void MacOSXWindow::setOSHandle( vlc_window_t *pWnd ) const
{
    if( pWnd && m_pWindow )
    {
        pWnd->type = VLC_WINDOW_TYPE_NSOBJECT;
        pWnd->info.has_double_click = true;
        pWnd->handle.nsobject = (__bridge void *)[m_pWindow contentView];
    }
}


void MacOSXWindow::reparent( OSWindow *pParent, int x, int y, int w, int h )
{
    @autoreleasepool {
        MacOSXWindow *pMacParent = static_cast<MacOSXWindow*>(pParent);

        // Remove from old parent
        if( m_pParent && m_pParent->getNSWindow() && m_pWindow )
        {
            [m_pParent->getNSWindow() removeChildWindow:m_pWindow];
        }

        m_pParent = pMacParent;

        // Add to new parent
        if( m_pParent && m_pParent->getNSWindow() && m_pWindow )
        {
            [m_pParent->getNSWindow() addChildWindow:m_pWindow
                                             ordered:NSWindowAbove];
        }

        moveResize( x, y, w, h );
    }
}


bool MacOSXWindow::invalidateRect( int x, int y, int w, int h ) const
{
    @autoreleasepool {
        if( m_pWindow )
        {
            NSView *contentView = [m_pWindow contentView];
            if( contentView )
            {
                NSRect rect = NSMakeRect( x, [contentView bounds].size.height - y - h, w, h );
                [contentView setNeedsDisplayInRect:rect];
                return true;
            }
        }
        return false;
    }
}
