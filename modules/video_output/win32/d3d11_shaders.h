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
#include "d3d_dynamic_shader.h"

#include "../../video_chroma/d3d11_fmt.h"

#include <wrl/client.h>

/* Vertex Shader compiled structures */
struct d3d11_vertex_shader_t {
    Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  layout;
};

/* A Quad is texture that can be displayed in a rectangle */
struct d3d11_quad_t
{
    d3d11_quad_t()
    {
        video_format_Init(&quad_fmt, 0);
    }

    ~d3d11_quad_t()
    {
        ReleaseD3D11PictureSys(&picSys);
        video_format_Clean(&quad_fmt);
    }

    void Reset();
    void UpdateViewport(const RECT *, const d3d_format_t *display);

    picture_sys_d3d11_t       picSys = {};
    d3d_quad_t                generic = {};
    video_format_t            quad_fmt = {};
    UINT                      resourceCount = 0;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> viewpointShaderConstant;
    Microsoft::WRL::ComPtr<ID3D11Buffer> pPixelShaderConstants;
    UINT                       PSConstantsCount = 0;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  d3dpixelShader[DXGI_MAX_RENDER_TARGET];
    Microsoft::WRL::ComPtr<ID3D11SamplerState> SamplerStates[2];
    D3D11_VIEWPORT            cropViewport[DXGI_MAX_RENDER_TARGET];

    PS_CONSTANT_BUFFER        pConstants;
    VS_PROJECTION_CONST       vConstants;
};

HRESULT D3D11_CompilePixelShaderBlob(vlc_object_t *, const d3d_shader_compiler_t *,
                                 d3d11_device_t *, const display_info_t *,
                                 video_transfer_func_t,
                                 bool src_full_range,
                                 const d3d11_quad_t *, d3d_shader_blob pPSBlob[DXGI_MAX_RENDER_TARGET]);
#define D3D11_CompilePixelShaderBlob(a,b,c,d,e,f,g,h) \
    D3D11_CompilePixelShaderBlob(VLC_OBJECT(a),b,c,d,e,f,g,h)
HRESULT D3D11_SetQuadPixelShader(vlc_object_t *, d3d11_device_t *,
                                 bool sharp,
                                 d3d11_quad_t *quad, d3d_shader_blob pPSBlob[DXGI_MAX_RENDER_TARGET]);

HRESULT D3D11_CompileVertexShaderBlob(vlc_object_t *, const d3d_shader_compiler_t *,
                                      d3d11_device_t *, bool flat, d3d_shader_blob *);

HRESULT D3D11_CreateVertexShader(vlc_object_t *, d3d_shader_blob *, d3d11_device_t *, d3d11_vertex_shader_t *);
#define D3D11_CreateVertexShader(a,b,c,d) D3D11_CreateVertexShader(VLC_OBJECT(a),b,c,d)

HRESULT D3D11_CreateRenderTargets(d3d11_device_t *, ID3D11Resource *, const d3d_format_t *,
                                  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> output[DXGI_MAX_RENDER_TARGET]);

void D3D11_ClearRenderTargets(d3d11_device_t *, const d3d_format_t *,
                              Microsoft::WRL::ComPtr<ID3D11RenderTargetView> targets[DXGI_MAX_RENDER_TARGET]);

void D3D11_ReleaseVertexShader(d3d11_vertex_shader_t *);

#endif /* VLC_D3D11_SHADERS_H */
