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

#include <vlc_tick.h>

#include "sampler.h"

struct vlc_gl_filter;

struct vlc_gl_tex_size {
    unsigned width;
    unsigned height;
};

struct vlc_gl_input_meta {
    vlc_tick_t pts;
};

typedef int
vlc_gl_filter_open_fn(struct vlc_gl_filter *filter,
                      const config_chain_t *config,
                      struct vlc_gl_tex_size *size_out);

struct vlc_gl_filter_ops {
    /**
     * Draw the result of the filter to the current framebuffer
     */
    int (*draw)(struct vlc_gl_filter *filter,
                const struct vlc_gl_input_meta *meta);

    /**
     * Free filter resources
     */
    void (*close)(struct vlc_gl_filter *filter);
};

struct vlc_gl_filter_owner_ops {
    /**
     * Get the sampler associated to this filter.
     *
     * The instance is lazy-loaded (to avoid creating one for blend filters).
     * Successive calls to this function for the same filter is guaranteed to
     * always return the same sampler.
     *
     * \param filter the filter
     * \return sampler the sampler, NULL on error
     */
    struct vlc_gl_sampler *
    (*get_sampler)(struct vlc_gl_filter *filter);
};

/**
 * OpenGL filter, in charge of a rendering pass.
 */
struct vlc_gl_filter {
    vlc_object_t obj;
    module_t *module;

    const struct vlc_gl_api *api;

    struct {
        /**
         * A blend filter draws over the input picture (without reading it).
         *
         * This flag must be set by the filter module (default is false).
         */
        bool blend;
    } config;

    const struct vlc_gl_filter_ops *ops;
    void *sys;

    const struct vlc_gl_filter_owner_ops *owner_ops;
};

static inline struct vlc_gl_sampler *
vlc_gl_filter_GetSampler(struct vlc_gl_filter *filter)
{
    return filter->owner_ops->get_sampler(filter);
}

#endif
