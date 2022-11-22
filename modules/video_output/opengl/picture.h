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

    uint32_t formats[PICTURE_PLANE_MAX];
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

/**
 * Convert from picture coordinates to texture coordinates, which can be used to
 * sample at the correct location.
 *
 * This is a equivalent to retrieve the matrix and multiply manually.
 *
 * The picture and texture coords may point to the same memory, in that case
 * the transformation is applied in place (overwriting the picture coordinates
 * by the texture coordinates).
 *
 * \param picture the OpenGL picture
 * \param coords_count the number of coordinates (x,y) coordinates to convert
 * \param pic_coords picture coordinates as an array of 2*coords_count floats
 * \param tex_coords_out texture coordinates as an array of 2*coords_count
 *                       floats
 */
void
vlc_gl_picture_ToTexCoords(const struct vlc_gl_picture *pic,
                           unsigned coords_count, const float *pic_coords,
                           float *tex_coords_out);

/**
 * Return a matrix to orient texture coordinates
 *
 * This matrix is 2x2 and is stored in column-major order.
 *
 * While pic_to_tex_matrix transforms any picture coordinates into texture
 * coordinates, it may be useful for example for vertex or fragment shaders to
 * sample one pixel to the left of the current one, or two pixels to the top.
 * Since the input texture may be rotated or flipped, the shaders need to
 * know in which direction is the top and in which direction is the right of
 * the picture.
 *
 * This 2x2 matrix allows to transform a 2D vector expressed in picture
 * coordinates into a 2D vector expressed in texture coordinates.
 *
 * Concretely, it contains the coordinates (U, V) of the transformed unit
 * vectors u = / 1 \ and v = / 0 \:
 *             \ 0 /         \ 1 /
 *
 *     / Ux Vx \
 *     \ Uy Vy /
 *
 * It is guaranteed that:
 *  - both U and V are unit vectors (this matrix does not change the scaling);
 *  - only one of their components have a non-zero value (they may not be
 *    oblique); in other words, here are the possible values for U and V:
 *
 *        /  0 \  or  / 0 \  or  / 1 \  or  / -1 \
 *        \ -1 /      \ 1 /      \ 0 /      \  0 /
 *
 *  - U and V are orthogonal.
 *
 * Therefore, there are 8 possible matrices (4 possible rotations, flipped or
 * not).
 *
 * It may theoretically change on every picture (the transform matrix provided
 * by Android may change). If it has changed since the last picture, then
 * pic->mtx_has_changed is true.
 */
void
vlc_gl_picture_ComputeDirectionMatrix(const struct vlc_gl_picture *pic,
                                      float direction[2*2]);

#endif
