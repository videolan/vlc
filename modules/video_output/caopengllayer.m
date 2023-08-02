/*****************************************************************************
 * caopengllayer.m: CAOpenGLLayer (Mac OS X) video output
 *****************************************************************************
 * Copyright (C) 2014-2017 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: David Fuhrmann <david dot fuhrmann at googlemail dot com>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont at videolan dot org>
 *
 * Some of the code is based on mpv's video_layer.swift by "der richter"
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_opengl.h>
#include <vlc_atomic.h>

#import <QuartzCore/QuartzCore.h>
#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#import <dlfcn.h>

#include "opengl/vout_helper.h"

/*****************************************************************************
 * Vout interface
 *****************************************************************************/
static int  Open   (vlc_object_t *);
static void Close  (vlc_object_t *);

static picture_pool_t *Pool (vout_display_t *vd, unsigned requested_count);
static void PictureRender   (vout_display_t *vd, picture_t *pic, subpicture_t *subpicture);
static void PictureDisplay  (vout_display_t *vd, picture_t *pic, subpicture_t *subpicture);
static int Control          (vout_display_t *vd, int query, va_list ap);

/**
 * Protocol declaration that drawable-nsobject should follow
 */
@protocol VLCOpenGLVideoViewEmbedding <NSObject>
- (void)addVoutSubview:(NSView *)view;
- (void)removeVoutSubview:(NSView *)view;
@end

/**
 * Layer subclass that handles OpenGL video rendering
 */
@interface VLCCAOpenGLLayer : CAOpenGLLayer
{
    NSLock *_displayLock;
    vout_display_t *_voutDisplay; // All accesses to this must be @synchronized(self)
                                  // unless you can be sure it won't be called in teardown
    CGLContextObj _glContext;
}

- (instancetype)initWithVoutDisplay:(vout_display_t *)vd;
- (void)displayFromVout;
- (void)reportCurrentLayerSize;
- (void)reportCurrentLayerSizeWithScale:(CGFloat)scale;
- (void)vlcClose;
@end

/**
 * View subclass which is backed by a VLCCAOpenGLLayer
 */
#if __MAC_OS_X_VERSION_MAX_ALLOWED < 101400
// macOS SDKs lower than 10.14 did not have a NSViewLayerContentScaleDelegate
// protocol definition, but its not needed, it will work fine without it as the
// delegate method even existed before, just not the protocol.
@interface VLCVideoLayerView : NSView <CALayerDelegate>
#else
@interface VLCVideoLayerView : NSView <CALayerDelegate, NSViewLayerContentScaleDelegate>
#endif
{
    vout_display_t *_vlc_vd; // All accesses to this must be @synchronized(self)
}

- (instancetype)initWithVoutDisplay:(vout_display_t *)vd;
- (void)vlcClose;
@end

struct vout_display_sys_t {
    vout_window_t *embed;
    id<VLCOpenGLVideoViewEmbedding> container;

    picture_pool_t *pool;
    picture_resource_t resource;

    VLCVideoLayerView *videoView; // Layer-backed view that creates videoLayer
    VLCCAOpenGLLayer *videoLayer; // Backing layer of videoView

    vlc_gl_t *gl;
    vout_display_opengl_t *vgl;

    vout_display_place_t place;
    vout_display_cfg_t cfg;

    atomic_bool is_ready;
};

#pragma mark -
#pragma mark OpenGL context helpers

/**
 * Create a new CGLContextObj for use by VLC
 * This function may try various pixel formats until it finds a suitable/compatible
 * one that works on the given hardware.
 * \return CGLContextObj or NULL in case of error
 */
CGLContextObj vlc_CreateCGLContext()
{
    CGLError err;
    GLint npix = 0;
    CGLPixelFormatObj pix;
    CGLContextObj ctx;

    CGLPixelFormatAttribute attribs[12] = {
        kCGLPFAAllowOfflineRenderers,
        kCGLPFADoubleBuffer,
        kCGLPFAAccelerated,
        kCGLPFANoRecovery,
        kCGLPFAColorSize, 24,
        kCGLPFAAlphaSize, 8,
        kCGLPFADepthSize, 24,
        0, // If ever extending this list, adjust the offset below!
        0
    };

    if (@available(macOS 10.8, *)) {
        // Enable automatic graphics switching support, important on Macs
        // with dedicated GPUs, as it allows to not always use the dedicated
        // GPU which has more power consumption
        attribs[10] = kCGLPFASupportsAutomaticGraphicsSwitching;
    }

    err = CGLChoosePixelFormat(attribs, &pix, &npix);
    if (err != kCGLNoError || pix == NULL) {
        return NULL;
    }

    err = CGLCreateContext(pix, NULL, &ctx);
    if (err != kCGLNoError || ctx == NULL) {
        return NULL;
    }

    CGLDestroyPixelFormat(pix);
    return ctx;
}

struct vlc_gl_sys
{
    CGLContextObj cgl; // The CGL context managed by us
    CGLContextObj cgl_prev; // The previously current CGL context, if any
};

/**
 * Flush the OpenGL context
 * In case of double-buffering swaps the back buffer with the front buffer.
 * \note This function implicitly calls \c glFlush() before it returns.
 */
static void gl_cb_Swap(vlc_gl_t *vlc_gl)
{
    struct vlc_gl_sys *sys = vlc_gl->sys;

    // Copies a double-buffered contexts back buffer to front buffer, calling
    // glFlush before this is not needed and discouraged for performance reasons.
    // An implicit glFlush happens before CGLFlushDrawable returns.
    CGLFlushDrawable(sys->cgl);
}

/**
 * Make the OpenGL context the current one
 * Makes the CGL context the current context, if it is not already the current one,
 * and locks it.
 */
static int gl_cb_MakeCurrent(vlc_gl_t *vlc_gl)
{
    CGLError err;
    struct vlc_gl_sys *sys = vlc_gl->sys;

    sys->cgl_prev = CGLGetCurrentContext();

    if (sys->cgl_prev != sys->cgl) {
        err = CGLSetCurrentContext(sys->cgl);
        if (err != kCGLNoError) {
            msg_Err(vlc_gl, "Failure setting current CGLContext: %s", CGLErrorString(err));
            return VLC_EGENERIC;
        }
    }

    err = CGLLockContext(sys->cgl);
    if (err != kCGLNoError) {
        msg_Err(vlc_gl, "Failure locking CGLContext: %s", CGLErrorString(err));
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/**
 * Make the OpenGL context no longer current one.
 * Makes the previous context the current one and unlocks the CGL context.
 */
static void gl_cb_ReleaseCurrent(vlc_gl_t *vlc_gl)
{
    CGLError err;
    struct vlc_gl_sys *sys = vlc_gl->sys;

    assert(CGLGetCurrentContext() == sys->cgl);

    err = CGLUnlockContext(sys->cgl);
    if (err != kCGLNoError) {
        msg_Err(vlc_gl, "Failure unlocking CGLContext: %s", CGLErrorString(err));
        abort();
    }

    if (sys->cgl_prev != sys->cgl) {
        err = CGLSetCurrentContext(sys->cgl_prev);
        if (err != kCGLNoError) {
            msg_Err(vlc_gl, "Failure restoring previous CGLContext: %s", CGLErrorString(err));
            abort();
        }
    }

    sys->cgl_prev = NULL;
}

/**
 * Look up OpenGL symbols by name
 */
static void *gl_cb_GetProcAddress(vlc_gl_t *vlc_gl, const char *name)
{
    VLC_UNUSED(vlc_gl);

    return dlsym(RTLD_DEFAULT, name);
}


#pragma mark -
#pragma mark Module functions

static int Open(vlc_object_t *this)
{
    @autoreleasepool {
        vout_display_t *vd = (vout_display_t *)this;
        vout_display_sys_t *sys;

        vd->sys = sys = vlc_obj_calloc(this, 1, sizeof(*sys));
        if (sys == NULL)
            return VLC_ENOMEM;

        // Only use this video output on macOS 10.14 or higher
        // currently, as it has some issues on at least macOS 10.7
        // and the old NSView based output still works fine on old
        // macOS versions.
        if (@available(macOS 10.14, *)) {
            // This is intentionally left empty, as the check
            // can not be negated or combined with other conditions!
        } else {
            if (!vd->obj.force)
                return VLC_EGENERIC;
        }

        // Obtain container NSObject
        id container = var_CreateGetAddress(vd, "drawable-nsobject");
        if (container) {
            vout_display_DeleteWindow(vd, NULL);
        } else {
            sys->embed = vout_display_NewWindow(vd, VOUT_WINDOW_TYPE_NSOBJECT);
            if (sys->embed)
                container = sys->embed->handle.nsobject;

            if (!container) {
                msg_Err(vd, "No drawable-nsobject found!");
                goto error;
            }
        }

        // Retain container, released in Close
        sys->container = [container retain];
    
        // Create the CGL context
        CGLContextObj cgl_ctx = vlc_CreateCGLContext();
        if (cgl_ctx == NULL) {
            msg_Err(vd, "Failure to create CGL context!");
            goto error;
        }

        // Create a pseudo-context object which provides needed callbacks
        // for VLC to deal with the CGL context. Usually this should be done
        // by a proper opengl provider module, but we do not have that currently.
        sys->gl = vlc_object_create(vd, sizeof(*sys->gl));
        if (unlikely(!sys->gl))
            goto error;
        
        struct vlc_gl_sys *glsys = sys->gl->sys = malloc(sizeof(*glsys));
        if (unlikely(!glsys)) {
            Close(this);
            return VLC_ENOMEM;
        }
        glsys->cgl = cgl_ctx;
        glsys->cgl_prev = NULL;

        sys->gl->swap = gl_cb_Swap;
        sys->gl->makeCurrent = gl_cb_MakeCurrent;
        sys->gl->releaseCurrent = gl_cb_ReleaseCurrent;
        sys->gl->getProcAddress = gl_cb_GetProcAddress;

        // Set the CGL context to the "macosx-glcontext" as the
        // CGL context is needed for CIFilters and the CVPX converter
        var_Create(vd->obj.parent, "macosx-glcontext", VLC_VAR_ADDRESS);
        var_SetAddress(vd->obj.parent, "macosx-glcontext", cgl_ctx);

        dispatch_sync(dispatch_get_main_queue(), ^{
            // Reverse vertical alignment as the GL tex are Y inverted
           sys->cfg = *vd->cfg;
           if (sys->cfg.align.vertical == VOUT_DISPLAY_ALIGN_TOP)
               sys->cfg.align.vertical = VOUT_DISPLAY_ALIGN_BOTTOM;
           else if (sys->cfg.align.vertical == VOUT_DISPLAY_ALIGN_BOTTOM)
               sys->cfg.align.vertical = VOUT_DISPLAY_ALIGN_TOP;

            // Create video view
            sys->videoView = [[VLCVideoLayerView alloc] initWithVoutDisplay:vd];
            sys->videoLayer = (VLCCAOpenGLLayer*)[[sys->videoView layer] retain];
            // Add video view to container
            if ([container respondsToSelector:@selector(addVoutSubview:)]) {
                [container addVoutSubview:sys->videoView];
            } else if ([container isKindOfClass:[NSView class]]) {
                NSView *containerView = container;
                [containerView addSubview:sys->videoView];
                [sys->videoView setFrame:containerView.bounds];
                [sys->videoLayer reportCurrentLayerSize];
            } else {
                [sys->videoView release];
                [sys->videoLayer release];
                sys->videoView = nil;
                sys->videoLayer = nil;
            }

            vout_display_PlacePicture(&sys->place, &vd->source, &sys->cfg, false);
        });

        if (sys->videoView == nil) {
            msg_Err(vd,
                    "Invalid drawable-nsobject object, must either be an NSView "
                    "or comply with the VLCOpenGLVideoViewEmbedding protocol");
            goto error;
        }

        // Initialize OpenGL video display
        const vlc_fourcc_t *spu_chromas;

        if (vlc_gl_MakeCurrent(sys->gl))
            goto error;

        sys->vgl = vout_display_opengl_New(&vd->fmt, &spu_chromas, sys->gl,
                                           &vd->cfg->viewpoint);
        vlc_gl_ReleaseCurrent(sys->gl);

        if (sys->vgl == NULL) {
            msg_Err(vd, "Error while initializing OpenGL display");
            goto error;
        }

        vd->info.has_pictures_invalid = false;
        vd->info.subpicture_chromas = spu_chromas;

        vd->pool    = Pool;
        vd->prepare = PictureRender;
        vd->display = PictureDisplay;
        vd->control = Control;

        atomic_init(&sys->is_ready, false);
        return VLC_SUCCESS;

    error:
        Close(this);
        return VLC_EGENERIC;
    }
}

static void Close(vlc_object_t *p_this)
{
    vout_display_t *vd = (vout_display_t *)p_this;
    vout_display_sys_t *sys = vd->sys;

    atomic_store(&sys->is_ready, false);
    [sys->videoView vlcClose];

    if (sys->vgl && !vlc_gl_MakeCurrent(sys->gl)) {
        vout_display_opengl_Delete(sys->vgl);
        vlc_gl_ReleaseCurrent(sys->gl);
    }

    if (sys->embed)
        vout_display_DeleteWindow(vd, sys->embed);

    if (sys->gl) {
        struct vlc_gl_sys *glsys = sys->gl->sys;

        // It should never happen that the context is destroyed and we
        // still have a previous context set, as it would mean non-balanced
        // calls to MakeCurrent/ReleaseCurrent.
        assert(glsys->cgl_prev == NULL);

        CGLReleaseContext(glsys->cgl);
        vlc_object_release(sys->gl);
        free(glsys);
    }

    // Copy pointers out of sys, as sys can be gone already
    // when the dispatch_async block is run!
    id container = sys->container;
    VLCVideoLayerView *videoView = sys->videoView;
    VLCCAOpenGLLayer *videoLayer = sys->videoLayer;

    dispatch_async(dispatch_get_main_queue(), ^{
        // Remove vout subview from container
        if ([container respondsToSelector:@selector(removeVoutSubview:)]) {
            [container removeVoutSubview:videoView];
        }
        [videoView removeFromSuperview];

        [videoView release];
        [container release];
        [videoLayer release];
    });
}

static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->pool && vlc_gl_MakeCurrent(sys->gl) == VLC_SUCCESS)
    {
        sys->pool = vout_display_opengl_GetPool(sys->vgl, count);
        vlc_gl_ReleaseCurrent(sys->gl);
    }
    return sys->pool;
}

static void PictureRender(vout_display_t *vd, picture_t *pic, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    if (vlc_gl_MakeCurrent(sys->gl) == VLC_SUCCESS)
    {
        vout_display_opengl_Prepare(sys->vgl, pic, subpicture);
        vlc_gl_ReleaseCurrent(sys->gl);

        atomic_store(&sys->is_ready, true);
    }
}

static void PictureDisplay(vout_display_t *vd, picture_t *pic, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    [sys->videoLayer displayFromVout];

    picture_Release(pic);
    if (subpicture)
        subpicture_Delete(subpicture);
}

static int Control(vout_display_t *vd, int query, va_list ap)
{
    vout_display_sys_t *sys = vd->sys;

    if (!vd->sys)
        return VLC_EGENERIC;

    switch (query)
    {
        case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
            return VLC_SUCCESS;

        case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        case VOUT_DISPLAY_CHANGE_ZOOM:
        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
        {
            @synchronized(sys->videoLayer)
            {
                vout_display_cfg_t cfg;

                if (query == VOUT_DISPLAY_CHANGE_SOURCE_ASPECT ||
                    query == VOUT_DISPLAY_CHANGE_SOURCE_CROP) {
                    cfg = *vd->cfg;
                } else {
                    cfg = *(const vout_display_cfg_t*)va_arg (ap, const vout_display_cfg_t *);
                }

                cfg.display.width = sys->cfg.display.width;
                cfg.display.height = sys->cfg.display.height;

                // Reverse vertical alignment as the GL tex are Y inverted
                if (cfg.align.vertical == VOUT_DISPLAY_ALIGN_TOP)
                    cfg.align.vertical = VOUT_DISPLAY_ALIGN_BOTTOM;
                else if (cfg.align.vertical == VOUT_DISPLAY_ALIGN_BOTTOM)
                    cfg.align.vertical = VOUT_DISPLAY_ALIGN_TOP;
                sys->cfg = cfg;

                vout_display_PlacePicture(&sys->place, &vd->source, &cfg, false);
            }

            // Note!
            // No viewport or aspect ratio is set here, as that needs to be set
            // when rendering. The viewport is always set to match the layer
            // size by the OS right before the OpenGL render callback, so
            // setting it here has no effect.
            return VLC_SUCCESS;
        }

        case VOUT_DISPLAY_CHANGE_VIEWPOINT:
        {
            return vout_display_opengl_SetViewpoint(sys->vgl,
                        &va_arg (ap, const vout_display_cfg_t* )->viewpoint);
        }

        case VOUT_DISPLAY_RESET_PICTURES:
            vlc_assert_unreachable();
        default:
            msg_Err (vd, "Unhandled request %d", query);
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

#pragma mark -
#pragma mark VLCVideoLayerView

@implementation VLCVideoLayerView

- (instancetype)initWithVoutDisplay:(vout_display_t *)vd
{
    self = [super init];
    if (self) {
        _vlc_vd = vd;

        self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        self.wantsLayer = YES;
    }
    return self;
}

/**
 * Invalidates VLC objects (notably _vlc_vd)
 * This method must be called in VLCs module Close (or indirectly by the View)
 * to ensure all critical VLC resources that might be gone when the module is
 * closed are properly NULLed. This is necessary as dealloc is only called later
 * as it has to be done async on the main thread, because NSView must be
 * dealloc'ed on the main thread and the view own the layer, so the layer
 * will stay valid until the view is gone, and might still use _vlc_vd
 * even after the VLC module is gone and the resources would be invalid.
 */
- (void)vlcClose
{
    @synchronized (self) {
        [(VLCCAOpenGLLayer *)self.layer vlcClose];
        _vlc_vd = NULL;
    }
}

- (void)viewWillStartLiveResize
{
    [(VLCCAOpenGLLayer *)self.layer setAsynchronous:YES];
}

- (void)viewDidEndLiveResize
{
    [(VLCCAOpenGLLayer *)self.layer setAsynchronous:NO];

    // After a live resize we need to tell the core about our new size
    [(VLCCAOpenGLLayer *)self.layer reportCurrentLayerSize];
}

- (CALayer *)makeBackingLayer
{
    @synchronized(self) {
        NSAssert(_vlc_vd != NULL, @"Cannot create backing layer without vout display!");

        VLCCAOpenGLLayer *layer = [[VLCCAOpenGLLayer alloc] initWithVoutDisplay:_vlc_vd];
        layer.delegate = self;
        return [layer autorelease];
    }
}

/* Layer delegate method that ensures the layer always get the
 * correct contentScale based on whether the view is on a HiDPI
 * display or not, and when it is moved between displays.
 */
- (BOOL)layer:(CALayer *)layer
shouldInheritContentsScale:(CGFloat)newScale
   fromWindow:(NSWindow *)window
{
    // If the scale changes, from the OpenGL point of view
    // the size changes, so we need to indicate a resize
    if (layer == self.layer) {
        [(VLCCAOpenGLLayer *)self.layer
            reportCurrentLayerSizeWithScale:newScale];
        // FIXME
        // For a brief moment the old image with a wrong scale
        // is still visible, thats because the resize event is not
        // processed immediately. Ideally this would be handled similar
        // to how the live resize is done, to avoid this.
    }
    return YES;
}

/*
 * General properties
 */

- (BOOL)isOpaque
{
    return YES;
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

/* Left mouse button down */
- (void)mouseDown:(NSEvent *)event
{
    @synchronized(self) {
        if (!_vlc_vd) {
            [super mouseDown:event];
            return;
        }

        if (event.type == NSLeftMouseDown &&
            !(event.modifierFlags & NSControlKeyMask) &&
            event.clickCount == 1) {
            vout_display_SendEventMousePressed(_vlc_vd, MOUSE_BUTTON_LEFT);
        }
    }

    [super mouseDown:event];
}

/* Left mouse button up */
- (void)mouseUp:(NSEvent *)event
{
    @synchronized(self) {
        if (!_vlc_vd) {
            [super mouseUp:event];
            return;
        }

        if (event.type == NSLeftMouseUp) {
            vout_display_SendEventMouseReleased(_vlc_vd, MOUSE_BUTTON_LEFT);
        }
    }

    [super mouseUp:event];
}

/* Middle mouse button down */
- (void)otherMouseDown:(NSEvent *)event
{
    @synchronized(self) {
        if (_vlc_vd)
            vout_display_SendEventMousePressed(_vlc_vd, MOUSE_BUTTON_CENTER);
    }

    [super otherMouseDown:event];
}

/* Middle mouse button up */
- (void)otherMouseUp:(NSEvent *)event
{
    @synchronized(self) {
        if (_vlc_vd)
            vout_display_SendEventMouseReleased(_vlc_vd, MOUSE_BUTTON_CENTER);
    }

    [super otherMouseUp:event];
}

- (void)mouseMovedInternal:(NSEvent *)event
{
    @synchronized(self) {
        if (!_vlc_vd) {
            return;
        }

        vout_display_sys_t *sys = _vlc_vd->sys;

        // Convert window-coordinate point to view space
        NSPoint pointInView = [self convertPoint:event.locationInWindow fromView:nil];

        // Convert to pixels
        NSPoint pointInBacking = [self convertPointToBacking:pointInView];

        vout_display_SendMouseMovedDisplayCoordinates(_vlc_vd, ORIENT_VFLIPPED,
                                                      pointInBacking.x, pointInBacking.y,
                                                      &sys->place);
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

#pragma mark -
#pragma mark VLCCAOpenGLLayer

@implementation VLCCAOpenGLLayer

- (instancetype)initWithVoutDisplay:(vout_display_t *)vd
{
    self = [super init];
    if (self) {
        _displayLock = [[NSLock alloc] init];
        _voutDisplay = vd;

        struct vlc_gl_sys *glsys = vd->sys->gl->sys;
        _glContext = CGLRetainContext(glsys->cgl);

        [CATransaction lock];
        self.needsDisplayOnBoundsChange = YES;
        self.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
        self.asynchronous = NO;
        self.opaque = 1.0;
        self.hidden = NO;
        [CATransaction unlock];
    }

    return self;
}

/**
 * Invalidates VLC objects (notably _voutDisplay)
 * This method must be called in VLCs module Close (or indirectly by the View).
 */
- (void)vlcClose
{
    @synchronized (self) {
        _voutDisplay = NULL;
    }
}

- (void)dealloc
{
    CGLReleaseContext(_glContext);
    [_displayLock release];
    [super dealloc];
}

- (void)layoutSublayers
{
    [super layoutSublayers];

    if (self.asynchronous) {
        // During live resize, the size is updated in the
        // OpenGL draw callback, to ensure atomic size changes
        // that are in sync with the real layer size.
        // This bypasses the core but is needed for resizing
        // without glitches or lags.
        return;
    }

    [self reportCurrentLayerSize];
}

- (void)reportCurrentLayerSizeWithScale:(CGFloat)scale
{
    CGSize newSize = self.visibleRect.size;

    // Calculate pixel values
    newSize.width *= scale;
    newSize.height *= scale;

    @synchronized(self) {
        if (!_voutDisplay)
            return;

        vout_display_sys_t *sys = _voutDisplay->sys;
        @synchronized(sys->videoLayer) {
            sys->cfg.display.width = newSize.width;
            sys->cfg.display.height = newSize.height;
        }

        /* Workaround: there's no window module, so signal the correct size to the core. */
        vout_display_SendEventDisplaySize (_voutDisplay,
            newSize.width, newSize.height);
    }
}

- (void)reportCurrentLayerSize
{
    CGFloat scale = self.contentsScale;
    [self reportCurrentLayerSizeWithScale:scale];
}

- (void)display
{
    [_displayLock lock];

    [super display];
    [CATransaction flush];

    [_displayLock unlock];
}

- (void)displayFromVout
{
    if (self.asynchronous) {
        // During live resizing we do not take updates
        // from the vout, as those would interfere with
        // the rendering currently happening on the main
        // thread for the resize. Rendering anyway happens
        // triggered by the OS every display refresh, so
        // forcing an update here would be useless anyway.
        return;
    }

    [self display];
}

- (BOOL)canDrawInCGLContext:(CGLContextObj)glContext
                pixelFormat:(CGLPixelFormatObj)pixelFormat
               forLayerTime:(CFTimeInterval)timeInterval
                displayTime:(const CVTimeStamp *)timeStamp
{
    @synchronized(self) {
        if (!_voutDisplay)
            return NO;

        return (atomic_load(&_voutDisplay->sys->is_ready));
    }
}

- (void)drawInCGLContext:(CGLContextObj)glContext
             pixelFormat:(CGLPixelFormatObj)pixelFormat
            forLayerTime:(CFTimeInterval)timeInterval
             displayTime:(const CVTimeStamp *)timeStamp
{
    @synchronized(self) {
        if (!_voutDisplay)
            return;

        vout_display_sys_t *sys = _voutDisplay->sys;

        if (vlc_gl_MakeCurrent(sys->gl))
            return;

        GLint dims[4] = { 0, 0, 0, 0 };
        glGetIntegerv(GL_VIEWPORT, dims);
        NSSize newSize = NSMakeSize(dims[2], dims[3]);

        if (NSEqualSizes(newSize, NSZeroSize)) {
            newSize = self.bounds.size;
            CGFloat scale = self.contentsScale;
            newSize.width *= scale;
            newSize.height *= scale;
        }

        @synchronized(sys->videoView)
        {
            sys->cfg.display.width = newSize.width;
            sys->cfg.display.height = newSize.height;

            vout_display_PlacePicture(&sys->place, &_voutDisplay->source, &sys->cfg, false);
        }

        // Ensure viewport and aspect ratio is correct
        vout_display_opengl_Viewport(sys->vgl, sys->place.x, sys->place.y,
                                     sys->place.width, sys->place.height);
        vout_display_opengl_SetWindowAspectRatio(sys->vgl, (float)sys->place.width / sys->place.height);

        vout_display_opengl_Display(sys->vgl, &_voutDisplay->source);
        vlc_gl_ReleaseCurrent(sys->gl);
    }
}

- (CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask
{
    CGLPixelFormatObj fmt = CGLGetPixelFormat(_glContext);
    
    return (fmt) ? CGLRetainPixelFormat(fmt) : NULL;
}

- (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pixelFormat
{
    return CGLRetainContext(_glContext);
}

@end

/*
 * Module descriptor
 */

vlc_module_begin()
    set_description(N_("Core Animation OpenGL Layer (Mac OS X)"))
    set_capability("vout display", 300)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_callbacks(Open, Close)
vlc_module_end()
