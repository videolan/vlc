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

#include <vlc_common.h>
#include <vlc_memstream.h>
#include "internal.h"

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
        case GL_RED:
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
                 vlc_fourcc_t chroma, video_color_space_t yuv_space,
                 bool *swap_uv, const char *swizzle_per_tex[])
{
    const vlc_chroma_description_t *desc = vlc_fourcc_GetChromaDescription(chroma);
    if (desc == NULL)
        return VLC_EGENERIC;

    GLint oneplane_texfmt, oneplane16_texfmt, twoplanes_texfmt;

    if (HasExtension(tc->glexts, "GL_ARB_texture_rg"))
    {
        oneplane_texfmt = GL_RED;
        oneplane16_texfmt = GL_R16;
        twoplanes_texfmt = GL_RG;
    }
    else
    {
        oneplane_texfmt = GL_LUMINANCE;
        oneplane16_texfmt = GL_LUMINANCE16;
        twoplanes_texfmt = GL_LUMINANCE_ALPHA;
    }

    float yuv_range_correction = 1.0;
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
            if (oneplane16_texfmt == 0
             || GetTexFormatSize(tc, tex_target, oneplane_texfmt,
                                 oneplane16_texfmt, GL_UNSIGNED_SHORT) != 16)
                return VLC_EGENERIC;

            internal = oneplane16_texfmt;
            type = GL_UNSIGNED_SHORT;
            yuv_range_correction = (float)((1 << 16) - 1)
                                 / ((1 << desc->pixel_bits) - 1);
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
        if (desc->pixel_size != 1)
            return VLC_EGENERIC;

        tc->tex_count = 2;
        tc->texs[0] = (struct opengl_tex_cfg) {
            { 1, 1 }, { 1, 1 }, oneplane_texfmt, oneplane_texfmt, GL_UNSIGNED_BYTE
        };
        tc->texs[1] = (struct opengl_tex_cfg) {
            { 1, 2 }, { 1, 2 }, twoplanes_texfmt, twoplanes_texfmt, GL_UNSIGNED_BYTE
        };

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

    const float *matrix;
    switch (yuv_space)
    {
        case COLOR_SPACE_BT601:
            matrix = matrix_bt601_tv2full;
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


    tc->tex_count = 1;
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

GLuint
opengl_fragment_shader_init_impl(opengl_tex_converter_t *tc, GLenum tex_target,
                                 vlc_fourcc_t chroma, video_color_space_t yuv_space)
{
    const char *swizzle_per_tex[PICTURE_PLANE_MAX] = { NULL, };
    const bool is_yuv = vlc_fourcc_IsYUV(chroma);
    bool yuv_swap_uv = false;
    int ret;

    if (chroma == VLC_CODEC_XYZ12)
        return xyz12_shader_init(tc);

    if (is_yuv)
        ret = tc_yuv_base_init(tc, tex_target, chroma, yuv_space,
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
        ADDF("uniform %s Texture%u;"
             "varying vec2 TexCoord%u;", sampler, i, i);

    if (tex_target == GL_TEXTURE_RECTANGLE)
    {
        for (unsigned i = 0; i < tc->tex_count; ++i)
            ADDF("uniform vec2 TexSize%u;", i);
    }

    if (is_yuv)
        ADD("uniform vec4 Coefficients[4];");

    ADD("uniform vec4 FillColor;"
        "void main(void) {"
        "float val;vec4 colors;");

    if (tex_target == GL_TEXTURE_RECTANGLE)
    {
        for (unsigned i = 0; i < tc->tex_count; ++i)
            ADDF("vec2 TexCoordRect%u = vec2(TexCoord%u.x * TexSize%u.x, "
                 "TexCoord%u.y * TexSize%u.y);", i, i, i, i, i);
    }

    unsigned color_idx = 0;
    for (unsigned i = 0; i < tc->tex_count; ++i)
    {
        const char *swizzle = swizzle_per_tex[i];
        if (swizzle)
        {
            size_t swizzle_count = strlen(swizzle);
            ADDF("colors = %s(Texture%u, %s%u);", lookup, i, coord_name, i);
            for (unsigned j = 0; j < swizzle_count; ++j)
            {
                ADDF("val = colors.%c;"
                     "vec4 color%u = vec4(val, val, val, 1);",
                     swizzle[j], color_idx);
                color_idx++;
                assert(color_idx <= PICTURE_PLANE_MAX);
            }
        }
        else
        {
            ADDF("vec4 color%u = %s(Texture%u, %s%u);",
                 color_idx, lookup, i, coord_name, i);
            color_idx++;
            assert(color_idx <= PICTURE_PLANE_MAX);
        }
    }
    unsigned color_count = color_idx;
    assert(yuv_space == COLOR_SPACE_UNDEF || color_count == 3);

    if (is_yuv)
        ADD("vec4 result = (color0 * Coefficients[0]) + Coefficients[3];");
    else
        ADD("vec4 result = color0;");

    for (unsigned i = 1; i < color_count; ++i)
    {
        unsigned color_idx;
        if (yuv_swap_uv)
        {
            assert(color_count == 3);
            color_idx = (i % 2) + 1;
        }
        else
            color_idx = i;

        if (is_yuv)
            ADDF("result = (color%u * Coefficients[%u]) + result;", color_idx, i);
        else
            ADDF("result = color%u + result;", color_idx);
    }

    ADD("gl_FragColor = result * FillColor;"
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
    free(ms.ptr);

    tc->tex_target = tex_target;

    tc->pf_fetch_locations = tc_base_fetch_locations;
    tc->pf_prepare_shader = tc_base_prepare_shader;

    return fragment_shader;
}
