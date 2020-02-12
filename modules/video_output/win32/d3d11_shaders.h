/*****************************************************************************
 * d3d11_shaders.h: Direct3D11 Shaders
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

#ifndef VLC_D3D11_SHADERS_H
#define VLC_D3D11_SHADERS_H

#include <d3dcompiler.h>
#include "../../video_chroma/d3d11_fmt.h"

typedef struct
{
    HINSTANCE                 compiler_dll; /* handle of the opened d3dcompiler dll */
    pD3DCompile               OurD3DCompile;
} d3d11_shaders_t;

#include <vlc_es.h>

#define DEFAULT_BRIGHTNESS         100
#define DEFAULT_SRGB_BRIGHTNESS    100
#define MAX_HLG_BRIGHTNESS        1000
#define MAX_PQ_BRIGHTNESS        10000

typedef struct {
    video_color_primaries_t  primaries;
    video_transfer_func_t    transfer;
    video_color_space_t      color;
    bool                     b_full_range;
    unsigned                 luminance_peak;
    const d3d_format_t       *pixelFormat;
} display_info_t;

/* structures passed to the pixel shader */
typedef struct {
    FLOAT Opacity;
    FLOAT BoundaryX;
    FLOAT BoundaryY;
    FLOAT LuminanceScale;
} PS_CONSTANT_BUFFER;

typedef struct {
    FLOAT WhitePoint[4*4];
    FLOAT Colorspace[4*4];
    FLOAT Primaries[4*4];
} PS_COLOR_TRANSFORM;

typedef struct {
    FLOAT View[4*4];
    FLOAT Zoom[4*4];
    FLOAT Projection[4*4];
} VS_PROJECTION_CONST;

extern const char* globVertexShaderFlat;
extern const char* globVertexShaderProjection;

/* Vertex Shader compiled sructures */
typedef struct {
    ID3D11VertexShader        *shader;
    ID3D11InputLayout         *layout;
} d3d_vshader_t;

/* A Quad is texture that can be displayed in a rectangle */
typedef struct
{
    picture_sys_d3d11_t       picSys;
    const d3d_format_t        *textureFormat;
    UINT                      resourceCount;
    ID3D11Buffer              *pVertexBuffer;
    UINT                      vertexCount;
    UINT                      vertexStride;
    ID3D11Buffer              *pIndexBuffer;
    UINT                      indexCount;
    ID3D11Buffer              *pVertexShaderConstants;
    ID3D11Buffer              *pPixelShaderConstants[2];
    UINT                       PSConstantsCount;
    ID3D11PixelShader         *d3dpixelShader[D3D11_MAX_SHADER_VIEW];
    ID3D11SamplerState        *d3dsampState[2];
    D3D11_VIEWPORT            cropViewport[D3D11_MAX_SHADER_VIEW];
    unsigned int              i_width;
    unsigned int              i_height;
    video_projection_mode_t   projection;

    PS_CONSTANT_BUFFER        shaderConstants;
} d3d_quad_t;

#define D3D11_MAX_RENDER_TARGET    2

bool IsRGBShader(const d3d_format_t *);

int D3D11_InitShaders(vlc_object_t *, d3d11_shaders_t *);
void D3D11_ReleaseShaders(d3d11_shaders_t *);

HRESULT D3D11_CompilePixelShader(vlc_object_t *, const d3d11_shaders_t *, bool legacy_shader,
                                 d3d11_device_t *, const display_info_t *,
                                 video_transfer_func_t, video_color_primaries_t,
                                 bool src_full_range,
                                 d3d_quad_t *);
#define D3D11_CompilePixelShader(a,b,c,d,e,f,g,h,i) \
    D3D11_CompilePixelShader(VLC_OBJECT(a),b,c,d,e,f,g,h,i)
void D3D11_ReleasePixelShader(d3d_quad_t *);

HRESULT D3D11_CompileFlatVertexShader(vlc_object_t *, const d3d11_shaders_t *, d3d11_device_t *, d3d_vshader_t *);
#define D3D11_CompileFlatVertexShader(a,b,c,d) D3D11_CompileFlatVertexShader(VLC_OBJECT(a),b,c,d)

HRESULT D3D11_CompileProjectionVertexShader(vlc_object_t *, const d3d11_shaders_t *, d3d11_device_t *, d3d_vshader_t *);
#define D3D11_CompileProjectionVertexShader(a,b,c,d) D3D11_CompileProjectionVertexShader(VLC_OBJECT(a),b,c,d)

float GetFormatLuminance(vlc_object_t *, const video_format_t *);
#define GetFormatLuminance(a,b)  GetFormatLuminance(VLC_OBJECT(a),b)

HRESULT D3D11_CreateRenderTargets(d3d11_device_t *, ID3D11Resource *, const d3d_format_t *,
                                  ID3D11RenderTargetView *output[D3D11_MAX_RENDER_TARGET]);

void D3D11_ClearRenderTargets(d3d11_device_t *, const d3d_format_t *,
                              ID3D11RenderTargetView *targets[D3D11_MAX_RENDER_TARGET]);

void D3D11_SetVertexShader(d3d_vshader_t *dst, d3d_vshader_t *src);
void D3D11_ReleaseVertexShader(d3d_vshader_t *);

#endif /* VLC_D3D11_SHADERS_H */
