/*****************************************************************************
 * mock.c: mock OpenGL filter
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

/**
 * This mock draws a triangle, by default in blend mode:
 *
 *     ./vlc file.mkv --video-filter='opengl{filter=mock}'
 *
 * The filter configuration may be passed as a separate parameter:
 *
 *    ./vlc file.mkv --video-filter=opengl --opengl-filter=mock
 *
 * It can be configured as a non-blend filter, to mask the video with the
 * triangle instead:
 *
 *     ./vlc file.mkv --video-filter='opengl{filter=mock{mask}}'
 *
 * The triangle may be rotated (the value is in degrees):
 *
 *     ./vlc file.mkv --video-filter='opengl{filter=mock{angle=45}}'
 *     ./vlc file.mkv --video-filter='opengl{filter=mock{mask,angle=45}}'
 *
 * If a speed is specified, the triangle automatically rotates with the video
 * timestamps:
 *
 *     ./vlc file.mkv --video-filter='opengl{filter=mock{speed=1}}'
 *
 * Multi-sampling anti-aliasing level can be specified (default is 4):
 *
 *     ./vlc file.mkv --video-filter='opengl{filter=mock{msaa=0}}'
 *
 * Several instances may be combined:
 *
 *     ./vlc file.mkv --video-filter='opengl{filter="mock{mask,speed=1}:mock{angle=180,speed=-1}"}'
 *
 * It can also be used to filter planes separately, drawing each one with a
 * small offset:
 *
 *     ./vlc file.mkv --video-filter='opengl{filter=mock{plane}}'
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_opengl.h>

#include <math.h>

#include "filter.h"
#include "gl_api.h"
#include "gl_common.h"
#include "gl_util.h"

#define MOCK_CFG_PREFIX "mock-"

static const char *const filter_options[] = {
    "angle", "mask", "msaa", "plane", "speed", NULL
};

struct sys {
    GLuint program_id;

    GLuint vbo;

    struct {
        GLint vertex_pos;
        GLint rotation_matrix;
        GLint vertex_color; // blend (non-mask) only
        GLint plane;
    } loc;

    float theta0;
    float speed;

    float rotation_matrix[16];
    float ar;
};

static void
InitMatrix(struct sys *sys, vlc_tick_t pts)
{
    float time_sec = secf_from_vlc_tick(pts);
    /* Full cycle in 60 seconds if speed = 1 */
    float theta = sys->theta0 + sys->speed * time_sec * 2 * 3.141592f / 60;
    float cos_theta = cos(theta);
    float sin_theta = sin(theta);
    float ar = sys->ar;

    /* Defined in column-major order */
    memcpy(sys->rotation_matrix, (float[16]) {
        cos_theta,        sin_theta * ar,  0,          0,
        -sin_theta / ar,  cos_theta,       0,          0,
        0,                0,               0,          0,
        0,                0,               0,          1,
    }, sizeof(sys->rotation_matrix));
}

static int
DrawBlend(struct vlc_gl_filter *filter, const struct vlc_gl_input_meta *meta)
{
    struct sys *sys = filter->sys;

    const opengl_vtable_t *vt = &filter->api->vt;

    vt->UseProgram(sys->program_id);

    vt->Enable(GL_BLEND);
    vt->BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /*
     * The VBO data contains, for each vertex, 2 floats for the vertex position
     * followed by 3 floats for the associated color:
     *
     *  |     vertex 0      |     vertex 1      | ...
     *  | x | y | R | G | B | x | y | R | G | B | x | ...
     *   \-----/ \---------/
     * vertex_pos vertex_color
     */

    const GLsizei stride = 5 * sizeof(float);

    vt->BindBuffer(GL_ARRAY_BUFFER, sys->vbo);

    vt->EnableVertexAttribArray(sys->loc.vertex_pos);
    vt->VertexAttribPointer(sys->loc.vertex_pos, 2, GL_FLOAT, GL_FALSE, stride,
                            (const void *) 0);

    intptr_t offset = 2 * sizeof(float);
    vt->EnableVertexAttribArray(sys->loc.vertex_color);
    vt->VertexAttribPointer(sys->loc.vertex_color, 3, GL_FLOAT, GL_FALSE,
                            stride, (const void *) offset);

    InitMatrix(sys, meta->pts);
    vt->UniformMatrix4fv(sys->loc.rotation_matrix, 1, GL_FALSE,
                         sys->rotation_matrix);

    vt->DrawArrays(GL_TRIANGLES, 0, 3);

    vt->Disable(GL_BLEND);

    return VLC_SUCCESS;
}

static int
DrawMask(struct vlc_gl_filter *filter, const struct vlc_gl_input_meta *meta)
{
    struct sys *sys = filter->sys;

    const opengl_vtable_t *vt = &filter->api->vt;

    vt->UseProgram(sys->program_id);

    struct vlc_gl_sampler *sampler = vlc_gl_filter_GetSampler(filter);
    vlc_gl_sampler_Load(sampler);

    vt->BindBuffer(GL_ARRAY_BUFFER, sys->vbo);
    vt->EnableVertexAttribArray(sys->loc.vertex_pos);
    vt->VertexAttribPointer(sys->loc.vertex_pos, 2, GL_FLOAT, GL_FALSE, 0,
                            (const void *) 0);

    InitMatrix(sys, meta->pts);
    vt->UniformMatrix4fv(sys->loc.rotation_matrix, 1, GL_FALSE,
                         sys->rotation_matrix);

    vt->Clear(GL_COLOR_BUFFER_BIT);
    vt->DrawArrays(GL_TRIANGLES, 0, 3);

    return VLC_SUCCESS;
}

static int
DrawPlane(struct vlc_gl_filter *filter, const struct vlc_gl_input_meta *meta)
{
    struct sys *sys = filter->sys;

    const opengl_vtable_t *vt = &filter->api->vt;

    vt->UseProgram(sys->program_id);

    struct vlc_gl_sampler *sampler = vlc_gl_filter_GetSampler(filter);
    vlc_gl_sampler_Load(sampler);

    vt->Uniform1i(sys->loc.plane, (int) meta->plane);

    vt->BindBuffer(GL_ARRAY_BUFFER, sys->vbo);
    vt->EnableVertexAttribArray(sys->loc.vertex_pos);
    vt->VertexAttribPointer(sys->loc.vertex_pos, 2, GL_FLOAT, GL_FALSE, 0,
                            (const void *) 0);

    vt->Clear(GL_COLOR_BUFFER_BIT);
    vt->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    return VLC_SUCCESS;
}

static void
Close(struct vlc_gl_filter *filter)
{
    struct sys *sys = filter->sys;

    const opengl_vtable_t *vt = &filter->api->vt;
    vt->DeleteProgram(sys->program_id);
    vt->DeleteBuffers(1, &sys->vbo);

    free(sys);
}

static int
InitBlend(struct vlc_gl_filter *filter)
{
    struct sys *sys = filter->sys;
    const opengl_vtable_t *vt = &filter->api->vt;

    static const char *const VERTEX_SHADER_BODY =
        "attribute vec2 vertex_pos;\n"
        "attribute vec3 vertex_color;\n"
        "uniform mat4 rotation_matrix;\n"
        "varying vec3 color;\n"
        "void main() {\n"
        "  gl_Position = rotation_matrix * vec4(vertex_pos, 0.0, 1.0);\n"
        "  color = vertex_color;\n"
        "}\n";

    static const char *const FRAGMENT_SHADER_BODY =
        "varying vec3 color;\n"
        "void main() {\n"
        "  gl_FragColor = vec4(color, 0.5);\n"
        "}\n";

    const char *shader_version;
    const char *shader_precision;
    if (filter->api->is_gles)
    {
        shader_version = "#version 100\n";
        shader_precision = "precision highp float;\n";
    }
    else
    {
        shader_version = "#version 120\n";
        shader_precision = "";
    }

    const char *vertex_shader[] = {
        shader_version,
        VERTEX_SHADER_BODY,
    };
    const char *fragment_shader[] = {
        shader_version,
        shader_precision,
        FRAGMENT_SHADER_BODY,
    };

    GLuint program_id =
        vlc_gl_BuildProgram(VLC_OBJECT(filter), vt,
                            ARRAY_SIZE(vertex_shader), vertex_shader,
                            ARRAY_SIZE(fragment_shader), fragment_shader);

    if (!program_id)
        return VLC_EGENERIC;

    sys->program_id = program_id;

    sys->loc.vertex_pos = vt->GetAttribLocation(sys->program_id, "vertex_pos");
    assert(sys->loc.vertex_pos != -1);

    sys->loc.rotation_matrix = vt->GetUniformLocation(sys->program_id,
                                                      "rotation_matrix");
    assert(sys->loc.rotation_matrix != -1);

    sys->loc.vertex_color = vt->GetAttribLocation(program_id, "vertex_color");
    assert(sys->loc.vertex_color != -1);

    sys->loc.rotation_matrix = vt->GetUniformLocation(sys->program_id,
                                                      "rotation_matrix");
    assert(sys->loc.rotation_matrix != -1);

    vt->GenBuffers(1, &sys->vbo);

    static const GLfloat data[] = {
      /* x   y      R  G  B */
         0,  1,     1, 0, 0,
        -1, -1,     0, 1, 0,
         1, -1,     0, 0, 1,
    };

    vt->BindBuffer(GL_ARRAY_BUFFER, sys->vbo);
    vt->BufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
    vt->BindBuffer(GL_ARRAY_BUFFER, 0);

    filter->config.blend = true;

    static const struct vlc_gl_filter_ops ops = {
        .draw = DrawBlend,
        .close = Close,
    };
    filter->ops = &ops;

    return VLC_SUCCESS;
}

static int
InitMask(struct vlc_gl_filter *filter)
{
    struct sys *sys = filter->sys;
    const opengl_vtable_t *vt = &filter->api->vt;

    struct vlc_gl_sampler *sampler = vlc_gl_filter_GetSampler(filter);

    static const char *const VERTEX_SHADER_BODY =
        "attribute vec2 vertex_pos;\n"
        "uniform mat4 rotation_matrix;\n"
        "varying vec2 tex_coords;\n"
        "void main() {\n"
        "  vec4 pos = rotation_matrix * vec4(vertex_pos, 0.0, 1.0);\n"
        "  tex_coords = vec2((pos.x + 1.0) / 2.0,\n"
        "                    (pos.y + 1.0) / 2.0);\n"
        "  gl_Position = pos\n;"
        "}\n";

    static const char *const FRAGMENT_SHADER_BODY =
        "varying vec2 tex_coords;\n"
        "void main() {\n"
        "  gl_FragColor = vlc_texture(tex_coords);\n"
        "}\n";

    const char *extensions = sampler->shader.extensions
                           ? sampler->shader.extensions : "";

    const char *shader_version;
    const char *shader_precision;
    if (filter->api->is_gles)
    {
        shader_version = "#version 100\n";
        shader_precision = "precision highp float;\n";
    }
    else
    {
        shader_version = "#version 120\n";
        shader_precision = "";
    }

    const char *vertex_shader[] = {
        shader_version,
        VERTEX_SHADER_BODY,
    };
    const char *fragment_shader[] = {
        shader_version,
        extensions,
        shader_precision,
        sampler->shader.body,
        FRAGMENT_SHADER_BODY,
    };

    GLuint program_id =
        vlc_gl_BuildProgram(VLC_OBJECT(filter), vt,
                            ARRAY_SIZE(vertex_shader), vertex_shader,
                            ARRAY_SIZE(fragment_shader), fragment_shader);
    if (!program_id)
        return VLC_EGENERIC;

    sys->program_id = program_id;

    vlc_gl_sampler_FetchLocations(sampler, program_id);

    sys->loc.vertex_pos = vt->GetAttribLocation(sys->program_id, "vertex_pos");
    assert(sys->loc.vertex_pos != -1);

    sys->loc.rotation_matrix = vt->GetUniformLocation(sys->program_id,
                                                      "rotation_matrix");
    assert(sys->loc.rotation_matrix != -1);

    vt->GenBuffers(1, &sys->vbo);

    static const GLfloat data[] = {
      /* x   y */
         0,  1,
        -1, -1,
         1, -1,
    };

    vt->BindBuffer(GL_ARRAY_BUFFER, sys->vbo);
    vt->BufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
    vt->BindBuffer(GL_ARRAY_BUFFER, 0);

    filter->config.blend = false;

    static const struct vlc_gl_filter_ops ops = {
        .draw = DrawMask,
        .close = Close,
    };
    filter->ops = &ops;

    return VLC_SUCCESS;
}

static int
InitPlane(struct vlc_gl_filter *filter)
{
    struct sys *sys = filter->sys;
    const opengl_vtable_t *vt = &filter->api->vt;

    /* Must be initialized before calling vlc_gl_filter_GetSampler() */
    filter->config.filter_planes = true;

    struct vlc_gl_sampler *sampler = vlc_gl_filter_GetSampler(filter);

    static const char *const VERTEX_SHADER_BODY =
        "attribute vec2 vertex_pos;\n"
        "varying vec2 tex_coords;\n"
        "void main() {\n"
        "  gl_Position = vec4(vertex_pos, 0.0, 1.0);\n"
        "  tex_coords = vec2((vertex_pos.x + 1.0) / 2.0,\n"
        "                    (vertex_pos.y + 1.0) / 2.0);\n"
        "}\n";

    static const char *const FRAGMENT_SHADER_BODY =
        "varying vec2 tex_coords;\n"
        "uniform int plane;\n"
        "void main() {\n"
        "  vec2 offset = vec2(float(plane) * 0.02);\n"
        "  gl_FragColor = vlc_texture(fract(tex_coords + offset));\n"
        "}\n";

    const char *extensions = sampler->shader.extensions
                           ? sampler->shader.extensions : "";

    const char *shader_version;
    const char *shader_precision;
    if (filter->api->is_gles)
    {
        shader_version = "#version 100\n";
        shader_precision = "precision highp float;\n";
    }
    else
    {
        shader_version = "#version 120\n";
        shader_precision = "";
    }

    const char *vertex_shader[] = {
        shader_version,
        VERTEX_SHADER_BODY,
    };
    const char *fragment_shader[] = {
        shader_version,
        extensions,
        shader_precision,
        sampler->shader.body,
        FRAGMENT_SHADER_BODY,
    };

    GLuint program_id =
        vlc_gl_BuildProgram(VLC_OBJECT(filter), vt,
                            ARRAY_SIZE(vertex_shader), vertex_shader,
                            ARRAY_SIZE(fragment_shader), fragment_shader);
    if (!program_id)
        return VLC_EGENERIC;

    sys->program_id = program_id;

    vlc_gl_sampler_FetchLocations(sampler, program_id);

    sys->loc.vertex_pos = vt->GetAttribLocation(sys->program_id, "vertex_pos");
    assert(sys->loc.vertex_pos != -1);

    sys->loc.plane = vt->GetUniformLocation(program_id, "plane");
    assert(sys->loc.plane != -1);

    static const GLfloat vertex_pos[] = {
        -1,  1,
        -1, -1,
         1,  1,
         1, -1,
    };

    vt->BindBuffer(GL_ARRAY_BUFFER, sys->vbo);
    vt->BufferData(GL_ARRAY_BUFFER, sizeof(vertex_pos), vertex_pos,
                   GL_STATIC_DRAW);

    vt->BindBuffer(GL_ARRAY_BUFFER, 0);

    static const struct vlc_gl_filter_ops ops = {
        .draw = DrawPlane,
        .close = Close,
    };
    filter->ops = &ops;

    return VLC_SUCCESS;
}

static vlc_gl_filter_open_fn Open;
static int
Open(struct vlc_gl_filter *filter, const config_chain_t *config,
     struct vlc_gl_tex_size *size_out)
{
    (void) config;

    config_ChainParse(filter, MOCK_CFG_PREFIX, filter_options, config);

    bool mask = var_InheritBool(filter, MOCK_CFG_PREFIX "mask");
    bool plane = var_InheritBool(filter, MOCK_CFG_PREFIX "plane");
    float angle = var_InheritFloat(filter, MOCK_CFG_PREFIX "angle");
    float speed = var_InheritFloat(filter, MOCK_CFG_PREFIX "speed");
    int msaa = var_InheritInteger(filter, MOCK_CFG_PREFIX "msaa");

    struct sys *sys = filter->sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_EGENERIC;

    int ret;
    if (plane)
        ret = InitPlane(filter);
    else if (mask)
        ret = InitMask(filter);
    else
        ret = InitBlend(filter);

    if (ret != VLC_SUCCESS)
        goto error;

    sys->ar = (float) size_out->width / size_out->height;
    sys->theta0 = angle * M_PI / 180; /* angle in degrees, theta0 in radians */
    sys->speed = speed;

    /* MSAA is not supported for plane filters */
    filter->config.msaa_level = plane ? 0 : msaa;

    return VLC_SUCCESS;

error:
    free(sys);
    return VLC_EGENERIC;
}

vlc_module_begin()
    set_shortname("mock")
    set_description("mock OpenGL filter")
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_capability("opengl filter", 0)
    set_callback(Open)
    add_shortcut("mock");
    add_float(MOCK_CFG_PREFIX "angle", 0.f, NULL, NULL) /* in degrees */
    add_float(MOCK_CFG_PREFIX "speed", 0.f, NULL, NULL) /* in rotations per minute */
    add_bool(MOCK_CFG_PREFIX "mask", false, NULL, NULL)
    add_bool(MOCK_CFG_PREFIX "plane", false, NULL, NULL)
    add_integer(MOCK_CFG_PREFIX "msaa", 4, NULL, NULL);
vlc_module_end()
