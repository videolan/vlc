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
#include <vlc_opengl_platform.h>
#include <vlc_opengl_interop.h>
#include <vlc_opengl.h>
#include <vlc_opengl_filter.h>
#include "../../codec/vt_utils.h"
#include "gl_common.h"

#if TARGET_OS_IPHONE
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#include <CoreVideo/CVOpenGLESTextureCache.h>
#include <OpenGLES/EAGL.h>
#else
#include <IOSurface/IOSurface.h>
#endif

struct priv
{
#if TARGET_OS_IPHONE
    CVOpenGLESTextureCacheRef cache;
    CVOpenGLESTextureRef last_cvtexs[PICTURE_PLANE_MAX];
#else
    CGLContextObj gl_ctx;
#endif
    picture_t *last_pic;

    struct {
        PFNGLACTIVETEXTUREPROC ActiveTexture;
        PFNGLBINDTEXTUREPROC BindTexture;
        PFNGLTEXPARAMETERIPROC TexParameteri;
        PFNGLTEXPARAMETERFPROC TexParameterf;
    } gl;
};

#if TARGET_OS_IPHONE
/* CVOpenGLESTextureCache version (ios) */
static int
tc_cvpx_update(const struct vlc_gl_interop *interop, uint32_t textures[],
               const int32_t tex_width[], const int32_t tex_height[],
               picture_t *pic, const size_t *plane_offset)
{
    (void) plane_offset;
    struct priv *priv = interop->priv;

    /* Sanity check, don't change format behind interop's back. */
    assert(pic->format.i_chroma == interop->fmt_in.i_chroma);

    CVPixelBufferRef pixelBuffer = cvpxpic_get_ref(pic);

    for (unsigned i = 0; i < interop->tex_count; ++i)
    {
        if (likely(priv->last_cvtexs[i]))
        {
            CFRelease(priv->last_cvtexs[i]);
            priv->last_cvtexs[i] = NULL;
        }
    }

    if (priv->last_pic != NULL)
        picture_Release(priv->last_pic);
    priv->last_pic = picture_Hold(pic);

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
        priv->gl.BindTexture(interop->tex_target, textures[i]);
        priv->gl.TexParameteri(interop->tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        priv->gl.TexParameteri(interop->tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        priv->gl.TexParameterf(interop->tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        priv->gl.TexParameterf(interop->tex_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        priv->gl.BindTexture(interop->tex_target, 0);
        priv->last_cvtexs[i] = cvtex;
    }

    return VLC_SUCCESS;
}

#else
/* IOSurface version (macos) */
static int
tc_cvpx_update(const struct vlc_gl_interop *interop, uint32_t textures[],
               const int32_t tex_width[], const int32_t tex_height[],
               picture_t *pic, const size_t *plane_offset)
{
    (void) plane_offset;
    struct priv *priv = interop->priv;

    CVPixelBufferRef pixelBuffer = cvpxpic_get_ref(pic);

    IOSurfaceRef surface = CVPixelBufferGetIOSurface(pixelBuffer);

    for (unsigned i = 0; i < interop->tex_count; ++i)
    {
        priv->gl.ActiveTexture(GL_TEXTURE0 + i);
        priv->gl.BindTexture(interop->tex_target, textures[i]);

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

    if (priv->last_pic != NULL)
        picture_Release(priv->last_pic);
    priv->last_pic = picture_Hold(pic);

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
#endif
    if (priv->last_pic != NULL)
        picture_Release(priv->last_pic);
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

#define LOAD_FUNCTION(name) \
    priv->gl.name = vlc_gl_GetProcAddress(interop->gl, "gl" # name); \
    assert(priv->gl.name != NULL)

    LOAD_FUNCTION(ActiveTexture);
    LOAD_FUNCTION(BindTexture);
    LOAD_FUNCTION(TexParameteri);
    LOAD_FUNCTION(TexParameterf);

#if TARGET_OS_IPHONE
    const GLenum tex_target = GL_TEXTURE_2D;

    {
        CVEAGLContext eagl_ctx = [EAGLContext currentContext];
        if (!eagl_ctx)
        {
            msg_Warn(&interop->obj, "OpenGL provider is not using CAEAGL, cannot use interop_cvpx");
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
        priv->gl_ctx = CGLGetCurrentContext();
        if (!priv->gl_ctx)
        {
            msg_Warn(&interop->obj, "OpenGL provider is not using CGL, cannot use interop_cvpx");
            free(priv);
            return VLC_EGENERIC;
        }
    }
#endif

    struct vlc_gl_extension_vt extension_vt;
    vlc_gl_LoadExtensionFunctions(interop->gl, &extension_vt);

    /* RG textures are available natively since OpenGL 3.0 and OpenGL ES 3.0 */
    bool has_texture_rg = vlc_gl_GetVersionMajor(&extension_vt) >= 3
        || (interop->gl->api_type == VLC_OPENGL
            && vlc_gl_HasExtension(&extension_vt, "GL_ARB_texture_rg"))
        || (interop->gl->api_type == VLC_OPENGL_ES2
            && vlc_gl_HasExtension(&extension_vt, "GL_EXT_texture_rg"));

    interop->tex_target = tex_target;

    switch (interop->fmt_in.i_chroma)
    {
        case VLC_CODEC_CVPX_UYVY:
            /* Generate a VLC_CODEC_VYUY shader in order to use the "gbr"
             * swizzling. Indeed, the Y, Cb and Cr color channels within the
             * GL_RGB_422_APPLE format are mapped into the existing green, blue
             * and red color channels, respectively. cf. APPLE_rgb_422 khronos
             * extension. */

            interop->fmt_out.i_chroma = VLC_CODEC_VYUY;
            interop->fmt_out.space = interop->fmt_in.space;

            interop->tex_count = 1;
            interop->texs[0] = (struct vlc_gl_tex_cfg) {
                .w = {1, 1},
                .h = {1, 1},
                .internal = GL_RGB,
                .format = GL_RGB_422_APPLE,
                .type = GL_UNSIGNED_SHORT_8_8_APPLE,
            };

            break;
        case VLC_CODEC_CVPX_NV12:
        {
            interop->fmt_out.i_chroma = VLC_CODEC_NV12;
            interop->fmt_out.space = interop->fmt_in.space;

            interop->tex_count = 2;
            interop->texs[0] = (struct vlc_gl_tex_cfg) {
                .w = {1, 1},
                .h = {1, 1},
                .internal = has_texture_rg ? GL_RED : GL_LUMINANCE,
                .format = has_texture_rg ? GL_RED : GL_LUMINANCE,
                .type = GL_UNSIGNED_BYTE,
            };
            interop->texs[1] = (struct vlc_gl_tex_cfg) {
                .w = {1, 2},
                .h = {1, 2},
                .internal = has_texture_rg ? GL_RG : GL_LUMINANCE_ALPHA,
                .format = has_texture_rg ? GL_RG : GL_LUMINANCE_ALPHA,
                .type = GL_UNSIGNED_BYTE,
            };

            break;
        }
        case VLC_CODEC_CVPX_P010:
        {
            if (!has_texture_rg
             || vlc_gl_interop_GetTexFormatSize(interop, tex_target, GL_RG,
                                                GL_RG16, GL_UNSIGNED_SHORT) != 16)
                goto error;

            interop->fmt_out.i_chroma = VLC_CODEC_P010;
            interop->fmt_out.space = interop->fmt_in.space;

            interop->tex_count = 2;
            interop->texs[0] = (struct vlc_gl_tex_cfg) {
                .w = {1, 1},
                .h = {1, 1},
                .internal = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT,
            };
            interop->texs[1] = (struct vlc_gl_tex_cfg) {
                .w = {1, 2},
                .h = {1, 2},
                .internal = GL_RG16,
                .format = GL_RG,
                .type = GL_UNSIGNED_SHORT,
            };

            break;
        }
        case VLC_CODEC_CVPX_I420:
            interop->fmt_out.i_chroma = VLC_CODEC_I420;
            interop->fmt_out.space = interop->fmt_in.space;

            interop->tex_count = 3;
            interop->texs[0] = (struct vlc_gl_tex_cfg) {
                .w = {1, 1},
                .h = {1, 1},
                .internal = has_texture_rg ? GL_RED : GL_LUMINANCE,
                .format = has_texture_rg ? GL_RED : GL_LUMINANCE,
                .type = GL_UNSIGNED_BYTE,
            };
            interop->texs[1] = interop->texs[2] = (struct vlc_gl_tex_cfg) {
                .w = {1, 2},
                .h = {1, 2},
                .internal = has_texture_rg ? GL_RED : GL_LUMINANCE,
                .format = has_texture_rg ? GL_RED : GL_LUMINANCE,
                .type = GL_UNSIGNED_BYTE,
            };

            break;
        case VLC_CODEC_CVPX_BGRA:
            interop->fmt_out.i_chroma = VLC_CODEC_RGB32;
            interop->fmt_out.space = COLOR_SPACE_UNDEF;

            interop->tex_count = 1;
            interop->texs[0] = (struct vlc_gl_tex_cfg) {
                .w = {1, 1},
                .h = {1, 1},
                .internal = GL_RGBA,
                .format = GL_BGRA,
#if TARGET_OS_IPHONE
                .type = GL_UNSIGNED_BYTE,
#else
                .type = GL_UNSIGNED_INT_8_8_8_8_REV,
#endif
            };

            break;
        default:
            vlc_assert_unreachable();
    }

#if TARGET_OS_IPHONE
    interop->handle_texs_gen = true;

    for (unsigned i = 0; i < interop->tex_count; ++i)
        priv->last_cvtexs[i] = NULL;
#endif

    priv->last_pic = NULL;
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
    set_capability("glinterop", 100)
    set_callback(Open)
    set_subcategory(SUBCAT_VIDEO_VOUT)
vlc_module_end ()
