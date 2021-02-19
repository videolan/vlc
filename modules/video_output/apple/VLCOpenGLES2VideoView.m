/*****************************************************************************
 * VLCOpenGLES2VideoView.m: iOS OpenGL ES provider through CAEAGLLayer
 *****************************************************************************
 * Copyright (C) 2001-2017 VLC authors and VideoLAN
 * Copyright (C) 2021 Videolabs
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

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
#import <vlc_vout_display.h>
#import <vlc_opengl.h>
#import <vlc_dialog.h>
#import "../opengl/vout_helper.h"
#import "../opengl/gl_api.h"

@interface VLCOpenGLES2VideoView : UIView {
    vlc_gl_t *_gl;

    EAGLContext *_eaglContext;
    EAGLContext *_previousEaglContext;
    CAEAGLLayer *_layer;

    vlc_mutex_t _mutex;
    vlc_cond_t  _cond;
    vlc_cond_t  _gl_attached_wait;
    BOOL        _gl_attached;

    BOOL _bufferNeedReset;
    BOOL _appActive;
    BOOL _eaglEnabled;

    GLuint _renderBuffer;
    GLuint _frameBuffer;
    CGSize _size;

    struct vlc_gl_api _api;
}

- (id)initWithFrame:(CGRect)frame gl:(vlc_gl_t*)gl;
- (BOOL)makeCurrent;
- (void)releaseCurrent;
- (void)presentRenderbuffer;
- (void)didMoveToWindow;
- (void)detachFromWindow;
- (void)resize:(CGSize)size;
@end

static void vlc_dispatch_sync(void (^block_function)())
{
    CFRunLoopRef runloop = CFRunLoopGetMain();

    __block vlc_sem_t performed;
    vlc_sem_init(&performed, 0);

    CFStringRef modes_cfstrings[] = {
        kCFRunLoopDefaultMode,
        CFSTR("org.videolan.vlccore.window"),
    };

    CFArrayRef modes = CFArrayCreate(NULL, (const void **)modes_cfstrings,
            ARRAY_SIZE(modes_cfstrings),
            &kCFTypeArrayCallBacks);

    /* NOTE: we're using CFRunLoopPerformBlock with a custom mode tag
     * to avoid deadlocks between the window module (main thread) and the
     * display module, which would happen when using dispatch_sycn here. */
    CFRunLoopPerformBlock(runloop, modes, ^{
        (block_function)();
        vlc_sem_post(&performed);
    });
    CFRunLoopWakeUp(runloop);

    vlc_sem_wait(&performed);
    CFRelease(modes);
}

/*****************************************************************************
 * vlc_gl_t callbacks
 *****************************************************************************/
static void *GetSymbol(vlc_gl_t *gl, const char *name)
{
    VLC_UNUSED(gl);
    return dlsym(RTLD_DEFAULT, name);
}

static int MakeCurrent(vlc_gl_t *gl)
{
    VLCOpenGLES2VideoView *view = (__bridge VLCOpenGLES2VideoView *)gl->sys;

    if (![view makeCurrent])
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static void ReleaseCurrent(vlc_gl_t *gl)
{
    VLCOpenGLES2VideoView *view = (__bridge VLCOpenGLES2VideoView *)gl->sys;
    [view releaseCurrent];
}

static void Swap(vlc_gl_t *gl)
{
    VLCOpenGLES2VideoView *view = (__bridge VLCOpenGLES2VideoView *)gl->sys;
    [view presentRenderbuffer];
}

static void Resize(vlc_gl_t *gl, unsigned width, unsigned height)
{
    VLC_UNUSED(gl); VLC_UNUSED(width); VLC_UNUSED(height);
    /* Use the parent frame size for now, resize is smoother and called
     * automatically from the main thread queue. */
    VLCOpenGLES2VideoView *view = (__bridge VLCOpenGLES2VideoView *)gl->sys;
    [view resize:CGSizeMake(width, height)];
}

static void Close(vlc_gl_t *gl)
{
    /* Transfer ownership back from VLC to ARC so that it can be released. */
    VLCOpenGLES2VideoView *view = (__bridge_transfer VLCOpenGLES2VideoView*)gl->sys;

    /* We need to detach because the superview has a reference to our view. */
    [view detachFromWindow];
}

/*****************************************************************************
 * Our UIView object
 *****************************************************************************/
@implementation VLCOpenGLES2VideoView

+ (Class)layerClass
{
    return [CAEAGLLayer class];
}

- (id)initWithFrame:(CGRect)frame gl:(vlc_gl_t*)gl
{
    _gl = gl;

    _appActive = ([UIApplication sharedApplication].applicationState == UIApplicationStateActive);
    if (unlikely(!_appActive))
        return nil;

    self = [super initWithFrame:frame];
    if (!self)
        return nil;

    _eaglEnabled = YES;
    _bufferNeedReset = YES;

    vlc_mutex_init(&_mutex);
    vlc_cond_init(&_gl_attached_wait);
    vlc_cond_init(&_cond);

    /* The following creates a new OpenGL ES context with the API version we
     * need. If there is already an active context created by another OpenGL
     * provider we cache it and restore analog to the
     * makeCurrent/releaseCurrent pattern used through-out the class */
    _eaglContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    _previousEaglContext = nil;

    _layer = (CAEAGLLayer *)self.layer;
    _layer.drawableProperties = [NSDictionary dictionaryWithObject:kEAGLColorFormatRGBA8 forKey: kEAGLDrawablePropertyColorFormat];
    _layer.opaque = YES;

    /* Resize is done accordingly to the parent frame directly. */
    self.contentMode = UIViewContentModeScaleToFill;

    /* Connect to the parent UIView which will contain this surface.
     * Using a parent UIView makes it easier to handle window resize and
     * still have full control over the display object, since layers don't
     * need to draw anything to exist. */
    if (![self attachToWindow: _gl->surface])
        return nil;

    /* Listen application state change because we cannot use OpenGL in the
     * background. This should probably move to the vout_window reports in
     * the future, which could even signal that we need to disable the whole
     * display and potentially adapt playback for that. */

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationStateChanged:)
                                                 name:UIApplicationWillEnterForegroundNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationStateChanged:)
                                                 name:UIApplicationDidBecomeActiveNotification
                                               object:nil];
    if (_appActive)
        [self resize:CGSizeMake(frame.size.width, frame.size.height)];

    /* Setup the usual vlc_gl_t callbacks before loading the API since we need
     * the get_proc_address symbol and a current context. */
    gl->make_current = MakeCurrent;
    gl->release_current = ReleaseCurrent;
    gl->resize = Resize;
    gl->swap = Swap;
    gl->get_proc_address = GetSymbol;
    gl->destroy = Close;

    return self;
}

- (BOOL)attachToWindow:(vout_window_t*)wnd
{
    @try {
        UIView *viewContainer = (__bridge UIView*)wnd->handle.nsobject;
        /* get the object we will draw into */
        if (unlikely(viewContainer == nil)) {
            msg_Err(_gl, "provided view container is nil");
            return NO;
        }

        if (unlikely(![viewContainer respondsToSelector:@selector(isKindOfClass:)])) {
            msg_Err(_gl, "void pointer not an ObjC object");
            return NO;
        }

        if (unlikely(![viewContainer isKindOfClass:[UIView class]])) {
            msg_Err(_gl, "passed ObjC object not of class UIView");
            return NO;
        }

        /* Initial size setup */
        self.frame = viewContainer.bounds;

        [viewContainer addSubview:self];

        return YES;
    } @catch (NSException *exception) {
        msg_Err(_gl, "Handling the view container failed due to an Obj-C exception (%s, %s", [exception.name UTF8String], [exception.reason UTF8String]);
        return NO;
    }
}

- (void)detachFromWindow
{
    EAGLContext *previous_context = [EAGLContext currentContext];
    [EAGLContext setCurrentContext:_eaglContext];
    glDeleteFramebuffers(1, &_frameBuffer);
    glDeleteRenderbuffers(1, &_renderBuffer);
    [EAGLContext setCurrentContext:previous_context];

    /* Flush the OpenGL pipeline before leaving. */
    vlc_mutex_lock(&_mutex);
    if (_eaglEnabled)
        [self flushEAGLLocked];
    _eaglEnabled = NO;
    vlc_mutex_unlock(&_mutex);

    /* This cannot be a synchronous dispatch because player is usually running
     * in the main thread and block the main thread unless we accept our fate
     * and exit here. */
    dispatch_async(dispatch_get_main_queue(), ^{
         /* Remove the external references to the view so that
          * dealloc can be called by ARC. */

         [[NSNotificationCenter defaultCenter] removeObserver:self
                                                         name:UIApplicationWillResignActiveNotification
                                                       object:nil];

         [[NSNotificationCenter defaultCenter] removeObserver:self
                                                         name:UIApplicationDidBecomeActiveNotification
                                                       object:nil];

         [[NSNotificationCenter defaultCenter] removeObserver:self
                                                         name:UIApplicationWillEnterForegroundNotification
                                                       object:nil];

         [[NSNotificationCenter defaultCenter] removeObserver:self
                                                         name:UIApplicationDidEnterBackgroundNotification
                                                       object:nil];
        assert(!_gl_attached);
        [self removeFromSuperview];
    });
}

- (void)didMoveToWindow
{
    self.contentScaleFactor = self.window.screen.scale;

    vlc_mutex_lock(&_mutex);
    _bufferNeedReset = YES;
    vlc_mutex_unlock(&_mutex);
}

- (BOOL)doResetBuffers
{
    if (_frameBuffer != 0)
    {
        /* clear frame buffer */
        glDeleteFramebuffers(1, &_frameBuffer);
        _frameBuffer = 0;
    }

    if (_renderBuffer != 0)
    {
        /* clear render buffer */
        glDeleteRenderbuffers(1, &_renderBuffer);
        _renderBuffer = 0;
    }

    glGenFramebuffers(1, &_frameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, _frameBuffer);

    glGenRenderbuffers(1, &_renderBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _renderBuffer);

    [_eaglContext renderbufferStorage:GL_RENDERBUFFER fromDrawable:_layer];

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _renderBuffer);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        msg_Err(_gl, "Failed to make complete framebuffer object %x", glCheckFramebufferStatus(GL_FRAMEBUFFER));
        return NO;
    }
    return YES;
}

- (BOOL)makeCurrent
{
    assert(![NSThread isMainThread]);

    vlc_mutex_lock(&_mutex);
    assert(!_gl_attached);

    while (unlikely(!_appActive))
        vlc_cond_wait(&_cond, &_mutex);

    assert(_eaglEnabled);
    _previousEaglContext = [EAGLContext currentContext];

    assert(_eaglContext);
    BOOL result = [EAGLContext setCurrentContext:_eaglContext];
    assert(result == YES);

    _gl_attached = YES;
    return YES;
}

- (void)releaseCurrent
{
    vlc_mutex_assert(&_mutex);

    assert(_gl_attached);
    _gl_attached = NO;
    [EAGLContext setCurrentContext:_previousEaglContext];
    _previousEaglContext = nil;
    vlc_cond_signal(&_gl_attached_wait);
    vlc_mutex_unlock(&_mutex);
}

- (void)presentRenderbuffer
{
    [_eaglContext presentRenderbuffer:GL_RENDERBUFFER];
}

- (void)resize:(CGSize)size
{
    vlc_mutex_lock(&_mutex);
    _size = size;

    vlc_dispatch_sync(^{
        CGRect rect = self.bounds;
        rect.size = size;
        /* Bitmap size = view size * contentScaleFactor, so we need to divide the
         * scale factor to get the real view size. */
        rect.size.width /= self.contentScaleFactor;
        rect.size.height /= self.contentScaleFactor;

        self.bounds = rect;
    });

    /* If size is NULL, rendering must be disabled */
    if (size.width != 0 && size.height != 0)
    {
        EAGLContext *previousContext = [EAGLContext currentContext];
        [EAGLContext setCurrentContext:_eaglContext];
        [self doResetBuffers];
        [EAGLContext setCurrentContext:previousContext];
    }

    vlc_mutex_unlock(&_mutex);
}

- (void)layoutSubviews
{
    vlc_mutex_lock(&_mutex);
    _bufferNeedReset = YES;
    vlc_mutex_unlock(&_mutex);
}

- (void)flushEAGLLocked
{
    assert(_eaglEnabled);

    /* Ensure that all previously submitted commands are drained from the
     * command buffer and are executed by OpenGL ES before moving to the
     * background.*/
    EAGLContext *previousEaglContext = [EAGLContext currentContext];
    if ([EAGLContext setCurrentContext:_eaglContext])
        glFinish();
    [EAGLContext setCurrentContext:previousEaglContext];
}

- (void)applicationStateChanged:(NSNotification *)notification
{
    vlc_mutex_lock(&_mutex);

    if ([[notification name] isEqualToString:UIApplicationDidEnterBackgroundNotification])
    {
        _appActive = NO;

        /* Wait for the vout to unlock the eagl context before releasing
         * it. */
        while (_gl_attached && _eaglEnabled)
            vlc_cond_wait(&_gl_attached_wait, &_mutex);

        /* _eaglEnabled can change during the vlc_cond_wait
         * as the mutex is unlocked during that, so this check
         * has to be done after the vlc_cond_wait! */
        if (_eaglEnabled) {
            [self flushEAGLLocked];
            _eaglEnabled = NO;
        }
    }
    else if ([[notification name] isEqualToString:UIApplicationWillEnterForegroundNotification])
    {
        _eaglEnabled = YES;
        [self resize:self.frame.size];
        _appActive = YES;
    }

    vlc_cond_broadcast(&_cond);
    vlc_mutex_unlock(&_mutex);
}

- (void)updateConstraints
{
    [super updateConstraints];
}

- (BOOL)isOpaque
{
    return YES;
}

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
    /* Disable events for this view, as the vout_window view will be the one
     * handling them. */
    return nil;
}
@end



static int Open(vlc_gl_t *gl, unsigned width, unsigned height)
{
    vout_window_t *wnd = gl->surface;

    /* We only support UIView container window. */
    if (wnd->type != VOUT_WINDOW_TYPE_NSOBJECT)
        return VLC_EGENERIC;

    @autoreleasepool {
        /* NOTE: we're using CFRunLoopPerformBlock with the "vlc_runloop" tag
         * to avoid deadlocks between the window module (main thread) and the
         * display module, which would happen when using dispatch_sycn here. */
        vlc_dispatch_sync(^{
            gl->sys = (__bridge_retained void*)[[VLCOpenGLES2VideoView alloc]
               initWithFrame:CGRectMake(0.,0.,width,height) gl:gl];
        });

        if (gl->sys == NULL)
        {
            msg_Err(gl, "Creating OpenGL ES 2 view failed");
            return VLC_EGENERIC;
        }
    }

    return VLC_SUCCESS;
}

vlc_module_begin ()
    set_shortname (N_("CAEAGL"))
    set_description (N_("CAEAGL provider for OpenGL"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("opengl es2", 50)
    set_callback(Open)
    add_shortcut ("caeagl")
vlc_module_end ()
