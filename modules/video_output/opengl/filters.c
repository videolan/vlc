/*****************************************************************************
 * filters.c
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

#include "filters.h"

#include <vlc_common.h>
#include <vlc_list.h>

#include "filter_priv.h"
#include "renderer.h"
#include "sampler_priv.h"

struct vlc_gl_filters {
    struct vlc_gl_t *gl;
    const struct vlc_gl_api *api;

    /**
     * Interop to use for the sampler of the first filter of the chain,
     * the one which uses the picture_t as input.
     */
    struct vlc_gl_interop *interop;

    struct vlc_list list; /**< list of vlc_gl_filter.node */
};

struct vlc_gl_filters *
vlc_gl_filters_New(struct vlc_gl_t *gl, const struct vlc_gl_api *api,
                   struct vlc_gl_interop *interop)
{
    struct vlc_gl_filters *filters = malloc(sizeof(*filters));
    if (!filters)
        return NULL;

    filters->gl = gl;
    filters->api = api;
    filters->interop = interop;
    vlc_list_init(&filters->list);
    return filters;
}

void
vlc_gl_filters_Delete(struct vlc_gl_filters *filters)
{
    struct vlc_gl_filter_priv *priv;
    vlc_list_foreach(priv, &filters->list, node)
    {
        struct vlc_gl_filter *filter = &priv->filter;
        vlc_gl_filter_Delete(filter);
    }

    free(filters);
}

struct vlc_gl_filter *
vlc_gl_filters_Append(struct vlc_gl_filters *filters, const char *name,
                      const config_chain_t *config)
{
    struct vlc_gl_filter *filter = vlc_gl_filter_New(filters->gl, filters->api);
    if (!filter)
        return NULL;

    struct vlc_gl_filter_priv *priv = vlc_gl_filter_PRIV(filter);

    bool first_filter = vlc_list_is_empty(&filters->list);
    if (first_filter)
        priv->sampler = vlc_gl_sampler_NewFromInterop(filters->interop);
    else
    {
        video_format_t fmt;
        video_format_Init(&fmt, VLC_CODEC_RGBA);
        // TODO set format width/height

        priv->sampler =
            vlc_gl_sampler_NewFromTexture2D(filters->gl, filters->api, &fmt);
    }

    if (!priv->sampler)
    {
        vlc_gl_filter_Delete(filter);
        return NULL;
    }

    int ret = vlc_gl_filter_LoadModule(filters->gl, name, filter, config,
                                       priv->sampler);
    if (ret != VLC_SUCCESS)
    {
        /* Creation failed, do not call close() */
        filter->ops = NULL;
        vlc_gl_filter_Delete(filter);
        return NULL;
    }

    vlc_list_append(&priv->node, &filters->list);

    return filter;
}

int
vlc_gl_filters_UpdatePicture(struct vlc_gl_filters *filters,
                             picture_t *picture)
{
    assert(!vlc_list_is_empty(&filters->list));

    struct vlc_gl_filter_priv *first_filter =
        vlc_list_first_entry_or_null(&filters->list, struct vlc_gl_filter_priv,
                                     node);

    assert(first_filter);

    return vlc_gl_sampler_UpdatePicture(first_filter->sampler, picture);
}

int
vlc_gl_filters_Draw(struct vlc_gl_filters *filters)
{
    struct vlc_gl_filter_priv *priv;
    vlc_list_foreach(priv, &filters->list, node)
    {
        struct vlc_gl_filter *filter = &priv->filter;
        int ret = filter->ops->draw(filter);
        if (ret != VLC_SUCCESS)
            return ret;
    }

    return VLC_SUCCESS;
}
