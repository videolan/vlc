/*****************************************************************************
 * glsharpen.c : Sharpen GPU video filter
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

#include <stdatomic.h>
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_opengl.h>
#include <vlc_filter.h>

#include "video_output/opengl/filter.h"
#include "video_output/opengl/gl_api.h"
#include "video_output/opengl/gl_common.h"
#include "video_output/opengl/gl_util.h"
#include "video_output/opengl/sampler.h"

#define FILTER_PREFIX "sharpen-"

typedef struct {
    struct vlc_gl_sampler *sampler;

    GLuint program_id;

    GLuint vbo;

    struct {
        GLint vertex_pos;
        GLint tex_coords_in;
        GLint sigma;
        GLint one_pixel_up;
        GLint one_pixel_right;
    } loc;
    _Atomic float sigma;

    /* Texture coordinates unit vectors */
    float u[2];
    float v[2];
} gl_filter_sys_t;

static const char *const ppsz_filter_options[] = {
    "sigma", NULL
};

static int OpenGLFilterSharpenSigmaVarCallback(vlc_object_t *p_this, char const *psz_variable,
                                               vlc_value_t oldvalue, vlc_value_t newvalue,
                                               void *p_data)
{
    _Atomic float *atom = p_data;
    atomic_store_explicit(atom, newvalue.f_float, memory_order_relaxed);
    (void) p_this; (void) psz_variable; (void) oldvalue;
    return VLC_SUCCESS;
}

static int VideoFilterSharpenSigmaVarCallback(vlc_object_t *p_this, char const *psz_variable,
                                              vlc_value_t oldvalue, vlc_value_t newvalue,
                                              void *p_data)
{
    struct vlc_gl_filter *filter = p_data;
    var_Set(filter, psz_variable, newvalue);
    (void) p_this; (void) psz_variable; (void) oldvalue;
    return VLC_SUCCESS;
}

static int
Draw(struct vlc_gl_filter *filter, const struct vlc_gl_picture *pic,
     const struct vlc_gl_input_meta *meta)
{
    (void) meta;

    gl_filter_sys_t *sys = filter->sys;

    const opengl_vtable_t *vt = &filter->api->vt;

    vt->UseProgram(sys->program_id);

    struct vlc_gl_sampler *sampler = sys->sampler;
    vlc_gl_sampler_Update(sampler, pic);
    vlc_gl_sampler_Load(sampler);

    vt->BindBuffer(GL_ARRAY_BUFFER, sys->vbo);

    if (pic->mtx_has_changed)
    {
        float coords[] = {
            0, 1,
            0, 0,
            1, 1,
            1, 0,
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

        /* Compute the (normalized) vector representing the unit vectors in
         * texture coordinates, to take any orientation/flip into account. */
        float direction[2*2];
        vlc_gl_picture_ComputeDirectionMatrix(pic, direction);
        sys->u[0] = direction[0];
        sys->u[1] = direction[1];
        sys->v[0] = direction[2];
        sys->v[1] = direction[3];
    }

    struct vlc_gl_format *glfmt = &sampler->glfmt;

    GLsizei width = glfmt->tex_widths[meta->plane];
    GLsizei height = glfmt->tex_heights[meta->plane];
    vt->Uniform2f(sys->loc.one_pixel_right, sys->u[0] / width,
                                            sys->u[1] / height);
    vt->Uniform2f(sys->loc.one_pixel_up, sys->v[0] / width,
                                         sys->v[1] / height);

    const GLsizei stride = 4 * sizeof(float);

    vt->EnableVertexAttribArray(sys->loc.vertex_pos);
    vt->VertexAttribPointer(sys->loc.vertex_pos, 2, GL_FLOAT, GL_FALSE, stride,
                            (const void *) 0);

    intptr_t offset = 2 * sizeof(float);
    vt->EnableVertexAttribArray(sys->loc.tex_coords_in);
    vt->VertexAttribPointer(sys->loc.tex_coords_in, 2, GL_FLOAT, GL_FALSE,
                            stride, (const void *) offset);

    vt->Uniform1f(sys->loc.sigma,
                  atomic_load_explicit(&sys->sigma, memory_order_relaxed));

    vt->Clear(GL_COLOR_BUFFER_BIT);
    vt->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    return VLC_SUCCESS;
}

static void
Close(struct vlc_gl_filter *filter) {
    gl_filter_sys_t *sys = filter->sys;

    vlc_gl_sampler_Delete(sys->sampler);

    const opengl_vtable_t *vt = &filter->api->vt;
    vt->DeleteProgram(sys->program_id);
    vt->DeleteBuffers(1, &sys->vbo);
    var_DelCallback(filter, FILTER_PREFIX "sigma", 
                    OpenGLFilterSharpenSigmaVarCallback,
                    &sys->sigma);

    vlc_gl_t *gl = (vlc_gl_t *)vlc_object_parent(filter);
    filter_t *video_filter = (filter_t *)vlc_object_parent(gl);
    if (var_Type(video_filter, FILTER_PREFIX "sigma"))
    {
        var_DelCallback( video_filter, FILTER_PREFIX "sigma",
                        VideoFilterSharpenSigmaVarCallback, filter );
    }
    free(sys);
}

static vlc_gl_filter_open_fn Open;
static int
Open(struct vlc_gl_filter *filter, const config_chain_t *config,
     const struct vlc_gl_format *glfmt, struct vlc_gl_tex_size *size_out)
{
    (void) size_out;

    filter->config.filter_planes = false;
    filter->config.blend = false;
    filter->config.msaa_level = 0;

    gl_filter_sys_t *sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_EGENERIC;

    struct vlc_gl_sampler *sampler =
        vlc_gl_sampler_New(filter->gl, filter->api, glfmt, false);
    if (!sampler)
    {
        free(sys);
        return VLC_EGENERIC;
    }

    sys->sampler = sampler;
    filter->sys = sys;

    config_ChainParse(filter, FILTER_PREFIX, ppsz_filter_options, config);

    atomic_init(&sys->sigma,
                var_CreateGetFloatCommand(filter, FILTER_PREFIX "sigma"));

    var_AddCallback(filter, FILTER_PREFIX "sigma", OpenGLFilterSharpenSigmaVarCallback,
                    &sys->sigma );

    vlc_gl_t *gl = (vlc_gl_t *)vlc_object_parent(filter);
    filter_t *video_filter = (filter_t *)vlc_object_parent(gl);
    if (var_Type(video_filter, FILTER_PREFIX "sigma"))
    {
        var_AddCallback( video_filter, FILTER_PREFIX "sigma",
                        VideoFilterSharpenSigmaVarCallback, filter );
    }
    
    static const char *const VERTEX_SHADER =
        "attribute vec2 vertex_pos;\n"
        "attribute vec2 tex_coords_in;\n"
        "varying vec2 tex_coords;\n"
        "void main() {\n"
        "  gl_Position = vec4(vertex_pos, 0.0, 1.0);\n"
        "  tex_coords = tex_coords_in;\n"
        "}\n";

    static const char *const FRAGMENT_SHADER =
        "varying vec2 tex_coords;\n"
        "uniform float sigma;\n"
        "uniform vec2 one_pixel_up;\n"
        "uniform vec2 one_pixel_right;\n"
        "void main() {\n"
        /*
         * Kernel:
         * -1 -1 -1
         * -1  8 -1
         * -1 -1 -1
         */
        "  vec3 color = vlc_texture(tex_coords).rgb;\n"
        "  vec3 pix = -(  vlc_texture(tex_coords + one_pixel_up - one_pixel_right).rgb\n"
        "               + vlc_texture(tex_coords                - one_pixel_right).rgb\n"
        "               + vlc_texture(tex_coords - one_pixel_up - one_pixel_right).rgb\n"
        "               + vlc_texture(tex_coords - one_pixel_up                  ).rgb\n"
        "               + vlc_texture(tex_coords - one_pixel_up + one_pixel_right).rgb\n"
        "               + vlc_texture(tex_coords                + one_pixel_right).rgb\n"
        "               + vlc_texture(tex_coords + one_pixel_up + one_pixel_right).rgb\n"
        "               + vlc_texture(tex_coords + one_pixel_up                  ).rgb)\n"
        "             + 8.0 * color;\n"
        "  gl_FragColor = vec4(clamp(color + sigma * pix, 0.0, 1.0), 1.0);\n"
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

    sys->loc.tex_coords_in = vt->GetAttribLocation(program_id, "tex_coords_in");
    assert(sys->loc.tex_coords_in != -1);

    sys->loc.sigma = vt->GetUniformLocation(program_id, "sigma");
    assert(sys->loc.sigma != -1);

    sys->loc.one_pixel_up = vt->GetUniformLocation(program_id, "one_pixel_up");
    assert(sys->loc.one_pixel_up != -1);

    sys->loc.one_pixel_right =
        vt->GetUniformLocation(program_id, "one_pixel_right");
    assert(sys->loc.one_pixel_right != -1);

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

#define SIG_TEXT N_("Sharpen strength (0-2)")
#define SIG_LONGTEXT N_("Set the Sharpen strength, between 0 and 2. Defaults to 0.05.")

static int OpenVideoFilter(filter_t *filter)
{
    config_ChainParse( filter, FILTER_PREFIX, ppsz_filter_options,
                   filter->p_cfg );

    var_CreateGetFloatCommand(filter, FILTER_PREFIX "sigma");

    module_t *module = vlc_gl_WrapOpenGLFilter(filter, "glsharpen");
    return module ? VLC_SUCCESS : VLC_EGENERIC;
}

vlc_module_begin()
    set_shortname("glsharpen")
    set_description("Sharpen opengl video filter")
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_capability("video filter", 0)
    add_float_with_range( FILTER_PREFIX "sigma", 0.05, 0.0, 2.0,
        SIG_TEXT, SIG_LONGTEXT )
    change_safe()
    add_shortcut("glsharpen")
    set_callback(OpenVideoFilter)
    add_submodule()
        set_capability("opengl filter", 0)
        set_callback(Open)
        add_shortcut("glsharpen")
vlc_module_end ()
