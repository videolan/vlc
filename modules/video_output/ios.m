/*****************************************************************************
 * ios.m: iOS OpenGL ES provider
 *****************************************************************************
 * Copyright (C) 2001-2017 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
 *          Rémi Denis-Courmont
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *          Eric Petit <titer@m0k.org>
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
#import "opengl/vout_helper.h"

/**
 * Forward declarations
 */

struct picture_sys_t {
    CVPixelBufferRef pixelBuffer;
};

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

static picture_pool_t* PicturePool(vout_display_t *, unsigned);
static void PictureRender(vout_display_t *, picture_t *, subpicture_t *);
static void PictureDisplay(vout_display_t *, picture_t *, subpicture_t *);
static int Control(vout_display_t*, int, va_list);

static void *OurGetProcAddress(vlc_gl_t *, const char *);

static int GLESMakeCurrent(vlc_gl_t *);
static void GLESSwap(vlc_gl_t *);
static void GLESReleaseCurrent(vlc_gl_t *);

/**
 * Module declaration
 */
vlc_module_begin ()
    set_shortname("iOS vout")
    set_description("iOS OpenGL video output")
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout display", 300)
    set_callbacks(Open, Close)

    add_shortcut("vout_ios2", "vout_ios")
    add_glopts()
vlc_module_end ()

@interface VLCOpenGLES2VideoView : UIView {
    vout_display_t *_voutDisplay;
    EAGLContext *_eaglContext;
    GLuint _renderBuffer;
    GLuint _frameBuffer;

    vlc_mutex_t _mutex;

    BOOL _bufferNeedReset;
    BOOL _appActive;
    BOOL _placeInvalidated;

    /* Written from MT, read locked from vout */
    vout_display_place_t _place;
    CGSize _viewSize;
    CGFloat _scaleFactor;

    /* Written from vout, read locked from MT */
    vout_display_cfg_t _cfg;
}
@property (readonly) GLuint renderBuffer;
@property (readonly) GLuint frameBuffer;
@property (readonly) EAGLContext* eaglContext;
@property GLuint shaderProgram;

- (id)initWithFrameAndVd:(CGRect)frame withVd:(vout_display_t*)vd;
- (void)createBuffers;
- (void)destroyBuffers;
- (BOOL)makeCurrent:(EAGLContext **)previousEaglContext;
- (BOOL)makeCurrentWithGL:(EAGLContext **)previousEaglContext withGL:(vlc_gl_t *)gl;
- (void)releaseCurrent:(EAGLContext *)previousEaglContext;

- (void)updateVoutCfg:(const vout_display_cfg_t *)cfg withVGL:(vout_display_opengl_t *)vgl;
- (void)getPlaceLocked:(vout_display_place_t *)place;
- (void)reshape;
- (void)propagateDimensionsToVoutCore;
@end

struct vout_display_sys_t
{
    VLCOpenGLES2VideoView *glESView;
    UIView *viewContainer;
    UITapGestureRecognizer *tapRecognizer;

    vlc_gl_t *gl;

    picture_pool_t *picturePool;
};

struct gl_sys
{
    VLCOpenGLES2VideoView *glESView;
    vout_display_opengl_t *vgl;
    EAGLContext *previousEaglContext;
};

static void *OurGetProcAddress(vlc_gl_t *gl, const char *name)
{
    VLC_UNUSED(gl);

    return dlsym(RTLD_DEFAULT, name);
}

static int Open(vlc_object_t *this)
{
    vout_display_t *vd = (vout_display_t *)this;

    if (vout_display_IsWindowed(vd))
        return VLC_EGENERIC;

    vout_display_sys_t *sys = vlc_obj_calloc (this, 1, sizeof(*sys));

    if (!sys)
        return VLC_ENOMEM;

    vd->sys = sys;
    sys->picturePool = NULL;
    sys->gl = NULL;

    var_Create(vd->obj.parent, "ios-eaglcontext", VLC_VAR_ADDRESS);

    @autoreleasepool {
        /* setup the actual OpenGL ES view */

        [VLCOpenGLES2VideoView performSelectorOnMainThread:@selector(getNewView:)
                                                withObject:[NSArray arrayWithObjects:
                                                           [NSValue valueWithPointer:&sys->glESView],
                                                           [NSValue valueWithPointer:vd], nil]
                                             waitUntilDone:YES];
        if (!sys->glESView) {
            msg_Err(vd, "Creating OpenGL ES 2 view failed");
            goto bailout;
        }

        [sys->glESView performSelectorOnMainThread:@selector(fetchViewContainer) withObject:nil waitUntilDone:YES];
        if (!sys->viewContainer) {
            msg_Err(vd, "Fetching view container failed");
            goto bailout;
        }

        const vlc_fourcc_t *subpicture_chromas;
        video_format_t fmt = vd->fmt;

        sys->gl = vlc_object_create(this, sizeof(*sys->gl));
        if (!sys->gl)
            goto bailout;

        struct gl_sys *glsys = sys->gl->sys =
            vlc_obj_malloc(this, sizeof(struct gl_sys));
        if (unlikely(!sys->gl->sys))
            goto bailout;
        glsys->glESView = sys->glESView;
        glsys->vgl = NULL;
        /* Initialize common OpenGL video display */
        sys->gl->makeCurrent = GLESMakeCurrent;
        sys->gl->releaseCurrent = GLESReleaseCurrent;
        sys->gl->swap = GLESSwap;
        sys->gl->getProcAddress = OurGetProcAddress;

        if (vlc_gl_MakeCurrent(sys->gl) != VLC_SUCCESS)
            goto bailout;

        var_SetAddress(vd->obj.parent, "ios-eaglcontext", [sys->glESView eaglContext]);

        vout_display_opengl_t *vgl = vout_display_opengl_New(&vd->fmt, &subpicture_chromas,
                                                             sys->gl, &vd->cfg->viewpoint);
        vlc_gl_ReleaseCurrent(sys->gl);
        if (!vgl)
            goto bailout;
        glsys->vgl = vgl;

        /* */
        vout_display_info_t info = vd->info;
        info.has_pictures_invalid = false;
        info.subpicture_chromas = subpicture_chromas;

        /* Setup vout_display_t once everything is fine */
        vd->info = info;

        vd->pool = PicturePool;
        vd->prepare = PictureRender;
        vd->display = PictureDisplay;
        vd->control = Control;

        /* */
        [[NSNotificationCenter defaultCenter] addObserver:sys->glESView
                                                 selector:@selector(applicationStateChanged:)
                                                     name:UIApplicationWillResignActiveNotification
                                                   object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:sys->glESView
                                                 selector:@selector(applicationStateChanged:)
                                                     name:UIApplicationDidBecomeActiveNotification
                                                   object:nil];
        [sys->glESView reshape];
        return VLC_SUCCESS;

    bailout:
        Close(this);
        return VLC_EGENERIC;
    }
}

static void Close (vlc_object_t *this)
{
    vout_display_t *vd = (vout_display_t *)this;
    vout_display_sys_t *sys = vd->sys;

    @autoreleasepool {
        if (sys->tapRecognizer) {
            [sys->tapRecognizer.view performSelectorOnMainThread:@selector(removeGestureRecognizer:) withObject:sys->tapRecognizer waitUntilDone:YES];
            [sys->tapRecognizer release];
        }

        var_Destroy (vd, "drawable-nsobject");
        @synchronized(sys->viewContainer) {
            [sys->glESView performSelectorOnMainThread:@selector(removeFromSuperview) withObject:nil waitUntilDone:NO];
            [sys->viewContainer performSelectorOnMainThread:@selector(release) withObject:nil waitUntilDone:NO];
        }
        sys->viewContainer = nil;

        if (sys->gl != NULL) {
            struct gl_sys *glsys = sys->gl->sys;
            msg_Dbg(this, "deleting display");

            if (likely(glsys->vgl))
            {
                vlc_gl_MakeCurrent(sys->gl);
                vout_display_opengl_Delete(glsys->vgl);
                vlc_gl_ReleaseCurrent(sys->gl);
            }
            vlc_object_release(sys->gl);
        }

        [sys->glESView release];
    }
    var_Destroy(vd->obj.parent, "ios-eaglcontext");
}

/*****************************************************************************
 * vout display callbacks
 *****************************************************************************/

static int Control(vout_display_t *vd, int query, va_list ap)
{
    vout_display_sys_t *sys = vd->sys;
    struct gl_sys *glsys = sys->gl->sys;

    switch (query) {
        case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        case VOUT_DISPLAY_CHANGE_ZOOM:
        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
        case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
        {
            const vout_display_cfg_t *cfg;

            if (query == VOUT_DISPLAY_CHANGE_SOURCE_ASPECT ||
                query == VOUT_DISPLAY_CHANGE_SOURCE_CROP)
                cfg = vd->cfg;
            else
                cfg = (const vout_display_cfg_t*)va_arg(ap, const vout_display_cfg_t *);

            assert(cfg);

            [sys->glESView updateVoutCfg:cfg withVGL:glsys->vgl];

            return VLC_SUCCESS;
        }

        case VOUT_DISPLAY_CHANGE_VIEWPOINT:
            return vout_display_opengl_SetViewpoint(glsys->vgl,
                &va_arg (ap, const vout_display_cfg_t* )->viewpoint);

        case VOUT_DISPLAY_RESET_PICTURES:
            vlc_assert_unreachable ();
        default:
            msg_Err(vd, "Unknown request %d", query);
            return VLC_EGENERIC;
    }
}

static void PictureDisplay(vout_display_t *vd, picture_t *pic, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    struct gl_sys *glsys = sys->gl->sys;

    if (vlc_gl_MakeCurrent(sys->gl) == VLC_SUCCESS)
    {
        vout_display_opengl_Display(glsys->vgl, &vd->source);
        vlc_gl_ReleaseCurrent(sys->gl);
    }

    picture_Release(pic);

    if (subpicture)
        subpicture_Delete(subpicture);
}

static void PictureRender(vout_display_t *vd, picture_t *pic, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    struct gl_sys *glsys = sys->gl->sys;

    if (vlc_gl_MakeCurrent(sys->gl) == VLC_SUCCESS)
    {
        vout_display_opengl_Prepare(glsys->vgl, pic, subpicture);
        vlc_gl_ReleaseCurrent(sys->gl);
    }
}

static picture_pool_t *PicturePool(vout_display_t *vd, unsigned requested_count)
{
    vout_display_sys_t *sys = vd->sys;
    struct gl_sys *glsys = sys->gl->sys;

    if (!sys->picturePool && vlc_gl_MakeCurrent(sys->gl) == VLC_SUCCESS)
    {
        sys->picturePool = vout_display_opengl_GetPool(glsys->vgl, requested_count);
        vlc_gl_ReleaseCurrent(sys->gl);
    }
    return sys->picturePool;
}

/*****************************************************************************
 * vout opengl callbacks
 *****************************************************************************/
static int GLESMakeCurrent(vlc_gl_t *gl)
{
    struct gl_sys *sys = gl->sys;

    if (![sys->glESView makeCurrentWithGL:&sys->previousEaglContext withGL:gl])
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static void GLESReleaseCurrent(vlc_gl_t *gl)
{
    struct gl_sys *sys = gl->sys;

    [sys->glESView releaseCurrent:sys->previousEaglContext];
}

static void GLESSwap(vlc_gl_t *gl)
{
    struct gl_sys *sys = gl->sys;

    [[sys->glESView eaglContext] presentRenderbuffer:GL_RENDERBUFFER];
}


/*****************************************************************************
 * Our UIView object
 *****************************************************************************/
@implementation VLCOpenGLES2VideoView
@synthesize eaglContext = _eaglContext;

+ (Class)layerClass
{
    return [CAEAGLLayer class];
}

+ (void)getNewView:(NSArray *)value
{
    id *ret = [[value objectAtIndex:0] pointerValue];
    vout_display_t *vd = [[value objectAtIndex:1] pointerValue];
    *ret = [[self alloc] initWithFrameAndVd:CGRectMake(0.,0.,320.,240.) withVd:vd];
}

- (id)initWithFrameAndVd:(CGRect)frame withVd:(vout_display_t*)vd
{
    self = [super initWithFrame:frame];

    if (!self)
        return nil;

    _appActive = ([UIApplication sharedApplication].applicationState == UIApplicationStateActive);
    if (unlikely(!_appActive))
        return nil;

    _voutDisplay = vd;
    _cfg = *_voutDisplay->cfg;

    vlc_mutex_init(&_mutex);

    /* the following creates a new OpenGL ES context with the API version we
     * need if there is already an active context created by another OpenGL
     * provider we cache it and restore analog to the
     * makeCurrent/releaseCurrent pattern used through-out the class */
    EAGLContext *previousEaglContext = [EAGLContext currentContext];

    _eaglContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];

    if (unlikely(!_eaglContext)
     || unlikely(![EAGLContext setCurrentContext:_eaglContext]))
    {
        vlc_mutex_destroy(&_mutex);
        return nil;
    }

    CAEAGLLayer *layer = (CAEAGLLayer *)self.layer;
    layer.drawableProperties = [NSDictionary dictionaryWithObject:kEAGLColorFormatRGBA8 forKey: kEAGLDrawablePropertyColorFormat];
    layer.opaque = YES;

    self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

    [self releaseCurrent:previousEaglContext];

    [self createBuffers];
    [self reshape];

    return self;
}

- (void)fetchViewContainer
{
    @try {
        /* get the object we will draw into */
        UIView *viewContainer = var_CreateGetAddress (_voutDisplay, "drawable-nsobject");
        if (unlikely(viewContainer == nil)) {
            msg_Err(_voutDisplay, "provided view container is nil");
            return;
        }

        if (unlikely(![viewContainer respondsToSelector:@selector(isKindOfClass:)])) {
            msg_Err(_voutDisplay, "void pointer not an ObjC object");
            return;
        }

        [viewContainer retain];

        @synchronized(viewContainer) {
            if (![viewContainer isKindOfClass:[UIView class]]) {
                msg_Err(_voutDisplay, "passed ObjC object not of class UIView");
                return;
            }

            vout_display_sys_t *sys = _voutDisplay->sys;

            /* This will be released in Close(), on
             * main thread, after we are done using it. */
            sys->viewContainer = viewContainer;

            self.frame = viewContainer.bounds;
            [self reshape];

            [sys->viewContainer addSubview:self];

            /* add tap gesture recognizer for DVD menus and stuff */
            sys->tapRecognizer = [[UITapGestureRecognizer alloc] initWithTarget:self
                                                                         action:@selector(tapRecognized:)];
            if (sys->viewContainer.window) {
                if (sys->viewContainer.window.rootViewController) {
                    if (sys->viewContainer.window.rootViewController.view)
                        [sys->viewContainer.superview addGestureRecognizer:sys->tapRecognizer];
                }
            }
            sys->tapRecognizer.cancelsTouchesInView = NO;
        }
    } @catch (NSException *exception) {
        msg_Err(_voutDisplay, "Handling the view container failed due to an Obj-C exception (%s, %s", [exception.name UTF8String], [exception.reason UTF8String]);
        vout_display_sys_t *sys = _voutDisplay->sys;
        sys->viewContainer = nil;
    }
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [_eaglContext release];
    vlc_mutex_destroy(&_mutex);
    [super dealloc];
}

- (void)didMoveToWindow
{
    self.contentScaleFactor = self.window.screen.scale;

    vlc_mutex_lock(&_mutex);
    _bufferNeedReset = YES;
    vlc_mutex_unlock(&_mutex);
}

- (void)createBuffers
{
    if (![NSThread isMainThread])
    {
        [self performSelectorOnMainThread:@selector(createBuffers)
                                                 withObject:nil
                                              waitUntilDone:YES];
        return;
    }

    EAGLContext *previousEaglContext;
    if (![self makeCurrent:&previousEaglContext])
        return;

    glDisable(GL_DEPTH_TEST);

    glGenFramebuffers(1, &_frameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, _frameBuffer);

    glGenRenderbuffers(1, &_renderBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _renderBuffer);

    [_eaglContext renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer *)self.layer];

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _renderBuffer);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        msg_Err(_voutDisplay, "Failed to make complete framebuffer object %x", glCheckFramebufferStatus(GL_FRAMEBUFFER));

    [self releaseCurrent:previousEaglContext];
}

- (void)destroyBuffers
{
    if (![NSThread isMainThread])
    {
        [self performSelectorOnMainThread:@selector(destroyBuffers)
                                                 withObject:nil
                                              waitUntilDone:YES];
        return;
    }

    EAGLContext *previousEaglContext;
    if (![self makeCurrent:&previousEaglContext])
        return;

    /* clear frame buffer */
    glDeleteFramebuffers(1, &_frameBuffer);
    _frameBuffer = 0;

    /* clear render buffer */
    glDeleteRenderbuffers(1, &_renderBuffer);
    _renderBuffer = 0;

    [self releaseCurrent:previousEaglContext];
}

- (BOOL)makeCurrentWithGL:(EAGLContext **)previousEaglContext withGL:(vlc_gl_t *)gl
{
    vlc_mutex_lock(&_mutex);

    if (unlikely(!_appActive))
    {
        vlc_mutex_unlock(&_mutex);
        return NO;
    }

    *previousEaglContext = [EAGLContext currentContext];

    BOOL success = [EAGLContext setCurrentContext:_eaglContext];
    BOOL resetBuffers = NO;

    if (success && gl != NULL)
    {
        struct gl_sys *glsys = gl->sys;

        if (unlikely(_bufferNeedReset))
        {
            _bufferNeedReset = NO;
            resetBuffers = YES;
        }
        if (unlikely(_placeInvalidated && glsys->vgl))
        {
            _placeInvalidated = NO;

            vout_display_place_t place;
            [self getPlaceLocked: &place];
            vout_display_opengl_SetWindowAspectRatio(glsys->vgl, (float)place.width / place.height);

            // x / y are top left corner, but we need the lower left one
            glViewport(_place.x, _place.y, _place.width, _place.height);
        }
    }

    vlc_mutex_unlock(&_mutex);

    if (resetBuffers)
    {
        [self destroyBuffers];
        [self createBuffers];
    }
    return success;
}

- (BOOL)makeCurrent:(EAGLContext **)previousEaglContext
{
    return [self makeCurrentWithGL:previousEaglContext withGL:nil];
}

- (void)releaseCurrent:(EAGLContext *)previousEaglContext
{
    [EAGLContext setCurrentContext:previousEaglContext];
}

- (void)layoutSubviews
{
    [self reshape];

    vlc_mutex_lock(&_mutex);
    _bufferNeedReset = YES;
    vlc_mutex_unlock(&_mutex);
}

- (void)getPlaceLocked:(vout_display_place_t *)place
{
    vout_display_cfg_t cfg = _cfg;

    cfg.display.width  = _viewSize.width * _scaleFactor;
    cfg.display.height = _viewSize.height * _scaleFactor;

    vout_display_PlacePicture(place, &_voutDisplay->source, &cfg, false);
}

- (void)reshape
{
    if (![NSThread isMainThread])
    {
        [self performSelectorOnMainThread:@selector(reshape)
                                                 withObject:nil
                                              waitUntilDone:YES];
        return;
    }

    vlc_mutex_lock(&_mutex);
    _viewSize = [self bounds].size;
    _scaleFactor = self.contentScaleFactor;

    vout_display_place_t place;
    [self getPlaceLocked: &place];

    if (memcmp(&place, &_place, sizeof(vout_display_place_t)) != 0)
    {
        _placeInvalidated = YES;
        _place = place;
    }
    vout_display_SendEventDisplaySize(_voutDisplay, _viewSize.width * _scaleFactor,
                                      _viewSize.height * _scaleFactor);

    vlc_mutex_unlock(&_mutex);
}

- (void)tapRecognized:(UITapGestureRecognizer *)tapRecognizer
{
    UIGestureRecognizerState state = [tapRecognizer state];
    CGPoint touchPoint = [tapRecognizer locationInView:self];
    CGFloat scaleFactor = self.contentScaleFactor;
    vout_display_SendMouseMovedDisplayCoordinates(_voutDisplay, ORIENT_NORMAL,
                                                  (int)touchPoint.x * scaleFactor, (int)touchPoint.y * scaleFactor,
                                                  &_place);

    vout_display_SendEventMousePressed(_voutDisplay, MOUSE_BUTTON_LEFT);
    vout_display_SendEventMouseReleased(_voutDisplay, MOUSE_BUTTON_LEFT);
}

- (void)updateVoutCfg:(const vout_display_cfg_t *)cfg withVGL:(vout_display_opengl_t *)vgl
{
    if (memcmp(&_cfg, cfg, sizeof(vout_display_cfg_t)) == 0)
        return;

    vlc_mutex_lock(&_mutex);
    _cfg = *cfg;

    vout_display_place_t place;
    [self getPlaceLocked: &place];
    vout_display_opengl_SetWindowAspectRatio(vgl, (float)place.width / place.height);

    vlc_mutex_unlock(&_mutex);

    [self performSelectorOnMainThread:@selector(setNeedsUpdateConstraints)
                           withObject:nil
                        waitUntilDone:NO];
}

- (void)applicationStateChanged:(NSNotification *)notification
{
    vlc_mutex_lock(&_mutex);

    if ([[notification name] isEqualToString:UIApplicationWillResignActiveNotification]
        || [[notification name] isEqualToString:UIApplicationDidEnterBackgroundNotification]
        || [[notification name] isEqualToString:UIApplicationWillTerminateNotification])
        _appActive = NO;
    else
        _appActive = YES;

    vlc_mutex_unlock(&_mutex);
}

- (void)updateConstraints
{
    [super updateConstraints];
    [self reshape];
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
