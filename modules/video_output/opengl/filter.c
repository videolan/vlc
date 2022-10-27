/*****************************************************************************
 * filter.c
 *****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 * Copyright (C) 2020 Videolabs
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

#include "filter_priv.h"

#include <assert.h>

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_opengl.h>

#include "gl_api.h"

struct vlc_gl_filter *
vlc_gl_filter_New(struct vlc_gl_t *gl, const struct vlc_gl_api *api)
{
    struct vlc_gl_filter_priv *priv = vlc_object_create(gl, sizeof(*priv));
    if (!priv)
        return NULL;

    priv->size_out.width = 0;
    priv->size_out.height = 0;

    priv->plane_count = 0;
    priv->tex_count = 0;

    priv->has_picture = false;

    struct vlc_gl_filter *filter = &priv->filter;
    filter->gl = gl;
    filter->api = api;
    filter->config.filter_planes = false;
    filter->config.blend = false;
    filter->config.msaa_level = 0;
    filter->ops = NULL;
    filter->sys = NULL;
    filter->module = NULL;

    vlc_list_init(&priv->blend_subfilters);

    /* Expose a const pointer to the OpenGL format publicly */
    filter->glfmt_in = &priv->glfmt_in;

    return filter;
}

static int
ActivateGLFilter(void *func, bool forced, va_list args)
{
    (void) forced;
    vlc_gl_filter_open_fn *activate = func;
    struct vlc_gl_filter *filter = va_arg(args, struct vlc_gl_filter *);
    const config_chain_t *config = va_arg(args, config_chain_t *);
    const struct vlc_gl_format *glfmt = va_arg(args, struct vlc_gl_format *);
    struct vlc_gl_tex_size *size_out = va_arg(args, struct vlc_gl_tex_size *);

    return activate(filter, config, glfmt, size_out);
}

#undef vlc_gl_filter_LoadModule
int
vlc_gl_filter_LoadModule(vlc_object_t *parent, const char *name,
                         struct vlc_gl_filter *filter,
                         const config_chain_t *config,
                         const struct vlc_gl_format *glfmt,
                         struct vlc_gl_tex_size *size_out)
{
    filter->module = vlc_module_load(parent, "opengl filter", name, true,
                                     ActivateGLFilter, filter, config,
                                     glfmt, size_out);
    if (!filter->module)
        return VLC_EGENERIC;

    assert(filter->ops->draw);
    return VLC_SUCCESS;
}

static void
DeleteFramebuffersOut(struct vlc_gl_filter_priv *priv)
{
    const opengl_vtable_t *vt = &priv->filter.api->vt;

    vt->DeleteFramebuffers(priv->tex_count, priv->framebuffers_out);
    vt->DeleteTextures(priv->tex_count, priv->textures_out);
}

static void
DeleteFramebufferMSAA(struct vlc_gl_filter_priv *priv)
{
    const opengl_vtable_t *vt = &priv->filter.api->vt;

    vt->DeleteFramebuffers(1, &priv->framebuffer_msaa);
    vt->DeleteRenderbuffers(1, &priv->renderbuffer_msaa);
}

void
vlc_gl_filter_Delete(struct vlc_gl_filter *filter)
{
    if (filter->ops && filter->ops->close)
        filter->ops->close(filter);

    if (filter->module)
        module_unneed(filter, filter->module);

    struct vlc_gl_filter_priv *priv = vlc_gl_filter_PRIV(filter);

    struct vlc_gl_filter_priv *subfilter_priv;
    vlc_list_foreach(subfilter_priv, &priv->blend_subfilters, node)
    {
        struct vlc_gl_filter *subfilter = &subfilter_priv->filter;
        vlc_gl_filter_Delete(subfilter);
    }

    if (priv->tex_count)
        DeleteFramebuffersOut(priv);

    if (filter->config.msaa_level)
        DeleteFramebufferMSAA(priv);

    vlc_object_delete(&filter->obj);
}

static int
InitPlane(struct vlc_gl_filter_priv *priv, unsigned plane, GLsizei width,
          GLsizei height)
{
    const opengl_vtable_t *vt = &priv->filter.api->vt;

    GLuint framebuffer = priv->framebuffers_out[plane];
    GLuint texture = priv->textures_out[plane];

    vt->BindTexture(GL_TEXTURE_2D, texture);
    vt->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, NULL);
    vt->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    vt->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    /* iOS needs GL_CLAMP_TO_EDGE or power-of-two textures */
    vt->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    vt->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    /* Create a framebuffer and attach the texture */
    vt->BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    vt->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, texture, 0);

    GLenum status = vt->CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static int
InitFramebuffersOut(struct vlc_gl_filter_priv *priv)
{
    assert(priv->size_out.width > 0 && priv->size_out.height > 0);

    /* Not initialized yet */
    assert(priv->tex_count == 0);

    const opengl_vtable_t *vt = &priv->filter.api->vt;

    struct vlc_gl_filter *filter = &priv->filter;
    if (filter->config.filter_planes)
        priv->tex_count = priv->glfmt_in.tex_count;
    else
        priv->tex_count = 1;

    vt->GenFramebuffers(priv->tex_count, priv->framebuffers_out);
    vt->GenTextures(priv->tex_count, priv->textures_out);

    memcpy(priv->tex_widths, priv->plane_widths,
           priv->tex_count * sizeof(*priv->tex_widths));
    memcpy(priv->tex_heights, priv->plane_heights,
           priv->tex_count * sizeof(*priv->tex_heights));

    for (unsigned i = 0; i < priv->tex_count; ++i)
    {
        /* Init one framebuffer and texture for each plane */
        int ret =
            InitPlane(priv, i, priv->tex_widths[i], priv->tex_heights[i]);
        if (ret != VLC_SUCCESS)
        {
            DeleteFramebuffersOut(priv);
            return ret;
        }
    }

    return VLC_SUCCESS;
}

static int
InitFramebufferMSAA(struct vlc_gl_filter_priv *priv, unsigned msaa_level)
{
    assert(msaa_level);
    assert(priv->size_out.width > 0 && priv->size_out.height > 0);

    const opengl_vtable_t *vt = &priv->filter.api->vt;

    vt->GenRenderbuffers(1, &priv->renderbuffer_msaa);
    vt->BindRenderbuffer(GL_RENDERBUFFER, priv->renderbuffer_msaa);
    vt->RenderbufferStorageMultisample(GL_RENDERBUFFER, msaa_level,
                                       GL_RGBA8,
                                       priv->size_out.width,
                                       priv->size_out.height);

    vt->GenFramebuffers(1, &priv->framebuffer_msaa);
    vt->BindFramebuffer(GL_FRAMEBUFFER, priv->framebuffer_msaa);
    vt->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                GL_RENDERBUFFER, priv->renderbuffer_msaa);

    GLenum status = vt->CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        DeleteFramebufferMSAA(priv);
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

int
vlc_gl_filter_InitFramebuffers(struct vlc_gl_filter *filter, bool has_out)
{
    struct vlc_gl_filter_priv *priv = vlc_gl_filter_PRIV(filter);

    unsigned msaa_level = priv->filter.config.msaa_level;
    if (msaa_level)
    {
        int ret = InitFramebufferMSAA(priv, msaa_level);
        if (ret != VLC_SUCCESS)
            return ret;
    }

    /* Every non-blend filter needs its own framebuffer, except the last one */
    if (has_out)
    {
        int ret = InitFramebuffersOut(priv);
        if (ret != VLC_SUCCESS)
        {
            DeleteFramebufferMSAA(priv);
            return ret;
        }
    }

    return VLC_SUCCESS;
}

void
vlc_gl_filter_InitPlaneSizes(struct vlc_gl_filter *filter)
{
    struct vlc_gl_filter_priv *priv = vlc_gl_filter_PRIV(filter);

    if (filter->config.filter_planes)
    {
        struct vlc_gl_format *glfmt = &priv->glfmt_in;

        priv->plane_count = glfmt->tex_count;
        for (unsigned i = 0; i < glfmt->tex_count; ++i)
        {
            priv->plane_widths[i] = priv->size_out.width
                                  * glfmt->tex_widths[i]
                                  / glfmt->tex_widths[0];
            priv->plane_heights[i] = priv->size_out.height
                                   * glfmt->tex_heights[i]
                                   / glfmt->tex_heights[0];
        }
    }
    else
    {
        priv->plane_count = 1;
        priv->plane_widths[0] = priv->size_out.width;
        priv->plane_heights[0] = priv->size_out.height;
    }
}

void
vlc_gl_filter_ApplyOutputSize(struct vlc_gl_filter *filter)
{
    struct vlc_gl_filter_priv *priv = vlc_gl_filter_PRIV(filter);

    vlc_gl_filter_InitPlaneSizes(filter);

    const opengl_vtable_t *vt = &priv->filter.api->vt;
    GL_ASSERT_NOERROR(vt);

    unsigned msaa_level = filter->config.msaa_level;
    if (msaa_level)
    {
        vt->BindRenderbuffer(GL_RENDERBUFFER, priv->renderbuffer_msaa);
        vt->RenderbufferStorageMultisample(GL_RENDERBUFFER, msaa_level,
                                           GL_RGBA8,
                                           priv->size_out.width,
                                           priv->size_out.height);
    }

    if (priv->tex_count)
    {
        memcpy(priv->tex_widths, priv->plane_widths,
               priv->tex_count * sizeof(*priv->tex_widths));
        memcpy(priv->tex_heights, priv->plane_heights,
               priv->tex_count * sizeof(*priv->tex_heights));

        for (unsigned plane = 0; plane < priv->tex_count; ++plane)
        {
            vt->BindTexture(GL_TEXTURE_2D, priv->textures_out[plane]);
            vt->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, priv->tex_widths[plane],
                           priv->tex_heights[plane], 0, GL_RGBA,
                           GL_UNSIGNED_BYTE, NULL);
        }
    }

    GL_ASSERT_NOERROR(vt);
}
