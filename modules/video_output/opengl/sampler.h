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
    struct vlc_gl_t *gl;
    const opengl_vtable_t *vt;

    /* Input format */
    const video_format_t *fmt;

    struct {
        GLfloat OrientationMatrix[4*4];
        GLfloat TexCoordsMap[PICTURE_PLANE_MAX][3*3];
    } var;
    struct {
        GLint Texture[PICTURE_PLANE_MAX];
        GLint TexSize[PICTURE_PLANE_MAX]; /* for GL_TEXTURE_RECTANGLE */
        GLint ConvMatrix;
        GLint *pl_vars; /* for pl_sh_res */

        GLint TransformMatrix;
        GLint OrientationMatrix;
        GLint TexCoordsMap[PICTURE_PLANE_MAX];
    } uloc;

    bool yuv_color;
    GLfloat conv_matrix[4*4];

    /* libplacebo context */
    struct pl_context *pl_ctx;
    struct pl_shader *pl_sh;
    const struct pl_shader_res *pl_sh_res;

    GLsizei tex_width[PICTURE_PLANE_MAX];
    GLsizei tex_height[PICTURE_PLANE_MAX];

    GLuint textures[PICTURE_PLANE_MAX];

    struct {
        unsigned int i_x_offset;
        unsigned int i_y_offset;
        unsigned int i_visible_width;
        unsigned int i_visible_height;
    } last_source;

    struct vlc_gl_interop *interop;

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

    /**
     * Callback to fetch locations of uniform or attributes variables
     *
     * This function pointer cannot be NULL. This callback is called one time
     * after init.
     *
     * \param sampler the sampler
     * \param program linked program that will be used by this sampler
     * \return VLC_SUCCESS or a VLC error
     */
    int (*pf_fetch_locations)(struct vlc_gl_sampler *sampler, GLuint program);

    /**
     * Callback to prepare the fragment shader
     *
     * This function pointer cannot be NULL. This callback can be used to
     * specify values of uniform variables.
     *
     * \param sampler the sampler
     */
    void (*pf_prepare_shader)(const struct vlc_gl_sampler *sampler);
};

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

static inline int
vlc_gl_sampler_FetchLocations(struct vlc_gl_sampler *sampler, GLuint program)
{
    return sampler->pf_fetch_locations(sampler, program);
}

static inline void
vlc_gl_sampler_PrepareShader(const struct vlc_gl_sampler *sampler)
{
    sampler->pf_prepare_shader(sampler);
}

#endif
