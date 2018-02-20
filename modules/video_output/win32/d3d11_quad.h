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

typedef struct {
    FLOAT Opacity;
    FLOAT BoundaryX;
    FLOAT BoundaryY;
    FLOAT LuminanceScale;
} PS_CONSTANT_BUFFER;

/* A Quad is texture that can be displayed in a rectangle */
typedef struct
{
    picture_sys_t             picSys;
    UINT                      resourceCount;
    ID3D11Buffer              *pVertexBuffer;
    UINT                      vertexCount;
    ID3D11VertexShader        *d3dvertexShader;
    ID3D11Buffer              *pIndexBuffer;
    UINT                      indexCount;
    ID3D11Buffer              *pVertexShaderConstants;
    ID3D11Buffer              *pPixelShaderConstants[2];
    UINT                       PSConstantsCount;
    ID3D11PixelShader         *d3dpixelShader;
    D3D11_VIEWPORT            cropViewport;
    unsigned int              i_width;
    unsigned int              i_height;
    video_projection_mode_t   projection;

    PS_CONSTANT_BUFFER        shaderConstants;
} d3d_quad_t;

#endif /* VLC_D3D11_QUAD_H */
