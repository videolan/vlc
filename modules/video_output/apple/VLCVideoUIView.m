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
 * @brief UIView-based vout_window_t provider
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
#import <vlc_vout_window.h>

@interface VLCVideoUIView : UIView {
    /* VLC window object, set to NULL under _mutex lock when closing. */
    vout_window_t *_wnd;
    vlc_mutex_t _mutex;

    /* Parent view defined by libvlc_media_player_set_nsobject. */
    UIView *_viewContainer;

    /* Window observer for mouse-like events. */
    UITapGestureRecognizer *_tapRecognizer;

    /* Window state */
    BOOL _enabled;
    int _subviews;
}

- (id)initWithWindow:(vout_window_t *)wnd;
- (BOOL)fetchViewContainer;
- (void)detachFromParent;
- (void)tapRecognized:(UITapGestureRecognizer *)tapRecognizer;
- (void)enable;
- (void)disable;
@end

/*****************************************************************************
 * Our UIView object
 *****************************************************************************/
@implementation VLCVideoUIView

- (id)initWithWindow:(vout_window_t *)wnd
{
    _wnd = wnd;
    _enabled = NO;
    _subviews = 0;

    self = [super initWithFrame:CGRectMake(0., 0., 320., 240.)];
    if (!self)
        return nil;

    vlc_mutex_init(&_mutex);

    /* The window is controlled by the host application through the UIView
     * sizing mechanisms. */
    self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

    if (![self fetchViewContainer])
        return nil;

    /* add tap gesture recognizer for DVD menus and stuff */
    _tapRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self action:@selector(tapRecognized:)];

    CGSize size = _viewContainer.bounds.size;
    [self reportEvent:^{
        vout_window_ReportSize(_wnd, size.width, size.height);
    }];

    return self;
}

- (BOOL)fetchViewContainer
{
    @try {
        /* get the object we will draw into */
        UIView *viewContainer = (__bridge UIView*)var_InheritAddress (_wnd, "drawable-nsobject");
        if (unlikely(viewContainer == nil)) {
            msg_Err(_wnd, "provided view container is nil");
            return NO;
        }

        if (unlikely(![viewContainer respondsToSelector:@selector(isKindOfClass:)])) {
            msg_Err(_wnd, "void pointer not an ObjC object");
            return NO;
        }

        if (![viewContainer isKindOfClass:[UIView class]]) {
            msg_Err(_wnd, "passed ObjC object not of class UIView");
            return NO;
        }

        /* We need to store the view container because we'll add our view
         * only when a subview (hopefully able to handle the rendering) will
         * get created. The main reason for this is that we cannot report
         * events from the window until the display is opened, otherwise a
         * race condition involving locking both the main thread and the lock
         * in the core for the display are happening. */
        _viewContainer = viewContainer;

        self.frame = viewContainer.bounds;
        [self reshape];

        return YES;
    } @catch (NSException *exception) {
        msg_Err(_wnd, "Handling the view container failed due to an Obj-C exception (%s, %s", [exception.name UTF8String], [exception.reason UTF8String]);
        return NO;
    }
}

- (void)reportEvent:(void(^)())eventBlock
{
    CFStringRef mode = CFSTR("org.videolan.vlccore.window");
    CFRunLoopRef runloop = CFRunLoopGetCurrent();
    CFRunLoopPerformBlock(runloop, mode, ^{
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
            (eventBlock)();
            CFRunLoopStop(runloop);
        });
    });
    CFRunLoopWakeUp(runloop);
    CFRunLoopRunInMode(mode, 0, NO);
}

- (void)detachFromParent
{
    /* We need to lock because we consider that _wnd might be destroyed
     * after this function returns, typically as it will be called in the
     * Close() operation which preceed the vout_window_t destruction in
     * the core. */
    vlc_mutex_lock(&_mutex);
    /* The UIView must not be attached before releasing. Disable() is doing
     * exactly this asynchronously in the main thread so ensure it was called
     * here before detaching from the parent. */
    _wnd = NULL;
    vlc_mutex_unlock(&_mutex);
}

/**
 * We track whether we currently have a child view, which will be able
 * to do the actual rendering. Depending on how many children view we had
 * and whether we are in enabled state or not, we add or remove the view
 * from/to the parent UIView.
 * This is needed because we cannot emit resize event from the main
 * thread as long as the display is not created, since the display will
 * need the main thread too. It would non-deterministically deadlock
 * otherwise.
 */

- (void)didAddSubview:(UIView*)subview
{
    assert(_enabled);
    _subviews++;
    if (_subviews == 1)
        [_viewContainer addSubview:self];

    VLC_UNUSED(subview);
}

- (void)willRemoveSubview:(UIView*)subview
{
    _subviews--;
    if (_enabled && _subviews == 0)
        [self removeFromSuperview];

    VLC_UNUSED(subview);
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
    if (_enabled)
        return;

    assert(_subviews == 0);
    _enabled = YES;

    /* Bind tapRecognizer. */
    [self addGestureRecognizer:_tapRecognizer];
    _tapRecognizer.cancelsTouchesInView = NO;
}

- (void)disable
{
    if (!_enabled)
        return;

    _enabled = NO;
    assert(_subviews == 0);
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
    vlc_mutex_lock(&_mutex);
    if (_wnd == NULL)
    {
        vlc_mutex_unlock(&_mutex);
        return;
    }

    [self reportEvent:^{
        vout_window_ReportSize(_wnd,
                viewSize.width * scaleFactor,
                viewSize.height * scaleFactor);
    }];
    vlc_mutex_unlock(&_mutex);
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

    [self reportEvent:^{
        vout_window_ReportMouseMoved(_wnd,
                (int)touchPoint.x * scaleFactor, (int)touchPoint.y * scaleFactor);
        vout_window_ReportMousePressed(_wnd, MOUSE_BUTTON_LEFT);
        vout_window_ReportMouseReleased(_wnd, MOUSE_BUTTON_LEFT);
    }];
    vlc_mutex_unlock(&_mutex);
}

- (void)updateConstraints
{
    [super updateConstraints];
    [self reshape];
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
@end

/**
 * C core wrapper of the vout window operations for the ObjC module.
 */

static int Enable(vout_window_t *wnd, const vout_window_cfg_t *cfg)
{
    VLCVideoUIView *sys = (__bridge VLCVideoUIView *)wnd->sys;
    dispatch_async(dispatch_get_main_queue(), ^{
        [sys enable];
    });
    return VLC_SUCCESS;
}

static void Disable(vout_window_t *wnd)
{
    VLCVideoUIView *sys = (__bridge VLCVideoUIView *)wnd->sys;
    dispatch_async(dispatch_get_main_queue(), ^{
        [sys disable];
    });
}

static void Close(vout_window_t *wnd)
{
    VLCVideoUIView *sys = (__bridge_transfer VLCVideoUIView*)wnd->sys;

    /* We need to signal the asynchronous implementation that we have been
     * closed and cannot used _wnd anymore. */
    [sys detachFromParent];
}

static const struct vout_window_operations window_ops =
{
    .enable = Enable,
    .disable = Disable,
    .destroy = Close,
};

static int Open(vout_window_t *wnd)
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

    wnd->type = VOUT_WINDOW_TYPE_NSOBJECT;
    wnd->handle.nsobject = wnd->sys;
    wnd->ops = &window_ops;

    return VLC_SUCCESS;
}

vlc_module_begin ()
    set_shortname("UIView")
    set_description("iOS UIView vout window provider")
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout window", 300)
    set_callback(Open)

    add_shortcut("uiview", "ios")
vlc_module_end ()
