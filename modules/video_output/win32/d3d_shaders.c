/*****************************************************************************
 * d3d_shaders.c: Direct3D Shader APIs
 *****************************************************************************
 * Copyright (C) 2017-2021 VLC authors and VideoLAN
 *
 * Authors: Steve Lhomme <robux4@gmail.com>
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

#include <assert.h>

#include "common.h"

#include "d3d_shaders.h"

#define SPHERE_RADIUS 1.f

#define SPHERE_SLICES 128
#define nbLatBands SPHERE_SLICES
#define nbLonBands SPHERE_SLICES

float (D3D_GetFormatLuminance)(vlc_object_t *o, const video_format_t *fmt)
{
    switch (fmt->transfer)
    {
        case TRANSFER_FUNC_SMPTE_ST2084:
            /* that's the default PQ value if the metadata are not set */
            return MAX_PQ_BRIGHTNESS;
        case TRANSFER_FUNC_HLG:
            return MAX_HLG_BRIGHTNESS;
        case TRANSFER_FUNC_BT470_BG:
        case TRANSFER_FUNC_BT470_M:
        case TRANSFER_FUNC_BT709:
        case TRANSFER_FUNC_SRGB:
            return DEFAULT_BRIGHTNESS;
        default:
            msg_Dbg(o, "unhandled source transfer %d", fmt->transfer);
            return DEFAULT_BRIGHTNESS;
    }
}

struct xy_primary {
    double x, y;
};

struct cie1931_primaries {
    struct xy_primary red, green, blue, white;
};

static const struct cie1931_primaries STANDARD_PRIMARIES[] = {
#define CIE_D65 {0.31271, 0.32902}
#define CIE_C   {0.31006, 0.31616}

    [COLOR_PRIMARIES_BT601_525] = {
        .red   = {0.630, 0.340},
        .green = {0.310, 0.595},
        .blue  = {0.155, 0.070},
        .white = CIE_D65
    },
    [COLOR_PRIMARIES_BT601_625] = {
        .red   = {0.640, 0.330},
        .green = {0.290, 0.600},
        .blue  = {0.150, 0.060},
        .white = CIE_D65
    },
    [COLOR_PRIMARIES_BT709] = {
        .red   = {0.640, 0.330},
        .green = {0.300, 0.600},
        .blue  = {0.150, 0.060},
        .white = CIE_D65
    },
    [COLOR_PRIMARIES_BT2020] = {
        .red   = {0.708, 0.292},
        .green = {0.170, 0.797},
        .blue  = {0.131, 0.046},
        .white = CIE_D65
    },
    [COLOR_PRIMARIES_DCI_P3] = {
        .red   = {0.680, 0.320},
        .green = {0.265, 0.690},
        .blue  = {0.150, 0.060},
        .white = CIE_D65
    },
    [COLOR_PRIMARIES_FCC1953] = {
        .red   = {0.670, 0.330},
        .green = {0.210, 0.710},
        .blue  = {0.140, 0.080},
        .white = CIE_C
    },
#undef CIE_D65
#undef CIE_C
};

static void ChromaticAdaptation(const struct xy_primary *src_white,
                                const struct xy_primary *dst_white,
                                double in_out[3 * 3])
{
    if (fabs(src_white->x - dst_white->x) < 1e-6 &&
        fabs(src_white->y - dst_white->y) < 1e-6)
        return;

    /* TODO, see http://www.brucelindbloom.com/index.html?Eqn_ChromAdapt.html */
}

static void Float3x3Inverse(double in_out[3 * 3])
{
    double m00 = in_out[0 + 0*3], m01 = in_out[1 + 0*3], m02 = in_out[2 + 0*3],
          m10 = in_out[0 + 1*3], m11 = in_out[1 + 1*3], m12 = in_out[2 + 1*3],
          m20 = in_out[0 + 2*3], m21 = in_out[1 + 2*3], m22 = in_out[2 + 2*3];

    // calculate the adjoint
    in_out[0 + 0*3] =  (m11 * m22 - m21 * m12);
    in_out[1 + 0*3] = -(m01 * m22 - m21 * m02);
    in_out[2 + 0*3] =  (m01 * m12 - m11 * m02);
    in_out[0 + 1*3] = -(m10 * m22 - m20 * m12);
    in_out[1 + 1*3] =  (m00 * m22 - m20 * m02);
    in_out[2 + 1*3] = -(m00 * m12 - m10 * m02);
    in_out[0 + 2*3] =  (m10 * m21 - m20 * m11);
    in_out[1 + 2*3] = -(m00 * m21 - m20 * m01);
    in_out[2 + 2*3] =  (m00 * m11 - m10 * m01);

    // calculate the determinant (as inverse == 1/det * adjoint,
    // adjoint * m == identity * det, so this calculates the det)
    double det = m00 * in_out[0 + 0*3] + m10 * in_out[1 + 0*3] + m20 * in_out[2 + 0*3];
    det = 1.0f / det;

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++)
            in_out[j + i*3] *= det;
    }
}

static void Float3x3Multiply(double m1[3 * 3], const double m2[3 * 3])
{
    double a00 = m1[0 + 0*3], a01 = m1[1 + 0*3], a02 = m1[2 + 0*3],
           a10 = m1[0 + 1*3], a11 = m1[1 + 1*3], a12 = m1[2 + 1*3],
           a20 = m1[0 + 2*3], a21 = m1[1 + 2*3], a22 = m1[2 + 2*3];

    for (int i = 0; i < 3; i++) {
        m1[i + 0*3] = a00 * m2[i + 0*3] + a01 * m2[i + 1*3] + a02 * m2[i + 2*3];
        m1[i + 1*3] = a10 * m2[i + 0*3] + a11 * m2[i + 1*3] + a12 * m2[i + 2*3];
        m1[i + 2*3] = a20 * m2[i + 0*3] + a21 * m2[i + 1*3] + a22 * m2[i + 2*3];
    }
}

static void Float3Multiply(const double in[3], const double mult[3 * 3], double out[3])
{
    for (size_t i=0; i<3; i++)
    {
        out[i] = mult[i + 0*3] * in[0] +
                 mult[i + 1*3] * in[1] +
                 mult[i + 2*3] * in[2];
    }
}

/* from http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html */
static void GetRGB2XYZMatrix(const struct cie1931_primaries *primaries,
                             double out[3 * 3])
{
#define RED   0
#define GREEN 1
#define BLUE  2
    double X[3], Y[3], Z[3], S[3], W[3];
    double W_TO_S[3 * 3];

    X[RED  ] = primaries->red.x / primaries->red.y;
    X[GREEN] = 1;
    X[BLUE ] = (1 - primaries->red.x - primaries->red.y) / primaries->red.y;

    Y[RED  ] = primaries->green.x / primaries->green.y;
    Y[GREEN] = 1;
    Y[BLUE ] = (1 - primaries->green.x - primaries->green.y) / primaries->green.y;

    Z[RED  ] = primaries->blue.x / primaries->blue.y;
    Z[GREEN] = 1;
    Z[BLUE ] = (1 - primaries->blue.x - primaries->blue.y) / primaries->blue.y;

    W_TO_S[0 + 0*3] = X[RED  ];
    W_TO_S[1 + 0*3] = X[GREEN];
    W_TO_S[2 + 0*3] = X[BLUE ];
    W_TO_S[0 + 1*3] = Y[RED  ];
    W_TO_S[1 + 1*3] = Y[GREEN];
    W_TO_S[2 + 1*3] = Y[BLUE ];
    W_TO_S[0 + 2*3] = Z[RED  ];
    W_TO_S[1 + 2*3] = Z[GREEN];
    W_TO_S[2 + 2*3] = Z[BLUE ];

    Float3x3Inverse(W_TO_S);

    W[0] = primaries->white.x / primaries->white.y; /* Xw */
    W[1] = 1;                  /* Yw */
    W[2] = (1 - primaries->white.x - primaries->white.y) / primaries->white.y; /* Yw */

    Float3Multiply(W, W_TO_S, S);

    out[0 + 0*3] = S[RED  ] * X[RED  ];
    out[1 + 0*3] = S[GREEN] * Y[RED  ];
    out[2 + 0*3] = S[BLUE ] * Z[RED  ];
    out[0 + 1*3] = S[RED  ] * X[GREEN];
    out[1 + 1*3] = S[GREEN] * Y[GREEN];
    out[2 + 1*3] = S[BLUE ] * Z[GREEN];
    out[0 + 2*3] = S[RED  ] * X[BLUE ];
    out[1 + 2*3] = S[GREEN] * Y[BLUE ];
    out[2 + 2*3] = S[BLUE ] * Z[BLUE ];
#undef RED
#undef GREEN
#undef BLUE
}

/* from http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html */
static void GetXYZ2RGBMatrix(const struct cie1931_primaries *primaries,
                             double out[3 * 3])
{
    GetRGB2XYZMatrix(primaries, out);
    Float3x3Inverse(out);
}

static void GetPrimariesTransform(FLOAT Primaries[4*3], video_color_primaries_t src,
                                  video_color_primaries_t dst)
{
    const struct cie1931_primaries *p_src = &STANDARD_PRIMARIES[src];
    const struct cie1931_primaries *p_dst = &STANDARD_PRIMARIES[dst];
    double rgb2xyz[3 * 3], xyz2rgb[3 * 3];

    /* src[RGB] -> src[XYZ] */
    GetRGB2XYZMatrix(p_src, rgb2xyz);

    /* src[XYZ] -> dst[XYZ] */
    ChromaticAdaptation(&p_src->white, &p_dst->white, rgb2xyz);

    /* dst[XYZ] -> dst[RGB] */
    GetXYZ2RGBMatrix(p_dst, xyz2rgb);

    /* src[RGB] -> src[XYZ] -> dst[XYZ] -> dst[RGB] */
    Float3x3Multiply(xyz2rgb, rgb2xyz);

    for (size_t i=0;i<3; ++i)
    {
        for (size_t j=0;j<3; ++j)
            Primaries[j + i*4] = xyz2rgb[j + i*3];
        Primaries[3 + i*4] = 0;
    }
}

bool D3D_UpdateQuadOpacity(d3d_quad_t *quad, float opacity)
{
    if (quad->shaderConstants->Opacity == opacity)
        return false;

    quad->shaderConstants->Opacity = opacity;
    return true;
}

bool D3D_UpdateQuadLuminanceScale(d3d_quad_t *quad, float luminanceScale)
{
    if (quad->shaderConstants->LuminanceScale == luminanceScale)
        return false;

    quad->shaderConstants->LuminanceScale = luminanceScale;
    return true;
}

static void MultMat43(FLOAT dst[4*3], const FLOAT left[4*3], const FLOAT right[4*3])
{
    // Cache the invariants in registers
    FLOAT x = left[0*4 + 0];
    FLOAT y = left[0*4 + 1];
    FLOAT z = left[0*4 + 2];
    FLOAT w = left[0*4 + 3];
    // Perform the operation on the first row
    dst[0*4 + 0] = (right[0*4 + 0] * x) + (right[1*4 + 0] * y) + (right[2*4 + 0] * z);
    dst[0*4 + 1] = (right[0*4 + 1] * x) + (right[1*4 + 1] * y) + (right[2*4 + 1] * z);
    dst[0*4 + 2] = (right[0*4 + 2] * x) + (right[1*4 + 2] * y) + (right[2*4 + 2] * z);
    dst[0*4 + 3] = (right[0*4 + 3] * x) + (right[1*4 + 3] * y) + (right[2*4 + 3] * z) + w;
    // Repeat for all the other rows
    x = left[1*4 + 0];
    y = left[1*4 + 1];
    z = left[1*4 + 2];
    w = left[1*4 + 3];
    dst[1*4 + 0] = (right[0*4 + 0] * x) + (right[1*4 + 0] * y) + (right[2*4 + 0] * z);
    dst[1*4 + 1] = (right[0*4 + 1] * x) + (right[1*4 + 1] * y) + (right[2*4 + 1] * z);
    dst[1*4 + 2] = (right[0*4 + 2] * x) + (right[1*4 + 2] * y) + (right[2*4 + 2] * z);
    dst[1*4 + 3] = (right[0*4 + 3] * x) + (right[1*4 + 3] * y) + (right[2*4 + 3] * z) + w;
    x = left[2*4 + 0];
    y = left[2*4 + 1];
    z = left[2*4 + 2];
    w = left[2*4 + 3];
    dst[2*4 + 0] = (right[0*4 + 0] * x) + (right[1*4 + 0] * y) + (right[2*4 + 0] * z);
    dst[2*4 + 1] = (right[0*4 + 1] * x) + (right[1*4 + 1] * y) + (right[2*4 + 1] * z);
    dst[2*4 + 2] = (right[0*4 + 2] * x) + (right[1*4 + 2] * y) + (right[2*4 + 2] * z);
    dst[2*4 + 3] = (right[0*4 + 3] * x) + (right[1*4 + 3] * y) + (right[2*4 + 3] * z) + w;
    // x = left[3*4 + 0];
    // y = left[3*4 + 1];
    // z = left[3*4 + 2];
    // w = left[3*4 + 3];
    // dst[3*4 + 0] = (right[0*4 + 0] * x) + (right[1*4 + 0] * y) + (right[2*4 + 0] * z) + (right[3*4 + 0] * w);
    // dst[3*4 + 1] = (right[0*4 + 1] * x) + (right[1*4 + 1] * y) + (right[2*4 + 1] * z) + (right[3*4 + 1] * w);
    // dst[3*4 + 2] = (right[0*4 + 2] * x) + (right[1*4 + 2] * y) + (right[2*4 + 2] * z) + (right[3*4 + 2] * w);
    // dst[3*4 + 3] = (right[0*4 + 3] * x) + (right[1*4 + 3] * y) + (right[2*4 + 3] * z) + (right[3*4 + 3] * w);
}

void D3D_SetupQuad(vlc_object_t *o, const video_format_t *fmt, d3d_quad_t *quad,
                   const display_info_t *displayFormat)
{
    quad->shaderConstants->LuminanceScale = (float)displayFormat->luminance_peak / D3D_GetFormatLuminance(o, fmt);

    /* pixel shader constant buffer */
    quad->shaderConstants->Opacity = 1.0;
    if (fmt->i_visible_width == fmt->i_width)
        quad->shaderConstants->BoundaryX = 1.0; /* let texture clamping happen */
    else
        quad->shaderConstants->BoundaryX = (FLOAT) (fmt->i_visible_width - 1) / fmt->i_width;
    if (fmt->i_visible_height == fmt->i_height)
        quad->shaderConstants->BoundaryY = 1.0; /* let texture clamping happen */
    else
        quad->shaderConstants->BoundaryY = (FLOAT) (fmt->i_visible_height - 1) / fmt->i_height;

#define COLOR_CONSTANTS_601_KR 0.2990f
#define COLOR_CONSTANTS_601_KB 0.1140f
#define COLOR_CONSTANTS_601_KG (1.0f - COLOR_CONSTANTS_601_KR - COLOR_CONSTANTS_601_KB)

#define COLOR_CONSTANTS_709_KR 0.2126f
#define COLOR_CONSTANTS_709_KB 0.0722f
#define COLOR_CONSTANTS_709_KG (1.0f - COLOR_CONSTANTS_709_KR - COLOR_CONSTANTS_709_KB)

#define COLOR_CONSTANTS_2020_KR 0.2627f
#define COLOR_CONSTANTS_2020_KB 0.0593f
#define COLOR_CONSTANTS_2020_KG (1.0f - COLOR_CONSTANTS_2020_KR - COLOR_CONSTANTS_2020_KB)

#define COLOR_SHIFT_STUDIO_8_MIN_Y   16
#define COLOR_SHIFT_STUDIO_8_MAX_Y   235
#define COLOR_SHIFT_STUDIO_8_MIN_UV  16
#define COLOR_SHIFT_STUDIO_8_MAX_UV  240
#define COLOR_SHIFT_STUDIO_8_UV     ((double)128)
#define COLOR_COEFF_STUDIO_8_Y      ((double)(COLOR_SHIFT_STUDIO_8_MAX_Y - COLOR_SHIFT_STUDIO_8_MIN_Y))
#define COLOR_COEFF_STUDIO_8_UV     ((double)((COLOR_SHIFT_STUDIO_8_MAX_UV - COLOR_SHIFT_STUDIO_8_MIN_UV) / 2))

#define COLOR_SHIFT_STUDIO_10_MIN_Y   64
#define COLOR_SHIFT_STUDIO_10_MAX_Y   940
#define COLOR_SHIFT_STUDIO_10_MIN_UV  64
#define COLOR_SHIFT_STUDIO_10_MAX_UV  960
#define COLOR_SHIFT_STUDIO_10_UV     ((double)512)
#define COLOR_COEFF_STUDIO_10_Y      ((double)(COLOR_SHIFT_STUDIO_10_MAX_Y - COLOR_SHIFT_STUDIO_10_MIN_Y))
#define COLOR_COEFF_STUDIO_10_UV     ((double)((COLOR_SHIFT_STUDIO_10_MAX_UV - COLOR_SHIFT_STUDIO_10_MIN_UV) / 2))

#define COLOR_SHIFT_STUDIO_12_MIN_Y   256
#define COLOR_SHIFT_STUDIO_12_MAX_Y   3760
#define COLOR_SHIFT_STUDIO_12_MIN_UV  256
#define COLOR_SHIFT_STUDIO_12_MAX_UV  3840
#define COLOR_SHIFT_STUDIO_12_UV     ((double)2048)
#define COLOR_COEFF_STUDIO_12_Y      ((double)(COLOR_SHIFT_STUDIO_12_MAX_Y - COLOR_SHIFT_STUDIO_12_MIN_Y))
#define COLOR_COEFF_STUDIO_12_UV     ((double)((COLOR_SHIFT_STUDIO_12_MAX_UV - COLOR_SHIFT_STUDIO_12_MIN_UV) / 2))

#define COLOR_COEFF_FULL_8_RGB      ((double)255)
#define COLOR_COEFF_FULL_10_RGB     ((double)1023)
#define COLOR_COEFF_FULL_12_RGB     ((double)4095)

#define COLOR_SHIFT_FULL_8_MIN_Y   0
#define COLOR_SHIFT_FULL_8_MAX_Y   COLOR_COEFF_FULL_8_RGB
#define COLOR_COEFF_FULL_8_Y      ((double)(COLOR_SHIFT_FULL_8_MAX_Y - COLOR_SHIFT_FULL_8_MIN_Y))
#define COLOR_COEFF_FULL_8_UV     ((double)(COLOR_SHIFT_FULL_8_MAX_Y - COLOR_SHIFT_FULL_8_MIN_Y) / 2.0f)
#define COLOR_SHIFT_FULL_8_UV     ((double)128)

#define COLOR_SHIFT_FULL_10_MIN_Y   0
#define COLOR_SHIFT_FULL_10_MAX_Y   COLOR_COEFF_FULL_10_RGB
#define COLOR_COEFF_FULL_10_Y      ((double)(COLOR_SHIFT_FULL_10_MAX_Y - COLOR_SHIFT_FULL_10_MIN_Y))
#define COLOR_COEFF_FULL_10_UV     ((double)(COLOR_SHIFT_FULL_10_MAX_Y - COLOR_SHIFT_FULL_10_MIN_Y) / 2.0f)
#define COLOR_SHIFT_FULL_10_UV     ((double)512)

#define COLOR_SHIFT_FULL_12_MIN_Y   0
#define COLOR_SHIFT_FULL_12_MAX_Y   COLOR_COEFF_FULL_12_RGB
#define COLOR_COEFF_FULL_12_Y      ((double)(COLOR_SHIFT_FULL_12_MAX_Y - COLOR_SHIFT_FULL_12_MIN_Y))
#define COLOR_COEFF_FULL_12_UV     ((double)(COLOR_SHIFT_FULL_12_MAX_Y - COLOR_SHIFT_FULL_12_MIN_Y) / 2.0f)
#define COLOR_SHIFT_FULL_12_UV     ((double)2048)

#define COLOR_SHIFT_STUDIO_8_MIN_RGB   16
#define COLOR_SHIFT_STUDIO_8_MAX_RGB   235
#define COLOR_COEFF_STUDIO_8_RGB      ((double)(COLOR_SHIFT_STUDIO_8_MAX_RGB - COLOR_SHIFT_STUDIO_8_MIN_RGB))


#define COLOR_MATRIX_YUV2RGB(src, bits, yuv_range, rgb_range) \
    static const FLOAT COLORSPACE_BT##src##_##yuv_range##_##bits##_TO_##rgb_range##_RGBA[4*3] = { \
       COLOR_COEFF_##rgb_range##_##bits##_RGB / COLOR_COEFF_##yuv_range##_##bits##_Y, \
       0.f, \
       COLOR_COEFF_##rgb_range##_##bits##_RGB / COLOR_COEFF_##yuv_range##_##bits##_UV * (1.f - COLOR_CONSTANTS_##src##_KR), \
       0.f, \
       \
       COLOR_COEFF_##rgb_range##_##bits##_RGB / COLOR_COEFF_##yuv_range##_##bits##_Y, \
       - COLOR_COEFF_##rgb_range##_##bits##_RGB / COLOR_COEFF_##yuv_range##_##bits##_UV * (1.f - COLOR_CONSTANTS_##src##_KB) * COLOR_CONSTANTS_##src##_KB / COLOR_CONSTANTS_##src##_KG, \
       - COLOR_COEFF_##rgb_range##_##bits##_RGB / COLOR_COEFF_##yuv_range##_##bits##_UV * (1.f - COLOR_CONSTANTS_##src##_KR) * COLOR_CONSTANTS_##src##_KR / COLOR_CONSTANTS_##src##_KG, \
       0.f, \
       \
       COLOR_COEFF_##rgb_range##_##bits##_RGB / COLOR_COEFF_##yuv_range##_##bits##_Y, \
       COLOR_COEFF_##rgb_range##_##bits##_RGB / COLOR_COEFF_##yuv_range##_##bits##_UV * (1.f - COLOR_CONSTANTS_##src##_KB), \
       0.f, \
       0.f, \
    };

    const bool RGB_src_shader = DxgiIsRGBFormat(quad->textureFormat);

    FLOAT itu_black_level = 0.f;
    FLOAT itu_achromacy   = 0.f;
    if (!RGB_src_shader)
    {
        switch (quad->textureFormat->bitsPerChannel)
        {
        case 8:
            /* Rec. ITU-R BT.709-6 ¶4.6 */
            itu_black_level  =              16.f / 255.f;
            itu_achromacy    =             128.f / 255.f;
            break;
        case 10:
            /* Rec. ITU-R BT.709-6 ¶4.6 */
            itu_black_level  =              64.f / 1023.f;
            itu_achromacy    =             512.f / 1023.f;
            break;
        case 12:
            /* Rec. ITU-R BT.2020-2 Table 5 */
            itu_black_level  =               256.f / 4095.f;
            itu_achromacy    =              2048.f / 4095.f;
            break;
        default:
            /* unknown bitdepth, use approximation for infinite bit depth */
            itu_black_level  =              16.f / 256.f;
            itu_achromacy    =             128.f / 256.f;
            break;
        }
    }

    static const FLOAT IDENTITY_4X3[4 * 3] = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
    };


    COLOR_MATRIX_YUV2RGB(601,8,STUDIO,FULL);
    COLOR_MATRIX_YUV2RGB(709,8,STUDIO,FULL);
    COLOR_MATRIX_YUV2RGB(2020,8,STUDIO,FULL);
    COLOR_MATRIX_YUV2RGB(601,10,STUDIO,FULL);
    COLOR_MATRIX_YUV2RGB(709,10,STUDIO,FULL);
    COLOR_MATRIX_YUV2RGB(2020,10,STUDIO,FULL);
    COLOR_MATRIX_YUV2RGB(601,12,STUDIO,FULL);
    COLOR_MATRIX_YUV2RGB(709,12,STUDIO,FULL);
    COLOR_MATRIX_YUV2RGB(2020,12,STUDIO,FULL);
    COLOR_MATRIX_YUV2RGB(601,8,FULL,FULL);
    COLOR_MATRIX_YUV2RGB(709,8,FULL,FULL);
    COLOR_MATRIX_YUV2RGB(2020,8,FULL,FULL);
    COLOR_MATRIX_YUV2RGB(601,10,FULL,FULL);
    COLOR_MATRIX_YUV2RGB(709,10,FULL,FULL);
    COLOR_MATRIX_YUV2RGB(2020,10,FULL,FULL);
    COLOR_MATRIX_YUV2RGB(601,12,FULL,FULL);
    COLOR_MATRIX_YUV2RGB(709,12,FULL,FULL);
    COLOR_MATRIX_YUV2RGB(2020,12,FULL,FULL);

    static const FLOAT COLORSPACE_FULL_RGBA_TO_BT601_YUV[4*3] = {
        0.299000f,  0.587000f,  0.114000f, 0.f,
       -0.168736f, -0.331264f,  0.500000f, 0.f,
        0.500000f, -0.418688f, -0.081312f, 0.f,
    };

    FLOAT WhitePoint[4*3];
    memcpy(WhitePoint, IDENTITY_4X3, sizeof(WhitePoint));

    const FLOAT *ppColorspace;
    if (RGB_src_shader == DxgiIsRGBFormat(displayFormat->pixelFormat))
    {
        ppColorspace = IDENTITY_4X3;
    }
    else if (RGB_src_shader)
    {
        ppColorspace = COLORSPACE_FULL_RGBA_TO_BT601_YUV;
        WhitePoint[0*4 + 3] = -itu_black_level;
        WhitePoint[1*4 + 3] = itu_achromacy;
        WhitePoint[2*4 + 3] = itu_achromacy;
    }
    else
    {
        switch (fmt->space){
            case COLOR_SPACE_BT709:
                if (fmt->color_range == COLOR_RANGE_FULL)
                {
                    if (quad->textureFormat->bitsPerChannel == 12)
                        ppColorspace = COLORSPACE_BT709_FULL_12_TO_FULL_RGBA;
                    else if (quad->textureFormat->bitsPerChannel == 10)
                        ppColorspace = COLORSPACE_BT709_FULL_10_TO_FULL_RGBA;
                    else
                        ppColorspace = COLORSPACE_BT709_FULL_8_TO_FULL_RGBA;
                }
                else
                {
                    if (quad->textureFormat->bitsPerChannel == 12)
                        ppColorspace = COLORSPACE_BT709_STUDIO_12_TO_FULL_RGBA;
                    else if (quad->textureFormat->bitsPerChannel == 10)
                        ppColorspace = COLORSPACE_BT709_STUDIO_10_TO_FULL_RGBA;
                    else
                        ppColorspace = COLORSPACE_BT709_STUDIO_8_TO_FULL_RGBA;
                }
                break;
            case COLOR_SPACE_BT2020:
                if (fmt->color_range == COLOR_RANGE_FULL)
                {
                    if (quad->textureFormat->bitsPerChannel == 12)
                        ppColorspace = COLORSPACE_BT2020_FULL_12_TO_FULL_RGBA;
                    else if (quad->textureFormat->bitsPerChannel == 10)
                        ppColorspace = COLORSPACE_BT2020_FULL_10_TO_FULL_RGBA;
                    else
                        ppColorspace = COLORSPACE_BT2020_FULL_8_TO_FULL_RGBA;
                }
                else
                {
                    if (quad->textureFormat->bitsPerChannel == 12)
                        ppColorspace = COLORSPACE_BT2020_STUDIO_12_TO_FULL_RGBA;
                    else if (quad->textureFormat->bitsPerChannel == 10)
                        ppColorspace = COLORSPACE_BT2020_STUDIO_10_TO_FULL_RGBA;
                    else
                        ppColorspace = COLORSPACE_BT2020_STUDIO_8_TO_FULL_RGBA;
                }
                break;
            case COLOR_SPACE_BT601:
                if (fmt->color_range == COLOR_RANGE_FULL)
                {
                    if (quad->textureFormat->bitsPerChannel == 12)
                        ppColorspace = COLORSPACE_BT601_FULL_12_TO_FULL_RGBA;
                    else if (quad->textureFormat->bitsPerChannel == 10)
                        ppColorspace = COLORSPACE_BT601_FULL_10_TO_FULL_RGBA;
                    else
                        ppColorspace = COLORSPACE_BT601_FULL_8_TO_FULL_RGBA;
                }
                else
                {
                    if (quad->textureFormat->bitsPerChannel == 12)
                        ppColorspace = COLORSPACE_BT601_STUDIO_12_TO_FULL_RGBA;
                    else if (quad->textureFormat->bitsPerChannel == 10)
                        ppColorspace = COLORSPACE_BT601_STUDIO_10_TO_FULL_RGBA;
                    else
                        ppColorspace = COLORSPACE_BT601_STUDIO_8_TO_FULL_RGBA;
                }
                break;
            default:
            case COLOR_SPACE_UNDEF:
                msg_Warn(o, "unknown colorspace, using BT709");
                ppColorspace = COLORSPACE_BT709_STUDIO_8_TO_FULL_RGBA;
                break;
        }

        /* all matrices work in studio range and output in full range */
        if (fmt->color_range != COLOR_RANGE_FULL)
            WhitePoint[0*4 + 3] = -itu_black_level;
        WhitePoint[1*4 + 3] = -itu_achromacy;
        WhitePoint[2*4 + 3] = -itu_achromacy;
    }

    MultMat43(quad->shaderConstants->Colorspace, ppColorspace, WhitePoint);

    if (fmt->primaries != displayFormat->primaries)
    {
        FLOAT Primaries[4*3];
        GetPrimariesTransform(Primaries, fmt->primaries, displayFormat->primaries);
        MultMat43(quad->shaderConstants->Colorspace, Primaries, quad->shaderConstants->Colorspace);
    }
}

/**
 * Compute the vertex ordering needed to rotate the video. Without
 * rotation, the vertices of the rectangle are defined in a counterclockwise
 * order. This function computes a remapping of the coordinates to
 * implement the rotation, given fixed texture coordinates.
 * The unrotated order is the following:
 * 3--2
 * |  |
 * 0--1
 * For a 180 degrees rotation it should like this:
 * 1--0
 * |  |
 * 2--3
 * Vertex 0 should be assigned coordinates at index 2 from the
 * unrotated order and so on, thus yielding order: 2 3 0 1.
 */
static void orientationVertexOrder(video_transform_t orientation, int vertex_order[static 4])
{
    switch (orientation) {
        case TRANSFORM_R90:
            vertex_order[0] = 3;
            vertex_order[1] = 0;
            vertex_order[2] = 1;
            vertex_order[3] = 2;
            break;
        case TRANSFORM_R270:
            vertex_order[0] = 1;
            vertex_order[1] = 2;
            vertex_order[2] = 3;
            vertex_order[3] = 0;
            break;
        case TRANSFORM_R180:
            vertex_order[0] = 2;
            vertex_order[1] = 3;
            vertex_order[2] = 0;
            vertex_order[3] = 1;
            break;
        case TRANSFORM_TRANSPOSE:
            vertex_order[0] = 2;
            vertex_order[1] = 1;
            vertex_order[2] = 0;
            vertex_order[3] = 3;
            break;
        case TRANSFORM_HFLIP:
            vertex_order[0] = 1;
            vertex_order[1] = 0;
            vertex_order[2] = 3;
            vertex_order[3] = 2;
            break;
        case TRANSFORM_VFLIP:
            vertex_order[0] = 3;
            vertex_order[1] = 2;
            vertex_order[2] = 1;
            vertex_order[3] = 0;
            break;
        case TRANSFORM_ANTI_TRANSPOSE: /* transpose + vflip */
            vertex_order[0] = 0;
            vertex_order[1] = 3;
            vertex_order[2] = 2;
            vertex_order[3] = 1;
            break;
       default:
            vertex_order[0] = 0;
            vertex_order[1] = 1;
            vertex_order[2] = 2;
            vertex_order[3] = 3;
            break;
    }
}

static void SetupQuadFlat(d3d_vertex_t *dst_data, const RECT *output,
                          const d3d_quad_t *quad,
                          WORD *triangle_pos, video_transform_t orientation)
{
    unsigned int src_width = quad->i_width;
    unsigned int src_height = quad->i_height;
    float MidX,MidY;

    float top, bottom, left, right;
    /* find the middle of the visible part of the texture, it will be a 0,0
     * the rest of the visible area must correspond to -1,1 */
    switch (orientation)
    {
    case TRANSFORM_R90: /* 90° anti clockwise */
        /* right/top aligned */
        MidY = (output->left + output->right) / 2.f;
        MidX = (output->top + output->bottom) / 2.f;
        top    =  MidY / (MidY - output->top);
        bottom = -(src_height - MidX) / (MidX - output->top);
        left   =  (MidX - src_height) / (MidX - output->left);
        right  =                 MidX / (MidX - (src_width - output->right));
        break;
    case TRANSFORM_R180: /* 180° */
        /* right/top aligned */
        MidY = (output->top + output->bottom) / 2.f;
        MidX = (output->left + output->right) / 2.f;
        top    =  (src_height - MidY) / (output->bottom - MidY);
        bottom = -MidY / (MidY - output->top);
        left   = -MidX / (MidX - output->left);
        right  =  (src_width  - MidX) / (output->right - MidX);
        break;
    case TRANSFORM_R270: /* 90° clockwise */
        /* right/top aligned */
        MidY = (output->left + output->right) / 2.f;
        MidX = (output->top + output->bottom) / 2.f;
        top    =  (src_width  - MidX) / (output->right - MidX);
        bottom = -MidY / (MidY - output->top);
        left   = -MidX / (MidX - output->left);
        right  =  (src_height - MidY) / (output->bottom - MidY);
        break;
    case TRANSFORM_ANTI_TRANSPOSE:
        MidY = (output->left + output->right) / 2.f;
        MidX = (output->top + output->bottom) / 2.f;
        top    =  (src_width  - MidX) / (output->right - MidX);
        bottom = -MidY / (MidY - output->top);
        left   = -(src_height - MidY) / (output->bottom - MidY);
        right  =  MidX / (MidX - output->left);
        break;
    case TRANSFORM_TRANSPOSE:
        MidY = (output->left + output->right) / 2.f;
        MidX = (output->top + output->bottom) / 2.f;
        top    =  (src_width  - MidX) / (output->right - MidX);
        bottom = -MidY / (MidY - output->top);
        left   = -MidX / (MidX - output->left);
        right  =  (src_height - MidY) / (output->bottom - MidY);
        break;
    case TRANSFORM_VFLIP:
        MidY = (output->top + output->bottom) / 2.f;
        MidX = (output->left + output->right) / 2.f;
        top    =  (src_height - MidY) / (output->bottom - MidY);
        bottom = -MidY / (MidY - output->top);
        left   = -MidX / (MidX - output->left);
        right  =  (src_width  - MidX) / (output->right - MidX);
        break;
    case TRANSFORM_HFLIP:
        MidY = (output->top + output->bottom) / 2.f;
        MidX = (output->left + output->right) / 2.f;
        top    =  MidY / (MidY - output->top);
        bottom = -(src_height - MidY) / (output->bottom - MidY);
        left   = -(src_width  - MidX) / (output->right - MidX);
        right  =  MidX / (MidX - output->left);
        break;
    case TRANSFORM_IDENTITY:
    default:
        /* left/top aligned */
        MidY = (output->top + output->bottom) / 2.f;
        MidX = (output->left + output->right) / 2.f;
        top    =  MidY / (MidY - output->top);
        bottom = -(src_height - MidY) / (output->bottom - MidY);
        left   = -MidX / (MidX - output->left);
        right  =  (src_width  - MidX) / (output->right - MidX);
        break;
    }

    const float vertices_coords[4][2] = {
        { left,  bottom },
        { right, bottom },
        { right, top    },
        { left,  top    },
    };

    /* Compute index remapping necessary to implement the rotation. */
    int vertex_order[4];
    orientationVertexOrder(orientation, vertex_order);

    for (int i = 0; i < 4; ++i) {
        dst_data[i].position.x  = vertices_coords[vertex_order[i]][0];
        dst_data[i].position.y  = vertices_coords[vertex_order[i]][1];
    }

    // bottom left
    dst_data[0].position.z = 0.0f;
    dst_data[0].texture.uv[0] = 0.0f;
    dst_data[0].texture.uv[1] = 1.0f;

    // bottom right
    dst_data[1].position.z = 0.0f;
    dst_data[1].texture.uv[0] = 1.0f;
    dst_data[1].texture.uv[1] = 1.0f;

    // top right
    dst_data[2].position.z = 0.0f;
    dst_data[2].texture.uv[0] = 1.0f;
    dst_data[2].texture.uv[1] = 0.0f;

    // top left
    dst_data[3].position.z = 0.0f;
    dst_data[3].texture.uv[0] = 0.0f;
    dst_data[3].texture.uv[1] = 0.0f;

    /* Make sure surfaces are facing the right way */
    if( orientation == TRANSFORM_HFLIP || orientation == TRANSFORM_VFLIP
     || orientation == TRANSFORM_TRANSPOSE  || orientation == TRANSFORM_ANTI_TRANSPOSE )
    {
        triangle_pos[0] = 0;
        triangle_pos[1] = 1;
        triangle_pos[2] = 3;

        triangle_pos[3] = 3;
        triangle_pos[4] = 1;
        triangle_pos[5] = 2;
    }
    else
    {
        triangle_pos[0] = 3;
        triangle_pos[1] = 1;
        triangle_pos[2] = 0;

        triangle_pos[3] = 2;
        triangle_pos[4] = 1;
        triangle_pos[5] = 3;
    }
}

static void SetupQuadSphere(d3d_vertex_t *dst_data, const RECT *output,
                            const d3d_quad_t *quad, WORD *triangle_pos)
{
    const float scaleX = (float)(RECTWidth(*output))  / quad->i_width;
    const float scaleY = (float)(RECTHeight(*output)) / quad->i_height;
    for (unsigned lat = 0; lat <= nbLatBands; lat++) {
        float theta = lat * (float) M_PI / nbLatBands;
        float sinTheta, cosTheta;

        sincosf(theta, &sinTheta, &cosTheta);

        for (unsigned lon = 0; lon <= nbLonBands; lon++) {
            float phi = lon * 2 * (float) M_PI / nbLonBands;
            float sinPhi, cosPhi;

            sincosf(phi, &sinPhi, &cosPhi);

            float x = -sinPhi * sinTheta;
            float y = cosTheta;
            float z = cosPhi * sinTheta;

            unsigned off1 = lat * (nbLonBands + 1) + lon;
            dst_data[off1].position.x = SPHERE_RADIUS * x;
            dst_data[off1].position.y = SPHERE_RADIUS * y;
            dst_data[off1].position.z = SPHERE_RADIUS * z;

            dst_data[off1].texture.uv[0] = scaleX * lon / (float) nbLonBands; // 0(left) to 1(right)
            dst_data[off1].texture.uv[1] = scaleY * lat / (float) nbLatBands; // 0(top) to 1 (bottom)
        }
    }

    for (unsigned lat = 0; lat < nbLatBands; lat++) {
        for (unsigned lon = 0; lon < nbLonBands; lon++) {
            unsigned first = (lat * (nbLonBands + 1)) + lon;
            unsigned second = first + nbLonBands + 1;

            unsigned off = (lat * nbLatBands + lon) * 3 * 2;

            triangle_pos[off] = first;
            triangle_pos[off + 1] = first + 1;
            triangle_pos[off + 2] = second;

            triangle_pos[off + 3] = second;
            triangle_pos[off + 4] = first + 1;
            triangle_pos[off + 5] = second + 1;
        }
    }
}


static void SetupQuadCube(d3d_vertex_t *dst_data, const RECT *output,
                          const d3d_quad_t *quad, WORD *triangle_pos)
{
#define CUBEFACE(swap, value) \
    swap(value, -1.f,  1.f), \
    swap(value, -1.f, -1.f), \
    swap(value,  1.f,  1.f), \
    swap(value,  1.f, -1.f)

#define X_FACE(v, a, b) (v), (b), (a)
#define Y_FACE(v, a, b) (a), (v), (b)
#define Z_FACE(v, a, b) (a), (b), (v)

    static const float coord[] = {
        CUBEFACE(Z_FACE, -1.f), // FRONT
        CUBEFACE(Z_FACE, +1.f), // BACK
        CUBEFACE(X_FACE, -1.f), // LEFT
        CUBEFACE(X_FACE, +1.f), // RIGHT
        CUBEFACE(Y_FACE, -1.f), // BOTTOM
        CUBEFACE(Y_FACE, +1.f), // TOP
    };

#undef X_FACE
#undef Y_FACE
#undef Z_FACE
#undef CUBEFACE

    const float scaleX = (float)(output->right - output->left) / quad->i_width;
    const float scaleY = (float)(output->bottom - output->top) / quad->i_height;

    const float col[] = {0.f, scaleX / 3, scaleX * 2 / 3, scaleX};
    const float row[] = {0.f, scaleY / 2, scaleY};

    const float tex[] = {
        col[1], row[1], // front
        col[1], row[2],
        col[2], row[1],
        col[2], row[2],

        col[3], row[1], // back
        col[3], row[2],
        col[2], row[1],
        col[2], row[2],

        col[2], row[0], // left
        col[2], row[1],
        col[1], row[0],
        col[1], row[1],

        col[0], row[0], // right
        col[0], row[1],
        col[1], row[0],
        col[1], row[1],

        col[0], row[2], // bottom
        col[0], row[1],
        col[1], row[2],
        col[1], row[1],

        col[2], row[0], // top
        col[2], row[1],
        col[3], row[0],
        col[3], row[1],
    };

    const unsigned i_nbVertices = ARRAY_SIZE(coord) / 3;

    for (unsigned v = 0; v < i_nbVertices; ++v)
    {
        dst_data[v].position.x = coord[3 * v];
        dst_data[v].position.y = coord[3 * v + 1];
        dst_data[v].position.z = coord[3 * v + 2];

        dst_data[v].texture.uv[0] = tex[2 * v];
        dst_data[v].texture.uv[1] = tex[2 * v + 1];
    }

    const WORD ind[] = {
        2, 1, 0,       3, 1, 2, // front
        4, 7, 6,       5, 7, 4, // back
        8, 11, 10,     9, 11, 8, // left
        14, 13, 12,    15, 13, 14, // right
        16, 19, 18,    17, 19, 16, // bottom
        22, 21, 20,    23, 21, 22, // top
    };

    memcpy(triangle_pos, ind, sizeof(ind));
}

static void getZoomMatrix(float zoom, FLOAT matrix[static 16]) {

    const FLOAT m[] = {
        /* x   y     z     w */
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, zoom, 1.0f
    };

    memcpy(matrix, m, sizeof(m));
}

/* perspective matrix see https://www.opengl.org/sdk/docs/man2/xhtml/gluPerspective.xml */
static void getProjectionMatrix(float sar, float fovy, FLOAT matrix[static 16]) {

    float zFar  = 1000;
    float zNear = 0.01;

    float f = 1.f / tanf(fovy / 2.f);

    const FLOAT m[] = {
        f / sar, 0.f,                   0.f,                0.f,
        0.f,     f,                     0.f,                0.f,
        0.f,     0.f,     (zNear + zFar) / (zNear - zFar), -1.f,
        0.f,     0.f, (2 * zNear * zFar) / (zNear - zFar),  0.f};

     memcpy(matrix, m, sizeof(m));
}

static float UpdateFOVy(float f_fovx, float f_sar)
{
    return 2 * atanf(tanf(f_fovx / 2) / f_sar);
}

static float UpdateZ(float f_fovx, float f_fovy)
{
    /* Do trigonometry to calculate the minimal z value
     * that will allow us to zoom out without seeing the outside of the
     * sphere (black borders). */
    float tan_fovx_2 = tanf(f_fovx / 2);
    float tan_fovy_2 = tanf(f_fovy / 2);
    float z_min = - SPHERE_RADIUS / sinf(atanf(sqrtf(
                    tan_fovx_2 * tan_fovx_2 + tan_fovy_2 * tan_fovy_2)));

    /* The FOV value above which z is dynamically calculated. */
    const float z_thresh = 90.f;

    float f_z;
    if (f_fovx <= z_thresh * M_PI / 180)
        f_z = 0;
    else
    {
        float f = z_min / ((FIELD_OF_VIEW_DEGREES_MAX - z_thresh) * M_PI / 180);
        f_z = f * f_fovx - f * z_thresh * M_PI / 180;
        if (f_z < z_min)
            f_z = z_min;
    }
    return f_z;
}

void D3D_UpdateViewpoint(d3d_quad_t *quad, const vlc_viewpoint_t *viewpoint, float f_sar)
{
    // Convert degree into radian
    float f_fovx = viewpoint->fov * (float)M_PI / 180.f;
    if ( f_fovx > FIELD_OF_VIEW_DEGREES_MAX * M_PI / 180 + 0.001f ||
         f_fovx < -0.001f )
        return;

    float f_fovy = UpdateFOVy(f_fovx, f_sar);
    float f_z = UpdateZ(f_fovx, f_fovy);

    getZoomMatrix(SPHERE_RADIUS * f_z, quad->vertexConstants->Zoom);
    getProjectionMatrix(f_sar, f_fovy, quad->vertexConstants->Projection);
    vlc_viewpoint_to_4x4(viewpoint, quad->vertexConstants->View);
}

bool D3D_QuadSetupBuffers(vlc_object_t *o, d3d_quad_t *quad, video_projection_mode_t projection)
{
    switch (projection)
    {
    case PROJECTION_MODE_RECTANGULAR:
        quad->vertexCount = 4;
        quad->indexCount = 2 * 3;
        break;
    case PROJECTION_MODE_EQUIRECTANGULAR:
        quad->vertexCount = (SPHERE_SLICES + 1) * (SPHERE_SLICES + 1);
        quad->indexCount = nbLatBands * nbLonBands * 2 * 3;
        break;
    case PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD:
        quad->vertexCount = 4 * 6;
        quad->indexCount = 6 * 2 * 3;
        break;
    default:
        msg_Warn(o, "Projection mode %d not handled", projection);
        return false;
    }

    quad->vertexStride = sizeof(d3d_vertex_t);
    quad->projection = projection;

    return true;
}

bool D3D_SetupQuadData(vlc_object_t *o, d3d_quad_t *quad, const RECT *output, d3d_vertex_t*dst_data,
                       void *pData, video_transform_t orientation)
{
    switch (quad->projection)
    {
    case PROJECTION_MODE_RECTANGULAR:
        SetupQuadFlat(dst_data, output, quad, pData, orientation);
        break;
    case PROJECTION_MODE_EQUIRECTANGULAR:
        SetupQuadSphere(dst_data, output, quad, pData);
        break;
    case PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD:
        SetupQuadCube(dst_data, output, quad, pData);
        break;
    default:
        msg_Warn(o, "Projection mode %d not handled", quad->projection);
        return false;
    }
    return true;
}
