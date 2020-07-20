/*****************************************************************************
 * filters.h
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

#ifndef VLC_GL_FILTERS_H
#define VLC_GL_FILTERS_H

#include <vlc_common.h>
#include <vlc_list.h>
#include <vlc_opengl.h>
#include <vlc_tick.h>

#include "filter.h"
#include "gl_api.h"
#include "interop.h"
#include "sampler.h"

struct vlc_gl_filters;

/**
 * Create a new OpenGL filter chain
 *
 * \param gl the OpenGL context
 * \param api the OpenGL api
 * \param interop the interop to use for the sampler of the first filter
 */
struct vlc_gl_filters *
vlc_gl_filters_New(struct vlc_gl_t *gl, const struct vlc_gl_api *api,
                   struct vlc_gl_interop *interop);

/**
 * Delete the OpenGL filter chain
 *
 * \param filters the filter chain
 */
void
vlc_gl_filters_Delete(struct vlc_gl_filters *filters);

/**
 * Create and append a filter loaded from a module to the filter chain
 *
 * The created filter is owned by the filter chain.
 *
 * \param filters the filter chain
 * \param name the module name
 * \param config the module configuration
 * \return a weak reference to the filter (NULL on error)
 */
struct vlc_gl_filter *
vlc_gl_filters_Append(struct vlc_gl_filters *filters, const char *name,
                      const config_chain_t *config);

/**
 * Init the framebuffers for the appended filters.
 *
 * This function must be called once after all filters have been appended. It
 * is an error to call vlc_gl_filters_Append() after this function.
 *
 * \param filters the filter chain
 * \return VLC_SUCCESS on success, another value on error
 */
int
vlc_gl_filters_InitFramebuffers(struct vlc_gl_filters *filters);

/**
 * Update the input picture to pass to the first filter
 *
 * \param filters the filter chain
 * \param picture the new input picture
 * \return VLC_SUCCESS on success, another value on error
 */
int
vlc_gl_filters_UpdatePicture(struct vlc_gl_filters *filters,
                             picture_t *picture);

bool
vlc_gl_filters_WillUpdate(struct vlc_gl_filters *filters, bool new_picture);

/**
 * Draw by executing all the filters
 *
 * \param filters the filter chain
 * \return VLC_SUCCESS on success, another value on error
 */
int
vlc_gl_filters_Draw(struct vlc_gl_filters *filters,
                    struct vlc_gl_input_meta *output_meta);

/**
 * Flush all filters
 *
 * Discard any internal filter state.
 *
 * \param filters the filter chain
 */
void
vlc_gl_filters_Flush(struct vlc_gl_filters *filters);

/**
 * Set the viewport.
 */
void
vlc_gl_filters_SetViewport(struct vlc_gl_filters *filters, int x, int y,
                           unsigned width, unsigned height);

#endif
