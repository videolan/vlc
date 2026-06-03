/*****************************************************************************
 * vlc_opengl_picture.h: VLC OpenGL picture API
 *****************************************************************************
 * Copyright (C) 2020-2026 VLC authors and VideoLAN
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
#define VLC_GL_PICTURE_H 1

#include <math.h>

#include <vlc_picture.h>

/**
 * \file
 * This file defines the public OpenGL picture interface.
 *
 * A vlc_gl_picture stores the OpenGL textures and transformation matrix
 * associated with a VLC picture, as provided by the interop/importer.
 */

/**
 * An OpenGL picture state, with textures and a coordinate transformation matrix.
 */
struct vlc_gl_picture {
    /**
     * Texture handles for each plane.
     */
    uint32_t textures[PICTURE_PLANE_MAX];

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
static inline void
vlc_gl_picture_ToTexCoords(const struct vlc_gl_picture *pic,
                           unsigned coords_count,
                           const float *pic_coords,
                           float *tex_coords_out)
{
    const float *mtx = pic->mtx;
    assert(mtx);

#define MTX(ROW,COL) mtx[(COL)*2+(ROW)]
    for (unsigned i = 0; i < coords_count; ++i)
    {
        /* Store the coordinates, in case the transform must be applied in
         * place (i.e. with pic_coords == tex_coords_out) */
        float x = pic_coords[0];
        float y = pic_coords[1];
        tex_coords_out[0] = MTX(0,0) * x + MTX(0,1) * y + MTX(0,2);
        tex_coords_out[1] = MTX(1,0) * x + MTX(1,1) * y + MTX(1,2);
        pic_coords += 2;
        tex_coords_out += 2;
    }
#undef MTX
}

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
static inline void
vlc_gl_picture_ComputeDirectionMatrix(const struct vlc_gl_picture *pic,
                                      float direction[static 2*2])
{
    /**
     * The direction matrix is extracted from pic->mtx:
     *
     *    mtx = / a b c \
     *          \ d e f /
     *
     * The last column (the offset part of the affine transformation) is
     * discarded, and the 2 remaining column vectors are normalized to remove
     * any scaling:
     *
     *    direction = / a/unorm  b/vnorm \
     *                \ d/unorm  e/vnorm /
     *
     * where unorm = norm( / a \ ) and vnorm = norm( / b \ ).
     *                     \ d /                     \ e /
     */

    float ux = pic->mtx[0];
    float uy = pic->mtx[1];
    float vx = pic->mtx[2];
    float vy = pic->mtx[3];

    float unorm = sqrt(ux * ux + uy * uy);
    float vnorm = sqrt(vx * vx + vy * vy);

    direction[0] = ux / unorm;
    direction[1] = uy / unorm;
    direction[2] = vx / vnorm;
    direction[3] = vy / vnorm;
}

#endif /* VLC_GL_PICTURE_H */
