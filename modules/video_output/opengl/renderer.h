/*****************************************************************************
 * renderer.h: OpenGL internal header
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
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

#ifndef VLC_GL_RENDERER_H
#define VLC_GL_RENDERER_H

#include <vlc_common.h>
#include <vlc_codec.h>
#include <vlc_opengl.h>
#include <vlc_plugin.h>

#include "gl_api.h"
#include "gl_common.h"
#include "interop.h"

struct pl_context;
struct pl_shader;
struct pl_shader_res;

/**
 * OpenGL picture renderer
 */
struct vlc_gl_renderer
{
    /* Pointer to object gl, set by the caller */
    vlc_gl_t *gl;

    /* libplacebo context, created by the caller (optional) */
    struct pl_context *pl_ctx;

    /* Set by the caller */
    const struct vlc_gl_api *api;
    const opengl_vtable_t *vt; /* for convenience, same as &api->vt */

    /* True to dump shaders, set by the caller */
    bool b_dump_shaders;

    /* GLSL version, set by the caller. 100 for GLSL ES, 120 for desktop GLSL */
    unsigned glsl_version;
    /* Precision header, set by the caller. In OpenGLES, the fragment language
     * has no default precision qualifier for floating point types. */
    const char *glsl_precision_header;

    GLuint program_id;

    struct {
        GLfloat OrientationMatrix[16];
        GLfloat ProjectionMatrix[16];
        GLfloat StereoMatrix[3*3];
        GLfloat ZoomMatrix[16];
        GLfloat ViewMatrix[16];

        GLfloat TexCoordsMap[PICTURE_PLANE_MAX][3*3];
    } var;

    struct {
        GLint Texture[PICTURE_PLANE_MAX];
        GLint TexSize[PICTURE_PLANE_MAX]; /* for GL_TEXTURE_RECTANGLE */
        GLint ConvMatrix;
        GLint FillColor;
        GLint *pl_vars; /* for pl_sh_res */

        GLint TransformMatrix;
        GLint OrientationMatrix;
        GLint StereoMatrix;
        GLint ProjectionMatrix;
        GLint ViewMatrix;
        GLint ZoomMatrix;

        GLint TexCoordsMap[PICTURE_PLANE_MAX];
    } uloc;

    struct {
        GLint PicCoordsIn;
        GLint VertexPosition;
    } aloc;

    bool yuv_color;
    GLfloat conv_matrix[16];

    struct pl_shader *pl_sh;
    const struct pl_shader_res *pl_sh_res;

    struct vlc_gl_interop *interop;

    video_format_t fmt;

    GLsizei tex_width[PICTURE_PLANE_MAX];
    GLsizei tex_height[PICTURE_PLANE_MAX];

    GLuint textures[PICTURE_PLANE_MAX];

    unsigned nb_indices;
    GLuint vertex_buffer_object;
    GLuint index_buffer_object;
    GLuint texture_buffer_object;

    struct {
        unsigned int i_x_offset;
        unsigned int i_y_offset;
        unsigned int i_visible_width;
        unsigned int i_visible_height;
    } last_source;

    /* View point */
    vlc_viewpoint_t vp;
    float f_teta;
    float f_phi;
    float f_roll;
    float f_fovx; /* f_fovx and f_fovy are linked but we keep both */
    float f_fovy; /* to avoid recalculating them when needed.      */
    float f_z;    /* Position of the camera on the shpere radius vector */
    float f_sar;

    /**
     * Callback to fetch locations of uniform or attributes variables
     *
     * This function pointer cannot be NULL. This callback is called one time
     * after init.
     *
     * \param renderer OpenGL renderer
     * \param program linked program that will be used by this renderer
     * \return VLC_SUCCESS or a VLC error
     */
    int (*pf_fetch_locations)(struct vlc_gl_renderer *renderer, GLuint program);

    /**
     * Callback to prepare the fragment shader
     *
     * This function pointer cannot be NULL. This callback can be used to
     * specify values of uniform variables.
     *
     * \param renderer OpenGL renderer
     * \param tex_width array of tex width (one per plane)
     * \param tex_height array of tex height (one per plane)
     * \param alpha alpha value, used only for RGBA fragment shader
     */
    void (*pf_prepare_shader)(const struct vlc_gl_renderer *renderer,
                              const GLsizei *tex_width, const GLsizei *tex_height,
                              float alpha);
};

/**
 * Create a new renderer
 *
 * \param gl the GL context
 * \param api the OpenGL API
 * \param context the video context
 * \param fmt the video format
 * \param dump_shaders indicate if the shaders must be dumped in logs
 */
struct vlc_gl_renderer *
vlc_gl_renderer_New(vlc_gl_t *gl, const struct vlc_gl_api *api,
                    vlc_video_context *context, const video_format_t *fmt,
                    bool dump_shaders);

/**
 * Delete a renderer
 *
 * \param renderer the renderer
 */
void
vlc_gl_renderer_Delete(struct vlc_gl_renderer *renderer);

/**
 * Prepare the fragment shader
 *
 * Concretely, it allocates OpenGL textures if necessary and uploads the
 * picture.
 *
 * \param sr the renderer
 * \param subpicture the subpicture to render
 */
int
vlc_gl_renderer_Prepare(struct vlc_gl_renderer *renderer, picture_t *picture);

/**
 * Draw the prepared picture
 *
 * \param sr the renderer
 */
int
vlc_gl_renderer_Draw(struct vlc_gl_renderer *renderer);

int
vlc_gl_renderer_SetViewpoint(struct vlc_gl_renderer *renderer,
                             const vlc_viewpoint_t *p_vp);

void
vlc_gl_renderer_SetWindowAspectRatio(struct vlc_gl_renderer *renderer,
                                     float f_sar);

#endif /* include-guard */
