/*****************************************************************************
 * d3d11_shaders.c: Direct3D11 Shaders
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
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

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0601 // _WIN32_WINNT_WIN7
# undef _WIN32_WINNT
# define _WIN32_WINNT 0x0601 // _WIN32_WINNT_WIN7
#endif

#include <vlc_common.h>

#include <assert.h>

#define COBJMACROS
#include <d3d11.h>

#include "d3d11_shaders.h"

#if !VLC_WINSTORE_APP
# define D3DCompile(args...)                    shaders->OurD3DCompile(args)
#endif

#define ST2084_PQ_CONSTANTS  "const float ST2084_m1 = 2610.0 / (4096.0 * 4);\n\
const float ST2084_m2 = (2523.0 / 4096.0) * 128.0;\n\
const float ST2084_c1 = 3424.0 / 4096.0;\n\
const float ST2084_c2 = (2413.0 / 4096.0) * 32.0;\n\
const float ST2084_c3 = (2392.0 / 4096.0) * 32.0;\n"

#define STRINGIZE2(s) #s
#define STRINGIZE(s) STRINGIZE2(s)

static const char* globPixelShaderDefault = "\
  cbuffer PS_CONSTANT_BUFFER : register(b0)\n\
  {\n\
    float Opacity;\n\
    float BoundaryX;\n\
    float BoundaryY;\n\
    float LuminanceScale;\n\
  };\n\
  cbuffer PS_COLOR_TRANSFORM : register(b1)\n\
  {\n\
    float4x4 WhitePoint;\n\
    float4x4 Colorspace;\n\
    float4x4 Primaries;\n\
  };\n\
  Texture2D%s shaderTexture[" STRINGIZE(D3D11_MAX_SHADER_VIEW) "];\n\
  SamplerState SamplerStates[2];\n\
  \n\
  struct PS_INPUT\n\
  {\n\
    float4 Position   : SV_POSITION;\n\
    float3 Texture    : TEXCOORD0;\n\
  };\n\
  \n\
  /* see http://filmicworlds.com/blog/filmic-tonemapping-operators/ */\n\
  inline float4 hable(float4 x) {\n\
      const float A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30;\n\
      return ((x * (A*x + (C*B))+(D*E))/(x * (A*x + B) + (D*F))) - E/F;\n\
  }\n\
  \n\
  /* https://en.wikipedia.org/wiki/Hybrid_Log-Gamma#Technical_details */\n\
  inline float inverse_HLG(float x){\n\
      const float B67_a = 0.17883277;\n\
      const float B67_b = 0.28466892;\n\
      const float B67_c = 0.55991073;\n\
      const float B67_inv_r2 = 4.0; /* 1/0.5² */\n\
      if (x <= 0.5)\n\
          x = x * x * B67_inv_r2;\n\
      else\n\
          x = exp((x - B67_c) / B67_a) + B67_b;\n\
      return x;\n\
  }\n\
  \n\
  inline float4 sourceToLinear(float4 rgb) {\n\
      %s;\n\
  }\n\
  \n\
  inline float4 linearToDisplay(float4 rgb) {\n\
      %s;\n\
  }\n\
  \n\
  inline float4 transformPrimaries(float4 rgb) {\n\
      %s;\n\
  }\n\
  \n\
  inline float4 toneMapping(float4 rgb) {\n\
      %s;\n\
  }\n\
  \n\
  inline float4 adjustRange(float4 rgb) {\n\
      %s;\n\
  }\n\
  \n\
  inline float4 reorderPlanes(float4 rgb) {\n\
      %s;\n\
  }\n\
  \n\
  inline float4 sampleTexture(SamplerState samplerState, float3 coords) {\n\
      float4 sample;\n\
      %s /* sampling routine in sample */\n\
      return sample;\n\
  }\n\
  \n\
  float4 main( PS_INPUT In ) : SV_TARGET\n\
  {\n\
    float4 sample;\n\
    \n\
    if (In.Texture.x > BoundaryX || In.Texture.y > BoundaryY) \n\
        sample = sampleTexture( SamplerStates[1], In.Texture );\n\
    else\n\
        sample = sampleTexture( SamplerStates[0], In.Texture );\n\
    float4 rgba = max(mul(mul(sample, WhitePoint), Colorspace),0);\n\
    float opacity = rgba.a * Opacity;\n\
    float4 rgb = rgba; rgb.a = 0;\n\
    rgb = sourceToLinear(rgb);\n\
    rgb = transformPrimaries(rgb);\n\
    rgb = toneMapping(rgb);\n\
    rgb = linearToDisplay(rgb);\n\
    rgb = adjustRange(rgb);\n\
    rgb = reorderPlanes(rgb);\n\
    return float4(rgb.rgb, saturate(opacity));\n\
  }\n\
";

const char* globVertexShaderFlat = "\
struct VS_INPUT\n\
{\n\
  float4 Position   : POSITION;\n\
  float4 Texture    : TEXCOORD0;\n\
};\n\
\n\
struct VS_OUTPUT\n\
{\n\
  float4 Position   : SV_POSITION;\n\
  float4 Texture    : TEXCOORD0;\n\
};\n\
\n\
VS_OUTPUT main( VS_INPUT In )\n\
{\n\
  return In;\n\
}\n\
";

const char* globVertexShaderProjection = "\n\
cbuffer VS_PROJECTION_CONST : register(b0)\n\
{\n\
   float4x4 View;\n\
   float4x4 Zoom;\n\
   float4x4 Projection;\n\
};\n\
struct VS_INPUT\n\
{\n\
  float4 Position   : POSITION;\n\
  float4 Texture    : TEXCOORD0;\n\
};\n\
\n\
struct VS_OUTPUT\n\
{\n\
  float4 Position   : SV_POSITION;\n\
  float4 Texture    : TEXCOORD0;\n\
};\n\
\n\
VS_OUTPUT main( VS_INPUT In )\n\
{\n\
  VS_OUTPUT Output;\n\
  float4 pos = In.Position;\n\
  pos = mul(View, pos);\n\
  pos = mul(Zoom, pos);\n\
  pos = mul(Projection, pos);\n\
  Output.Position = pos;\n\
  Output.Texture = In.Texture;\n\
  return Output;\n\
}\n\
";

static ID3DBlob* D3D11_CompileShader(vlc_object_t *, const d3d11_shaders_t *, const d3d11_device_t *,
                              const char *psz_shader, bool pixel);

bool IsRGBShader(const d3d_format_t *cfg)
{
    return cfg->resourceFormat[0] != DXGI_FORMAT_R8_UNORM &&
           cfg->resourceFormat[0] != DXGI_FORMAT_R16_UNORM &&
           cfg->formatTexture != DXGI_FORMAT_YUY2 &&
           cfg->formatTexture != DXGI_FORMAT_AYUV &&
           cfg->formatTexture != DXGI_FORMAT_Y210 &&
           cfg->formatTexture != DXGI_FORMAT_Y410 &&
           cfg->formatTexture != DXGI_FORMAT_420_OPAQUE;
}

static HRESULT CompileTargetShader(vlc_object_t *o, const d3d11_shaders_t *shaders, bool legacy_shader,
                                   d3d11_device_t *d3d_dev,
                                   const char *psz_sampler,
                                   const char *psz_src_to_linear,
                                   const char *psz_primaries_transform,
                                   const char *psz_linear_to_display,
                                   const char *psz_tone_mapping,
                                   const char *psz_adjust_range, const char *psz_move_planes,
                                   ID3D11PixelShader **output)
{
    char *shader;
    int allocated = asprintf(&shader, globPixelShaderDefault, legacy_shader ? "" : "Array",
                             psz_src_to_linear, psz_linear_to_display,
                             psz_primaries_transform, psz_tone_mapping,
                             psz_adjust_range, psz_move_planes, psz_sampler);
    if (allocated <= 0)
    {
        msg_Err(o, "no room for the Pixel Shader");
        return E_OUTOFMEMORY;
    }
    if (var_InheritInteger(o, "verbose") >= 4)
        msg_Dbg(o, "shader %s", shader);
#ifndef NDEBUG
    else {
    msg_Dbg(o,"psz_src_to_linear %s", psz_src_to_linear);
    msg_Dbg(o,"psz_primaries_transform %s", psz_primaries_transform);
    msg_Dbg(o,"psz_tone_mapping %s", psz_tone_mapping);
    msg_Dbg(o,"psz_linear_to_display %s", psz_linear_to_display);
    msg_Dbg(o,"psz_adjust_range %s", psz_adjust_range);
    msg_Dbg(o,"psz_sampler %s", psz_sampler);
    msg_Dbg(o,"psz_move_planes %s", psz_move_planes);
    }
#endif

    ID3DBlob *pPSBlob = D3D11_CompileShader(o, shaders, d3d_dev, shader, true);
    free(shader);
    if (!pPSBlob)
        return E_INVALIDARG;

    HRESULT hr = ID3D11Device_CreatePixelShader(d3d_dev->d3ddevice,
                                                (void *)ID3D10Blob_GetBufferPointer(pPSBlob),
                                                ID3D10Blob_GetBufferSize(pPSBlob), NULL, output);

    ID3D10Blob_Release(pPSBlob);
    return hr;
}

HRESULT (D3D11_CompilePixelShader)(vlc_object_t *o, const d3d11_shaders_t *shaders, bool legacy_shader,
                                 d3d11_device_t *d3d_dev,
                                 const display_info_t *display,
                                 video_transfer_func_t transfer,
                                 video_color_primaries_t primaries, bool src_full_range,
                                 d3d_quad_t *quad)
{
    static const char *DEFAULT_NOOP = "return rgb";
    const char *psz_sampler[2] = {NULL, NULL};
    const char *psz_src_to_linear     = DEFAULT_NOOP;
    const char *psz_linear_to_display = DEFAULT_NOOP;
    const char *psz_primaries_transform = DEFAULT_NOOP;
    const char *psz_tone_mapping      = "return rgb * LuminanceScale";
    const char *psz_adjust_range      = DEFAULT_NOOP;
    const char *psz_move_planes[2]    = {DEFAULT_NOOP, DEFAULT_NOOP};
    char *psz_range = NULL;

    D3D11_SAMPLER_DESC sampDesc;
    memset(&sampDesc, 0, sizeof(sampDesc));
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr;
    hr = ID3D11Device_CreateSamplerState(d3d_dev->d3ddevice, &sampDesc, &quad->d3dsampState[0]);
    if (FAILED(hr)) {
        msg_Err(o, "Could not Create the D3d11 Sampler State. (hr=0x%lX)", hr);
        return hr;
    }

    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    hr = ID3D11Device_CreateSamplerState(d3d_dev->d3ddevice, &sampDesc, &quad->d3dsampState[1]);
    if (FAILED(hr)) {
        msg_Err(o, "Could not Create the D3d11 Sampler State. (hr=0x%lX)", hr);
        ID3D11SamplerState_Release(quad->d3dsampState[0]);
        return hr;
    }

    if ( display->pixelFormat->formatTexture == DXGI_FORMAT_NV12 ||
         display->pixelFormat->formatTexture == DXGI_FORMAT_P010 )
    {
        /* we need 2 shaders, one for the Y target, one for the UV target */
        switch (quad->textureFormat->formatTexture)
        {
        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_P010:
            psz_sampler[0] =
                    "sample.x  = shaderTexture[0].Sample(samplerState, coords).x;\n"
                    "sample.y = 0.0;\n"
                    "sample.z = 0.0;\n"
                    "sample.a = 1;";
            psz_sampler[1] =
                    "sample.xy  = shaderTexture[1].Sample(samplerState, coords).xy;\n"
                    "sample.z = 0.0;\n"
                    "sample.a = 1;";
            break;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_B5G6R5_UNORM:
            /* Y */
            psz_sampler[0] =
                    "sample = shaderTexture[0].Sample(samplerState, coords);\n";
            psz_move_planes[0] = "return rgb";
            /* UV */
            psz_sampler[1] =
                    "sample = shaderTexture[0].Sample(samplerState, coords);\n";
            psz_move_planes[1] =
                    "rgb.x = rgb.y;\n"
                    "rgb.y = rgb.z;\n"
                    "rgb.z = 0;\n"
                    "return rgb";
            break;
        case DXGI_FORMAT_UNKNOWN:
            switch (quad->textureFormat->fourcc)
            {
            case VLC_CODEC_YUVA:
                /* Y */
                psz_sampler[0] =
                        "sample.x = shaderTexture[0].Sample(samplerState, coords).x;\n"
                        "sample.y = 0.0;\n"
                        "sample.z = 0.0;\n"
                        "sample.a = shaderTexture[3].Sample(samplerState, coords).x;";
                /* UV */
                psz_sampler[1] =
                        "sample.x = shaderTexture[1].Sample(samplerState, coords).x;\n"
                        "sample.y = shaderTexture[2].Sample(samplerState, coords).x;\n"
                        "sample.z = 0.0;\n"
                        "sample.a = shaderTexture[3].Sample(samplerState, coords).x;";
                break;
            default:
                vlc_assert_unreachable();
            }
            break;
        default:
            vlc_assert_unreachable();
        }
    }
    else
    {
        switch (quad->textureFormat->formatTexture)
        {
        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_P010:
            psz_sampler[0] =
                    "sample.x  = shaderTexture[0].Sample(samplerState, coords).x;\n"
                    "sample.yz = shaderTexture[1].Sample(samplerState, coords).xy;\n"
                    "sample.a  = 1;";
            break;
        case DXGI_FORMAT_YUY2:
            psz_sampler[0] =
                    "sample.x  = shaderTexture[0].Sample(samplerState, coords).x;\n"
                    "sample.y  = shaderTexture[0].Sample(samplerState, coords).y;\n"
                    "sample.z  = shaderTexture[0].Sample(samplerState, coords).a;\n"
                    "sample.a  = 1;";
            break;
        case DXGI_FORMAT_Y210:
            psz_sampler[0] =
                    "sample.x  = shaderTexture[0].Sample(samplerState, coords).r;\n"
                    "sample.y  = shaderTexture[0].Sample(samplerState, coords).g;\n"
                    "sample.z  = shaderTexture[0].Sample(samplerState, coords).a;\n"
                    "sample.a  = 1;";
            break;
        case DXGI_FORMAT_Y410:
            psz_sampler[0] =
                    "sample.x  = shaderTexture[0].Sample(samplerState, coords).g;\n"
                    "sample.y  = shaderTexture[0].Sample(samplerState, coords).r;\n"
                    "sample.z  = shaderTexture[0].Sample(samplerState, coords).b;\n"
                    "sample.a  = 1;";
            break;
        case DXGI_FORMAT_AYUV:
            psz_sampler[0] =
                    "sample.x  = shaderTexture[0].Sample(samplerState, coords).z;\n"
                    "sample.y  = shaderTexture[0].Sample(samplerState, coords).y;\n"
                    "sample.z  = shaderTexture[0].Sample(samplerState, coords).x;\n"
                    "sample.a  = 1;";
            break;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_B5G6R5_UNORM:
            psz_sampler[0] =
                    "sample = shaderTexture[0].Sample(samplerState, coords);";
            break;
        case DXGI_FORMAT_UNKNOWN:
            switch (quad->textureFormat->fourcc)
            {
            case VLC_CODEC_I420_10L:
                psz_sampler[0] =
                       "float3 coords_2 = float3(coords.x/2, coords.y, coords.z);\n"
                       "sample.x  = shaderTexture[0].Sample(samplerState, coords_2).x * 64;\n"
                       "sample.y  = shaderTexture[1].Sample(samplerState, coords_2).x * 64;\n"
                       "sample.z  = shaderTexture[2].Sample(samplerState, coords_2).x * 64;\n"
                       "sample.a  = 1;";
                break;
            case VLC_CODEC_I444_16L:
                psz_sampler[0] =
                       "float3 coords_2 = float3(coords.x/2, coords.y, coords.z);\n"
                       "sample.x  = shaderTexture[0].Sample(samplerState, coords_2).x;\n"
                       "sample.y  = shaderTexture[1].Sample(samplerState, coords_2).x;\n"
                       "sample.z  = shaderTexture[2].Sample(samplerState, coords_2).x;\n"
                       "sample.a  = 1;";
                break;
            case VLC_CODEC_I420:
                psz_sampler[0] =
                       "sample.x  = shaderTexture[0].Sample(samplerState, coords).x;\n"
                       "sample.y  = shaderTexture[1].Sample(samplerState, coords).x;\n"
                       "sample.z  = shaderTexture[2].Sample(samplerState, coords).x;\n"
                       "sample.a  = 1;";
                break;
            case VLC_CODEC_YUVA:
                psz_sampler[0] =
                       "sample.x  = shaderTexture[0].Sample(samplerState, coords).x;\n"
                       "sample.y  = shaderTexture[1].Sample(samplerState, coords).x;\n"
                       "sample.z  = shaderTexture[2].Sample(samplerState, coords).x;\n"
                       "sample.a  = shaderTexture[3].Sample(samplerState, coords).x;";
                break;
            default:
                vlc_assert_unreachable();
            }
            break;
        default:
            vlc_assert_unreachable();
        }
    }

    video_transfer_func_t src_transfer;

    if (transfer != display->transfer)
    {
        /* we need to go in linear mode */
        switch (transfer)
        {
            case TRANSFER_FUNC_SMPTE_ST2084:
                /* ST2084 to Linear */
                psz_src_to_linear =
                       ST2084_PQ_CONSTANTS
                       "rgb = pow(max(rgb, 0), 1.0/ST2084_m2);\n"
                       "rgb = max(rgb - ST2084_c1, 0.0) / (ST2084_c2 - ST2084_c3 * rgb);\n"
                       "rgb = pow(rgb, 1.0/ST2084_m1);\n"
                       "return rgb * 10000";
                src_transfer = TRANSFER_FUNC_LINEAR;
                break;
            case TRANSFER_FUNC_HLG:
                psz_src_to_linear = "const float alpha_gain = 2000; /* depends on the display output */\n"
                                    "/* TODO: in one call */\n"
                                    "rgb.r = inverse_HLG(rgb.r);\n"
                                    "rgb.g = inverse_HLG(rgb.g);\n"
                                    "rgb.b = inverse_HLG(rgb.b);\n"
                                    "float3 ootf_2020 = float3(0.2627, 0.6780, 0.0593);\n"
                                    "float ootf_ys = alpha_gain * dot(ootf_2020, rgb);\n"
                                    "return rgb * pow(ootf_ys, 0.200)";
                src_transfer = TRANSFER_FUNC_LINEAR;
                break;
            case TRANSFER_FUNC_BT709:
                psz_src_to_linear = "return pow(rgb, 1.0 / 0.45)";
                src_transfer = TRANSFER_FUNC_LINEAR;
                break;
            case TRANSFER_FUNC_BT470_M:
            case TRANSFER_FUNC_SRGB:
                psz_src_to_linear = "return pow(rgb, 2.2)";
                src_transfer = TRANSFER_FUNC_LINEAR;
                break;
            case TRANSFER_FUNC_BT470_BG:
                psz_src_to_linear = "return pow(rgb, 2.8)";
                src_transfer = TRANSFER_FUNC_LINEAR;
                break;
            default:
                msg_Dbg(o, "unhandled source transfer %d", transfer);
                src_transfer = transfer;
                break;
        }

        switch (display->transfer)
        {
            case TRANSFER_FUNC_SRGB:
                if (src_transfer == TRANSFER_FUNC_LINEAR)
                {
                    /* Linear to sRGB */
                    psz_linear_to_display = "return pow(rgb, 1.0 / 2.2)";

                    if (transfer == TRANSFER_FUNC_SMPTE_ST2084 || transfer == TRANSFER_FUNC_HLG)
                    {
                        /* HDR tone mapping */
                        psz_tone_mapping =
                            "static const float4 HABLE_DIV = hable(11.2);\n"
                            "rgb = hable(rgb * LuminanceScale) / HABLE_DIV;\n"
                            "return rgb";
                    }
                }
                else
                    msg_Warn(o, "don't know how to transfer from %d to sRGB", src_transfer);
                break;

            case TRANSFER_FUNC_SMPTE_ST2084:
                if (src_transfer == TRANSFER_FUNC_LINEAR)
                {
                    /* Linear to ST2084 */
                    psz_linear_to_display =
                           ST2084_PQ_CONSTANTS
                           "rgb = pow(rgb / 10000, ST2084_m1);\n"
                           "rgb = (ST2084_c1 + ST2084_c2 * rgb) / (1 + ST2084_c3 * rgb);\n"
                           "rgb = pow(rgb, ST2084_m2);\n"
                           "return rgb";
                }
                else
                    msg_Warn(o, "don't know how to transfer from %d to SMPTE ST 2084", src_transfer);
                break;
            default:
                msg_Warn(o, "don't know how to transfer from %d to %d", src_transfer, display->transfer);
                break;
        }
    }

    if (display->primaries != primaries)
    {
        switch (primaries)
        {
        case COLOR_PRIMARIES_BT601_525:
        case COLOR_PRIMARIES_BT601_625:
        case COLOR_PRIMARIES_BT709:
        case COLOR_PRIMARIES_BT2020:
        case COLOR_PRIMARIES_DCI_P3:
        case COLOR_PRIMARIES_FCC1953:
            psz_primaries_transform = "return max(mul(rgb, Primaries), 0)";
            break;
        default:
            /* see STANDARD_PRIMARIES */
            msg_Warn(o, "unhandled color primaries %d", primaries);
        }
    }

    int range_adjust = 0;
    if (display->b_full_range) {
        if (!src_full_range)
            range_adjust = 1; /* raise the source to full range */
    } else {
        if (src_full_range)
            range_adjust = -1; /* lower the source to studio range */
    }
    if (!IsRGBShader(quad->textureFormat) && !src_full_range)
        range_adjust--; /* the YUV->RGB conversion already output full range */

    if (range_adjust != 0)
    {
        psz_range = malloc(256);
        if (likely(psz_range))
        {
            FLOAT itu_black_level;
            FLOAT itu_range_factor;
            FLOAT itu_white_level;
            switch (quad->textureFormat->bitsPerChannel)
            {
            case 8:
                /* Rec. ITU-R BT.709-6 §4.6 */
                itu_black_level  =              16.f / 255.f;
                itu_white_level  =             235.f / 255.f;
                itu_range_factor = (float)(235 - 16) / 255.f;
                break;
            case 10:
                /* Rec. ITU-R BT.709-6 §4.6 */
                itu_black_level  =              64.f / 1023.f;
                itu_white_level  =             940.f / 1023.f;
                itu_range_factor = (float)(940 - 64) / 1023.f;
                break;
            case 12:
                /* Rec. ITU-R BT.2020-2 Table 5 */
                itu_black_level  =               256.f / 4095.f;
                itu_white_level  =              3760.f / 4095.f;
                itu_range_factor = (float)(3760 - 256) / 4095.f;
                break;
            default:
                /* unknown bitdepth, use approximation for infinite bit depth */
                itu_black_level  =              16.f / 256.f;
                itu_white_level  =             235.f / 256.f;
                itu_range_factor = (float)(235 - 16) / 256.f;
                break;
            }

            FLOAT black_level = 0;
            FLOAT range_factor = 1.0f;
            if (range_adjust > 0)
            {
                /* expand the range from studio to full range */
                while (range_adjust--)
                {
                    black_level -= itu_black_level;
                    range_factor /= itu_range_factor;
                }
                sprintf(psz_range, "return clamp((rgb + %f) * %f, 0, 1)",
                        black_level, range_factor);
            }
            else
            {
                /* shrink the range to studio range */
                while (range_adjust++)
                {
                    black_level += itu_black_level;
                    range_factor *= itu_range_factor;
                }
                sprintf(psz_range, "return clamp(rgb + %f * %f,%f,%f)",
                        black_level, range_factor, itu_black_level, itu_white_level);
            }
            psz_adjust_range = psz_range;
        }
    }

    hr = CompileTargetShader(o, shaders, legacy_shader, d3d_dev,
                                     psz_sampler[0], psz_src_to_linear,
                                     psz_primaries_transform,
                                     psz_linear_to_display, psz_tone_mapping,
                                     psz_adjust_range, psz_move_planes[0], &quad->d3dpixelShader[0]);
    if (!FAILED(hr) && psz_sampler[1])
        hr = CompileTargetShader(o, shaders, legacy_shader, d3d_dev,
                                 psz_sampler[1], psz_src_to_linear,
                                 psz_primaries_transform,
                                 psz_linear_to_display, psz_tone_mapping,
                                 psz_adjust_range, psz_move_planes[1], &quad->d3dpixelShader[1]);
    free(psz_range);

    return hr;
}

void D3D11_ReleasePixelShader(d3d_quad_t *quad)
{
    for (size_t i=0; i<D3D11_MAX_SHADER_VIEW; i++)
    {
        if (quad->d3dpixelShader[i])
        {
            ID3D11PixelShader_Release(quad->d3dpixelShader[i]);
            quad->d3dpixelShader[i] = NULL;
        }
    }
}

static ID3DBlob* D3D11_CompileShader(vlc_object_t *obj, const d3d11_shaders_t *shaders,
                                     const d3d11_device_t *d3d_dev,
                                     const char *psz_shader, bool pixel)
{
    ID3DBlob* pShaderBlob = NULL, *pErrBlob;
    const char *target;
    if (pixel)
    {
        if (likely(d3d_dev->feature_level >= D3D_FEATURE_LEVEL_10_0))
            target = "ps_4_0";
        else if (d3d_dev->feature_level >= D3D_FEATURE_LEVEL_9_3)
            target = "ps_4_0_level_9_3";
        else
            target = "ps_4_0_level_9_1";
    }
    else
    {
        if (likely(d3d_dev->feature_level >= D3D_FEATURE_LEVEL_10_0))
            target = "vs_4_0";
        else if (d3d_dev->feature_level >= D3D_FEATURE_LEVEL_9_3)
            target = "vs_4_0_level_9_3";
        else
            target = "vs_4_0_level_9_1";
    }

    HRESULT hr = D3DCompile(psz_shader, strlen(psz_shader),
                            NULL, NULL, NULL, "main", target,
                            0, 0, &pShaderBlob, &pErrBlob);

    if (FAILED(hr)) {
        char *err = pErrBlob ? ID3D10Blob_GetBufferPointer(pErrBlob) : NULL;
        msg_Err(obj, "invalid %s Shader (hr=0x%lX): %s", pixel?"Pixel":"Vertex", hr, err );
        if (pErrBlob)
            ID3D10Blob_Release(pErrBlob);
        return NULL;
    }
    return pShaderBlob;
}

#undef GetFormatLuminance
float GetFormatLuminance(vlc_object_t *o, const video_format_t *fmt)
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

HRESULT D3D11_CreateRenderTargets( d3d11_device_t *d3d_dev, ID3D11Resource *texture,
                                   const d3d_format_t *cfg, ID3D11RenderTargetView *output[D3D11_MAX_RENDER_TARGET] )
{
    D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
    renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    renderTargetViewDesc.Texture2D.MipSlice = 0;

    for (size_t i=0; i<D3D11_MAX_SHADER_VIEW; i++)
    {
        if (cfg->resourceFormat[i])
        {
            renderTargetViewDesc.Format = cfg->resourceFormat[i];
            HRESULT hr = ID3D11Device_CreateRenderTargetView(d3d_dev->d3ddevice, texture,
                                                             &renderTargetViewDesc, &output[i]);
            if (FAILED(hr))
            {
                return hr;
            }
        }
    }
    return S_OK;
}

void D3D11_ClearRenderTargets(d3d11_device_t *d3d_dev, const d3d_format_t *cfg,
                              ID3D11RenderTargetView *targets[D3D11_MAX_RENDER_TARGET])
{
    static const FLOAT blackY[1] = {0.0f};
    static const FLOAT blackUV[2] = {0.5f, 0.5f};
    static const FLOAT blackRGBA[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    static const FLOAT blackYUY2[4] = {0.0f, 0.5f, 0.0f, 0.5f};
    static const FLOAT blackVUYA[4] = {0.5f, 0.5f, 0.0f, 1.0f};
    static const FLOAT blackY210[4] = {0.0f, 0.5f, 0.5f, 0.0f};

    static_assert(D3D11_MAX_RENDER_TARGET >= 2, "we need at least 2 RenderTargetView for NV12/P010");

    switch (cfg->formatTexture)
    {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
        ID3D11DeviceContext_ClearRenderTargetView( d3d_dev->d3dcontext, targets[0], blackY);
        ID3D11DeviceContext_ClearRenderTargetView( d3d_dev->d3dcontext, targets[1], blackUV);
        break;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_B5G6R5_UNORM:
        ID3D11DeviceContext_ClearRenderTargetView( d3d_dev->d3dcontext, targets[0], blackRGBA);
        break;
    case DXGI_FORMAT_YUY2:
        ID3D11DeviceContext_ClearRenderTargetView( d3d_dev->d3dcontext, targets[0], blackYUY2);
        break;
    case DXGI_FORMAT_Y410:
        ID3D11DeviceContext_ClearRenderTargetView( d3d_dev->d3dcontext, targets[0], blackVUYA);
        break;
    case DXGI_FORMAT_Y210:
        ID3D11DeviceContext_ClearRenderTargetView( d3d_dev->d3dcontext, targets[0], blackY210);
        break;
    case DXGI_FORMAT_AYUV:
        ID3D11DeviceContext_ClearRenderTargetView( d3d_dev->d3dcontext, targets[0], blackVUYA);
        break;
    default:
        vlc_assert_unreachable();
    }
}

static HRESULT D3D11_CompileVertexShader(vlc_object_t *obj, const d3d11_shaders_t *shaders,
                                         d3d11_device_t *d3d_dev, const char *psz_shader,
                                         d3d_vshader_t *output)
{
   HRESULT hr = E_FAIL;
   ID3DBlob *pVSBlob = D3D11_CompileShader(obj, shaders, d3d_dev, psz_shader, false);
   if (!pVSBlob)
       goto error;

   hr = ID3D11Device_CreateVertexShader(d3d_dev->d3ddevice, (void *)ID3D10Blob_GetBufferPointer(pVSBlob),
                                        ID3D10Blob_GetBufferSize(pVSBlob), NULL, &output->shader);

   if(FAILED(hr)) {
       msg_Err(obj, "Failed to create the flat vertex shader. (hr=0x%lX)", hr);
       goto error;
   }

   static D3D11_INPUT_ELEMENT_DESC layout[] = {
   { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
   { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
   };

   hr = ID3D11Device_CreateInputLayout(d3d_dev->d3ddevice, layout, 2, (void *)ID3D10Blob_GetBufferPointer(pVSBlob),
                                       ID3D10Blob_GetBufferSize(pVSBlob), &output->layout);

   ID3D10Blob_Release(pVSBlob);
   pVSBlob = NULL;
   if(FAILED(hr)) {
       msg_Err(obj, "Failed to create the vertex input layout. (hr=0x%lX)", hr);
       goto error;
   }

   return S_OK;
error:
   return hr;
}

void D3D11_SetVertexShader(d3d_vshader_t *dst, d3d_vshader_t *src)
{
    dst->layout = src->layout;
    ID3D11InputLayout_AddRef(dst->layout);
    dst->shader = src->shader;
    ID3D11VertexShader_AddRef(dst->shader);
}

void D3D11_ReleaseVertexShader(d3d_vshader_t *shader)
{
    if (shader->layout)
    {
        ID3D11InputLayout_Release(shader->layout);
        shader->layout = NULL;
    }
    if (shader->shader)
    {
        ID3D11VertexShader_Release(shader->shader);
        shader->shader = NULL;
    }
}

HRESULT (D3D11_CompileFlatVertexShader)(vlc_object_t *obj, const d3d11_shaders_t *shaders,
                                      d3d11_device_t *d3d_dev, d3d_vshader_t *output)
{
    return D3D11_CompileVertexShader(obj, shaders, d3d_dev, globVertexShaderFlat, output);
}

HRESULT (D3D11_CompileProjectionVertexShader)(vlc_object_t *obj, const d3d11_shaders_t *shaders,
                                            d3d11_device_t *d3d_dev, d3d_vshader_t *output)
{
    return D3D11_CompileVertexShader(obj, shaders, d3d_dev, globVertexShaderProjection, output);
}

#if !VLC_WINSTORE_APP
static HINSTANCE Direct3D11LoadShaderLibrary(void)
{
    HINSTANCE instance = NULL;
    /* d3dcompiler_47 is the latest on windows 8.1 */
    for (int i = 47; i > 41; --i) {
        WCHAR filename[19];
        _snwprintf(filename, 19, TEXT("D3DCOMPILER_%d.dll"), i);
        instance = LoadLibrary(filename);
        if (instance) break;
    }
    return instance;
}
#endif // !VLC_WINSTORE_APP

int (D3D11_InitShaders)(vlc_object_t *obj, d3d11_shaders_t *shaders)
{
#if !VLC_WINSTORE_APP
    shaders->compiler_dll = Direct3D11LoadShaderLibrary();
    if (!shaders->compiler_dll) {
        msg_Err(obj, "cannot load d3dcompiler.dll, aborting");
        return VLC_EGENERIC;
    }

    shaders->OurD3DCompile = (void *)GetProcAddress(shaders->compiler_dll, "D3DCompile");
    if (!shaders->OurD3DCompile) {
        msg_Err(obj, "Cannot locate reference to D3DCompile in d3dcompiler DLL");
        FreeLibrary(shaders->compiler_dll);
        return VLC_EGENERIC;
    }
#endif // !VLC_WINSTORE_APP

    return VLC_SUCCESS;
}

void D3D11_ReleaseShaders(d3d11_shaders_t *shaders)
{
#if !VLC_WINSTORE_APP
    if (shaders->compiler_dll)
    {
        FreeLibrary(shaders->compiler_dll);
        shaders->compiler_dll = NULL;
    }
    shaders->OurD3DCompile = NULL;
#endif // !VLC_WINSTORE_APP
}
