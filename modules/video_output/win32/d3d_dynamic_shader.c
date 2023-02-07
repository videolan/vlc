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

struct d3d_shader_compiler_t
{
    HINSTANCE                 compiler_dll; /* handle of the opened d3dcompiler dll */
    pD3DCompile               OurD3DCompile;
};

static const char globPixelShaderDefault[] = "\
#pragma warning( disable: 3571 )\n\
cbuffer PS_CONSTANT_BUFFER : register(b0)\n\
{\n\
    float4x3 Colorspace;\n\
    float Opacity;\n\
    float LuminanceScale;\n\
    float2 Boundary;\n\
};\n\
Texture2D shaderTexture[TEXTURE_RESOURCES];\n\
SamplerState normalSampler : register(s0);\n\
SamplerState borderSampler : register(s1);\n\
\n\
struct PS_INPUT\n\
{\n\
    float4 Position   : SV_POSITION;\n\
    float2 uv         : TEXCOORD;\n\
};\n\
\n\
#define TONE_MAP_HABLE       1\n\
\n\
#define SRC_TRANSFER_709     1\n\
#define SRC_TRANSFER_PQ      2\n\
#define SRC_TRANSFER_HLG     3\n\
#define SRC_TRANSFER_SRGB    4\n\
#define SRC_TRANSFER_470BG   5\n\
\n\
#define DST_TRANSFER_SRGB    1\n\
#define DST_TRANSFER_PQ      2\n\
\n\
#define FULL_RANGE           1\n\
#define STUDIO_RANGE         2\n\
\n\
#define SAMPLE_NV12_TO_YUVA          1\n\
#define SAMPLE_YUY2_TO_YUVA          2\n\
#define SAMPLE_Y210_TO_YUVA          3\n\
#define SAMPLE_Y410_TO_YUVA          4\n\
#define SAMPLE_AYUV_TO_YUVA          5\n\
#define SAMPLE_RGBA_TO_RGBA          6\n\
#define SAMPLE_TRIPLANAR_TO_YUVA     7\n\
#define SAMPLE_TRIPLANAR10_TO_YUVA   8\n\
#define SAMPLE_PLANAR_YUVA_TO_YUVA   9\n\
\n\
#define SAMPLE_NV12_TO_NV_Y         10\n\
#define SAMPLE_NV12_TO_NV_UV        11\n\
#define SAMPLE_RGBA_TO_NV_R         12\n\
#define SAMPLE_RGBA_TO_NV_GB        13\n\
#define SAMPLE_PLANAR_YUVA_TO_NV_Y  14\n\
#define SAMPLE_PLANAR_YUVA_TO_NV_UV 15\n\
\n\
#if (TONE_MAPPING==TONE_MAP_HABLE)\n\
/* see http://filmicworlds.com/blog/filmic-tonemapping-operators/ */\n\
inline float3 hable(float3 x) {\n\
    const float A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30;\n\
    return ((x * (A*x + (C*B))+(D*E))/(x * (A*x + B) + (D*F))) - E/F;\n\
}\n\
#endif\n\
\n\
#if (SRC_TO_LINEAR==SRC_TRANSFER_HLG)\n\
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
#endif\n\
\n\
inline float3 sourceToLinear(float3 rgb) {\n\
#if (SRC_TO_LINEAR==SRC_TRANSFER_PQ)\n\
    const float ST2084_m1 = 2610.0 / (4096.0 * 4);\n\
    const float ST2084_m2 = (2523.0 / 4096.0) * 128.0;\n\
    const float ST2084_c1 = 3424.0 / 4096.0;\n\
    const float ST2084_c2 = (2413.0 / 4096.0) * 32.0;\n\
    const float ST2084_c3 = (2392.0 / 4096.0) * 32.0;\n\
    rgb = pow(max(rgb, 0), 1.0/ST2084_m2);\n\
    rgb = max(rgb - ST2084_c1, 0.0) / (ST2084_c2 - ST2084_c3 * rgb);\n\
    rgb = pow(rgb, 1.0/ST2084_m1);\n\
    return rgb * 10000;\n\
#elif (SRC_TO_LINEAR==SRC_TRANSFER_HLG)\n\
    /* TODO: in one call */\n\
    rgb.r = inverse_HLG(rgb.r);\n\
    rgb.g = inverse_HLG(rgb.g);\n\
    rgb.b = inverse_HLG(rgb.b);\n\
    float3 ootf_2020 = float3(0.2627, 0.6780, 0.0593);\n\
    float ootf_ys = 1000 * dot(ootf_2020, rgb);\n\
    return rgb * pow(ootf_ys, 0.200);\n\
#elif (SRC_TO_LINEAR==SRC_TRANSFER_709)\n\
    return pow(rgb, 1.0 / 0.45);\n\
#elif (SRC_TO_LINEAR==SRC_TRANSFER_SRGB)\n\
    return pow(rgb, 2.2);\n\
#elif (SRC_TO_LINEAR==SRC_TRANSFER_470BG)\n\
    return pow(rgb, 2.8);\n\
#else\n\
    return rgb;\n\
#endif\n\
}\n\
\n\
inline float3 linearToDisplay(float3 rgb) {\n\
#if (LINEAR_TO_DST==DST_TRANSFER_SRGB)\n\
    return pow(rgb, 1.0 / 2.2);\n\
#elif (LINEAR_TO_DST==DST_TRANSFER_PQ)\n\
    const float ST2084_m1 = 2610.0 / (4096.0 * 4);\n\
    const float ST2084_m2 = (2523.0 / 4096.0) * 128.0;\n\
    const float ST2084_c1 = 3424.0 / 4096.0;\n\
    const float ST2084_c2 = (2413.0 / 4096.0) * 32.0;\n\
    const float ST2084_c3 = (2392.0 / 4096.0) * 32.0;\n\
    rgb = pow(rgb / 10000, ST2084_m1);\n\
    rgb = (ST2084_c1 + ST2084_c2 * rgb) / (1 + ST2084_c3 * rgb);\n\
    return pow(rgb, ST2084_m2);\n\
#else\n\
    return rgb;\n\
#endif\n\
}\n\
\n\
inline float3 toneMapping(float3 rgb) {\n\
    rgb = rgb * LuminanceScale;\n\
#if (TONE_MAPPING==TONE_MAP_HABLE)\n\
    const float3 HABLE_DIV = hable(11.2);\n\
    rgb = hable(rgb) / HABLE_DIV;\n\
#endif\n\
    return rgb;\n\
}\n\
\n\
inline float4 sampleTexture(SamplerState samplerState, float2 coords) {\n\
    float4 sample;\n\
    /* sampling routine in sample */\n\
#if (SAMPLE_TEXTURES==SAMPLE_NV12_TO_YUVA)\n\
    sample.x  = shaderTexture[0].Sample(samplerState, coords).x;\n\
    sample.yz = shaderTexture[1].Sample(samplerState, coords).xy;\n\
    sample.a  = 1;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_YUY2_TO_YUVA)\n\
    sample.x  = shaderTexture[0].Sample(samplerState, coords).x;\n\
    sample.y  = shaderTexture[0].Sample(samplerState, coords).y;\n\
    sample.z  = shaderTexture[0].Sample(samplerState, coords).a;\n\
    sample.a  = 1;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_Y210_TO_YUVA)\n\
    sample.x  = shaderTexture[0].Sample(samplerState, coords).r;\n\
    sample.y  = shaderTexture[0].Sample(samplerState, coords).g;\n\
    sample.z  = shaderTexture[0].Sample(samplerState, coords).a;\n\
    sample.a  = 1;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_Y410_TO_YUVA)\n\
    sample.x  = shaderTexture[0].Sample(samplerState, coords).g;\n\
    sample.y  = shaderTexture[0].Sample(samplerState, coords).r;\n\
    sample.z  = shaderTexture[0].Sample(samplerState, coords).b;\n\
    sample.a  = 1;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_AYUV_TO_YUVA)\n\
    sample.x  = shaderTexture[0].Sample(samplerState, coords).z;\n\
    sample.y  = shaderTexture[0].Sample(samplerState, coords).y;\n\
    sample.z  = shaderTexture[0].Sample(samplerState, coords).x;\n\
    sample.a  = 1;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_RGBA_TO_RGBA)\n\
    sample = shaderTexture[0].Sample(samplerState, coords);\n\
#elif (SAMPLE_TEXTURES==SAMPLE_TRIPLANAR_TO_YUVA)\n\
    sample.x  = shaderTexture[0].Sample(samplerState, coords).x;\n\
    sample.y  = shaderTexture[1].Sample(samplerState, coords).x;\n\
    sample.z  = shaderTexture[2].Sample(samplerState, coords).x;\n\
    sample.a  = 1;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_TRIPLANAR10_TO_YUVA)\n\
    sample.x  = shaderTexture[0].Sample(samplerState, coords).x;\n\
    sample.y  = shaderTexture[1].Sample(samplerState, coords).x;\n\
    sample.z  = shaderTexture[2].Sample(samplerState, coords).x;\n\
    sample = sample * 64;\n\
    sample.a  = 1;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_PLANAR_YUVA_TO_YUVA)\n\
    sample.x  = shaderTexture[0].Sample(samplerState, coords).x;\n\
    sample.y  = shaderTexture[1].Sample(samplerState, coords).x;\n\
    sample.z  = shaderTexture[2].Sample(samplerState, coords).x;\n\
    sample.a  = shaderTexture[3].Sample(samplerState, coords).x;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_NV12_TO_NV_Y)\n\
    sample.x  = shaderTexture[0].Sample(samplerState, coords).x;\n\
    sample.y = 0.0;\n\
    sample.z = 0.0;\n\
    sample.a = 1;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_NV12_TO_NV_UV)\n\
    // TODO should be shaderTexture[0] ?\n\
    sample.xy  = shaderTexture[1].Sample(samplerState, coords).xy;\n\
    sample.z = 0.0;\n\
    sample.a = 1;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_RGBA_TO_NV_R)\n\
    sample = shaderTexture[0].Sample(samplerState, coords);\n\
#elif (SAMPLE_TEXTURES==SAMPLE_RGBA_TO_NV_GB)\n\
    sample.x = shaderTexture[0].Sample(samplerState, coords).y;\n\
    sample.y = shaderTexture[0].Sample(samplerState, coords).z;\n\
    sample.z = 0;\n\
    sample.a = 1;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_PLANAR_YUVA_TO_NV_Y)\n\
    sample.x = shaderTexture[0].Sample(samplerState, coords).x;\n\
    sample.y = 0.0;\n\
    sample.z = 0.0;\n\
    sample.a = shaderTexture[3].Sample(samplerState, coords).x;\n\
#elif (SAMPLE_TEXTURES==SAMPLE_PLANAR_YUVA_TO_NV_UV)\n\
    sample.x = shaderTexture[1].Sample(samplerState, coords).x;\n\
    sample.y = shaderTexture[2].Sample(samplerState, coords).x;\n\
    sample.z = 0.0;\n\
    sample.a = shaderTexture[3].Sample(samplerState, coords).x;\n\
#endif\n\
    return sample;\n\
}\n\
\n\
float4 main( PS_INPUT In ) : SV_TARGET\n\
{\n\
    float4 sample;\n\
    \n\
    if (In.uv.x > Boundary.x || In.uv.y > Boundary.y) \n\
        sample = sampleTexture( borderSampler, In.uv );\n\
    else\n\
        sample = sampleTexture( normalSampler, In.uv );\n\
    float3 rgb = max(mul(sample, Colorspace),0);\n\
    rgb = sourceToLinear(rgb);\n\
    rgb = toneMapping(rgb);\n\
    rgb = linearToDisplay(rgb);\n\
    return float4(rgb, saturate(sample.a * Opacity));\n\
}\n\
";

static const char globVertexShader[] = "\n\
#if HAS_PROJECTION\n\
cbuffer VS_PROJECTION_CONST : register(b0)\n\
{\n\
    float4x4 View;\n\
    float4x4 Zoom;\n\
    float4x4 Projection;\n\
};\n\
#endif\n\
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
#if HAS_PROJECTION\n\
    pos = mul(View, pos);\n\
    pos = mul(Zoom, pos);\n\
    pos = mul(Projection, pos);\n\
#endif\n\
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
                             const D3D_SHADER_MACRO *defines,
                             d3d_shader_blob *blob)
{
    ID3D10Blob* pShaderBlob = NULL, *pErrBlob = NULL;

    UINT compileFlags = 0;
#ifdef VLC_WINSTORE_APP
    VLC_UNUSED(compiler);
#else
# define D3DCompile(a,b,c,d,e,f,g,h,i,j,k)    compiler->OurD3DCompile(a,b,c,d,e,f,g,h,i,j,k)
# if !defined(NDEBUG)
    if (IsDebuggerPresent())
        compileFlags += D3DCOMPILE_DEBUG;
# endif
#endif
    HRESULT hr;
    {
        const char *target;
        if (pixelShader)
        {
            if (feature_level >= D3D_FEATURE_LEVEL_12_0)
                target = "ps_5_0";
            else if (likely(feature_level >= D3D_FEATURE_LEVEL_10_0))
                target = "ps_4_0";
            else if (feature_level >= D3D_FEATURE_LEVEL_9_3)
                target = "ps_4_0_level_9_3";
            else
                target = "ps_4_0_level_9_1";
        }
        else
        {
            if (feature_level >= D3D_FEATURE_LEVEL_12_0)
                target = "vs_5_0";
            else if (likely(feature_level >= D3D_FEATURE_LEVEL_10_0))
                target = "vs_4_0";
            else if (feature_level >= D3D_FEATURE_LEVEL_9_3)
                target = "vs_4_0_level_9_3";
            else
                target = "vs_4_0_level_9_1";
        }

        hr = D3DCompile(psz_shader, strlen(psz_shader),
                        NULL, defines, NULL, "main", target,
                        compileFlags, 0, &pShaderBlob, &pErrBlob);
    }

    if (FAILED(hr) || pErrBlob) {
        const char *err = pErrBlob ? ID3D10Blob_GetBufferPointer(pErrBlob) : NULL;
        if (SUCCEEDED(hr))
            msg_Dbg(obj, "%s Shader: %s", pixelShader?"Pixel":"Vertex", err );
        else
            msg_Err(obj, "invalid %s Shader (hr=0x%lX): %s", pixelShader?"Pixel":"Vertex", hr, err );
        if (pErrBlob)
            ID3D10Blob_Release(pErrBlob);
        if (FAILED(hr))
            return hr;
    }
    if (!pShaderBlob)
        return E_INVALIDARG;
    ID3D10BlobtoBlob(pShaderBlob, blob);
    return S_OK;
}

static HRESULT CompilePixelShaderBlob(vlc_object_t *o, const d3d_shader_compiler_t *compiler,
                                   D3D_FEATURE_LEVEL feature_level,
                                   const char *psz_sampler,
                                   const char *psz_shader_resource_views,
                                   const char *psz_src_to_linear,
                                   const char *psz_linear_to_display,
                                   const char *psz_tone_mapping,
                                   d3d_shader_blob *pPSBlob)
{
    if (var_InheritInteger(o, "verbose") >= 4)
        msg_Dbg(o, "shader %s", globPixelShaderDefault);

    D3D_SHADER_MACRO defines[] = {
         { "TEXTURE_RESOURCES", psz_shader_resource_views },
         { "TONE_MAPPING",      psz_tone_mapping },
         { "SRC_TO_LINEAR",     psz_src_to_linear },
         { "LINEAR_TO_DST",     psz_linear_to_display },
         { "SAMPLE_TEXTURES",   psz_sampler },
         { NULL, NULL },
    };
#ifndef NDEBUG
    size_t param=0;
    while (defines[param].Name)
    {
        msg_Dbg(o,"%s = %s", defines[param].Name, defines[param].Definition);
        param++;
    }
#endif

    return CompileShader(o, compiler, feature_level, globPixelShaderDefault, true,
                         defines, pPSBlob);
}

HRESULT (D3D_CompilePixelShader)(vlc_object_t *o, const d3d_shader_compiler_t *compiler,
                                 D3D_FEATURE_LEVEL feature_level,
                                 const display_info_t *display,
                                 video_transfer_func_t transfer,
                                 bool src_full_range,
                                 const d3d_format_t *dxgi_fmt,
                                 d3d_shader_blob pPSBlob[DXGI_MAX_RENDER_TARGET],
                                 size_t shader_views[DXGI_MAX_RENDER_TARGET])
{
    static const char *DEFAULT_NOOP = "0";
    const char *psz_sampler[DXGI_MAX_RENDER_TARGET] = {NULL, NULL};
    const char *psz_shader_resource_views[DXGI_MAX_RENDER_TARGET] = { NULL, NULL };
    const char *psz_src_to_linear     = DEFAULT_NOOP;
    const char *psz_linear_to_display = DEFAULT_NOOP;
    const char *psz_tone_mapping      = DEFAULT_NOOP;
    shader_views[1] = 0;

    if ( display->pixelFormat->formatTexture == DXGI_FORMAT_NV12 ||
         display->pixelFormat->formatTexture == DXGI_FORMAT_P010 )
    {
        /* we need 2 shaders, one for the Y target, one for the UV target */
        switch (dxgi_fmt->formatTexture)
        {
        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_P010:
            psz_sampler[0] = "SAMPLE_NV12_TO_NV_Y";
            psz_shader_resource_views[0] = "1"; shader_views[0] = 1;
            psz_sampler[1] = "SAMPLE_NV12_TO_NV_UV";
            psz_shader_resource_views[1] = "2"; shader_views[1] = 2; // TODO should be 1 ?
            break;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_B5G6R5_UNORM:
            /* Y */
            psz_sampler[0] = "SAMPLE_RGBA_TO_NV_R";
            psz_shader_resource_views[0] = "1"; shader_views[0] = 1;
            /* UV */
            psz_sampler[1] = "SAMPLE_RGBA_TO_NV_GB";
            psz_shader_resource_views[1] = "1"; shader_views[1] = 1;
            break;
        case DXGI_FORMAT_UNKNOWN:
            switch (dxgi_fmt->fourcc)
            {
            case VLC_CODEC_YUVA:
                /* Y */
                psz_sampler[0] = "SAMPLE_PLANAR_YUVA_TO_NV_Y";
                psz_shader_resource_views[0] = "4"; shader_views[0] = 4;
                /* UV */
                psz_sampler[1] = "SAMPLE_PLANAR_YUVA_TO_NV_UV";
                psz_shader_resource_views[1] = "4"; shader_views[1] = 4;
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
            psz_sampler[0] = "SAMPLE_NV12_TO_YUVA";
            psz_shader_resource_views[0] = "2"; shader_views[0] = 2;
            break;
        case DXGI_FORMAT_YUY2:
            psz_sampler[0] = "SAMPLE_YUY2_TO_YUVA";
            psz_shader_resource_views[0] = "1"; shader_views[0] = 1;
            break;
        case DXGI_FORMAT_Y210:
            psz_sampler[0] = "SAMPLE_Y210_TO_YUVA";
            psz_shader_resource_views[0] = "1"; shader_views[0] = 1;
            break;
        case DXGI_FORMAT_Y410:
            psz_sampler[0] = "SAMPLE_Y410_TO_YUVA";
            psz_shader_resource_views[0] = "1"; shader_views[0] = 1;
            break;
        case DXGI_FORMAT_AYUV:
            psz_sampler[0] = "SAMPLE_AYUV_TO_YUVA";
            psz_shader_resource_views[0] = "1"; shader_views[0] = 1;
            break;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_B5G6R5_UNORM:
            psz_sampler[0] = "SAMPLE_RGBA_TO_RGBA";
            psz_shader_resource_views[0] = "1"; shader_views[0] = 1;
            break;
        case DXGI_FORMAT_UNKNOWN:
            switch (dxgi_fmt->fourcc)
            {
            case VLC_CODEC_I420_10L:
                psz_sampler[0] = "SAMPLE_TRIPLANAR10_TO_YUVA";
                psz_shader_resource_views[0] = "3"; shader_views[0] = 3;
                break;
            case VLC_CODEC_I444_16L:
            case VLC_CODEC_I444:
            case VLC_CODEC_I420:
                psz_sampler[0] = "SAMPLE_TRIPLANAR_TO_YUVA";
                psz_shader_resource_views[0] = "3"; shader_views[0] = 3;
                break;
            case VLC_CODEC_YUVA:
                psz_sampler[0] = "SAMPLE_PLANAR_YUVA_TO_YUVA";
                psz_shader_resource_views[0] = "4"; shader_views[0] = 4;
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
                psz_src_to_linear = "SRC_TRANSFER_PQ";
                src_transfer = TRANSFER_FUNC_LINEAR;
                break;
            case TRANSFER_FUNC_HLG:
                psz_src_to_linear = "SRC_TRANSFER_HLG";
                src_transfer = TRANSFER_FUNC_LINEAR;
                break;
            case TRANSFER_FUNC_BT709:
                psz_src_to_linear = "SRC_TRANSFER_709";
                src_transfer = TRANSFER_FUNC_LINEAR;
                break;
            case TRANSFER_FUNC_BT470_M:
            case TRANSFER_FUNC_SRGB:
                psz_src_to_linear = "SRC_TRANSFER_SRGB";
                src_transfer = TRANSFER_FUNC_LINEAR;
                break;
            case TRANSFER_FUNC_BT470_BG:
                psz_src_to_linear = "SRC_TRANSFER_470BG";
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
                    psz_linear_to_display = "DST_TRANSFER_SRGB";

                    if (transfer == TRANSFER_FUNC_SMPTE_ST2084 || transfer == TRANSFER_FUNC_HLG)
                    {
                        /* HDR tone mapping */
                        psz_tone_mapping = "TONE_MAP_HABLE";
                    }
                }
                else
                    msg_Warn(o, "don't know how to transfer from %d to sRGB", src_transfer);
                break;

            case TRANSFER_FUNC_SMPTE_ST2084:
                if (src_transfer == TRANSFER_FUNC_LINEAR)
                {
                    /* Linear to ST2084 */
                    psz_linear_to_display = "DST_TRANSFER_PQ";
                }
                else
                    msg_Warn(o, "don't know how to transfer from %d to SMPTE ST 2084", src_transfer);
                break;
            default:
                msg_Warn(o, "don't know how to transfer from %d to %d", src_transfer, display->transfer);
                break;
        }
    }

    bool dst_full_range = display->b_full_range;
    if (!DxgiIsRGBFormat(dxgi_fmt) && DxgiIsRGBFormat(display->pixelFormat))
    {
        if (src_full_range && !dst_full_range)
            msg_Warn(o, "unsupported display full range YUV on studio range RGB");
    }

    HRESULT hr;
    hr = CompilePixelShaderBlob(o, compiler, feature_level,
                                psz_sampler[0], psz_shader_resource_views[0],
                                psz_src_to_linear,
                                psz_linear_to_display,
                                psz_tone_mapping,
                                &pPSBlob[0]);
    if (SUCCEEDED(hr) && psz_sampler[1])
    {
        hr = CompilePixelShaderBlob(o, compiler, feature_level,
                                    psz_sampler[1],  psz_shader_resource_views[1],
                                    psz_src_to_linear,
                                    psz_linear_to_display,
                                    psz_tone_mapping,
                                    &pPSBlob[1]);
        if (FAILED(hr))
            D3D_ShaderBlobRelease(&pPSBlob[0]);
    }

    return hr;
}

HRESULT D3D_CompileVertexShader(vlc_object_t *obj, const d3d_shader_compiler_t *compiler,
                                D3D_FEATURE_LEVEL feature_level, bool flat,
                                d3d_shader_blob *blob)
{
    D3D_SHADER_MACRO defines[] = {
         { "HAS_PROJECTION", flat ? "0" : "1" },
         { NULL, NULL },
    };
    return CompileShader(obj, compiler, feature_level, globVertexShader, false, defines, blob);
}


int D3D_CreateShaderCompiler(vlc_object_t *obj, d3d_shader_compiler_t **compiler)
{
    *compiler = calloc(1, sizeof(d3d_shader_compiler_t));
    if (unlikely(*compiler == NULL))
        return VLC_ENOMEM;
#ifndef VLC_WINSTORE_APP
    /* d3dcompiler_47 is the latest on windows 10 */
    for (int i = 47; i > 41; --i)
    {
        WCHAR filename[19];
        _snwprintf(filename, ARRAY_SIZE(filename), TEXT("D3DCOMPILER_%d.dll"), i);
        (*compiler)->compiler_dll = LoadLibrary(filename);
        if ((*compiler)->compiler_dll) break;
    }
    if ((*compiler)->compiler_dll)
        (*compiler)->OurD3DCompile = (pD3DCompile)((void*)GetProcAddress((*compiler)->compiler_dll, "D3DCompile"));

    if (!(*compiler)->OurD3DCompile) {
        msg_Err(obj, "Cannot locate reference to D3DCompile in d3dcompiler DLL");
        FreeLibrary((*compiler)->compiler_dll);
        free(*compiler);
        return VLC_EGENERIC;
    }
#endif // !VLC_WINSTORE_APP

    return VLC_SUCCESS;
}

void D3D_ReleaseShaderCompiler(d3d_shader_compiler_t *compiler)
{
#ifndef VLC_WINSTORE_APP
    if (compiler->compiler_dll)
        FreeLibrary(compiler->compiler_dll);
#endif // !VLC_WINSTORE_APP
    free(compiler);
}

