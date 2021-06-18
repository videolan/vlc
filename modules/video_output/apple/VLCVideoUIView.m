/*****************************************************************************
 * VLCVideoUIView.m: iOS UIView vout window provider
 *****************************************************************************
 * Copyright (C) 2001-2017 VLC authors and VideoLAN
 * Copyright (C) 2020 Videolabs
 *
 * Authors: Pierre d'Herbemont <pdherbemont at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
 *          Rémi Denis-Courmont
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *          Eric Petit <titer@m0k.org>
 *          Alexandre Janniaux <ajanni@videolabs.io>
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
 *****************************************************************************/

/**
 * @file VLCVideoUIView.m
 * @brief UIView-based vlc_window_t provider
 *
 * This UIView window provider mostly handles resizing constraints from parent
 * views and provides event forwarding to VLC. It is usable for any kind of
 * subview and in particular can be used to implement a CAEAGLLayer in a
 * vlc_gl_t provider as well as a CAMetalLayer, or other CALayer based video
 * output in general.
 *
 * In particular, UI event will be forwarded to the core without the display
 * lock thanks to this implementation, but vout display implementation will
 * need to let the event pass through to this UIView.
 *
 * Note that this module is asynchronous with the usual VLC execution flow:
 * except during Open(), where a status code is needed and synchronization
 * must be done with the main thread, everything is forwarded to and
 * asynchronously executed by the main thread. In particular, the closing
 * of this module must be done asynchronously to not require the main thread
 * to run, and the hosting application will need to drain the main thread
 * dispatch queue. For iOS, it basically means nothing more than running the
 * usual UIApplicationMain.
 */

#import <UIKit/UIKit.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>
#import <QuartzCore/QuartzCore.h>
#import <dlfcn.h>

#ifdef HAVE_CONFIG_H
# import "config.h"
#endif

#import <vlc_common.h>
#import <vlc_plugin.h>
#import <vlc_dialog.h>
#import <vlc_mouse.h>
#import <vlc_window.h>

#import <assert.h>

@interface VLCVideoUIView : UIView {
    /* VLC window object, set to NULL under _mutex lock when closing. */
    vlc_window_t *_wnd;
    vlc_mutex_t _mutex;

    vlc_mutex_t _size_mutex;
    unsigned _requested_width;
    unsigned _requested_height;
    unsigned _width;
    unsigned _height;
    BOOL _resizing;

    /* Parent view defined by libvlc_media_player_set_nsobject. */
    UIView *_viewContainer;

    /* Window observer for mouse-like events. */
    UITapGestureRecognizer *_tapRecognizer;

    /* Window state */
    BOOL _enabled;
    dispatch_queue_t _eventq;
    atomic_bool _avstatEnabled;

    /* Constraints */
    NSArray<NSLayoutConstraint*> *_constraints;

    /* Other */
    CADisplayLink *_displayLink;
}

- (id)initWithWindow:(vlc_window_t *)wnd;
- (UIView *)fetchViewContainer;
- (void)detachFromParent;
- (void)tapRecognized:(UITapGestureRecognizer *)tapRecognizer;
- (void)enable;
- (void)disable;
- (void)applicationStateChanged:(NSNotification*)notification;
- (void)displayLinkUpdate:(CADisplayLink *)sender;
- (void)setAvstatEnabled:(bool)enabled;
@end

/*****************************************************************************
 * Our UIView object
 *****************************************************************************/
@implementation VLCVideoUIView

- (id)initWithWindow:(vlc_window_t *)wnd
{
    _wnd = wnd;
    _enabled = NO;

    vlc_mutex_init(&_size_mutex);
    _requested_height = 0;
    _requested_width = 0;
    _resizing = NO;
    atomic_init(&_avstatEnabled, false);

    UIView *superview = [self fetchViewContainer];
    if (superview == nil)
        return nil;

    self = [super initWithFrame:[superview frame]];
    if (!self)
        return nil;

    _eventq = dispatch_queue_create("vlc_eventq", DISPATCH_QUEUE_SERIAL);
    vlc_mutex_init(&_mutex);

    /* The window is controlled by the host application through the UIView
     * sizing mechanisms. */
    self.translatesAutoresizingMaskIntoConstraints = false;

    /* add tap gesture recognizer for DVD menus and stuff */
    if (var_InheritBool( wnd, "mouse-events" ) == true) {
        _tapRecognizer = [[UITapGestureRecognizer alloc]
            initWithTarget:self action:@selector(tapRecognized:)];
        _tapRecognizer.cancelsTouchesInView = NO;
    }

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationStateChanged:)
                                                 name:UIApplicationWillEnterForegroundNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationStateChanged:)
                                                 name:UIApplicationWillResignActiveNotification
                                               object:nil];
    _viewContainer = superview;

    CGSize size = superview.bounds.size;
    _width = size.width;
    _height = size.height;

    [self reportEvent:^{
        vlc_window_ReportSize(_wnd, size.width, size.height);
    }];

    _displayLink = [CADisplayLink displayLinkWithTarget:self
                                               selector:@selector(displayLinkUpdate:)];

    return self;
}

- (void)didMoveToSuperview
{
    if ([self superview] == nil)
        return;

    _constraints = @[
        [self.centerXAnchor constraintEqualToAnchor:[[self superview] centerXAnchor]],
        [self.centerYAnchor constraintEqualToAnchor:[[self superview] centerYAnchor]],
        [self.widthAnchor constraintEqualToAnchor:[[self superview] widthAnchor]],
        [self.heightAnchor constraintEqualToAnchor:[[self superview] heightAnchor]],
    ];
    [[self superview] addConstraints:_constraints];
    [NSLayoutConstraint activateConstraints:_constraints];
    [_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
}

- (void)willRemoveFromSuperview
{
    if ([self superview] == nil)
        return;

    [_displayLink removeFromRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
    [NSLayoutConstraint deactivateConstraints:_constraints];
    [[self superview] removeConstraints:_constraints];
    _constraints = nil;
}

- (UIView *)fetchViewContainer
{
    @try {
        /* get the object we will draw into */
        UIView *viewContainer = (__bridge UIView*)var_InheritAddress (_wnd, "drawable-nsobject");
        if (unlikely(viewContainer == nil)) {
            msg_Err(_wnd, "provided view container is nil");
            return nil;
        }

        if (unlikely(![viewContainer respondsToSelector:@selector(isKindOfClass:)])) {
            msg_Err(_wnd, "void pointer not an ObjC object");
            return nil;
        }

        if (![viewContainer isKindOfClass:[UIView class]]) {
            msg_Err(_wnd, "passed ObjC object not of class UIView");
            return nil;
        }

        return viewContainer;
    } @catch (NSException *exception) {
        msg_Err(_wnd, "Handling the view container failed due to an Obj-C exception (%s, %s", [exception.name UTF8String], [exception.reason UTF8String]);
        return nil;
    }
}

- (void)displayLinkUpdate:(CADisplayLink *)sender
{
    vlc_mutex_lock(&_mutex);
    if (_wnd == NULL)
    {
        vlc_mutex_unlock(&_mutex);
        return;
    }
    vlc_tick_t now = vlc_tick_now();
    CFTimeInterval current_ts = [sender timestamp];
    CFTimeInterval target_ts = 0.;
    if (@available(iOS 10, *))
        target_ts = [sender targetTimestamp];
    if (atomic_load(&_avstatEnabled))
        msg_Info(_wnd, "avstats: [RENDER][CADISPLAYLINK] ts=%" PRId64 " "
                 "prev_ts=%" PRId64 " target_ts=%" PRId64,
                 NS_FROM_VLC_TICK(now),
                 NS_FROM_VLC_TICK(vlc_tick_from_sec(current_ts)),
                 NS_FROM_VLC_TICK(vlc_tick_from_sec(target_ts)));

    vlc_mutex_unlock(&_mutex);
}

- (void)reportEventAsync:(void(^)())eventBlock
{
    dispatch_async(_eventq, eventBlock);
}

- (void)reportEvent:(void(^)())eventBlock
{
    CFStringRef mode = CFSTR("org.videolan.vlccore.window");
    CFRunLoopRef runloop = CFRunLoopGetCurrent();

    /* Callback hell right below, we need to execute the call
     * to CFRunLoopStop inside the CFRunLoopRunInMode context
     * since the CFRunLoopRunInMode might have already returned
     * otherwise, which means more callback wrapping. */
    CFRunLoopPerformBlock(runloop, mode, ^{
        /* Execute the event in a different thread, we don't
         * want to block the main CFRunLoop since the vout
         * display module typically needs it to Open(). */
        dispatch_async(_eventq, ^{
            (eventBlock)();
            CFRunLoopPerformBlock(runloop, mode, ^{
                /* Signal that we can end the ReportEvent call */
                CFRunLoopStop(runloop);
            });
            CFRunLoopWakeUp(runloop);
        });
    });
    /* Above and here, the CFRunLoopWakeUp call is necessary to
     * signal to the event loop that it will need to process the
     * blocks. They don't act like CFRunLoopSource so they won't
     * wake up the loop otherwise. */
    CFRunLoopWakeUp(runloop);
    for (;;)
    {
        /* We need a timeout here, otherwise the CFRunLoopInMode
         * call will check the events (if woken up), and since
         * we might have no event, it would return a timeout
         * result code, and loop again, creating a busy loop.
         * INFINITY is more than enough, and we'll interrupt
         * anyway. */
        CFRunLoopRunResult ret = CFRunLoopRunInMode(mode, INFINITY, YES);

        /* Usual CFRunLoop are typically checking result code
         * like kCFRunLoopRunFinished too, but we really want
         * to receive the Stop signal from above to leave the
         * loop in the correct state. */
        if (ret == kCFRunLoopRunStopped)
            break;
    }
}

- (void)detachFromParent
{
    /* We need to lock because we consider that _wnd might be destroyed
     * after this function returns, typically as it will be called in the
     * Close() operation which preceed the vlc_window_t destruction in
     * the core. */
    vlc_mutex_lock(&_mutex);
    /* The UIView must not be attached before releasing. Disable() is doing
     * exactly this asynchronously in the main thread so ensure it was called
     * here before detaching from the parent. */
    _wnd = NULL;
    vlc_mutex_unlock(&_mutex);
}

/**
 * Vout window operations implemention, which are expected to be run on
 * the main thread only. Core C wrappers below must typically use
 * dispatch_async with dispatch_get_main_queue() to call them.
 *
 * The addition of the UIView to the parent UIView might happen later
 * if there's no subview attached yet.
 */

- (void)enable
{
    assert(!_enabled);
    _enabled = YES;

    /**
     * Given -[UIView addGestureRecognizer:] can raise an exception if
     * tapRecognizer is nil and given tapRecognizer can be nil if
     * "mouse-events" var == false, then add tapRecognizer to the view only if
     * it's not nil
     */
    if (_tapRecognizer != nil) {
        [self addGestureRecognizer:_tapRecognizer];
    }
    [_viewContainer addSubview:self];
}

- (void)disable
{
    assert(_enabled);
    _enabled = NO;
    [self removeFromSuperview];

    [_tapRecognizer.view removeGestureRecognizer:_tapRecognizer];
}

/**
 * Window state tracking and reporting
 */

- (void)didMoveToWindow
{
    self.contentScaleFactor = self.window.screen.scale;
}

- (void)layoutSubviews
{
    [self reshape];
}

- (void)reshape
{
    assert([NSThread isMainThread]);

    CGSize viewSize = [self bounds].size;
    CGFloat scaleFactor = self.contentScaleFactor;

    /* We need to lock to ensure _wnd is still valid, see detachFromParent. */
    vlc_mutex_lock(&_size_mutex);
    _requested_width = viewSize.width * scaleFactor;
    _requested_height = viewSize.height * scaleFactor;
    if (_resizing == NO) {
        _resizing = YES;
        [self reportEventAsync:^{
            unsigned w, h;
            vlc_mutex_lock(&_size_mutex);
            while (_requested_width != _width ||
                   _requested_height != _height) {
                w = _requested_width;
                h = _requested_height;
                vlc_mutex_unlock(&_size_mutex);

                vlc_mutex_lock(&_mutex);
                if (_wnd != NULL)
                    vlc_window_ReportSize(_wnd, w, h);
                vlc_mutex_unlock(&_mutex);

                vlc_mutex_lock(&_size_mutex);
                _width = w;
                _height = h;
            }
            _resizing = NO;
            vlc_mutex_unlock(&_size_mutex);
        }];
    }
    vlc_mutex_unlock(&_size_mutex);
}

- (void)tapRecognized:(UITapGestureRecognizer *)tapRecognizer
{
    UIGestureRecognizerState state = [tapRecognizer state];
    CGPoint touchPoint = [tapRecognizer locationInView:self];
    CGFloat scaleFactor = self.contentScaleFactor;

    /* We need to lock to ensure _wnd is still valid, see detachFromParent. */
    vlc_mutex_lock(&_mutex);
    if (_wnd == NULL)
    {
        vlc_mutex_unlock(&_mutex);
        return;
    }

    vlc_window_ReportMouseMoved(_wnd,
    (int)touchPoint.x * scaleFactor, (int)touchPoint.y * scaleFactor);
    vlc_window_ReportMousePressed(_wnd, MOUSE_BUTTON_LEFT);
    vlc_window_ReportMouseReleased(_wnd, MOUSE_BUTTON_LEFT);
    vlc_mutex_unlock(&_mutex);
}

- (void)updateConstraints
{
    [super updateConstraints];
    [self reshape];
}

- (void)applicationStateChanged:(NSNotification *)notification
{
    vlc_mutex_lock(&_mutex);

    if (_wnd == NULL)
    {
        vlc_mutex_unlock(&_mutex);
        return;
    }

    [self reportEvent:^{
        if ([[notification name] isEqualToString:UIApplicationWillEnterForegroundNotification])
            vout_window_ReportVisibilityChanged(_wnd, VOUT_WINDOW_VISIBLE);
        else if ([[notification name] isEqualToString:UIApplicationWillResignActiveNotification])
            vout_window_ReportVisibilityChanged(_wnd, VOUT_WINDOW_NOT_VISIBLE);
    }];

    vlc_mutex_unlock(&_mutex);
}

/* Subview are expected to fill the whole frame so tell the compositor
 * that it doesn't have to bother with what's behind the window. */
- (BOOL)isOpaque
{
    return YES;
}

/* Prevent the subviews (which are renderers only) to get events so that
 * they can be dispatched from this vout_window module. */
- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)setAvstatEnabled:(BOOL)enabled
{
    atomic_store(&_avstatEnabled, enabled);
}
@end

/**
 * C core wrapper of the vout window operations for the ObjC module.
 */

static int Enable(vlc_window_t *wnd, const vlc_window_cfg_t *cfg)
{
    VLCVideoUIView *sys = (__bridge VLCVideoUIView *)wnd->sys;
    dispatch_async(dispatch_get_main_queue(), ^{
        [sys enable];
    });
    return VLC_SUCCESS;
}

static void Disable(vlc_window_t *wnd)
{
    VLCVideoUIView *sys = (__bridge VLCVideoUIView *)wnd->sys;
    dispatch_async(dispatch_get_main_queue(), ^{
        [sys disable];
    });
}

static int OnAvstatChanged(vlc_object_t *obj, const char *name, vlc_value_t oldv,
                           vlc_value_t newv, void *opaque)
{
    VLCVideoUIView *view = (__bridge VLCVideoUIView*)opaque;
    [view setAvstatEnabled: newv.b_bool ? YES : NO];
    return VLC_SUCCESS;
}

static void Close(vlc_window_t *wnd)
{
    VLCVideoUIView *sys = (__bridge_transfer VLCVideoUIView*)wnd->sys;

    var_DelCallback(wnd, "avstat", OnAvstatChanged, (__bridge void*)sys);

    /* We need to signal the asynchronous implementation that we have been
     * closed and cannot used _wnd anymore. */
    [sys detachFromParent];
}

static const struct vlc_window_operations window_ops =
{
    .enable = Enable,
    .disable = Disable,
    .destroy = Close,
};

static int Open(vlc_window_t *wnd)
{
    dispatch_sync(dispatch_get_main_queue(), ^{
        VLCVideoUIView *sys = [[VLCVideoUIView alloc] initWithWindow:wnd];
        wnd->sys = (__bridge_retained void*)sys;
    });

    if (wnd->sys == NULL)
    {
        msg_Err(wnd, "Creating UIView window provider failed");
        return VLC_EGENERIC;
    }

    var_Create(wnd, "avstat", VLC_VAR_BOOL | VLC_VAR_DOINHERIT);
    var_AddCallback(wnd, "avstat", OnAvstatChanged, wnd->sys);
    var_TriggerCallback(wnd, "avstat");

    wnd->type = VLC_WINDOW_TYPE_NSOBJECT;
    wnd->handle.nsobject = wnd->sys;
    wnd->ops = &window_ops;

    return VLC_SUCCESS;
}

vlc_module_begin ()
    set_shortname("UIView")
    set_description("iOS UIView vout window provider")
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout window", 300)
    set_callback(Open)

    add_shortcut("uiview", "ios")
vlc_module_end ()
