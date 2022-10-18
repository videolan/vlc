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

#include "picture.h"

struct vlc_gl_filter;

#ifdef __cplusplus
extern "C"
{
#endif

struct vlc_gl_tex_size {
    unsigned width;
    unsigned height;
};

struct vlc_gl_input_meta {
    vlc_tick_t pts;
    unsigned plane;
    const vlc_video_dovi_metadata_t *dovi_rpu;
};

typedef int
vlc_gl_filter_open_fn(struct vlc_gl_filter *filter,
                      const config_chain_t *config,
                      const struct vlc_gl_format *glfmt,
                      struct vlc_gl_tex_size *size_out);

#define set_callback_opengl_filter(open) \
    { \
        vlc_gl_filter_open_fn *fn = open; \
        (void) fn; \
        set_callback(fn); \
    }

struct vlc_gl_filter_ops {
    /**
     * Draw the result of the filter to the current framebuffer
     */
    int (*draw)(struct vlc_gl_filter *filter, const struct vlc_gl_picture *pic,
                const struct vlc_gl_input_meta *meta);

    /**
     * Free filter resources
     */
    void (*close)(struct vlc_gl_filter *filter);

    /**
     * Request a (responsive) filter to adapt its output size (optional)
     *
     * A responsive filter is a filter for which the size of the produced
     * pictures depends on the output (e.g. display) size rather than the
     * input. This is for example the case for a renderer.
     *
     * A new output size is requested (size_out). The filter is authorized to
     * change the size_out to enforce its own constraints.
     *
     * In addition, it may request to the previous filter (if any) an optimal
     * size it wants to receive. If set to non-zero value, this previous filter
     * will receive this size as its requested size (and so on).
     *
     * \retval true if the resize is accepted (possibly with a modified
     *              size_out)
     * \retval false if the resize is rejected (included on error)
     */
    int (*request_output_size)(struct vlc_gl_filter *filter,
                               struct vlc_gl_tex_size *size_out,
                               struct vlc_gl_tex_size *optimal_in);

    /**
     * Callback to notify input size changes
     *
     * When a filter changes its output size as a result of
     * request_output_size(), the next filter is notified by this callback.
     */
    void (*on_input_size_change)(struct vlc_gl_filter *filter,
                                 const struct vlc_gl_tex_size *size);
};

/**
 * OpenGL filter, in charge of a rendering pass.
 */
struct vlc_gl_filter {
    vlc_object_t obj;
    module_t *module;

    struct vlc_gl_t *gl;
    const struct vlc_gl_api *api;
    const struct vlc_gl_format *glfmt_in;

    struct {
        /**
         * An OpenGL filter may either operate on the input RGBA picture, or on
         * individual input planes (without chroma conversion) separately.
         *
         * In practice, this is useful for deinterlace filters.
         *
         * This flag must be set by the filter module (default is false).
         */
        bool filter_planes;

        /**
         * A blend filter draws over the input picture (without reading it).
         *
         * Meaningless if filter_planes is true.
         *
         * This flag must be set by the filter module (default is false).
         */
        bool blend;

        /**
         * Request MSAA level.
         *
         * This value must be set by the filter module (default is 0, which
         * means disabled).
         *
         * Meaningless if filter_planes is true.
         *
         * The actual MSAA level may be overwritten to 0 if multisampling is
         * not supported, or to a higher value if another filter rendering on
         * the same framebuffer requested a higher MSAA level.
         */
        unsigned msaa_level;
    } config;

    const struct vlc_gl_filter_ops *ops;
    void *sys;
};

#ifdef __cplusplus
}
#endif

#endif
