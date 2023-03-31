/*****************************************************************************
 * filter_draw.c
 *****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 * Copyright (C) 2020 Videolabs
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
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_opengl.h>

#include "filter.h"
#include "gl_api.h"
#include "gl_common.h"
#include "gl_util.h"
#include "sampler.h"

#define DRAW_VFLIP_SHORTTEXT "VFlip the video"
#define DRAW_VFLIP_LONGTEXT \
    "Apply a vertical flip to the video"

#define DRAW_CFG_PREFIX "draw-"

static const char *const filter_options[] = { "vflip", NULL };

struct sys {
    struct vlc_gl_sampler *sampler;

    GLuint program_id;

    GLuint vbo;

    struct {
        GLint vertex_pos;
        GLint tex_coords_in;
    } loc;

    bool vflip;
};

static int
Draw(struct vlc_gl_filter *filter, const struct vlc_gl_picture *pic,
     const struct vlc_gl_input_meta *meta)
{
    (void) meta;

    struct sys *sys = filter->sys;

    const opengl_vtable_t *vt = &filter->api->vt;

    vt->UseProgram(sys->program_id);

    struct vlc_gl_sampler *sampler = sys->sampler;
    vlc_gl_sampler_Update(sampler, pic);
    vlc_gl_sampler_Load(sampler);

    vt->BindBuffer(GL_ARRAY_BUFFER, sys->vbo);

    if (pic->mtx_has_changed)
    {
        float coords[] = {
            0, sys->vflip ? 0 : 1,
            0, sys->vflip ? 1 : 0,
            1, sys->vflip ? 0 : 1,
            1, sys->vflip ? 1 : 0,
        };

        /* Transform coordinates in place */
        vlc_gl_picture_ToTexCoords(pic, 4, coords, coords);

        const float data[] = {
            -1,  1, coords[0], coords[1],
            -1, -1, coords[2], coords[3],
             1,  1, coords[4], coords[5],
             1, -1, coords[6], coords[7],
        };
        vt->BufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
    }

    const GLsizei stride = 4 * sizeof(float);

    vt->EnableVertexAttribArray(sys->loc.vertex_pos);
    vt->VertexAttribPointer(sys->loc.vertex_pos, 2, GL_FLOAT, GL_FALSE, stride,
                            (const void *) 0);

    intptr_t offset = 2 * sizeof(float);
    vt->EnableVertexAttribArray(sys->loc.tex_coords_in);
    vt->VertexAttribPointer(sys->loc.tex_coords_in, 2, GL_FLOAT, GL_FALSE,
                            stride, (const void *) offset);

    vt->Clear(GL_COLOR_BUFFER_BIT);
    vt->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    GL_ASSERT_NOERROR(vt);

    return VLC_SUCCESS;
}

static void
Close(struct vlc_gl_filter *filter)
{
    struct sys *sys = filter->sys;

    vlc_gl_sampler_Delete(sys->sampler);

    const opengl_vtable_t *vt = &filter->api->vt;
    vt->DeleteProgram(sys->program_id);
    vt->DeleteBuffers(1, &sys->vbo);

    free(sys);
}

static int
Open(struct vlc_gl_filter *filter, const config_chain_t *config,
     const struct vlc_gl_format *glfmt, struct vlc_gl_tex_size *size_out)
{
    (void) size_out;

    struct vlc_gl_sampler *sampler =
        vlc_gl_sampler_New(filter->gl, filter->api, glfmt, false);
    if (!sampler)
        return VLC_EGENERIC;

    struct sys *sys = filter->sys = malloc(sizeof(*sys));
    if (!sys)
    {
        vlc_gl_sampler_Delete(sampler);
        return VLC_EGENERIC;
    }

    sys->sampler = sampler;

    static const char *const VERTEX_SHADER_BODY =
        "attribute vec2 vertex_pos;\n"
        "attribute vec2 tex_coords_in;\n"
        "varying vec2 tex_coords;\n"
        "void main() {\n"
        "  gl_Position = vec4(vertex_pos, 0.0, 1.0);\n"
        "  tex_coords = tex_coords_in;\n"
        "}\n";

    static const char *const FRAGMENT_SHADER_BODY =
        "varying vec2 tex_coords;\n"
        "void main() {\n"
        "  gl_FragColor = vlc_texture(tex_coords);\n"
        "}\n";

    const char *extensions = sampler->shader.extensions
                           ? sampler->shader.extensions : "";

    const opengl_vtable_t *vt = &filter->api->vt;

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

    config_ChainParse(filter, DRAW_CFG_PREFIX, filter_options, config);
    sys->vflip = var_InheritBool(filter, DRAW_CFG_PREFIX "vflip");

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
        goto error;

    vlc_gl_sampler_FetchLocations(sampler, program_id);

    sys->program_id = program_id;

    sys->loc.vertex_pos = vt->GetAttribLocation(program_id, "vertex_pos");
    assert(sys->loc.vertex_pos != -1);

    sys->loc.tex_coords_in = vt->GetAttribLocation(program_id, "tex_coords_in");
    assert(sys->loc.tex_coords_in != -1);

    vt->GenBuffers(1, &sys->vbo);

    static const struct vlc_gl_filter_ops ops = {
        .draw = Draw,
        .close = Close,
    };
    filter->ops = &ops;

    return VLC_SUCCESS;

error:
    free(sys);
    return VLC_EGENERIC;
}

vlc_module_begin()
    add_shortcut("draw")
    set_shortname("draw")
    set_capability("opengl filter", 0)
    set_callback_opengl_filter(Open)
    /* Hide the option - this is just used as a hack and not meant for user config */
    set_subcategory(SUBCAT_HIDDEN)
    add_bool(DRAW_CFG_PREFIX "vflip", false,
             DRAW_VFLIP_SHORTTEXT, DRAW_VFLIP_LONGTEXT)
vlc_module_end()
