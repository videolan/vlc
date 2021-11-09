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

struct vlc_gl_filter_priv {
    struct vlc_gl_filter filter;

    /* For a blend filter, must be the same as the size_out of the previous
     * filter */
    struct vlc_gl_tex_size size_out;

    struct vlc_gl_format glfmt_in;

    /* Describe the output planes, independently of whether textures are
     * created for this filter (the last filter does not own any textures). */
    unsigned plane_count;
    GLsizei plane_widths[PICTURE_PLANE_MAX];
    GLsizei plane_heights[PICTURE_PLANE_MAX];

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

    struct vlc_list node; /**< node of vlc_gl_filters.list */

    /* Blend filters are attached to their non-blend "parent" instead of the
     * filter chain to simplify the rendering code */
    struct vlc_list blend_subfilters; /**< list of vlc_gl_filter_priv.node */

    bool has_picture;
};

static inline struct vlc_gl_filter_priv *
vlc_gl_filter_PRIV(struct vlc_gl_filter *filter)
{
    return container_of(filter, struct vlc_gl_filter_priv, filter);
}

struct vlc_gl_filter *
vlc_gl_filter_New(struct vlc_gl_t *gl, const struct vlc_gl_api *api);

int
vlc_gl_filter_LoadModule(vlc_object_t *parent, const char *name,
                         struct vlc_gl_filter *filter,
                         const config_chain_t *config,
                         const struct vlc_gl_format *glfmt,
                         struct vlc_gl_tex_size *size_out);
#define vlc_gl_filter_LoadModule(o, a, b, c, d, e) \
    vlc_gl_filter_LoadModule(VLC_OBJECT(o), a, b, c, d, e)

void
vlc_gl_filter_Delete(struct vlc_gl_filter *filter);

int
vlc_gl_filter_InitFramebuffers(struct vlc_gl_filter *filter, bool is_last);

/** Recompute plane count, widths and heights after size_out have changed */
void
vlc_gl_filter_InitPlaneSizes(struct vlc_gl_filter *filter);

void
vlc_gl_filter_ApplyOutputSize(struct vlc_gl_filter *filter);

#endif
