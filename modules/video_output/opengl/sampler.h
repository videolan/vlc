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

#include "gl_api.h"
#include "gl_common.h"
#include "picture.h"

#ifdef __cplusplus
extern "C"
{
#endif

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
    struct vlc_gl_format glfmt;

    /**
     * Matrix to convert from picture coordinates to texture coordinates
     *
     * The matrix is 2x3 and is stored in column-major order:
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
         * Version header that is appropriate for this shader.
         */
        char *version;

        /**
         * Precision preamble that is appropriate for this shader.
         */
        char *precision;

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
 * Create a new sampler
 *
 * \param gl the OpenGL context
 * \param api the OpenGL API
 * \param glfmt the input format
 * \param expose_planes if set, vlc_texture() exposes a single plane at a time
 *                      (selected by vlc_gl_sampler_SetCurrentPlane())
 */
struct vlc_gl_sampler *
vlc_gl_sampler_New(struct vlc_gl_t *gl, const struct vlc_gl_api *api,
                   const struct vlc_gl_format *glfmt, bool expose_planes);

/**
 * Delete a sampler
 *
 * \param sampler the sampler
 */
void
vlc_gl_sampler_Delete(struct vlc_gl_sampler *sampler);

/**
 * Update the input textures
 *
 * \param sampler the sampler
 * \param picture the OpenGL picture
 */
int
vlc_gl_sampler_Update(struct vlc_gl_sampler *sampler,
                      const struct vlc_gl_picture *picture);

/**
 * Select the plane to expose
 *
 * If the sampler exposes planes separately (for plane filters), select the
 * plane to expose via the GLSL function vlc_texture().
 *
 * \param sampler the sampler
 * \param plane the plane number
 */
void
vlc_gl_sampler_SelectPlane(struct vlc_gl_sampler *sampler, unsigned plane);

#ifdef __cplusplus
}
#endif

#endif
