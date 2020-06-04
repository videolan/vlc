/*****************************************************************************
 * sampler_priv.h
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

#include <vlc_common.h>

#include "sampler.h"

struct vlc_gl_interop;

/**
 * Create a new sampler
 *
 * \param interop the interop
 */
struct vlc_gl_sampler *
vlc_gl_sampler_New(struct vlc_gl_interop *interop);

/**
 * Delete a sampler
 *
 * \param sampler the sampler
 */
void
vlc_gl_sampler_Delete(struct vlc_gl_sampler *sampler);

/**
 * Update the input picture
 *
 * This changes the current input picture, available from the fragment shader.
 *
 * \param sampler the sampler
 * \param picture the new picture
 */
int
vlc_gl_sampler_Update(struct vlc_gl_sampler *sampler, picture_t *picture);

#endif
