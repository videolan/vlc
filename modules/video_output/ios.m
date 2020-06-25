/*****************************************************************************
 * ios.m: iOS OpenGL ES provider
 *****************************************************************************
 * Copyright (C) 2001-2017 VLC authors and VideoLAN
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
static int Open(vout_display_t *vd, const vout_display_cfg_t *cfg,
                video_format_t *fmt, vlc_video_context *context);
static void Close(vout_display_t *vd);

static void PictureRender(vout_display_t *, picture_t *, subpicture_t *, vlc_tick_t);
static void PictureDisplay(vout_display_t *, picture_t *);
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
    set_callback_display(Open, 300)

    add_shortcut("vout_ios2", "vout_ios")
    add_glopts()
vlc_module_end ()

@interface VLCOpenGLES2VideoView : UIView {
    vout_display_t *_voutDisplay;
    EAGLContext *_eaglContext;
    CAEAGLLayer *_layer;

    vlc_mutex_t _mutex;
    vlc_cond_t  _gl_attached_wait;
    BOOL        _gl_attached;

    BOOL _bufferNeedReset;
    BOOL _appActive;
    BOOL _eaglEnabled;
    BOOL _placeInvalidated;

    UIView *_viewContainer;
    UITapGestureRecognizer *_tapRecognizer;

    /* Written from MT, read locked from vout */
    vout_display_place_t _place;
    CGSize _viewSize;
    CGFloat _scaleFactor;

    /* Written from vout, read locked from MT */
    vout_display_cfg_t _cfg;
}

- (id)initWithFrame:(CGRect)frame andVD:(vout_display_t*)vd;
- (void)cleanAndRelease:(BOOL)flushed;
- (BOOL)makeCurrent:(EAGLContext **)previousEaglContext withGL:(vlc_gl_t *)gl;
- (void)releaseCurrent:(EAGLContext *)previousEaglContext;
- (void)presentRenderbuffer;

- (void)updateVoutCfg:(const vout_display_cfg_t *)cfg withVGL:(vout_display_opengl_t *)vgl;
- (void)getPlaceLocked:(vout_display_place_t *)place;
@end

struct vout_display_sys_t
{
    VLCOpenGLES2VideoView *glESView;

    vlc_gl_t *gl;

    vout_window_t *embed;
};

struct gl_sys
{
    VLCOpenGLES2VideoView *glESView;
    vout_display_opengl_t *vgl;
    GLuint renderBuffer;
    GLuint frameBuffer;
    EAGLContext *previousEaglContext;
};

static void *OurGetProcAddress(vlc_gl_t *gl, const char *name)
{
    VLC_UNUSED(gl);

    return dlsym(RTLD_DEFAULT, name);
}

static int Open(vout_display_t *vd, const vout_display_cfg_t *cfg,
                video_format_t *fmt, vlc_video_context *context)
{
    if (vout_display_cfg_IsWindowed(cfg))
        return VLC_EGENERIC;

    vout_display_sys_t *sys = vlc_obj_calloc(VLC_OBJECT(vd), 1, sizeof(*sys));

    if (!sys)
        return VLC_ENOMEM;

    vd->sys = sys;
    sys->gl = NULL;

    var_Create(vlc_object_parent(vd), "ios-eaglcontext", VLC_VAR_ADDRESS);

    @autoreleasepool {
        /* setup the actual OpenGL ES view */

        [VLCOpenGLES2VideoView performSelectorOnMainThread:@selector(getNewView:)
                                                withObject:[NSArray arrayWithObjects:
                                                           [NSValue valueWithPointer:&sys->glESView],
                                                           [NSValue valueWithPointer:vd], nil]
                                             waitUntilDone:YES];
        if (!sys->glESView) {
            msg_Err(vd, "Creating OpenGL ES 2 view failed");
            var_Destroy(vlc_object_parent(vd), "ios-eaglcontext");
            return VLC_EGENERIC;
        }

        const vlc_fourcc_t *subpicture_chromas;

        sys->embed = cfg->window;
        sys->gl = vlc_object_create(vd, sizeof(*sys->gl));
        if (!sys->gl)
            goto bailout;

        struct gl_sys *glsys = sys->gl->sys =
            vlc_obj_malloc(VLC_OBJECT(vd), sizeof(struct gl_sys));
        if (unlikely(!sys->gl->sys))
            goto bailout;
        glsys->glESView = sys->glESView;
        glsys->vgl = NULL;
        glsys->renderBuffer = glsys->frameBuffer = 0;

        /* Initialize common OpenGL video display */
        sys->gl->make_current = GLESMakeCurrent;
        sys->gl->release_current = GLESReleaseCurrent;
        sys->gl->swap = GLESSwap;
        sys->gl->get_proc_address = OurGetProcAddress;

        if (vlc_gl_MakeCurrent(sys->gl) != VLC_SUCCESS)
            goto bailout;

        vout_display_opengl_t *vgl = vout_display_opengl_New(fmt, &subpicture_chromas,
                                                             sys->gl, &cfg->viewpoint,
                                                             context);
        vlc_gl_ReleaseCurrent(sys->gl);
        if (!vgl)
            goto bailout;
        glsys->vgl = vgl;

        /* Setup vout_display_t once everything is fine */
        vd->info.subpicture_chromas = subpicture_chromas;

        vd->prepare = PictureRender;
        vd->display = PictureDisplay;
        vd->control = Control;
        vd->close   = Close;

        return VLC_SUCCESS;

    bailout:
        Close(vd);
        return VLC_EGENERIC;
    }
}

static void Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    @autoreleasepool {
        BOOL flushed = NO;
        if (sys->gl != NULL) {
            struct gl_sys *glsys = sys->gl->sys;
            msg_Dbg(vd, "deleting display");

            if (likely(glsys->vgl))
            {
                int ret = vlc_gl_MakeCurrent(sys->gl);
                vout_display_opengl_Delete(glsys->vgl);
                if (ret == VLC_SUCCESS)
                {
                    vlc_gl_ReleaseCurrent(sys->gl);
                    flushed = YES;
                }
            }
            vlc_object_delete(sys->gl);
        }

        [sys->glESView cleanAndRelease:flushed];
    }
    var_Destroy(vlc_object_parent(vd), "ios-eaglcontext");
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
            const vout_display_cfg_t *cfg =
                va_arg(ap, const vout_display_cfg_t *);

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

static void PictureDisplay(vout_display_t *vd, picture_t *pic)
{
    vout_display_sys_t *sys = vd->sys;
    struct gl_sys *glsys = sys->gl->sys;
    VLC_UNUSED(pic);

    if (vlc_gl_MakeCurrent(sys->gl) == VLC_SUCCESS)
    {
        vout_display_opengl_Display(glsys->vgl);
        vlc_gl_ReleaseCurrent(sys->gl);
    }
}

static void PictureRender(vout_display_t *vd, picture_t *pic, subpicture_t *subpicture,
                          vlc_tick_t date)
{
    VLC_UNUSED(date);
    vout_display_sys_t *sys = vd->sys;
    struct gl_sys *glsys = sys->gl->sys;

    if (vlc_gl_MakeCurrent(sys->gl) == VLC_SUCCESS)
    {
        vout_display_opengl_Prepare(glsys->vgl, pic, subpicture);
        vlc_gl_ReleaseCurrent(sys->gl);
    }
}

/*****************************************************************************
 * vout opengl callbacks
 *****************************************************************************/
static int GLESMakeCurrent(vlc_gl_t *gl)
{
    struct gl_sys *sys = gl->sys;

    if (![sys->glESView makeCurrent:&sys->previousEaglContext withGL:gl])
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

    [sys->glESView presentRenderbuffer];
}


/*****************************************************************************
 * Our UIView object
 *****************************************************************************/
@implementation VLCOpenGLES2VideoView

+ (Class)layerClass
{
    return [CAEAGLLayer class];
}

+ (void)getNewView:(NSArray *)value
{
    id *ret = [[value objectAtIndex:0] pointerValue];
    vout_display_t *vd = [[value objectAtIndex:1] pointerValue];
    *ret = [[self alloc] initWithFrame:CGRectMake(0.,0.,320.,240.) andVD:vd];
}

- (id)initWithFrame:(CGRect)frame andVD:(vout_display_t*)vd
{
    _appActive = ([UIApplication sharedApplication].applicationState == UIApplicationStateActive);
    if (unlikely(!_appActive))
        return nil;

    self = [super initWithFrame:frame];
    if (!self)
        return nil;

    _eaglEnabled = YES;
    _bufferNeedReset = YES;
    _voutDisplay = vd;
    _cfg = *_voutDisplay->cfg;

    vlc_mutex_init(&_mutex);
    vlc_cond_init(&_gl_attached_wait);
    _gl_attached = YES;

    /* the following creates a new OpenGL ES context with the API version we
     * need if there is already an active context created by another OpenGL
     * provider we cache it and restore analog to the
     * makeCurrent/releaseCurrent pattern used through-out the class */
    EAGLContext *previousEaglContext = [EAGLContext currentContext];

    _eaglContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];

    if (unlikely(!_eaglContext)
     || unlikely(![EAGLContext setCurrentContext:_eaglContext]))
    {
        [_eaglContext release];
        [self release];
        return nil;
    }
    [self releaseCurrent:previousEaglContext];

    /* Set "ios-eaglcontext" to be used by cvpx fitlers/glconv */
    var_SetAddress(vlc_object_parent(_voutDisplay), "ios-eaglcontext", _eaglContext);

    _layer = (CAEAGLLayer *)self.layer;
    _layer.drawableProperties = [NSDictionary dictionaryWithObject:kEAGLColorFormatRGBA8 forKey: kEAGLDrawablePropertyColorFormat];
    _layer.opaque = YES;

    self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

    if (![self fetchViewContainer])
    {
        [_eaglContext release];
        [self release];
        return nil;
    }

    /* */
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationStateChanged:)
                                                 name:UIApplicationWillResignActiveNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationStateChanged:)
                                                 name:UIApplicationDidBecomeActiveNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationStateChanged:)
                                                 name:UIApplicationDidEnterBackgroundNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationStateChanged:)
                                                 name:UIApplicationWillEnterForegroundNotification
                                               object:nil];

    return self;
}

- (BOOL)fetchViewContainer
{
    @try {
        /* get the object we will draw into */
        UIView *viewContainer = var_InheritAddress (_voutDisplay, "drawable-nsobject");
        if (unlikely(viewContainer == nil)) {
            msg_Err(_voutDisplay, "provided view container is nil");
            return NO;
        }

        if (unlikely(![viewContainer respondsToSelector:@selector(isKindOfClass:)])) {
            msg_Err(_voutDisplay, "void pointer not an ObjC object");
            return NO;
        }

        [viewContainer retain];

        if (![viewContainer isKindOfClass:[UIView class]]) {
            msg_Err(_voutDisplay, "passed ObjC object not of class UIView");
            return NO;
        }

        /* This will be released in Close(), on
         * main thread, after we are done using it. */
        _viewContainer = viewContainer;

        self.frame = viewContainer.bounds;
        [self reshape];

        [_viewContainer addSubview:self];

        /* add tap gesture recognizer for DVD menus and stuff */
        _tapRecognizer = [[UITapGestureRecognizer alloc] initWithTarget:self
                                                                 action:@selector(tapRecognized:)];
        if (_viewContainer.window
         && _viewContainer.window.rootViewController
         && _viewContainer.window.rootViewController.view)
            [_viewContainer.superview addGestureRecognizer:_tapRecognizer];
        _tapRecognizer.cancelsTouchesInView = NO;
        return YES;
    } @catch (NSException *exception) {
        msg_Err(_voutDisplay, "Handling the view container failed due to an Obj-C exception (%s, %s", [exception.name UTF8String], [exception.reason UTF8String]);
        vout_display_sys_t *sys = _voutDisplay->sys;
        if (_tapRecognizer)
            [_tapRecognizer release];
        return NO;
    }
}

- (void)cleanAndReleaseFromMainThread
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    [_tapRecognizer.view removeGestureRecognizer:_tapRecognizer];
    [_tapRecognizer release];

    [self removeFromSuperview];
    [_viewContainer release];

    assert(!_gl_attached);
    [_eaglContext release];
    [self release];
}

- (void)cleanAndRelease:(BOOL)flushed
{
    vlc_mutex_lock(&_mutex);
    if (_eaglEnabled && !flushed)
        [self flushEAGLLocked];
    _voutDisplay = nil;
    _eaglEnabled = NO;
    vlc_mutex_unlock(&_mutex);

    [self performSelectorOnMainThread:@selector(cleanAndReleaseFromMainThread)
                           withObject:nil
                        waitUntilDone:NO];
}

- (void)dealloc
{
    [super dealloc];
}

- (void)didMoveToWindow
{
    self.contentScaleFactor = self.window.screen.scale;

    vlc_mutex_lock(&_mutex);
    _bufferNeedReset = YES;
    vlc_mutex_unlock(&_mutex);
}

- (BOOL)doResetBuffers:(vlc_gl_t *)gl
{
    struct gl_sys *glsys = gl->sys;

    if (glsys->frameBuffer != 0)
    {
        /* clear frame buffer */
        glDeleteFramebuffers(1, &glsys->frameBuffer);
        glsys->frameBuffer = 0;
    }

    if (glsys->renderBuffer != 0)
    {
        /* clear render buffer */
        glDeleteRenderbuffers(1, &glsys->renderBuffer);
        glsys->renderBuffer = 0;
    }

    glDisable(GL_DEPTH_TEST);

    glGenFramebuffers(1, &glsys->frameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, glsys->frameBuffer);

    glGenRenderbuffers(1, &glsys->renderBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, glsys->renderBuffer);

    [_eaglContext renderbufferStorage:GL_RENDERBUFFER fromDrawable:_layer];

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, glsys->renderBuffer);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        msg_Err(_voutDisplay, "Failed to make complete framebuffer object %x", glCheckFramebufferStatus(GL_FRAMEBUFFER));
        return NO;
    }
    return YES;
}

- (BOOL)makeCurrent:(EAGLContext **)previousEaglContext withGL:(vlc_gl_t *)gl
{
    vlc_mutex_lock(&_mutex);
    assert(!_gl_attached);

    if (unlikely(!_appActive))
    {
        vlc_mutex_unlock(&_mutex);
        return NO;
    }

    assert(_eaglEnabled);
    *previousEaglContext = [EAGLContext currentContext];

    if (![EAGLContext setCurrentContext:_eaglContext])
    {
        vlc_mutex_unlock(&_mutex);
        return NO;
    }

    BOOL resetBuffers = NO;

    if (gl != NULL)
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
            vout_display_opengl_Viewport(glsys->vgl, _place.x, _place.y, _place.width, _place.height);
        }
    }

    _gl_attached = YES;

    vlc_mutex_unlock(&_mutex);

    if (resetBuffers && ![self doResetBuffers:gl])
    {
        [self releaseCurrent:*previousEaglContext];
        return NO;
    }
    return YES;
}

- (void)releaseCurrent:(EAGLContext *)previousEaglContext
{
    [EAGLContext setCurrentContext:previousEaglContext];

    vlc_mutex_lock(&_mutex);
    assert(_gl_attached);
    _gl_attached = NO;
    vlc_cond_signal(&_gl_attached_wait);
    vlc_mutex_unlock(&_mutex);
}

- (void)presentRenderbuffer
{
    [_eaglContext presentRenderbuffer:GL_RENDERBUFFER];
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
    assert(_voutDisplay);
    vout_display_cfg_t cfg = _cfg;

    cfg.display.width  = _viewSize.width * _scaleFactor;
    cfg.display.height = _viewSize.height * _scaleFactor;

    vout_display_PlacePicture(place, &_voutDisplay->source, &cfg);
}

- (void)reshape
{
    assert([NSThread isMainThread]);

    vlc_mutex_lock(&_mutex);
    if (!_voutDisplay)
    {
        vlc_mutex_unlock(&_mutex);
        return;
    }
    _viewSize = [self bounds].size;
    _scaleFactor = self.contentScaleFactor;

    vout_display_place_t place;
    [self getPlaceLocked: &place];

    if (memcmp(&place, &_place, sizeof(vout_display_place_t)) != 0)
    {
        _placeInvalidated = YES;
        _place = place;
    }

    vlc_mutex_unlock(&_mutex);
}

- (void)tapRecognized:(UITapGestureRecognizer *)tapRecognizer
{
    vlc_mutex_lock(&_mutex);
    if (!_voutDisplay)
    {
        vlc_mutex_unlock(&_mutex);
        return;
    }

    UIGestureRecognizerState state = [tapRecognizer state];
    CGPoint touchPoint = [tapRecognizer locationInView:self];
    CGFloat scaleFactor = self.contentScaleFactor;
    vout_display_SendMouseMovedDisplayCoordinates(_voutDisplay,
                                                  (int)touchPoint.x * scaleFactor, (int)touchPoint.y * scaleFactor);

    vout_display_SendEventMousePressed(_voutDisplay, MOUSE_BUTTON_LEFT);
    vout_display_SendEventMouseReleased(_voutDisplay, MOUSE_BUTTON_LEFT);

    vlc_mutex_unlock(&_mutex);
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

    if ([[notification name] isEqualToString:UIApplicationWillResignActiveNotification])
        _appActive = NO;
    else if ([[notification name] isEqualToString:UIApplicationDidEnterBackgroundNotification])
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
        _eaglEnabled = YES;
    else
    {
        assert([[notification name] isEqualToString:UIApplicationDidBecomeActiveNotification]);
        _appActive = YES;
    }

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
