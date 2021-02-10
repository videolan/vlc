/*****************************************************************************
 * d3d_dynamic_shader.c: Direct3D Shader APIs
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

#include <windows.h>
#include <assert.h>

#include <vlc_common.h>

#define COBJMACROS
#include <d3dcompiler.h>

#include "d3d_shaders.h"
#include "d3d_dynamic_shader.h"

static const char globPixelShaderDefault[] = "\
  cbuffer PS_CONSTANT_BUFFER : register(b0)\n\
  {\n\
    float4x4 WhitePoint;\n\
    float4x4 Colorspace;\n\
    float4x4 Primaries;\n\
    float Opacity;\n\
    float LuminanceScale;\n\
    float BoundaryX;\n\
    float BoundaryY;\n\
  };\n\
  Texture2D%s shaderTexture[4];\n\
  SamplerState normalSampler : register(s0);\n\
  SamplerState borderSampler : register(s1);\n\
  \n\
  struct PS_INPUT\n\
  {\n\
    float4 Position   : SV_POSITION;\n\
    float2 uv         : TEXCOORD;\n\
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
const float ST2084_m1 = 2610.0 / (4096.0 * 4);\n\
const float ST2084_m2 = (2523.0 / 4096.0) * 128.0;\n\
const float ST2084_c1 = 3424.0 / 4096.0;\n\
const float ST2084_c2 = (2413.0 / 4096.0) * 32.0;\n\
const float ST2084_c3 = (2392.0 / 4096.0) * 32.0;\n\
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
  inline float4 sampleTexture(SamplerState samplerState, float2 uv) {\n\
      float4 sample;\n\
      float3 coords = float3(uv, 0);\n\
%s /* sampling routine in sample */\n\
      return sample;\n\
  }\n\
  \n\
  float4 main( PS_INPUT In ) : SV_TARGET\n\
  {\n\
    float4 sample;\n\
    \n\
    if (In.uv.x > BoundaryX || In.uv.y > BoundaryY) \n\
        sample = sampleTexture( borderSampler, In.uv );\n\
    else\n\
        sample = sampleTexture( normalSampler, In.uv );\n\
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

static const char globVertexShaderFlat[] = "\
struct d3d_vertex_t\n\
{\n\
  float3 Position   : POSITION;\n\
  float2 uv         : TEXCOORD;\n\
};\n\
\n\
struct PS_INPUT\n\
{\n\
  float4 Position   : SV_POSITION;\n\
  float2 uv         : TEXCOORD;\n\
};\n\
\n\
PS_INPUT main( d3d_vertex_t In )\n\
{\n\
  PS_INPUT Output;\n\
  Output.Position = float4(In.Position, 1);\n\
  Output.uv  = In.uv;\n\
  return Output;\n\
}\n\
";

static const char globVertexShaderProjection[] = "\n\
cbuffer VS_PROJECTION_CONST : register(b0)\n\
{\n\
   float4x4 View;\n\
   float4x4 Zoom;\n\
   float4x4 Projection;\n\
};\n\
struct d3d_vertex_t\n\
{\n\
  float3 Position   : POSITION;\n\
  float2 uv         : TEXCOORD;\n\
};\n\
\n\
struct PS_INPUT\n\
{\n\
  float4 Position   : SV_POSITION;\n\
  float2 uv         : TEXCOORD;\n\
};\n\
\n\
PS_INPUT main( d3d_vertex_t In )\n\
{\n\
  PS_INPUT Output;\n\
  float4 pos = float4(In.Position, 1);\n\
  pos = mul(View, pos);\n\
  pos = mul(Zoom, pos);\n\
  pos = mul(Projection, pos);\n\
  Output.Position = pos;\n\
  Output.uv = In.uv;\n\
  return Output;\n\
}\n\
";

static void ReleaseID3D10Blob(d3d_shader_blob *blob)
{
    ID3D10Blob_Release( (ID3D10Blob*)blob->opaque );
}

static void ID3D10BlobtoBlob(ID3D10Blob *d3dblob, d3d_shader_blob *blob)
{
    blob->opaque = d3dblob;
    blob->pf_release = ReleaseID3D10Blob;
    blob->buf_size = ID3D10Blob_GetBufferSize(d3dblob);
    blob->buffer = ID3D10Blob_GetBufferPointer(d3dblob);
}


static HRESULT CompileShader(vlc_object_t *obj, const d3d_shader_compiler_t *compiler,
                             D3D_FEATURE_LEVEL feature_level,
                             const char *psz_shader, bool pixelShader,
                             d3d_shader_blob *blob)
{
    ID3D10Blob* pShaderBlob = NULL, *pErrBlob;
    const char *target;
    if (pixelShader)
    {
        if (likely(feature_level >= D3D_FEATURE_LEVEL_10_0))
            target = "ps_4_0";
        else if (feature_level >= D3D_FEATURE_LEVEL_9_3)
            target = "ps_4_0_level_9_3";
        else
            target = "ps_4_0_level_9_1";
    }
    else
    {
        if (likely(feature_level >= D3D_FEATURE_LEVEL_10_0))
            target = "vs_4_0";
        else if (feature_level >= D3D_FEATURE_LEVEL_9_3)
            target = "vs_4_0_level_9_3";
        else
            target = "vs_4_0_level_9_1";
    }

    UINT compileFlags = 0;
#if VLC_WINSTORE_APP
    VLC_UNUSED(compiler);
#else
# define D3DCompile(args...)    compiler->OurD3DCompile(args)
# if !defined(NDEBUG)
    if (IsDebuggerPresent())
        compileFlags += D3DCOMPILE_DEBUG;
# endif
#endif
    HRESULT hr = D3DCompile(psz_shader, strlen(psz_shader),
                            NULL, NULL, NULL, "main", target,
                            compileFlags, 0, &pShaderBlob, &pErrBlob);

    if (FAILED(hr)) {
        char *err = pErrBlob ? ID3D10Blob_GetBufferPointer(pErrBlob) : NULL;
        msg_Err(obj, "invalid %s Shader (hr=0x%lX): %s", pixelShader?"Pixel":"Vertex", hr, err );
        if (pErrBlob)
            ID3D10Blob_Release(pErrBlob);
        return E_FAIL;
    }
    if (!pShaderBlob)
        return E_INVALIDARG;
    ID3D10BlobtoBlob(pShaderBlob, blob);
    return S_OK;
}

static HRESULT CompilePixelShaderBlob(vlc_object_t *o, const d3d_shader_compiler_t *compiler,
                                   D3D_FEATURE_LEVEL feature_level,
                                   bool texture_array,
                                   const char *psz_sampler,
                                   const char *psz_src_to_linear,
                                   const char *psz_primaries_transform,
                                   const char *psz_linear_to_display,
                                   const char *psz_tone_mapping,
                                   const char *psz_adjust_range, const char *psz_move_planes,
                                   d3d_shader_blob *pPSBlob)
{
    char *shader;
    int allocated = asprintf(&shader, globPixelShaderDefault, texture_array ? "Array" : "",
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

    HRESULT hr = CompileShader(o, compiler, feature_level, shader, true, pPSBlob);
    free(shader);
    return hr;
}

HRESULT (D3D_CompilePixelShader)(vlc_object_t *o, const d3d_shader_compiler_t *compiler,
                                 D3D_FEATURE_LEVEL feature_level,
                                 bool texture_array,
                                 const display_info_t *display,
                                 video_transfer_func_t transfer,
                                 video_color_primaries_t primaries, bool src_full_range,
                                 const d3d_format_t *dxgi_fmt,
                                 d3d_shader_blob pPSBlob[DXGI_MAX_RENDER_TARGET])
{
    static const char *DEFAULT_NOOP = "return rgb";
    const char *psz_sampler[DXGI_MAX_RENDER_TARGET] = {NULL, NULL};
    const char *psz_src_to_linear     = DEFAULT_NOOP;
    const char *psz_linear_to_display = DEFAULT_NOOP;
    const char *psz_primaries_transform = DEFAULT_NOOP;
    const char *psz_tone_mapping      = "return rgb * LuminanceScale";
    const char *psz_adjust_range      = DEFAULT_NOOP;
    const char *psz_move_planes[2]    = {DEFAULT_NOOP, DEFAULT_NOOP};
    char *psz_range = NULL;

    if ( display->pixelFormat->formatTexture == DXGI_FORMAT_NV12 ||
         display->pixelFormat->formatTexture == DXGI_FORMAT_P010 )
    {
        /* we need 2 shaders, one for the Y target, one for the UV target */
        switch (dxgi_fmt->formatTexture)
        {
        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_P010:
            psz_sampler[0] =
                    "sample.x  = shaderTexture[0].Sample(samplerState, coords).x;\n"
                    "sample.y = 0.0;\n"
                    "sample.z = 0.0;\n"
                    "sample.a = 1;";
            psz_sampler[1] =
                    // TODO should be shaderTexture[0] ?
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
            switch (dxgi_fmt->fourcc)
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
        switch (dxgi_fmt->formatTexture)
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
            switch (dxgi_fmt->fourcc)
            {
            case VLC_CODEC_I420_10L:
                psz_sampler[0] =
                       "sample.x  = shaderTexture[0].Sample(samplerState, coords).x * 64;\n"
                       "sample.y  = shaderTexture[1].Sample(samplerState, coords).x * 64;\n"
                       "sample.z  = shaderTexture[2].Sample(samplerState, coords).x * 64;\n"
                       "sample.a  = 1;";
                break;
            case VLC_CODEC_I444_16L:
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
    if (!DxgiIsRGBFormat(dxgi_fmt) && !src_full_range && DxgiIsRGBFormat(display->pixelFormat))
        range_adjust--; /* the YUV->RGB conversion already output full range */

    if (range_adjust != 0)
    {
        FLOAT itu_black_level;
        FLOAT itu_range_factor;
        FLOAT itu_white_level;
        switch (dxgi_fmt->bitsPerChannel)
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
            asprintf(&psz_range, "return clamp((rgb + %f) * %f, 0, 1)",
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
            asprintf(&psz_range, "return clamp(rgb + %f * %f,%f,%f)",
                     black_level, range_factor, itu_black_level, itu_white_level);
        }
        psz_adjust_range = psz_range;
    }

    HRESULT hr;
    hr = CompilePixelShaderBlob(o, compiler, feature_level, texture_array,
                                psz_sampler[0],
                                psz_src_to_linear,
                                psz_primaries_transform,
                                psz_linear_to_display,
                                psz_tone_mapping,
                                psz_adjust_range, psz_move_planes[0], &pPSBlob[0]);
    if (SUCCEEDED(hr) && psz_sampler[1])
    {
        hr = CompilePixelShaderBlob(o, compiler, feature_level, texture_array,
                                    psz_sampler[1],
                                    psz_src_to_linear,
                                    psz_primaries_transform,
                                    psz_linear_to_display,
                                    psz_tone_mapping,
                                    psz_adjust_range, psz_move_planes[1], &pPSBlob[1]);
        if (FAILED(hr))
            D3D_ShaderBlobRelease(&pPSBlob[0]);
    }
    free(psz_range);

    return hr;
}

HRESULT D3D_CompileVertexShader(vlc_object_t *obj, const d3d_shader_compiler_t *compiler,
                                D3D_FEATURE_LEVEL feature_level, bool flat,
                                d3d_shader_blob *blob)
{
    return CompileShader(obj, compiler, feature_level,
                         flat ? globVertexShaderFlat : globVertexShaderProjection,
                         false, blob);
}


#if !VLC_WINSTORE_APP
static HINSTANCE Direct3DLoadShaderLibrary(void)
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

int D3D_InitShaderCompiler(vlc_object_t *obj, d3d_shader_compiler_t *compiler)
{
#if !VLC_WINSTORE_APP
    compiler->compiler_dll = Direct3DLoadShaderLibrary();
    if (!compiler->compiler_dll) {
        msg_Err(obj, "cannot load d3dcompiler.dll, aborting");
        return VLC_EGENERIC;
    }

    compiler->OurD3DCompile = (void *)GetProcAddress(compiler->compiler_dll, "D3DCompile");
    if (!compiler->OurD3DCompile) {
        msg_Err(obj, "Cannot locate reference to D3DCompile in d3dcompiler DLL");
        FreeLibrary(compiler->compiler_dll);
        return VLC_EGENERIC;
    }
#endif // !VLC_WINSTORE_APP

    return VLC_SUCCESS;
}

void D3D_ReleaseShaderCompiler(d3d_shader_compiler_t *compiler)
{
#if !VLC_WINSTORE_APP
    if (compiler->compiler_dll)
    {
        FreeLibrary(compiler->compiler_dll);
        compiler->compiler_dll = NULL;
    }
    compiler->OurD3DCompile = NULL;
#endif // !VLC_WINSTORE_APP
}

