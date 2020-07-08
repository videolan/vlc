/*****************************************************************************
 * d3d11_quad.c: Direct3D11 Quad handling
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

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0601 // _WIN32_WINNT_WIN7
# undef _WIN32_WINNT
# define _WIN32_WINNT 0x0601 // _WIN32_WINNT_WIN7
#endif

#include <assert.h>
#include <vlc_common.h>

#define COBJMACROS
#include <d3d11.h>

#include "d3d11_quad.h"
#include "common.h"

#define SPHERE_SLICES 128
#define nbLatBands SPHERE_SLICES
#define nbLonBands SPHERE_SLICES

void D3D11_RenderQuad(d3d11_device_t *d3d_dev, d3d_quad_t *quad, d3d_vshader_t *vsshader,
                      ID3D11ShaderResourceView *resourceView[D3D11_MAX_SHADER_VIEW],
                      d3d11_select_plane_t selectPlane, void *selectOpaque)
{
    UINT offset = 0;

    /* Render the quad */
    ID3D11DeviceContext_IASetPrimitiveTopology(d3d_dev->d3dcontext, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    /* vertex shader */
    ID3D11DeviceContext_IASetInputLayout(d3d_dev->d3dcontext, vsshader->layout);
    ID3D11DeviceContext_IASetVertexBuffers(d3d_dev->d3dcontext, 0, 1, &quad->pVertexBuffer, &quad->vertexStride, &offset);
    ID3D11DeviceContext_IASetIndexBuffer(d3d_dev->d3dcontext, quad->pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    if ( quad->pVertexShaderConstants )
        ID3D11DeviceContext_VSSetConstantBuffers(d3d_dev->d3dcontext, 0, 1, &quad->pVertexShaderConstants);

    ID3D11DeviceContext_VSSetShader(d3d_dev->d3dcontext, vsshader->shader, NULL, 0);

    if (quad->d3dsampState[0])
        ID3D11DeviceContext_PSSetSamplers(d3d_dev->d3dcontext, 0, 2, quad->d3dsampState);

    /* pixel shader */
    ID3D11DeviceContext_PSSetConstantBuffers(d3d_dev->d3dcontext, 0, ARRAY_SIZE(quad->pPixelShaderConstants), quad->pPixelShaderConstants);
    assert(quad->resourceCount <= D3D11_MAX_SHADER_VIEW);
    ID3D11DeviceContext_PSSetShaderResources(d3d_dev->d3dcontext, 0, quad->resourceCount, resourceView);

    for (size_t i=0; i<D3D11_MAX_SHADER_VIEW; i++)
    {
        if (!quad->d3dpixelShader[i])
            break;

        if (unlikely(!selectPlane(selectOpaque, i)))
            continue;

        ID3D11DeviceContext_PSSetShader(d3d_dev->d3dcontext, quad->d3dpixelShader[i], NULL, 0);

        ID3D11DeviceContext_RSSetViewports(d3d_dev->d3dcontext, 1, &quad->cropViewport[i]);

        ID3D11DeviceContext_DrawIndexed(d3d_dev->d3dcontext, quad->indexCount, 0, 0);
    }

    /* force unbinding the input texture, otherwise we get:
     * OMSetRenderTargets: Resource being set to OM RenderTarget slot 0 is still bound on input! */
    ID3D11ShaderResourceView *reset[D3D11_MAX_SHADER_VIEW] = { 0 };
    ID3D11DeviceContext_PSSetShaderResources(d3d_dev->d3dcontext, 0, quad->resourceCount, reset);
}

static bool AllocQuadVertices(vlc_object_t *o, d3d11_device_t *d3d_dev, d3d_quad_t *quad, video_projection_mode_t projection)
{
    HRESULT hr;

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

    D3D11_BUFFER_DESC bd;
    memset(&bd, 0, sizeof(bd));
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = quad->vertexStride * quad->vertexCount;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = ID3D11Device_CreateBuffer(d3d_dev->d3ddevice, &bd, NULL, &quad->pVertexBuffer);
    if(FAILED(hr)) {
        msg_Err(o, "Failed to create vertex buffer. (hr=%lX)", hr);
        goto fail;
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
        goto fail;
    }

    return true;
fail:
    if (quad->pVertexBuffer)
    {
        ID3D11Buffer_Release(quad->pVertexBuffer);
        quad->pVertexBuffer = NULL;
    }
    if (quad->pVertexBuffer)
    {
        ID3D11Buffer_Release(quad->pIndexBuffer);
        quad->pIndexBuffer = NULL;
    }
    return false;
}

void D3D11_ReleaseQuad(d3d_quad_t *quad)
{
    if (quad->pPixelShaderConstants[PS_CONST_LUMI_BOUNDS])
    {
        ID3D11Buffer_Release(quad->pPixelShaderConstants[PS_CONST_LUMI_BOUNDS]);
        quad->pPixelShaderConstants[PS_CONST_LUMI_BOUNDS] = NULL;
    }
    if (quad->pPixelShaderConstants[PS_CONST_COLORSPACE])
    {
        ID3D11Buffer_Release(quad->pPixelShaderConstants[PS_CONST_COLORSPACE]);
        quad->pPixelShaderConstants[PS_CONST_COLORSPACE] = NULL;
    }
    if (quad->pVertexBuffer)
    {
        ID3D11Buffer_Release(quad->pVertexBuffer);
        quad->pVertexBuffer = NULL;
    }
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
    D3D11_ReleasePixelShader(quad);
    for (size_t i=0; i<2; i++)
    {
        if (quad->d3dsampState[i])
        {
            ID3D11SamplerState_Release(quad->d3dsampState[i]);
            quad->d3dsampState[i] = NULL;
        }
    }
    ReleaseD3D11PictureSys(&quad->picSys);
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
    bool result = true;
    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    d3d_vertex_t *dst_data;

    if (unlikely(quad->pVertexBuffer == NULL))
        return false;

    /* create the vertices */
    hr = ID3D11DeviceContext_Map(d3d_dev->d3dcontext, (ID3D11Resource *)quad->pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr)) {
        msg_Err(o, "Failed to lock the vertex buffer (hr=0x%lX)", hr);
        return false;
    }
    dst_data = mappedResource.pData;

    /* create the vertex indices */
    hr = ID3D11DeviceContext_Map(d3d_dev->d3dcontext, (ID3D11Resource *)quad->pIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr)) {
        msg_Err(o, "Failed to lock the index buffer (hr=0x%lX)", hr);
        ID3D11DeviceContext_Unmap(d3d_dev->d3dcontext, (ID3D11Resource *)quad->pVertexBuffer, 0);
        return false;
    }

    switch (quad->projection)
    {
    case PROJECTION_MODE_RECTANGULAR:
        SetupQuadFlat(dst_data, output, quad, mappedResource.pData, orientation);
        break;
    case PROJECTION_MODE_EQUIRECTANGULAR:
        SetupQuadSphere(dst_data, output, quad, mappedResource.pData);
        break;
    case PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD:
        SetupQuadCube(dst_data, output, quad, mappedResource.pData);
        break;
    default:
        msg_Warn(o, "Projection mode %d not handled", quad->projection);
        result = false;
    }

    ID3D11DeviceContext_Unmap(d3d_dev->d3dcontext, (ID3D11Resource *)quad->pIndexBuffer, 0);
    ID3D11DeviceContext_Unmap(d3d_dev->d3dcontext, (ID3D11Resource *)quad->pVertexBuffer, 0);

    return result;
}

static bool ShaderUpdateConstants(vlc_object_t *o, d3d11_device_t *d3d_dev, d3d_quad_t *quad, size_t index, void *new_buf)
{
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = ID3D11DeviceContext_Map(d3d_dev->d3dcontext, (ID3D11Resource *)quad->pPixelShaderConstants[index], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr))
    {
        msg_Err(o, "Failed to lock the picture shader constants (hr=0x%lX)", hr);
        return false;
    }

    if (index == PS_CONST_LUMI_BOUNDS)
        memcpy(mappedResource.pData, new_buf,sizeof(PS_CONSTANT_BUFFER));
    else
        memcpy(mappedResource.pData, new_buf,sizeof(PS_COLOR_TRANSFORM));
    ID3D11DeviceContext_Unmap(d3d_dev->d3dcontext, (ID3D11Resource *)quad->pPixelShaderConstants[index], 0);
    return true;
}

#undef D3D11_UpdateQuadOpacity
void D3D11_UpdateQuadOpacity(vlc_object_t *o, d3d11_device_t *d3d_dev, d3d_quad_t *quad, float opacity)
{
    if (quad->shaderConstants.Opacity == opacity)
        return;

    float old = quad->shaderConstants.Opacity;
    quad->shaderConstants.Opacity = opacity;
    if (!ShaderUpdateConstants(o, d3d_dev, quad, PS_CONST_LUMI_BOUNDS, &quad->shaderConstants))
        quad->shaderConstants.Opacity = old;
}

#undef D3D11_UpdateQuadLuminanceScale
void D3D11_UpdateQuadLuminanceScale(vlc_object_t *o, d3d11_device_t *d3d_dev, d3d_quad_t *quad, float luminanceScale)
{
    if (quad->shaderConstants.LuminanceScale == luminanceScale)
        return;

    float old = quad->shaderConstants.LuminanceScale;
    quad->shaderConstants.LuminanceScale = luminanceScale;
    if (!ShaderUpdateConstants(o, d3d_dev, quad, PS_CONST_LUMI_BOUNDS, &quad->shaderConstants))
        quad->shaderConstants.LuminanceScale = old;
}

#undef D3D11_AllocateQuad
int D3D11_AllocateQuad(vlc_object_t *o, d3d11_device_t *d3d_dev,
                       video_projection_mode_t projection, d3d_quad_t *quad)
{
    HRESULT hr;
    static_assert((sizeof(PS_CONSTANT_BUFFER)%16)==0,"Constant buffers require 16-byte alignment");
    D3D11_BUFFER_DESC constantDesc = {
        .Usage = D3D11_USAGE_DYNAMIC,
        .ByteWidth = sizeof(PS_CONSTANT_BUFFER),
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };
    hr = ID3D11Device_CreateBuffer(d3d_dev->d3ddevice, &constantDesc, NULL, &quad->pPixelShaderConstants[PS_CONST_LUMI_BOUNDS]);
    if(FAILED(hr)) {
        msg_Err(o, "Could not create the pixel shader constant buffer. (hr=0x%lX)", hr);
        goto error;
    }

    static_assert((sizeof(PS_COLOR_TRANSFORM)%16)==0,"Constant buffers require 16-byte alignment");
    constantDesc.ByteWidth = sizeof(PS_COLOR_TRANSFORM);
    hr = ID3D11Device_CreateBuffer(d3d_dev->d3ddevice, &constantDesc, NULL, &quad->pPixelShaderConstants[PS_CONST_COLORSPACE]);
    if(FAILED(hr)) {
        msg_Err(o, "Could not create the pixel shader colorspace buffer. (hr=0x%lX)", hr);
        goto error;
    }

    if (projection == PROJECTION_MODE_EQUIRECTANGULAR || projection == PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD)
    {
        static_assert((sizeof(VS_PROJECTION_CONST)%16)==0,"Constant buffers require 16-byte alignment");
        constantDesc.ByteWidth = sizeof(VS_PROJECTION_CONST);
        hr = ID3D11Device_CreateBuffer(d3d_dev->d3ddevice, &constantDesc, NULL, &quad->pVertexShaderConstants);
        if(FAILED(hr)) {
            msg_Err(o, "Could not create the vertex shader constant buffer. (hr=0x%lX)", hr);
            goto error;
        }
    }

    if (!AllocQuadVertices(o, d3d_dev, quad, projection))
        goto error;
    quad->projection = projection;

    return VLC_SUCCESS;

error:
    D3D11_ReleaseQuad(quad);
    return VLC_EGENERIC;
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

#undef D3D11_SetupQuad
int D3D11_SetupQuad(vlc_object_t *o, d3d11_device_t *d3d_dev, const video_format_t *fmt, d3d_quad_t *quad,
                    const display_info_t *displayFormat, const RECT *output,
                    video_orientation_t orientation)
{
    const bool RGB_src_shader = IsRGBShader(quad->textureFormat);

    quad->shaderConstants.LuminanceScale = (float)displayFormat->luminance_peak / GetFormatLuminance(o, fmt);

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

    ShaderUpdateConstants(o, d3d_dev, quad, PS_CONST_LUMI_BOUNDS, &quad->shaderConstants);

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

    PS_COLOR_TRANSFORM colorspace;

    memcpy(colorspace.WhitePoint, IDENTITY_4X4, sizeof(colorspace.WhitePoint));

    const FLOAT *ppColorspace;
    if (RGB_src_shader == IsRGBShader(displayFormat->pixelFormat))
    {
        ppColorspace = IDENTITY_4X4;
    }
    else if (RGB_src_shader)
    {
        ppColorspace = COLORSPACE_FULL_RGBA_TO_BT601_YUV;
        colorspace.WhitePoint[0*4 + 3] = -itu_black_level;
        colorspace.WhitePoint[1*4 + 3] = itu_achromacy;
        colorspace.WhitePoint[2*4 + 3] = itu_achromacy;
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
        colorspace.WhitePoint[0*4 + 3] = -itu_black_level;
        colorspace.WhitePoint[1*4 + 3] = -itu_achromacy;
        colorspace.WhitePoint[2*4 + 3] = -itu_achromacy;
    }

    memcpy(colorspace.Colorspace, ppColorspace, sizeof(colorspace.Colorspace));

    if (fmt->primaries != displayFormat->primaries)
    {
        GetPrimariesTransform(colorspace.Primaries, fmt->primaries,
                              displayFormat->primaries);
    }

    ShaderUpdateConstants(o, d3d_dev, quad, PS_CONST_COLORSPACE, &colorspace);


    if (!D3D11_UpdateQuadPosition(o, d3d_dev, quad, output, orientation))
        return VLC_EGENERIC;

    for (size_t i=0; i<D3D11_MAX_SHADER_VIEW; i++)
    {
        quad->cropViewport[i].MinDepth = 0.0f;
        quad->cropViewport[i].MaxDepth = 1.0f;
    }
    quad->resourceCount = DxgiResourceCount(quad->textureFormat);

    return VLC_SUCCESS;
}

void D3D11_UpdateViewport(d3d_quad_t *quad, const RECT *rect, const d3d_format_t *display)
{
    LONG srcAreaWidth, srcAreaHeight;

    srcAreaWidth  = RECTWidth(*rect);
    srcAreaHeight = RECTHeight(*rect);

    quad->cropViewport[0].TopLeftX = rect->left;
    quad->cropViewport[0].TopLeftY = rect->top;
    quad->cropViewport[0].Width    = srcAreaWidth;
    quad->cropViewport[0].Height   = srcAreaHeight;

    switch ( quad->textureFormat->formatTexture )
    {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
        quad->cropViewport[1].TopLeftX = rect->left / 2;
        quad->cropViewport[1].TopLeftY = rect->top / 2;
        quad->cropViewport[1].Width    = srcAreaWidth / 2;
        quad->cropViewport[1].Height   = srcAreaHeight / 2;
        break;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_B5G6R5_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_YUY2:
    case DXGI_FORMAT_AYUV:
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y410:
        if ( display->formatTexture == DXGI_FORMAT_NV12 ||
             display->formatTexture == DXGI_FORMAT_P010 )
        {
            quad->cropViewport[1].TopLeftX = rect->left / 2;
            quad->cropViewport[1].TopLeftY = rect->top / 2;
            quad->cropViewport[1].Width    = srcAreaWidth / 2;
            quad->cropViewport[1].Height   = srcAreaHeight / 2;
        }
        break;
    case DXGI_FORMAT_UNKNOWN:
        switch ( quad->textureFormat->fourcc )
        {
        case VLC_CODEC_YUVA:
            if ( display->formatTexture != DXGI_FORMAT_NV12 &&
                 display->formatTexture != DXGI_FORMAT_P010 )
            {
                quad->cropViewport[1] = quad->cropViewport[2] =
                quad->cropViewport[3] = quad->cropViewport[0];
                break;
            }
            /* fallthrough */
        case VLC_CODEC_I420_10L:
        case VLC_CODEC_I420:
            quad->cropViewport[1].TopLeftX = quad->cropViewport[0].TopLeftX / 2;
            quad->cropViewport[1].TopLeftY = quad->cropViewport[0].TopLeftY / 2;
            quad->cropViewport[1].Width    = quad->cropViewport[0].Width / 2;
            quad->cropViewport[1].Height   = quad->cropViewport[0].Height / 2;
            quad->cropViewport[2] = quad->cropViewport[1];
            break;
        }
        break;
    default:
        vlc_assert_unreachable();
    }
}
