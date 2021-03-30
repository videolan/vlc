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

#include "gl_api.h"
#include "sampler_priv.h"

#undef vlc_gl_filter_New
struct vlc_gl_filter *
vlc_gl_filter_New(vlc_object_t *parent, const struct vlc_gl_api *api)
{
    struct vlc_gl_filter_priv *priv = vlc_object_create(parent, sizeof(*priv));
    if (!priv)
        return NULL;

    priv->sampler = NULL;
    priv->size_out.width = 0;
    priv->size_out.height = 0;

    priv->tex_count = 0;

    struct vlc_gl_filter *filter = &priv->filter;
    filter->api = api;
    filter->config.blend = false;
    filter->config.msaa_level = 0;
    filter->ops = NULL;
    filter->sys = NULL;
    filter->module = NULL;

    vlc_list_init(&priv->blend_subfilters);

    return filter;
}

static int
ActivateGLFilter(void *func, bool forced, va_list args)
{
    (void) forced;
    vlc_gl_filter_open_fn *activate = func;
    struct vlc_gl_filter *filter = va_arg(args, struct vlc_gl_filter *);
    const config_chain_t *config = va_arg(args, config_chain_t *);
    struct vlc_gl_tex_size *size_out = va_arg(args, struct vlc_gl_tex_size *);

    return activate(filter, config, size_out);
}

#undef vlc_gl_filter_LoadModule
int
vlc_gl_filter_LoadModule(vlc_object_t *parent, const char *name,
                         struct vlc_gl_filter *filter,
                         const config_chain_t *config,
                         struct vlc_gl_tex_size *size_out)
{
    filter->module = vlc_module_load(parent, "opengl filter", name, true,
                                     ActivateGLFilter, filter, config,
                                     size_out);
    if (!filter->module)
        return VLC_EGENERIC;

    assert(filter->ops->draw);
    return VLC_SUCCESS;
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

    if (priv->sampler)
        vlc_gl_sampler_Delete(priv->sampler);

    const opengl_vtable_t *vt = &filter->api->vt;

    if (priv->tex_count)
    {
        vt->DeleteFramebuffers(priv->tex_count, priv->framebuffers_out);
        vt->DeleteTextures(priv->tex_count, priv->textures_out);
    }

    if (filter->config.msaa_level)
    {
        vt->DeleteFramebuffers(1, &priv->framebuffer_msaa);
        vt->DeleteRenderbuffers(1, &priv->renderbuffer_msaa);
    }

    vlc_object_delete(&filter->obj);
}
