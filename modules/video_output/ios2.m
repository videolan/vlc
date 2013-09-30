/*****************************************************************************
 * ios2.m: iOS OpenGL ES 2 provider
 *****************************************************************************
 * Copyright (C) 2001-2013 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
 *          Rémi Denis-Courmont
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *          Eric Petit <titer@m0k.org>
 *
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
#import <QuartzCore/QuartzCore.h>
#import <dlfcn.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_opengl.h>
#include <vlc_dialog.h>
#include "opengl.h"

/**
 * Forward declarations
 */
static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

static picture_pool_t* PicturePool(vout_display_t *vd, unsigned requested_count);
static void PictureRender(vout_display_t* vd, picture_t *pic, subpicture_t *subpicture);
static void PictureDisplay(vout_display_t* vd, picture_t *pic, subpicture_t *subpicture);
static int Control(vout_display_t* vd, int query, va_list ap);

static void *OurGetProcAddress(vlc_gl_t *, const char *);

static int OpenglESClean(vlc_gl_t* gl);
static void OpenglESSwap(vlc_gl_t* gl);

/**
 * Module declaration
 */
vlc_module_begin ()
    set_shortname("iOS vout")
    set_description(N_("iOS OpenGL video output"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout display", 300)
    set_callbacks(Open, Close)

    add_shortcut("vout_ios2")
vlc_module_end ()

@interface VLCOpenGLES2VideoView : UIView {
    vout_display_t *_voutDisplay;
    EAGLContext *_eaglContext;
    GLuint _renderBuffer;
    GLuint _frameBuffer;

    BOOL _bufferNeedReset;
    BOOL _appActive;
}
@property (readwrite) vout_display_t* voutDisplay;
@property (readonly) EAGLContext* eaglContext;
@property (readonly) BOOL isAppActive;

- (void)createBuffers;
- (void)destroyBuffers;
- (void)resetBuffers;
@end

struct vout_display_sys_t
{
    VLCOpenGLES2VideoView *glESView;
    UIView* viewContainer;

    vlc_gl_t gl;
    vout_display_opengl_t *vgl;

    picture_pool_t *picturePool;
    bool has_first_frame;

    vout_display_place_t place;
};

static void *OurGetProcAddress(vlc_gl_t *gl, const char *name)
{
    VLC_UNUSED(gl);

    return dlsym(RTLD_DEFAULT, name);
}

static int Open(vlc_object_t *this)
{
    vout_display_t *vd = (vout_display_t *)this;
    vout_display_sys_t *sys = calloc (1, sizeof(*sys));
    NSAutoreleasePool *autoreleasePool = nil;

    if (!sys)
        return VLC_ENOMEM;

    vd->sys = sys;
    sys->picturePool = NULL;
    sys->gl.sys = NULL;

    autoreleasePool = [[NSAutoreleasePool alloc] init];

    /* get the object we will draw into */
    UIView* viewContainer = var_CreateGetAddress (vd, "drawable-nsobject");
    if (!viewContainer || ![viewContainer isKindOfClass:[UIView class]])
        goto bailout;

    vout_display_DeleteWindow (vd, NULL);

    /* This will be released in Close(), on
     * main thread, after we are done using it. */
    sys->viewContainer = [viewContainer retain];

    /* setup the actual OpenGL ES view */
    sys->glESView = [[VLCOpenGLES2VideoView alloc] initWithFrame:[viewContainer bounds]];

    if (!sys->glESView)
        goto bailout;

    [sys->glESView setVoutDisplay:vd];

    [sys->viewContainer performSelectorOnMainThread:@selector(addSubview:) withObject:sys->glESView waitUntilDone:YES];

    /* Initialize common OpenGL video display */
    sys->gl.lock = OpenglESClean;
    sys->gl.unlock = nil;
    sys->gl.swap = OpenglESSwap;
    sys->gl.getProcAddress = OurGetProcAddress;
    sys->gl.sys = sys;
    const vlc_fourcc_t *subpicture_chromas;
    video_format_t fmt = vd->fmt;

    sys->vgl = vout_display_opengl_New (&vd->fmt, &subpicture_chromas, &sys->gl);
    if (!sys->vgl) {
        sys->gl.sys = NULL;
        goto bailout;
    }

    /* */
    vout_display_info_t info = vd->info;
    info.has_pictures_invalid = false;
    info.has_event_thread = true;
    info.subpicture_chromas = subpicture_chromas;

    /* Setup vout_display_t once everything is fine */
    vd->info = info;

    vd->pool = PicturePool;
    vd->prepare = PictureRender;
    vd->display = PictureDisplay;
    vd->control = Control;

    /* */
    [[NSNotificationCenter defaultCenter] addObserver:sys->glESView selector:@selector(applicationStateChanged:) name:UIApplicationWillResignActiveNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:sys->glESView selector:@selector(applicationStateChanged:) name:UIApplicationDidBecomeActiveNotification object:nil];
    [sys->glESView performSelectorOnMainThread:@selector(reshape) withObject:nil waitUntilDone:YES];

    [autoreleasePool release];
    return VLC_SUCCESS;

bailout:
    [autoreleasePool release];
    Close(this);
    return VLC_EGENERIC;
}

void Close (vlc_object_t *this)
{
    vout_display_t *vd = (vout_display_t *)this;
    vout_display_sys_t *sys = vd->sys;

    [sys->glESView setVoutDisplay:nil];

    var_Destroy (vd, "drawable-nsobject");
    [sys->viewContainer performSelectorOnMainThread:@selector(release) withObject:nil waitUntilDone:NO];
    [sys->glESView performSelectorOnMainThread:@selector(removeFromSuperview) withObject:nil waitUntilDone:NO];

    if (sys->gl.sys != NULL) {
        msg_Dbg(this, "deleting display");
        vout_display_opengl_Delete(sys->vgl);
    }

    [sys->glESView release];

    free(sys);
}

/*****************************************************************************
 * vout display callbacks
 *****************************************************************************/

static int Control(vout_display_t *vd, int query, va_list ap)
{
    vout_display_sys_t *sys = vd->sys;

    switch (query) {
        case VOUT_DISPLAY_CHANGE_FULLSCREEN:
        case VOUT_DISPLAY_CHANGE_WINDOW_STATE:
        case VOUT_DISPLAY_HIDE_MOUSE:
            return VLC_SUCCESS;
        case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        case VOUT_DISPLAY_CHANGE_ZOOM:
        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
        case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
        {
            if (!vd->sys)
                return VLC_EGENERIC;

            NSAutoreleasePool * autoreleasePool = [[NSAutoreleasePool alloc] init];

            const vout_display_cfg_t *cfg;
            const video_format_t *source;
            bool is_forced = false;

            if (query == VOUT_DISPLAY_CHANGE_SOURCE_ASPECT || query == VOUT_DISPLAY_CHANGE_SOURCE_CROP) {
                source = (const video_format_t *)va_arg(ap, const video_format_t *);
                cfg = vd->cfg;
            } else {
                source = &vd->source;
                cfg = (const vout_display_cfg_t*)va_arg(ap, const vout_display_cfg_t *);
                if (query == VOUT_DISPLAY_CHANGE_DISPLAY_SIZE)
                    is_forced = (bool)va_arg(ap, int);
            }

            if (query == VOUT_DISPLAY_CHANGE_DISPLAY_SIZE && is_forced
                && (cfg->display.width != vd->cfg->display.width
                    || cfg->display.height != vd->cfg->display.height))
                return VLC_EGENERIC;

            /* we always use our current frame here, because we have some size constraints
             in the ui vout provider */
            vout_display_cfg_t cfg_tmp = *cfg;
            CGRect bounds;
            bounds = [sys->glESView bounds];

            /* on HiDPI displays, the point bounds don't equal the actual pixel based bounds */
            CGFloat scaleFactor = sys->glESView.contentScaleFactor;
            cfg_tmp.display.width = bounds.size.width * scaleFactor;
            cfg_tmp.display.height = bounds.size.height * scaleFactor;

            vout_display_place_t place;
            vout_display_PlacePicture(&place, source, &cfg_tmp, false);
            @synchronized (sys->glESView) {
                sys->place = place;
            }

            /* For resize, we call glViewport in reshape and not here.
             This has the positive side effect that we avoid erratic sizing as we animate every resize. */
            if (query != VOUT_DISPLAY_CHANGE_DISPLAY_SIZE)
                // x / y are top left corner, but we need the lower left one
                glViewport(place.x, cfg_tmp.display.height - (place.y + place.height), place.width, place.height);

            [autoreleasePool release];
            return VLC_SUCCESS;
        }

        case VOUT_DISPLAY_GET_OPENGL:
        {
            vlc_gl_t **gl = va_arg(ap, vlc_gl_t **);
            *gl = &sys->gl;
            return VLC_SUCCESS;
        }

        case VOUT_DISPLAY_RESET_PICTURES:
            assert (0);
        default:
            msg_Err(vd, "Unknown request %i in iOS ES 2 vout display", query);
            return VLC_EGENERIC;
    }
}

static void PictureDisplay(vout_display_t *vd, picture_t *pic, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    sys->has_first_frame = true;
    if (likely([sys->glESView isAppActive]))
        vout_display_opengl_Display(sys->vgl, &vd->source);

    picture_Release(pic);

    if (subpicture)
        subpicture_Delete(subpicture);
}

static void PictureRender(vout_display_t *vd, picture_t *pic, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    if (likely([sys->glESView isAppActive]))
        vout_display_opengl_Prepare(sys->vgl, pic, subpicture);
}

static picture_pool_t *PicturePool(vout_display_t *vd, unsigned requested_count)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->picturePool)
        sys->picturePool = vout_display_opengl_GetPool(sys->vgl, requested_count);
    assert(sys->picturePool);
    return sys->picturePool;
}

/*****************************************************************************
 * vout opengl callbacks
 *****************************************************************************/
static int OpenglESClean(vlc_gl_t *gl)
{
    vout_display_sys_t *sys = (vout_display_sys_t *)gl->sys;
    if (likely([sys->glESView isAppActive]))
        [sys->glESView resetBuffers];
    return 0;
}

static void OpenglESSwap(vlc_gl_t *gl)
{
    vout_display_sys_t *sys = (vout_display_sys_t *)gl->sys;
    if (likely([sys->glESView isAppActive]))
        [[sys->glESView eaglContext] presentRenderbuffer:GL_RENDERBUFFER];
}

/*****************************************************************************
 * Our UIView object
 *****************************************************************************/
@implementation VLCOpenGLES2VideoView
@synthesize voutDisplay = _voutDisplay, eaglContext = _eaglContext, isAppActive = _appActive;

+ (Class)layerClass
{
    return [CAEAGLLayer class];
}

- (id)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame]; // perform selector on main thread?

    if (!self)
        return nil;

    CAEAGLLayer * layer = (CAEAGLLayer *)self.layer;
    layer.drawableProperties = [NSDictionary dictionaryWithObject:kEAGLColorFormatRGBA8 forKey: kEAGLDrawablePropertyColorFormat];
    layer.opaque = YES;

    _eaglContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    if (!_eaglContext)
        return nil;
    [EAGLContext setCurrentContext:_eaglContext];

    [self performSelectorOnMainThread:@selector(createBuffers) withObject:nil waitUntilDone:YES];
    [self performSelectorOnMainThread:@selector(reshape) withObject:nil waitUntilDone:NO];
    [self setAutoresizingMask: UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];

    _appActive = ([UIApplication sharedApplication].applicationState == UIApplicationStateActive);

    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [_eaglContext release];
    [super dealloc];
}

- (void)didMoveToWindow
{
    self.contentScaleFactor = self.window.screen.scale;
    _bufferNeedReset = YES;
}

- (void)createBuffers
{
    /* make sure the current context is us */
    [EAGLContext setCurrentContext:_eaglContext];

    /* create render buffer */
    glGenRenderbuffers(1, &_renderBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _renderBuffer);

    /* create frame buffer */
    glGenFramebuffers(1, &_frameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, _frameBuffer);

    /* allocate storage for the pixels we are going to to draw to */
    [_eaglContext renderbufferStorage:GL_RENDERBUFFER fromDrawable:(id<EAGLDrawable>)self.layer];

    /* bind render buffer to frame buffer */
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _renderBuffer);

    /* make sure that our shape is ok */
    [self performSelectorOnMainThread:@selector(reshape) withObject:nil waitUntilDone:NO];
}

- (void)destroyBuffers
{
    /* re-set current context */
    [EAGLContext setCurrentContext:_eaglContext];

    /* clear frame buffer */
    glDeleteFramebuffers(1, &_frameBuffer);
    _frameBuffer = 0;

    /* clear render buffer */
    glDeleteRenderbuffers(1, &_renderBuffer);
    _renderBuffer = 0;
}

- (void)resetBuffers
{
    if (_bufferNeedReset) {
        [self destroyBuffers];
        [self createBuffers];
        _bufferNeedReset = NO;
    }
}

- (void)layoutSubviews
{
    /* this method is called as soon as we are resized.
     * so set a variable to re-create our buffers on the next clean event */
    _bufferNeedReset = YES;
}

/**
 * Method called by Cocoa when the view is resized.
 */
- (void)reshape
{
    assert([[NSThread currentThread] isMainThread]);

    CGRect bounds;
    bounds = [self bounds];

    vout_display_place_t place;

    @synchronized(self) {
        if (_voutDisplay) {
            vout_display_cfg_t cfg_tmp = *(_voutDisplay->cfg);
            CGFloat scaleFactor = self.contentScaleFactor;

            cfg_tmp.display.width  = bounds.size.width * scaleFactor;
            cfg_tmp.display.height = bounds.size.height * scaleFactor;

            vout_display_PlacePicture(&place, &_voutDisplay->source, &cfg_tmp, false);
            _voutDisplay->sys->place = place;
            vout_display_SendEventDisplaySize(_voutDisplay, bounds.size.width * scaleFactor, bounds.size.height * scaleFactor, _voutDisplay->cfg->is_fullscreen);
        }
    }

    // x / y are top left corner, but we need the lower left one
    glViewport(place.x, place.y, place.width, place.height);
}

- (void)applicationStateChanged:(NSNotification *)notification
{
    if ([[notification name] isEqualToString:UIApplicationWillResignActiveNotification] || [[notification name] isEqualToString:UIApplicationDidEnterBackgroundNotification] || [[notification name] isEqualToString:UIApplicationWillTerminateNotification])
        _appActive = NO;
    else
        _appActive = YES;
}

- (void)updateConstraints
{
    [self reshape];
    [super updateConstraints];
}

- (BOOL)isOpaque
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

@end
