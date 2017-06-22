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

#include "converter.h"

GLuint
opengl_fragment_shader_init_impl(opengl_tex_converter_t *,
                                 GLenum, vlc_fourcc_t, video_color_space_t);
int
opengl_tex_converter_generic_init(opengl_tex_converter_t *, bool);

void
opengl_tex_converter_generic_deinit(opengl_tex_converter_t *tc);

#endif /* include-guard */
