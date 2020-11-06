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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include "interop.h"
#include "../../codec/vt_utils.h"

#if TARGET_OS_IPHONE
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#include <CoreVideo/CVOpenGLESTextureCache.h>
#else
#include <IOSurface/IOSurface.h>
#endif

struct priv
{
#if TARGET_OS_IPHONE
    CVOpenGLESTextureCacheRef cache;
    CVOpenGLESTextureRef last_cvtexs[PICTURE_PLANE_MAX];
#else
    picture_t *last_pic;
    CGLContextObj gl_ctx;
#endif
};

#if TARGET_OS_IPHONE
/* CVOpenGLESTextureCache version (ios) */
static int
tc_cvpx_update(const struct vlc_gl_interop *interop, GLuint *textures,
               const GLsizei *tex_width, const GLsizei *tex_height,
               picture_t *pic, const size_t *plane_offset)
{
    (void) plane_offset;
    struct priv *priv = interop->priv;

    CVPixelBufferRef pixelBuffer = cvpxpic_get_ref(pic);

    for (unsigned i = 0; i < interop->tex_count; ++i)
    {
        if (likely(priv->last_cvtexs[i]))
        {
            CFRelease(priv->last_cvtexs[i]);
            priv->last_cvtexs[i] = NULL;
        }
    }

    CVOpenGLESTextureCacheFlush(priv->cache, 0);

    for (unsigned i = 0; i < interop->tex_count; ++i)
    {
        CVOpenGLESTextureRef cvtex;
        CVReturn err = CVOpenGLESTextureCacheCreateTextureFromImage(
            kCFAllocatorDefault, priv->cache, pixelBuffer, NULL,
            interop->tex_target, interop->texs[i].internal, tex_width[i], tex_height[i],
            interop->texs[i].format, interop->texs[i].type, i, &cvtex);
        if (err != noErr)
        {
            msg_Err(interop->gl,
                    "CVOpenGLESTextureCacheCreateTextureFromImage failed: %d",
                    err);
            return VLC_EGENERIC;
        }

        textures[i] = CVOpenGLESTextureGetName(cvtex);
        interop->vt->BindTexture(interop->tex_target, textures[i]);
        interop->vt->TexParameteri(interop->tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        interop->vt->TexParameteri(interop->tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        interop->vt->TexParameterf(interop->tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        interop->vt->TexParameterf(interop->tex_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        interop->vt->BindTexture(interop->tex_target, 0);
        priv->last_cvtexs[i] = cvtex;
    }

    return VLC_SUCCESS;
}

#else
/* IOSurface version (macos) */
static int
tc_cvpx_update(const struct vlc_gl_interop *interop, GLuint *textures,
               const GLsizei *tex_width, const GLsizei *tex_height,
               picture_t *pic, const size_t *plane_offset)
{
    (void) plane_offset;
    struct priv *priv = interop->priv;

    CVPixelBufferRef pixelBuffer = cvpxpic_get_ref(pic);

    IOSurfaceRef surface = CVPixelBufferGetIOSurface(pixelBuffer);

    for (unsigned i = 0; i < interop->tex_count; ++i)
    {
        interop->vt->ActiveTexture(GL_TEXTURE0 + i);
        interop->vt->BindTexture(interop->tex_target, textures[i]);

        CGLError err =
            CGLTexImageIOSurface2D(priv->gl_ctx, interop->tex_target,
                                   interop->texs[i].internal,
                                   tex_width[i], tex_height[i],
                                   interop->texs[i].format,
                                   interop->texs[i].type,
                                   surface, i);
        if (err != kCGLNoError)
        {
            msg_Err(interop->gl, "CGLTexImageIOSurface2D error: %u: %s", i,
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
Close(struct vlc_gl_interop *interop)
{
    struct priv *priv = interop->priv;

#if TARGET_OS_IPHONE
    for (unsigned i = 0; i < interop->tex_count; ++i)
    {
        if (likely(priv->last_cvtexs[i]))
            CFRelease(priv->last_cvtexs[i]);
    }
    CFRelease(priv->cache);
#else
    if (priv->last_pic != NULL)
        picture_Release(priv->last_pic);
#endif
    free(priv);
}

static int
Open(vlc_object_t *obj)
{
    struct vlc_gl_interop *interop = (void *) obj;

    if (interop->fmt_in.i_chroma != VLC_CODEC_CVPX_UYVY
     && interop->fmt_in.i_chroma != VLC_CODEC_CVPX_NV12
     && interop->fmt_in.i_chroma != VLC_CODEC_CVPX_I420
     && interop->fmt_in.i_chroma != VLC_CODEC_CVPX_BGRA
     && interop->fmt_in.i_chroma != VLC_CODEC_CVPX_P010)
        return VLC_EGENERIC;

    /* The pictures are uploaded upside-down */
    video_format_TransformBy(&interop->fmt_out, TRANSFORM_VFLIP);

    struct priv *priv = calloc(1, sizeof(struct priv));
    if (unlikely(priv == NULL))
        return VLC_ENOMEM;

#if TARGET_OS_IPHONE
    const GLenum tex_target = GL_TEXTURE_2D;

    {
        CVEAGLContext eagl_ctx = var_InheritAddress(interop->gl, "ios-eaglcontext");
        if (!eagl_ctx)
        {
            msg_Err(interop->gl, "can't find ios-eaglcontext");
            free(priv);
            return VLC_EGENERIC;
        }
        CVReturn err =
            CVOpenGLESTextureCacheCreate(kCFAllocatorDefault, NULL,
                                         eagl_ctx, NULL, &priv->cache);
        if (err != noErr)
        {
            msg_Err(interop->gl, "CVOpenGLESTextureCacheCreate failed: %d", err);
            free(priv);
            return VLC_EGENERIC;
        }
    }
#else
    const GLenum tex_target = GL_TEXTURE_RECTANGLE;
    {
        priv->gl_ctx = var_InheritAddress(interop->gl, "macosx-glcontext");
        if (!priv->gl_ctx)
        {
            msg_Err(interop->gl, "can't find macosx-glcontext");
            free(priv);
            return VLC_EGENERIC;
        }
    }
#endif

    int ret;
    switch (interop->fmt_in.i_chroma)
    {
        case VLC_CODEC_CVPX_UYVY:
            /* Generate a VLC_CODEC_VYUY shader in order to use the "gbr"
             * swizzling. Indeed, the Y, Cb and Cr color channels within the
             * GL_RGB_422_APPLE format are mapped into the existing green, blue
             * and red color channels, respectively. cf. APPLE_rgb_422 khronos
             * extenstion. */

            ret = opengl_interop_init(interop, tex_target, VLC_CODEC_VYUY,
                                      interop->fmt_in.space);
            if (ret != VLC_SUCCESS)
                goto error;

            interop->texs[0].internal = GL_RGB;
            interop->texs[0].format = GL_RGB_422_APPLE;
            interop->texs[0].type = GL_UNSIGNED_SHORT_8_8_APPLE;
            interop->texs[0].w = interop->texs[0].h = (vlc_rational_t) { 1, 1 };
            break;
        case VLC_CODEC_CVPX_NV12:
        {
            ret = opengl_interop_init(interop, tex_target, VLC_CODEC_NV12,
                                      interop->fmt_in.space);
            if (ret != VLC_SUCCESS)
                goto error;
            break;
        }
        case VLC_CODEC_CVPX_P010:
        {
            ret = opengl_interop_init(interop, tex_target, VLC_CODEC_P010,
                                      interop->fmt_in.space);
            if (ret != VLC_SUCCESS)
                goto error;

            break;
        }
        case VLC_CODEC_CVPX_I420:
            ret = opengl_interop_init(interop, tex_target, VLC_CODEC_I420,
                                      interop->fmt_in.space);
            if (ret != VLC_SUCCESS)
                goto error;

            break;
        case VLC_CODEC_CVPX_BGRA:
            ret = opengl_interop_init(interop, tex_target, VLC_CODEC_RGB32,
                                      COLOR_SPACE_UNDEF);
            if (ret != VLC_SUCCESS)
                goto error;

            interop->texs[0].internal = GL_RGBA;
            interop->texs[0].format = GL_BGRA;
#if TARGET_OS_IPHONE
            interop->texs[0].type = GL_UNSIGNED_BYTE;
#else
            interop->texs[0].type = GL_UNSIGNED_INT_8_8_8_8_REV;
#endif
            break;
        default:
            vlc_assert_unreachable();
    }

#if TARGET_OS_IPHONE
    interop->handle_texs_gen = true;
#endif
    interop->priv = priv;
    static const struct vlc_gl_interop_ops ops = {
        .update_textures = tc_cvpx_update,
        .close = Close,
    };
    interop->ops = &ops;

    return VLC_SUCCESS;

error:
    free(priv);
    return VLC_EGENERIC;
}

vlc_module_begin ()
    set_description("Apple OpenGL CVPX converter")
    set_capability("glinterop", 1)
    set_callback(Open)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
vlc_module_end ()
