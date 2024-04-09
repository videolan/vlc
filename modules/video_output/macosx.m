/*****************************************************************************
 * macosx.m: MacOS X OpenGL provider
 *****************************************************************************
 * Copyright (C) 2001-2013 VLC authors and VideoLAN
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
 *          Eric Petit <titer@m0k.org>
 *          Benjamin Pracht <bigben at videolan dot org>
 *          Damien Fouilleul <damienf at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
 *          Rémi Denis-Courmont
 *          Juho Vähä-Herttua <juhovh at iki dot fi>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#import <dlfcn.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_opengl.h>
#include <vlc_dialog.h>
#include "opengl/renderer.h"
#include "opengl/vout_helper.h"

/**
 * Forward declarations
 */
static int Open(vout_display_t *vd,
                video_format_t *fmt, vlc_video_context *context);
static void Close(vout_display_t *vd);

static void PictureRender (vout_display_t *vd, picture_t *pic, const vlc_render_subpicture *subpicture,
                           vlc_tick_t date);
static void PictureDisplay (vout_display_t *vd, picture_t *pic);
static int Control (vout_display_t *vd, int query);

static void *OurGetProcAddress(vlc_gl_t *, const char *);

static int OpenglLock (vlc_gl_t *gl);
static void OpenglUnlock (vlc_gl_t *gl);
static void OpenglSwap (vlc_gl_t *gl);

/**
 * Module declaration
 */
vlc_module_begin ()
    /* Will be loaded even without interface module. see voutgl.m */
    set_shortname ("Mac OS X")
    set_description (N_("Mac OS X OpenGL video output"))
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_callback_display(Open, 290)
    add_shortcut ("macosx", "vout_macosx")
    add_glopts ()

    add_opengl_submodule_renderer()
vlc_module_end ()

/**
 * Obj-C protocol declaration that drawable-nsobject should follow
 */
@protocol VLCVideoViewEmbedding <NSObject>
- (void)addVoutSubview:(NSView *)view;
- (void)removeVoutSubview:(NSView *)view;
@end

@interface VLCOpenGLVideoView : NSOpenGLView
{
    vout_display_t *vd;
}
- (void)setVoutDisplay:(vout_display_t *)vd;
- (void)setVoutFlushing:(BOOL)flushing;
- (void)render;
@end


typedef struct vout_display_sys_t
{
    VLCOpenGLVideoView *glView;
    id<VLCVideoViewEmbedding> container;

    vlc_gl_t *gl;
    vout_display_opengl_t *vgl;

    picture_t *current;
    bool has_first_frame;

    vout_display_cfg_t cfg;
    vout_display_place_t place;
} vout_display_sys_t;

struct gl_sys
{
    CGLContextObj locked_ctx;
    VLCOpenGLVideoView *glView;
};

static void *OurGetProcAddress(vlc_gl_t *gl, const char *name)
{
    VLC_UNUSED(gl);

    return dlsym(RTLD_DEFAULT, name);
}

static int SetViewpoint(vout_display_t *vd, const vlc_viewpoint_t *vp)
{
    vout_display_sys_t *sys = vd->sys;
    return vout_display_opengl_SetViewpoint (sys->vgl, vp);
}

static const struct vlc_display_operations ops = {
    Close, PictureRender, PictureDisplay, Control, NULL, SetViewpoint, NULL,
};

static int Open (vout_display_t *vd,
                 video_format_t *fmt, vlc_video_context *context)
{

    if (vd->cfg->window->type != VLC_WINDOW_TYPE_NSOBJECT)
        return VLC_EGENERIC;

    vout_display_sys_t *sys = vlc_obj_calloc (vd, 1, sizeof(*sys));

    if (!sys)
        return VLC_ENOMEM;
    sys->cfg = *vd->cfg;

    @autoreleasepool {
        if (!CGDisplayUsesOpenGLAcceleration (kCGDirectMainDisplay))
            msg_Err (vd, "no OpenGL hardware acceleration found. this can lead to slow output and unexpected results");

        vd->sys = sys;
        sys->vgl = NULL;
        sys->gl = NULL;

        /* Get the drawable object */
        id container = vd->cfg->window->handle.nsobject;
        assert(container != nil);

        /* This will be released in Close(), on
         * main thread, after we are done using it. */
        sys->container = [container retain];

        /* Get our main view*/
        [VLCOpenGLVideoView performSelectorOnMainThread:@selector(getNewView:)
                                             withObject:[NSValue valueWithPointer:&sys->glView]
                                          waitUntilDone:YES];
        if (!sys->glView) {
            msg_Err(vd, "Initialization of open gl view failed");
            goto error;
        }

        [sys->glView setVoutDisplay:vd];

        /* We don't wait, that means that we'll have to be careful about releasing
         * container.
         * That's why we'll release on main thread in Close(). */
        if ([(id)container respondsToSelector:@selector(addVoutSubview:)])
            [(id)container performSelectorOnMainThread:@selector(addVoutSubview:)
                                            withObject:sys->glView
                                         waitUntilDone:NO];
        else if ([container isKindOfClass:[NSView class]]) {
            NSView *parentView = container;
            [parentView performSelectorOnMainThread:@selector(addSubview:)
                                         withObject:sys->glView
                                      waitUntilDone:NO];
            [sys->glView performSelectorOnMainThread:@selector(setFrameToBoundsOfView:)
                                          withObject:[NSValue valueWithPointer:parentView]
                                       waitUntilDone:NO];
        } else {
            msg_Err(vd, "Invalid drawable-nsobject object. drawable-nsobject must either be an NSView or comply to the @protocol VLCVideoViewEmbedding.");
            goto error;
        }

        /* Initialize common OpenGL video display */
        sys->gl = vlc_object_create(vd, sizeof(*sys->gl));

        if( unlikely( !sys->gl ) )
            goto error;

        struct gl_sys *glsys = sys->gl->sys = malloc(sizeof(struct gl_sys));
        if( unlikely( !sys->gl->sys ) )
        {
            vlc_object_delete(sys->gl);
            goto error;
        }
        glsys->locked_ctx = NULL;
        glsys->glView = sys->glView;

        static const struct vlc_gl_operations gl_ops =
        {
            .make_current = OpenglLock,
            .release_current = OpenglUnlock,
            .swap = OpenglSwap,
            .get_proc_address = OurGetProcAddress,
        };
        sys->gl->ops = &gl_ops;
        sys->gl->api_type = VLC_OPENGL;

        const vlc_fourcc_t *subpicture_chromas;

        if (vlc_gl_MakeCurrent(sys->gl) != VLC_SUCCESS)
        {
            msg_Err(vd, "Can't attach gl context");
            goto error;
        }
        sys->vgl = vout_display_opengl_New (fmt, &subpicture_chromas, sys->gl,
                                            &vd->cfg->viewpoint, context);
        vlc_gl_ReleaseCurrent(sys->gl);
        if (!sys->vgl) {
            msg_Err(vd, "Error while initializing opengl display.");
            goto error;
        }

        /* Setup vout_display_t once everything is fine */
        vd->info.subpicture_chromas = subpicture_chromas;

        vd->ops = &ops;

        return VLC_SUCCESS;

    error:
        Close(vd);
        return VLC_EGENERIC;
    }
}

static void Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    @autoreleasepool {
        [sys->glView setVoutDisplay:nil];

        var_Destroy (vd, "drawable-nsobject");

        if (sys->vgl != NULL)
        {
            vlc_gl_MakeCurrent(sys->gl);
            vout_display_opengl_Delete (sys->vgl);
            vlc_gl_ReleaseCurrent(sys->gl);
        }

        if (sys->gl != NULL)
        {
            assert(((struct gl_sys *)sys->gl->sys)->locked_ctx == NULL);
            free(sys->gl->sys);
            vlc_object_delete(sys->gl);
        }

        VLCOpenGLVideoView *glView = sys->glView;
        id<VLCVideoViewEmbedding> viewContainer = sys->container;
        dispatch_async(dispatch_get_main_queue(), ^{
            if ([viewContainer respondsToSelector:@selector(removeVoutSubview:)]) {
                /* This will retain sys->glView */
                [viewContainer removeVoutSubview:sys->glView];
            }

            /* release on main thread as explained in Open() */
            [viewContainer release];
            [glView removeFromSuperview];
            [glView release];
        });
    }
}

/*****************************************************************************
 * vout display callbacks
 *****************************************************************************/

static void PictureRender (vout_display_t *vd, picture_t *pic,
                           const vlc_render_subpicture *subpicture,
                           vlc_tick_t date)
{
    VLC_UNUSED(date);
    vout_display_sys_t *sys = vd->sys;

    if (vlc_gl_MakeCurrent(sys->gl) == VLC_SUCCESS)
    {
        vout_display_opengl_Prepare (sys->vgl, pic, subpicture);
        vlc_gl_ReleaseCurrent(sys->gl);
    }
}

static void PictureDisplay (vout_display_t *vd, picture_t *pic)
{
    vout_display_sys_t *sys = vd->sys;
    (void)pic;

    @synchronized(sys->glView)
    {
        [sys->glView setVoutFlushing:YES];
        if (vlc_gl_MakeCurrent(sys->gl) == VLC_SUCCESS)
        {
            [sys->glView render];
            vlc_gl_ReleaseCurrent(sys->gl);
            vlc_gl_Swap(sys->gl);
        }
        [sys->glView setVoutFlushing:NO];
    }
    sys->has_first_frame = true;
}

static void UpdatePlace (vout_display_t *vd, const vout_display_cfg_t *cfg)
{
    vout_display_sys_t *sys = vd->sys;
    vout_display_place_t place;
    /* We never receive resize from the core, so provide the size ourselves */
    vout_display_PlacePicture(&place, vd->source, &cfg->display);
    /* Reverse vertical alignment as the GL tex are Y inverted */
    place.y = cfg->display.height - (place.y + place.height);
    sys->place = place;
}

static int Control (vout_display_t *vd, int query)
{
    vout_display_sys_t *sys = vd->sys;

    if (!vd->sys)
        return VLC_EGENERIC;

    @autoreleasepool {
        switch (query)
        {
            /* We handle the resizing ourselves, nothing to report either. */
            case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
                return VLC_SUCCESS;

            case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
            case VOUT_DISPLAY_CHANGE_ZOOM:
            case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
            case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
            {
                @synchronized(sys->glView) {
                    vout_display_cfg_t cfg;
                    cfg = *vd->cfg;
                    cfg.display.width = sys->cfg.display.width;
                    cfg.display.height = sys->cfg.display.height;
                    sys->cfg = cfg;
                    UpdatePlace(vd, &cfg);
                }
                return VLC_SUCCESS;
            }

            default:
                msg_Err (vd, "Unknown request in Mac OS X vout display");
                return VLC_EGENERIC;
        }
    }
}

/*****************************************************************************
 * vout opengl callbacks
 *****************************************************************************/
static int OpenglLock (vlc_gl_t *gl)
{
    struct gl_sys *sys = gl->sys;
    if (![sys->glView respondsToSelector:@selector(openGLContext)])
        return 1;

    assert(sys->locked_ctx == NULL);

    NSOpenGLContext *context = [sys->glView openGLContext];
    CGLContextObj cglcntx = [context CGLContextObj];

    CGLError err = CGLLockContext (cglcntx);
    if (kCGLNoError == err) {
        sys->locked_ctx = cglcntx;
        [context makeCurrentContext];
        return 0;
    }
    return 1;
}

static void OpenglUnlock (vlc_gl_t *gl)
{
    struct gl_sys *sys = gl->sys;
    CGLUnlockContext (sys->locked_ctx);
    sys->locked_ctx = NULL;
}

static void OpenglSwap (vlc_gl_t *gl)
{
    struct gl_sys *sys = gl->sys;
    [[sys->glView openGLContext] flushBuffer];
}

/*****************************************************************************
 * Our NSView object
 *****************************************************************************/
@implementation VLCOpenGLVideoView

#define VLCAssertMainThread() assert([[NSThread currentThread] isMainThread])


+ (void)getNewView:(NSValue *)value
{
    id *ret = [value pointerValue];
    *ret = [[self alloc] init];
}

/**
 * Gets called by the Open() method.
 */
- (id)init
{
    VLCAssertMainThread();

    /* Warning - this may be called on non main thread */

    NSOpenGLPixelFormatAttribute attribs[] =
    {
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAAccelerated,
        NSOpenGLPFANoRecovery,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAAllowOfflineRenderers,
        0
    };

    NSOpenGLPixelFormat *fmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];

    if (!fmt)
        return nil;

    self = [super initWithFrame:NSMakeRect(0,0,10,10) pixelFormat:fmt];
    [fmt release];

    if (!self)
        return nil;

    /* enable HiDPI support */
    [self setWantsBestResolutionOpenGLSurface:YES];

    /* request our screen's HDR mode (introduced in OS X 10.11) */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
    if ([self respondsToSelector:@selector(setWantsExtendedDynamicRangeOpenGLSurface:)]) {
        [self setWantsExtendedDynamicRangeOpenGLSurface:YES];
    }
#pragma clang diagnostic pop

    /* Swap buffers only during the vertical retrace of the monitor.
     http://developer.apple.com/documentation/GraphicsImaging/
     Conceptual/OpenGL/chap5/chapter_5_section_44.html */
    GLint params[] = { 1 };
    CGLSetParameter ([[self openGLContext] CGLContextObj], kCGLCPSwapInterval, params);

    [NSNotificationCenter.defaultCenter addObserverForName:NSApplicationDidChangeScreenParametersNotification
                                                      object:NSApplication.sharedApplication
                                                       queue:nil
                                                  usingBlock:^(NSNotification *notification) {
                                                      [self performSelectorOnMainThread:@selector(reshape)
                                                                             withObject:nil
                                                                          waitUntilDone:NO];
                                                  }];

    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    return self;
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
    [super dealloc];
}

/**
 * Gets called by the Open() method.
 */
- (void)setFrameToBoundsOfView:(NSValue *)value
{
    NSView *parentView = [value pointerValue];
    [self setFrame:[parentView bounds]];
}

/**
 * Gets called by the Close and Open methods.
 * (Non main thread).
 */
- (void)setVoutDisplay:(vout_display_t *)aVd
{
    @synchronized(self) {
        vd = aVd;
    }
}

/**
 * Gets called when the vout will acquire the lock and flush.
 * (Non main thread).
 */
- (void)setVoutFlushing:(BOOL)flushing
{
    if (!flushing)
        return;
}

/**
 * Local method that locks the gl context.
 */
- (BOOL)lockgl
{
    VLCAssertMainThread();
    NSOpenGLContext *context = [self openGLContext];
    CGLError err = CGLLockContext ([context CGLContextObj]);
    if (err == kCGLNoError)
        [context makeCurrentContext];
    return err == kCGLNoError;
}

/**
 * Local method that unlocks the gl context.
 */
- (void)unlockgl
{
    VLCAssertMainThread();
    CGLUnlockContext ([[self openGLContext] CGLContextObj]);
}

/**
 * Local method that force a rendering of a frame.
 * This will get called if Cocoa forces us to redraw (via -drawRect).
 *
 * NOTE: must be called in a @synchronized() block with sys->glView or self.
 */
- (void)render
{
    if (!vd) {
        glClear (GL_COLOR_BUFFER_BIT);
        return;
    }

    vout_display_sys_t *sys = vd->sys;

    vout_display_opengl_Viewport(sys->vgl, sys->place.x, sys->place.y,
                                 sys->place.width, sys->place.height);
    vout_display_opengl_SetOutputSize(sys->vgl, sys->cfg.display.width, sys->cfg.display.height);

    if (sys->has_first_frame)
        vout_display_opengl_Display (sys->vgl);
    else
        glClear (GL_COLOR_BUFFER_BIT);
    vlc_gl_Swap(sys->gl);
}

/**
 * Method called by Cocoa when the view is resized.
 */
- (void)reshape
{
    VLCAssertMainThread();

    /* on HiDPI displays, the point bounds don't equal the actual pixel based bounds */
    NSRect bounds = [self convertRectToBacking:[self bounds]];

    @synchronized(self) {
        if (vd == NULL) return;
        vout_display_sys_t *sys = vd->sys;
        sys->cfg.display.width  = bounds.size.width;
        sys->cfg.display.height = bounds.size.height;
        UpdatePlace(vd, &sys->cfg);
    }
    [super reshape];
}

/**
 * Method called by Cocoa when the view is resized or the location has changed.
 * We just need to make sure we are locking here.
 */
- (void)update
{
    VLCAssertMainThread();
    BOOL success = [self lockgl];
    if (!success)
        return;

    [super update];

    [self unlockgl];
}

/**
 * Method called by Cocoa to force redraw.
 */
- (void)drawRect:(NSRect) rect
{
    VLCAssertMainThread();

    @synchronized(self) {
        BOOL success = [self lockgl];
        if (!success)
            return;

        [self render];
        [[self openGLContext] flushBuffer];
        [self unlockgl];
    }
}

- (void)renewGState
{
    // Comment take from Apple GLEssentials sample code:
    // https://developer.apple.com/library/content/samplecode/GLEssentials
    //
    // OpenGL rendering is not synchronous with other rendering on the OSX.
    // Therefore, call disableScreenUpdatesUntilFlush so the window server
    // doesn't render non-OpenGL content in the window asynchronously from
    // OpenGL content, which could cause flickering.  (non-OpenGL content
    // includes the title bar and drawing done by the app with other APIs)

    // In macOS 10.13 and later, window updates are automatically batched
    // together and this no longer needs to be called (effectively a no-op)
    [[self window] disableScreenUpdatesUntilFlush];

    [super renewGState];
}

- (BOOL)isOpaque
{
    return YES;
}

#pragma mark -
#pragma mark Mouse handling

#warning Missing mouse handling, must be implemented in Vout Window

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)mouseDownCanMoveWindow
{
    return YES;
}

@end
