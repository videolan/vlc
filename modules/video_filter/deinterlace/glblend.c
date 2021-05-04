/*****************************************************************************
 * glblend.c
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_picture.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_opengl.h>
#include <vlc_filter.h>

#include "video_output/opengl/filter.h"
#include "video_output/opengl/gl_api.h"
#include "video_output/opengl/gl_common.h"
#include "video_output/opengl/gl_util.h"

struct sys {
    GLuint program_id;

    GLuint vbo;

    struct {
        GLint vertex_pos;
        GLint height;
    } loc;
};

static int
Draw(struct vlc_gl_filter *filter, const struct vlc_gl_input_meta *meta)
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

    GLsizei height = sampler->tex_heights[meta->plane];
    vt->Uniform1f(sys->loc.height, height);

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
Open(struct vlc_gl_filter *filter, const config_chain_t *config,
     struct vlc_gl_tex_size *size_out)
{
    (void) config;
    (void) size_out;

    struct sys *sys = filter->sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_EGENERIC;

    static const struct vlc_gl_filter_ops ops = {
        .draw = Draw,
        .close = Close,
    };
    filter->ops = &ops;
    filter->config.filter_planes = true;

    struct vlc_gl_sampler *sampler = vlc_gl_filter_GetSampler(filter);

    static const char *const VERTEX_SHADER =
        "attribute vec2 vertex_pos;\n"
        "varying vec2 tex_coords;\n"
        "varying vec2 tex_coords_up;\n"
        "uniform float height;\n"
        "void main() {\n"
        "  gl_Position = vec4(vertex_pos, 0.0, 1.0);\n"
        "  tex_coords = vec2((vertex_pos.x + 1.0) / 2.0,\n"
        "                    (vertex_pos.y + 1.0) / 2.0);\n"
        "  tex_coords_up = vec2(tex_coords.x,\n"
        "                       tex_coords.y + 1.0 / height);\n"
        "}\n";

    static const char *const FRAGMENT_SHADER =
        "varying vec2 tex_coords;\n"
        "varying vec2 tex_coords_up;\n"
        "void main() {\n"
        "  vec4 pix = vlc_texture(tex_coords);\n"
        "  vec4 pix_up = vlc_texture(tex_coords_up);\n"
        "  gl_FragColor = (pix + pix_up) / 2.0;\n"
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

    const char *extensions = sampler->shader.extensions
                           ? sampler->shader.extensions : "";

    const opengl_vtable_t *vt = &filter->api->vt;

    const char *vertex_shader[] = { shader_version, VERTEX_SHADER };
    const char *fragment_shader[] = {
        shader_version,
        extensions,
        shader_precision,
        sampler->shader.body,
        FRAGMENT_SHADER,
    };

    GLuint program_id =
        vlc_gl_BuildProgram(VLC_OBJECT(filter), vt,
                            ARRAY_SIZE(vertex_shader), vertex_shader,
                            ARRAY_SIZE(fragment_shader), fragment_shader);

    if (!program_id)
        goto error;

    vlc_gl_sampler_FetchLocations(sampler, program_id);

    sys->program_id = program_id;

    sys->loc.vertex_pos = vt->GetAttribLocation(program_id, "vertex_pos");
    assert(sys->loc.vertex_pos != -1);

    sys->loc.height = vt->GetUniformLocation(program_id, "height");
    assert(sys->loc.height != -1);

    vt->GenBuffers(1, &sys->vbo);

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

    return VLC_SUCCESS;

error:
    free(sys);
    return VLC_EGENERIC;
}

static int OpenVideoFilter(vlc_object_t *obj)
{
    filter_t *filter = (filter_t*)obj;

    char *mode = var_InheritString(obj, "deinterlace-mode");
    bool is_supported = !mode
        || !strcmp(mode, "auto")
        || !strcmp(mode, "blend");
    free(mode);

    if (!is_supported)
        return VLC_EGENERIC;

    const config_chain_t *prev_chain = filter->p_cfg;
    var_Create(filter, "opengl-filter", VLC_VAR_STRING);
    var_SetString(filter, "opengl-filter", "glblend");

    filter->p_cfg = NULL;
    module_t *module = module_need(obj, "video filter", "opengl", true);
    filter->p_cfg = prev_chain;

    var_Destroy(filter, "opengl-filter");

    if (module == NULL)
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname("blend")
    set_description("OpenGL blend deinterlace filter")
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)

    set_capability("video filter", 0)
    set_callback(OpenVideoFilter)
    add_shortcut("glblend")

    add_submodule()
        set_capability("opengl filter", 0)
        set_callback(Open)
        add_shortcut("glblend")
vlc_module_end()
