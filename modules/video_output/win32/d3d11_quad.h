/*****************************************************************************
 * d3d11_quad.h: Direct3D11 Quad handling
 *****************************************************************************
 * Copyright (C) 2017-2018 VLC authors and VideoLAN
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

#ifndef VLC_D3D11_QUAD_H
#define VLC_D3D11_QUAD_H

#include "../../video_chroma/d3d11_fmt.h"
#include "d3d11_shaders.h"

#define SPHERE_RADIUS 1.f

/* A Quad is texture that can be displayed in a rectangle */
typedef struct
{
    picture_sys_t             picSys;
    const d3d_format_t        *formatInfo;
    UINT                      resourceCount;
    ID3D11Buffer              *pVertexBuffer;
    UINT                      vertexCount;
    UINT                      vertexStride;
    ID3D11VertexShader        *d3dvertexShader;
    ID3D11Buffer              *pIndexBuffer;
    UINT                      indexCount;
    ID3D11Buffer              *pVertexShaderConstants;
    ID3D11Buffer              *pPixelShaderConstants[2];
    UINT                       PSConstantsCount;
    ID3D11PixelShader         *d3dpixelShader;
    ID3D11InputLayout         *pVertexLayout;
    D3D11_VIEWPORT            cropViewport;
    unsigned int              i_width;
    unsigned int              i_height;
    video_projection_mode_t   projection;

    PS_CONSTANT_BUFFER        shaderConstants;
} d3d_quad_t;

/* matches the D3D11_INPUT_ELEMENT_DESC we setup */
typedef struct d3d_vertex_t {
    struct {
        FLOAT x;
        FLOAT y;
        FLOAT z;
    } position;
    struct {
        FLOAT x;
        FLOAT y;
    } texture;
} d3d_vertex_t;

void D3D11_RenderQuad(d3d11_device_t *, d3d_quad_t *,
                      ID3D11ShaderResourceView *resourceViews[D3D11_MAX_SHADER_VIEW],
                      ID3D11RenderTargetView *);

void D3D11_ReleaseQuad(d3d_quad_t *);

int D3D11_SetupQuad(vlc_object_t *, d3d11_device_t *, const video_format_t *, d3d_quad_t *,
                    const display_info_t *, const RECT *,
                    ID3D11VertexShader *, ID3D11InputLayout *, video_projection_mode_t,
                    video_orientation_t);
#define D3D11_SetupQuad(a,b,c,d,e,f,g,h,i,j)  D3D11_SetupQuad(VLC_OBJECT(a),b,c,d,e,f,g,h,i,j)

bool D3D11_UpdateQuadPosition( vlc_object_t *, d3d11_device_t *, d3d_quad_t *,
                               const RECT *output, video_orientation_t );
#define D3D11_UpdateQuadPosition(a,b,c,d,e)  D3D11_UpdateQuadPosition(VLC_OBJECT(a),b,c,d,e)

void D3D11_UpdateQuadOpacity(vlc_object_t *, d3d11_device_t *, d3d_quad_t *, float opacity);
#define D3D11_UpdateQuadOpacity(a,b,c,d)  D3D11_UpdateQuadOpacity(VLC_OBJECT(a),b,c,d)

void D3D11_UpdateQuadLuminanceScale(vlc_object_t *, d3d11_device_t *, d3d_quad_t *, float luminanceScale);
#define D3D11_UpdateQuadLuminanceScale(a,b,c,d)  D3D11_UpdateQuadLuminanceScale(VLC_OBJECT(a),b,c,d)

#endif /* VLC_D3D11_QUAD_H */
