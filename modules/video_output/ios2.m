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
#import "opengl.h"

/**
 * Forward declarations
 */

enum
{
    UNIFORM_Y,
    UNIFORM_UV,
    UNIFORM_COLOR_CONVERSION_MATRIX,
    UNIFORM_TRANSFORM_MATRIX,
    NUM_UNIFORMS
};
GLint uniforms[NUM_UNIFORMS];

// Attribute index.
enum
{
    ATTRIB_VERTEX,
    ATTRIB_TEXCOORD,
    NUM_ATTRIBUTES
};

struct picture_sys_t {
    CVPixelBufferRef pixelBuffer;
};

// BT.601, which is the standard for SDTV.
static const GLfloat kColorConversion601[] = {
    1.164383561643836,  1.164383561643836, 1.164383561643836,
                  0.0, -0.391762290094914, 2.017232142857142,
    1.596026785714286, -0.812967647237771,               0.0,
};

// BT.709, which is the standard for HDTV.
static const GLfloat kColorConversion709[] = {
    1.164383561643836,  1.164383561643836, 1.164383561643836,
                  0.0,  -0.21324861427373, 2.112401785714286,
    1.792741071428571, -0.532909328559444,               0.0,
};

static NSString *const fragmentShaderString = @" \
 varying highp vec2 texCoordVarying; \
 precision mediump float; \
\
 uniform sampler2D SamplerY; \
 uniform sampler2D SamplerUV; \
 uniform mat3 colorConversionMatrix; \
\
 void main() \
 { \
    mediump vec3 yuv; \
    lowp vec3 rgb; \
\
    yuv.x = (texture2D(SamplerY, texCoordVarying).r - (16.0/255.0)); \
    yuv.yz = (texture2D(SamplerUV, texCoordVarying).rg - vec2(0.5, 0.5)); \
\
    rgb = colorConversionMatrix * yuv; \
\
    gl_FragColor = vec4(rgb,1); \
 } \
";

static NSString *const vertexShaderString = @" \
 attribute vec4 position; \
 attribute vec2 texCoord; \
 uniform mat4 transformMatrix; \
\
 varying vec2 texCoordVarying; \
\
 void main() \
 { \
    gl_Position = position * transformMatrix; \
    texCoordVarying = texCoord; \
 } \
";

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

static picture_pool_t* PicturePool(vout_display_t *, unsigned);
static void PictureRender(vout_display_t *, picture_t *, subpicture_t *);
static void PictureDisplay(vout_display_t *, picture_t *, subpicture_t *);
static int Control(vout_display_t*, int, va_list);

static void *OurGetProcAddress(vlc_gl_t *, const char *);

static int OpenglESClean(vlc_gl_t *);
static void OpenglESSwap(vlc_gl_t *);

static picture_pool_t *ZeroCopyPicturePool(vout_display_t *, unsigned);
static void DestroyZeroCopyPoolPicture(picture_t *);
static void ZeroCopyClean(vout_display_t *vd, picture_t *pic, subpicture_t *subpicture);
static void ZeroCopyDisplay(vout_display_t *, picture_t *, subpicture_t *);

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
    bool _zeroCopy;

    CVOpenGLESTextureCacheRef _videoTextureCache;
    CVOpenGLESTextureRef _lumaTexture;
    CVOpenGLESTextureRef _chromaTexture;

    const GLfloat *_preferredConversion;
}
@property (readonly) GLuint renderBuffer;
@property (readonly) GLuint frameBuffer;
@property (readwrite) vout_display_t* voutDisplay;
@property (readonly) EAGLContext* eaglContext;
@property (readonly) BOOL isAppActive;
@property GLuint shaderProgram;

- (id)initWithFrame:(CGRect)frame zeroCopy:(bool)zero_copy voutDisplay:(vout_display_t *)vd;

- (void)createBuffers;
- (void)destroyBuffers;
- (void)resetBuffers;

- (void)reshape;

- (void)setupZeroCopyGL;
- (void)displayPixelBuffer:(CVPixelBufferRef)pixelBuffer;
@end

struct vout_display_sys_t
{
    VLCOpenGLES2VideoView *glESView;
    UIView *viewContainer;
    UITapGestureRecognizer *tapRecognizer;

    vlc_gl_t gl;
    vout_display_opengl_t *vgl;

    picture_pool_t *picturePool;
    bool has_first_frame;
    bool zero_copy;

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

    if (vout_display_IsWindowed(vd))
        return VLC_EGENERIC;

    vout_display_sys_t *sys = calloc (1, sizeof(*sys));

    if (!sys)
        return VLC_ENOMEM;

    vd->sys = sys;
    sys->picturePool = NULL;
    sys->gl.sys = NULL;

    @autoreleasepool {
        if (vd->fmt.i_chroma == VLC_CODEC_CVPX_OPAQUE) {
            msg_Dbg(vd, "will use zero-copy rendering");
            sys->zero_copy = true;
        }

        /* setup the actual OpenGL ES view */
        sys->glESView = [[VLCOpenGLES2VideoView alloc] initWithFrame:CGRectMake(0.,0.,320.,240.) zeroCopy:sys->zero_copy voutDisplay:vd];
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
        if (!sys->zero_copy) {
            msg_Dbg(vd, "will use regular OpenGL rendering");
            /* Initialize common OpenGL video display */
            sys->gl.lock = OpenglESClean;
            sys->gl.unlock = nil;
            sys->gl.swap = OpenglESSwap;
            sys->gl.getProcAddress = OurGetProcAddress;
            sys->gl.sys = sys;

            sys->vgl = vout_display_opengl_New(&vd->fmt, &subpicture_chromas, &sys->gl,
                                               &vd->cfg->viewpoint);
            if (!sys->vgl) {
                sys->gl.sys = NULL;
                goto bailout;
            }
        } else {
            subpicture_chromas = gl_subpicture_chromas;
        }

        /* */
        vout_display_info_t info = vd->info;
        info.has_pictures_invalid = false;
        info.has_event_thread = true;
        info.subpicture_chromas = subpicture_chromas;
        info.is_slow = !sys->zero_copy;
        info.has_hide_mouse = false;

        /* Setup vout_display_t once everything is fine */
        vd->info = info;

        if (sys->zero_copy) {
            vd->pool = ZeroCopyPicturePool;
            vd->prepare = ZeroCopyClean;
            vd->display = ZeroCopyDisplay;
        } else {
            vd->pool = PicturePool;
            vd->prepare = PictureRender;
            vd->display = PictureDisplay;
        }
        vd->control = Control;
        vd->manage = NULL;

        /* forward our dimensions to the vout core */
        CGFloat scaleFactor;
        CGSize viewSize;
        @synchronized(sys->viewContainer) {
            scaleFactor = sys->viewContainer.contentScaleFactor;
            viewSize = sys->viewContainer.bounds.size;
        }
        vout_display_SendEventFullscreen(vd, false);
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

        if (sys->gl.sys != NULL) {
            @synchronized (sys->glESView) {
                msg_Dbg(this, "deleting display");

                if (likely([sys->glESView isAppActive]))
                    vout_display_opengl_Delete(sys->vgl);
            }
        }

        [sys->glESView release];

        /* when using the traditional pipeline, the cross-platform code will free the the pool */
        if (sys->zero_copy) {
            if (sys->picturePool)
                picture_pool_Release(sys->picturePool);
            sys->picturePool = NULL;
        }
        
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

                // x / y are top left corner, but we need the lower left one
                if (query != VOUT_DISPLAY_CHANGE_DISPLAY_SIZE)
                    glViewport(place.x, cfg_tmp.display.height - (place.y + place.height), place.width, place.height);
            }
            return VLC_SUCCESS;
        }

        case VOUT_DISPLAY_CHANGE_VIEWPOINT:
            return vout_display_opengl_SetViewpoint(sys->vgl,
                &va_arg (ap, const vout_display_cfg_t* )->viewpoint);

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
            vout_display_opengl_Display(sys->vgl, &vd->source);
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
 * zero copy display callbacks
 *****************************************************************************/

static picture_pool_t *ZeroCopyPicturePool(vout_display_t *vd, unsigned requested_count)
{
    vout_display_sys_t *sys = vd->sys;
    if (sys->picturePool != NULL)
        return sys->picturePool;

    picture_t** pictures = calloc(requested_count, sizeof(*pictures));
    if (!pictures)
        goto bailout;

    for (unsigned x = 0; x < requested_count; x++) {
        picture_sys_t *picsys = calloc(1, sizeof(*picsys));
        if (unlikely(!picsys)) {
            goto bailout;
        }

        picture_resource_t picture_resource;
        picture_resource.p_sys = picsys;
        picture_resource.pf_destroy = DestroyZeroCopyPoolPicture;

        picture_t *picture = picture_NewFromResource(&vd->fmt, &picture_resource);
        if (unlikely(picture == NULL)) {
            free(picsys);
            goto bailout;
        }

        pictures[x] = picture;
    }

    picture_pool_configuration_t pool_config;
    memset(&pool_config, 0, sizeof(pool_config));
    pool_config.picture_count = requested_count;
    pool_config.picture = pictures;

    sys->picturePool = picture_pool_NewExtended(&pool_config);

bailout:
    if (sys->picturePool == NULL && pictures) {
        for (unsigned x = 0; x < requested_count; x++)
            DestroyZeroCopyPoolPicture(pictures[x]);
        free(pictures);
    }

    return sys->picturePool;
}

static void DestroyZeroCopyPoolPicture(picture_t *picture)
{
    picture_sys_t *p_sys = (picture_sys_t *)picture->p_sys;

    if (p_sys->pixelBuffer != nil) {
        CFRelease(p_sys->pixelBuffer);
        p_sys->pixelBuffer = nil;
    }

    free(p_sys);
    free(picture);
}

static void ZeroCopyClean(vout_display_t *vd, picture_t *pic, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    if (likely([sys->glESView isAppActive]))
        [sys->glESView resetBuffers];
}

static void ZeroCopyDisplay(vout_display_t *vd, picture_t *pic, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    sys->has_first_frame = true;
    @synchronized (sys->glESView) {
        if (likely([sys->glESView isAppActive])) {
            if (pic->p_sys != NULL) {
                picture_sys_t *picsys = pic->p_sys;

                if (picsys->pixelBuffer != nil) {
                    [sys->glESView displayPixelBuffer: picsys->pixelBuffer];
                }
            }
        }
    }

    picture_Release(pic);

    if (subpicture)
        subpicture_Delete(subpicture);
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

- (id)initWithFrame:(CGRect)frame zeroCopy:(bool)zero_copy voutDisplay:(vout_display_t *)vd
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
    if (zero_copy) {
        _preferredConversion = kColorConversion709;
        [self setupZeroCopyGL];
    }

    [self reshape];
    [self setAutoresizingMask: UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];

    _zeroCopy = zero_copy;

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
    if (_zeroCopy) {
        [self cleanUpTextures];

        if(likely(_videoTextureCache))
            CFRelease(_videoTextureCache);
    }

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

    glEnableVertexAttribArray(ATTRIB_VERTEX);
    glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), 0);

    glEnableVertexAttribArray(ATTRIB_TEXCOORD);
    glVertexAttribPointer(ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), 0);

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
        EAGLContext *previousContext = [EAGLContext currentContext];
        [EAGLContext setCurrentContext:_eaglContext];

        [self destroyBuffers];
        [self createBuffers];
        _bufferNeedReset = NO;

        [EAGLContext setCurrentContext:previousContext];
    }
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

- (void)displayPixelBuffer:(CVPixelBufferRef)pixelBuffer
{
    /* the currently current context may not be ours, so cache it and restore it later */
    EAGLContext *previousContext = [EAGLContext currentContext];
    [EAGLContext setCurrentContext:_eaglContext];

    CVReturn err;
    if (pixelBuffer != NULL) {
        int frameWidth = (int)CVPixelBufferGetWidth(pixelBuffer);
        int frameHeight = (int)CVPixelBufferGetHeight(pixelBuffer);

        if (!_videoTextureCache) {
            if (_voutDisplay)
                msg_Err(_voutDisplay, "No video texture cache");
            goto done;
        }

        [self cleanUpTextures];

        /* Use the color attachment of the pixel buffer to determine the appropriate color conversion matrix. */
        CFTypeRef colorAttachments = CVBufferGetAttachment(pixelBuffer, kCVImageBufferYCbCrMatrixKey, NULL);

        if (colorAttachments == kCVImageBufferYCbCrMatrix_ITU_R_601_4)
            _preferredConversion = kColorConversion601;
        else
            _preferredConversion = kColorConversion709;

        /* Create Y and UV textures from the pixel buffer.
         * These textures will be drawn on the frame buffer Y-plane. */
        glActiveTexture(GL_TEXTURE0);

        err = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                           _videoTextureCache,
                                                           pixelBuffer,
                                                           NULL,
                                                           GL_TEXTURE_2D,
                                                           GL_RED_EXT,
                                                           frameWidth,
                                                           frameHeight,
                                                           GL_RED_EXT,
                                                           GL_UNSIGNED_BYTE,
                                                           0,
                                                           &_lumaTexture);
        if (err) {
            if (_voutDisplay)
                msg_Err(_voutDisplay, "Error at CVOpenGLESTextureCacheCreateTextureFromImage %d", err);
        }

        glBindTexture(CVOpenGLESTextureGetTarget(_lumaTexture), CVOpenGLESTextureGetName(_lumaTexture));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // UV-plane.
        glActiveTexture(GL_TEXTURE1);
        err = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                           _videoTextureCache,
                                                           pixelBuffer,
                                                           NULL,
                                                           GL_TEXTURE_2D,
                                                           GL_RG_EXT,
                                                           frameWidth / 2,
                                                           frameHeight / 2,
                                                           GL_RG_EXT,
                                                           GL_UNSIGNED_BYTE,
                                                           1,
                                                           &_chromaTexture);
        if (err) {
            if (_voutDisplay)
                msg_Err(_voutDisplay, "Error at CVOpenGLESTextureCacheCreateTextureFromImage %d", err);
        }

        glBindTexture(CVOpenGLESTextureGetTarget(_chromaTexture), CVOpenGLESTextureGetName(_chromaTexture));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindFramebuffer(GL_FRAMEBUFFER, _frameBuffer);
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Use shader program.
    glUseProgram(self.shaderProgram);
    glUniformMatrix3fv(uniforms[UNIFORM_COLOR_CONVERSION_MATRIX], 1, GL_FALSE, _preferredConversion);

    GLfloat transformMatrix[16];
    orientationTransformMatrix(transformMatrix, _voutDisplay->fmt.orientation);
    glUniformMatrix4fv(uniforms[UNIFORM_TRANSFORM_MATRIX], 1, GL_FALSE, transformMatrix);

    // Set up the quad vertices with respect to the orientation and aspect ratio of the video.
    CGRect vertexSamplingRect = self.bounds;

    // Compute normalized quad coordinates to draw the frame into.
    CGSize normalizedSamplingSize = CGSizeMake(0.0, 0.0);
    CGSize cropScaleAmount = CGSizeMake(vertexSamplingRect.size.width/self.bounds.size.width, vertexSamplingRect.size.height/self.bounds.size.height);

    // Normalize the quad vertices.
    if (cropScaleAmount.width > cropScaleAmount.height) {
        normalizedSamplingSize.width = 1.0;
        normalizedSamplingSize.height = cropScaleAmount.height/cropScaleAmount.width;
    }
    else {
        normalizedSamplingSize.width = 1.0;
        normalizedSamplingSize.height = cropScaleAmount.width/cropScaleAmount.height;
    }

    /*
     The quad vertex data defines the region of 2D plane onto which we draw our pixel buffers.
     Vertex data formed using (-1,-1) and (1,1) as the bottom left and top right coordinates respectively, covers the entire screen.
     */
    GLfloat quadVertexData [] = {
        -1 * normalizedSamplingSize.width, -1 * normalizedSamplingSize.height,
        normalizedSamplingSize.width, -1 * normalizedSamplingSize.height,
        -1 * normalizedSamplingSize.width, normalizedSamplingSize.height,
        normalizedSamplingSize.width, normalizedSamplingSize.height,
    };

    // Update attribute values.
    glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, quadVertexData);
    glEnableVertexAttribArray(ATTRIB_VERTEX);

    /*
     The texture vertices are set up such that we flip the texture vertically. This is so that our top left origin buffers match OpenGL's bottom left texture coordinate system.
     */
    CGRect textureSamplingRect = CGRectMake(0, 0, 1, 1);
    GLfloat quadTextureData[] =  {
        CGRectGetMinX(textureSamplingRect), CGRectGetMaxY(textureSamplingRect),
        CGRectGetMaxX(textureSamplingRect), CGRectGetMaxY(textureSamplingRect),
        CGRectGetMinX(textureSamplingRect), CGRectGetMinY(textureSamplingRect),
        CGRectGetMaxX(textureSamplingRect), CGRectGetMinY(textureSamplingRect)
    };

    glVertexAttribPointer(ATTRIB_TEXCOORD, 2, GL_FLOAT, 0, 0, quadTextureData);
    glEnableVertexAttribArray(ATTRIB_TEXCOORD);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindRenderbuffer(GL_RENDERBUFFER, _renderBuffer);
    [_eaglContext presentRenderbuffer:GL_RENDERBUFFER];

    glFlush();

done:
    /* restore previous eagl context which we cached on entry */
    [EAGLContext setCurrentContext:previousContext];
}

- (void)setupZeroCopyGL
{
    EAGLContext *previousContext = [EAGLContext currentContext];
    [EAGLContext setCurrentContext:_eaglContext];
    [self loadShaders];

    glUseProgram(self.shaderProgram);

    // 0 and 1 are the texture IDs of _lumaTexture and _chromaTexture respectively.
    glUniform1i(uniforms[UNIFORM_Y], 0);
    glUniform1i(uniforms[UNIFORM_UV], 1);

    GLfloat transformMatrix[16];
    orientationTransformMatrix(transformMatrix, _voutDisplay->fmt.orientation);
    glUniformMatrix4fv(uniforms[UNIFORM_TRANSFORM_MATRIX], 1, GL_FALSE, transformMatrix);

    glUniformMatrix3fv(uniforms[UNIFORM_COLOR_CONVERSION_MATRIX], 1, GL_FALSE, _preferredConversion);

    if (!_videoTextureCache) {
        CVReturn err = CVOpenGLESTextureCacheCreate(kCFAllocatorDefault, NULL, _eaglContext, NULL, &_videoTextureCache);
        if (err != noErr) {
            if (_voutDisplay)
                msg_Err(_voutDisplay, "Error at CVOpenGLESTextureCacheCreate %d", err);
        }
    }
    [EAGLContext setCurrentContext:previousContext];
}

- (void)cleanUpTextures
{
    if (_lumaTexture) {
        CFRelease(_lumaTexture);
        _lumaTexture = NULL;
    }

    if (_chromaTexture) {
        CFRelease(_chromaTexture);
        _chromaTexture = NULL;
    }

    // Periodic texture cache flush every frame
    CVOpenGLESTextureCacheFlush(_videoTextureCache, 0);
}

- (BOOL)loadShaders
{
    GLuint vertShader, fragShader;
    NSURL *vertShaderURL, *fragShaderURL;

    // Create the shader program.
    self.shaderProgram = glCreateProgram();

    // Create and compile the vertex shader.
    if (![self compileShader:&vertShader type:GL_VERTEX_SHADER sourceString:vertexShaderString]) {
        if (_voutDisplay)
            msg_Err(_voutDisplay, "Failed to compile vertex shader");
        return NO;
    }

    // Create and compile fragment shader.
    if (![self compileShader:&fragShader type:GL_FRAGMENT_SHADER sourceString:fragmentShaderString]) {
        if (_voutDisplay)
            msg_Err(_voutDisplay, "Failed to compile fragment shader");
        return NO;
    }

    // Attach vertex shader to program.
    glAttachShader(self.shaderProgram, vertShader);

    // Attach fragment shader to program.
    glAttachShader(self.shaderProgram, fragShader);

    // Bind attribute locations. This needs to be done prior to linking.
    glBindAttribLocation(self.shaderProgram, ATTRIB_VERTEX, "position");
    glBindAttribLocation(self.shaderProgram, ATTRIB_TEXCOORD, "texCoord");

    // Link the program.
    if (![self linkProgram:self.shaderProgram]) {
        if (_voutDisplay)
            msg_Err(_voutDisplay, "Failed to link program: %d", self.shaderProgram);

        if (vertShader) {
            glDeleteShader(vertShader);
            vertShader = 0;
        }
        if (fragShader) {
            glDeleteShader(fragShader);
            fragShader = 0;
        }
        if (self.shaderProgram) {
            glDeleteProgram(self.shaderProgram);
            self.shaderProgram = 0;
        }

        return NO;
    }

    // Get uniform locations.
    uniforms[UNIFORM_Y] = glGetUniformLocation(self.shaderProgram, "SamplerY");
    uniforms[UNIFORM_UV] = glGetUniformLocation(self.shaderProgram, "SamplerUV");
    uniforms[UNIFORM_COLOR_CONVERSION_MATRIX] = glGetUniformLocation(self.shaderProgram, "colorConversionMatrix");
    uniforms[UNIFORM_TRANSFORM_MATRIX] = glGetUniformLocation(self.shaderProgram, "transformMatrix");

    // Release vertex and fragment shaders.
    if (vertShader) {
        glDetachShader(self.shaderProgram, vertShader);
        glDeleteShader(vertShader);
    }
    if (fragShader) {
        glDetachShader(self.shaderProgram, fragShader);
        glDeleteShader(fragShader);
    }

    return YES;
}

- (BOOL)compileShader:(GLuint *)shader type:(GLenum)type sourceString:sourceString
{
    GLint status;
    const GLchar *source;
    source = (GLchar *)[sourceString UTF8String];

    *shader = glCreateShader(type);
    glShaderSource(*shader, 1, &source, NULL);
    glCompileShader(*shader);

#ifndef NDEBUG
    GLint logLength;
    glGetShaderiv(*shader, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0) {
        GLchar *log = (GLchar *)malloc(logLength);
        glGetShaderInfoLog(*shader, logLength, &logLength, log);
        if (_voutDisplay)
            msg_Dbg(_voutDisplay, "Shader compile log:\n%s", log);
        free(log);
    }
#endif

    glGetShaderiv(*shader, GL_COMPILE_STATUS, &status);
    if (status == 0) {
        glDeleteShader(*shader);
        return NO;
    }

    return YES;
}

- (BOOL)linkProgram:(GLuint)prog
{
    GLint status;
    glLinkProgram(prog);

#ifndef NDEBUG
    GLint logLength;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0) {
        GLchar *log = (GLchar *)malloc(logLength);
        glGetProgramInfoLog(prog, logLength, &logLength, log);
        if (_voutDisplay)
            msg_Dbg(_voutDisplay, "Program link log:\n%s", log);
        free(log);
    }
#endif

    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (status == 0) {
        return NO;
    }

    return YES;
}

@end
