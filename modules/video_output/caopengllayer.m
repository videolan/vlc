/*****************************************************************************
 * caopengllayer.m: CAOpenGLLayer (Mac OS X) video output
 *****************************************************************************
 * Copyright (C) 2014-2017 VLC authors and VideoLAN
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

#include "opengl/renderer.h"
#include "opengl/vout_helper.h"

/*****************************************************************************
 * Vout interface
 *****************************************************************************/
static int Open(vout_display_t *vd, video_format_t *fmt, vlc_video_context *context);
static void Close(vout_display_t *vd);

static void PictureRender   (vout_display_t *vd, picture_t *pic, const vlc_render_subpicture *subpicture,
                             vlc_tick_t date);
static void PictureDisplay  (vout_display_t *vd, picture_t *pic);
static int Control          (vout_display_t *vd, int);

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

typedef struct vout_display_sys_t {

    id<VLCOpenGLVideoViewEmbedding> container;

    VLCVideoLayerView *videoView; // Layer-backed view that creates videoLayer
    VLCCAOpenGLLayer *videoLayer; // Backing layer of videoView

    vlc_gl_t *gl;
    vout_display_opengl_t *vgl;

    vout_display_place_t place;
    vout_display_cfg_t cfg;

    atomic_bool is_ready;
} vout_display_sys_t;

#pragma mark -
#pragma mark OpenGL context helpers

// kCGLRenderer* enum value define card value, but family is enough here.
// However they follow some pattern by familly.
#define kRendererIntelFamilyMask 0x00024000

/*
 * GL API does not provide a way to know if a device is a lowpower one.
 * We could make some guess here:
 * On a MacBookPro (intel):
 *   - GeForce and Radeon card could be discrete or external.
 *   - Intel could be integrated or external.
 * To be check MacPro or MacMini or ARM ones.
 */
static bool vlc_IsLowPowerDevice(GLint renderer_id) {
    int renderer_vendor = renderer_id & kCGLRendererIDMatchingMask;
    // Consider Intel familly card as low power devices.
    return renderer_vendor == kCGLRendererIntel900ID ||
           renderer_vendor == kCGLRendererIntelX3100ID ||
           renderer_vendor == kCGLRendererIntelHDID ||
           renderer_vendor == kCGLRendererIntelHD4000ID ||
           renderer_vendor == kCGLRendererIntelHD5000ID;
}

/*
 * Search for a low power device.
 * Without proper API (like Metal), here we try to look on all the available
 * renderer and filter them.
 * We are looking for the one attached to the main display, accelerated, and
 * with low power consumption.
 * Without any match, we let CGLChoosePixelFormat/CGLCreateContext do as before.
 */
static GLint vlc_SearchGLRendererId() {
    CGLRendererInfoObj renderer_info = NULL;
    GLint renderer_count = 0;
    if (CGLQueryRendererInfo((GLuint)-1, &renderer_info, &renderer_count) !=
        kCGLNoError)
      return -1;

    GLint best_match = -1;
    for (GLint i = 0; i < renderer_count && best_match == -1; ++i) {
      GLint renderer_id = -1;
      if (CGLDescribeRenderer(renderer_info, i, kCGLRPRendererID,
                              &renderer_id) != kCGLNoError)
        break;
      GLint accelerated = 0;
      if (CGLDescribeRenderer(renderer_info, i, kCGLRPAccelerated,
                              &accelerated) != kCGLNoError)
        break;
      if (!accelerated)
        continue; // avoid not accelerated device
      GLint display = -1;
      if (CGLDescribeRenderer(renderer_info, i, kCGLRPDisplayMask, &display) !=
          kCGLNoError)
        break;
      CGDirectDisplayID display_id = CGOpenGLDisplayMaskToDisplayID(display);
      if (display_id != CGMainDisplayID())
        continue;
      if (vlc_IsLowPowerDevice(renderer_id))
        best_match = renderer_id;
    }
    CGLDestroyRendererInfo(renderer_info);
    return best_match;
}

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

    GLint renderer_id = vlc_SearchGLRendererId();

    CGLPixelFormatAttribute attribs[15] = {
        kCGLPFAAllRenderers,
        kCGLPFAAllowOfflineRenderers,
        kCGLPFADoubleBuffer,
        kCGLPFAAccelerated,
        kCGLPFANoRecovery,
        kCGLPFAColorSize, 24,
        kCGLPFAAlphaSize, 8,
        kCGLPFADepthSize, 24,

        // Enable automatic graphics switching support, important on Macs
        // with dedicated GPUs, as it allows to not always use the dedicated
        // GPU which has more power consumption
        kCGLPFASupportsAutomaticGraphicsSwitching,
        0
    };

    // A low power renderer was found, ask to use it.
    if (renderer_id != -1) {
        attribs[12] = kCGLPFARendererID;
        attribs[13] = renderer_id;
        attribs[14] = 0;
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

static int SetViewpoint(vout_display_t *vd, const vlc_viewpoint_t *vp)
{
    vout_display_sys_t *sys = vd->sys;
    if (vlc_gl_MakeCurrent(sys->gl) != VLC_SUCCESS)
        return VLC_EGENERIC;

    int ret = vout_display_opengl_SetViewpoint(sys->vgl, vp);
    vlc_gl_ReleaseCurrent(sys->gl);
    return ret;
}

static const struct vlc_display_operations ops = {
    Close, PictureRender, PictureDisplay, Control, NULL, SetViewpoint, NULL,
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

/*****************************************************************************
 * Open: This function allocates and initializes the OpenGL vout method.
 *****************************************************************************/
static int Open (vout_display_t *vd,
                 video_format_t *fmt, vlc_video_context *context)
{
    vout_display_sys_t *sys;
    if (vd->cfg->window->type != VLC_WINDOW_TYPE_NSOBJECT)
        return VLC_EGENERIC;

    @autoreleasepool {
        vout_display_sys_t *sys;

        vd->sys = sys = vlc_obj_calloc(vd, 1, sizeof(*sys));
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

        id container = vd->cfg->window->handle.nsobject;
        if (!container) {
            msg_Err(vd, "No drawable-nsobject found!");
            goto error;
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

        static const struct vlc_gl_operations gl_ops =
        {
            .make_current = gl_cb_MakeCurrent,
            .release_current = gl_cb_ReleaseCurrent,
            .swap = gl_cb_Swap,
            .get_proc_address = gl_cb_GetProcAddress,
        };
        sys->gl->ops = &gl_ops;
        sys->gl->api_type = VLC_OPENGL;

        struct vlc_gl_sys *glsys = sys->gl->sys = malloc(sizeof(*glsys));
        if (unlikely(!glsys)) {
            Close(vd);
            return VLC_ENOMEM;
        }
        glsys->cgl = cgl_ctx;
        glsys->cgl_prev = NULL;

        dispatch_sync(dispatch_get_main_queue(), ^{
           sys->cfg = *vd->cfg;

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
            } else {
                [sys->videoView release];
                [sys->videoLayer release];
                sys->videoView = nil;
                sys->videoLayer = nil;
            }

            vout_display_PlacePicture(&sys->place, vd->source, &vd->cfg->display);
            // Reverse vertical alignment as the GL tex are Y inverted
            sys->place.y = vd->cfg->display.height - (sys->place.y + sys->place.height);
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

        sys->vgl = vout_display_opengl_New(fmt, &spu_chromas, sys->gl,
                                           &vd->cfg->viewpoint, context);
        vlc_gl_ReleaseCurrent(sys->gl);

        if (sys->vgl == NULL) {
            msg_Err(vd, "Error while initializing OpenGL display");
            goto error;
        }

        vd->info.subpicture_chromas = spu_chromas;

        vd->ops = &ops;

        atomic_init(&sys->is_ready, false);
        return VLC_SUCCESS;

    error:
        Close(vd);
        return VLC_EGENERIC;
    }
}

static void Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    atomic_store(&sys->is_ready, false);
    [sys->videoLayer vlcClose];
    [sys->videoView vlcClose];

    if (sys->vgl && !vlc_gl_MakeCurrent(sys->gl)) {
        vout_display_opengl_Delete(sys->vgl);
        vlc_gl_ReleaseCurrent(sys->gl);
    }


    if (sys->gl) {
        struct vlc_gl_sys *glsys = sys->gl->sys;

        // It should never happen that the context is destroyed and we
        // still have a previous context set, as it would mean non-balanced
        // calls to MakeCurrent/ReleaseCurrent.
        assert(glsys->cgl_prev == NULL);

        CGLReleaseContext(glsys->cgl);
        vlc_object_delete(sys->gl);
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

static void PictureRender (vout_display_t *vd, picture_t *pic,
                           const vlc_render_subpicture *subpicture,
                           vlc_tick_t date)
{
    VLC_UNUSED(date);
    vout_display_sys_t *sys = vd->sys;

    if (vlc_gl_MakeCurrent(sys->gl) == VLC_SUCCESS)
    {
        vout_display_opengl_Prepare(sys->vgl, pic, subpicture);
        vlc_gl_ReleaseCurrent(sys->gl);

        atomic_store(&sys->is_ready, true);
    }
}

static void PictureDisplay (vout_display_t *vd, picture_t *pic)
{
    vout_display_sys_t *sys = vd->sys;
    VLC_UNUSED(pic);


    [sys->videoLayer displayFromVout];
}

static int Control (vout_display_t *vd, int query)
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
                vout_display_cfg_t cfg = *vd->cfg;
                cfg.display.width = sys->cfg.display.width;
                cfg.display.height = sys->cfg.display.height;

                sys->cfg = cfg;

                vout_display_PlacePicture(&sys->place, vd->source, &cfg.display);
                // Reverse vertical alignment as the GL tex are Y inverted
                sys->place.y = cfg.display.height - (sys->place.y + sys->place.height);
            }

            // Note!
            // No viewport or aspect ratio is set here, as that needs to be set
            // when rendering. The viewport is always set to match the layer
            // size by the OS right before the OpenGL render callback, so
            // setting it here has no effect.
            return VLC_SUCCESS;
        }

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
    return YES;
}

/*
 * General properties
 */

- (BOOL)isOpaque
{
    return YES;
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

        vout_display_sys_t *sys = vd->sys;
        struct vlc_gl_sys *glsys = sys->gl->sys;
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
         vout_display_sys_t *sys = _voutDisplay->sys;

        return (atomic_load(&sys->is_ready));
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

            vout_display_PlacePicture(&sys->place, _voutDisplay->source, &sys->cfg.display);
        }

        // Ensure viewport and aspect ratio is correct
        vout_display_opengl_Viewport(sys->vgl, sys->place.x, sys->place.y,
                                     sys->place.width, sys->place.height);
        vout_display_opengl_SetOutputSize(sys->vgl, sys->cfg.display.width, sys->cfg.display.height);

        vout_display_opengl_Display(sys->vgl);
        vlc_gl_ReleaseCurrent(sys->gl);
        vlc_gl_Swap(sys->gl);
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
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_callback_display(Open, 300)

    add_opengl_submodule_renderer()
vlc_module_end()
