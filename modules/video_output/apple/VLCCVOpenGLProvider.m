/*****************************************************************************
 * VLCCVOpenGLProvider.m: iOS OpenGL ES offscreen provider backed by
 *                        CVPixelBuffer supporting both iOS/tvOS and MacOSX
 *****************************************************************************
 * Copyright (C) 2021 Videolabs
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
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

#ifdef HAVE_CONFIG_H
# import "config.h"
#endif

#import <TargetConditionals.h>

#if TARGET_OS_IPHONE
# import <OpenGLES/EAGL.h>
# import <OpenGLES/ES2/gl.h>
# import <CoreVideo/CVOpenGLESTextureCache.h>
#else
# import <Cocoa/Cocoa.h>
# import <OpenGL/OpenGL.h>
# import <OpenGL/gl.h>
#endif

#import <CoreVideo/CoreVideo.h>
#import <dlfcn.h>

#import <vlc_common.h>
#import <vlc_filter.h>
#import <vlc_picture.h>
#import <vlc_plugin.h>
#import <vlc_opengl.h>
#import <vlc_opengl_platform.h>
#import <vlc_picture_pool.h>

#import "codec/vt_utils.h"
#import "video_output/opengl/vout_helper.h"

#define BUFFER_COUNT 5

struct vlc_cvbuffer {
    CVPixelBufferRef cvpx;
    GLuint           fbo;

#if TARGET_OS_IPHONE
    CVOpenGLESTextureRef texture;
#else
    CVOpenGLTextureRef texture;
#endif
};

@interface VLCCVOpenGLProvider: NSObject
{
    vlc_gl_t *_gl;

#if TARGET_OS_IPHONE
    EAGLContext* _context;
    EAGLContext* _previousContext;
    CVOpenGLESTextureCacheRef _textureCache;
#else
    CGLContextObj _context;
    CGLContextObj _previousContext;
    CVOpenGLTextureCacheRef _textureCache;
#endif

    /* Framebuffers are stored into the vlc_cvbuffer object for convenience
     * when getting a picture from the pool, but it's also stored there for
     * allocation/deallocation, since:
     *  1/ The pictures won't need the framebuffer after being swapped.
     *  2/ The picture cannot release the framebuffer at release since it
     *     needs an OpenGL context.
     *  3/ We can delete the framebuffer as soon as we won't draw into the
     *     frame anymore.
     * Note that framebuffers could be reused too when resizing, but this is
     * a bit inconvenient to implement. */
    GLuint _fbos[BUFFER_COUNT];

    struct vlc_video_context *_vctx_out;
    picture_pool_t *_pool;
    picture_t *_currentPicture;
    size_t _countCurrent;

    video_format_t _fmt_out;
}

- (id)initWithGL:(vlc_gl_t*)gl width:(unsigned)width height:(unsigned)height;
- (void)makeCurrent;
- (void)releaseCurrent;
- (picture_t*)swap;
- (int)resize:(CGSize)size;

@end

/*****************************************************************************
 * vout opengl callbacks
 *****************************************************************************/
static void *GetSymbol(vlc_gl_t *gl, const char *name)
{
    VLC_UNUSED(gl);

    return dlsym(RTLD_DEFAULT, name);
}

static int MakeCurrent(vlc_gl_t *gl)
{
    VLCCVOpenGLProvider *context = (__bridge VLCCVOpenGLProvider*) gl->sys;

    [context makeCurrent];
    return VLC_SUCCESS;
}

static void ReleaseCurrent(vlc_gl_t *gl)
{
    VLCCVOpenGLProvider *context = (__bridge VLCCVOpenGLProvider*)gl->sys;
    [context releaseCurrent];
}

static picture_t* Swap(vlc_gl_t *gl)
{
    VLCCVOpenGLProvider *context = (__bridge VLCCVOpenGLProvider*)gl->sys;
    return [context swap];
}

static void Resize(vlc_gl_t *gl, unsigned width, unsigned height)
{
    VLCCVOpenGLProvider *context = (__bridge VLCCVOpenGLProvider*)gl->sys;
    [context resize:CGSizeMake(width, height)];
}

static void Close(vlc_gl_t *gl)
{
    VLCCVOpenGLProvider *context = (__bridge_transfer VLCCVOpenGLProvider*)gl->sys;
    gl->sys = nil;

    /* context has been transferred and gl->sys won't track it now, so we can
     * let ARC release it. */
    (void)context;
}

static void FreeCVBuffer(picture_t *picture)
{
    struct vlc_cvbuffer *buffer = picture->p_sys;
    if (buffer->texture)
        CFRelease(buffer->texture);
    if (buffer->cvpx)
        CFRelease(buffer->cvpx);
    free(buffer);
}


@implementation VLCCVOpenGLProvider

- (picture_t *)initBuffer
{
    struct vlc_cvbuffer *buffer = malloc(sizeof *buffer);
    if (buffer == NULL)
        return NULL;
    buffer->texture = NULL;
    buffer->cvpx = NULL;
    buffer->fbo = 0;

    /* CoreVideo functions will use this variable for error reporting. */
    CVReturn cvret;
    unsigned width  = _fmt_out.i_visible_width;
    unsigned height = _fmt_out.i_visible_height;

    /* The buffer are IOSurface-backed, we don't map them to CPU memory. */
    const picture_resource_t resource = {
        .p_sys = buffer,
        .pf_destroy = FreeCVBuffer,
    };
    picture_t *picture = picture_NewFromResource(&_fmt_out, &resource);
    if (picture == NULL)
    {
        free(buffer);
        return NULL;
    }

    NSDictionary *cvpx_attr = @{
        (__bridge NSString*)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
        (__bridge NSString*)kCVPixelBufferWidthKey: @(width),
        (__bridge NSString*)kCVPixelBufferHeightKey: @(height),
        (__bridge NSString*)kCVPixelBufferBytesPerRowAlignmentKey: @(16),
        /* Necessary for having iosurface-backed CVPixelBuffer, but
         * note that iOS simulator won't be able to display them. */
        (__bridge NSString*)kCVPixelBufferIOSurfacePropertiesKey: @{},
#if TARGET_OS_IPHONE
        (__bridge NSString*)kCVPixelBufferOpenGLESCompatibilityKey : @YES,
        (__bridge NSString*)kCVPixelBufferIOSurfaceOpenGLESFBOCompatibilityKey: @YES,
        (__bridge NSString*)kCVPixelBufferIOSurfaceOpenGLESTextureCompatibilityKey: @YES,
#endif
    };

    cvret = CVPixelBufferCreate(kCFAllocatorDefault, width, height,
        kCVPixelFormatType_32BGRA, (__bridge CFDictionaryRef)cvpx_attr,
        &buffer->cvpx);

    if (cvret != kCVReturnSuccess)
    {
        picture_Release(picture);
        return nil;
    }

    /* The CVPX buffer will be hold by the picture_t. */
    cvpxpic_attach(picture, buffer->cvpx, _vctx_out, NULL);

#if TARGET_OS_IPHONE
    cvret = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
            _textureCache, buffer->cvpx,
            nil,                                  // CFDictionaryRef textureAttributes
            GL_TEXTURE_2D,                        // GLenum target
            GL_RGBA,                              // GLint internalFormat
            _fmt_out.i_visible_width,             // GLsizei width
            _fmt_out.i_visible_height,            // GLsizei height
            GL_BGRA,                              // GLenum format for native data
            GL_UNSIGNED_BYTE,                     // GLenum type
            0,                                    // size_t planeIndex
            &buffer->texture);

    if (cvret != kCVReturnSuccess)
    {
        picture_Release(picture);
        return nil;
    }

    assert(CVOpenGLESTextureGetTarget(buffer->texture)
            == GL_TEXTURE_2D);

    GLuint name = CVOpenGLESTextureGetName(buffer->texture);
    GLenum target = CVOpenGLESTextureGetTarget(buffer->texture);

#else

    cvret = CVOpenGLTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
            _textureCache, buffer->cvpx,
            nil, &buffer->texture);

    if (cvret != kCVReturnSuccess)
    {
        picture_Release(picture);
        return nil;
    }

    assert(CVOpenGLTextureGetTarget(buffer->texture)
            == GL_TEXTURE_RECTANGLE);

    GLenum target = CVOpenGLTextureGetTarget(buffer->texture);
    GLuint name = CVOpenGLTextureGetName(buffer->texture);
#endif

    glGenFramebuffers(1, &buffer->fbo);

    glBindTexture(target, name);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, buffer->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            target, name, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    assert(status == GL_FRAMEBUFFER_COMPLETE);
    assert(cvret == kCVReturnSuccess);

    return picture;
}

- (id)initWithGL:(vlc_gl_t*)gl width:(unsigned)width height:(unsigned)height
{
    _gl = gl;
    _context = nil;
    _previousContext = nil;
    _textureCache = nil;
    _pool = nil;
    _currentPicture = nil;

    /* CoreVideo functions will use this variable for error reporting. */
    CVReturn cvret;

    video_format_Init(&_fmt_out, VLC_CODEC_CVPX_BGRA);

    _fmt_out.i_visible_width
        = _fmt_out.i_width
        = width;

    _fmt_out.i_visible_height
        = _fmt_out.i_height
        = height;

    static struct vlc_video_context_operations vctx_ops =
        { .destroy = NULL };

    _vctx_out = vlc_video_context_CreateCVPX(
        gl->device, CVPX_VIDEO_CONTEXT_DEFAULT, sizeof(VLCCVOpenGLProvider*),
        &vctx_ops);

    if (_vctx_out == NULL)
        return nil;

#if TARGET_OS_IPHONE
    _context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    if (_context == nil)
        return nil;

    cvret = CVOpenGLESTextureCacheCreate(kCFAllocatorDefault,
            nil, _context, nil, &_textureCache);

#else
    CGLPixelFormatAttribute pixel_attr[] = {
        kCGLPFAAccelerated,
        kCGLPFAAllowOfflineRenderers,
        0,
    };

    CGLPixelFormatObj pixelFormat;
    GLint numPixelFormats = 0;

    CGLChoosePixelFormat(pixel_attr, &pixelFormat, &numPixelFormats);
    CGLError cglerr = CGLCreateContext(pixelFormat, NULL, &_context);
    CGLDestroyPixelFormat(pixelFormat);
    if (cglerr != kCGLNoError)
        return nil;

    cvret = CVOpenGLTextureCacheCreate(kCFAllocatorDefault,
        nil, _context, pixelFormat, nil, &_textureCache);
#endif
    if (cvret != kCVReturnSuccess)
        return nil;

    /* OpenGL context is now current, so we can use OpenGL functions. */
    if ([self resize:CGSizeMake(width, height)] != VLC_SUCCESS)
        return nil;

    static const struct vlc_gl_operations gl_ops =
    {
        .make_current = MakeCurrent,
        .release_current = ReleaseCurrent,
        .resize = Resize,
        .swap_offscreen = Swap,
        .get_proc_address = GetSymbol,
        .close = Close,
    };
    gl->ops = &gl_ops;
    gl->offscreen_vflip = true;
    gl->offscreen_vctx_out = _vctx_out;
    gl->offscreen_chroma_out = VLC_CODEC_CVPX_BGRA;

    return self;
}

- (void)makeCurrent
{
    /* TODO: check if it works with app inactive */
    if (_countCurrent++ > 0)
        return;

#if TARGET_OS_IPHONE
    _previousContext = [EAGLContext currentContext];
    [EAGLContext setCurrentContext:_context];
#else
    _previousContext = CGLGetCurrentContext();
    CGLSetCurrentContext(_context);
#endif
}

- (void)releaseCurrent
{
    if (--_countCurrent > 0)
        return;

#if TARGET_OS_IPHONE
    [EAGLContext setCurrentContext:_previousContext];
#else
    CGLSetCurrentContext(_previousContext);
#endif

    _previousContext = nil;
}

- (picture_t*)swap
{
    /* EAGLContext has no formal swap operation but we swap the backing
     * CVPX buffer ourselves. */

    /* Note:
     * The result must be used after completion of the rendering, meaning it must wait
     * for completion of a fence or call glFinish:
     *  https://www.khronos.org/registry/OpenGL/extensions/APPLE/APPLE_sync.txt
     * In practice, it seems that implicit sync is enough, probably because of the
     * actual context changes, but it might need a proper synchronization mechanism
     * in the future. */

    [self makeCurrent];

    picture_t *output = _currentPicture;
    output->p_sys = NULL;
    glFinish();

    picture_t *next_picture = picture_pool_Wait(_pool);
    struct vlc_cvbuffer *buffer = next_picture->p_sys;
    assert(buffer != NULL);

    _currentPicture = next_picture;
    // TODO: rebind at makeCurrent instead, if not binded?
    glFinish();
    glBindFramebuffer(GL_FRAMEBUFFER, buffer->fbo);

    [self releaseCurrent];

    return output;
}

- (int)resize:(CGSize)size
{
    [self makeCurrent];

    if (_pool)
    {
        glDeleteFramebuffers(BUFFER_COUNT, _fbos);
        picture_pool_Release(_pool);
    }

    if (_currentPicture)
        picture_Release(_currentPicture);

    picture_t *pictures[BUFFER_COUNT];

    /* OpenGL context is now current, so we can use OpenGL functions.
     * Allocate the pictures and store the framebuffer for later deallocation
     * since framebuffers can only be deleted within an OpenGL context. */
    size_t bufferCount;
    for (bufferCount = 0; bufferCount < BUFFER_COUNT; ++bufferCount)
    {
        picture_t *picture = [self initBuffer];
        if (picture == NULL)
            break;
        struct vlc_cvbuffer *buffer = picture->p_sys;
        _fbos[bufferCount] = buffer->fbo;
        pictures[bufferCount] = picture;
    }
    if (bufferCount != BUFFER_COUNT)
    {
        for (size_t i=0; i<bufferCount; ++i)
            picture_Release(pictures[i]);
        glDeleteFramebuffers(bufferCount, _fbos);
        return VLC_ENOMEM;
    }

    _pool = picture_pool_New(BUFFER_COUNT, pictures);
    if (_pool == nil)
    {
        for (size_t i=0; i<BUFFER_COUNT; ++i)
            picture_Release(pictures[i]);
        glDeleteFramebuffers(BUFFER_COUNT, _fbos);
        return VLC_ENOMEM;
    }

    _currentPicture = picture_pool_Wait(_pool);
    struct vlc_cvbuffer *buffer = _currentPicture->p_sys;
    assert(buffer != NULL);
    glBindFramebuffer(GL_FRAMEBUFFER, buffer->fbo);

    [self releaseCurrent];
    return VLC_SUCCESS;
}

- (void)dealloc
{
    /* Delete OpenGL resources */
    if (_context != nil && _pool != nil)
    {
        [self makeCurrent];
        glFinish();
        /* Delete native resources */
        glDeleteFramebuffers(BUFFER_COUNT, _fbos);
        [self releaseCurrent];
    }

    if (_textureCache != nil)
        CFRelease(_textureCache);

    if (_currentPicture != nil)
        picture_Release(_currentPicture);

    if (_pool != nil)
        picture_pool_Release(_pool);

    if (_vctx_out != nil)
        vlc_video_context_Release(_vctx_out);

    video_format_Clean(&_fmt_out);
}

@end

static int Open(vlc_gl_t *gl, unsigned width, unsigned height)
{
    VLCCVOpenGLProvider *sys = [[VLCCVOpenGLProvider alloc] initWithGL:gl width:width height:height];
    if (sys == nil)
        return VLC_EGENERIC;;

    gl->sys = (__bridge_retained void*)sys;

    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname( N_("cvpx_gl") )
    set_description( N_("OpenGL backed by CVPixelBuffer") )
#if TARGET_OS_IPHONE
    set_capability( "opengl es2 offscreen", 100 )
#else
    set_capability( "opengl offscreen", 100 )
#endif
    add_shortcut( "cvpx_gl" )
    set_callback( Open)
vlc_module_end()
