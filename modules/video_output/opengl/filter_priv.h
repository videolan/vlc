/*****************************************************************************
 * filter_priv.h
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

#ifndef VLC_GL_FILTER_PRIV_H
#define VLC_GL_FILTER_PRIV_H

#include <vlc_common.h>
#include <vlc_list.h>
#include <vlc_picture.h>

#include "filter.h"
#include "sampler.h"

struct vlc_gl_filter_priv {
    struct vlc_gl_filter filter;

    /* For a blend filter, must be the same as the size_out of the previous
     * filter */
    struct vlc_gl_tex_size size_out;

    /* Only meaningful for non-blend filters { */
    struct vlc_gl_sampler *sampler; /* owned */

    /* owned (this filter must delete it) */
    GLuint framebuffers_out[PICTURE_PLANE_MAX];

    /* owned (each attached to framebuffers_out[i]) */
    GLuint textures_out[PICTURE_PLANE_MAX];
    GLsizei tex_widths[PICTURE_PLANE_MAX];
    GLsizei tex_heights[PICTURE_PLANE_MAX];
    unsigned tex_count;

    /* For multisampling, if msaa_level != 0 */
    GLuint framebuffer_msaa; /* owned */
    GLuint renderbuffer_msaa; /* owned (attached to framebuffer_msaa) */
    /* } */

    /* For lazy-loading sampler */
    struct vlc_gl_filters *filters; /* weak reference to the container */

    /* Previous filter to construct the expected sampler. It is necessary
     * because owner_ops->get_sampler() may be called during the Open(), while
     * the filter is not added to the filter chain yet. */
    struct vlc_gl_filter_priv *prev_filter;

    struct vlc_list node; /**< node of vlc_gl_filters.list */

    /* Blend filters are attached to their non-blend "parent" instead of the
     * filter chain to simplify the rendering code */
    struct vlc_list blend_subfilters; /**< list of vlc_gl_filter_priv.node */
};

#define vlc_gl_filter_PRIV(filter) \
    container_of(filter, struct vlc_gl_filter_priv, filter)

struct vlc_gl_filter *
vlc_gl_filter_New(vlc_object_t *parent, const struct vlc_gl_api *api);
#define vlc_gl_filter_New(o, a) vlc_gl_filter_New(VLC_OBJECT(o), a)

int
vlc_gl_filter_LoadModule(vlc_object_t *parent, const char *name,
                         struct vlc_gl_filter *filter,
                         const config_chain_t *config,
                         struct vlc_gl_tex_size *size_out);
#define vlc_gl_filter_LoadModule(o, a, b, c, d) \
    vlc_gl_filter_LoadModule(VLC_OBJECT(o), a, b, c, d)

void
vlc_gl_filter_Delete(struct vlc_gl_filter *filter);

#endif
