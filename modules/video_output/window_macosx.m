/**
 * @file window_macosx.m
 * @brief macOS Window and View output provider
 */

/* Copyright (C) 2020 VLC authors and VideoLAN
 *
 * Authors: Marvin Scholz <epirat 07 at gmail dot com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#import <Cocoa/Cocoa.h>

#include <vlc_common.h>
#include <vlc_threads.h>
#include <vlc_plugin.h>
#include <vlc_window.h>
#include <vlc_mouse.h>

#define VLC_ASSERT_MAINTHREAD NSAssert([[NSThread currentThread] isMainThread], \
    @"Must be called from the main thread!")

/**
 * Style mask for a decorated window
 */
static const NSWindowStyleMask decoratedWindowStyleMask =
    NSWindowStyleMaskTitled
    | NSWindowStyleMaskClosable
    | NSWindowStyleMaskMiniaturizable
    | NSWindowStyleMaskResizable;

/**
 * Style mask for a non-decorated window
 */
static const NSWindowStyleMask undecoratedWindowStyleMask =
    NSWindowStyleMaskBorderless
    | NSWindowStyleMaskResizable;


#pragma mark -
#pragma mark Obj-C Interfaces

NS_ASSUME_NONNULL_BEGIN

/**
 * Window delegate and Core interactions class
 *
 * Acts as delegate for the Window and handles communication
 * of the window state from/to the core. This is not done directly
 * in the window controller, as the module on VLCs side might be
 * already gone when the window and its controller is still around
 * so we need a separate object that we can invalidate as soon
 * as the VLC module object is gone.
 */
@interface VLCVideoWindowModuleDelegate : NSObject {
    @private
    // VLC window object, only use it on the eventQueue
    vlc_mutex_t        _lock;
    unsigned           _requested_width;
    unsigned           _requested_height;
    unsigned           _width;
    unsigned           _height;
    BOOL               _resizing;
    vlc_window_t*      vlc_vout_window;
    dispatch_queue_t   eventQueue;

    BOOL               _isViewSet;
}

- (instancetype)initWithVLCVoutWindow:(vlc_window_t *)vout_window;

/// Reports that the window is fullscreen now
- (void)reportFullscreen;

/// Reports that the previously fullscreen window is no longer fullscreen
- (void)reportWindowed;

/// Reports the new window size in pixels
- (void)reportSizeChanged:(NSSize)newSize;

///
- (void)reportMousePressed:(int)button;

///
- (void)reportMouseReleased:(int)button;

///
- (void)reportMouseMoved:(NSPoint)location;

/// Reports that the window was closed
- (void)reportClose;

@end

/**
 * Video output window class
 *
 * Custom NSWindow subclass, mostly to overwrite that the window
 * can become the key window even if its using the borderless
 * (undecorated) style.
 */
@interface VLCVideoWindow : NSWindow
@end


/**
 * Video view class
 *
 * Custom NSWindow subclass, used to track resizes so that
 * the core can be notified about the new sizes in a timely manner.
 */
@interface VLCVideoWindowContentView : NSView {
    @private
    __weak VLCVideoWindowModuleDelegate *_moduleDelegate;
}

- (instancetype)initWithModuleDelegate:(VLCVideoWindowModuleDelegate *)delegate;
@end

/**
 * Video output window controller class
 *
 * Controller for the VLC standalone video window (independent of the interface)
 *
 * Implements all interactions between the display module and the NSWindow
 * class, except for resizes (which is handled by VLCVideoWindowContentView).
 */
@interface VLCVideoStandaloneWindowController : NSWindowController <NSWindowDelegate> {
@private
    __weak VLCVideoWindowModuleDelegate *_moduleDelegate;
}

- (instancetype)initWithModuleDelegate:(VLCVideoWindowModuleDelegate *)delegate;
- (void)showWindowWithConfig:(const vlc_window_cfg_t *restrict)config;

/* Methods called by the callbacks to change properties of the Window */
- (void)setWindowDecorated:(BOOL)decorated;
- (void)setWindowFullscreen:(BOOL)fullscreen;

@end


#pragma mark -
#pragma mark Obj-C Implementations

@implementation VLCVideoWindowModuleDelegate : NSObject

- (instancetype)initWithVLCVoutWindow:(vlc_window_t *)vout_window
{
    NSAssert(vout_window != NULL,
             @"VLCVideoWindowDelegate must be initialized with a valid vout_window");

    self = [super init];
    if (self) {
        eventQueue = dispatch_queue_create("org.videolan.vlc.vout", DISPATCH_QUEUE_SERIAL);
        _requested_width = 0;
        _requested_height = 0;
        _width = 0;
        _height = 0;
        _resizing = NO;
        vlc_mutex_init(&_lock);
        vlc_vout_window = vout_window;
    }

    return self;
}

- (void)enqueueEventBlock:(void (^)(void))block
{
    dispatch_async(eventQueue, block);
}

- (void)setViewObject:(id)view
{
    NSAssert(_isViewSet == NO,
             @"VLCVideoWindowDelegate's viewObject must only bet set once");
    vlc_vout_window->type = VLC_WINDOW_TYPE_NSOBJECT;
    vlc_vout_window->handle.nsobject = (__bridge void*)view;
}

- (void)reportFullscreen
{
    [self enqueueEventBlock:^void (void) {
        vlc_window_ReportFullscreen(vlc_vout_window, NULL);
    }];
}

- (void)reportWindowed
{
    [self enqueueEventBlock:^void (void) {
        vlc_window_ReportWindowed(vlc_vout_window);
    }];
}

- (void)reportSizeChanged:(NSSize)newSize
{
    vlc_mutex_lock(&_lock);
    _requested_width = (unsigned int)newSize.width;
    _requested_height = (unsigned int)newSize.height;
    if (_resizing == NO) {
        _resizing = YES;
        [self enqueueEventBlock:^void (void) {
            unsigned w, h;
            vlc_mutex_lock(&_lock);
            while (_requested_width != _width ||
                   _requested_height != _height) {
                w = _requested_width;
	        h = _requested_height;
                vlc_mutex_unlock(&_lock);

                vlc_window_ReportSize(vlc_vout_window, w, h);

                vlc_mutex_lock(&_lock);
		_width = w;
                _height = h;
            }
            _resizing = NO;
            vlc_mutex_unlock(&_lock);
        }];
    }
    vlc_mutex_unlock(&_lock);
}

- (void)reportMousePressed:(int)button
{
    [self enqueueEventBlock:^void (void) {
        vlc_window_ReportMousePressed(vlc_vout_window, button);
    }];
}

- (void)reportMouseReleased:(int)button
{
    [self enqueueEventBlock:^void (void) {
        vlc_window_ReportMouseReleased(vlc_vout_window, button);
    }];
}

- (void)reportMouseMoved:(NSPoint)location {
    [self enqueueEventBlock:^void (void) {
        vlc_window_ReportMouseMoved(vlc_vout_window, location.x, location.y);
    }];
}

- (void)reportClose
{
    [self enqueueEventBlock:^void (void) {
        vlc_window_ReportClose(vlc_vout_window);
    }];
}

- (void)dealloc
{
    dispatch_sync(eventQueue, ^void (void) {
        self->vlc_vout_window = NULL;
    });
}

@end


@implementation VLCVideoStandaloneWindowController

/**
 * Initializes the window controller with the given module delegate
 */
- (instancetype)initWithModuleDelegate:(VLCVideoWindowModuleDelegate *)delegate;
{
    VLC_ASSERT_MAINTHREAD;

    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSZeroRect
                                                   styleMask:decoratedWindowStyleMask
                                                     backing:NSBackingStoreBuffered
                                                       defer:YES];

    self = [super initWithWindow:window];
    if (self) {
        // Set the initial vout title
        [window setTitle:[NSString stringWithUTF8String:VOUT_TITLE " (VLC Video Output)"]];

        // The content always changes during live resize
        [window setPreservesContentDuringLiveResize:NO];

        // Do not release on close (we might want to re-open the window later)
        [window setReleasedWhenClosed:NO];

        // Hint that the window should become a primary fullscreen window
        [window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];

        // Create and set custom content view for the window
        VLCVideoWindowContentView *view =
            [[VLCVideoWindowContentView alloc] initWithModuleDelegate:delegate];

        if (view == nil)
            return nil;

        [window setContentView:view];

        [window setDelegate:self];

        // Position the window in the center
        [window center];

        [self setWindowFrameAutosaveName:@"VLCVideoStandaloneWindow"];

        _moduleDelegate = delegate;

        [_moduleDelegate setViewObject:view];
    }

    return self;
}

/**
 * Applies the given config to the window and shows it.
 */
- (void)showWindowWithConfig:(const vlc_window_cfg_t *restrict)config
{
    VLC_ASSERT_MAINTHREAD;

    // Convert from backing to window coordinates
    NSRect backingRect = NSMakeRect(0, 0, config->width, config->height);
    NSRect windowRect = [self.window convertRectFromBacking:backingRect];
    [self.window setContentSize:windowRect.size];

    // Set decoration
    [self setWindowDecorated:config->is_decorated];

    // This should always be called last, to ensure we only show the
    // window once its fully configured. Else there could be visible
    // changes or animations when the config is applied.
    [self showWindow:nil];
    [self.window makeKeyAndOrderFront:nil];
}

- (BOOL)windowShouldClose:(NSWindow *)sender
{
    [_moduleDelegate reportClose];
    return YES;
}

#pragma mark Helper methods

- (BOOL)isWindowFullscreen
{
    return ((self.window.styleMask & NSFullScreenWindowMask) == NSFullScreenWindowMask);
}

#pragma mark Module interactions

- (void)setWindowDecorated:(BOOL)decorated
{
    NSWindowStyleMask mask =
        (decorated) ? decoratedWindowStyleMask : undecoratedWindowStyleMask;

    [self.window setStyleMask:mask];
}

- (void)setWindowFullscreen:(BOOL)fullscreen
{
    if (!!fullscreen == !![self isWindowFullscreen]) {
        // Nothing to do, just report the state to core
        if (fullscreen) {
            [_moduleDelegate reportFullscreen];
        } else {
            [_moduleDelegate reportWindowed];
        }
        return;
    }

    [self.window toggleFullScreen:nil];
}

#pragma mark Window delegate

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
    [_moduleDelegate reportFullscreen];
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
    [_moduleDelegate reportWindowed];
}

@end



@implementation VLCVideoWindowContentView {
    NSTrackingArea *_trackingArea;
}

- (instancetype)initWithModuleDelegate:(VLCVideoWindowModuleDelegate *)delegate;
{
    self = [super init];
    if (self) {
        NSAssert(delegate != nil, @"Invalid VLCVideoWindowModuleDelegate passed.");
        self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        _moduleDelegate = delegate;
    }
    return self;
}

- (void)layout {
    [self reportBackingSize];
    [super layout];
}

- (void)drawRect:(NSRect)dirtyRect
{
    [[NSColor blackColor] setFill];
    NSRectFill(dirtyRect);
}

/**
 * Report the view size in the backing size dimensions to VLC core
 */
- (void)reportBackingSize
{
    NSRect bounds = [self convertRectToBacking:self.bounds];
    [_moduleDelegate reportSizeChanged:bounds.size];
}

/**
 * Handle view size changes
 */
- (void)resizeSubviewsWithOldSize:(NSSize)oldSize
{
    [self reportBackingSize];
    [super resizeSubviewsWithOldSize:oldSize];
}

/**
 * Handle view backing property changes
 */
- (void)viewDidChangeBackingProperties
{
    // When the view backing size changes, it means the view effectively
    // resizes from VLC core perspective, as it operates on the real
    // backing dimensions, not the view point size.
    [self reportBackingSize];
    [super viewDidChangeBackingProperties];
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)mouseDownCanMoveWindow
{
    return YES;
}


#pragma mark View mouse events

- (void)updateTrackingAreas {
    if (_trackingArea) {
        [self removeTrackingArea:_trackingArea];
    }

    _trackingArea = [[NSTrackingArea alloc]
        initWithRect:self.bounds
        options: NSTrackingMouseMoved | NSTrackingActiveAlways
        owner:self
        userInfo:nil
    ];

    [self addTrackingArea:_trackingArea];
}

/* Left mouse button down */
- (void)mouseDown:(NSEvent *)event
{
    @synchronized(self) {
        if (event.type == NSLeftMouseDown &&
            !(event.modifierFlags & NSControlKeyMask) &&
            event.clickCount == 1) {
            [_moduleDelegate reportMousePressed:MOUSE_BUTTON_LEFT];
        }
    }

    [super mouseDown:event];
}

/* Left mouse button up */
- (void)mouseUp:(NSEvent *)event
{
    @synchronized(self) {
        if (event.type == NSLeftMouseUp) {
            [_moduleDelegate reportMouseReleased:MOUSE_BUTTON_LEFT];
        }
    }

    [super mouseUp:event];
}

/* Middle mouse button down */
- (void)otherMouseDown:(NSEvent *)event
{
    @synchronized(self) {
        [_moduleDelegate reportMousePressed:MOUSE_BUTTON_CENTER];
    }

    [super otherMouseDown:event];
}

/* Middle mouse button up */
- (void)otherMouseUp:(NSEvent *)event
{
    @synchronized(self) {
        [_moduleDelegate reportMouseReleased:MOUSE_BUTTON_CENTER];
    }

    [super otherMouseUp:event];
}

- (void)mouseMovedInternal:(NSEvent *)event
{
    NSPoint pointInView = 
        [self convertPoint:event.locationInWindow fromView:nil];
    if ([self mouse:pointInView inRect:self.bounds]) {
        // Invert Y coordinates
        CGPoint pointInWindow = 
            CGPointMake(pointInView.x, self.bounds.size.height - pointInView.y);
        NSPoint pointInBacking = [self convertPointToBacking:pointInWindow];
        [_moduleDelegate reportMouseMoved:pointInBacking];
    }
}

/* Mouse moved */
- (void)mouseMoved:(NSEvent *)event
{
    [self mouseMovedInternal:event];
    [super mouseMoved:event];
}

/* Mouse moved while clicked */
- (void)mouseDragged:(NSEvent *)event
{
    [self mouseMovedInternal:event];
    [super mouseDragged:event];
}

/* Mouse moved while center-clicked */
- (void)otherMouseDragged:(NSEvent *)event
{
    [self mouseMovedInternal:event];
    [super otherMouseDragged:event];
}

/* Mouse moved while right-clicked */
- (void)rightMouseDragged:(NSEvent *)event
{
    [self mouseMovedInternal:event];
    [super rightMouseDragged:event];
}

@end

@implementation VLCVideoWindow

- (BOOL)canBecomeKeyWindow
{
    // A window with NSWindowStyleMaskBorderless can usually not become key
    // window, unless we return YES here.
    return YES;
}

@end

NS_ASSUME_NONNULL_END


#pragma mark -
#pragma mark VLC module

@interface VLCVoutWindow : NSObject
    @property (nonatomic) VLCVideoStandaloneWindowController *windowController;
    @property (nonatomic) VLCVideoWindowContentView *contentView;
    @property (nonatomic) VLCVideoWindowModuleDelegate *delegate;
@end

@implementation VLCVoutWindow
@end

/* Enable Window
 */
static int WindowEnable(vlc_window_t *wnd, const vlc_window_cfg_t *restrict cfg)
{
    VLCVoutWindow *sys = (__bridge VLCVoutWindow*)wnd->sys;

    @autoreleasepool {
        __weak VLCVideoStandaloneWindowController *weakWc = sys.windowController;
        dispatch_sync(dispatch_get_main_queue(), ^{
            [weakWc showWindowWithConfig:cfg];
        });
    }

    return VLC_SUCCESS;
}

/* Request to close the window */
static void WindowDisable(vlc_window_t *wnd)
{
    VLCVoutWindow *sys = (__bridge VLCVoutWindow*)wnd->sys;

    @autoreleasepool {
        __weak VLCVideoStandaloneWindowController *weakWc = sys.windowController;
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakWc close];
        });
    }
}

/* Request to resize the window */
static void WindowResize(vlc_window_t *wnd, unsigned width, unsigned height)
{
    VLCVoutWindow *sys = (__bridge VLCVoutWindow*)wnd->sys;

    @autoreleasepool {
        __weak VLCVideoStandaloneWindowController *weakWc = sys.windowController;
        dispatch_async(dispatch_get_main_queue(), ^{
            VLCVideoStandaloneWindowController *wc = weakWc;
            // Convert from backing to window coordinates
            NSRect backingRect = NSMakeRect(0, 0, width, height);
            NSRect windowRect = [wc.window convertRectFromBacking:backingRect];
            [wc.window setContentSize:windowRect.size];

            // Size is reported by resizeSubviewsWithOldSize:, do not
            // report it here, else it would get reported twice.
        });
    }
}

/* Request to enter fullscreen */
static void WindowSetFullscreen(vlc_window_t *wnd, const char *idstr)
{
    VLCVoutWindow *sys = (__bridge VLCVoutWindow*)wnd->sys;

    @autoreleasepool {
        __weak VLCVideoStandaloneWindowController *weakWc = sys.windowController;
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakWc setWindowFullscreen:YES];
        });
    }
}

/* Request to exit fullscreen */
static void WindowUnsetFullscreen(vlc_window_t *wnd)
{
    VLCVoutWindow *sys = (__bridge VLCVoutWindow*)wnd->sys;

    @autoreleasepool {
        __weak VLCVideoStandaloneWindowController *weakWc = sys.windowController;
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakWc setWindowFullscreen:NO];
        });
    }
}

static void WindowSetTitle(struct vlc_window *wnd, const char *title)
{
    VLCVoutWindow *sys = (__bridge VLCVoutWindow*)wnd->sys;
    @autoreleasepool {
        __weak VLCVideoStandaloneWindowController *weakWc = sys.windowController;
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakWc.window setTitle:[NSString stringWithUTF8String:title]];
        });
    }
}

/*
 * Module destruction
 */
void Close(vlc_window_t *wnd)
{
    VLCVoutWindow *sys = (__bridge_transfer VLCVoutWindow*)wnd->sys;
    VLC_UNUSED(sys);
}

/*
 * Callbacks
 */
static const struct vlc_window_operations ops = {
    .enable = WindowEnable,
    .disable = WindowDisable,
    .resize = WindowResize,
    .set_state = NULL,
    .unset_fullscreen = WindowUnsetFullscreen,
    .set_fullscreen = WindowSetFullscreen,
    .set_title = WindowSetTitle,
    .destroy = Close,
};

/*
 * Module initialization
 */
int Open(vlc_window_t *wnd)
{
    @autoreleasepool {
        msg_Info(wnd, "using the macOS new video output window module");

        // Check if there is an NSApplication, needed for the connection
        // to the Window Server so we can use NSWindows, NSViews, etc.
        if (NSApp == nil) {
            msg_Err(wnd, "cannot create video output window without NSApplication");
            return VLC_EGENERIC;
        }

        VLCVoutWindow *sys = [VLCVoutWindow new];
        if (unlikely(sys == nil))
            return VLC_ENOMEM;

        VLCVideoWindowModuleDelegate *moduleDelegate;
        moduleDelegate = [[VLCVideoWindowModuleDelegate alloc] initWithVLCVoutWindow:wnd];
        if (unlikely(moduleDelegate == nil))
            return VLC_ENOMEM;
        sys.delegate = moduleDelegate;

        __block VLCVideoStandaloneWindowController *windowController;
        if (CFRunLoopGetCurrent() != CFRunLoopGetMain())
            dispatch_sync(dispatch_get_main_queue(), ^{
                windowController = [[VLCVideoStandaloneWindowController alloc] initWithModuleDelegate:moduleDelegate];
            });
        else
            windowController = [[VLCVideoStandaloneWindowController alloc] initWithModuleDelegate:moduleDelegate];

        if (unlikely(windowController == nil))
            return VLC_ENOMEM;
        sys.windowController = windowController;

        wnd->ops = &ops;
        wnd->sys = (__bridge_retained void*)sys;

        return VLC_SUCCESS;
    }
}

static int EmbedOpen(vlc_window_t *wnd)
{
    NSView *drawable = (__bridge NSView*)
        var_InheritAddress(wnd, "drawable-nsobject");
    if (drawable == nil) {
        msg_Err(wnd, "cannot create video output window without NSApplication");
        return VLC_EGENERIC;
    }

    msg_Info(wnd, "using the macOS embed window module");

    // Check if there is an NSApplication, needed for the connection
    // to the Window Server so we can use NSWindows, NSViews, etc.
    if (NSApp == nil) {
        msg_Err(wnd, "cannot create video output window without NSApplication");
        return VLC_EGENERIC;
    }

    @autoreleasepool {
        VLCVoutWindow *sys = [VLCVoutWindow new];
        if (unlikely(sys == nil))
            return VLC_ENOMEM;

        VLCVideoWindowModuleDelegate *moduleDelegate;
        moduleDelegate = [[VLCVideoWindowModuleDelegate alloc] initWithVLCVoutWindow:wnd];
        if (unlikely(moduleDelegate == nil))
            return VLC_ENOMEM;
        sys.delegate = moduleDelegate;

        __block VLCVideoWindowContentView *contentView;
        dispatch_sync(dispatch_get_main_queue(), ^{
            contentView = [[VLCVideoWindowContentView alloc] initWithModuleDelegate:moduleDelegate];
            contentView.frame = drawable.frame;
            [drawable addSubview:contentView];
        });
        if (unlikely(contentView == nil))
            return VLC_ENOMEM;
        sys.contentView = contentView;

        [moduleDelegate setViewObject:contentView];

        wnd->ops = &ops;
        wnd->sys = (__bridge_retained void*)sys;

        return VLC_SUCCESS;
    }

    return VLC_SUCCESS;
}

/*
 * Module declaration
 */
vlc_module_begin()
    set_description("macOS Video Output Window")
    set_capability("vout window", 1)
    set_callback(Open)
    add_shortcut("macos-window")

    add_submodule()
    set_shortname(N_("NSObject Drawable"))
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout window", 10)
    set_callback(EmbedOpen)
    add_shortcut("macos-embed")
vlc_module_end()
