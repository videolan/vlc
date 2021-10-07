/*****************************************************************************
 * picture.h
 *****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

#ifndef VLC_GL_PICTURE_H
#define VLC_GL_PICTURE_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_es.h>
#include <vlc_picture.h>
#include "gl_common.h"

/**
 * Format of an OpenGL picture
 */
struct vlc_gl_format {
    video_format_t fmt;

    GLenum tex_target;

    unsigned tex_count;
    GLsizei tex_widths[PICTURE_PLANE_MAX];
    GLsizei tex_heights[PICTURE_PLANE_MAX];

    GLsizei visible_widths[PICTURE_PLANE_MAX];
    GLsizei visible_heights[PICTURE_PLANE_MAX];
};

/**
 * OpenGL picture
 *
 * It can only be properly used if its format, described by a vlc_gl_format, is
 * known.
 */
struct vlc_gl_picture {
    GLuint textures[PICTURE_PLANE_MAX];

    /**
     * Matrix to convert from 2D pictures coordinates to texture coordinates
     *
     * tex_coords =     mtx    × pic_coords
     *
     *  / tex_x \    / a b c \   / pic_x \
     *  \ tex_y / =  \ d e f / × | pic_y |
     *                           \   1   /
     *
     * It is stored in column-major order: [a, d, b, e, c, f].
     */
    float mtx[2*3];

    /**
     * Indicate if the transform to convert picture coordinates to textures
     * coordinates have changed due to the last picture.
     *
     * The filters should check this flag on every draw() call, and update
     * their coordinates if necessary.
     *
     * It is guaranteed to be true for the first picture.
     */
    bool mtx_has_changed;
};

#endif
