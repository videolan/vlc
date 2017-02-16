/*****************************************************************************
 * ios2.m: iOS OpenGL ES 2 provider
 *****************************************************************************
 * Copyright (C) 2001-2016 VLC authors and VideoLAN
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

static int OpenglESLock(vlc_gl_t *);
static void OpenglESSwap(vlc_gl_t *);
static void OpenglESUnlock(vlc_gl_t *);

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

    add_shortcut("vout_ios2")
vlc_module_end ()

@interface VLCOpenGLES2VideoView : UIView {
    vout_display_t *_voutDisplay;
    EAGLContext *_eaglContext;
    EAGLContext *_previousEaglContext;
    GLuint _renderBuffer;
    GLuint _frameBuffer;

    BOOL _bufferNeedReset;
    BOOL _appActive;
}
@property (readonly) GLuint renderBuffer;
@property (readonly) GLuint frameBuffer;
@property (readwrite) vout_display_t* voutDisplay;
@property (readonly) EAGLContext* eaglContext;
@property (readonly) BOOL isAppActive;
@property GLuint shaderProgram;

- (id)initWithFrame:(CGRect)frame voutDisplay:(vout_display_t *)vd;

- (void)createBuffers;
- (void)destroyBuffers;
- (void)resetBuffers;
- (void)lock;
- (void)unlock;

- (void)reshape;
@end

struct vout_display_sys_t
{
    VLCOpenGLES2VideoView *glESView;
    UIView *viewContainer;
    UITapGestureRecognizer *tapRecognizer;

    vlc_gl_t *gl;
    vout_display_opengl_t *vgl;

    picture_pool_t *picturePool;
    bool has_first_frame;

    vout_display_place_t place;
};

struct gl_sys
{
    CVEAGLContext locked_ctx;
    VLCOpenGLES2VideoView *glESView;
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

    vout_display_sys_t *sys = calloc (1, sizeof(*sys));

    if (!sys)
        return VLC_ENOMEM;

    vd->sys = sys;
    sys->picturePool = NULL;
    sys->gl = NULL;

    @autoreleasepool {
        /* setup the actual OpenGL ES view */
        sys->glESView = [[VLCOpenGLES2VideoView alloc] initWithFrame:CGRectMake(0.,0.,320.,240.) voutDisplay:vd];
        sys->glESView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

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

        struct gl_sys *glsys = sys->gl->sys = malloc(sizeof(struct gl_sys));
        if (unlikely(!sys->gl->sys))
            goto bailout;
        glsys->locked_ctx = NULL;
        glsys->glESView = sys->glESView;
        /* Initialize common OpenGL video display */
        sys->gl->makeCurrent = OpenglESLock;
        sys->gl->releaseCurrent = OpenglESUnlock;
        sys->gl->swap = OpenglESSwap;
        sys->gl->getProcAddress = OurGetProcAddress;

        vlc_gl_MakeCurrent(sys->gl);
        sys->vgl = vout_display_opengl_New(&vd->fmt, &subpicture_chromas,
                                           sys->gl, &vd->cfg->viewpoint);
        vlc_gl_ReleaseCurrent(sys->gl);
        if (!sys->vgl)
            goto bailout;

        /* */
        vout_display_info_t info = vd->info;
        info.has_pictures_invalid = false;
        info.subpicture_chromas = subpicture_chromas;
        info.has_hide_mouse = false;

        /* Setup vout_display_t once everything is fine */
        vd->info = info;

        vd->pool = PicturePool;
        vd->prepare = PictureRender;
        vd->display = PictureDisplay;
        vd->control = Control;
        vd->manage = NULL;

        /* forward our dimensions to the vout core */
        CGFloat scaleFactor;
        CGSize viewSize;
        @synchronized(sys->viewContainer) {
            scaleFactor = sys->viewContainer.contentScaleFactor;
            viewSize = sys->viewContainer.bounds.size;
        }
        vout_display_SendEventDisplaySize(vd, viewSize.width * scaleFactor, viewSize.height * scaleFactor);

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

void Close (vlc_object_t *this)
{
    vout_display_t *vd = (vout_display_t *)this;
    vout_display_sys_t *sys = vd->sys;

    @autoreleasepool {
        if (sys->tapRecognizer) {
            [sys->tapRecognizer.view removeGestureRecognizer:sys->tapRecognizer];
            [sys->tapRecognizer release];
        }

        [sys->glESView setVoutDisplay:nil];

        var_Destroy (vd, "drawable-nsobject");
        @synchronized(sys->viewContainer) {
            [sys->glESView performSelectorOnMainThread:@selector(removeFromSuperview) withObject:nil waitUntilDone:NO];
            [sys->viewContainer performSelectorOnMainThread:@selector(release) withObject:nil waitUntilDone:NO];
        }
        sys->viewContainer = nil;

        if (sys->gl != NULL) {
            @synchronized (sys->glESView) {
                msg_Dbg(this, "deleting display");

                if (likely([sys->glESView isAppActive]) && sys->vgl)
                {
                    vlc_gl_MakeCurrent(sys->gl);
                    vout_display_opengl_Delete(sys->vgl);
                    vlc_gl_ReleaseCurrent(sys->gl);
                }
            }
            vlc_object_release(sys->gl);
        }

        [sys->glESView release];

        free(sys);
    }
}

/*****************************************************************************
 * vout display callbacks
 *****************************************************************************/

static int Control(vout_display_t *vd, int query, va_list ap)
{
    vout_display_sys_t *sys = vd->sys;

    switch (query) {
        case VOUT_DISPLAY_HIDE_MOUSE:
            return VLC_EGENERIC;

        case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        case VOUT_DISPLAY_CHANGE_ZOOM:
        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
        case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
        {
            if (!vd->sys)
                return VLC_EGENERIC;

            @autoreleasepool {
                const vout_display_cfg_t *cfg;
                const video_format_t *source;

                if (query == VOUT_DISPLAY_CHANGE_SOURCE_ASPECT ||
                    query == VOUT_DISPLAY_CHANGE_SOURCE_CROP) {
                    source = (const video_format_t *)va_arg(ap, const video_format_t *);
                    cfg = vd->cfg;
                } else {
                    source = &vd->source;
                    cfg = (const vout_display_cfg_t*)va_arg(ap, const vout_display_cfg_t *);
                }

                /* we don't adapt anything here regardless of what the vout core
                 * wants since we are not in a traditional desktop window */
                if (!cfg)
                    return VLC_EGENERIC;

                vout_display_cfg_t cfg_tmp = *cfg;
                CGSize viewSize;
                viewSize = [sys->glESView bounds].size;

                /* on HiDPI displays, the point bounds don't equal the actual pixels */
                CGFloat scaleFactor = sys->glESView.contentScaleFactor;
                cfg_tmp.display.width = viewSize.width * scaleFactor;
                cfg_tmp.display.height = viewSize.height * scaleFactor;

                vout_display_place_t place;
                vout_display_PlacePicture(&place, source, &cfg_tmp, false);
                @synchronized (sys->glESView) {
                    sys->place = place;
                }

                if (sys->gl != NULL)
                    vout_display_opengl_SetWindowAspectRatio(sys->vgl, (float)place.width / place.height);

                // x / y are top left corner, but we need the lower left one
                if (query != VOUT_DISPLAY_CHANGE_DISPLAY_SIZE)
                    glViewport(place.x, cfg_tmp.display.height - (place.y + place.height), place.width, place.height);
            }
            return VLC_SUCCESS;
        }

        case VOUT_DISPLAY_CHANGE_VIEWPOINT:
            if (sys->gl != NULL)
                return vout_display_opengl_SetViewpoint(sys->vgl,
                    &va_arg (ap, const vout_display_cfg_t* )->viewpoint);
            else
                return VLC_EGENERIC;

        case VOUT_DISPLAY_RESET_PICTURES:
            vlc_assert_unreachable ();
        default:
            msg_Err(vd, "Unknown request %d", query);
        case VOUT_DISPLAY_CHANGE_FULLSCREEN:
            return VLC_EGENERIC;
    }
}

static void PictureDisplay(vout_display_t *vd, picture_t *pic, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    sys->has_first_frame = true;
    @synchronized (sys->glESView) {
        if (likely([sys->glESView isAppActive])) {
            vlc_gl_MakeCurrent(sys->gl);
            vout_display_opengl_Display(sys->vgl, &vd->source);
            vlc_gl_ReleaseCurrent(sys->gl);
        }
    }

    picture_Release(pic);

    if (subpicture)
        subpicture_Delete(subpicture);
}

static void PictureRender(vout_display_t *vd, picture_t *pic, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    if (likely([sys->glESView isAppActive]))
    {
        vlc_gl_MakeCurrent(sys->gl);
        vout_display_opengl_Prepare(sys->vgl, pic, subpicture);
        vlc_gl_ReleaseCurrent(sys->gl);
    }
}

static picture_pool_t *PicturePool(vout_display_t *vd, unsigned requested_count)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->picturePool)
    {
        vlc_gl_MakeCurrent(sys->gl);
        sys->picturePool = vout_display_opengl_GetPool(sys->vgl, requested_count);
        vlc_gl_ReleaseCurrent(sys->gl);
    }
    assert(sys->picturePool);
    return sys->picturePool;
}

/*****************************************************************************
 * vout opengl callbacks
 *****************************************************************************/
static int OpenglESLock(vlc_gl_t *gl)
{
    struct gl_sys *sys = gl->sys;

    [sys->glESView lock];
    if (likely([sys->glESView isAppActive]))
        [sys->glESView resetBuffers];
    sys->locked_ctx = (__bridge CVEAGLContext) ((__bridge void *) [sys->glESView eaglContext]);
    return 0;
}

static void OpenglESUnlock(vlc_gl_t *gl)
{
    struct gl_sys *sys = gl->sys;

    [sys->glESView unlock];
}

static void OpenglESSwap(vlc_gl_t *gl)
{
    struct gl_sys *sys = gl->sys;

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

- (id)initWithFrame:(CGRect)frame voutDisplay:(vout_display_t *)vd
{
    self = [super initWithFrame:frame];

    if (!self)
        return nil;

    _appActive = ([UIApplication sharedApplication].applicationState == UIApplicationStateActive);
    if (unlikely(!_appActive))
        return nil;

    _eaglContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];

    if (unlikely(!_eaglContext))
        return nil;
    if (unlikely(![EAGLContext setCurrentContext:_eaglContext]))
        return nil;

    CAEAGLLayer *layer = (CAEAGLLayer *)self.layer;
    layer.drawableProperties = [NSDictionary dictionaryWithObject:kEAGLColorFormatRGBA8 forKey: kEAGLDrawablePropertyColorFormat];
    layer.opaque = YES;

    _voutDisplay = vd;

    [self createBuffers];

    [self reshape];
    [self setAutoresizingMask: UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];

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

        [viewContainer retain];

        @synchronized(viewContainer) {
            if (unlikely(![viewContainer respondsToSelector:@selector(isKindOfClass:)])) {
                msg_Err(_voutDisplay, "void pointer not an ObjC object");
                return;
            }

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

            [sys->viewContainer performSelectorOnMainThread:@selector(addSubview:)
                                                 withObject:self
                                              waitUntilDone:YES];

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
    [super dealloc];
}

- (void)didMoveToWindow
{
    self.contentScaleFactor = self.window.screen.scale;
    _bufferNeedReset = YES;
}

- (void)createBuffers
{
    glDisable(GL_DEPTH_TEST);

    glGenFramebuffers(1, &_frameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, _frameBuffer);

    glGenRenderbuffers(1, &_renderBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _renderBuffer);

    [_eaglContext renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer *)self.layer];

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _renderBuffer);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        if (_voutDisplay)
            msg_Err(_voutDisplay, "Failed to make complete framebuffer object %x", glCheckFramebufferStatus(GL_FRAMEBUFFER));
    }
}

- (void)destroyBuffers
{
    /* re-set current context */
    EAGLContext *previousContext = [EAGLContext currentContext];
    [EAGLContext setCurrentContext:_eaglContext];

    /* clear frame buffer */
    glDeleteFramebuffers(1, &_frameBuffer);
    _frameBuffer = 0;

    /* clear render buffer */
    glDeleteRenderbuffers(1, &_renderBuffer);
    _renderBuffer = 0;
    [EAGLContext setCurrentContext:previousContext];
}

- (void)resetBuffers
{
    if (unlikely(_bufferNeedReset)) {
        [self destroyBuffers];
        [self createBuffers];
        _bufferNeedReset = NO;
    }
}

- (void)lock
{
    _previousEaglContext = [EAGLContext currentContext];
    [EAGLContext setCurrentContext:_eaglContext];
}

- (void)unlock
{
    [EAGLContext setCurrentContext:_previousEaglContext];
}

- (void)layoutSubviews
{
    [self reshape];

    _bufferNeedReset = YES;
}

- (void)reshape
{
    EAGLContext *previousContext = [EAGLContext currentContext];
    [EAGLContext setCurrentContext:_eaglContext];

    CGSize viewSize = [self bounds].size;

    vout_display_place_t place;

    @synchronized (self) {
        if (_voutDisplay) {
            vout_display_cfg_t cfg_tmp = *(_voutDisplay->cfg);
            CGFloat scaleFactor = self.contentScaleFactor;

            cfg_tmp.display.width  = viewSize.width * scaleFactor;
            cfg_tmp.display.height = viewSize.height * scaleFactor;

            vout_display_PlacePicture(&place, &_voutDisplay->source, &cfg_tmp, false);
            _voutDisplay->sys->place = place;
            vout_display_SendEventDisplaySize(_voutDisplay, viewSize.width * scaleFactor,
                                              viewSize.height * scaleFactor);
        }
    }

    // x / y are top left corner, but we need the lower left one
    glViewport(place.x, place.y, place.width, place.height);
    [EAGLContext setCurrentContext:previousContext];
}

- (void)tapRecognized:(UITapGestureRecognizer *)tapRecognizer
{
    UIGestureRecognizerState state = [tapRecognizer state];
    CGPoint touchPoint = [tapRecognizer locationInView:self];
    CGFloat scaleFactor = self.contentScaleFactor;
    vout_display_SendMouseMovedDisplayCoordinates(_voutDisplay, ORIENT_NORMAL,
                                                  (int)touchPoint.x * scaleFactor, (int)touchPoint.y * scaleFactor,
                                                  &_voutDisplay->sys->place);

    vout_display_SendEventMousePressed(_voutDisplay, MOUSE_BUTTON_LEFT);
    vout_display_SendEventMouseReleased(_voutDisplay, MOUSE_BUTTON_LEFT);
}

- (void)applicationStateChanged:(NSNotification *)notification
{
    @synchronized (self) {
    if ([[notification name] isEqualToString:UIApplicationWillResignActiveNotification]
        || [[notification name] isEqualToString:UIApplicationDidEnterBackgroundNotification]
        || [[notification name] isEqualToString:UIApplicationWillTerminateNotification])
        _appActive = NO;
    else
        _appActive = YES;
    }
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
