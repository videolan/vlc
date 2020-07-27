/*****************************************************************************
 * cvpx_gl.m: iOS OpenGL ES offscreen provider baked by CVPX buffer
 *            supporting both iOS and MacOSX
 *****************************************************************************
 * Copyright (C) 2020 Videolabs
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

#import "TargetConditionals.h"

#if TARGET_OS_IPHONE
# import <OpenGLES/EAGL.h>
# import <OpenGLES/ES2/gl.h>
# import <CoreVideo/CVOpenGLESTextureCache.h>
#elif TARGET_OS_MAC
# import <Cocoa/Cocoa.h>
# import <OpenGL/OpenGL.h>
# import <OpenGL/gl.h>
#endif

#import <CoreVideo/CoreVideo.h>
#include <dlfcn.h>

#include <vlc_common.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include <vlc_plugin.h>
#include <vlc_opengl.h>

#include "../codec/vt_utils.h"
#include "../video_output/opengl/vout_helper.h"

struct vlc_video_context_operations vctx_ops = {0};

@interface VLCCVOpenGLContext : NSObject
{
    vlc_gl_t *_gl;

#if TARGET_OS_IPHONE
    EAGLContext* _context;
    EAGLContext* _previousContext;
    CVOpenGLESTextureCacheRef _textureCache;
    CVOpenGLESTextureRef _texture;
    CVOpenGLESTextureRef _textures[2];
#elif TARGET_OS_MAC
    CGLContextObj _context;
    CGLContextObj _previousContext;
    CVOpenGLTextureCacheRef _textureCache;
    CVOpenGLTextureRef _textures[2];
#endif

    CVPixelBufferPoolRef _pool;
    CVPixelBufferRef _buffers[2];
    GLuint _fbos[2];
    int _currentFlip;

    video_format_t _fmt_out;
}

- (id)initWithGL:(vlc_gl_t*)gl width:(unsigned)width height:(unsigned)height;
- (BOOL)makeCurrent;
- (void)releaseCurrent;
- (picture_t*)swap;
- (void)resize:(CGSize)size;

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
    VLCCVOpenGLContext *context = (__bridge VLCCVOpenGLContext*) gl->sys;
    assert(context);

    assert([context makeCurrent]);
    return VLC_SUCCESS;
}

static void ReleaseCurrent(vlc_gl_t *gl)
{
    VLCCVOpenGLContext *context = (__bridge VLCCVOpenGLContext*)gl->sys;
    [context releaseCurrent];
}

static picture_t* Swap(vlc_gl_t *gl)
{
    VLCCVOpenGLContext *context = (__bridge VLCCVOpenGLContext*)gl->sys;
    return [context swap];
}

static void Resize(vlc_gl_t *gl, unsigned width, unsigned height)
{
    VLCCVOpenGLContext *context = (__bridge VLCCVOpenGLContext*)gl->sys;
    [context resize:CGSizeMake(width, height)];
}

static void Close(vlc_gl_t *gl)
{
    VLCCVOpenGLContext *context = (__bridge_transfer VLCCVOpenGLContext*)gl->sys;
    context = nil;
}


@implementation VLCCVOpenGLContext

- (id)initWithGL:(vlc_gl_t*)gl width:(unsigned)width height:(unsigned)height
{
    _gl = gl;
    _previousContext = nil;
    _currentFlip = 0;

    CVReturn cvret;

    video_format_Init(&_fmt_out, VLC_CODEC_CVPX_BGRA);

    _fmt_out.i_visible_width
        = _fmt_out.i_width
        = width;

    _fmt_out.i_visible_height
        = _fmt_out.i_height
        = height;

    CFDictionaryRef cvpx_attr = (__bridge CFDictionaryRef)@{
        (__bridge NSString*)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
        (__bridge NSString*)kCVPixelBufferWidthKey: @(width),
        (__bridge NSString*)kCVPixelBufferHeightKey: @(height),
        (__bridge NSString*)kCVPixelBufferBytesPerRowAlignmentKey: @(16),
        (__bridge NSString*)kCVPixelBufferIOSurfacePropertiesKey: @{},

#if TARGET_OS_IPHONE
        //(__bridge NSString*)kCVPixelBufferOpenGLESCompatibilityKey : @YES,
        //(__bridge NSString*)kCVPixelBufferIOSurfaceOpenGLESFBOCompatibilityKey: @YES,
        //(__bridge NSString*)kCVPixelBufferIOSurfaceOpenGLESTextureCompatibilityKey: @YES,
#endif

    };

#if TARGET_OS_IPHONE
    _context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    assert(_context != nil);

    cvret = CVOpenGLESTextureCacheCreate(kCFAllocatorDefault,
            nil, _context, nil, &_textureCache);

    assert(cvret == kCVReturnSuccess);
    EAGLContext *previous_context = [EAGLContext currentContext];
    [EAGLContext setCurrentContext:_context];

#elif TARGET_OS_MAC
    CGLPixelFormatAttribute pixel_attr[] = {
        kCGLPFAAccelerated,
        kCGLPFAAllowOfflineRenderers,
        0,
    };

    CGLPixelFormatObj pixelFormat;
    GLint numPixelFormats = 0;

    //CGDirectDisplayID display = CGMainDisplayID ();
    //attribs[1] = CGDisplayIDToOpenGLDisplayMask (display);
    CGLChoosePixelFormat (pixel_attr, &pixelFormat, &numPixelFormats);

    CGLCreateContext( pixelFormat, NULL, &_context ) ;
    CGLDestroyPixelFormat( pixelFormat ) ;

    CGLContextObj previous_context = CGLGetCurrentContext();
    CGLSetCurrentContext(_context) ;

    cvret = CVOpenGLTextureCacheCreate(kCFAllocatorDefault,
        nil,
        _context,
        pixelFormat,
        nil,
        &_textureCache);
#endif

    // init framebuffer
    glGenFramebuffers(ARRAY_SIZE(_buffers), _fbos);

    for (int buffer = 0; buffer < ARRAY_SIZE(_buffers); ++buffer)
    {
        cvret = CVPixelBufferCreate(kCFAllocatorDefault, width, height, kCVPixelFormatType_32BGRA, cvpx_attr, &_buffers[buffer]);
        assert(cvret == kCVReturnSuccess);

#if TARGET_OS_IPHONE
        cvret = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                _textureCache, _buffers[buffer],
                nil,                                    // CFDictionaryRef textureAttributes
                GL_TEXTURE_2D,                        // GLenum target
                GL_RGBA,                                // GLint internalFormat
                _fmt_out.i_visible_width,                   // GLsizei width
                _fmt_out.i_visible_height,                  // GLsizei height
                GL_BGRA,                                // GLenum format for native data
                GL_UNSIGNED_BYTE,                       // GLenum type
                0,                                      // size_t planeIndex
                &_textures[buffer]);

        // bind buffer
        assert(CVOpenGLESTextureGetTarget(_textures[buffer]) == GL_TEXTURE_2D);
        GLuint name = CVOpenGLESTextureGetName(_textures[buffer]);
        GLenum target = CVOpenGLESTextureGetTarget(_textures[buffer]);

#elif TARGET_OS_MAC
        cvret = CVOpenGLTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                _textureCache, _buffers[buffer],
                nil, // CFDictionaryRef textureAttributes
                &_textures[buffer]);

        // bind buffer
        assert(CVOpenGLTextureGetTarget(_textures[buffer]) == GL_TEXTURE_RECTANGLE);

        GLenum target = CVOpenGLTextureGetTarget(_textures[buffer]);
        GLuint name = CVOpenGLTextureGetName(_textures[buffer]);
#endif

        glBindTexture(target, name);
        glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, _fbos[buffer]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                target, name, 0);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        assert(status == GL_FRAMEBUFFER_COMPLETE);
        assert(cvret == kCVReturnSuccess);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, _fbos[0]);

    gl->make_current = MakeCurrent;
    gl->release_current = ReleaseCurrent;
    gl->resize = Resize;
    gl->swap = Swap;
    gl->get_proc_address = GetSymbol;
    gl->destroy = Close;
    gl->offscreen_vflip = true;

#if TARGET_OS_IPHONE
    [EAGLContext setCurrentContext:previous_context];
#elif TARGET_OS_MAC
    CGLSetCurrentContext(previous_context);
#endif

    // TODO
    struct vlc_decoder_device *dec_device = NULL;

    struct vlc_video_context *vctx_out = vlc_video_context_CreateCVPX(
        dec_device, CVPX_VIDEO_CONTEXT_DEFAULT, sizeof(VLCCVOpenGLContext*), &vctx_ops);

    assert(vctx_out);
    if (vctx_out == NULL)
        goto error;

    gl->offscreen_vctx_out = vctx_out;
    gl->offscreen_chroma_out = VLC_CODEC_CVPX_BGRA;

#if TARGET_OS_IPHONE
    var_Create(_gl, "ios-eaglcontext", VLC_VAR_ADDRESS);
    var_SetAddress(_gl, "ios-eaglcontext", (__bridge void*)_context);
#elif TARGET_OS_MAC
    var_Create(_gl, "macosx-glcontext", VLC_VAR_ADDRESS);
    var_SetAddress(_gl, "macosx-glcontext", _context);
#endif

    return self;
error:

    return nil;
}

- (BOOL)makeCurrent
{
    /* TODO check if it works with app inactive */

    assert(_previousContext == NULL);

#if TARGET_OS_IPHONE
    _previousContext = [EAGLContext currentContext];
    [EAGLContext setCurrentContext:_context];
#elif TARGET_OS_MAC
    _previousContext = CGLGetCurrentContext();
    CGLSetCurrentContext(_context);
#endif

    return YES;
}

- (void)releaseCurrent
{
#if TARGET_OS_IPHONE
    [EAGLContext setCurrentContext:_previousContext];
#elif TARGET_OS_MAC
    CGLSetCurrentContext(_previousContext);
#endif

    _previousContext = nil;
}

- (picture_t*)swap
{
    /* EAGLContext has no formal swap operation but we swap the backing
     * CVPX buffer ourselves. */

    // Note:
    // The result must be used after completion of the rendering, meaning it must wait
    // for completion of a fence or call glFinish:
    //  https://www.khronos.org/registry/OpenGL/extensions/APPLE/APPLE_sync.txt

#if TARGET_OS_IPHONE
    EAGLContext *previous_context = [EAGLContext currentContext];
    [EAGLContext setCurrentContext:_context];
#elif TARGET_OS_MAC
    CGLContextObj previous_context = CGLGetCurrentContext();
    CGLSetCurrentContext(_context);
#endif

    CVPixelBufferRef cvpx_buffer = _buffers[_currentFlip];

    glFlush();
    glFinish();

    picture_t *output = picture_NewFromFormat(&_fmt_out);
    if (output == NULL)
        return NULL;
    cvpxpic_attach(output, cvpx_buffer, _gl->offscreen_vctx_out, NULL);

    _currentFlip = (_currentFlip + 1) % 2;
    glBindFramebuffer(GL_FRAMEBUFFER, _fbos[_currentFlip]);

#if TARGET_OS_IPHONE
    [EAGLContext setCurrentContext:previous_context];
#elif TARGET_OS_MAC
    CGLSetCurrentContext(previous_context);
#endif

    return output;
}

- (void)resize:(CGSize)size
{

}

- (void)dealloc
{
    // cleanup
}

@end

static int Open(vlc_gl_t *gl, unsigned width, unsigned height)
{
    VLCCVOpenGLContext *sys = [[VLCCVOpenGLContext alloc] initWithGL:gl width:width height:height];
    assert(sys);

    gl->sys = (__bridge_retained void*)sys;

    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname( N_("cvpx_gl") )
    set_description( N_("OpenGL baked by CVPX buffer") )
#if TARGET_OS_IPHONE
    set_capability( "opengl es2 offscreen", 10 )
#elif TARGET_OS_MAC
    set_capability( "opengl offscreen", 10 )
#endif
    add_shortcut( "cvpx_gl" )
    set_callback( Open)
vlc_module_end()
