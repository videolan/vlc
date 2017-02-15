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
#include <VideoToolbox/VideoToolbox.h>

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

struct picture_sys_t
{
    CVPixelBufferRef pixelBuffer;
};

struct priv
{
    picture_t *last_pic;
#if TARGET_OS_IPHONE
    CVOpenGLESTextureCacheRef cache;
#endif
};

static void
pic_destroy_cb(picture_t *pic)
{
    if (pic->p_sys->pixelBuffer != NULL)
        CFRelease(pic->p_sys->pixelBuffer);

    free(pic->p_sys);
    free(pic);
}

static picture_pool_t *
tc_cvpx_get_pool(const opengl_tex_converter_t *tc, const video_format_t *fmt,
                 unsigned requested_count)
{
    (void) tc;
    picture_t *picture[VLCGL_PICTURE_MAX] = {NULL, };
    unsigned count;

    for (count = 0; count < requested_count; count++)
    {
        picture_sys_t *p_picsys = calloc(1, sizeof(*p_picsys));
        if (unlikely(p_picsys == NULL))
            goto error;
        picture_resource_t rsc = {
            .p_sys = p_picsys,
            .pf_destroy = pic_destroy_cb,
        };

        picture[count] = picture_NewFromResource(fmt, &rsc);
        if (!picture[count])
        {
            free(p_picsys);
            goto error;
        }
    }

    /* Wrap the pictures into a pool */
    picture_pool_configuration_t pool_cfg = {
        .picture_count = requested_count,
        .picture       = picture,
    };
    picture_pool_t *pool = picture_pool_NewExtended(&pool_cfg);
    if (!pool)
        goto error;

    return pool;
error:
    for (unsigned i = 0; i < count; i++)
        picture_Release(picture[i]);
    return NULL;
}

#if TARGET_OS_IPHONE
/* CVOpenGLESTextureCache version (ios) */
static int
tc_cvpx_update(const opengl_tex_converter_t *tc, GLuint *textures,
               const GLsizei *tex_width, const GLsizei *tex_height,
               picture_t *pic, const size_t *plane_offset)
{
    (void) plane_offset;
    struct priv *priv = tc->priv;
    picture_sys_t *picsys = pic->p_sys;

    assert(picsys->pixelBuffer != NULL);

    for (unsigned i = 0; i < tc->tex_count; ++i)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glClientActiveTexture(GL_TEXTURE0 + i);

        CVOpenGLESTextureRef texture;
        CVReturn err = CVOpenGLESTextureCacheCreateTextureFromImage(
            kCFAllocatorDefault, priv->cache, picsys->pixelBuffer, NULL,
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
        {
            picture_sys_t *picsys = priv->last_pic->p_sys;
            assert(picsys->pixelBuffer != NULL);
            CFRelease(picsys->pixelBuffer);
            picsys->pixelBuffer = NULL;
            picture_Release(priv->last_pic);
        }
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
    picture_sys_t *picsys = pic->p_sys;

    assert(picsys->pixelBuffer != NULL);

    IOSurfaceRef surface = CVPixelBufferGetIOSurface(picsys->pixelBuffer);

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
        {
            picture_sys_t *picsys = priv->last_pic->p_sys;
            assert(picsys->pixelBuffer != NULL);
            CFRelease(picsys->pixelBuffer);
            picsys->pixelBuffer = NULL;
            picture_Release(priv->last_pic);
        }
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

GLuint
opengl_tex_converter_cvpx_init(const video_format_t *fmt,
                               opengl_tex_converter_t *tc)
{
    if (fmt->i_chroma != VLC_CODEC_CVPX_UYVY
     && fmt->i_chroma != VLC_CODEC_CVPX_NV12
     && fmt->i_chroma != VLC_CODEC_CVPX_I420
     && fmt->i_chroma != VLC_CODEC_CVPX_BGRA)
        return 0;

    struct priv *priv = calloc(1, sizeof(struct priv));
    if (unlikely(priv == NULL))
        return 0;

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
            return 0;
        }
    }
    tc->handle_texs_gen = true;
#else
    const GLenum tex_target = GL_TEXTURE_RECTANGLE;
#endif

    GLuint fragment_shader;
    switch (fmt->i_chroma)
    {
        case VLC_CODEC_CVPX_UYVY:
            fragment_shader =
                opengl_fragment_shader_init(tc, tex_target, VLC_CODEC_UYVY,
                                            fmt->space);
            tc->texs[0].internal = GL_RGB;
            tc->texs[0].format = GL_RGB_422_APPLE;
            tc->texs[0].type = GL_UNSIGNED_SHORT_8_8_APPLE;
            break;
        case VLC_CODEC_CVPX_NV12:
        {
            fragment_shader =
                opengl_fragment_shader_init(tc, tex_target, VLC_CODEC_NV12,
                                            fmt->space);
            break;
        }
        case VLC_CODEC_CVPX_I420:
            fragment_shader =
                opengl_fragment_shader_init(tc, tex_target, VLC_CODEC_I420,
                                            fmt->space);
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
        return 0;
    }

    tc->priv              = priv;
    tc->chroma            = fmt->i_chroma;
    tc->pf_get_pool       = tc_cvpx_get_pool;
    tc->pf_update         = tc_cvpx_update;
    tc->pf_release        = tc_cvpx_release;

    return fragment_shader;
}
