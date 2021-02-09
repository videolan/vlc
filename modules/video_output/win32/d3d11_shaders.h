/*****************************************************************************
 * d3d11_shaders.h: Direct3D11 Shaders
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

#ifndef VLC_D3D11_SHADERS_H
#define VLC_D3D11_SHADERS_H

#include "d3d_shaders.h"

#include "../../video_chroma/d3d11_fmt.h"

/* Vertex Shader compiled sructures */
typedef struct {
    ID3D11VertexShader        *shader;
    ID3D11InputLayout         *layout;
} d3d11_vertex_shader_t;

/* A Quad is texture that can be displayed in a rectangle */
typedef struct
{
    picture_sys_d3d11_t       picSys;
    d3d_quad_t                generic;
    UINT                      resourceCount;
    ID3D11Buffer              *pVertexBuffer;
    ID3D11Buffer              *pIndexBuffer;
    ID3D11Buffer              *viewpointShaderConstant;
    ID3D11Buffer              *pPixelShaderConstants[2];
    UINT                       PSConstantsCount;
    ID3D11PixelShader         *d3dpixelShader[DXGI_MAX_RENDER_TARGET];
    ID3D11SamplerState        *SamplerStates[2];
    D3D11_VIEWPORT            cropViewport[DXGI_MAX_RENDER_TARGET];

    PS_CONSTANT_BUFFER        pConstants;
    PS_COLOR_TRANSFORM        cConstants;
    VS_PROJECTION_CONST       vConstants;
} d3d11_quad_t;

HRESULT D3D11_CompilePixelShader(vlc_object_t *, const d3d_shader_compiler_t *,
                                 d3d11_device_t *, bool texture_array, size_t texture_count,
                                 const display_info_t *, bool sharp,
                                 video_transfer_func_t, video_color_primaries_t,
                                 bool src_full_range,
                                 d3d11_quad_t *);
#define D3D11_CompilePixelShader(a,b,c,d,e,f,g,h,i,j,k) \
    D3D11_CompilePixelShader(VLC_OBJECT(a),b,c,d,e,f,g,h,i,j,k)
void D3D11_ReleasePixelShader(d3d11_quad_t *);

HRESULT D3D11_CompileFlatVertexShader(vlc_object_t *, const d3d_shader_compiler_t *, d3d11_device_t *, d3d11_vertex_shader_t *);
#define D3D11_CompileFlatVertexShader(a,b,c,d) D3D11_CompileFlatVertexShader(VLC_OBJECT(a),b,c,d)

HRESULT D3D11_CompileProjectionVertexShader(vlc_object_t *, const d3d_shader_compiler_t *, d3d11_device_t *, d3d11_vertex_shader_t *);
#define D3D11_CompileProjectionVertexShader(a,b,c,d) D3D11_CompileProjectionVertexShader(VLC_OBJECT(a),b,c,d)

HRESULT D3D11_CreateRenderTargets(d3d11_device_t *, ID3D11Resource *, const d3d_format_t *,
                                  ID3D11RenderTargetView *output[DXGI_MAX_RENDER_TARGET]);

void D3D11_ClearRenderTargets(d3d11_device_t *, const d3d_format_t *,
                              ID3D11RenderTargetView *targets[DXGI_MAX_RENDER_TARGET]);

void D3D11_ReleaseVertexShader(d3d11_vertex_shader_t *);

#endif /* VLC_D3D11_SHADERS_H */
