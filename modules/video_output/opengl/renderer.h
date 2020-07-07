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

#include "filter.h"
#include "gl_api.h"
#include "gl_common.h"
#include "interop.h"
#include "sampler.h"

struct pl_context;
struct pl_shader;
struct pl_shader_res;

#define add_opengl_submodule_renderer() \
    add_submodule() \
    add_shortcut("renderer") \
    set_shortname("renderer") \
    set_capability("opengl filter", 0) \
    set_callback(vlc_gl_renderer_Open)

/**
 * OpenGL picture renderer
 */
struct vlc_gl_renderer
{
    /* Set by the caller */
    const struct vlc_gl_api *api;
    const opengl_vtable_t *vt; /* for convenience, same as &api->vt */

    /* True to dump shaders */
    bool dump_shaders;

    GLuint program_id;

    struct {
        GLfloat ProjectionMatrix[16];
        GLfloat StereoMatrix[3*3];
        GLfloat ZoomMatrix[16];
        GLfloat ViewMatrix[16];
    } var;

    struct {
        GLint StereoMatrix;
        GLint ProjectionMatrix;
        GLint ViewMatrix;
        GLint ZoomMatrix;
    } uloc;

    struct {
        GLint PicCoordsIn;
        GLint VertexPosition;
    } aloc;

    struct vlc_gl_sampler *sampler;

    unsigned nb_indices;
    GLuint vertex_buffer_object;
    GLuint index_buffer_object;
    GLuint texture_buffer_object;

    /* View point */
    vlc_viewpoint_t vp;
    float f_teta;
    float f_phi;
    float f_roll;
    float f_fovx; /* f_fovx and f_fovy are linked but we keep both */
    float f_fovy; /* to avoid recalculating them when needed.      */
    float f_z;    /* Position of the camera on the shpere radius vector */
    float f_sar;
};

vlc_gl_filter_open_fn vlc_gl_renderer_Open;

int
vlc_gl_renderer_SetViewpoint(struct vlc_gl_renderer *renderer,
                             const vlc_viewpoint_t *p_vp);

void
vlc_gl_renderer_SetWindowAspectRatio(struct vlc_gl_renderer *renderer,
                                     float f_sar);

#endif /* include-guard */
