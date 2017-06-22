/*****************************************************************************
 * converter_cvpx.c: OpenGL Apple CVPX opaque converter
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
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
# include "config.h"
#endif

#include "internal.h"
#include "../../codec/vt_utils.h"

#if TARGET_OS_IPHONE
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#include <CoreVideo/CVOpenGLESTextureCache.h>
struct gl_sys
{
    CVEAGLContext locked_ctx;
};
#else
#include <IOSurface/IOSurface.h>
struct gl_sys
{
    CGLContextObj locked_ctx;
};
#endif

struct priv
{
    picture_t *last_pic;
#if TARGET_OS_IPHONE
    CVOpenGLESTextureCacheRef cache;
#endif
};

#if TARGET_OS_IPHONE
/* CVOpenGLESTextureCache version (ios) */
static int
tc_cvpx_update(const opengl_tex_converter_t *tc, GLuint *textures,
               const GLsizei *tex_width, const GLsizei *tex_height,
               picture_t *pic, const size_t *plane_offset)
{
    (void) plane_offset;
    struct priv *priv = tc->priv;

    CVPixelBufferRef pixelBuffer = cvpxpic_get_ref(pic);

    for (unsigned i = 0; i < tc->tex_count; ++i)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glClientActiveTexture(GL_TEXTURE0 + i);

        CVOpenGLESTextureRef texture;
        CVReturn err = CVOpenGLESTextureCacheCreateTextureFromImage(
            kCFAllocatorDefault, priv->cache, pixelBuffer, NULL,
            tc->tex_target, tc->texs[i].internal, tex_width[i], tex_height[i],
            tc->texs[i].format, tc->texs[i].type, i, &texture);
        if (err != noErr)
        {
            msg_Err(tc->gl,
                    "CVOpenGLESTextureCacheCreateTextureFromImage failed: %d",
                    err);
            return VLC_EGENERIC;
        }

        textures[i] = CVOpenGLESTextureGetName(texture);
        glBindTexture(tc->tex_target, textures[i]);
        glTexParameteri(tc->tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(tc->tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(tc->tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(tc->tex_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        CFRelease(texture);
    }

    if (priv->last_pic != pic)
    {
        if (priv->last_pic != NULL)
            picture_Release(priv->last_pic);
        priv->last_pic = picture_Hold(pic);
    }
    return VLC_SUCCESS;
}

#else
/* IOSurface version (macos) */
static int
tc_cvpx_update(const opengl_tex_converter_t *tc, GLuint *textures,
               const GLsizei *tex_width, const GLsizei *tex_height,
               picture_t *pic, const size_t *plane_offset)
{
    (void) plane_offset;
    struct priv *priv = tc->priv;
    struct gl_sys *glsys = tc->gl->sys;

    CVPixelBufferRef pixelBuffer = cvpxpic_get_ref(pic);

    IOSurfaceRef surface = CVPixelBufferGetIOSurface(pixelBuffer);

    for (unsigned i = 0; i < tc->tex_count; ++i)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glClientActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(tc->tex_target, textures[i]);

        CGLError err =
            CGLTexImageIOSurface2D(glsys->locked_ctx, tc->tex_target,
                                   tc->texs[i].internal,
                                   tex_width[i], tex_height[i],
                                   tc->texs[i].format,
                                   tc->texs[i].type,
                                   surface, i);
        if (err != kCGLNoError)
        {
            msg_Err(tc->gl, "CGLTexImageIOSurface2D error: %d: %s\n", i,
                    CGLErrorString(err));
            return VLC_EGENERIC;
        }
    }

    if (priv->last_pic != pic)
    {
        if (priv->last_pic != NULL)
            picture_Release(priv->last_pic);
        priv->last_pic = picture_Hold(pic);
    }

    return VLC_SUCCESS;
}
#endif

static void
tc_cvpx_release(const opengl_tex_converter_t *tc)
{
    struct priv *priv = tc->priv;

    if (priv->last_pic != NULL)
        picture_Release(priv->last_pic);
#if TARGET_OS_IPHONE
    CFRelease(priv->cache);
#endif
    free(tc->priv);
}

int
opengl_tex_converter_cvpx_init(opengl_tex_converter_t *tc)
{
    if (tc->fmt.i_chroma != VLC_CODEC_CVPX_UYVY
     && tc->fmt.i_chroma != VLC_CODEC_CVPX_NV12
     && tc->fmt.i_chroma != VLC_CODEC_CVPX_I420
     && tc->fmt.i_chroma != VLC_CODEC_CVPX_BGRA)
        return VLC_EGENERIC;

    struct priv *priv = calloc(1, sizeof(struct priv));
    if (unlikely(priv == NULL))
        return VLC_ENOMEM;

#if TARGET_OS_IPHONE
    const GLenum tex_target = GL_TEXTURE_2D;

    {
        struct gl_sys *glsys = tc->gl->sys;
        CVReturn err =
            CVOpenGLESTextureCacheCreate(kCFAllocatorDefault, NULL,
                                         glsys->locked_ctx, NULL, &priv->cache);
        if (err != noErr)
        {
            msg_Err(tc->gl, "CVOpenGLESTextureCacheCreate failed: %d", err);
            free(priv);
            return VLC_EGENERIC;
        }
    }
    tc->handle_texs_gen = true;
#else
    const GLenum tex_target = GL_TEXTURE_RECTANGLE;
#endif

    GLuint fragment_shader;
    switch (tc->fmt.i_chroma)
    {
        case VLC_CODEC_CVPX_UYVY:
            fragment_shader =
                opengl_fragment_shader_init(tc, tex_target, VLC_CODEC_UYVY,
                                            tc->fmt.space);
            tc->texs[0].internal = GL_RGB;
            tc->texs[0].format = GL_RGB_422_APPLE;
            tc->texs[0].type = GL_UNSIGNED_SHORT_8_8_APPLE;
            break;
        case VLC_CODEC_CVPX_NV12:
        {
            fragment_shader =
                opengl_fragment_shader_init(tc, tex_target, VLC_CODEC_NV12,
                                            tc->fmt.space);
            break;
        }
        case VLC_CODEC_CVPX_I420:
            fragment_shader =
                opengl_fragment_shader_init(tc, tex_target, VLC_CODEC_I420,
                                            tc->fmt.space);
            break;
        case VLC_CODEC_CVPX_BGRA:
            fragment_shader =
                opengl_fragment_shader_init(tc, tex_target, VLC_CODEC_RGB32,
                                            COLOR_SPACE_UNDEF);
            tc->texs[0].internal = GL_RGBA;
            tc->texs[0].format = GL_BGRA;
#if TARGET_OS_IPHONE
            tc->texs[0].type = GL_UNSIGNED_BYTE;
#else
            tc->texs[0].type = GL_UNSIGNED_INT_8_8_8_8_REV;
#endif
            break;
        default:
            vlc_assert_unreachable();
    }

    if (fragment_shader == 0)
    {
        free(priv);
        return VLC_EGENERIC;
    }

    tc->priv              = priv;
    tc->pf_update         = tc_cvpx_update;
    tc->pf_release        = tc_cvpx_release;
    tc->fshader           = fragment_shader;

    return VLC_SUCCESS;
}
