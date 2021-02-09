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

void D3D11_RenderQuad(d3d11_device_t *d3d_dev, d3d11_quad_t *quad, d3d11_vertex_shader_t *vsshader,
                      ID3D11ShaderResourceView *resourceView[DXGI_MAX_SHADER_VIEW],
                      d3d11_select_plane_t selectPlane, void *selectOpaque)
{
    UINT offset = 0;

    /* Render the quad */
    ID3D11DeviceContext_IASetPrimitiveTopology(d3d_dev->d3dcontext, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    /* vertex shader */
    ID3D11DeviceContext_IASetInputLayout(d3d_dev->d3dcontext, vsshader->layout);
    ID3D11DeviceContext_IASetVertexBuffers(d3d_dev->d3dcontext, 0, 1, &quad->pVertexBuffer, &quad->generic.vertexStride, &offset);
    ID3D11DeviceContext_IASetIndexBuffer(d3d_dev->d3dcontext, quad->pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    if ( quad->viewpointShaderConstant )
        ID3D11DeviceContext_VSSetConstantBuffers(d3d_dev->d3dcontext, 0, 1, &quad->viewpointShaderConstant);

    ID3D11DeviceContext_VSSetShader(d3d_dev->d3dcontext, vsshader->shader, NULL, 0);

    if (quad->SamplerStates[0])
        ID3D11DeviceContext_PSSetSamplers(d3d_dev->d3dcontext, 0, 2, quad->SamplerStates);

    /* pixel shader */
    ID3D11DeviceContext_PSSetConstantBuffers(d3d_dev->d3dcontext, 0, 1, &quad->pPixelShaderConstants);
    assert(quad->resourceCount <= DXGI_MAX_SHADER_VIEW);

    ID3D11DeviceContext_PSSetShaderResources(d3d_dev->d3dcontext, 0, quad->resourceCount, resourceView);

    for (size_t i=0; i<ARRAY_SIZE(quad->d3dpixelShader); i++)
    {
        if (!quad->d3dpixelShader[i])
            break;

        if (unlikely(!selectPlane(selectOpaque, i)))
            continue;

        ID3D11DeviceContext_PSSetShader(d3d_dev->d3dcontext, quad->d3dpixelShader[i], NULL, 0);

        ID3D11DeviceContext_RSSetViewports(d3d_dev->d3dcontext, 1, &quad->cropViewport[i]);

        ID3D11DeviceContext_DrawIndexed(d3d_dev->d3dcontext, quad->generic.indexCount, 0, 0);

        // /* force unbinding the input texture, otherwise we get:
        // * OMSetRenderTargets: Resource being set to OM RenderTarget slot 0 is still bound on input! */
        // ID3D11ShaderResourceView *reset[DXGI_MAX_SHADER_VIEW] = { 0 };
        // ID3D11DeviceContext_PSSetShaderResources(d3d_dev->d3dcontext, 0, quad->resourceCount, reset);
    }
}

static bool AllocQuadVertices(vlc_object_t *o, d3d11_device_t *d3d_dev, d3d11_quad_t *quad, video_projection_mode_t projection)
{
    HRESULT hr;

    if (!D3D_QuadSetupBuffers(o, &quad->generic, projection))
        return false;

    D3D11_BUFFER_DESC bd;
    memset(&bd, 0, sizeof(bd));
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = quad->generic.vertexStride * quad->generic.vertexCount;
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
        .ByteWidth = sizeof(WORD) * quad->generic.indexCount,
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
    if (quad->pIndexBuffer)
    {
        ID3D11Buffer_Release(quad->pIndexBuffer);
        quad->pIndexBuffer = NULL;
    }
    return false;
}

void D3D11_ReleaseQuad(d3d11_quad_t *quad)
{
    if (quad->pPixelShaderConstants)
    {
        ID3D11Buffer_Release(quad->pPixelShaderConstants);
        quad->pPixelShaderConstants = NULL;
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
    if (quad->viewpointShaderConstant)
    {
        ID3D11Buffer_Release(quad->viewpointShaderConstant);
        quad->viewpointShaderConstant = NULL;
    }
    D3D11_ReleaseQuadPixelShader(quad);
    ReleaseD3D11PictureSys(&quad->picSys);
}

#undef D3D11_UpdateQuadPosition
bool D3D11_UpdateQuadPosition( vlc_object_t *o, d3d11_device_t *d3d_dev, d3d11_quad_t *quad,
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

    result = D3D_SetupQuadData(o, &quad->generic, output, dst_data, mappedResource.pData, orientation);

    ID3D11DeviceContext_Unmap(d3d_dev->d3dcontext, (ID3D11Resource *)quad->pIndexBuffer, 0);
    ID3D11DeviceContext_Unmap(d3d_dev->d3dcontext, (ID3D11Resource *)quad->pVertexBuffer, 0);

    return result;
}

static bool ShaderUpdateConstants(vlc_object_t *o, d3d11_device_t *d3d_dev, d3d11_quad_t *quad, int type, void *new_buf)
{
    ID3D11Resource *res;
    switch (type)
    {
        case PS_CONST_LUMI_BOUNDS:
            res = (ID3D11Resource *)quad->pPixelShaderConstants;
            break;
        case VS_CONST_VIEWPOINT:
            res = (ID3D11Resource *)quad->viewpointShaderConstant;
            break;
    }

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = ID3D11DeviceContext_Map(d3d_dev->d3dcontext, res, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (unlikely(FAILED(hr)))
    {
        msg_Err(o, "Failed to lock the picture shader constants (hr=0x%lX)", hr);
        return false;
    }

    switch (type)
    {
        case PS_CONST_LUMI_BOUNDS:
            memcpy(mappedResource.pData, new_buf, sizeof(PS_CONSTANT_BUFFER));
            break;
        case VS_CONST_VIEWPOINT:
            memcpy(mappedResource.pData, new_buf, sizeof(VS_PROJECTION_CONST));
            break;
    }
    ID3D11DeviceContext_Unmap(d3d_dev->d3dcontext, res, 0);
    return true;
}

void (D3D11_UpdateQuadOpacity)(vlc_object_t *o, d3d11_device_t *d3d_dev, d3d11_quad_t *quad, float opacity)
{
    float old = quad->generic.shaderConstants->Opacity;
    if (!D3D_UpdateQuadOpacity(&quad->generic, opacity))
        return;

    if (!ShaderUpdateConstants(o, d3d_dev, quad, PS_CONST_LUMI_BOUNDS, quad->generic.shaderConstants))
        D3D_UpdateQuadOpacity(&quad->generic, old);
}

void (D3D11_UpdateQuadLuminanceScale)(vlc_object_t *o, d3d11_device_t *d3d_dev, d3d11_quad_t *quad, float luminanceScale)
{
    float old = quad->generic.shaderConstants->LuminanceScale;
    if (!D3D_UpdateQuadLuminanceScale(&quad->generic, luminanceScale))
        return;

    if (!ShaderUpdateConstants(o, d3d_dev, quad, PS_CONST_LUMI_BOUNDS, quad->generic.shaderConstants))
        D3D_UpdateQuadLuminanceScale(&quad->generic, old);
}

void (D3D11_UpdateViewpoint)(vlc_object_t *o, d3d11_device_t *d3d_dev, d3d11_quad_t *quad,
                             const vlc_viewpoint_t *viewpoint, float f_sar)
{
    if (!quad->viewpointShaderConstant)
        return;

    D3D_UpdateViewpoint(&quad->generic, viewpoint, f_sar);

    ShaderUpdateConstants(o, d3d_dev, quad, VS_CONST_VIEWPOINT, quad->generic.vertexConstants);
}

#undef D3D11_AllocateQuad
int D3D11_AllocateQuad(vlc_object_t *o, d3d11_device_t *d3d_dev,
                       video_projection_mode_t projection, d3d11_quad_t *quad)
{
    quad->generic.vertexConstants = &quad->vConstants;
    quad->generic.shaderConstants = &quad->pConstants;

    HRESULT hr;
    static_assert((sizeof(PS_CONSTANT_BUFFER)%16)==0,"Constant buffers require 16-byte alignment");
    D3D11_BUFFER_DESC constantDesc = {
        .Usage = D3D11_USAGE_DYNAMIC,
        .ByteWidth = sizeof(PS_CONSTANT_BUFFER),
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };
    hr = ID3D11Device_CreateBuffer(d3d_dev->d3ddevice, &constantDesc, NULL, &quad->pPixelShaderConstants);
    if(FAILED(hr)) {
        msg_Err(o, "Could not create the pixel shader constant buffer. (hr=0x%lX)", hr);
        goto error;
    }

    if (projection == PROJECTION_MODE_EQUIRECTANGULAR || projection == PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD)
    {
        static_assert((sizeof(VS_PROJECTION_CONST)%16)==0,"Constant buffers require 16-byte alignment");
        constantDesc.ByteWidth = sizeof(VS_PROJECTION_CONST);
        hr = ID3D11Device_CreateBuffer(d3d_dev->d3ddevice, &constantDesc, NULL, &quad->viewpointShaderConstant);
        if(FAILED(hr)) {
            msg_Err(o, "Could not create the vertex shader constant buffer. (hr=0x%lX)", hr);
            goto error;
        }
    }

    if (!AllocQuadVertices(o, d3d_dev, quad, projection))
        goto error;

    return VLC_SUCCESS;

error:
    D3D11_ReleaseQuad(quad);
    return VLC_EGENERIC;
}

#undef D3D11_SetupQuad
int D3D11_SetupQuad(vlc_object_t *o, d3d11_device_t *d3d_dev, const video_format_t *fmt, d3d11_quad_t *quad,
                    const display_info_t *displayFormat)
{
    D3D_SetupQuad(o, fmt, &quad->generic, displayFormat);

    ShaderUpdateConstants(o, d3d_dev, quad, PS_CONST_LUMI_BOUNDS, quad->generic.shaderConstants);

    for (size_t i=0; i<ARRAY_SIZE(quad->cropViewport); i++)
    {
        quad->cropViewport[i].MinDepth = 0.0f;
        quad->cropViewport[i].MaxDepth = 1.0f;
    }
    quad->resourceCount = DxgiResourceCount(quad->generic.textureFormat);

    return VLC_SUCCESS;
}

void D3D11_UpdateViewport(d3d11_quad_t *quad, const RECT *rect, const d3d_format_t *display)
{
    LONG srcAreaWidth, srcAreaHeight;

    srcAreaWidth  = RECTWidth(*rect);
    srcAreaHeight = RECTHeight(*rect);

    quad->cropViewport[0].TopLeftX = rect->left;
    quad->cropViewport[0].TopLeftY = rect->top;
    quad->cropViewport[0].Width    = srcAreaWidth;
    quad->cropViewport[0].Height   = srcAreaHeight;

    switch ( quad->generic.textureFormat->formatTexture )
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
        switch ( quad->generic.textureFormat->fourcc )
        {
        case VLC_CODEC_YUVA:
            if ( display->formatTexture != DXGI_FORMAT_NV12 &&
                 display->formatTexture != DXGI_FORMAT_P010 )
            {
                quad->cropViewport[1] = quad->cropViewport[0];
                break;
            }
            /* fallthrough */
        case VLC_CODEC_I420_10L:
        case VLC_CODEC_I420:
            quad->cropViewport[1].TopLeftX = quad->cropViewport[0].TopLeftX / 2;
            quad->cropViewport[1].TopLeftY = quad->cropViewport[0].TopLeftY / 2;
            quad->cropViewport[1].Width    = quad->cropViewport[0].Width / 2;
            quad->cropViewport[1].Height   = quad->cropViewport[0].Height / 2;
            break;
        }
        break;
    default:
        vlc_assert_unreachable();
    }
}
