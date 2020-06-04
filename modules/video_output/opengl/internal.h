/*****************************************************************************
 * internal.h: OpenGL internal header
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
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

#ifndef VLC_OPENGL_INTERNAL_H
#define VLC_OPENGL_INTERNAL_H

#include "interop.h"
#include "renderer.h"

int
opengl_interop_init_impl(struct vlc_gl_interop *interop, GLenum tex_target,
                         vlc_fourcc_t chroma, video_color_space_t yuv_space);

int
opengl_interop_generic_init(struct vlc_gl_interop *interop, bool);

void
opengl_interop_generic_deinit(struct vlc_gl_interop *interop);

#endif /* include-guard */
