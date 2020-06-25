/*****************************************************************************
 * caopengllayer.m: CAOpenGLLayer (Mac OS X) video output
 *****************************************************************************
 * Copyright (C) 2014-2017 VLC authors and VideoLAN
 *
 * Authors: David Fuhrmann <david dot fuhrmann at googlemail dot com>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont at videolan dot org>
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

#import <QuartzCore/QuartzCore.h>
#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#import <dlfcn.h>               /* dlsym */

#include "opengl/vout_helper.h"

#define OSX_SIERRA_AND_HIGHER (NSAppKitVersionNumber >= 1485)

/*****************************************************************************
 * Vout interface
 *****************************************************************************/
static int Open(vout_display_t *vd, const vout_display_cfg_t *cfg,
                video_format_t *fmt, vlc_video_context *context);
static void Close(vout_display_t *vd);

vlc_module_begin()
    set_description(N_("Core Animation OpenGL Layer (Mac OS X)"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_callback_display(Open, 0)
vlc_module_end()

static void PictureRender   (vout_display_t *vd, picture_t *pic, subpicture_t *subpicture,
                             vlc_tick_t date);
static void PictureDisplay  (vout_display_t *vd, picture_t *pic);
static int Control          (vout_display_t *vd, int query, va_list ap);

static void *OurGetProcAddress (vlc_gl_t *gl, const char *name);
static int OpenglLock         (vlc_gl_t *gl);
static void OpenglUnlock       (vlc_gl_t *gl);
static void OpenglSwap         (vlc_gl_t *gl);

@protocol VLCCoreAnimationVideoLayerEmbedding <NSObject>
- (void)addVoutLayer:(CALayer *)aLayer;
- (void)removeVoutLayer:(CALayer *)aLayer;
- (CGSize)currentOutputSize;
@end

@interface VLCCAOpenGLLayer : CAOpenGLLayer

@property (nonatomic, readwrite) vout_display_t* voutDisplay;
@property (nonatomic, readwrite) CGLContextObj glContext;

@end


struct vout_display_sys_t {

    CALayer <VLCCoreAnimationVideoLayerEmbedding> *container;
    vout_window_t *embed;
    VLCCAOpenGLLayer *cgLayer;

    vlc_gl_t *gl;
    vout_display_opengl_t *vgl;

    vout_display_place_t place;

    bool  b_frame_available;
};

struct gl_sys
{
    CGLContextObj locked_ctx;
    VLCCAOpenGLLayer *cgLayer;
};

/*****************************************************************************
 * Open: This function allocates and initializes the OpenGL vout method.
 *****************************************************************************/
static int Open (vout_display_t *vd, const vout_display_cfg_t *cfg,
                 video_format_t *fmt, vlc_video_context *context)
{
    vout_display_sys_t *sys;

    if (cfg->window->type != VOUT_WINDOW_TYPE_NSOBJECT)
        return VLC_EGENERIC;

    /* Allocate structure */
    vd->sys = sys = calloc(1, sizeof(vout_display_sys_t));
    if (sys == NULL)
        return VLC_EGENERIC;

    @autoreleasepool {
        id container = var_CreateGetAddress(vd, "drawable-nsobject");
        if (!container) {
            sys->embed = cfg->window;
            container = sys->embed->handle.nsobject;

            if (!container) {
                msg_Err(vd, "No drawable-nsobject found!");
                goto bailout;
            }
        }

        /* store for later, released in Close() */
        sys->container = [container retain];

        [CATransaction begin];
        sys->cgLayer = [[VLCCAOpenGLLayer alloc] init];
        [sys->cgLayer setVoutDisplay:vd];

        [sys->cgLayer performSelectorOnMainThread:@selector(display)
                                       withObject:nil
                                    waitUntilDone:YES];

        if ([container respondsToSelector:@selector(addVoutLayer:)]) {
            msg_Dbg(vd, "container implements implicit protocol");
            [container addVoutLayer:sys->cgLayer];
        } else if ([container respondsToSelector:@selector(addSublayer:)] ||
                   [container isKindOfClass:[CALayer class]]) {
            msg_Dbg(vd, "container doesn't implement implicit protocol, fallback mode used");
            [container addSublayer:sys->cgLayer];
        } else {
            msg_Err(vd, "Provided NSObject container isn't compatible");
            [sys->cgLayer release];
            sys->cgLayer = nil;
            [CATransaction commit];
            goto bailout;
        }
        [CATransaction commit];

        if (!sys->cgLayer)
            goto bailout;

        if (![sys->cgLayer glContext])
            msg_Warn(vd, "we might not have an OpenGL context yet");

        /* Initialize common OpenGL video display */
        sys->gl = vlc_object_create(vd, sizeof(*sys->gl));
        if (unlikely(!sys->gl))
            goto bailout;
        sys->gl->make_current = OpenglLock;
        sys->gl->release_current = OpenglUnlock;
        sys->gl->swap = OpenglSwap;
        sys->gl->get_proc_address = OurGetProcAddress;

        struct gl_sys *glsys = sys->gl->sys = malloc(sizeof(*glsys));
        if (!sys->gl->sys)
            goto bailout;
        glsys->locked_ctx = NULL;
        glsys->cgLayer = sys->cgLayer;

        const vlc_fourcc_t *subpicture_chromas;
        if (!OpenglLock(sys->gl)) {
            sys->vgl = vout_display_opengl_New(fmt, &subpicture_chromas,
                                               sys->gl, &cfg->viewpoint, context);
            OpenglUnlock(sys->gl);
        } else
            sys->vgl = NULL;
        if (!sys->vgl) {
            msg_Err(vd, "Error while initializing opengl display.");
            goto bailout;
        }

        /* setup vout display */
        vd->info.subpicture_chromas = subpicture_chromas;

        vd->prepare = PictureRender;
        vd->display = PictureDisplay;
        vd->control = Control;
        vd->close   = Close;

        if (OSX_SIERRA_AND_HIGHER) {
            /* request our screen's HDR mode (introduced in OS X 10.11, but correctly supported in 10.12 only) */
            if ([sys->cgLayer respondsToSelector:@selector(setWantsExtendedDynamicRangeContent:)]) {
                [sys->cgLayer setWantsExtendedDynamicRangeContent:YES];
            }
        }

        /* setup initial state */
        CGSize outputSize;
        if ([container respondsToSelector:@selector(currentOutputSize)])
            outputSize = [container currentOutputSize];
        else
            outputSize = [sys->container visibleRect].size;
        vout_window_ReportSize(sys->embed, (int)outputSize.width, (int)outputSize.height);

        return VLC_SUCCESS;

    bailout:
        Close(vd);
        return VLC_EGENERIC;
    }
}

static void Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->cgLayer) {
        if ([sys->container respondsToSelector:@selector(removeVoutLayer:)])
            [sys->container removeVoutLayer:sys->cgLayer];
        else
            [sys->cgLayer removeFromSuperlayer];

        if ([sys->cgLayer glContext])
            CGLReleaseContext([sys->cgLayer glContext]);

        [sys->cgLayer release];
    }

    if (sys->container)
        [sys->container release];

    if (sys->vgl != NULL && !OpenglLock(sys->gl)) {
        vout_display_opengl_Delete(sys->vgl);
        OpenglUnlock(sys->gl);
    }

    if (sys->gl != NULL)
    {
        if (sys->gl->sys != NULL)
        {
            assert(((struct gl_sys *)sys->gl->sys)->locked_ctx == NULL);
            free(sys->gl->sys);
        }
        vlc_object_delete(sys->gl);
    }

    free(sys);
}

static void PictureRender (vout_display_t *vd, picture_t *pic, subpicture_t *subpicture,
                           vlc_tick_t date)
{
    VLC_UNUSED(date);
    vout_display_sys_t *sys = vd->sys;

    if (pic == NULL) {
        msg_Warn(vd, "invalid pic, skipping frame");
        return;
    }

    @synchronized (sys->cgLayer) {
        if (!OpenglLock(sys->gl)) {
            vout_display_opengl_Prepare(sys->vgl, pic, subpicture);
            OpenglUnlock(sys->gl);
        }
    }
}

static void PictureDisplay (vout_display_t *vd, picture_t *pic)
{
    vout_display_sys_t *sys = vd->sys;
    VLC_UNUSED(pic);

    @synchronized (sys->cgLayer) {
        sys->b_frame_available = YES;

        /* Calling display on the non-main thread is not officially supported, but
         * its suggested at several places and works fine here. Flush is thread-safe
         * and makes sure the picture is actually displayed. */
        [sys->cgLayer display];
        [CATransaction flush];
    }
}

static int Control (vout_display_t *vd, int query, va_list ap)
{
    vout_display_sys_t *sys = vd->sys;

    if (!vd->sys)
        return VLC_EGENERIC;

    switch (query)
    {
        case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
        case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        case VOUT_DISPLAY_CHANGE_ZOOM:
        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
        {
            const vout_display_cfg_t *cfg =
                va_arg (ap, const vout_display_cfg_t *);

            /* we always use our current frame here */
            vout_display_cfg_t cfg_tmp = *cfg;
            [CATransaction lock];
            CGRect bounds = [sys->cgLayer visibleRect];
            [CATransaction unlock];
            cfg_tmp.display.width = bounds.size.width;
            cfg_tmp.display.height = bounds.size.height;

            /* Reverse vertical alignment as the GL tex are Y inverted */
            if (cfg_tmp.align.vertical == VLC_VIDEO_ALIGN_TOP)
                cfg_tmp.align.vertical = VLC_VIDEO_ALIGN_BOTTOM;
            else if (cfg_tmp.align.vertical == VLC_VIDEO_ALIGN_BOTTOM)
                cfg_tmp.align.vertical = VLC_VIDEO_ALIGN_TOP;

            vout_display_place_t place;
            vout_display_PlacePicture(&place, &vd->source, &cfg_tmp);
            if (unlikely(OpenglLock(sys->gl)))
                // don't return an error or we need to handle VOUT_DISPLAY_RESET_PICTURES
                return VLC_SUCCESS;

            vout_display_opengl_SetWindowAspectRatio(sys->vgl, (float)place.width / place.height);
            OpenglUnlock(sys->gl);

            sys->place = place;

            return VLC_SUCCESS;
        }

        case VOUT_DISPLAY_CHANGE_VIEWPOINT:
        {
            int ret;

            if (OpenglLock(sys->gl))
                return VLC_EGENERIC;

            ret = vout_display_opengl_SetViewpoint(sys->vgl,
                &va_arg (ap, const vout_display_cfg_t* )->viewpoint);
            OpenglUnlock(sys->gl);
            return ret;
        }

        case VOUT_DISPLAY_RESET_PICTURES:
            vlc_assert_unreachable ();
        default:
            msg_Err (vd, "Unhandled request %d", query);
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

#pragma mark -
#pragma mark OpenGL callbacks

static int OpenglLock (vlc_gl_t *gl)
{
    struct gl_sys *sys = gl->sys;
    assert(sys->locked_ctx == NULL);

    CGLContextObj ctx = [sys->cgLayer glContext];
    if(!ctx) {
        return 1;
    }

    CGLError err = CGLLockContext(ctx);
    if (kCGLNoError == err) {
        sys->locked_ctx = ctx;
        CGLSetCurrentContext(ctx);
        return 0;
    }
    return 1;
}

static void OpenglUnlock (vlc_gl_t *gl)
{
    struct gl_sys *sys = gl->sys;
    CGLUnlockContext(sys->locked_ctx);
    sys->locked_ctx = NULL;
}

static void OpenglSwap (vlc_gl_t *gl)
{
    glFlush();
}

static void *OurGetProcAddress (vlc_gl_t *gl, const char *name)
{
    VLC_UNUSED(gl);

    return dlsym(RTLD_DEFAULT, name);
}

#pragma mark -
#pragma mark CA layer

/*****************************************************************************
 * @implementation VLCCAOpenGLLayer
 *****************************************************************************/
@implementation VLCCAOpenGLLayer

- (id)init {

    self = [super init];
    if (self) {
        [CATransaction lock];
        self.needsDisplayOnBoundsChange = YES;
        self.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
        self.asynchronous = NO;
        [CATransaction unlock];
    }

    return self;
}

- (void)setVoutDisplay:(vout_display_t *)aVd
{
    _voutDisplay = aVd;
}

- (void)resizeWithOldSuperlayerSize:(CGSize)size
{
    [super resizeWithOldSuperlayerSize: size];

    CGSize boundsSize = self.visibleRect.size;

    if (_voutDisplay)
    {
        vout_display_sys_t *sys = _voutDisplay->sys;
        vout_window_ReportSize(sys->embed, boundsSize.width, boundsSize.height);
    }
}

- (BOOL)canDrawInCGLContext:(CGLContextObj)glContext pixelFormat:(CGLPixelFormatObj)pixelFormat forLayerTime:(CFTimeInterval)timeInterval displayTime:(const CVTimeStamp *)timeStamp
{
    /* Only draw the frame if we have a frame that was previously rendered */
    if (!_voutDisplay)
        return false;

    return _voutDisplay->sys->b_frame_available;
}

- (void)drawInCGLContext:(CGLContextObj)glContext pixelFormat:(CGLPixelFormatObj)pixelFormat forLayerTime:(CFTimeInterval)timeInterval displayTime:(const CVTimeStamp *)timeStamp
{
    if (!_voutDisplay)
        return;
    vout_display_sys_t *sys = _voutDisplay->sys;

    if (!sys->vgl)
        return;

    CGRect bounds = [self visibleRect];

    // x / y are top left corner, but we need the lower left one
    vout_display_opengl_Viewport(sys->vgl, sys->place.x,
                                 bounds.size.height - (sys->place.y + sys->place.height),
                                 sys->place.width, sys->place.height);

    // flush is also done by this method, no need to call super
    vout_display_opengl_Display(sys->vgl);
    sys->b_frame_available = NO;
}

-(CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask
{
    // The default is fine for this demonstration.
    return [super copyCGLPixelFormatForDisplayMask:mask];
}

- (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pixelFormat
{
    // Only one opengl context is allowed for the module lifetime
    if(_glContext) {
        msg_Dbg(_voutDisplay, "Return existing context: %p", _glContext);
        return _glContext;
    }

    CGLContextObj context = [super copyCGLContextForPixelFormat:pixelFormat];

    // Swap buffers only during the vertical retrace of the monitor.
    //http://developer.apple.com/documentation/GraphicsImaging/
    //Conceptual/OpenGL/chap5/chapter_5_section_44.html /

    GLint params = 1;
    CGLSetParameter( CGLGetCurrentContext(), kCGLCPSwapInterval,
                     &params );

    @synchronized (self) {
        _glContext = context;
    }

    return context;
}

- (void)releaseCGLContext:(CGLContextObj)glContext
{
    // do not release anything here, we do that when closing the module
}

- (void)mouseButtonDown:(int)buttonNumber
{
    @synchronized (self) {
        if (_voutDisplay) {
            if (buttonNumber == 0)
                vout_display_SendEventMousePressed (_voutDisplay, MOUSE_BUTTON_LEFT);
            else if (buttonNumber == 1)
                vout_display_SendEventMousePressed (_voutDisplay, MOUSE_BUTTON_RIGHT);
            else
                vout_display_SendEventMousePressed (_voutDisplay, MOUSE_BUTTON_CENTER);
        }
    }
}

- (void)mouseButtonUp:(int)buttonNumber
{
    @synchronized (self) {
        if (_voutDisplay) {
            if (buttonNumber == 0)
                vout_display_SendEventMouseReleased (_voutDisplay, MOUSE_BUTTON_LEFT);
            else if (buttonNumber == 1)
                vout_display_SendEventMouseReleased (_voutDisplay, MOUSE_BUTTON_RIGHT);
            else
                vout_display_SendEventMouseReleased (_voutDisplay, MOUSE_BUTTON_CENTER);
        }
    }
}

- (void)mouseMovedToX:(double)xValue Y:(double)yValue
{
    @synchronized (self) {
        if (_voutDisplay) {
            vout_display_SendMouseMovedDisplayCoordinates (_voutDisplay,
                                                           xValue,
                                                           yValue);
        }
    }
}

@end
