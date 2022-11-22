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
#include <vlc_ancillary.h>
#include <vlc_list.h>

#include "filter_priv.h"
#include "importer_priv.h"

/* The filter chain contains the sequential list of filters.
 *
 * There are two types of filters:
 *  - blend filters just draw over the provided framebuffer (containing the
 *    result of the previous filter), without reading the input picture.
 *  - non-blend filters read their input picture and draw whatever they want to
 *    their own output framebuffer.
 *
 * For convenience, the filter chain does not store the filters as a single
 * sequential list, but as a list of non-blend filters, each containing the
 * list of their associated blend filters.
 *
 * For example, given the following sequence of filters:
 *    - draw
 *    - logo (a blend filter drawing a logo)
 *    - mask (a non-blend-filter applying a mask)
 *    - logo2 (another logo)
 *    - logo3 (yet another logo)
 *    - renderer
 *
 * the filter chain stores the filters as follow:
 *
 *     +- draw               (non-blend)
 *     |  +- logo            (blend)
 *     +- mask               (non-blend)
 *     |  +- logo2           (blend)
 *     |  +- logo3           (blend)
 *     +- renderer           (non-blend)
 *
 * An output framebuffer is created for each non-blend filters. It is used as
 * draw framebuffer for that filter and all its associated blend filters.
 *
 * If the first filter is a blend filter, then a "draw" filter is automatically
 * inserted.
 *
 *
 * ## Multisample anti-aliasing (MSAA)
 *
 * Each filter may also request multisample anti-aliasing, by providing a MSAA
 * level during its Open(), for example:
 *
 *     filter->config.msaa_level = 4;
 *
 * For example:
 *
 *     +- draw               msaa_level=0
 *     |  +- logo_msaa4      msaa_level=4
 *     |  +- logo_msaa2      msaa_level=2
 *     +- renderer           msaa_level=0
 *
 * Among a "group" of one non-blend filter and its associated blend filters,
 * the highest MSAA level (or 0 if multisampling is not supported) is assigned
 * both to the non-blend filter, to configure its MSAA framebuffer, and to the
 * blend filters, just for information and consistency:
 *
 *     +- draw               msaa_level=4
 *     |  +- logo_msaa4      msaa_level=4
 *     |  +- logo_msaa2      msaa_level=4
 *     +- renderer           msaa_level=0
 *
 * Some platforms (Android) do not support resolving multisample to the default
 * framebuffer. Therefore, the msaa_level must always be 0 on the last filter.
 * If this is not the case, a "draw" filter is automatically appended.
 *
 * For example:
 *
 *     +- draw               msaa_level=0
 *     |  +- logo_msaa4      msaa_level=4
 *     +- renderer           msaa_level=0
 *        +- logo_msaa2      msaa_level=2
 *
 * will become:
 *
 *     +- draw               msaa_level=4
 *     |  +- logo_msaa4      msaa_level=4
 *     +- renderer           msaa_level=2
 *     |  +- logo_msaa2      msaa_level=2
 *     +- draw               msaa_level=0
 */

struct vlc_gl_filters {
    struct vlc_gl_t *gl;
    const struct vlc_gl_api *api;

    /**
     * Interop to use for the input picture.
     */
    struct vlc_gl_interop *interop;
    struct vlc_gl_importer *importer;

    struct vlc_list list; /**< list of vlc_gl_filter.node */

    struct vlc_gl_filters_viewport {
        int x;
        int y;
        unsigned width;
        unsigned height;
    } viewport;

    struct {
        /** Last updated picture PTS */
        vlc_tick_t pts;

        /** Dolby Vision RPU metadata for the last picture, if any */
        vlc_video_dovi_metadata_t dovi_rpu;
        int has_dovi;
    } pic;
};

struct vlc_gl_filters *
vlc_gl_filters_New(struct vlc_gl_t *gl, const struct vlc_gl_api *api,
                   struct vlc_gl_interop *interop)
{
    struct vlc_gl_filters *filters = malloc(sizeof(*filters));
    if (!filters)
        return NULL;

    filters->importer = vlc_gl_importer_New(interop);
    if (!filters->importer)
    {
        free(filters);
        return NULL;
    }

    filters->gl = gl;
    filters->api = api;
    filters->interop = interop;
    vlc_list_init(&filters->list);

    memset(&filters->viewport, 0, sizeof(filters->viewport));
    filters->pic.pts = VLC_TICK_INVALID;
    filters->pic.has_dovi = 0;

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

    vlc_gl_importer_Delete(filters->importer);
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

    struct vlc_gl_tex_size size_in;
    struct vlc_gl_format *glfmt = &priv->glfmt_in;

    struct vlc_gl_filter_priv *prev_filter =
        vlc_list_last_entry_or_null(&filters->list, struct vlc_gl_filter_priv,
                                    node);
    if (!prev_filter)
    {
        size_in.width = filters->interop->fmt_out.i_visible_width;
        size_in.height = filters->interop->fmt_out.i_visible_height;

        assert(filters->importer);
        *glfmt = filters->importer->glfmt;
    }
    else
    {
        size_in = prev_filter->size_out;

        /* If the previous filter operated on planes, then its output chroma is
         * the same as its input chroma. Otherwise, it's RGBA. */
        vlc_fourcc_t chroma = prev_filter->filter.config.filter_planes
                            ? prev_filter->glfmt_in.fmt.i_chroma
                            : VLC_CODEC_RGBA;

        video_format_t *fmt = &glfmt->fmt;
        video_format_Init(fmt, chroma);
        fmt->i_width = fmt->i_visible_width = prev_filter->size_out.width;
        fmt->i_height = fmt->i_visible_height = prev_filter->size_out.height;

        glfmt->tex_target = GL_TEXTURE_2D;
        glfmt->tex_count = prev_filter->plane_count;

        size_t size = glfmt->tex_count * sizeof(GLsizei);
        memcpy(glfmt->tex_widths, prev_filter->plane_widths, size);
        memcpy(glfmt->tex_heights, prev_filter->plane_heights, size);
    }

    /* By default, the output size is the same as the input size. The filter
     * may change it during its Open(). */
    priv->size_out = size_in;

    int ret = vlc_gl_filter_LoadModule(filters->gl, name, filter, config,
                                       glfmt, &priv->size_out);
    if (ret != VLC_SUCCESS)
    {
        /* Creation failed, do not call close() */
        msg_Err(filters->gl, "Could not load OpenGL filter '%s'", name);
        filter->ops = NULL;
        vlc_gl_filter_Delete(filter);
        return NULL;
    }

    /* A blend filter may not change its output size. */
    assert(!filter->config.blend
           || (priv->size_out.width == size_in.width
            && priv->size_out.height == size_in.height));

    /* A filter operating on planes may not blend. */
    assert(!filter->config.filter_planes || !filter->config.blend);

    /* A filter operating on planes may not use anti-aliasing. */
    assert(!filter->config.filter_planes || !filter->config.msaa_level);

    if (filter->config.blend)
    {
        if (!prev_filter || prev_filter->filter.config.filter_planes)
        {
            /* We cannot blend with nothing, so insert a "draw" filter to draw
             * the input picture to blend with. */
            struct vlc_gl_filter *draw =
                vlc_gl_filters_Append(filters, "draw", NULL);
            if (!draw)
            {
                vlc_gl_filter_Delete(filter);
                return NULL;
            }
        }

        /* Append as a subfilter of a non-blend filter */
        struct vlc_gl_filter_priv *last_filter =
            vlc_list_last_entry_or_null(&filters->list,
                                        struct vlc_gl_filter_priv, node);
        assert(last_filter);
        vlc_list_append(&priv->node, &last_filter->blend_subfilters);
    }
    else
    {
        vlc_gl_filter_InitPlaneSizes(filter);

        /* Append to the main filter list */
        vlc_list_append(&priv->node, &filters->list);
    }

    return filter;
}

int
vlc_gl_filters_InitFramebuffers(struct vlc_gl_filters *filters)
{
    struct vlc_gl_filter_priv *priv = NULL;
    struct vlc_gl_filter_priv *subfilter_priv;

    const opengl_vtable_t *vt = &filters->api->vt;

    /* Save the current bindings to restore them at the end */
    GLint renderbuffer;
    GLint draw_framebuffer;
    vt->GetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
    vt->GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer);

    vlc_list_foreach(priv, &filters->list, node)
    {
        /* Compute the highest msaa_level among the filter and its subfilters */
        unsigned msaa_level = 0;
        if (filters->api->supports_multisample)
        {
            msaa_level = priv->filter.config.msaa_level;
            vlc_list_foreach(subfilter_priv, &priv->blend_subfilters, node)
            {
                if (subfilter_priv->filter.config.msaa_level > msaa_level)
                    msaa_level = subfilter_priv->filter.config.msaa_level;
            }
        }

        /* Update the actual msaa_level used to create the MSAA framebuffer */
        priv->filter.config.msaa_level = msaa_level;
        /* Also update the actual msaa_level for subfilters, just for info */
        vlc_list_foreach(subfilter_priv, &priv->blend_subfilters, node)
            subfilter_priv->filter.config.msaa_level = msaa_level;
    }

    /* "priv" is the last filter */
    assert(priv); /* There is at least one filter */

    bool insert_draw =
        /* Resolving multisampling to the default framebuffer might fail,
         * because its format may be different. */
        priv->filter.config.msaa_level ||
        /* A filter operating on planes may produce several textures.
         * They need to be chroma-converted to a single RGBA texture. */
        priv->filter.config.filter_planes;
    if (insert_draw)
    {
        struct vlc_gl_filter *draw =
            vlc_gl_filters_Append(filters, "draw", NULL);
        if (!draw)
            return VLC_EGENERIC;
    }

    vlc_list_foreach(priv, &filters->list, node)
    {
        struct vlc_gl_filter *filter = &priv->filter;

        bool is_last = vlc_list_is_last(&priv->node, &filters->list);
        /* Every non-blend filter needs its own framebuffer, except the last
         * one */
        bool has_out = !is_last;
        int ret = vlc_gl_filter_InitFramebuffers(filter, has_out);
        if (ret != VLC_SUCCESS)
            return ret;
    }

    /* Restore bindings */
    vt->BindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_framebuffer);
    vt->BindRenderbuffer(GL_RENDERBUFFER, renderbuffer);

    return VLC_SUCCESS;
}

int
vlc_gl_filters_UpdatePicture(struct vlc_gl_filters *filters,
                             picture_t *picture)
{
    assert(!vlc_list_is_empty(&filters->list));

    struct vlc_gl_importer *importer = filters->importer;
    int ret = vlc_gl_importer_Update(importer, picture);
    if (ret != VLC_SUCCESS)
        return ret;

    filters->pic.pts = picture->date;

    struct vlc_ancillary *dovi = picture_GetAncillary(picture, VLC_ANCILLARY_ID_DOVI);
    filters->pic.has_dovi = !!dovi;
    if (dovi) {
        memcpy(&filters->pic.dovi_rpu, vlc_ancillary_GetData(dovi),
               sizeof(filters->pic.dovi_rpu));
    }

    struct vlc_gl_filter_priv *first_filter =
        vlc_list_first_entry_or_null(&filters->list, struct vlc_gl_filter_priv,
                                     node);

    assert(first_filter);
    first_filter->has_picture = true;

    return VLC_SUCCESS;
}

int
vlc_gl_filters_Draw(struct vlc_gl_filters *filters)
{
    const opengl_vtable_t *vt = &filters->api->vt;
    struct vlc_gl_importer *importer = filters->importer;

    GLint value;
    vt->GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &value);
    GLuint draw_framebuffer = value; /* as GLuint */

    struct vlc_gl_input_meta meta = {
        .pts = filters->pic.pts,
        .plane = 0,
        .dovi_rpu = filters->pic.has_dovi ? &filters->pic.dovi_rpu : NULL,
    };

    struct vlc_gl_picture direct_pic;
    const struct vlc_gl_picture *pic;

    struct vlc_gl_filter_priv *priv;
    vlc_list_foreach(priv, &filters->list, node)
    {
        struct vlc_gl_filter_priv *previous =
            vlc_list_prev_entry_or_null(&filters->list, priv,
                                        struct vlc_gl_filter_priv, node);
        if (previous)
        {
            memcpy(direct_pic.textures, previous->textures_out,
                   previous->tex_count * sizeof(*direct_pic.textures));
            memcpy(direct_pic.mtx, MATRIX2x3_IDENTITY,
                   sizeof(MATRIX2x3_IDENTITY));
            /* The transform never changes (except for the first picture, where
             * it is defined for the first time) */
            direct_pic.mtx_has_changed = !priv->has_picture;

            priv->has_picture = true;

            pic = &direct_pic;
        }
        else
        {
            assert(importer);
            pic = &importer->pic;
        }

        struct vlc_gl_filter *filter = &priv->filter;

        if (filter->config.filter_planes)
        {
            for (unsigned i = 0; i < priv->tex_count; ++i)
            {
                meta.plane = i;

                /* Select the output texture associated to this plane */
                GLuint draw_fb = priv->framebuffers_out[i];
                vt->BindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fb);

                assert(!vlc_list_is_last(&priv->node, &filters->list));
                vt->Viewport(0, 0, priv->tex_widths[i], priv->tex_heights[i]);

                int ret = filter->ops->draw(filter, pic, &meta);
                if (ret != VLC_SUCCESS)
                    return ret;
            }
        }
        else
        {
            assert(priv->tex_count <= 1);
            unsigned msaa_level = priv->filter.config.msaa_level;
            GLuint draw_fb;
            if (msaa_level)
                draw_fb = priv->framebuffer_msaa;
            else
                draw_fb = priv->tex_count > 0 ? priv->framebuffers_out[0]
                                              : draw_framebuffer;

            vt->BindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fb);

            if (vlc_list_is_last(&priv->node, &filters->list))
            {
                /* The output viewport must be applied on the last filter */
                struct vlc_gl_filters_viewport *vp = &filters->viewport;
                vt->Viewport(vp->x, vp->y, vp->width, vp->height);
            }
            else
                vt->Viewport(0, 0, priv->tex_widths[0], priv->tex_heights[0]);

            meta.plane = 0;
            int ret = filter->ops->draw(filter, pic, &meta);
            if (ret != VLC_SUCCESS)
                return ret;

            /* Draw blend subfilters */
            struct vlc_gl_filter_priv *subfilter_priv;
            vlc_list_foreach(subfilter_priv, &priv->blend_subfilters, node)
            {
                /* Reset the draw buffer, in case it has been changed from a
                 * filter draw() callback */
                vt->BindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fb);

                struct vlc_gl_filter *subfilter = &subfilter_priv->filter;
                ret = subfilter->ops->draw(subfilter, NULL, &meta);
                if (ret != VLC_SUCCESS)
                    return ret;
            }

            if (filter->config.msaa_level)
            {
                /* Never resolve multisampling to the default framebuffer */
                assert(priv->tex_count == 1);
                assert(priv->framebuffers_out[0] != draw_framebuffer);

                /* Resolve the MSAA into the target framebuffer */
                vt->BindFramebuffer(GL_READ_FRAMEBUFFER,
                                    priv->framebuffer_msaa);
                vt->BindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                    priv->framebuffers_out[0]);

                GLint width = priv->size_out.width;
                GLint height = priv->size_out.height;
                vt->BlitFramebuffer(0, 0, width, height,
                                    0, 0, width, height,
                                    GL_COLOR_BUFFER_BIT, GL_NEAREST);
            }
        }
    }

    return VLC_SUCCESS;
}

void
vlc_gl_filters_SetViewport(struct vlc_gl_filters *filters, int x, int y,
                           unsigned width, unsigned height)
{
    filters->viewport.x = x;
    filters->viewport.y = y;
    filters->viewport.width = width;
    filters->viewport.height = height;
}

int
vlc_gl_filters_SetOutputSize(struct vlc_gl_filters *filters, unsigned width,
                             unsigned height)
{
    bool resized = false;
    struct vlc_gl_tex_size req = { width, height };

    struct vlc_gl_filter *next = NULL;

    struct vlc_gl_filter_priv *priv;
    vlc_list_reverse_foreach(priv, &filters->list, node)
    {
        struct vlc_gl_filter *filter = &priv->filter;
        if (!filter->ops->request_output_size) {
            /* Could not propagate further */
            break;
        }

        struct vlc_gl_tex_size optimal_in = {0};
        int ret =
            filter->ops->request_output_size(filter, &req, &optimal_in);
        if (ret != VLC_SUCCESS)
            break;

        /* The filter may have modified the requested size */
        priv->size_out = req;

        /* Recreate the framebuffers/textures with the new size */
        vlc_gl_filter_ApplyOutputSize(filter);

        resized = true;

        /* Notify the next filter of the input size change */
        if (next && next->ops->on_input_size_change)
            next->ops->on_input_size_change(next, &req);

        if (!optimal_in.width || !optimal_in.height)
            /* No specific input size requested, do not propagate further */
            break;

        /* Request the previous filter to output at the optimal input size of
         * the current filter. */
        req = optimal_in;

        /* The filters are iterated backwards, so the current filter will
         * become the next filter. */
        next = filter;
    }

    return resized ? VLC_SUCCESS : VLC_EGENERIC;
}
