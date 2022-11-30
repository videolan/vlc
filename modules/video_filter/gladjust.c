/*****************************************************************************
 * gladjust.c : Contrast/Hue/Saturation/Brightness GPU video filter
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

struct sys {
    struct vlc_gl_sampler *sampler;

    GLuint program_id;

    GLuint vbo;

    struct {
        GLint vertex_pos;
        GLint tex_coords_in;
        GLint contrast;
        GLint brightness;
        GLint hue;
        GLint saturation;
        GLint gamma;
        GLint brightness_threshold;
    } loc;
    _Atomic float contrast;
    _Atomic float brightness;
    _Atomic float hue;
    _Atomic float saturation;
    _Atomic float gamma;
    atomic_bool brightness_threshold;
};

static const char *const ppsz_filter_options[] = {
    "contrast", "brightness", "hue", "saturation", "gamma",
    "brightness-threshold", NULL
};

static int varFloatCallback(vlc_object_t *p_this, char const *psz_variable,
                            vlc_value_t oldvalue, vlc_value_t newvalue,
                            void *p_data)
{
    _Atomic float *atom = p_data;
    atomic_store_explicit(atom, newvalue.f_float, memory_order_relaxed);
    (void) p_this; (void) psz_variable; (void) oldvalue;
    return VLC_SUCCESS;
}

static int varBoolCallback(vlc_object_t *p_this, char const *psz_variable,
                           vlc_value_t oldvalue, vlc_value_t newvalue,
                           void *p_data)
{
    atomic_bool *atom = p_data;
    atomic_store_explicit(atom, newvalue.b_bool, memory_order_relaxed);
    (void) p_this; (void) psz_variable; (void) oldvalue;
    return VLC_SUCCESS;
}

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
    }

    const GLsizei stride = 4 * sizeof(float);

    vt->EnableVertexAttribArray(sys->loc.vertex_pos);
    vt->VertexAttribPointer(sys->loc.vertex_pos, 2, GL_FLOAT, GL_FALSE, stride,
                            (const void *) 0);

    intptr_t offset = 2 * sizeof(float);
    vt->EnableVertexAttribArray(sys->loc.tex_coords_in);
    vt->VertexAttribPointer(sys->loc.tex_coords_in, 2, GL_FLOAT, GL_FALSE,
                            stride, (const void *) offset);

    vt->Uniform1f(sys->loc.contrast,
                  atomic_load_explicit(&sys->contrast, memory_order_relaxed));
    vt->Uniform1f(sys->loc.brightness,
                  atomic_load_explicit(&sys->brightness, memory_order_relaxed) - 1.f);
    vt->Uniform1f(sys->loc.hue,
                  atomic_load_explicit(&sys->hue, memory_order_relaxed));
    vt->Uniform1f(sys->loc.saturation,
                  atomic_load_explicit(&sys->saturation, memory_order_relaxed));
    vt->Uniform1f(sys->loc.gamma,
                  1.f / atomic_load_explicit(&sys->gamma, memory_order_relaxed));
    vt->Uniform1i(sys->loc.brightness_threshold,
                  atomic_load_explicit(&sys->brightness_threshold, memory_order_relaxed));

    vt->Clear(GL_COLOR_BUFFER_BIT);
    vt->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    return VLC_SUCCESS;
}

static void
Close(struct vlc_gl_filter *filter) {
    struct sys *sys = filter->sys;

    const opengl_vtable_t *vt = &filter->api->vt;
    vt->DeleteProgram(sys->program_id);
    vt->DeleteBuffers(1, &sys->vbo);
    var_DelCallback( filter, "contrast", varFloatCallback, &sys->contrast );
    var_DelCallback( filter, "brightness", varFloatCallback, &sys->brightness );
    var_DelCallback( filter, "hue", varFloatCallback, &sys->hue );
    var_DelCallback( filter, "saturation", varFloatCallback, &sys->saturation );
    var_DelCallback( filter, "gamma", varFloatCallback, &sys->gamma );
    var_DelCallback( filter, "brightness-threshold", varBoolCallback,
                     &sys->brightness_threshold );

    vlc_gl_sampler_Delete(sys->sampler);
    free(sys);
}

static vlc_gl_filter_open_fn Open;
static int
Open(struct vlc_gl_filter *filter, const config_chain_t *config,
     const struct vlc_gl_format *glfmt, struct vlc_gl_tex_size *size_out)
{
    (void) config;
    (void) size_out;

    filter->config.filter_planes = false;
    filter->config.blend = false;
    filter->config.msaa_level = 0;

    struct sys *sys = malloc(sizeof(*sys));
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

    config_ChainParse(filter, "", ppsz_filter_options, config);

    atomic_init( &sys->contrast,
                 var_CreateGetFloatCommand( filter, "contrast" ) );
    atomic_init( &sys->brightness,
                 var_CreateGetFloatCommand( filter, "brightness" ) );
    atomic_init( &sys->hue, var_CreateGetFloatCommand( filter, "hue" ) );
    atomic_init( &sys->saturation,
                 var_CreateGetFloatCommand( filter, "saturation" ) );
    atomic_init( &sys->gamma,
                 var_CreateGetFloatCommand( filter, "gamma" ) );
    atomic_init( &sys->brightness_threshold,
                 var_CreateGetBoolCommand( filter, "brightness-threshold" ) );

    var_AddCallback( filter, "contrast", varFloatCallback,
                     &sys->contrast );
    var_AddCallback( filter, "brightness", varFloatCallback,
                     &sys->brightness );
    var_AddCallback( filter, "hue", varFloatCallback, &sys->hue );
    var_AddCallback( filter, "saturation", varFloatCallback,
                     &sys->saturation );
    var_AddCallback( filter, "gamma", varFloatCallback, &sys->gamma );
    var_AddCallback( filter, "brightness-threshold", varBoolCallback,
                     &sys->brightness_threshold );

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
        "uniform float contrast;\n"
        "uniform float brightness;\n"
        "uniform float hue;\n"
        "uniform float saturation;\n"
        "uniform float gamma;\n"
        "uniform bool brightness_threshold;\n"
        "\n"
        /* expects normalized rgb */
        "vec3 rgb_to_hsl(float r, float g, float b) {\n"
        "  float h = 0.0, s = 0.0, l = 0.0;\n"
        "  float cmin = min(min(r, g), b);\n"
        "  float cmax = max(max(r, g), b);\n"
        "  l = (cmin + cmax) / 2.0;\n"
        "  float diff = cmax - cmin;\n"
        "  if (diff < 0.001) {\n"
        "    return vec3(h, s, l);\n"
        "  }\n"
        "  s = diff / (1.0 - abs(2.0 * l - 1.0));\n"
        "  if (cmax == r) {\n"
        "    h = mod((g - b) / diff, 6.0);\n"
        "  }\n"
        "  else if (cmax == g) {\n"
        "    h = 2.0 + (b - r) / diff;\n"
        "  }\n"
        "  else {\n"
        "    h = 4.0 + (r - g) / diff;\n"
        "  }\n"
        "  h *= 60.0;\n"
        "  return vec3(h, s, l);\n"
        "}\n"
        "\n"
        "vec3 hsl_to_rgb(float h, float s, float l) {\n"
        "  vec3 res = vec3(0.0);\n"
        "  \n"
        "  float c = (1.0 - abs(2.0 * l - 1.0)) * s;\n"
        "  float x = c * (1.0 - abs(mod(h / 60.0, 2.0) - 1.0));\n"
        "  float m = l - c * 0.5;\n"
        "  \n"
        "  if (h < 60.0) {\n"
        "    res = vec3(c, x, 0.0);\n"
        "  }\n"
        "  else if (h < 120.0) {\n"
        "    res = vec3(x, c, 0.0);\n"
        "  }\n"
        "  else if (h < 180.0) {\n"
        "    res = vec3(0.0, c, x);\n"
        "  }\n"
        "  else if (h < 240.0) {\n"
        "    res = vec3(0.0, x, c);\n"
        "  }\n"
        "  else if (h < 300.0) {\n"
        "    res = vec3(x, 0.0, c);\n"
        "  }\n"
        "  else if (h < 360.0) {\n"
        "    res = vec3(c, 0.0, x);\n"
        "  }\n"
        "  return res + m;\n"
        "}\n"
        "\n"
        "void main() {\n"
        "  vec3 color = vlc_texture(tex_coords).rgb;\n"
        "  if (!brightness_threshold) {\n"
        "    color = rgb_to_hsl(color.r, color.g, color.b);\n"
        "    color = hsl_to_rgb(\n"
        "      mod(color.r - hue, 360.0),\n"
        "      clamp(color.g * saturation, 0.0, 1.0),\n"
        "      clamp(color.b, 0.0, 1.0)\n"
        "    );\n"
        "    color = pow(clamp(contrast * color + brightness + 0.5\n"
        "                      - contrast * 0.5, 0.0, 1.0), vec3(gamma));\n"
        "  } else {\n"
        "      color.r = color.r < brightness ? 0.0 : 1.0;\n"
        "      color.g = color.g < brightness ? 0.0 : 1.0;\n"
        "      color.b = color.b < brightness ? 0.0 : 1.0;\n"
        "  }\n"
        "  gl_FragColor = vec4(color, 0.0);\n"
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

    sys->loc.contrast = vt->GetUniformLocation(program_id, "contrast");
    assert(sys->loc.contrast != -1);

    sys->loc.brightness = vt->GetUniformLocation(program_id, "brightness");
    assert(sys->loc.brightness != -1);

    sys->loc.hue = vt->GetUniformLocation(program_id, "hue");
    assert(sys->loc.hue != -1);

    sys->loc.saturation = vt->GetUniformLocation(program_id, "saturation");
    assert(sys->loc.saturation != -1);

    sys->loc.gamma = vt->GetUniformLocation(program_id, "gamma");
    assert(sys->loc.gamma != -1);

    sys->loc.brightness_threshold = vt->GetUniformLocation(program_id, "brightness_threshold");
    assert(sys->loc.brightness_threshold != -1);

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


#define THRES_TEXT N_("Brightness threshold")
#define THRES_LONGTEXT N_("When this mode is enabled, pixels will be " \
        "shown as black or white. The threshold value will be the brightness " \
        "defined below." )
#define CONT_TEXT N_("Image contrast (0-2)")
#define CONT_LONGTEXT N_("Set the image contrast, between 0 and 2. Defaults to 1.")
#define HUE_TEXT N_("Image hue (-180..180)")
#define HUE_LONGTEXT N_("Set the image hue, between -180 and 180. Defaults to 0.")
#define SAT_TEXT N_("Image saturation (0-3)")
#define SAT_LONGTEXT N_("Set the image saturation, between 0 and 3. Defaults to 1.")
#define LUM_TEXT N_("Image brightness (0-2)")
#define LUM_LONGTEXT N_("Set the image brightness, between 0 and 2. Defaults to 1.")
#define GAMMA_TEXT N_("Image gamma (0-10)")
#define GAMMA_LONGTEXT N_("Set the image gamma, between 0.01 and 10. Defaults to 1.")

static int OpenVideoFilter(vlc_object_t *obj)
{
    filter_t *filter = (filter_t*)obj;

    module_t *module = vlc_gl_WrapOpenGLFilter(filter, "gladjust");
    return module ? VLC_SUCCESS : VLC_EGENERIC;
}

vlc_module_begin()
    set_shortname("gladjust")
    set_description("OpenGL Adjust Filter")
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_capability("video filter", 0)
    add_float_with_range( "contrast", 1.0, 0.0, 2.0, CONT_TEXT, CONT_LONGTEXT )
    add_float_with_range( "brightness", 1.0, 0.0, 2.0, LUM_TEXT, LUM_LONGTEXT )
    add_float_with_range( "hue", 0, -180., +180., HUE_TEXT, HUE_LONGTEXT )
    add_float_with_range( "saturation", 1.0, 0.0, 3.0, SAT_TEXT, SAT_LONGTEXT )
    add_float_with_range( "gamma", 1.0, 0.01, 10.0, GAMMA_TEXT, GAMMA_LONGTEXT )
    add_bool( "brightness-threshold", false, THRES_TEXT, THRES_LONGTEXT )
    set_callback(OpenVideoFilter)
    add_shortcut("gladjust")

    add_submodule()
        set_capability("opengl filter", 0)
        set_callback(Open)
        add_shortcut("gladjust")
vlc_module_end()
