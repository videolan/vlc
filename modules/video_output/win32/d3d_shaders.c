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

#define COBJMACROS
#include "d3d_shaders.h"

#define SPHERE_RADIUS 1.f

#define SPHERE_SLICES 128
#define nbLatBands SPHERE_SLICES
#define nbLonBands SPHERE_SLICES

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
    float3 Texture    : TEXCOORD;\n\
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
      const float B67_inv_r2 = 4.0; /* 1/0.5 */\n\
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
        sample = sampleTexture( borderSampler, In.Texture );\n\
    else\n\
        sample = sampleTexture( normalSampler, In.Texture );\n\
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

static const char* globVertexShaderFlat = "\
struct d3d_vertex_t\n\
{\n\
  float3 Position   : POSITION;\n\
  float2 uv         : TEXCOORD;\n\
};\n\
\n\
struct PS_INPUT\n\
{\n\
  float4 Position   : SV_POSITION;\n\
  float3 Texture    : TEXCOORD;\n\
};\n\
\n\
PS_INPUT main( d3d_vertex_t In )\n\
{\n\
  PS_INPUT Output;\n\
  Output.Position = float4(In.Position, 1);\n\
  Output.Texture  = float3(In.uv, 0);\n\
  return Output;\n\
}\n\
";

static const char* globVertexShaderProjection = "\n\
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
  float3 Texture    : TEXCOORD;\n\
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
  Output.Texture = float3(In.uv, 0);\n\
  return Output;\n\
}\n\
";

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

int (D3D_InitShaders)(vlc_object_t *obj, d3d_shader_compiler_t *compiler)
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

void D3D_ReleaseShaders(d3d_shader_compiler_t *compiler)
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

static ID3DBlob* CompileShader(vlc_object_t *obj, const d3d_shader_compiler_t *compiler,
                               D3D_FEATURE_LEVEL feature_level,
                               const char *psz_shader, bool pixel)
{
    ID3DBlob* pShaderBlob = NULL, *pErrBlob;
    const char *target;
    if (pixel)
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
        msg_Err(obj, "invalid %s Shader (hr=0x%lX): %s", pixel?"Pixel":"Vertex", hr, err );
        if (pErrBlob)
            ID3D10Blob_Release(pErrBlob);
        return NULL;
    }
    return pShaderBlob;
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
                                   ID3DBlob **pPSBlob)
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

    *pPSBlob = CompileShader(o, compiler, feature_level, shader, true);
    free(shader);
    if (!*pPSBlob)
        return E_INVALIDARG;
    return S_OK;
}

HRESULT (D3D_CompilePixelShader)(vlc_object_t *o, const d3d_shader_compiler_t *compiler,
                                 D3D_FEATURE_LEVEL feature_level,
                                 bool texture_array,
                                 const display_info_t *display,
                                 video_transfer_func_t transfer,
                                 video_color_primaries_t primaries, bool src_full_range,
                                 const d3d_format_t *dxgi_fmt,
                                 ID3DBlob *pPSBlob[DXGI_MAX_RENDER_TARGET])
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
    if (!DxgiIsRGBFormat(dxgi_fmt) && !src_full_range && DxgiIsRGBFormat(display->pixelFormat))
        range_adjust--; /* the YUV->RGB conversion already output full range */

    if (range_adjust != 0)
    {
        psz_range = malloc(256);
        if (likely(psz_range))
        {
            FLOAT itu_black_level;
            FLOAT itu_range_factor;
            FLOAT itu_white_level;
            switch (dxgi_fmt->bitsPerChannel)
            {
            case 8:
                /* Rec. ITU-R BT.709-6 ?4.6 */
                itu_black_level  =              16.f / 255.f;
                itu_white_level  =             235.f / 255.f;
                itu_range_factor = (float)(235 - 16) / 255.f;
                break;
            case 10:
                /* Rec. ITU-R BT.709-6 ?4.6 */
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
        {
            ID3D10Blob_Release(pPSBlob[0]);
            pPSBlob[0] = NULL;
        }
    }
    else
        pPSBlob[1] = NULL;
    free(psz_range);

    return hr;
}

HRESULT D3D_CompileVertexShader(vlc_object_t *obj, const d3d_shader_compiler_t *compiler,
                                D3D_FEATURE_LEVEL feature_level, bool flat,
                                ID3DBlob **pVSBlob)
{
   *pVSBlob = CompileShader(obj, compiler, feature_level,
                            flat ? globVertexShaderFlat : globVertexShaderProjection, false);
    if (!*pVSBlob)
        return E_FAIL;
    return S_OK;
}

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

static void GetPrimariesTransform(FLOAT Primaries[4*4], video_color_primaries_t src,
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
    for (size_t j=0;j<4; ++j)
        Primaries[j + 3*4] = j == 3;
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

    static const FLOAT IDENTITY_4X4[4 * 4] = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f,
    };

    /* matrices for studio range */
    /* see https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion, in studio range */
    static const FLOAT COLORSPACE_BT601_YUV_TO_FULL_RGBA[4*4] = {
        1.164383561643836f,                 0.f,  1.596026785714286f, 0.f,
        1.164383561643836f, -0.391762290094914f, -0.812967647237771f, 0.f,
        1.164383561643836f,  2.017232142857142f,                 0.f, 0.f,
                       0.f,                 0.f,                 0.f, 1.f,
    };

    static const FLOAT COLORSPACE_FULL_RGBA_TO_BT601_YUV[4*4] = {
        0.299000f,  0.587000f,  0.114000f, 0.f,
       -0.168736f, -0.331264f,  0.500000f, 0.f,
        0.500000f, -0.418688f, -0.081312f, 0.f,
              0.f,        0.f,        0.f, 1.f,
    };

    /* see https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.709_conversion, in studio range */
    static const FLOAT COLORSPACE_BT709_YUV_TO_FULL_RGBA[4*4] = {
        1.164383561643836f,                 0.f,  1.792741071428571f, 0.f,
        1.164383561643836f, -0.213248614273730f, -0.532909328559444f, 0.f,
        1.164383561643836f,  2.112401785714286f,                 0.f, 0.f,
                       0.f,                 0.f,                 0.f, 1.f,
    };
    /* see https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.2020_conversion, in studio range */
    static const FLOAT COLORSPACE_BT2020_YUV_TO_FULL_RGBA[4*4] = {
        1.164383561643836f,  0.000000000000f,  1.678674107143f, 0.f,
        1.164383561643836f, -0.127007098661f, -0.440987687946f, 0.f,
        1.164383561643836f,  2.141772321429f,  0.000000000000f, 0.f,
                       0.f,              0.f,              0.f, 1.f,
    };

    memcpy(quad->shaderConstants->WhitePoint, IDENTITY_4X4, sizeof(quad->shaderConstants->WhitePoint));

    const FLOAT *ppColorspace;
    if (RGB_src_shader == DxgiIsRGBFormat(displayFormat->pixelFormat))
    {
        ppColorspace = IDENTITY_4X4;
    }
    else if (RGB_src_shader)
    {
        ppColorspace = COLORSPACE_FULL_RGBA_TO_BT601_YUV;
        quad->shaderConstants->WhitePoint[0*4 + 3] = -itu_black_level;
        quad->shaderConstants->WhitePoint[1*4 + 3] = itu_achromacy;
        quad->shaderConstants->WhitePoint[2*4 + 3] = itu_achromacy;
    }
    else
    {
        switch (fmt->space){
            case COLOR_SPACE_BT709:
                ppColorspace = COLORSPACE_BT709_YUV_TO_FULL_RGBA;
                break;
            case COLOR_SPACE_BT2020:
                ppColorspace = COLORSPACE_BT2020_YUV_TO_FULL_RGBA;
                break;
            case COLOR_SPACE_BT601:
                ppColorspace = COLORSPACE_BT601_YUV_TO_FULL_RGBA;
                break;
            default:
            case COLOR_SPACE_UNDEF:
                if( fmt->i_height > 576 )
                {
                    ppColorspace = COLORSPACE_BT709_YUV_TO_FULL_RGBA;
                }
                else
                {
                    ppColorspace = COLORSPACE_BT601_YUV_TO_FULL_RGBA;
                }
                break;
        }
        /* all matrices work in studio range and output in full range */
        quad->shaderConstants->WhitePoint[0*4 + 3] = -itu_black_level;
        quad->shaderConstants->WhitePoint[1*4 + 3] = -itu_achromacy;
        quad->shaderConstants->WhitePoint[2*4 + 3] = -itu_achromacy;
    }

    memcpy(quad->shaderConstants->Colorspace, ppColorspace, sizeof(quad->shaderConstants->Colorspace));

    if (fmt->primaries != displayFormat->primaries)
    {
        GetPrimariesTransform(quad->shaderConstants->Primaries, fmt->primaries,
                              displayFormat->primaries);
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
static void orientationVertexOrder(video_orientation_t orientation, int vertex_order[static 4])
{
    switch (orientation) {
        case ORIENT_ROTATED_90:
            vertex_order[0] = 3;
            vertex_order[1] = 0;
            vertex_order[2] = 1;
            vertex_order[3] = 2;
            break;
        case ORIENT_ROTATED_270:
            vertex_order[0] = 1;
            vertex_order[1] = 2;
            vertex_order[2] = 3;
            vertex_order[3] = 0;
            break;
        case ORIENT_ROTATED_180:
            vertex_order[0] = 2;
            vertex_order[1] = 3;
            vertex_order[2] = 0;
            vertex_order[3] = 1;
            break;
        case ORIENT_TRANSPOSED:
            vertex_order[0] = 2;
            vertex_order[1] = 1;
            vertex_order[2] = 0;
            vertex_order[3] = 3;
            break;
        case ORIENT_HFLIPPED:
            vertex_order[0] = 1;
            vertex_order[1] = 0;
            vertex_order[2] = 3;
            vertex_order[3] = 2;
            break;
        case ORIENT_VFLIPPED:
            vertex_order[0] = 3;
            vertex_order[1] = 2;
            vertex_order[2] = 1;
            vertex_order[3] = 0;
            break;
        case ORIENT_ANTI_TRANSPOSED: /* transpose + vflip */
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
                          WORD *triangle_pos, video_orientation_t orientation)
{
    unsigned int src_width = quad->i_width;
    unsigned int src_height = quad->i_height;
    float MidX,MidY;

    float top, bottom, left, right;
    /* find the middle of the visible part of the texture, it will be a 0,0
     * the rest of the visible area must correspond to -1,1 */
    switch (orientation)
    {
    case ORIENT_ROTATED_90: /* 90° anti clockwise */
        /* right/top aligned */
        MidY = (output->left + output->right) / 2.f;
        MidX = (output->top + output->bottom) / 2.f;
        top    =  MidY / (MidY - output->top);
        bottom = -(src_height - MidX) / (MidX - output->top);
        left   =  (MidX - src_height) / (MidX - output->left);
        right  =                 MidX / (MidX - (src_width - output->right));
        break;
    case ORIENT_ROTATED_180: /* 180° */
        /* right/top aligned */
        MidY = (output->top + output->bottom) / 2.f;
        MidX = (output->left + output->right) / 2.f;
        top    =  (src_height - MidY) / (output->bottom - MidY);
        bottom = -MidY / (MidY - output->top);
        left   = -MidX / (MidX - output->left);
        right  =  (src_width  - MidX) / (output->right - MidX);
        break;
    case ORIENT_ROTATED_270: /* 90° clockwise */
        /* right/top aligned */
        MidY = (output->left + output->right) / 2.f;
        MidX = (output->top + output->bottom) / 2.f;
        top    =  (src_width  - MidX) / (output->right - MidX);
        bottom = -MidY / (MidY - output->top);
        left   = -MidX / (MidX - output->left);
        right  =  (src_height - MidY) / (output->bottom - MidY);
        break;
    case ORIENT_ANTI_TRANSPOSED:
        MidY = (output->left + output->right) / 2.f;
        MidX = (output->top + output->bottom) / 2.f;
        top    =  (src_width  - MidX) / (output->right - MidX);
        bottom = -MidY / (MidY - output->top);
        left   = -(src_height - MidY) / (output->bottom - MidY);
        right  =  MidX / (MidX - output->left);
        break;
    case ORIENT_TRANSPOSED:
        MidY = (output->left + output->right) / 2.f;
        MidX = (output->top + output->bottom) / 2.f;
        top    =  (src_width  - MidX) / (output->right - MidX);
        bottom = -MidY / (MidY - output->top);
        left   = -MidX / (MidX - output->left);
        right  =  (src_height - MidY) / (output->bottom - MidY);
        break;
    case ORIENT_VFLIPPED:
        MidY = (output->top + output->bottom) / 2.f;
        MidX = (output->left + output->right) / 2.f;
        top    =  (src_height - MidY) / (output->bottom - MidY);
        bottom = -MidY / (MidY - output->top);
        left   = -MidX / (MidX - output->left);
        right  =  (src_width  - MidX) / (output->right - MidX);
        break;
    case ORIENT_HFLIPPED:
        MidY = (output->top + output->bottom) / 2.f;
        MidX = (output->left + output->right) / 2.f;
        top    =  MidY / (MidY - output->top);
        bottom = -(src_height - MidY) / (output->bottom - MidY);
        left   = -(src_width  - MidX) / (output->right - MidX);
        right  =  MidX / (MidX - output->left);
        break;
    case ORIENT_NORMAL:
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
    if( orientation == ORIENT_TOP_RIGHT || orientation == ORIENT_BOTTOM_LEFT
     || orientation == ORIENT_LEFT_TOP  || orientation == ORIENT_RIGHT_BOTTOM )
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
                       void *pData, video_orientation_t orientation)
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
