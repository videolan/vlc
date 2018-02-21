/*****************************************************************************
 * d3d11_quad.c: Direct3D11 Qaud handling
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <vlc_common.h>

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < _WIN32_WINNT_WIN7
# undef _WIN32_WINNT
# define _WIN32_WINNT _WIN32_WINNT_WIN7
#endif

#define COBJMACROS
#include <d3d11.h>

#include "d3d11_quad.h"

#define SPHERE_SLICES 128
#define nbLatBands SPHERE_SLICES
#define nbLonBands SPHERE_SLICES

void D3D11_RenderQuad(d3d11_device_t *d3d_dev, d3d_quad_t *quad,
                      ID3D11ShaderResourceView *resourceView[D3D11_MAX_SHADER_VIEW],
                      ID3D11RenderTargetView *d3drenderTargetView)
{
    UINT offset = 0;

    ID3D11DeviceContext_OMSetRenderTargets(d3d_dev->d3dcontext, 1, &d3drenderTargetView, NULL);

    /* Render the quad */
    /* vertex shader */
    ID3D11DeviceContext_IASetVertexBuffers(d3d_dev->d3dcontext, 0, 1, &quad->pVertexBuffer, &quad->vertexStride, &offset);
    ID3D11DeviceContext_IASetIndexBuffer(d3d_dev->d3dcontext, quad->pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    if ( quad->pVertexShaderConstants )
        ID3D11DeviceContext_VSSetConstantBuffers(d3d_dev->d3dcontext, 0, 1, &quad->pVertexShaderConstants);

    ID3D11DeviceContext_VSSetShader(d3d_dev->d3dcontext, quad->d3dvertexShader, NULL, 0);

    /* pixel shader */
    ID3D11DeviceContext_PSSetShader(d3d_dev->d3dcontext, quad->d3dpixelShader, NULL, 0);

    ID3D11DeviceContext_PSSetConstantBuffers(d3d_dev->d3dcontext, 0, quad->PSConstantsCount, quad->pPixelShaderConstants);
    assert(quad->resourceCount <= D3D11_MAX_SHADER_VIEW);
    ID3D11DeviceContext_PSSetShaderResources(d3d_dev->d3dcontext, 0, quad->resourceCount, resourceView);

    ID3D11DeviceContext_RSSetViewports(d3d_dev->d3dcontext, 1, &quad->cropViewport);

    ID3D11DeviceContext_DrawIndexed(d3d_dev->d3dcontext, quad->indexCount, 0, 0);
}

static bool AllocQuadVertices(vlc_object_t *o, d3d11_device_t *d3d_dev, d3d_quad_t *quad)
{
    HRESULT hr;

    switch (quad->projection)
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
        msg_Warn(o, "Projection mode %d not handled", quad->projection);
        return false;
    }

    quad->vertexStride = sizeof(d3d_vertex_t);

    D3D11_BUFFER_DESC bd;
    memset(&bd, 0, sizeof(bd));
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = quad->vertexStride * quad->vertexCount;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = ID3D11Device_CreateBuffer(d3d_dev->d3ddevice, &bd, NULL, &quad->pVertexBuffer);
    if(FAILED(hr)) {
      msg_Err(o, "Failed to create vertex buffer. (hr=%lX)", hr);
      return false;
    }

    /* create the index of the vertices */
    D3D11_BUFFER_DESC quadDesc = {
        .Usage = D3D11_USAGE_DYNAMIC,
        .ByteWidth = sizeof(WORD) * quad->indexCount,
        .BindFlags = D3D11_BIND_INDEX_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };

    hr = ID3D11Device_CreateBuffer(d3d_dev->d3ddevice, &quadDesc, NULL, &quad->pIndexBuffer);
    if(FAILED(hr)) {
        msg_Err(o, "Could not create the quad indices. (hr=0x%lX)", hr);
        ID3D11Buffer_Release(quad->pVertexBuffer);
        quad->pVertexBuffer = NULL;
        return false;
    }

    return true;
}

void D3D11_ReleaseQuad(d3d_quad_t *quad)
{
    if (quad->pPixelShaderConstants[0])
    {
        ID3D11Buffer_Release(quad->pPixelShaderConstants[0]);
        quad->pPixelShaderConstants[0] = NULL;
    }
    if (quad->pPixelShaderConstants[1])
    {
        ID3D11Buffer_Release(quad->pPixelShaderConstants[1]);
        quad->pPixelShaderConstants[1] = NULL;
    }
    if (quad->pVertexBuffer)
    {
        ID3D11Buffer_Release(quad->pVertexBuffer);
        quad->pVertexBuffer = NULL;
    }
    quad->d3dvertexShader = NULL;
    if (quad->pIndexBuffer)
    {
        ID3D11Buffer_Release(quad->pIndexBuffer);
        quad->pIndexBuffer = NULL;
    }
    if (quad->pVertexShaderConstants)
    {
        ID3D11Buffer_Release(quad->pVertexShaderConstants);
        quad->pVertexShaderConstants = NULL;
    }
    ReleasePictureSys(&quad->picSys);
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
    dst_data[0].texture.x = 0.0f;
    dst_data[0].texture.y = 1.0f;

    // bottom right
    dst_data[1].position.z = 0.0f;
    dst_data[1].texture.x = 1.0f;
    dst_data[1].texture.y = 1.0f;

    // top right
    dst_data[2].position.z = 0.0f;
    dst_data[2].texture.x = 1.0f;
    dst_data[2].texture.y = 0.0f;

    // top left
    dst_data[3].position.z = 0.0f;
    dst_data[3].texture.x = 0.0f;
    dst_data[3].texture.y = 0.0f;

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
    const float scaleX = (float)(output->right  - output->left) / quad->i_width;
    const float scaleY = (float)(output->bottom - output->top)   / quad->i_height;
    for (unsigned lat = 0; lat <= nbLatBands; lat++) {
        float theta = lat * (float) M_PI / nbLatBands;
        float sinTheta, cosTheta;

        sincosf(theta, &sinTheta, &cosTheta);

        for (unsigned lon = 0; lon <= nbLonBands; lon++) {
            float phi = lon * 2 * (float) M_PI / nbLonBands;
            float sinPhi, cosPhi;

            sincosf(phi, &sinPhi, &cosPhi);

            float x = cosPhi * sinTheta;
            float y = cosTheta;
            float z = sinPhi * sinTheta;

            unsigned off1 = lat * (nbLonBands + 1) + lon;
            dst_data[off1].position.x = SPHERE_RADIUS * x;
            dst_data[off1].position.y = SPHERE_RADIUS * y;
            dst_data[off1].position.z = SPHERE_RADIUS * z;

            dst_data[off1].texture.x = scaleX * lon / (float) nbLonBands; // 0(left) to 1(right)
            dst_data[off1].texture.y = scaleY * lat / (float) nbLatBands; // 0(top) to 1 (bottom)
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
    static const float coord[] = {
        -1.0,    1.0,    -1.0f, // front
        -1.0,    -1.0,   -1.0f,
        1.0,     1.0,    -1.0f,
        1.0,     -1.0,   -1.0f,

        -1.0,    1.0,    1.0f, // back
        -1.0,    -1.0,   1.0f,
        1.0,     1.0,    1.0f,
        1.0,     -1.0,   1.0f,

        -1.0,    1.0,    -1.0f, // left
        -1.0,    -1.0,   -1.0f,
        -1.0,     1.0,    1.0f,
        -1.0,     -1.0,   1.0f,

        1.0f,    1.0,    -1.0f, // right
        1.0f,   -1.0,    -1.0f,
        1.0f,   1.0,     1.0f,
        1.0f,   -1.0,    1.0f,

        -1.0,    -1.0,    1.0f, // bottom
        -1.0,    -1.0,   -1.0f,
        1.0,     -1.0,    1.0f,
        1.0,     -1.0,   -1.0f,

        -1.0,    1.0,    1.0f, // top
        -1.0,    1.0,   -1.0f,
        1.0,     1.0,    1.0f,
        1.0,     1.0,   -1.0f,
    };

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

        dst_data[v].texture.x = tex[2 * v];
        dst_data[v].texture.y = tex[2 * v + 1];
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

#undef D3D11_UpdateQuadPosition
bool D3D11_UpdateQuadPosition( vlc_object_t *o, d3d11_device_t *d3d_dev, d3d_quad_t *quad,
                                const RECT *output, video_orientation_t orientation )
{
    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE mappedResource;

    if (unlikely(quad->pVertexBuffer == NULL))
        return false;

    /* create the vertices */
    hr = ID3D11DeviceContext_Map(d3d_dev->d3dcontext, (ID3D11Resource *)quad->pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr)) {
        msg_Err(o, "Failed to lock the vertex buffer (hr=0x%lX)", hr);
        return false;
    }
    d3d_vertex_t *dst_data = mappedResource.pData;

    /* create the vertex indices */
    hr = ID3D11DeviceContext_Map(d3d_dev->d3dcontext, (ID3D11Resource *)quad->pIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr)) {
        msg_Err(o, "Failed to lock the index buffer (hr=0x%lX)", hr);
        ID3D11DeviceContext_Unmap(d3d_dev->d3dcontext, (ID3D11Resource *)quad->pVertexBuffer, 0);
        return false;
    }
    WORD *triangle_pos = mappedResource.pData;

    switch (quad->projection)
    {
    case PROJECTION_MODE_RECTANGULAR:
        SetupQuadFlat(dst_data, output, quad, triangle_pos, orientation);
        break;
    case PROJECTION_MODE_EQUIRECTANGULAR:
        SetupQuadSphere(dst_data, output, quad, triangle_pos);
        break;
    case PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD:
        SetupQuadCube(dst_data, output, quad, triangle_pos);
        break;
    default:
        msg_Warn(o, "Projection mode %d not handled", quad->projection);
        return false;
    }

    ID3D11DeviceContext_Unmap(d3d_dev->d3dcontext, (ID3D11Resource *)quad->pIndexBuffer, 0);
    ID3D11DeviceContext_Unmap(d3d_dev->d3dcontext, (ID3D11Resource *)quad->pVertexBuffer, 0);

    return true;
}

static bool D3D11_ShaderUpdateConstants(vlc_object_t *o, d3d11_device_t *d3d_dev, d3d_quad_t *quad)
{
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = ID3D11DeviceContext_Map(d3d_dev->d3dcontext, (ID3D11Resource *)quad->pPixelShaderConstants[0], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr))
    {
        msg_Err(o, "Failed to lock the picture shader constants (hr=0x%lX)", hr);
        return false;
    }

    PS_CONSTANT_BUFFER *dst_data = mappedResource.pData;
    *dst_data = quad->shaderConstants;
    ID3D11DeviceContext_Unmap(d3d_dev->d3dcontext, (ID3D11Resource *)quad->pPixelShaderConstants[0], 0);
    return true;
}

#undef D3D11_UpdateQuadOpacity
void D3D11_UpdateQuadOpacity(vlc_object_t *o, d3d11_device_t *d3d_dev, d3d_quad_t *quad, float opacity)
{
    if (quad->shaderConstants.Opacity == opacity)
        return;

    float old = quad->shaderConstants.Opacity;
    quad->shaderConstants.Opacity = opacity;
    if (!D3D11_ShaderUpdateConstants(o, d3d_dev, quad))
        quad->shaderConstants.Opacity = old;
}

#undef D3D11_UpdateQuadLuminanceScale
void D3D11_UpdateQuadLuminanceScale(vlc_object_t *o, d3d11_device_t *d3d_dev, d3d_quad_t *quad, float luminanceScale)
{
    if (quad->shaderConstants.LuminanceScale == luminanceScale)
        return;

    float old = quad->shaderConstants.LuminanceScale;
    quad->shaderConstants.LuminanceScale = luminanceScale;
    if (!D3D11_ShaderUpdateConstants(o, d3d_dev, quad))
        quad->shaderConstants.LuminanceScale = old;
}

#undef D3D11_SetupQuad
int D3D11_SetupQuad(vlc_object_t *o, d3d11_device_t *d3d_dev, const video_format_t *fmt, d3d_quad_t *quad,
                    const display_info_t *displayFormat, const RECT *output,
                    const d3d_format_t *cfg, ID3D11PixelShader *d3dpixelShader, ID3D11VertexShader *d3dvertexShader,
                    video_projection_mode_t projection, video_orientation_t orientation)
{
    HRESULT hr;
    const bool RGB_shader = IsRGBShader(cfg);

    quad->shaderConstants.LuminanceScale = GetFormatLuminance(o, fmt) / (float)displayFormat->luminance_peak;

    /* pixel shader constant buffer */
    quad->shaderConstants.Opacity = 1.0;
    if (fmt->i_visible_width == fmt->i_width)
        quad->shaderConstants.BoundaryX = 1.0; /* let texture clamping happen */
    else
        quad->shaderConstants.BoundaryX = (FLOAT) (fmt->i_visible_width - 1) / fmt->i_width;
    if (fmt->i_visible_height == fmt->i_height)
        quad->shaderConstants.BoundaryY = 1.0; /* let texture clamping happen */
    else
        quad->shaderConstants.BoundaryY = (FLOAT) (fmt->i_visible_height - 1) / fmt->i_height;

    static_assert((sizeof(PS_CONSTANT_BUFFER)%16)==0,"Constant buffers require 16-byte alignment");
    D3D11_BUFFER_DESC constantDesc = {
        .Usage = D3D11_USAGE_DYNAMIC,
        .ByteWidth = sizeof(PS_CONSTANT_BUFFER),
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };
    D3D11_SUBRESOURCE_DATA constantInit = { .pSysMem = &quad->shaderConstants };
    hr = ID3D11Device_CreateBuffer(d3d_dev->d3ddevice, &constantDesc, &constantInit, &quad->pPixelShaderConstants[0]);
    if(FAILED(hr)) {
        msg_Err(o, "Could not create the pixel shader constant buffer. (hr=0x%lX)", hr);
        goto error;
    }

    FLOAT itu_black_level = 0.f;
    FLOAT itu_achromacy   = 0.f;
    if (!RGB_shader)
    {
        switch (cfg->bitsPerChannel)
        {
        case 8:
            /* Rec. ITU-R BT.709-6 §4.6 */
            itu_black_level  =              16.f / 255.f;
            itu_achromacy    =             128.f / 255.f;
            break;
        case 10:
            /* Rec. ITU-R BT.709-6 §4.6 */
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
    static const FLOAT COLORSPACE_BT601_TO_FULL[4*4] = {
        1.164383561643836f,                 0.f,  1.596026785714286f, 0.f,
        1.164383561643836f, -0.391762290094914f, -0.812967647237771f, 0.f,
        1.164383561643836f,  2.017232142857142f,                 0.f, 0.f,
                       0.f,                 0.f,                 0.f, 1.f,
    };
    /* see https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.709_conversion, in studio range */
    static const FLOAT COLORSPACE_BT709_TO_FULL[4*4] = {
        1.164383561643836f,                 0.f,  1.792741071428571f, 0.f,
        1.164383561643836f, -0.213248614273730f, -0.532909328559444f, 0.f,
        1.164383561643836f,  2.112401785714286f,                 0.f, 0.f,
                       0.f,                 0.f,                 0.f, 1.f,
    };
    /* see https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.2020_conversion, in studio range */
    static const FLOAT COLORSPACE_BT2020_TO_FULL[4*4] = {
        1.164383561643836f,  0.000000000000f,  1.678674107143f, 0.f,
        1.164383561643836f, -0.127007098661f, -0.440987687946f, 0.f,
        1.164383561643836f,  2.141772321429f,  0.000000000000f, 0.f,
                       0.f,              0.f,              0.f, 1.f,
    };

    PS_COLOR_TRANSFORM colorspace;

    memcpy(colorspace.WhitePoint, IDENTITY_4X4, sizeof(colorspace.WhitePoint));

    const FLOAT *ppColorspace;
    if (RGB_shader)
        ppColorspace = IDENTITY_4X4;
    else {
        switch (fmt->space){
            case COLOR_SPACE_BT709:
                ppColorspace = COLORSPACE_BT709_TO_FULL;
                break;
            case COLOR_SPACE_BT2020:
                ppColorspace = COLORSPACE_BT2020_TO_FULL;
                break;
            case COLOR_SPACE_BT601:
                ppColorspace = COLORSPACE_BT601_TO_FULL;
                break;
            default:
            case COLOR_SPACE_UNDEF:
                if( fmt->i_height > 576 )
                    ppColorspace = COLORSPACE_BT709_TO_FULL;
                else
                    ppColorspace = COLORSPACE_BT601_TO_FULL;
                break;
        }
        /* all matrices work in studio range and output in full range */
        colorspace.WhitePoint[0*4 + 3] = -itu_black_level;
        colorspace.WhitePoint[1*4 + 3] = -itu_achromacy;
        colorspace.WhitePoint[2*4 + 3] = -itu_achromacy;
    }

    memcpy(colorspace.Colorspace, ppColorspace, sizeof(colorspace.Colorspace));

    constantInit.pSysMem = &colorspace;

    static_assert((sizeof(PS_COLOR_TRANSFORM)%16)==0,"Constant buffers require 16-byte alignment");
    constantDesc.ByteWidth = sizeof(PS_COLOR_TRANSFORM);
    hr = ID3D11Device_CreateBuffer(d3d_dev->d3ddevice, &constantDesc, &constantInit, &quad->pPixelShaderConstants[1]);
    if(FAILED(hr)) {
        msg_Err(o, "Could not create the pixel shader constant buffer. (hr=0x%lX)", hr);
        goto error;
    }
    quad->PSConstantsCount = 2;
    quad->projection = projection;

    /* vertex shader constant buffer */
    if (projection == PROJECTION_MODE_EQUIRECTANGULAR
        || projection == PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD)
    {
        constantDesc.ByteWidth = sizeof(VS_PROJECTION_CONST);
        static_assert((sizeof(VS_PROJECTION_CONST)%16)==0,"Constant buffers require 16-byte alignment");
        hr = ID3D11Device_CreateBuffer(d3d_dev->d3ddevice, &constantDesc, NULL, &quad->pVertexShaderConstants);
        if(FAILED(hr)) {
            msg_Err(o, "Could not create the vertex shader constant buffer. (hr=0x%lX)", hr);
            goto error;
        }
    }

    quad->picSys.formatTexture = cfg->formatTexture;
    quad->picSys.context = d3d_dev->d3dcontext;
    ID3D11DeviceContext_AddRef(quad->picSys.context);

    if (!AllocQuadVertices(o, d3d_dev, quad))
        goto error;
    if (!D3D11_UpdateQuadPosition(o, d3d_dev, quad, output, orientation))
        goto error;

    quad->d3dpixelShader = d3dpixelShader;
    quad->d3dvertexShader = d3dvertexShader;
    quad->resourceCount = DxgiResourceCount(quad->formatInfo);

    return VLC_SUCCESS;

error:
    D3D11_ReleaseQuad(quad);
    return VLC_EGENERIC;
}
