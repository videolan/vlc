/*****************************************************************************
 * filter.h
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

#ifndef VLC_GL_FILTER_H
#define VLC_GL_FILTER_H

#include "sampler.h"

struct vlc_gl_filter;

struct vlc_gl_tex_size {
    unsigned width;
    unsigned height;
};

typedef int
vlc_gl_filter_open_fn(struct vlc_gl_filter *filter,
                      const config_chain_t *config,
                      struct vlc_gl_tex_size *size_out,
                      struct vlc_gl_sampler *sampler);

struct vlc_gl_filter_ops {
    /**
     * Draw the result of the filter to the current framebuffer
     */
    int (*draw)(struct vlc_gl_filter *filter);

    /**
     * Free filter resources
     */
    void (*close)(struct vlc_gl_filter *filter);
};

/**
 * OpenGL filter, in charge of a rendering pass.
 */
struct vlc_gl_filter {
    vlc_object_t obj;
    module_t *module;

    const struct vlc_gl_api *api;

    const struct vlc_gl_filter_ops *ops;
    void *sys;
};

#endif
