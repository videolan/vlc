/*****************************************************************************
 * glsk.c : Sharpen/Contrast/Hue/Saturation/Gamma/Brightness GPU video filter for SK
 *****************************************************************************
 * Copyright (C) 2022 Videolabs
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
#include <vlc_filter.h>
#include <vlc_common.h>
#include <vlc_opengl.h>

#include "video_output/opengl/gl_common.h"
#include "video_output/opengl/filter.h"

#ifndef MODULE_STRING
#define MODULE_STRING "glsk"
#endif

typedef struct {
    struct vlc_gl_sampler *sampler;

    GLuint program_id;

    GLuint vbo;

    struct {
        GLint vertex_pos;
        GLint tex_coords_in;
    } loc;

    struct {
        struct {
            GLint enabled;
            GLint contrast;
            GLint brightness;
            GLint hue;
            GLint saturation;
            GLint gamma;
        } loc;
        _Atomic(bool)  enabled;
        _Atomic(float) contrast;
        _Atomic(float) brightness;
        _Atomic(float) hue;
        _Atomic(float) saturation;
        _Atomic(float) gamma;
    } adjust;
    
    struct {
        struct {
            GLint enabled;
            GLint sigma;
            GLint fast;
            GLint masked;
            GLint mask_contrast;
            GLint mask_brightness;
            GLint one_pixel_up;
            GLint one_pixel_right;
        } loc;
        _Atomic(bool)  enabled;
        _Atomic(float) sigma;
        _Atomic(bool)  fast;
        _Atomic(bool)  masked;
        _Atomic(float) mask_contrast;
        _Atomic(float) mask_brightness;
        /* Texture coordinates unit vectors */
        float u[2];
        float v[2];
    } sharpen;
    
} glsk_filter_sys_t;

static const char *const ppsz_filter_options[] = {
    "adjust", "contrast", "brightness", "hue", "saturation", "gamma",
    "sharpen", "sharpen-sigma", "sharpen-fast", "sharpen-mask", "sharpen-mask-contrast", "sharpen-mask-brightness",
    NULL
};

static int OpenGLFilterFloatVarCallback(vlc_object_t *p_this, char const *psz_variable,
                                               vlc_value_t oldvalue, vlc_value_t newvalue,
                                               void *p_data)
{
    _Atomic(float) *atom = p_data;
    atomic_store_explicit(atom, newvalue.f_float, memory_order_relaxed);
    (void) p_this; (void) psz_variable; (void) oldvalue;
    return VLC_SUCCESS;
}

static int OpenGLFilterBoolVarCallback(vlc_object_t *p_this, char const *psz_variable,
                                               vlc_value_t oldvalue, vlc_value_t newvalue,
                                               void *p_data)
{
    _Atomic(bool) *atom = p_data;
    atomic_store_explicit(atom, newvalue.b_bool, memory_order_relaxed);
    (void) p_this; (void) psz_variable; (void) oldvalue;
    return VLC_SUCCESS;
}

static int VideoFilterForwardVarCallback(vlc_object_t *p_this, char const *psz_variable,
                                              vlc_value_t oldvalue, vlc_value_t newvalue,
                                              void *p_data)
{
    struct vlc_gl_filter *filter = p_data;
    var_Set(filter, psz_variable, newvalue);
    (void) p_this; (void) oldvalue;
    return VLC_SUCCESS;
}

static int
Draw(struct vlc_gl_filter *filter, const struct vlc_gl_picture *pic,
     const struct vlc_gl_input_meta *meta)
{
    (void) meta;

    glsk_filter_sys_t *sys = filter->sys;

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
        sys->sharpen.u[0] = direction[0];
        sys->sharpen.u[1] = direction[1];
        sys->sharpen.v[0] = direction[2];
        sys->sharpen.v[1] = direction[3];
    }

    /** Vertex shader attributes */
    const GLsizei stride = 4 * sizeof(float);

    vt->EnableVertexAttribArray(sys->loc.vertex_pos);
    vt->VertexAttribPointer(sys->loc.vertex_pos, 2, GL_FLOAT, GL_FALSE, stride,
                            (const void *) 0);

    intptr_t offset = 2 * sizeof(float);
    vt->EnableVertexAttribArray(sys->loc.tex_coords_in);
    vt->VertexAttribPointer(sys->loc.tex_coords_in, 2, GL_FLOAT, GL_FALSE,
                            stride, (const void *) offset);
    
    /** Adjust shader uniforms*/
    vt->Uniform1i(sys->adjust.loc.enabled,
                  atomic_load_explicit(&sys->adjust.enabled, memory_order_relaxed));
    vt->Uniform1f(sys->adjust.loc.contrast,
                  atomic_load_explicit(&sys->adjust.contrast, memory_order_relaxed));
    vt->Uniform1f(sys->adjust.loc.brightness,
                  atomic_load_explicit(&sys->adjust.brightness, memory_order_relaxed) - 1.f);
    vt->Uniform1f(sys->adjust.loc.hue,
                  atomic_load_explicit(&sys->adjust.hue, memory_order_relaxed));
    vt->Uniform1f(sys->adjust.loc.saturation,
                  atomic_load_explicit(&sys->adjust.saturation, memory_order_relaxed));
    vt->Uniform1f(sys->adjust.loc.gamma,
                  1.f / atomic_load_explicit(&sys->adjust.gamma, memory_order_relaxed));

    /** Sharpen shader uniforms */
    struct vlc_gl_format *glfmt = &sampler->glfmt;

    GLsizei width = glfmt->tex_widths[meta->plane];
    GLsizei height = glfmt->tex_heights[meta->plane];

    vt->Uniform1i(sys->sharpen.loc.enabled,
                  atomic_load_explicit(&sys->sharpen.enabled, memory_order_relaxed));
    vt->Uniform1i(sys->sharpen.loc.fast,
                  atomic_load_explicit(&sys->sharpen.fast, memory_order_relaxed));              
    vt->Uniform1f(sys->sharpen.loc.sigma,
                  atomic_load_explicit(&sys->sharpen.sigma, memory_order_relaxed));
    vt->Uniform1i(sys->sharpen.loc.masked,
                  atomic_load_explicit(&sys->sharpen.masked, memory_order_relaxed));
    vt->Uniform1f(sys->sharpen.loc.mask_contrast,
                  atomic_load_explicit(&sys->sharpen.mask_contrast, memory_order_relaxed));
    vt->Uniform1f(sys->sharpen.loc.mask_brightness,
                  atomic_load_explicit(&sys->sharpen.mask_brightness, memory_order_relaxed));
    vt->Uniform2f(sys->sharpen.loc.one_pixel_right, sys->sharpen.u[0] / width,
                                                    sys->sharpen.u[1] / height);
    vt->Uniform2f(sys->sharpen.loc.one_pixel_up, sys->sharpen.v[0] / width,
                                                 sys->sharpen.v[1] / height);

    vt->Clear(GL_COLOR_BUFFER_BIT);
    vt->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    return VLC_SUCCESS;
}

static void
Close(struct vlc_gl_filter *filter) {
    
}

// static vlc_gl_filter_open_fn Open;
static int
Open(struct vlc_gl_filter *filter, const config_chain_t *config,
     const struct vlc_gl_format *glfmt, struct vlc_gl_tex_size *size_out)
{
    filter->config.filter_planes = false;
    filter->config.blend = false;
    filter->config.msaa_level = 0;

    glsk_filter_sys_t *sys = malloc(sizeof(*sys));
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

    atomic_init( &sys->adjust.enabled,
                 var_CreateGetBoolCommand( filter, "adjust" ) );
    atomic_init( &sys->adjust.contrast,
                 var_CreateGetFloatCommand( filter, "contrast" ) );
    atomic_init( &sys->adjust.brightness,
                 var_CreateGetFloatCommand( filter, "brightness" ) );
    atomic_init( &sys->adjust.hue, 
                 var_CreateGetFloatCommand( filter, "hue" ) );
    atomic_init( &sys->adjust.saturation,
                 var_CreateGetFloatCommand( filter, "saturation" ) );
    atomic_init( &sys->adjust.gamma,
                 var_CreateGetFloatCommand( filter, "gamma" ) );
    atomic_init(&sys->sharpen.enabled,
                var_CreateGetBoolCommand(filter, "sharpen"));
    atomic_init(&sys->sharpen.fast,
                var_GetBool(filter, "sharpen-fast"));
    atomic_init(&sys->sharpen.sigma,
                var_CreateGetFloatCommand(filter, "sharpen-sigma"));
    atomic_init(&sys->sharpen.masked,
                var_GetBool(filter, "sharpen-mask"));
    atomic_init(&sys->sharpen.mask_contrast,
                var_CreateGetFloatCommand(filter, "sharpen-mask-contrast"));
    atomic_init(&sys->sharpen.mask_brightness,
                var_CreateGetFloatCommand(filter, "sharpen-mask-brightness"));

    var_AddCallback(filter, "adjust", OpenGLFilterBoolVarCallback,
                        &sys->adjust.enabled );
    var_AddCallback( filter, "contrast", OpenGLFilterFloatVarCallback,
                     &sys->adjust.contrast );
    var_AddCallback( filter, "brightness", OpenGLFilterFloatVarCallback,
                     &sys->adjust.brightness );
    var_AddCallback( filter, "hue", OpenGLFilterFloatVarCallback, &sys->adjust.hue );
    var_AddCallback( filter, "saturation", OpenGLFilterFloatVarCallback,
                     &sys->adjust.saturation );
    var_AddCallback( filter, "gamma", OpenGLFilterFloatVarCallback, &sys->adjust.gamma );
    var_AddCallback(filter, "sharpen", OpenGLFilterBoolVarCallback,
                        &sys->sharpen.enabled );
    var_AddCallback(filter, "sharpen-sigma", OpenGLFilterFloatVarCallback,
                    &sys->sharpen.sigma );
    var_AddCallback(filter, "sharpen-mask", OpenGLFilterBoolVarCallback,
                        &sys->sharpen.masked );
    var_AddCallback(filter, "sharpen-mask-contrast", OpenGLFilterFloatVarCallback,
                    &sys->sharpen.mask_contrast );
    var_AddCallback(filter, "sharpen-mask-brightness", OpenGLFilterFloatVarCallback,
                    &sys->sharpen.mask_brightness );

    vlc_gl_t *gl = (vlc_gl_t *)vlc_object_parent(filter);
    filter_t *video_filter = (filter_t *)vlc_object_parent(gl);
    for( size_t i = 0; ppsz_filter_options[i] != NULL; i++ )
    {
        var_AddCallback( video_filter, ppsz_filter_options[i],
                        VideoFilterForwardVarCallback, filter );
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
        "uniform bool adjust;\n"
        "uniform float contrast;\n"
        "uniform float brightness;\n"
        "uniform float hue;\n"
        "uniform float saturation;\n"
        "uniform float gamma;\n"
        "uniform bool sharpen;\n"
        "uniform bool sharpen_fast;\n"
        "uniform float sigma;\n"
        "uniform bool sharpen_masked;\n"
        "uniform float sharpen_mask_brightness;\n"
        "uniform float sharpen_mask_contrast;\n"
        "uniform vec2 one_pixel_up;\n"
        "uniform vec2 one_pixel_right;\n"
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
        "float avg(vec3 v) { return (v.x + v.y + v.z) / 3.0; }\n"
        "\n"
        "void main() {\n"
        "  vec3 color = vlc_texture(tex_coords).rgb;\n"
        "  float sharpen_sigma = sigma;\n"
        "  if (sharpen) {\n"
        "    vec3 pix;"
        "    if (sharpen_fast) {\n"
        "       sharpen_sigma *= 2.0;\n"
        "       pix = -(  vlc_texture(tex_coords                - one_pixel_right).rgb\n"
        "               + vlc_texture(tex_coords - one_pixel_up                  ).rgb\n"
        "               + vlc_texture(tex_coords                + one_pixel_right).rgb\n"
        "               + vlc_texture(tex_coords + one_pixel_up                  ).rgb)\n"
        "               + 4.0 * color;\n"
        "       } else {\n"
        "       pix = -(  vlc_texture(tex_coords + one_pixel_up - one_pixel_right).rgb\n"
        "               + vlc_texture(tex_coords                - one_pixel_right).rgb\n"
        "               + vlc_texture(tex_coords - one_pixel_up - one_pixel_right).rgb\n"
        "               + vlc_texture(tex_coords - one_pixel_up                  ).rgb\n"
        "               + vlc_texture(tex_coords - one_pixel_up + one_pixel_right).rgb\n"
        "               + vlc_texture(tex_coords                + one_pixel_right).rgb\n"
        "               + vlc_texture(tex_coords + one_pixel_up + one_pixel_right).rgb\n"
        "               + vlc_texture(tex_coords + one_pixel_up                  ).rgb)\n"
        "               + 8.0 * color;\n"
        "    }\n"
        "    vec3 mask = vec3(1.0);\n"
        "    if (sharpen_masked) {\n"
        "      float mask_weight = 4.0;\n"
        "      float mask_offset = 4.0;\n"
        "      float src = avg(color);\n"
        "      float src_sx = avg(vlc_texture(tex_coords + one_pixel_right * mask_offset).rgb);\n"
        "      float src_sy = avg(vlc_texture(tex_coords + one_pixel_up * mask_offset).rgb);\n"
        "      vec2 edges = vec2(src - src_sx, src - src_sy);\n"
        "      float mag = length(edges);\n"
        "      float value = (mag * mask_weight / src) + (sharpen_mask_brightness - 1.0);\n"
        "      mask = vec3(clamp((value - 0.5) * sharpen_mask_contrast + 0.5, 0.0, 1.0));\n"
        "    }\n"
        "    color = clamp(color + sharpen_sigma * pix * mask, 0.0, 1.0);\n"
        "  }\n"
        "  if (adjust) {\n"
        "    color = rgb_to_hsl(color.r, color.g, color.b);\n"
        "    color = hsl_to_rgb(\n"
        "      mod(color.r - hue, 360.0),\n"
        "      clamp(color.g * saturation, 0.0, 1.0),\n"
        "      clamp(color.b, 0.0, 1.0)\n"
        "    );\n"
        "    color = pow(clamp(contrast * color + brightness + 0.5\n"
        "                      - contrast * 0.5, 0.0, 1.0), vec3(gamma));\n"
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
        FRAGMENT_SHADER
    };

    GLuint program_id =
        vlc_gl_BuildProgram(VLC_OBJECT(filter), vt,
                            ARRAY_SIZE(vertex_shader), vertex_shader,
                            ARRAY_SIZE(fragment_shader), fragment_shader);

    if (!program_id)
        goto error;

    vlc_gl_sampler_FetchLocations(sampler, program_id);

    sys->program_id = program_id;

    /** Global attributes */
    sys->loc.vertex_pos = vt->GetAttribLocation(program_id, "vertex_pos");
    assert(sys->loc.vertex_pos != -1);

    sys->loc.tex_coords_in = vt->GetAttribLocation(program_id, "tex_coords_in");
    assert(sys->loc.tex_coords_in != -1);

    /** Adjust uniforms */
    sys->adjust.loc.enabled = vt->GetUniformLocation(program_id, "adjust");
    assert(sys->adjust.loc.enabled != -1);

    sys->adjust.loc.contrast = vt->GetUniformLocation(program_id, "contrast");
    assert(sys->adjust.loc.contrast != -1);

    sys->adjust.loc.brightness = vt->GetUniformLocation(program_id, "brightness");
    assert(sys->adjust.loc.brightness != -1);

    sys->adjust.loc.hue = vt->GetUniformLocation(program_id, "hue");
    assert(sys->adjust.loc.hue != -1);

    sys->adjust.loc.saturation = vt->GetUniformLocation(program_id, "saturation");
    assert(sys->adjust.loc.saturation != -1);

    sys->adjust.loc.gamma = vt->GetUniformLocation(program_id, "gamma");
    assert(sys->adjust.loc.gamma != -1);

    /** Sharpen uniforms */
    sys->sharpen.loc.enabled = vt->GetUniformLocation(program_id, "sharpen");
    assert(sys->sharpen.loc.enabled != -1);

    sys->sharpen.loc.fast = vt->GetUniformLocation(program_id, "sharpen_fast");
    assert(sys->sharpen.loc.fast != -1);

    sys->sharpen.loc.sigma = vt->GetUniformLocation(program_id, "sigma");
    assert(sys->sharpen.loc.sigma != -1);
    
    sys->sharpen.loc.masked = vt->GetUniformLocation(program_id, "sharpen_masked");
    assert(sys->sharpen.loc.masked != -1);

    sys->sharpen.loc.mask_contrast = vt->GetUniformLocation(program_id, "sharpen_mask_contrast");
    assert(sys->sharpen.loc.mask_contrast != -1);
    
    sys->sharpen.loc.mask_brightness = vt->GetUniformLocation(program_id, "sharpen_mask_brightness");
    assert(sys->sharpen.loc.mask_brightness != -1);

    sys->sharpen.loc.one_pixel_up = vt->GetUniformLocation(program_id, "one_pixel_up");
    assert(sys->sharpen.loc.one_pixel_up != -1);

    sys->sharpen.loc.one_pixel_right =
        vt->GetUniformLocation(program_id, "one_pixel_right");
    assert(sys->sharpen.loc.one_pixel_right != -1);

    vt->GenBuffers(1, &sys->vbo);

    // var_GetBool(filter, FILTER_PREFIX "fast") ? 
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

static int OpenVideoFilter(filter_t *filter)
{
    config_ChainParse( filter, "", ppsz_filter_options,
                   filter->p_cfg );

    var_CreateGetBoolCommand( filter, "adjust" );
    var_CreateGetFloatCommand( filter, "contrast" );
    var_CreateGetFloatCommand( filter, "brightness" );
    var_CreateGetFloatCommand( filter, "hue" );
    var_CreateGetFloatCommand( filter, "saturation" );
    var_CreateGetFloatCommand( filter, "gamma" );

    var_CreateGetBoolCommand( filter, "sharpen" );
    var_CreateGetFloatCommand( filter, "sharpen-sigma" );
    var_CreateGetBoolCommand( filter, "sharpen-mask" );
    var_CreateGetFloatCommand( filter, "sharpen-mask-brightness" );
    var_CreateGetFloatCommand( filter, "sharpen-mask-contrast" );

    module_t *module = vlc_gl_WrapOpenGLFilter(filter, "glsk");
    return module ? VLC_SUCCESS : VLC_EGENERIC;
}

#define ADJUST_TEXT N_("Enable adjust GPU video filter")
#define ADJUST_LONGTEXT N_("Enable Contrast/Hue/Saturation/Brightness/Gamma GL/GLES video filter. Defaults to false.")
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

#define SHARPEN_TEXT N_("Enable sharpen GPU video filter")
#define SHARPEN_LONGTEXT N_("Enable sharpen GL/GLES video filter. Defaults to false.")
#define SHARPEN_SIGMA_TEXT N_("Sharpen strength (0-2)")
#define SHARPEN_SIGMA_LONGTEXT N_("Set the Sharpen strength, between 0 and 2. Defaults to 0.05.")
#define SHARPEN_FAST_TEXT N_("Faster sharpen kernel algorithm")
#define SHARPEN_FAST_LONGTEXT N_("Enable a faster but less precise sharpen kernel. Defaults to false.")

vlc_module_begin()
    set_shortname("glsk")
    set_description("OpenGL Filters for SK")
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_capability("video filter", 0)
    
    add_bool( "adjust", false, ADJUST_TEXT, ADJUST_LONGTEXT )
    add_float_with_range( "contrast", 1.0, 0.0, 2.0, CONT_TEXT, CONT_LONGTEXT )
    add_float_with_range( "brightness", 1.0, 0.0, 2.0, LUM_TEXT, LUM_LONGTEXT )
    add_float_with_range( "hue", 0, -180., +180., HUE_TEXT, HUE_LONGTEXT )
    add_float_with_range( "saturation", 1.0, 0.0, 3.0, SAT_TEXT, SAT_LONGTEXT )
    add_float_with_range( "gamma", 1.0, 0.01, 10.0, GAMMA_TEXT, GAMMA_LONGTEXT )

    add_bool( "sharpen", false, SHARPEN_TEXT, SHARPEN_LONGTEXT )
    add_float_with_range( "sharpen-sigma", 0.05, 0.0, 2.0, SHARPEN_SIGMA_TEXT, SHARPEN_SIGMA_LONGTEXT )
    add_bool( "sharpen-fast", false, SHARPEN_FAST_TEXT, SHARPEN_FAST_LONGTEXT )
    add_bool( "sharpen-mask", true, "", "" )
    add_float_with_range( "sharpen-mask-contrast", 1.0, 0.0, 2.0, "", "" )
    add_float_with_range( "sharpen-mask-brightness", 1.0, 0.0, 2.0, "", "" )
    
    set_callback(OpenVideoFilter)
    add_shortcut("glsk")

    add_submodule()
        set_capability("opengl filter", 0)
        set_callback(Open)
        add_shortcut("glsk")
vlc_module_end()
