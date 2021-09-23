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

#ifndef VLC_GL_SAMPLER_H
#define VLC_GL_SAMPLER_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_opengl.h>
#include <vlc_picture.h>

#include "gl_common.h"

/**
 * The purpose of a sampler is to provide pixel values of a VLC input picture,
 * stored in any format.
 *
 * Concretely, a GLSL function:
 *
 *     vec4 vlc_texture(vec2 coords)
 *
 * returns the RGBA values for the given coordinates.
 *
 * Contrary to the standard GLSL function:
 *
 *     vec4 texture2D(sampler2D sampler, vec2 coords)
 *
 * it does not take a sampler2D as parameter. The role of the sampler is to
 * abstract the input picture from the renderer, so the input picture is
 * implicitly available.
 */
struct vlc_gl_sampler {
    /* Input format */
    video_format_t fmt;

    /* Number of input planes */
    unsigned tex_count;

    /* Texture sizes (arrays of tex_count values) */
    const GLsizei *tex_widths;
    const GLsizei *tex_heights;

    /**
     * Matrix to convert from picture coordinates to texture coordinates
     *
     * The matrix is 3x2 and is stored in column-major order:
     *
     *     / a b c \
     *     \ d e f /
     *
     * It is stored as an array of 6 floats:
     *
     *     [a, d, b, e, c, f]
     *
     * To compute texture coordinates, left-multiply the picture coordinates
     * by this matrix:
     *
     *     tex_coords = pic_to_tex_matrix × pic_coords
     *
     *      / tex_x \       / a b c \       / pic_x \
     *      \ tex_y / =     \ d e f /     × | pic_y |
     *                                      \   1   /
     *
     * It is NULL before the first picture is available and may theoretically
     * change on every picture (the transform matrix provided by Android may
     * change). If it has changed since the last picture, then
     * vlc_gl_sampler_MustRecomputeCoords() will return true.
     */
    const float *pic_to_tex_matrix;

    struct {
        /**
         * Piece of fragment shader code declaration OpenGL extensions.
         *
         * It is initialized by the sampler, and may be NULL if no extensions
         * are required.
         *
         * If non-NULL, users of this sampler must inject this provided code
         * into their fragment shader, immediately after the "version" line.
         */
        char *extensions;

        /**
         * Piece of fragment shader code providing the GLSL function
         * vlc_texture(vec2 coords).
         *
         * It is initialized by the sampler, and is never NULL.
         *
         * Users of this sampler should inject this provided code into their
         * fragment shader, before any call to vlc_texture().
         */
        char *body;
    } shader;

    const struct vlc_gl_sampler_ops *ops;
};

struct vlc_gl_sampler_ops {
    /**
     * Callback to fetch locations of uniform or attributes variables
     *
     * This function pointer cannot be NULL. This callback is called one time
     * after init.
     *
     * \param sampler the sampler
     * \param program linked program that will be used by this sampler
     */
    void
    (*fetch_locations)(struct vlc_gl_sampler *sampler, GLuint program);

    /**
     * Callback to load sampler data
     *
     * This function pointer cannot be NULL. This callback can be used to
     * specify values of uniform variables.
     *
     * \param sampler the sampler
     */
    void
    (*load)(struct vlc_gl_sampler *sampler);
};

static inline void
vlc_gl_sampler_FetchLocations(struct vlc_gl_sampler *sampler, GLuint program)
{
    sampler->ops->fetch_locations(sampler, program);
}

static inline void
vlc_gl_sampler_Load(struct vlc_gl_sampler *sampler)
{
    sampler->ops->load(sampler);
}

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
 * \param sampler the sampler
 * \param coords_count the number of coordinates (x,y) coordinates to convert
 * \param pic_coords picture coordinates as an array of 2*coords_count floats
 * \param tex_coords_out texture coordinates as an array of 2*coords_count
 *                       floats
 */
void
vlc_gl_sampler_PicToTexCoords(struct vlc_gl_sampler *sampler,
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
 * Calling this function before the first picture (i.e. when
 * sampler->pic_to_tex_matrix is NULL) results in undefined behavior.
 *
 * It may theoretically change on every picture (the transform matrix provided
 * by Android may change). If it has changed since the last picture, then
 * vlc_gl_sampler_MustRecomputeCoords() will return true.
 */
void
vlc_gl_sampler_ComputeDirectionMatrix(struct vlc_gl_sampler *sampler,
                                      float direction[static 2*2]);

/**
 * Indicate if the transform to convert picture coordinates to textures
 * coordinates have changed due to the last picture.
 *
 * The filters should call this function on every draw() call, and update their
 * coordinates if necessary (using vlc_gl_sampler_PicToTexCoords()).
 *
 * It is guaranteed that it returns true for the first picture.
 *
 * \param sampler the sampler
 * \retval true if the transform has changed due to the last picture
 * \retval false if the transform remains the same
 */
bool
vlc_gl_sampler_MustRecomputeCoords(struct vlc_gl_sampler *sampler);

#endif
