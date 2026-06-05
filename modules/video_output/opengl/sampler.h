/*****************************************************************************
 * sampler.h
 *****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

#ifndef VLC_GL_SAMPLER_PRIV_H
#define VLC_GL_SAMPLER_PRIV_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_opengl.h>
#include <vlc_opengl_sampler.h>

#include "gl_common.h"
#include "picture.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Create a new sampler
 *
 * \param gl the OpenGL context
 * \param glfmt the input format
 * \param expose_planes if set, vlc_texture() exposes a single plane at a time
 */
struct vlc_gl_sampler *
vlc_gl_sampler_New(struct vlc_gl_t *gl,
                   const struct vlc_gl_format *glfmt, bool expose_planes);

/**
 * Delete a sampler
 *
 * \param sampler the sampler
 */
void
vlc_gl_sampler_Delete(struct vlc_gl_sampler *sampler);

#ifdef __cplusplus
}
#endif

#endif
