/*****************************************************************************
 * caopengllayer.m: CAOpenGLLayer (Mac OS X) video output
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
 * $Id$
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

#include "opengl.h"

/*****************************************************************************
 * Vout interface
 *****************************************************************************/
static int  Open   (vlc_object_t *);
static void Close  (vlc_object_t *);

vlc_module_begin()
    set_description(N_("Core Animation OpenGL Layer (Mac OS X)"))
    set_capability("vout display", 0)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_callbacks(Open, Close)
vlc_module_end()


static picture_pool_t *Pool (vout_display_t *vd, unsigned requested_count);
static void PictureRender   (vout_display_t *vd, picture_t *pic, subpicture_t *subpicture);
static void PictureDisplay  (vout_display_t *vd, picture_t *pic, subpicture_t *subpicture);
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

@interface VLCCAOpenGLLayer : CAOpenGLLayer {
    vout_display_t *_vd;
}

- (void)setVoutDisplay:(vout_display_t *)aVd;
@end


struct vout_display_sys_t {

    picture_pool_t *pool;
    picture_resource_t resource;

    CALayer <VLCCoreAnimationVideoLayerEmbedding> *container;
    vout_window_t *embed;
    VLCCAOpenGLLayer *cgLayer;

    CGLContextObj glContext;

    vlc_gl_t gl;
    vout_display_opengl_t *vgl;

    vout_display_place_t place;

    bool  b_frame_available;
};

/*****************************************************************************
 * Open: This function allocates and initializes the OpenGL vout method.
 *****************************************************************************/
static int Open (vlc_object_t *p_this)
{
    vout_display_t *vd = (vout_display_t *)p_this;
    vout_display_sys_t *sys;

    /* Allocate structure */
    vd->sys = sys = calloc(1, sizeof(vout_display_sys_t));
    if (sys == NULL)
        return VLC_EGENERIC;

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    id container = var_CreateGetAddress(vd, "drawable-nsobject");
    if (container)
        vout_display_DeleteWindow(vd, NULL);
    else {
        vout_window_cfg_t wnd_cfg;

        memset(&wnd_cfg, 0, sizeof(wnd_cfg));
        wnd_cfg.type = VOUT_WINDOW_TYPE_NSOBJECT;
        wnd_cfg.x = var_InheritInteger(vd, "video-x");
        wnd_cfg.y = var_InheritInteger(vd, "video-y");
        wnd_cfg.height = vd->cfg->display.height;
        wnd_cfg.width = vd->cfg->display.width;

        sys->embed = vout_display_NewWindow(vd, &wnd_cfg);
        if (sys->embed)
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

    [sys->cgLayer performSelectorOnMainThread:@selector(display) withObject:nil waitUntilDone:YES];

    if ([container respondsToSelector:@selector(addVoutLayer:)]) {
        msg_Dbg(vd, "container implements implicit protocol");
        [container addVoutLayer:sys->cgLayer];
    } else if ([container respondsToSelector:@selector(addSublayer:)] || [container isKindOfClass:[CALayer class]]) {
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

    if (!sys->glContext)
        msg_Warn(vd, "we might not have an OpenGL context yet");

    /* Initialize common OpenGL video display */
    sys->gl.lock = OpenglLock;
    sys->gl.unlock = OpenglUnlock;
    sys->gl.swap = OpenglSwap;
    sys->gl.getProcAddress = OurGetProcAddress;
    sys->gl.sys = sys;

    const vlc_fourcc_t *subpicture_chromas;
    video_format_t fmt = vd->fmt;
    sys->vgl = vout_display_opengl_New(&vd->fmt, &subpicture_chromas, &sys->gl);
    if (!sys->vgl) {
        msg_Err(vd, "Error while initializing opengl display.");
        sys->gl.sys = NULL;
        goto bailout;
    }

    /* setup vout display */
    vout_display_info_t info = vd->info;
    info.subpicture_chromas = subpicture_chromas;
    info.has_hide_mouse = true;
    vd->info = info;

    vd->pool    = Pool;
    vd->prepare = PictureRender;
    vd->display = PictureDisplay;
    vd->control = Control;

    /* setup initial state */
    CGSize outputSize;
    if ([container respondsToSelector:@selector(currentOutputSize)])
        outputSize = [container currentOutputSize];
    else
        outputSize = [sys->container visibleRect].size;
    vout_display_SendEventFullscreen(vd, false);
    vout_display_SendEventDisplaySize(vd, (int)outputSize.width, (int)outputSize.height, false);

    [pool release];
    return VLC_SUCCESS;

bailout:
    [pool release];
    Close(p_this);
    return VLC_EGENERIC;
}

static void Close (vlc_object_t *p_this)
{
    vout_display_t *vd = (vout_display_t *)p_this;
    vout_display_sys_t *sys = vd->sys;

    if (sys->cgLayer) {
        if ([sys->container respondsToSelector:@selector(removeVoutLayer:)])
            [sys->container removeVoutLayer:sys->cgLayer];
        else
            [sys->cgLayer removeFromSuperlayer];
        [sys->cgLayer release];
    }

    if (sys->container)
        [sys->container release];

    if (sys->embed)
        vout_display_DeleteWindow(vd, sys->embed);

    if (sys->gl.sys != NULL)
        vout_display_opengl_Delete(sys->vgl);

    if (sys->glContext)
        CGLReleaseContext(sys->glContext);

    free(sys);
}

static picture_pool_t *Pool (vout_display_t *vd, unsigned count)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->pool)
        sys->pool = vout_display_opengl_GetPool(sys->vgl, count);
    assert(sys->pool);
    return sys->pool;
}

static void PictureRender (vout_display_t *vd, picture_t *pic, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    if (pic == NULL) {
        msg_Warn(vd, "invalid pic, skipping frame");
        return;
    }

    @synchronized (sys->cgLayer) {
        vout_display_opengl_Prepare(sys->vgl, pic, subpicture);
    }
}

static void PictureDisplay (vout_display_t *vd, picture_t *pic, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    @synchronized (sys->cgLayer) {
        sys->b_frame_available = YES;

        /* Calling display on the non-main thread is not officially supported, but
         * its suggested at several places and works fine here. Flush is thread-safe
         * and makes sure the picture is actually displayed. */
        [sys->cgLayer display];
        [CATransaction flush];
    }

    picture_Release(pic);

    if (subpicture)
        subpicture_Delete(subpicture);
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
        case VOUT_DISPLAY_CHANGE_FULLSCREEN:
        {
            const vout_display_cfg_t *cfg;
            const video_format_t *source;

            if (query == VOUT_DISPLAY_CHANGE_SOURCE_ASPECT || query == VOUT_DISPLAY_CHANGE_SOURCE_CROP) {
                source = (const video_format_t *)va_arg (ap, const video_format_t *);
                cfg = vd->cfg;
            } else {
                source = &vd->source;
                cfg = (const vout_display_cfg_t*)va_arg (ap, const vout_display_cfg_t *);
            }

            /* we always use our current frame here */
            vout_display_cfg_t cfg_tmp = *cfg;
            [CATransaction lock];
            CGRect bounds = [sys->cgLayer bounds];
            [CATransaction unlock];
            cfg_tmp.display.width = bounds.size.width;
            cfg_tmp.display.height = bounds.size.height;

            vout_display_place_t place;
            vout_display_PlacePicture (&place, source, &cfg_tmp, false);
            sys->place = place;

            return VLC_SUCCESS;
        }

        case VOUT_DISPLAY_HIDE_MOUSE:
        {
            [NSCursor setHiddenUntilMouseMoves: YES];
            return VLC_SUCCESS;
        }

        case VOUT_DISPLAY_GET_OPENGL:
        {
            vlc_gl_t **gl = va_arg (ap, vlc_gl_t **);
            *gl = &sys->gl;
            return VLC_SUCCESS;
        }
        case VOUT_DISPLAY_CHANGE_WINDOW_STATE:
        {
            return VLC_SUCCESS;
        }

        case VOUT_DISPLAY_RESET_PICTURES:
            assert (0);
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
    vout_display_sys_t *sys = (vout_display_sys_t *)gl->sys;

    if(!sys->glContext) {
        return 1;
    }

    CGLError err = CGLLockContext(sys->glContext);
    if (kCGLNoError == err) {
        CGLSetCurrentContext(sys->glContext);
        return 0;
    }
    return 1;
}

static void OpenglUnlock (vlc_gl_t *gl)
{
    vout_display_sys_t *sys = (vout_display_sys_t *)gl->sys;

    if (!sys->glContext) {
        return;
    }

    CGLUnlockContext(sys->glContext);
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
        [self setAutoresizingMask: kCALayerWidthSizable | kCALayerHeightSizable];
        self.asynchronous = NO;
        [CATransaction unlock];
    }

    return self;
}

- (void)setVoutDisplay:(vout_display_t *)aVd
{
    _vd = aVd;
}

- (void)resizeWithOldSuperlayerSize:(CGSize)size
{
    [super resizeWithOldSuperlayerSize: size];

    CGSize boundsSize = self.bounds.size;
    if (_vd)
        vout_display_SendEventDisplaySize(_vd, boundsSize.width, boundsSize.height, _vd->cfg->is_fullscreen);
}

- (BOOL)canDrawInCGLContext:(CGLContextObj)glContext pixelFormat:(CGLPixelFormatObj)pixelFormat forLayerTime:(CFTimeInterval)timeInterval displayTime:(const CVTimeStamp *)timeStamp
{
    /* Only draw the frame if we have a frame that was previously rendered */
    if (!_vd)
        return false;

    return _vd->sys->b_frame_available;
}

- (void)drawInCGLContext:(CGLContextObj)glContext pixelFormat:(CGLPixelFormatObj)pixelFormat forLayerTime:(CFTimeInterval)timeInterval displayTime:(const CVTimeStamp *)timeStamp
{
    if (!_vd)
        return;
    vout_display_sys_t *sys = _vd->sys;

    if (!sys->vgl)
        return;

    CGRect bounds = [self bounds];
    // x / y are top left corner, but we need the lower left one
    glViewport (sys->place.x, bounds.size.height - (sys->place.y + sys->place.height), sys->place.width, sys->place.height);

    // flush is also done by this method, no need to call super
    vout_display_opengl_Display (sys->vgl, &_vd->source);
    sys->b_frame_available = NO;
}

- (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pixelFormat
{
    // Only one opengl context is allowed for the module lifetime
    if(_vd->sys->glContext) {
        msg_Dbg(_vd, "Return existing context: %p", _vd->sys->glContext);
        return _vd->sys->glContext;
    }

    CGLContextObj context = [super copyCGLContextForPixelFormat:pixelFormat];

    // Swap buffers only during the vertical retrace of the monitor.
    //http://developer.apple.com/documentation/GraphicsImaging/
    //Conceptual/OpenGL/chap5/chapter_5_section_44.html /

    GLint params = 1;
    CGLSetParameter( CGLGetCurrentContext(), kCGLCPSwapInterval,
                     &params );

    @synchronized (self) {
        _vd->sys->glContext = context;
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
        if (_vd) {
            if (buttonNumber == 0)
                vout_display_SendEventMousePressed (_vd, MOUSE_BUTTON_LEFT);
            else if (buttonNumber == 1)
                vout_display_SendEventMousePressed (_vd, MOUSE_BUTTON_RIGHT);
            else
                vout_display_SendEventMousePressed (_vd, MOUSE_BUTTON_CENTER);
        }
    }
}

- (void)mouseButtonUp:(int)buttonNumber
{
    @synchronized (self) {
        if (_vd) {
            if (buttonNumber == 0)
                vout_display_SendEventMouseReleased (_vd, MOUSE_BUTTON_LEFT);
            else if (buttonNumber == 1)
                vout_display_SendEventMouseReleased (_vd, MOUSE_BUTTON_RIGHT);
            else
                vout_display_SendEventMouseReleased (_vd, MOUSE_BUTTON_CENTER);
        }
    }
}

- (void)mouseMovedToX:(double)xValue Y:(double)yValue
{
    @synchronized (self) {
        if (_vd) {
            vout_display_SendMouseMovedDisplayCoordinates (_vd,
                                                           ORIENT_NORMAL,
                                                           xValue,
                                                           yValue,
                                                           &_vd->sys->place);
        }
    }
}

@end
