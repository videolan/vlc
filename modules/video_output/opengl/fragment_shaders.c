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
#include "../placebo_utils.h"
#endif

#include <vlc_common.h>
#include <vlc_memstream.h>
#include "interop.h"
#include "internal.h"
#include "vout_helper.h"

static const float MATRIX_COLOR_RANGE_LIMITED[4*3] = {
    255.0/219,         0,         0, -255.0/219 *  16.0/255,
            0, 255.0/224,         0, -255.0/224 * 128.0/255,
            0,         0, 255.0/224, -255.0/224 * 128.0/255,
};

static const float MATRIX_COLOR_RANGE_FULL[4*3] = {
    1, 0, 0,          0,
    0, 1, 0, -128.0/255,
    0, 0, 1, -128.0/255,
};

/*
 * Construct the transformation matrix from the luma weight of the red and blue
 * component (the green component is deduced).
 */
#define MATRIX_YUV_TO_RGB(KR, KB) \
    MATRIX_YUV_TO_RGB_(KR, (1-(KR)-(KB)), KB)

/*
 * Construct the transformation matrix from the luma weight of the RGB
 * components.
 *
 * KR: luma weight of the red component
 * KG: luma weight of the green component
 * KB: luma weight of the blue component
 *
 * By definition, KR + KG + KB == 1.
 *
 * Ref: <https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion>
 * Ref: libplacebo: src/colorspace.c:luma_coeffs()
 * */
#define MATRIX_YUV_TO_RGB_(KR, KG, KB) { \
    1,                         0,              2*(1.0-(KR)), \
    1, -2*(1.0-(KB))*((KB)/(KG)), -2*(1.0-(KR))*((KR)/(KG)), \
    1,              2*(1.0-(KB)),                         0, \
}

static const float MATRIX_BT601[3*3] = MATRIX_YUV_TO_RGB(0.299, 0.114);
static const float MATRIX_BT709[3*3] = MATRIX_YUV_TO_RGB(0.2126, 0.0722);
static const float MATRIX_BT2020[3*3] = MATRIX_YUV_TO_RGB(0.2627, 0.0593);

static void
init_conv_matrix(float conv_matrix_out[],
                 video_color_space_t color_space,
                 video_color_range_t color_range)
{
    const float *space_matrix;
    switch (color_space) {
        case COLOR_SPACE_BT601:
            space_matrix = MATRIX_BT601;
            break;
        case COLOR_SPACE_BT2020:
            space_matrix = MATRIX_BT2020;
            break;
        default:
            space_matrix = MATRIX_BT709;
    }

    /* Init the conversion matrix in row-major order. */

    const float *range_matrix = color_range == COLOR_RANGE_FULL
                              ? MATRIX_COLOR_RANGE_FULL
                              : MATRIX_COLOR_RANGE_LIMITED;
    /* Multiply the matrices on CPU once for all */
    for (int x = 0; x < 4; ++x)
    {
        for (int y = 0; y < 3; ++y)
        {
            /* Perform intermediate computation in double precision even if the
             * result is in single-precision, to avoid unnecessary errors. */
            double sum = 0;
            for (int k = 0; k < 3; ++k)
                sum += space_matrix[y * 3 + k] * range_matrix[k * 4 + x];
            conv_matrix_out[y * 4 + x] = sum;
        }
    }

    /* Add a row to fill a 4x4 matrix.
     * (non-square matrices are not supported on old OpenGL ES versions) */
    conv_matrix_out[12] = 0;
    conv_matrix_out[13] = 0;
    conv_matrix_out[14] = 0;
    conv_matrix_out[15] = 1;
}

static int
tc_yuv_base_init(opengl_tex_converter_t *tc, vlc_fourcc_t chroma,
                 const vlc_chroma_description_t *desc,
                 video_color_space_t yuv_space,
                 bool *swap_uv)
{
    /* The current implementation always converts from limited to full range. */
    const video_color_range_t range = COLOR_RANGE_LIMITED;
    float matrix[4*4];
    init_conv_matrix(matrix, yuv_space, range);

    if (desc->pixel_size == 2)
    {
        if (chroma != VLC_CODEC_P010 && chroma != VLC_CODEC_P016) {
            /* Do a bit shift if samples are stored on LSB. */
            float yuv_range_correction = (float)((1 << 16) - 1)
                                         / ((1 << desc->pixel_bits) - 1);
            /* We want to transform the input color (y, u, v, 1) to
             * (r*y, r*u, r*v, 1), where r = yuv_range_correction.
             *
             * This can be done by left-multiplying the color vector by a
             * matrix R:
             *
             *                 R
             *  / r*y \   / r 0 0 0 \   / y \
             *  | r*u | = | 0 r 0 0 | * | u |
             *  | r*v |   | 0 0 r 0 |   | v |
             *  \  1  /   \ 0 0 0 1 /   \ 1 /
             *
             * Combine this transformation with the color conversion matrix:
             *
             *     matrix := matrix * R
             *
             * This is equivalent to multipying the 3 first rows by r
             * (yuv_range_conversion).
             */
            for (int i = 0; i < 4*3; ++i)
                matrix[i] *= yuv_range_correction;
        }
    }

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++)
            tc->yuv_coefficients[i*4+j] = matrix[j*4+i];
    }

    tc->yuv_color = true;

    *swap_uv = chroma == VLC_CODEC_YV12 || chroma == VLC_CODEC_YV9 ||
               chroma == VLC_CODEC_NV21;
    return VLC_SUCCESS;
}

static int
tc_base_fetch_locations(opengl_tex_converter_t *tc, GLuint program)
{
    struct vlc_gl_interop *interop = tc->interop;

    if (tc->yuv_color)
    {
        tc->uloc.ConvMatrix = tc->vt->GetUniformLocation(program,
                                                         "ConvMatrix");
        if (tc->uloc.ConvMatrix == -1)
            return VLC_EGENERIC;
    }

    for (unsigned int i = 0; i < interop->tex_count; ++i)
    {
        char name[sizeof("TextureX")];
        snprintf(name, sizeof(name), "Texture%1u", i);
        tc->uloc.Texture[i] = tc->vt->GetUniformLocation(program, name);
        if (tc->uloc.Texture[i] == -1)
            return VLC_EGENERIC;
        if (interop->tex_target == GL_TEXTURE_RECTANGLE)
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
    const struct vlc_gl_interop *interop = tc->interop;

    if (tc->yuv_color)
        tc->vt->UniformMatrix4fv(tc->uloc.ConvMatrix, 1, GL_FALSE,
                                 tc->yuv_coefficients);

    for (unsigned i = 0; i < interop->tex_count; ++i)
        tc->vt->Uniform1i(tc->uloc.Texture[i], i);

    tc->vt->Uniform4f(tc->uloc.FillColor, 1.0f, 1.0f, 1.0f, alpha);

    if (interop->tex_target == GL_TEXTURE_RECTANGLE)
    {
        for (unsigned i = 0; i < interop->tex_count; ++i)
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
        struct pl_var var = sv.var;
        // libplacebo doesn't need anything else anyway
        if (var.type != PL_VAR_FLOAT)
            continue;
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

static int
opengl_init_swizzle(const struct vlc_gl_interop *interop,
                    const char *swizzle_per_tex[],
                    vlc_fourcc_t chroma,
                    const vlc_chroma_description_t *desc)
{
    GLint oneplane_texfmt;
    if (vlc_gl_StrHasToken(interop->glexts, "GL_ARB_texture_rg"))
        oneplane_texfmt = GL_RED;
    else
        oneplane_texfmt = GL_LUMINANCE;

    if (desc->plane_count == 3)
        swizzle_per_tex[0] = swizzle_per_tex[1] = swizzle_per_tex[2] = "r";
    else if (desc->plane_count == 2)
    {
        if (oneplane_texfmt == GL_RED)
        {
            swizzle_per_tex[0] = "r";
            swizzle_per_tex[1] = "rg";
        }
        else
        {
            swizzle_per_tex[0] = "x";
            swizzle_per_tex[1] = "xa";
        }
    }
    else if (desc->plane_count == 1)
    {
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
    return VLC_SUCCESS;
}

GLuint
opengl_fragment_shader_init(opengl_tex_converter_t *tc, GLenum tex_target,
                            vlc_fourcc_t chroma, video_color_space_t yuv_space)
{
    struct vlc_gl_interop *interop = tc->interop;

    const char *swizzle_per_tex[PICTURE_PLANE_MAX] = { NULL, };
    const bool is_yuv = vlc_fourcc_IsYUV(chroma);
    bool yuv_swap_uv = false;
    int ret;

    const vlc_chroma_description_t *desc = vlc_fourcc_GetChromaDescription(chroma);
    if (desc == NULL)
        return 0;

    if (chroma == VLC_CODEC_XYZ12)
        return xyz12_shader_init(tc);

    if (is_yuv)
    {
        ret = tc_yuv_base_init(tc, chroma, desc, yuv_space, &yuv_swap_uv);
        if (ret != VLC_SUCCESS)
            return 0;
        ret = opengl_init_swizzle(tc->interop, swizzle_per_tex, chroma, desc);
        if (ret != VLC_SUCCESS)
            return 0;
    }

    const char *sampler, *lookup, *coord_name;
    switch (tex_target)
    {
        case GL_TEXTURE_EXTERNAL_OES:
            sampler = "samplerExternalOES";
            lookup = "texture2D";
            coord_name = "TexCoord";
            break;
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

    ADDF("#version %u\n", tc->glsl_version);

    if (tex_target == GL_TEXTURE_EXTERNAL_OES)
        ADDF("#extension GL_OES_EGL_image_external : require\n");

    ADDF("%s", tc->glsl_precision_header);

    for (unsigned i = 0; i < interop->tex_count; ++i)
        ADDF("uniform %s Texture%u;\n"
             "varying vec2 TexCoord%u;\n", sampler, i, i);

#ifdef HAVE_LIBPLACEBO
    if (tc->pl_sh) {
        struct pl_shader *sh = tc->pl_sh;
        struct pl_color_map_params color_params = pl_color_map_default_params;
        color_params.intent = var_InheritInteger(tc->gl, "rendering-intent");
        color_params.tone_mapping_algo = var_InheritInteger(tc->gl, "tone-mapping");
        color_params.tone_mapping_param = var_InheritFloat(tc->gl, "tone-mapping-param");
#    if PL_API_VER >= 10
        color_params.desaturation_strength = var_InheritFloat(tc->gl, "desat-strength");
        color_params.desaturation_exponent = var_InheritFloat(tc->gl, "desat-exponent");
        color_params.desaturation_base = var_InheritFloat(tc->gl, "desat-base");
#    else
        color_params.tone_mapping_desaturate = var_InheritFloat(tc->gl, "tone-mapping-desat");
#    endif
        color_params.gamut_warning = var_InheritBool(tc->gl, "tone-mapping-warn");

        struct pl_color_space dst_space = pl_color_space_unknown;
        dst_space.primaries = var_InheritInteger(tc->gl, "target-prim");
        dst_space.transfer = var_InheritInteger(tc->gl, "target-trc");

        pl_shader_color_map(sh, &color_params,
                vlc_placebo_ColorSpace(&interop->fmt),
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
            const char *glsl_type_name = pl_var_glsl_type_name(sv.var);
            ADDF("uniform %s %s;\n", glsl_type_name, sv.var.name);
        }

        // We can't handle these yet, but nothing we use requires them, either
        assert(res->num_vertex_attribs == 0);
        assert(res->num_descriptors == 0);

        ADD(res->glsl);
    }
#else
    if (interop->fmt.transfer == TRANSFER_FUNC_SMPTE_ST2084 ||
        interop->fmt.primaries == COLOR_PRIMARIES_BT2020)
    {
        // no warning for HLG because it's more or less backwards-compatible
        msg_Warn(tc->gl, "VLC needs to be built with support for libplacebo "
                 "in order to display wide gamut or HDR signals correctly.");
    }
#endif

    if (tex_target == GL_TEXTURE_RECTANGLE)
    {
        for (unsigned i = 0; i < interop->tex_count; ++i)
            ADDF("uniform vec2 TexSize%u;\n", i);
    }

    if (is_yuv)
        ADD("uniform mat4 ConvMatrix;\n");

    ADD("uniform vec4 FillColor;\n"
        "void main(void) {\n");

    if (tex_target == GL_TEXTURE_RECTANGLE)
    {
        for (unsigned i = 0; i < interop->tex_count; ++i)
            ADDF(" vec2 TexCoordRect%u = vec2(TexCoord%u.x * TexSize%u.x, "
                 "TexCoord%u.y * TexSize%u.y);\n", i, i, i, i, i);
    }

    unsigned color_count;
    if (is_yuv) {
        ADD(" vec4 texel;\n"
            " vec4 pixel = vec4(0.0, 0.0, 0.0, 1.0);\n");
        unsigned color_idx = 0;
        for (unsigned i = 0; i < interop->tex_count; ++i)
        {
            const char *swizzle = swizzle_per_tex[i];
            assert(swizzle);
            size_t swizzle_count = strlen(swizzle);
            ADDF(" texel = %s(Texture%u, %s%u);\n", lookup, i, coord_name, i);
            for (unsigned j = 0; j < swizzle_count; ++j)
            {
                ADDF(" pixel[%u] = texel.%c;\n", color_idx, swizzle[j]);
                color_idx++;
                assert(color_idx <= PICTURE_PLANE_MAX);
            }
        }
        if (yuv_swap_uv) {
            ADD(" pixel = pixel.xzyw;\n");
        }
        ADD(" vec4 result = ConvMatrix * pixel;\n");
        color_count = color_idx;
    }
    else
    {
        ADDF(" vec4 result = %s(Texture0, %s0);\n", lookup, coord_name);
        if (yuv_swap_uv) {
            ADD(" result = result.xzyw;\n");
        }
        color_count = 1;
    }
    assert(yuv_space == COLOR_SPACE_UNDEF || color_count == 3);

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

    tc->pf_fetch_locations = tc_base_fetch_locations;
    tc->pf_prepare_shader = tc_base_prepare_shader;

    return fragment_shader;
}
