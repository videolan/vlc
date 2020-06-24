/*****************************************************************************
 * interop.h
 *****************************************************************************
 * Copyright (C) 2019 Videolabs
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
#include <vlc_modules.h>

#include "interop.h"
#include "internal.h"
#include "vout_helper.h"

struct vlc_gl_interop *
vlc_gl_interop_New(struct vlc_gl_t *gl, const struct vlc_gl_api *api,
                   vlc_video_context *context, const video_format_t *fmt)
{
    struct vlc_gl_interop *interop = vlc_object_create(gl, sizeof(*interop));
    if (!interop)
        return NULL;

    interop->init = opengl_interop_init_impl;
    interop->ops = NULL;
    interop->fmt_out = interop->fmt_in = *fmt;
    /* this is the only allocated field, and we don't need it */
    interop->fmt_out.p_palette = interop->fmt_in.p_palette = NULL;

    interop->gl = gl;
    interop->api = api;
    interop->vt = &api->vt;

    const vlc_chroma_description_t *desc =
        vlc_fourcc_GetChromaDescription(fmt->i_chroma);

    if (desc == NULL)
    {
        vlc_object_delete(interop);
        return NULL;
    }
    if (desc->plane_count == 0)
    {
        /* Opaque chroma: load a module to handle it */
        interop->vctx = context;
        interop->module = module_need_var(interop, "glinterop", "glinterop");
    }

    int ret;
    if (interop->module != NULL)
        ret = VLC_SUCCESS;
    else
    {
        /* Software chroma or gl hw converter failed: use a generic
         * converter */
        ret = opengl_interop_generic_init(interop, true);
    }

    if (ret != VLC_SUCCESS)
    {
        vlc_object_delete(interop);
        return NULL;
    }

    return interop;
}

struct vlc_gl_interop *
vlc_gl_interop_NewForSubpictures(struct vlc_gl_t *gl,
                                 const struct vlc_gl_api *api)
{
    struct vlc_gl_interop *interop = vlc_object_create(gl, sizeof(*interop));
    if (!interop)
        return NULL;

    interop->init = opengl_interop_init_impl;
    interop->ops = NULL;
    interop->gl = gl;
    interop->api = api;
    interop->vt = &api->vt;

    video_format_Init(&interop->fmt_in, VLC_CODEC_RGB32);
    interop->fmt_out = interop->fmt_in;

    int ret = opengl_interop_generic_init(interop, false);
    if (ret != VLC_SUCCESS)
    {
        vlc_object_delete(interop);
        return NULL;
    }

    return interop;
}

void
vlc_gl_interop_Delete(struct vlc_gl_interop *interop)
{
    if (interop->ops && interop->ops->close)
        interop->ops->close(interop);
    if (interop->module)
        module_unneed(interop, interop->module);
    vlc_object_delete(interop);
}

int
vlc_gl_interop_GenerateTextures(const struct vlc_gl_interop *interop,
                                const GLsizei *tex_width,
                                const GLsizei *tex_height, GLuint *textures)
{
    interop->vt->GenTextures(interop->tex_count, textures);

    for (unsigned i = 0; i < interop->tex_count; i++)
    {
        interop->vt->BindTexture(interop->tex_target, textures[i]);

#if !defined(USE_OPENGL_ES2)
        /* Set the texture parameters */
        interop->vt->TexParameterf(interop->tex_target, GL_TEXTURE_PRIORITY, 1.0);
        interop->vt->TexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#endif

        interop->vt->TexParameteri(interop->tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        interop->vt->TexParameteri(interop->tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        interop->vt->TexParameteri(interop->tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        interop->vt->TexParameteri(interop->tex_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    if (interop->ops->allocate_textures != NULL)
    {
        int ret = interop->ops->allocate_textures(interop, textures, tex_width, tex_height);
        if (ret != VLC_SUCCESS)
        {
            interop->vt->DeleteTextures(interop->tex_count, textures);
            memset(textures, 0, interop->tex_count * sizeof(GLuint));
            return ret;
        }
    }
    return VLC_SUCCESS;
}

void
vlc_gl_interop_DeleteTextures(const struct vlc_gl_interop *interop,
                              GLuint *textures)
{
    interop->vt->DeleteTextures(interop->tex_count, textures);
    memset(textures, 0, interop->tex_count * sizeof(GLuint));
}

static int GetTexFormatSize(const opengl_vtable_t *vt, int target,
                            int tex_format, int tex_internal, int tex_type)
{
    if (!vt->GetTexLevelParameteriv)
        return -1;

    GLint tex_param_size;
    int mul = 1;
    switch (tex_format)
    {
        case GL_BGRA:
            mul = 4;
            /* fall through */
        case GL_RED:
        case GL_RG:
            tex_param_size = GL_TEXTURE_RED_SIZE;
            break;
        case GL_LUMINANCE:
            tex_param_size = GL_TEXTURE_LUMINANCE_SIZE;
            break;
        default:
            return -1;
    }
    GLuint texture;

    vt->GenTextures(1, &texture);
    vt->BindTexture(target, texture);
    vt->TexImage2D(target, 0, tex_internal, 64, 64, 0, tex_format, tex_type, NULL);
    GLint size = 0;
    vt->GetTexLevelParameteriv(target, 0, tex_param_size, &size);

    vt->DeleteTextures(1, &texture);
    return size > 0 ? size * mul : size;
}

static int
interop_yuv_base_init(struct vlc_gl_interop *interop, GLenum tex_target,
                      vlc_fourcc_t chroma,
                      const vlc_chroma_description_t *desc)
{
    (void) chroma;

    GLint oneplane_texfmt, oneplane16_texfmt,
          twoplanes_texfmt, twoplanes16_texfmt;

    if (vlc_gl_StrHasToken(interop->api->extensions, "GL_ARB_texture_rg"))
    {
        oneplane_texfmt = GL_RED;
        oneplane16_texfmt = GL_R16;
        twoplanes_texfmt = GL_RG;
        twoplanes16_texfmt = GL_RG16;
    }
    else
    {
        oneplane_texfmt = GL_LUMINANCE;
        oneplane16_texfmt = GL_LUMINANCE16;
        twoplanes_texfmt = GL_LUMINANCE_ALPHA;
        twoplanes16_texfmt = 0;
    }

    if (desc->pixel_size == 2)
    {
        if (GetTexFormatSize(interop->vt, tex_target, oneplane_texfmt,
                             oneplane16_texfmt, GL_UNSIGNED_SHORT) != 16)
            return VLC_EGENERIC;
    }

    if (desc->plane_count == 3)
    {
        GLint internal = 0;
        GLenum type = 0;

        if (desc->pixel_size == 1)
        {
            internal = oneplane_texfmt;
            type = GL_UNSIGNED_BYTE;
        }
        else if (desc->pixel_size == 2)
        {
            internal = oneplane16_texfmt;
            type = GL_UNSIGNED_SHORT;
        }
        else
            return VLC_EGENERIC;

        assert(internal != 0 && type != 0);

        interop->tex_count = 3;
        for (unsigned i = 0; i < interop->tex_count; ++i )
        {
            interop->texs[i] = (struct vlc_gl_tex_cfg) {
                { desc->p[i].w.num, desc->p[i].w.den },
                { desc->p[i].h.num, desc->p[i].h.den },
                internal, oneplane_texfmt, type
            };
        }
    }
    else if (desc->plane_count == 2)
    {
        interop->tex_count = 2;

        if (desc->pixel_size == 1)
        {
            interop->texs[0] = (struct vlc_gl_tex_cfg) {
                { 1, 1 }, { 1, 1 }, oneplane_texfmt, oneplane_texfmt,
                GL_UNSIGNED_BYTE
            };
            interop->texs[1] = (struct vlc_gl_tex_cfg) {
                { 1, 2 }, { 1, 2 }, twoplanes_texfmt, twoplanes_texfmt,
                GL_UNSIGNED_BYTE
            };
        }
        else if (desc->pixel_size == 2)
        {
            if (twoplanes16_texfmt == 0
             || GetTexFormatSize(interop->vt, tex_target, twoplanes_texfmt,
                                 twoplanes16_texfmt, GL_UNSIGNED_SHORT) != 16)
                return VLC_EGENERIC;
            interop->texs[0] = (struct vlc_gl_tex_cfg) {
                { 1, 1 }, { 1, 1 }, oneplane16_texfmt, oneplane_texfmt,
                GL_UNSIGNED_SHORT
            };
            interop->texs[1] = (struct vlc_gl_tex_cfg) {
                { 1, 2 }, { 1, 2 }, twoplanes16_texfmt, twoplanes_texfmt,
                GL_UNSIGNED_SHORT
            };
        }
        else
            return VLC_EGENERIC;
    }
    else if (desc->plane_count == 1)
    {
        /* Y1 U Y2 V fits in R G B A */
        interop->tex_count = 1;
        interop->texs[0] = (struct vlc_gl_tex_cfg) {
            { 1, 2 }, { 1, 1 }, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE
        };
    }
    else
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static int
interop_rgb_base_init(struct vlc_gl_interop *interop, GLenum tex_target,
                      vlc_fourcc_t chroma)
{
    (void) tex_target;

    switch (chroma)
    {
        case VLC_CODEC_RGB32:
        case VLC_CODEC_RGBA:
            interop->texs[0] = (struct vlc_gl_tex_cfg) {
                { 1, 1 }, { 1, 1 }, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE
            };
            break;
        case VLC_CODEC_BGRA: {
            if (GetTexFormatSize(interop->vt, tex_target, GL_BGRA, GL_RGBA,
                                 GL_UNSIGNED_BYTE) != 32)
                return VLC_EGENERIC;
            interop->texs[0] = (struct vlc_gl_tex_cfg) {
                { 1, 1 }, { 1, 1 }, GL_RGBA, GL_BGRA, GL_UNSIGNED_BYTE
            };
            break;
        }
        default:
            return VLC_EGENERIC;
    }
    interop->tex_count = 1;
    return VLC_SUCCESS;
}

static void
interop_xyz12_init(struct vlc_gl_interop *interop)
{
    interop->tex_count = 1;
    interop->tex_target = GL_TEXTURE_2D;
    interop->texs[0] = (struct vlc_gl_tex_cfg) {
        { 1, 1 }, { 1, 1 }, GL_RGB, GL_RGB, GL_UNSIGNED_SHORT
    };
}

int
opengl_interop_init_impl(struct vlc_gl_interop *interop, GLenum tex_target,
                         vlc_fourcc_t chroma, video_color_space_t yuv_space)
{
    bool is_yuv = vlc_fourcc_IsYUV(chroma);
    const vlc_chroma_description_t *desc =
        vlc_fourcc_GetChromaDescription(chroma);
    if (!desc)
        return VLC_EGENERIC;

    assert(!interop->fmt_out.p_palette);
    interop->fmt_out.i_chroma = chroma;
    interop->fmt_out.space = yuv_space;
    interop->tex_target = tex_target;

    if (chroma == VLC_CODEC_XYZ12)
    {
        interop_xyz12_init(interop);
        return VLC_SUCCESS;
    }

    if (is_yuv)
        return interop_yuv_base_init(interop, tex_target, chroma, desc);

    return interop_rgb_base_init(interop, tex_target, chroma);
}
