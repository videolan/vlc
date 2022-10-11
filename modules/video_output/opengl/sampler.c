/*****************************************************************************
 * sampler.c
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "sampler.h"

#include <vlc_common.h>
#include <vlc_memstream.h>
#include <vlc_opengl.h>

#ifdef HAVE_LIBPLACEBO_GL
#include <libplacebo/opengl.h>
#include <libplacebo/shaders.h>
#include <libplacebo/shaders/colorspace.h>
#include "../libplacebo/utils.h"
#endif

#include "gl_api.h"
#include "gl_common.h"
#include "gl_util.h"

struct vlc_gl_sampler_priv {
    struct vlc_gl_sampler sampler;

    struct vlc_gl_t *gl;
    const struct vlc_gl_api *api;
    const opengl_vtable_t *vt; /* for convenience, same as &api->vt */

    struct vlc_gl_picture pic;

    struct {
        GLint Textures[PICTURE_PLANE_MAX];
        GLint TexSizes[PICTURE_PLANE_MAX]; /* for GL_TEXTURE_RECTANGLE */
        GLint ConvMatrix;
        GLint *pl_vars, *pl_descs; /* for pl_sh_res */
    } uloc;

    bool yuv_color;
    GLfloat conv_matrix[4*4];

#ifdef HAVE_LIBPLACEBO_GL
    /* libplacebo context */
    pl_log pl_log;
    pl_opengl pl_opengl;
    pl_shader pl_sh;
    pl_shader_obj dither_state, tone_map_state;
    const struct pl_shader_res *pl_sh_res;
#endif

    /* If set, vlc_texture() exposes a single plane (without chroma
     * conversion), selected by vlc_gl_sampler_SetCurrentPlane(). */
    bool expose_planes;
    unsigned plane;

    struct vlc_gl_extension_vt extension_vt;
};

static inline struct vlc_gl_sampler_priv *
PRIV(struct vlc_gl_sampler *sampler)
{
    return container_of(sampler, struct vlc_gl_sampler_priv, sampler);
}

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

    /* Init the conversion matrix in column-major order (OpenGL expects
     * column-major order by default, and OpenGL ES does not support row-major
     * order at all). */

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
            /* Notice the reversed indices: x is now the row, y is the
             * column. */
            conv_matrix_out[x * 4 + y] = sum;
        }
    }

    /* Add a row to fill a 4x4 matrix (remember it's in column-major order).
     * (non-square matrices are not supported on old OpenGL ES versions) */
    conv_matrix_out[3] = 0;
    conv_matrix_out[7] = 0;
    conv_matrix_out[11] = 0;
    conv_matrix_out[15] = 1;
}

static int
sampler_yuv_base_init(struct vlc_gl_sampler *sampler, vlc_fourcc_t chroma,
                      const vlc_chroma_description_t *desc,
                      video_color_space_t yuv_space)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);

    /* The current implementation always converts from limited to full range. */
    const video_color_range_t range = COLOR_RANGE_LIMITED;
    float *matrix = priv->conv_matrix;
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

    priv->yuv_color = true;

    /* Some formats require to swap the U and V components.
     *
     * This can be done by left-multiplying the color vector by a matrix S:
     *
     *               S
     *  / y \   / 1 0 0 0 \   / y \
     *  | v | = | 0 0 1 0 | * | u |
     *  | u |   | 0 1 0 0 |   | v |
     *  \ 1 /   \ 0 0 0 1 /   \ 1 /
     *
     * Combine this transformation with the color conversion matrix:
     *
     *     matrix := matrix * S
     *
     * This is equivalent to swap columns 1 and 2.
     */
    bool swap_uv = chroma == VLC_CODEC_YV12 || chroma == VLC_CODEC_YV9 ||
                   chroma == VLC_CODEC_NV21;
    if (swap_uv)
    {
        /* Remember, the matrix in column-major order */
        float tmp[4];
        /* tmp <- column1 */
        memcpy(tmp, matrix + 4, sizeof(tmp));
        /* column1 <- column2 */
        memcpy(matrix + 4, matrix + 8, sizeof(tmp));
        /* column2 <- tmp */
        memcpy(matrix + 8, tmp, sizeof(tmp));
    }
    return VLC_SUCCESS;
}

static void
sampler_base_fetch_locations(struct vlc_gl_sampler *sampler, GLuint program)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);

    const opengl_vtable_t *vt = priv->vt;

    if (priv->yuv_color)
    {
        priv->uloc.ConvMatrix = vt->GetUniformLocation(program, "ConvMatrix");
        assert(priv->uloc.ConvMatrix != -1);
    }

    struct vlc_gl_format *glfmt = &sampler->glfmt;

    assert(glfmt->tex_count < 10); /* to guarantee variable names length */
    for (unsigned int i = 0; i < glfmt->tex_count; ++i)
    {
        char name[sizeof("Textures[X]")];

        snprintf(name, sizeof(name), "Textures[%1u]", i);
        priv->uloc.Textures[i] = vt->GetUniformLocation(program, name);
        assert(priv->uloc.Textures[i] != -1);

        if (glfmt->tex_target == GL_TEXTURE_RECTANGLE)
        {
            snprintf(name, sizeof(name), "TexSizes[%1u]", i);
            priv->uloc.TexSizes[i] = vt->GetUniformLocation(program, name);
            assert(priv->uloc.TexSizes[i] != -1);
        }
    }

#ifdef HAVE_LIBPLACEBO_GL
    const struct pl_shader_res *res = priv->pl_sh_res;
    for (int i = 0; res && i < res->num_variables; i++) {
        struct pl_shader_var sv = res->variables[i];
        priv->uloc.pl_vars[i] = vt->GetUniformLocation(program, sv.var.name);
    }

    for (int i = 0; res && i < res->num_descriptors; i++) {
        struct pl_shader_desc sd = res->descriptors[i];
        priv->uloc.pl_descs[i] = vt->GetUniformLocation(program, sd.desc.name);
    }
#endif
}

static void
sampler_base_load(struct vlc_gl_sampler *sampler)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);

    const opengl_vtable_t *vt = priv->vt;
    struct vlc_gl_format *glfmt = &sampler->glfmt;
    struct vlc_gl_picture *pic = &priv->pic;

    if (priv->yuv_color)
        vt->UniformMatrix4fv(priv->uloc.ConvMatrix, 1, GL_FALSE,
                             priv->conv_matrix);

    for (unsigned i = 0; i < glfmt->tex_count; ++i)
    {
        vt->Uniform1i(priv->uloc.Textures[i], i);

        assert(pic->textures[i] != 0);
        vt->ActiveTexture(GL_TEXTURE0 + i);
        vt->BindTexture(glfmt->tex_target, pic->textures[i]);

    }

    if (glfmt->tex_target == GL_TEXTURE_RECTANGLE)
    {
        for (unsigned i = 0; i < glfmt->tex_count; ++i)
            vt->Uniform2f(priv->uloc.TexSizes[i], glfmt->tex_widths[i],
                          glfmt->tex_heights[i]);
    }

#ifdef HAVE_LIBPLACEBO_GL
    const struct pl_shader_res *res = priv->pl_sh_res;
    for (int i = 0; res && i < res->num_variables; i++) {
        GLint loc = priv->uloc.pl_vars[i];
        if (loc == -1) // uniform optimized out
            continue;

        struct pl_shader_var sv = res->variables[i];
        struct pl_var var = sv.var;
        // libplacebo doesn't need anything else anyway
        assert(var.type == PL_VAR_FLOAT);
        assert(var.dim_m == 1 || var.dim_m == var.dim_v);

        const float *f = sv.data;
        switch (var.dim_m) {
        case 4: vt->UniformMatrix4fv(loc, var.dim_a, GL_FALSE, f); break;
        case 3: vt->UniformMatrix3fv(loc, var.dim_a, GL_FALSE, f); break;
        case 2: vt->UniformMatrix2fv(loc, var.dim_a, GL_FALSE, f); break;

        case 1:
            switch (var.dim_v) {
            case 1: vt->Uniform1fv(loc, var.dim_a, f); break;
            case 2: vt->Uniform2fv(loc, var.dim_a, f); break;
            case 3: vt->Uniform3fv(loc, var.dim_a, f); break;
            case 4: vt->Uniform4fv(loc, var.dim_a, f); break;
            }
            break;
        }
    }

    for (int i = 0; res && i < res->num_descriptors; i++) {
        GLint loc = priv->uloc.pl_descs[i];
        if (loc == -1)
            continue;
        struct pl_shader_desc sd = res->descriptors[i];
        assert(sd.desc.type == PL_DESC_SAMPLED_TEX);
        pl_tex tex = sd.binding.object;
        int texid = glfmt->tex_count + i; // first free texture unit
        unsigned gltex, target;
        gltex = pl_opengl_unwrap(priv->pl_opengl->gpu, tex, &target, NULL, NULL);
        vt->Uniform1i(loc, texid);
        vt->ActiveTexture(GL_TEXTURE0 + texid);
        vt->BindTexture(target, gltex);

        static const GLint wraps[PL_TEX_ADDRESS_MODE_COUNT] = {
            [PL_TEX_ADDRESS_CLAMP]  = GL_CLAMP_TO_EDGE,
            [PL_TEX_ADDRESS_REPEAT] = GL_REPEAT,
            [PL_TEX_ADDRESS_MIRROR] = GL_MIRRORED_REPEAT,
        };

        static const GLint filters[PL_TEX_SAMPLE_MODE_COUNT] = {
            [PL_TEX_SAMPLE_NEAREST] = GL_NEAREST,
            [PL_TEX_SAMPLE_LINEAR]  = GL_LINEAR,
        };

        GLint filter = filters[sd.binding.sample_mode];
        GLint wrap = wraps[sd.binding.address_mode];
        vt->TexParameteri(target, GL_TEXTURE_MIN_FILTER, filter);
        vt->TexParameteri(target, GL_TEXTURE_MAG_FILTER, filter);
        switch (pl_tex_params_dimension(tex->params)) {
        case 3: vt->TexParameteri(target, GL_TEXTURE_WRAP_R, wrap); // fall through
        case 2: vt->TexParameteri(target, GL_TEXTURE_WRAP_T, wrap); // fall through
        case 1: vt->TexParameteri(target, GL_TEXTURE_WRAP_S, wrap); break;
        }
    }
#endif
}

static void
sampler_xyz12_fetch_locations(struct vlc_gl_sampler *sampler, GLuint program)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);
    const opengl_vtable_t *vt = priv->vt;

    priv->uloc.Textures[0] = vt->GetUniformLocation(program, "Textures[0]");
    assert(priv->uloc.Textures[0] != -1);
}

static void
sampler_xyz12_load(struct vlc_gl_sampler *sampler)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);
    const opengl_vtable_t *vt = priv->vt;
    struct vlc_gl_format *glfmt = &sampler->glfmt;
    struct vlc_gl_picture *pic = &priv->pic;

    vt->Uniform1i(priv->uloc.Textures[0], 0);

    assert(pic->textures[0] != 0);
    vt->ActiveTexture(GL_TEXTURE0);
    vt->BindTexture(glfmt->tex_target, pic->textures[0]);
}

static int
xyz12_shader_init(struct vlc_gl_sampler *sampler)
{
    static const struct vlc_gl_sampler_ops ops = {
        .fetch_locations = sampler_xyz12_fetch_locations,
        .load = sampler_xyz12_load,
    };
    sampler->ops = &ops;

    /* Shader for XYZ to RGB correction
     * 3 steps :
     *  - XYZ gamma correction
     *  - XYZ to RGB matrix conversion
     *  - reverse RGB gamma correction
     */
    static const char *template =
        "uniform sampler2D Textures[1];"
        "uniform vec4 xyz_gamma = vec4(2.6);"
        "uniform vec4 rgb_gamma = vec4(1.0/2.2);"
        /* WARN: matrix Is filled column by column (not row !) */
        "uniform mat4 matrix_xyz_rgb = mat4("
        "    3.240454 , -0.9692660, 0.0556434, 0.0,"
        "   -1.5371385,  1.8760108, -0.2040259, 0.0,"
        "    -0.4985314, 0.0415560, 1.0572252,  0.0,"
        "    0.0,      0.0,         0.0,        1.0 "
        " );"

        "vec4 vlc_texture(vec2 tex_coords)\n"
        "{ "
        " vec4 v_in, v_out;"
        " v_in  = texture2D(Textures[0], tex_coords);\n"
        " v_in = pow(v_in, xyz_gamma);"
        " v_out = matrix_xyz_rgb * v_in ;"
        " v_out = pow(v_out, rgb_gamma) ;"
        " v_out = clamp(v_out, 0.0, 1.0) ;"
        " return v_out;"
        "}\n";

    sampler->shader.body = strdup(template);
    if (!sampler->shader.body)
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
opengl_init_swizzle(struct vlc_gl_sampler *sampler,
                    const char *swizzle_per_tex[],
                    vlc_fourcc_t chroma,
                    const vlc_chroma_description_t *desc)
{
    if (desc->plane_count == 3)
        swizzle_per_tex[0] = swizzle_per_tex[1] = swizzle_per_tex[2] = "r";
    else if (desc->plane_count == 2)
    {
        swizzle_per_tex[0] = "r";
        if (sampler->glfmt.formats[1] == GL_RG)
            swizzle_per_tex[1] = "rg";
        else
            swizzle_per_tex[1] = "ra";
    }
    else if (desc->plane_count == 1)
    {
        /*
         * One plane, but uploaded into two separate textures for Y and UV.
         *
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
                swizzle_per_tex[0] = "g";
                swizzle_per_tex[1] = "rb";
                break;
            case VLC_CODEC_YUYV:
                swizzle_per_tex[0] = "r";
                swizzle_per_tex[1] = "ga";
                break;
            case VLC_CODEC_VYUY:
                swizzle_per_tex[0] = "g";
                swizzle_per_tex[1] = "br";
                break;
            case VLC_CODEC_YVYU:
                swizzle_per_tex[0] = "r";
                swizzle_per_tex[1] = "ag";
                break;
            default:
                assert(!"missing chroma");
                return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
}

static void
GetNames(GLenum tex_target, const char **glsl_sampler, const char **texture)
{
    switch (tex_target)
    {
        case GL_TEXTURE_EXTERNAL_OES:
            *glsl_sampler = "samplerExternalOES";
            *texture = "texture2D";
            break;
        case GL_TEXTURE_2D:
            *glsl_sampler = "sampler2D";
            *texture = "texture2D";
            break;
        case GL_TEXTURE_RECTANGLE:
            *glsl_sampler = "sampler2DRect";
            *texture = "texture2DRect";
            break;
        default:
            vlc_assert_unreachable();
    }
}

static int
InitShaderExtensions(struct vlc_gl_sampler *sampler, GLenum tex_target)
{
    if (tex_target == GL_TEXTURE_EXTERNAL_OES)
    {
        sampler->shader.extensions =
            strdup("#extension GL_OES_EGL_image_external : require\n");
        if (!sampler->shader.extensions)
            return VLC_EGENERIC;
    }
    else
        sampler->shader.extensions = NULL;

    return VLC_SUCCESS;
}

static void
sampler_planes_fetch_locations(struct vlc_gl_sampler *sampler, GLuint program)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);

    const opengl_vtable_t *vt = priv->vt;
    struct vlc_gl_format *glfmt = &sampler->glfmt;

    priv->uloc.Textures[0] = vt->GetUniformLocation(program, "Texture");
    assert(priv->uloc.Textures[0] != -1);

    if (glfmt->tex_target == GL_TEXTURE_RECTANGLE)
    {
        priv->uloc.TexSizes[0] = vt->GetUniformLocation(program, "TexSize");
        assert(priv->uloc.TexSizes[0] != -1);
    }
}

static void
sampler_planes_load(struct vlc_gl_sampler *sampler)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);
    unsigned plane = priv->plane;

    const opengl_vtable_t *vt = priv->vt;
    struct vlc_gl_format *glfmt = &sampler->glfmt;
    struct vlc_gl_picture *pic = &priv->pic;

    vt->Uniform1i(priv->uloc.Textures[0], 0);

    assert(pic->textures[plane] != 0);
    vt->ActiveTexture(GL_TEXTURE0);
    vt->BindTexture(glfmt->tex_target, pic->textures[plane]);

    if (glfmt->tex_target == GL_TEXTURE_RECTANGLE)
    {
        vt->Uniform2f(priv->uloc.TexSizes[0], glfmt->tex_widths[plane],
                      glfmt->tex_heights[plane]);
    }
}

static int
sampler_planes_init(struct vlc_gl_sampler *sampler)
{
    struct vlc_gl_format *glfmt = &sampler->glfmt;
    GLenum tex_target = glfmt->tex_target;

    struct vlc_memstream ms;
    if (vlc_memstream_open(&ms))
        return VLC_EGENERIC;

#define ADD(x) vlc_memstream_puts(&ms, x)
#define ADDF(x, ...) vlc_memstream_printf(&ms, x, ##__VA_ARGS__)

    const char *sampler_type;
    const char *texture_fn;
    GetNames(tex_target, &sampler_type, &texture_fn);

    ADDF("uniform %s Texture;\n", sampler_type);

    if (tex_target == GL_TEXTURE_RECTANGLE)
        ADD("uniform vec2 TexSize;\n");

    ADD("vec4 vlc_texture(vec2 tex_coords) {\n");

    if (tex_target == GL_TEXTURE_RECTANGLE)
    {
        /* The coordinates are in texels values, not normalized */
        ADD(" tex_coords = TexSize * tex_coords;\n");
    }

    ADDF("  return %s(Texture, tex_coords);\n", texture_fn);
    ADD("}\n");

#undef ADD
#undef ADDF

    if (vlc_memstream_close(&ms) != 0)
        return VLC_EGENERIC;

    int ret = InitShaderExtensions(sampler, tex_target);
    if (ret != VLC_SUCCESS)
    {
        free(ms.ptr);
        return VLC_EGENERIC;
    }
    sampler->shader.body = ms.ptr;

    static const struct vlc_gl_sampler_ops ops = {
        .fetch_locations = sampler_planes_fetch_locations,
        .load = sampler_planes_load,
    };
    sampler->ops = &ops;

    return VLC_SUCCESS;
}

static int
opengl_fragment_shader_init(struct vlc_gl_sampler *sampler, bool expose_planes)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);
    struct vlc_gl_format *glfmt = &sampler->glfmt;
    const video_format_t *fmt = &glfmt->fmt;

    GLenum tex_target = glfmt->tex_target;

    priv->expose_planes = expose_planes;
    priv->plane = 0;

    vlc_fourcc_t chroma = fmt->i_chroma;
    video_color_space_t yuv_space = fmt->space;

    const char *swizzle_per_tex[PICTURE_PLANE_MAX] = { NULL, };
    const bool is_yuv = vlc_fourcc_IsYUV(chroma);
    int ret;

    const vlc_chroma_description_t *desc = vlc_fourcc_GetChromaDescription(chroma);
    if (desc == NULL)
        return VLC_EGENERIC;

    unsigned tex_count = glfmt->tex_count;

    if (expose_planes)
        return sampler_planes_init(sampler);

    if (chroma == VLC_CODEC_XYZ12)
        return xyz12_shader_init(sampler);

    if (is_yuv)
    {
        ret = sampler_yuv_base_init(sampler, chroma, desc, yuv_space);
        if (ret != VLC_SUCCESS)
            return ret;
        ret = opengl_init_swizzle(sampler, swizzle_per_tex, chroma, desc);
        if (ret != VLC_SUCCESS)
            return ret;
    }

    const char *glsl_sampler, *lookup;
    GetNames(tex_target, &glsl_sampler, &lookup);

    struct vlc_memstream ms;
    if (vlc_memstream_open(&ms) != 0)
        return VLC_EGENERIC;

#define ADD(x) vlc_memstream_puts(&ms, x)
#define ADDF(x, ...) vlc_memstream_printf(&ms, x, ##__VA_ARGS__)

    ADDF("uniform %s Textures[%u];\n", glsl_sampler, tex_count);

#ifdef HAVE_LIBPLACEBO_GL
    if (priv->pl_sh) {
        pl_shader sh = priv->pl_sh;
        struct pl_color_map_params color_params;
        vlc_placebo_ColorMapParams(VLC_OBJECT(priv->gl), "gl", &color_params);

        struct pl_color_space dst_space = pl_color_space_unknown;
        dst_space.primaries = var_InheritInteger(priv->gl, "target-prim");
        dst_space.transfer = var_InheritInteger(priv->gl, "target-trc");

        pl_shader_color_map(sh, &color_params,
                vlc_placebo_ColorSpace(fmt),
                dst_space, &priv->tone_map_state, false);

        int method = var_InheritInteger(priv->gl, "dither-algo");
        if (method >= 0) {

            unsigned out_bits = 0;
            int override = var_InheritInteger(priv->gl, "dither-depth");
            if (override > 0)
                out_bits = override;
            else
            {
                GLint fb_depth = 0;
#if !defined(USE_OPENGL_ES2)
                const opengl_vtable_t *vt = priv->vt;
                /* fetch framebuffer depth (we are already bound to the default one). */
                if (vt->GetFramebufferAttachmentParameteriv != NULL)
                    vt->GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK_LEFT,
                                                            GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE,
                                                            &fb_depth);
#endif
                if (fb_depth <= 0)
                    fb_depth = 8;
                out_bits = fb_depth;
            }

            pl_shader_dither(sh, out_bits, &priv->dither_state, &(struct pl_dither_params) {
                .method   = method,
                .lut_size = 4, // avoid too large values, since this gets embedded
            });
        }

        const struct pl_shader_res *res = priv->pl_sh_res = pl_shader_finalize(sh);

        FREENULL(priv->uloc.pl_vars);
        priv->uloc.pl_vars = calloc(res->num_variables, sizeof(GLint));
        for (int i = 0; i < res->num_variables; i++) {
            struct pl_shader_var sv = res->variables[i];
            const char *glsl_type_name = pl_var_glsl_type_name(sv.var);
            ADDF("uniform %s %s", glsl_type_name, sv.var.name);
            if (sv.var.dim_a > 1) {
                ADDF("[%d];\n", sv.var.dim_a);
            } else {
                ADDF(";\n");
            }
        }

        FREENULL(priv->uloc.pl_descs);
        priv->uloc.pl_descs = calloc(res->num_descriptors, sizeof(GLint));
        for (int i = 0; i < res->num_descriptors; i++) {
            struct pl_shader_desc sd = res->descriptors[i];
            assert(sd.desc.type == PL_DESC_SAMPLED_TEX);
            pl_tex tex = sd.binding.object;
            assert(tex->sampler_type == PL_SAMPLER_NORMAL);
            int dims = pl_tex_params_dimension(tex->params);
            ADDF("uniform sampler%dD %s;\n", dims, sd.desc.name);
        }

        // We can't handle these yet, but nothing we use requires them, either
        assert(res->num_vertex_attribs == 0);

        ADD(res->glsl);
    }
#else
    if (fmt->transfer == TRANSFER_FUNC_SMPTE_ST2084 ||
        fmt->primaries == COLOR_PRIMARIES_BT2020)
    {
        // no warning for HLG because it's more or less backwards-compatible
        msg_Warn(priv->gl, "VLC needs to be built with support for libplacebo "
                 "in order to display wide gamut or HDR signals correctly.");
    }
#endif

    if (tex_target == GL_TEXTURE_RECTANGLE)
        ADDF("uniform vec2 TexSizes[%u];\n", tex_count);

    if (is_yuv)
        ADD("uniform mat4 ConvMatrix;\n");

    ADD("vec4 vlc_texture(vec2 tex_coords) {\n");

    unsigned color_count;
    if (is_yuv) {
        ADD(" vec4 pixel = vec4(\n");
        color_count = 0;
        for (unsigned i = 0; i < tex_count; ++i)
        {
            const char *swizzle = swizzle_per_tex[i];
            assert(swizzle);
            color_count += strlen(swizzle);
            assert(color_count < PICTURE_PLANE_MAX);
            if (tex_target == GL_TEXTURE_RECTANGLE)
            {
                /* The coordinates are in texels values, not normalized */
                ADDF("  %s(Textures[%u], TexSizes[%u] * tex_coords).%s,\n", lookup, i, i, swizzle);
            }
            else
            {
                ADDF("  %s(Textures[%u], tex_coords).%s,\n", lookup, i, swizzle);
            }
        }
        ADD("  1.0);\n");
        ADD(" vec4 result = ConvMatrix * pixel;\n");
    }
    else
    {
        if (tex_target == GL_TEXTURE_RECTANGLE)
            ADD(" tex_coords *= TexSizes[0];\n");

        ADDF(" vec4 result = %s(Textures[0], tex_coords);\n", lookup);
        color_count = 1;
    }
    assert(yuv_space == COLOR_SPACE_UNDEF || color_count == 3);

#ifdef HAVE_LIBPLACEBO_GL
    if (priv->pl_sh_res) {
        const struct pl_shader_res *res = priv->pl_sh_res;
        if (res->input != PL_SHADER_SIG_NONE) {
            assert(res->input  == PL_SHADER_SIG_COLOR);
            assert(res->output == PL_SHADER_SIG_COLOR);
            ADDF(" result = %s(result);\n", res->name);
        }
    }
#endif

    ADD(" return result;\n"
        "}\n");

#undef ADD
#undef ADDF

    if (vlc_memstream_close(&ms) != 0)
        return VLC_EGENERIC;

    ret = InitShaderExtensions(sampler, tex_target);
    if (ret != VLC_SUCCESS)
    {
        free(ms.ptr);
        return VLC_EGENERIC;
    }
    sampler->shader.body = ms.ptr;

    static const struct vlc_gl_sampler_ops ops = {
        .fetch_locations = sampler_base_fetch_locations,
        .load = sampler_base_load,
    };
    sampler->ops = &ops;

    return VLC_SUCCESS;
}

struct vlc_gl_sampler *
vlc_gl_sampler_New(struct vlc_gl_t *gl, const struct vlc_gl_api *api,
                   const struct vlc_gl_format *glfmt, bool expose_planes)
{
    struct vlc_gl_sampler_priv *priv = calloc(1, sizeof(*priv));
    if (!priv)
        return NULL;

    struct vlc_gl_sampler *sampler = &priv->sampler;
    vlc_gl_LoadExtensionFunctions(gl, &priv->extension_vt);

    priv->gl = gl;
    priv->api = api;
    priv->vt = &api->vt;

    struct vlc_gl_picture *pic = &priv->pic;
    memcpy(pic->mtx, MATRIX2x3_IDENTITY, sizeof(MATRIX2x3_IDENTITY));
    priv->pic.mtx_has_changed = true;

    sampler->pic_to_tex_matrix = pic->mtx;

    /* Formats with palette are not supported. This also allows to copy
     * video_format_t without possibility of failure. */
    assert(!glfmt->fmt.p_palette);

    sampler->glfmt = *glfmt;

    sampler->shader.extensions = NULL;
    sampler->shader.body = NULL;

#ifdef HAVE_LIBPLACEBO_GL
    priv->uloc.pl_vars = NULL;
    priv->uloc.pl_descs = NULL;
    priv->pl_sh_res = NULL;
    priv->pl_log = vlc_placebo_CreateLog(VLC_OBJECT(gl));
    priv->pl_opengl = pl_opengl_create(priv->pl_log, NULL);
    if (!priv->pl_opengl)
    {
        vlc_gl_sampler_Delete(sampler);
        return NULL;
    }

    priv->pl_sh = pl_shader_alloc(priv->pl_log, &(struct pl_shader_params) {
        .gpu = priv->pl_opengl->gpu,
        .glsl = {
#   ifdef USE_OPENGL_ES2
            .version = 100,
            .gles = true,
#   else
            .version = 120,
#   endif
        },
    });
#endif

    int ret = opengl_fragment_shader_init(sampler, expose_planes);
    if (ret != VLC_SUCCESS)
    {
        vlc_gl_sampler_Delete(sampler);
        return NULL;
    }

    return sampler;
}

void
vlc_gl_sampler_Delete(struct vlc_gl_sampler *sampler)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);

#ifdef HAVE_LIBPLACEBO_GL
    FREENULL(priv->uloc.pl_vars);
    FREENULL(priv->uloc.pl_descs);
    pl_shader_free(&priv->pl_sh);
    pl_shader_obj_destroy(&priv->tone_map_state);
    pl_shader_obj_destroy(&priv->dither_state);
    pl_opengl_destroy(&priv->pl_opengl);
    pl_log_destroy(&priv->pl_log);
#endif

    free(sampler->shader.extensions);
    free(sampler->shader.body);

    free(priv);
}

int
vlc_gl_sampler_Update(struct vlc_gl_sampler *sampler,
                      const struct vlc_gl_picture *picture)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);
    priv->pic = *picture;

    return VLC_SUCCESS;
}

void
vlc_gl_sampler_SelectPlane(struct vlc_gl_sampler *sampler, unsigned plane)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);
    priv->plane = plane;
}
