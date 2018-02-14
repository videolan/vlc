/*****************************************************************************
 * fragment_shaders.c: OpenGL fragment shaders
 *****************************************************************************
 * Copyright (C) 2016,2017 VLC authors and VideoLAN
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

#include <assert.h>
#include <stdlib.h>

#ifdef HAVE_LIBPLACEBO
#include <libplacebo/shaders.h>
#include <libplacebo/shaders/colorspace.h>
#endif

#include <vlc_common.h>
#include <vlc_memstream.h>
#include "internal.h"
#include "vout_helper.h"

#ifndef GL_RED
# define GL_RED 0x1903
#endif
#ifndef GL_RG
# define GL_RG 0x8227
#endif
#ifndef GL_R16
# define GL_R16 0x822A
#endif
#ifndef GL_BGRA
# define GL_BGRA 0x80E1
#endif
#ifndef GL_RG16
# define GL_RG16 0x822C
#endif
#ifndef GL_LUMINANCE16
# define GL_LUMINANCE16 0x8042
#endif
#ifndef GL_TEXTURE_RED_SIZE
# define GL_TEXTURE_RED_SIZE 0x805C
#endif
#ifndef GL_TEXTURE_LUMINANCE_SIZE
# define GL_TEXTURE_LUMINANCE_SIZE 0x8060
#endif

static int GetTexFormatSize(opengl_tex_converter_t *tc, int target,
                            int tex_format, int tex_internal, int tex_type)
{
    if (!tc->vt->GetTexLevelParameteriv)
        return -1;

    GLint tex_param_size;
    int mul = 1;
    switch (tex_format)
    {
        case GL_BGRA:
            mul = 4;
            /* fall through */
        case GL_RED:
        case GL_RG:
            tex_param_size = GL_TEXTURE_RED_SIZE;
            break;
        case GL_LUMINANCE:
            tex_param_size = GL_TEXTURE_LUMINANCE_SIZE;
            break;
        default:
            return -1;
    }
    GLuint texture;

    tc->vt->GenTextures(1, &texture);
    tc->vt->BindTexture(target, texture);
    tc->vt->TexImage2D(target, 0, tex_internal, 64, 64, 0, tex_format, tex_type, NULL);
    GLint size = 0;
    tc->vt->GetTexLevelParameteriv(target, 0, tex_param_size, &size);

    tc->vt->DeleteTextures(1, &texture);
    return size > 0 ? size * mul : size;
}

static int
tc_yuv_base_init(opengl_tex_converter_t *tc, GLenum tex_target,
                 vlc_fourcc_t chroma, const vlc_chroma_description_t *desc,
                 video_color_space_t yuv_space,
                 bool *swap_uv, const char *swizzle_per_tex[])
{
    GLint oneplane_texfmt, oneplane16_texfmt,
          twoplanes_texfmt, twoplanes16_texfmt;

    if (HasExtension(tc->glexts, "GL_ARB_texture_rg"))
    {
        oneplane_texfmt = GL_RED;
        oneplane16_texfmt = GL_R16;
        twoplanes_texfmt = GL_RG;
        twoplanes16_texfmt = GL_RG16;
    }
    else
    {
        oneplane_texfmt = GL_LUMINANCE;
        oneplane16_texfmt = GL_LUMINANCE16;
        twoplanes_texfmt = GL_LUMINANCE_ALPHA;
        twoplanes16_texfmt = 0;
    }

    float yuv_range_correction = 1.0;
    if (desc->pixel_size == 2)
    {
        if (GetTexFormatSize(tc, tex_target, oneplane_texfmt,
                             oneplane16_texfmt, GL_UNSIGNED_SHORT) != 16)
            return VLC_EGENERIC;

        /* Do a bit shift if samples are stored on LSB */
        /* This is a hackish way to detect endianness. FIXME: Add bit order
         * in vlc_chroma_description_t */
        if ((chroma >> 24) == 'L')
            yuv_range_correction = (float)((1 << 16) - 1)
                                 / ((1 << desc->pixel_bits) - 1);
    }

    if (desc->plane_count == 3)
    {
        GLint internal = 0;
        GLenum type = 0;

        if (desc->pixel_size == 1)
        {
            internal = oneplane_texfmt;
            type = GL_UNSIGNED_BYTE;
        }
        else if (desc->pixel_size == 2)
        {
            internal = oneplane16_texfmt;
            type = GL_UNSIGNED_SHORT;
        }
        else
            return VLC_EGENERIC;

        assert(internal != 0 && type != 0);

        tc->tex_count = 3;
        for (unsigned i = 0; i < tc->tex_count; ++i )
        {
            tc->texs[i] = (struct opengl_tex_cfg) {
                { desc->p[i].w.num, desc->p[i].w.den },
                { desc->p[i].h.num, desc->p[i].h.den },
                internal, oneplane_texfmt, type
            };
        }

        if (oneplane_texfmt == GL_RED)
            swizzle_per_tex[0] = swizzle_per_tex[1] = swizzle_per_tex[2] = "r";
    }
    else if (desc->plane_count == 2)
    {
        tc->tex_count = 2;

        if (desc->pixel_size == 1)
        {
            tc->texs[0] = (struct opengl_tex_cfg) {
                { 1, 1 }, { 1, 1 }, oneplane_texfmt, oneplane_texfmt,
                GL_UNSIGNED_BYTE
            };
            tc->texs[1] = (struct opengl_tex_cfg) {
                { 1, 2 }, { 1, 4 }, twoplanes_texfmt, twoplanes_texfmt,
                GL_UNSIGNED_BYTE
            };
        }
        else if (desc->pixel_size == 2)
        {
            if (twoplanes16_texfmt == 0
             || GetTexFormatSize(tc, tex_target, twoplanes_texfmt,
                                 twoplanes16_texfmt, GL_UNSIGNED_SHORT) != 16)
                return VLC_EGENERIC;
            tc->texs[0] = (struct opengl_tex_cfg) {
                { 1, 1 }, { 1, 1 }, oneplane16_texfmt, oneplane_texfmt,
                GL_UNSIGNED_SHORT
            };
            tc->texs[1] = (struct opengl_tex_cfg) {
                { 1, 2 }, { 1, 4 }, twoplanes16_texfmt, twoplanes_texfmt,
                GL_UNSIGNED_SHORT
            };
        }
        else
            return VLC_EGENERIC;

        if (oneplane_texfmt == GL_RED)
        {
            swizzle_per_tex[0] = "r";
            swizzle_per_tex[1] = "rg";
        }
        else
        {
            swizzle_per_tex[0] = NULL;
            swizzle_per_tex[1] = "xa";
        }
    }
    else if (desc->plane_count == 1)
    {
        /* Y1 U Y2 V fits in R G B A */
        tc->tex_count = 1;
        tc->texs[0] = (struct opengl_tex_cfg) {
            { 1, 2 }, { 1, 2 }, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE
        };

        /*
         * Set swizzling in Y1 U V order
         * R  G  B  A
         * U  Y1 V  Y2 => GRB
         * Y1 U  Y2 V  => RGA
         * V  Y1 U  Y2 => GBR
         * Y1 V  Y2 U  => RAG
         */
        switch (chroma)
        {
            case VLC_CODEC_UYVY:
                swizzle_per_tex[0] = "grb";
                break;
            case VLC_CODEC_YUYV:
                swizzle_per_tex[0] = "rga";
                break;
            case VLC_CODEC_VYUY:
                swizzle_per_tex[0] = "gbr";
                break;
            case VLC_CODEC_YVYU:
                swizzle_per_tex[0] = "rag";
                break;
            default:
                assert(!"missing chroma");
                return VLC_EGENERIC;
        }
    }
    else
        return VLC_EGENERIC;

    /* [R/G/B][Y U V O] from TV range to full range
     * XXX we could also do hue/brightness/constrast/gamma
     * by simply changing the coefficients
     */
    static const float matrix_bt601_tv2full[12] = {
        1.164383561643836,  0.0000,             1.596026785714286, -0.874202217873451 ,
        1.164383561643836, -0.391762290094914, -0.812967647237771,  0.531667823499146 ,
        1.164383561643836,  2.017232142857142,  0.0000,            -1.085630789302022 ,
    };
    static const float matrix_bt709_tv2full[12] = {
        1.164383561643836,  0.0000,             1.792741071428571, -0.972945075016308 ,
        1.164383561643836, -0.21324861427373,  -0.532909328559444,  0.301482665475862 ,
        1.164383561643836,  2.112401785714286,  0.0000,            -1.133402217873451 ,
    };
    static const float matrix_bt2020_tv2full[12] = {
        1.164383530616760,  0.0000,             1.678674221038818, -0.915687978267670 ,
        1.164383530616760, -0.187326118350029, -0.650424420833588,  0.347458571195602 ,
        1.164383530616760,  2.141772270202637,  0.0000,            -1.148145079612732 ,
    };

    const float *matrix;
    switch (yuv_space)
    {
        case COLOR_SPACE_BT601:
            matrix = matrix_bt601_tv2full;
            break;
        case COLOR_SPACE_BT2020:
            matrix = matrix_bt2020_tv2full;
            break;
        default:
            matrix = matrix_bt709_tv2full;
    };

    for (int i = 0; i < 4; i++) {
        float correction = i < 3 ? yuv_range_correction : 1.f;
        /* We place coefficient values for coefficient[4] in one array from
         * matrix values. Notice that we fill values from top down instead
         * of left to right.*/
        for (int j = 0; j < 4; j++)
            tc->yuv_coefficients[i*4+j] = j < 3 ? correction * matrix[j*4+i] : 0.f;
    }

    tc->yuv_color = true;

    *swap_uv = chroma == VLC_CODEC_YV12 || chroma == VLC_CODEC_YV9 ||
               chroma == VLC_CODEC_NV21;
    return VLC_SUCCESS;
}

static int
tc_rgb_base_init(opengl_tex_converter_t *tc, GLenum tex_target,
                 vlc_fourcc_t chroma)
{
    (void) tex_target;

    switch (chroma)
    {
        case VLC_CODEC_RGB32:
        case VLC_CODEC_RGBA:
            tc->texs[0] = (struct opengl_tex_cfg) {
                { 1, 1 }, { 1, 1 }, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE
            };
            break;
        case VLC_CODEC_BGRA: {
            if (GetTexFormatSize(tc, tex_target, GL_BGRA, GL_RGBA,
                                 GL_UNSIGNED_BYTE) != 32)
                return VLC_EGENERIC;
            tc->texs[0] = (struct opengl_tex_cfg) {
                { 1, 1 }, { 1, 1 }, GL_RGBA, GL_BGRA, GL_UNSIGNED_BYTE
            };
            break;
        }
        default:
            return VLC_EGENERIC;
    }
    tc->tex_count = 1;
    return VLC_SUCCESS;
}

static int
tc_base_fetch_locations(opengl_tex_converter_t *tc, GLuint program)
{
    if (tc->yuv_color)
    {
        tc->uloc.Coefficients = tc->vt->GetUniformLocation(program,
                                                            "Coefficients");
        if (tc->uloc.Coefficients == -1)
            return VLC_EGENERIC;
    }

    for (unsigned int i = 0; i < tc->tex_count; ++i)
    {
        char name[sizeof("TextureX")];
        snprintf(name, sizeof(name), "Texture%1u", i);
        tc->uloc.Texture[i] = tc->vt->GetUniformLocation(program, name);
        if (tc->uloc.Texture[i] == -1)
            return VLC_EGENERIC;
        if (tc->tex_target == GL_TEXTURE_RECTANGLE)
        {
            snprintf(name, sizeof(name), "TexSize%1u", i);
            tc->uloc.TexSize[i] = tc->vt->GetUniformLocation(program, name);
            if (tc->uloc.TexSize[i] == -1)
                return VLC_EGENERIC;
        }
    }

    tc->uloc.FillColor = tc->vt->GetUniformLocation(program, "FillColor");
    if (tc->uloc.FillColor == -1)
        return VLC_EGENERIC;

#ifdef HAVE_LIBPLACEBO
    const struct pl_shader_res *res = tc->pl_sh_res;
    for (int i = 0; res && i < res->num_variables; i++) {
        struct pl_shader_var sv = res->variables[i];
        tc->uloc.pl_vars[i] = tc->vt->GetUniformLocation(program, sv.var.name);
    }
#endif

    return VLC_SUCCESS;
}

static void
tc_base_prepare_shader(const opengl_tex_converter_t *tc,
                       const GLsizei *tex_width, const GLsizei *tex_height,
                       float alpha)
{
    (void) tex_width; (void) tex_height;

    if (tc->yuv_color)
        tc->vt->Uniform4fv(tc->uloc.Coefficients, 4, tc->yuv_coefficients);

    for (unsigned i = 0; i < tc->tex_count; ++i)
        tc->vt->Uniform1i(tc->uloc.Texture[i], i);

    tc->vt->Uniform4f(tc->uloc.FillColor, 1.0f, 1.0f, 1.0f, alpha);

    if (tc->tex_target == GL_TEXTURE_RECTANGLE)
    {
        for (unsigned i = 0; i < tc->tex_count; ++i)
            tc->vt->Uniform2f(tc->uloc.TexSize[i], tex_width[i],
                               tex_height[i]);
    }

#ifdef HAVE_LIBPLACEBO
    const struct pl_shader_res *res = tc->pl_sh_res;
    for (int i = 0; res && i < res->num_variables; i++) {
        GLint loc = tc->uloc.pl_vars[i];
        if (loc == -1) // uniform optimized out
            continue;

        struct pl_shader_var sv = res->variables[i];
#if PL_API_VER >= 4
        struct pl_var var = sv.var;
        // libplacebo doesn't need anything else anyway
        if (var.type != PL_VAR_FLOAT)
            continue;
#else
        struct ra_var var = sv.var;
        // libplacebo doesn't need anything else anyway
        if (var.type != RA_VAR_FLOAT)
            continue;
#endif

        if (var.dim_m > 1 && var.dim_m != var.dim_v)
            continue;

        const float *f = sv.data;
        switch (var.dim_m) {
        case 4: tc->vt->UniformMatrix4fv(loc, 1, GL_FALSE, f); break;
        case 3: tc->vt->UniformMatrix3fv(loc, 1, GL_FALSE, f); break;
        case 2: tc->vt->UniformMatrix2fv(loc, 1, GL_FALSE, f); break;

        case 1:
            switch (var.dim_v) {
            case 1: tc->vt->Uniform1f(loc, f[0]); break;
            case 2: tc->vt->Uniform2f(loc, f[0], f[1]); break;
            case 3: tc->vt->Uniform3f(loc, f[0], f[1], f[2]); break;
            case 4: tc->vt->Uniform4f(loc, f[0], f[1], f[2], f[3]); break;
            }
            break;
        }
    }
#endif
}

static int
tc_xyz12_fetch_locations(opengl_tex_converter_t *tc, GLuint program)
{
    tc->uloc.Texture[0] = tc->vt->GetUniformLocation(program, "Texture0");
    return tc->uloc.Texture[0] != -1 ? VLC_SUCCESS : VLC_EGENERIC;
}

static void
tc_xyz12_prepare_shader(const opengl_tex_converter_t *tc,
                        const GLsizei *tex_width, const GLsizei *tex_height,
                        float alpha)
{
    (void) tex_width; (void) tex_height; (void) alpha;
    tc->vt->Uniform1i(tc->uloc.Texture[0], 0);
}

static GLuint
xyz12_shader_init(opengl_tex_converter_t *tc)
{
    tc->tex_count = 1;
    tc->tex_target = GL_TEXTURE_2D;
    tc->texs[0] = (struct opengl_tex_cfg) {
        { 1, 1 }, { 1, 1 }, GL_RGB, GL_RGB, GL_UNSIGNED_SHORT
    };

    tc->pf_fetch_locations = tc_xyz12_fetch_locations;
    tc->pf_prepare_shader = tc_xyz12_prepare_shader;

    /* Shader for XYZ to RGB correction
     * 3 steps :
     *  - XYZ gamma correction
     *  - XYZ to RGB matrix conversion
     *  - reverse RGB gamma correction
     */
    static const char *template =
        "#version %u\n"
        "%s"
        "uniform sampler2D Texture0;"
        "uniform vec4 xyz_gamma = vec4(2.6);"
        "uniform vec4 rgb_gamma = vec4(1.0/2.2);"
        /* WARN: matrix Is filled column by column (not row !) */
        "uniform mat4 matrix_xyz_rgb = mat4("
        "    3.240454 , -0.9692660, 0.0556434, 0.0,"
        "   -1.5371385,  1.8760108, -0.2040259, 0.0,"
        "    -0.4985314, 0.0415560, 1.0572252,  0.0,"
        "    0.0,      0.0,         0.0,        1.0 "
        " );"

        "varying vec2 TexCoord0;"
        "void main()"
        "{ "
        " vec4 v_in, v_out;"
        " v_in  = texture2D(Texture0, TexCoord0);"
        " v_in = pow(v_in, xyz_gamma);"
        " v_out = matrix_xyz_rgb * v_in ;"
        " v_out = pow(v_out, rgb_gamma) ;"
        " v_out = clamp(v_out, 0.0, 1.0) ;"
        " gl_FragColor = v_out;"
        "}";

    char *code;
    if (asprintf(&code, template, tc->glsl_version, tc->glsl_precision_header) < 0)
        return 0;

    GLuint fragment_shader = tc->vt->CreateShader(GL_FRAGMENT_SHADER);
    tc->vt->ShaderSource(fragment_shader, 1, (const char **) &code, NULL);
    tc->vt->CompileShader(fragment_shader);
    free(code);
    return fragment_shader;
}

#ifdef HAVE_LIBPLACEBO
static struct pl_color_space pl_color_space_from_video_format(const video_format_t *fmt)
{
    static enum pl_color_primaries primaries[COLOR_PRIMARIES_MAX+1] = {
        [COLOR_PRIMARIES_UNDEF]     = PL_COLOR_PRIM_UNKNOWN,
        [COLOR_PRIMARIES_BT601_525] = PL_COLOR_PRIM_BT_601_525,
        [COLOR_PRIMARIES_BT601_625] = PL_COLOR_PRIM_BT_601_625,
        [COLOR_PRIMARIES_BT709]     = PL_COLOR_PRIM_BT_709,
        [COLOR_PRIMARIES_BT2020]    = PL_COLOR_PRIM_BT_2020,
        [COLOR_PRIMARIES_DCI_P3]    = PL_COLOR_PRIM_DCI_P3,
        [COLOR_PRIMARIES_BT470_M]   = PL_COLOR_PRIM_BT_470M,
    };

    static enum pl_color_transfer transfers[TRANSFER_FUNC_MAX+1] = {
        [TRANSFER_FUNC_UNDEF]        = PL_COLOR_TRC_UNKNOWN,
        [TRANSFER_FUNC_LINEAR]       = PL_COLOR_TRC_LINEAR,
        [TRANSFER_FUNC_SRGB]         = PL_COLOR_TRC_SRGB,
        [TRANSFER_FUNC_SMPTE_ST2084] = PL_COLOR_TRC_PQ,
        [TRANSFER_FUNC_HLG]          = PL_COLOR_TRC_HLG,
        // these are all designed to be displayed on BT.1886 displays, so this
        // is the correct way to handle them in libplacebo
        [TRANSFER_FUNC_BT470_BG]    = PL_COLOR_TRC_BT_1886,
        [TRANSFER_FUNC_BT470_M]     = PL_COLOR_TRC_BT_1886,
        [TRANSFER_FUNC_BT709]       = PL_COLOR_TRC_BT_1886,
        [TRANSFER_FUNC_SMPTE_240]   = PL_COLOR_TRC_BT_1886,
    };

    // Derive the signal peak/avg from the color light level metadata
    float sig_peak = fmt->lighting.MaxCLL / PL_COLOR_REF_WHITE;
    float sig_avg = fmt->lighting.MaxFALL / PL_COLOR_REF_WHITE;

    // As a fallback value for the signal peak, we can also use the mastering
    // metadata's luminance information
    if (!sig_peak)
        sig_peak = fmt->mastering.max_luminance / PL_COLOR_REF_WHITE;

    // Sanitize the sig_peak/sig_avg, because of buggy or low quality tagging
    // that's sadly common in lots of typical sources
    sig_peak = (sig_peak > 1.0 && sig_peak <= 100.0) ? sig_peak : 0.0;
    sig_avg  = (sig_avg >= 0.0 && sig_avg <= 1.0) ? sig_avg : 0.0;

    return (struct pl_color_space) {
        .primaries = primaries[fmt->primaries],
        .transfer  = transfers[fmt->transfer],
        .light     = PL_COLOR_LIGHT_UNKNOWN,
        .sig_peak  = sig_peak,
        .sig_avg   = sig_avg,
    };
}
#endif

GLuint
opengl_fragment_shader_init_impl(opengl_tex_converter_t *tc, GLenum tex_target,
                                 vlc_fourcc_t chroma, video_color_space_t yuv_space)
{
    const char *swizzle_per_tex[PICTURE_PLANE_MAX] = { NULL, };
    const bool is_yuv = vlc_fourcc_IsYUV(chroma);
    bool yuv_swap_uv = false;
    int ret;

    const vlc_chroma_description_t *desc = vlc_fourcc_GetChromaDescription(chroma);
    if (desc == NULL)
        return VLC_EGENERIC;

    if (chroma == VLC_CODEC_XYZ12)
        return xyz12_shader_init(tc);

    if (is_yuv)
        ret = tc_yuv_base_init(tc, tex_target, chroma, desc, yuv_space,
                               &yuv_swap_uv, swizzle_per_tex);
    else
        ret = tc_rgb_base_init(tc, tex_target, chroma);

    if (ret != VLC_SUCCESS)
        return 0;

    const char *sampler, *lookup, *coord_name;
    switch (tex_target)
    {
        case GL_TEXTURE_2D:
            sampler = "sampler2D";
            lookup  = "texture2D";
            coord_name = "TexCoord";
            break;
        case GL_TEXTURE_RECTANGLE:
            sampler = "sampler2DRect";
            lookup  = "texture2DRect";
            coord_name = "TexCoordRect";
            break;
        default:
            vlc_assert_unreachable();
    }

    struct vlc_memstream ms;
    if (vlc_memstream_open(&ms) != 0)
        return 0;

#define ADD(x) vlc_memstream_puts(&ms, x)
#define ADDF(x, ...) vlc_memstream_printf(&ms, x, ##__VA_ARGS__)

    ADDF("#version %u\n%s", tc->glsl_version, tc->glsl_precision_header);

    for (unsigned i = 0; i < tc->tex_count; ++i)
        ADDF("uniform %s Texture%u;\n"
             "varying vec2 TexCoord%u;\n", sampler, i, i);

#ifdef HAVE_LIBPLACEBO
    if (tc->pl_sh) {
        struct pl_shader *sh = tc->pl_sh;
        struct pl_color_map_params color_params = pl_color_map_default_params;
        color_params.intent = var_InheritInteger(tc->gl, "rendering-intent");
        color_params.tone_mapping_algo = var_InheritInteger(tc->gl, "tone-mapping");
        color_params.tone_mapping_param = var_InheritFloat(tc->gl, "tone-mapping-param");
        color_params.tone_mapping_desaturate = var_InheritFloat(tc->gl, "tone-mapping-desat");
        color_params.gamut_warning = var_InheritBool(tc->gl, "tone-mapping-warn");

        struct pl_color_space dst_space = pl_color_space_unknown;
        dst_space.primaries = var_InheritInteger(tc->gl, "target-prim");
        dst_space.transfer = var_InheritInteger(tc->gl, "target-trc");

        pl_shader_color_map(sh, &color_params,
                pl_color_space_from_video_format(&tc->fmt),
                dst_space, NULL, false);

        struct pl_shader_obj *dither_state = NULL;
        int method = var_InheritInteger(tc->gl, "dither-algo");
        if (method >= 0) {

            unsigned out_bits = 0;
            int override = var_InheritInteger(tc->gl, "dither-depth");
            if (override > 0)
                out_bits = override;
            else
            {
                GLint fb_depth = 0;
#if !defined(USE_OPENGL_ES2)
                /* fetch framebuffer depth (we are already bound to the default one). */
                if (tc->vt->GetFramebufferAttachmentParameteriv != NULL)
                    tc->vt->GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK_LEFT,
                                                                GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE,
                                                                &fb_depth);
#endif
                if (fb_depth <= 0)
                    fb_depth = 8;
                out_bits = fb_depth;
            }

            pl_shader_dither(sh, out_bits, &dither_state, &(struct pl_dither_params) {
                .method   = method,
                .lut_size = 4, // avoid too large values, since this gets embedded
            });
        }

        const struct pl_shader_res *res = tc->pl_sh_res = pl_shader_finalize(sh);
        pl_shader_obj_destroy(&dither_state);

        FREENULL(tc->uloc.pl_vars);
        tc->uloc.pl_vars = calloc(res->num_variables, sizeof(GLint));
        for (int i = 0; i < res->num_variables; i++) {
            struct pl_shader_var sv = res->variables[i];
#if PL_API_VER >= 4
            const char *glsl_type_name = pl_var_glsl_type_name(sv.var);
#else
            const char *glsl_type_name = ra_var_glsl_type_name(sv.var);
#endif
            ADDF("uniform %s %s;\n", glsl_type_name, sv.var.name);
        }

        // We can't handle these yet, but nothing we use requires them, either
        assert(res->num_vertex_attribs == 0);
        assert(res->num_descriptors == 0);

        ADD(res->glsl);
    }
#else
    if (tc->fmt.transfer == TRANSFER_FUNC_SMPTE_ST2084 ||
        tc->fmt.primaries == COLOR_PRIMARIES_BT2020)
    {
        // no warning for HLG because it's more or less backwards-compatible
        msg_Warn(tc->gl, "VLC needs to be built with support for libplacebo "
                 "in order to display wide gamut or HDR signals correctly.");
    }
#endif

    if (tex_target == GL_TEXTURE_RECTANGLE)
    {
        for (unsigned i = 0; i < tc->tex_count; ++i)
            ADDF("uniform vec2 TexSize%u;\n", i);
    }

    if (is_yuv)
        ADD("uniform vec4 Coefficients[4];\n");

    ADD("uniform vec4 FillColor;\n"
        "void main(void) {\n"
        " float val;vec4 colors;\n");

    if (tex_target == GL_TEXTURE_RECTANGLE)
    {
        for (unsigned i = 0; i < tc->tex_count; ++i)
            ADDF(" vec2 TexCoordRect%u = vec2(TexCoord%u.x * TexSize%u.x, "
                 "TexCoord%u.y * TexSize%u.y);\n", i, i, i, i, i);
    }

    unsigned color_idx = 0;
    for (unsigned i = 0; i < tc->tex_count; ++i)
    {
        const char *swizzle = swizzle_per_tex[i];
        if (swizzle)
        {
            size_t swizzle_count = strlen(swizzle);
            ADDF(" colors = %s(Texture%u, %s%u);\n", lookup, i, coord_name, i);
            for (unsigned j = 0; j < swizzle_count; ++j)
            {
                ADDF(" val = colors.%c;\n"
                     " vec4 color%u = vec4(val, val, val, 1);\n",
                     swizzle[j], color_idx);
                color_idx++;
                assert(color_idx <= PICTURE_PLANE_MAX);
            }
        }
        else
        {
            ADDF(" vec4 color%u = %s(Texture%u, %s%u);\n",
                 color_idx, lookup, i, coord_name, i);
            color_idx++;
            assert(color_idx <= PICTURE_PLANE_MAX);
        }
    }
    unsigned color_count = color_idx;
    assert(yuv_space == COLOR_SPACE_UNDEF || color_count == 3);

    if (is_yuv)
        ADD(" vec4 result = (color0 * Coefficients[0]) + Coefficients[3];\n");
    else
        ADD(" vec4 result = color0;\n");

    for (unsigned i = 1; i < color_count; ++i)
    {
        if (yuv_swap_uv)
        {
            assert(color_count == 3);
            color_idx = (i % 2) + 1;
        }
        else
            color_idx = i;

        if (is_yuv)
            ADDF(" result = (color%u * Coefficients[%u]) + result;\n",
                 color_idx, i);
        else
            ADDF(" result = color%u + result;\n", color_idx);
    }

#ifdef HAVE_LIBPLACEBO
    if (tc->pl_sh_res) {
        const struct pl_shader_res *res = tc->pl_sh_res;
        assert(res->input  == PL_SHADER_SIG_COLOR);
        assert(res->output == PL_SHADER_SIG_COLOR);
        ADDF(" result = %s(result);\n", res->name);
    }
#endif

    ADD(" gl_FragColor = result * FillColor;\n"
        "}");

#undef ADD
#undef ADDF

    if (vlc_memstream_close(&ms) != 0)
        return 0;

    GLuint fragment_shader = tc->vt->CreateShader(GL_FRAGMENT_SHADER);
    if (fragment_shader == 0)
    {
        free(ms.ptr);
        return 0;
    }
    GLint length = ms.length;
    tc->vt->ShaderSource(fragment_shader, 1, (const char **)&ms.ptr, &length);
    tc->vt->CompileShader(fragment_shader);
    if (tc->b_dump_shaders)
        msg_Dbg(tc->gl, "\n=== Fragment shader for fourcc: %4.4s, colorspace: %d ===\n%s\n",
                (const char *)&chroma, yuv_space, ms.ptr);
    free(ms.ptr);

    tc->tex_target = tex_target;

    tc->pf_fetch_locations = tc_base_fetch_locations;
    tc->pf_prepare_shader = tc_base_prepare_shader;

    return fragment_shader;
}
