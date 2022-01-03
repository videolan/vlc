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

#include "gl_util.h"
#include "interop.h"

struct vlc_gl_interop_private
{
    struct vlc_gl_interop interop;

#define OPENGL_VTABLE_F(X) \
        X(PFNGLDELETETEXTURESPROC, DeleteTextures) \
        X(PFNGLGENTEXTURESPROC,    GenTextures) \
        X(PFNGLBINDTEXTUREPROC,    BindTexture) \
        X(PFNGLTEXIMAGE2DPROC,     TexImage2D) \
        X(PFNGLTEXENVFPROC,        TexEnvf) \
        X(PFNGLTEXPARAMETERFPROC,  TexParameterf) \
        X(PFNGLTEXPARAMETERIPROC,  TexParameteri) \
        X(PFNGLGETERRORPROC,       GetError) \
        X(PFNGLGETTEXLEVELPARAMETERIVPROC, GetTexLevelParameteriv) \

    struct {
#define DECLARE_SYMBOL(type, name) type name;
        OPENGL_VTABLE_F(DECLARE_SYMBOL)
    } gl;
};

int
vlc_gl_interop_GenerateTextures(const struct vlc_gl_interop *interop,
                                const GLsizei *tex_width,
                                const GLsizei *tex_height, GLuint *textures)
{
    struct vlc_gl_interop_private *priv =
        container_of(interop, struct vlc_gl_interop_private, interop);

    priv->gl.GenTextures(interop->tex_count, textures);

    for (unsigned i = 0; i < interop->tex_count; i++)
    {
        priv->gl.BindTexture(interop->tex_target, textures[i]);

#if !defined(USE_OPENGL_ES2)
        /* Set the texture parameters */
        priv->gl.TexParameterf(interop->tex_target, GL_TEXTURE_PRIORITY, 1.0);
        priv->gl.TexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#endif

        priv->gl.TexParameteri(interop->tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        priv->gl.TexParameteri(interop->tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        priv->gl.TexParameteri(interop->tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        priv->gl.TexParameteri(interop->tex_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    if (interop->ops->allocate_textures != NULL)
    {
        int ret = interop->ops->allocate_textures(interop, textures, tex_width, tex_height);
        if (ret != VLC_SUCCESS)
        {
            priv->gl.DeleteTextures(interop->tex_count, textures);
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
    struct vlc_gl_interop_private *priv =
        container_of(interop, struct vlc_gl_interop_private, interop);
    priv->gl.DeleteTextures(interop->tex_count, textures);
    memset(textures, 0, interop->tex_count * sizeof(GLuint));
}

static int GetTexFormatSize(struct vlc_gl_interop *interop, GLenum target,
                            GLenum tex_format, GLint tex_internal, GLenum tex_type)
{
    struct vlc_gl_interop_private *priv =
        container_of(interop, struct vlc_gl_interop_private, interop);

    GL_ASSERT_NOERROR(&priv->gl);

    if (!priv->gl.GetTexLevelParameteriv)
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

    priv->gl.GenTextures(1, &texture);
    priv->gl.BindTexture(target, texture);
    priv->gl.TexImage2D(target, 0, tex_internal, 64, 64, 0, tex_format, tex_type, NULL);
    GLint size = 0;
    priv->gl.GetTexLevelParameteriv(target, 0, tex_param_size, &size);

    priv->gl.DeleteTextures(1, &texture);

    bool has_error = false;
    while (priv->gl.GetError() != GL_NO_ERROR)
        has_error = true;

    if (has_error)
        return -1;

    return size > 0 ? size * mul : size;
}

struct vlc_gl_interop *
vlc_gl_interop_New(struct vlc_gl_t *gl, vlc_video_context *context,
                   const video_format_t *fmt)
{
    struct vlc_gl_interop_private *priv = vlc_object_create(gl, sizeof *priv);
    if (priv == NULL)
        return NULL;

    struct vlc_gl_interop *interop = &priv->interop;

    interop->get_tex_format_size = GetTexFormatSize;
    interop->ops = NULL;
    interop->fmt_out = interop->fmt_in = *fmt;
    interop->gl = gl;
    /* this is the only allocated field, and we don't need it */
    interop->fmt_out.p_palette = interop->fmt_in.p_palette = NULL;

    const vlc_chroma_description_t *desc =
        vlc_fourcc_GetChromaDescription(fmt->i_chroma);

    if (desc == NULL)
    {
        vlc_object_delete(interop);
        return NULL;
    }

#define LOAD_SYMBOL(type, name) \
    priv->gl.name = vlc_gl_GetProcAddress(interop->gl, "gl" # name);
    OPENGL_VTABLE_F(LOAD_SYMBOL);
#undef LOAD_SYMBOL

    if (desc->plane_count == 0)
    {
        /* Opaque chroma: load a module to handle it */
        assert(context);
        interop->vctx = vlc_video_context_Hold(context);
        interop->module = module_need_var(interop, "glinterop", "glinterop");
    }
    else
    {
        interop->vctx = NULL;
        interop->module = module_need(interop, "opengl sw interop", NULL, false);
    }

    if (interop->module == NULL)
        goto error;

    return interop;

error:
    vlc_object_delete(interop);
    return NULL;
}

struct vlc_gl_interop *
vlc_gl_interop_NewForSubpictures(struct vlc_gl_t *gl)
{
    struct vlc_gl_interop_private *priv = vlc_object_create(gl, sizeof *priv);
    if (priv == NULL)
        return NULL;

    struct vlc_gl_interop *interop = &priv->interop;
    interop->ops = NULL;
    interop->gl = gl;

    video_format_Init(&interop->fmt_in, VLC_CODEC_RGB32);
    interop->fmt_out = interop->fmt_in;

#define LOAD_SYMBOL(type, name) \
    priv->gl.name = vlc_gl_GetProcAddress(interop->gl, "gl" # name);
    OPENGL_VTABLE_F(LOAD_SYMBOL);
#undef LOAD_SYMBOL

    interop->module = module_need(interop, "opengl sw interop", "sw", true);

    if (interop->module == NULL)
        goto error;

    return interop;
error:
    vlc_object_delete(interop);
    return NULL;
}

void
vlc_gl_interop_Delete(struct vlc_gl_interop *interop)
{
    if (interop->ops && interop->ops->close)
        interop->ops->close(interop);
    if (interop->vctx)
        vlc_video_context_Release(interop->vctx);
    if (interop->module)
        module_unneed(interop, interop->module);
    vlc_object_delete(interop);
}
