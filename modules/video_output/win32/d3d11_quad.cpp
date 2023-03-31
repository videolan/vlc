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

#include <cassert>
#include <vlc_common.h>

#include "d3d11_quad.h"
#include "common.h"

using Microsoft::WRL::ComPtr;

void D3D11_RenderQuad(d3d11_device_t *d3d_dev, d3d11_quad_t *quad, d3d11_vertex_shader_t *vsshader,
                      ID3D11ShaderResourceView *resourceView[DXGI_MAX_SHADER_VIEW],
                      d3d11_select_plane_t selectPlane, void *selectOpaque)
{
    UINT offset = 0;

    /* Render the quad */
    d3d_dev->d3dcontext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    /* vertex shader */
    d3d_dev->d3dcontext->IASetInputLayout(vsshader->layout.Get());
    d3d_dev->d3dcontext->IASetVertexBuffers(0, 1, quad->vertexBuffer.GetAddressOf(), &quad->generic.vertexStride, &offset);
    d3d_dev->d3dcontext->IASetIndexBuffer(quad->indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    if ( quad->viewpointShaderConstant.Get() )
        d3d_dev->d3dcontext->VSSetConstantBuffers(0, 1, quad->viewpointShaderConstant.GetAddressOf());

    d3d_dev->d3dcontext->VSSetShader(vsshader->shader.Get(), NULL, 0);

    if (quad->SamplerStates[0].Get())
    {
        ID3D11SamplerState *states[] = {quad->SamplerStates[0].Get(), quad->SamplerStates[1].Get()};
        d3d_dev->d3dcontext->PSSetSamplers(0, 2, states);
    }

    /* pixel shader */
    d3d_dev->d3dcontext->PSSetConstantBuffers(0, 1, quad->pPixelShaderConstants.GetAddressOf());
    assert(quad->resourceCount <= DXGI_MAX_SHADER_VIEW);

    d3d_dev->d3dcontext->PSSetShaderResources(0, quad->resourceCount, resourceView);

    for (size_t i=0; i<ARRAY_SIZE(quad->d3dpixelShader); i++)
    {
        if (!quad->d3dpixelShader[i].Get())
            break;

        ID3D11RenderTargetView *renderView = NULL;
        if (unlikely(!selectPlane(selectOpaque, i, &renderView)))
            continue;

        if (renderView != NULL)
            d3d_dev->d3dcontext->OMSetRenderTargets(1, &renderView, NULL);

        d3d_dev->d3dcontext->PSSetShader(quad->d3dpixelShader[i].Get(), NULL, 0);

        d3d_dev->d3dcontext->RSSetViewports(1, &quad->cropViewport[i]);

        d3d_dev->d3dcontext->DrawIndexed(quad->generic.indexCount, 0, 0);

        // /* force unbinding the input texture, otherwise we get:
        // * OMSetRenderTargets: Resource being set to OM RenderTarget slot 0 is still bound on input! */
        // ID3D11ShaderResourceView *reset[DXGI_MAX_SHADER_VIEW] = { 0 };
        // d3d_dev->d3dcontext->PSSetShaderResources(0, quad->resourceCount, reset);
    }
}

static bool AllocQuadVertices(vlc_object_t *o, d3d11_device_t *d3d_dev, d3d11_quad_t *quad, video_projection_mode_t projection)
{
    HRESULT hr;

    if (!D3D_QuadSetupBuffers(o, &quad->generic, projection))
        return false;

    D3D11_BUFFER_DESC bd = { };

    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = quad->generic.vertexStride * quad->generic.vertexCount;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = d3d_dev->d3ddevice->CreateBuffer(&bd, NULL, &quad->vertexBuffer);
    if(FAILED(hr)) {
        msg_Err(o, "Failed to create vertex buffer. (hr=%lX)", hr);
        goto fail;
    }

    /* create the index of the vertices */
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.ByteWidth = sizeof(WORD) * quad->generic.indexCount;

    hr = d3d_dev->d3ddevice->CreateBuffer(&bd, NULL, &quad->indexBuffer);
    if(FAILED(hr)) {
        msg_Err(o, "Could not create the quad indices. (hr=0x%lX)", hr);
        goto fail;
    }

    return true;
fail:
    quad->vertexBuffer.Reset();
    quad->indexBuffer.Reset();
    return false;
}

void d3d11_quad_t::Reset()
{
    pPixelShaderConstants.Reset();
    vertexBuffer.Reset();
    indexBuffer.Reset();
    viewpointShaderConstant.Reset();
    for (size_t i=0; i<ARRAY_SIZE(d3dpixelShader); i++)
    {
        d3dpixelShader[i].Reset();
        SamplerStates[i].Reset();
    }
    ReleaseD3D11PictureSys(&picSys);
}

#undef D3D11_UpdateQuadPosition
bool D3D11_UpdateQuadPosition( vlc_object_t *o, d3d11_device_t *d3d_dev, d3d11_quad_t *quad,
                                const RECT *output, video_transform_t orientation )
{
    bool result = true;
    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    d3d_vertex_t *dst_data;

    if (unlikely(quad->vertexBuffer.Get() == NULL))
        return false;

    /* create the vertices */
    hr = d3d_dev->d3dcontext->Map(quad->vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr)) {
        msg_Err(o, "Failed to lock the vertex buffer (hr=0x%lX)", hr);
        return false;
    }
    dst_data = static_cast<d3d_vertex_t*>(mappedResource.pData);

    /* create the vertex indices */
    hr = d3d_dev->d3dcontext->Map(quad->indexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr)) {
        msg_Err(o, "Failed to lock the index buffer (hr=0x%lX)", hr);
        d3d_dev->d3dcontext->Unmap(quad->vertexBuffer.Get(), 0);
        return false;
    }

    result = D3D_SetupQuadData(o, &quad->generic, output, dst_data, mappedResource.pData, orientation);

    d3d_dev->d3dcontext->Unmap(quad->indexBuffer.Get(), 0);
    d3d_dev->d3dcontext->Unmap(quad->vertexBuffer.Get(), 0);

    return result;
}

static bool ShaderUpdateConstants(vlc_object_t *o, d3d11_device_t *d3d_dev, d3d11_quad_t *quad, int type, void *new_buf)
{
    ID3D11Resource *res;
    switch (type)
    {
        case PS_CONST_LUMI_BOUNDS:
            res = quad->pPixelShaderConstants.Get();
            break;
        case VS_CONST_VIEWPOINT:
            res = quad->viewpointShaderConstant.Get();
            break;
    }

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = d3d_dev->d3dcontext->Map(res, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
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
    d3d_dev->d3dcontext->Unmap(res, 0);
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
    if (!quad->viewpointShaderConstant.Get())
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
    D3D11_BUFFER_DESC constantDesc = { };
    constantDesc.Usage = D3D11_USAGE_DYNAMIC;
    constantDesc.ByteWidth = sizeof(PS_CONSTANT_BUFFER);
    constantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constantDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = d3d_dev->d3ddevice->CreateBuffer(&constantDesc, NULL, &quad->pPixelShaderConstants);
    if(FAILED(hr)) {
        msg_Err(o, "Could not create the pixel shader constant buffer. (hr=0x%lX)", hr);
        goto error;
    }

    if (projection == PROJECTION_MODE_EQUIRECTANGULAR || projection == PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD)
    {
        static_assert((sizeof(VS_PROJECTION_CONST)%16)==0,"Constant buffers require 16-byte alignment");
        constantDesc.ByteWidth = sizeof(VS_PROJECTION_CONST);
        hr = d3d_dev->d3ddevice->CreateBuffer(&constantDesc, NULL, &quad->viewpointShaderConstant);
        if(FAILED(hr)) {
            msg_Err(o, "Could not create the vertex shader constant buffer. (hr=0x%lX)", hr);
            goto error;
        }
    }

    if (!AllocQuadVertices(o, d3d_dev, quad, projection))
        goto error;

    return VLC_SUCCESS;

error:
    quad->Reset();
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

void d3d11_quad_t::UpdateViewport(const RECT *rect, const d3d_format_t *display)
{
    LONG srcAreaWidth, srcAreaHeight;

    srcAreaWidth  = RECTWidth(*rect);
    srcAreaHeight = RECTHeight(*rect);

    cropViewport[0].TopLeftX = rect->left;
    cropViewport[0].TopLeftY = rect->top;
    cropViewport[0].Width    = srcAreaWidth;
    cropViewport[0].Height   = srcAreaHeight;

    switch ( generic.textureFormat->formatTexture )
    {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
        cropViewport[1].TopLeftX = rect->left / 2;
        cropViewport[1].TopLeftY = rect->top / 2;
        cropViewport[1].Width    = srcAreaWidth / 2;
        cropViewport[1].Height   = srcAreaHeight / 2;
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
            cropViewport[1].TopLeftX = rect->left / 2;
            cropViewport[1].TopLeftY = rect->top / 2;
            cropViewport[1].Width    = srcAreaWidth / 2;
            cropViewport[1].Height   = srcAreaHeight / 2;
        }
        break;
    case DXGI_FORMAT_UNKNOWN:
        switch ( generic.textureFormat->fourcc )
        {
        case VLC_CODEC_I444:
            if ( display->formatTexture != DXGI_FORMAT_NV12 &&
                 display->formatTexture != DXGI_FORMAT_P010 )
            {
                cropViewport[1] = cropViewport[0];
                break;
            }
            break;
        case VLC_CODEC_YUVA:
            if ( display->formatTexture != DXGI_FORMAT_NV12 &&
                 display->formatTexture != DXGI_FORMAT_P010 )
            {
                cropViewport[1] = cropViewport[0];
                break;
            }
            /* fallthrough */
        case VLC_CODEC_I420_10L:
        case VLC_CODEC_I420:
            cropViewport[1].TopLeftX = cropViewport[0].TopLeftX / 2;
            cropViewport[1].TopLeftY = cropViewport[0].TopLeftY / 2;
            cropViewport[1].Width    = cropViewport[0].Width / 2;
            cropViewport[1].Height   = cropViewport[0].Height / 2;
            break;
        }
        break;
    default:
        vlc_assert_unreachable();
    }
}

#ifdef HAVE_D3D11_4_H
HRESULT D3D11_InitFence(d3d11_device_t & d3d_dev, d3d11_gpu_fence & fence)
{
    HRESULT hr;
    ComPtr<ID3D11Device5> d3ddev5;
    hr = d3d_dev.d3ddevice->QueryInterface(IID_GRAPHICS_PPV_ARGS(&d3ddev5));
    if (FAILED(hr))
        goto error;
    hr = d3ddev5->CreateFence(fence.renderFence, D3D11_FENCE_FLAG_NONE, IID_GRAPHICS_PPV_ARGS(&fence.d3dRenderFence));
    if (FAILED(hr))
        goto error;
    hr = d3d_dev.d3dcontext->QueryInterface(IID_GRAPHICS_PPV_ARGS(&fence.d3dcontext4));
    if (FAILED(hr))
        goto error;
    fence.renderFinished = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (unlikely(fence.renderFinished == nullptr))
        goto error;
    return S_OK;
error:
    fence.d3dRenderFence.Reset();
    fence.d3dcontext4.Reset();
    CloseHandle(fence.renderFinished);
    return hr;
}

void D3D11_ReleaseFence(d3d11_gpu_fence & fence)
{
    if (fence.d3dcontext4.Get())
    {
        fence.d3dRenderFence.Reset();
        fence.d3dcontext4.Reset();
        CloseHandle(fence.renderFinished);
        fence.renderFinished = nullptr;
    }
}

int D3D11_WaitFence(d3d11_gpu_fence & fence)
{
    if (fence.d3dcontext4.Get())
    {
        if (fence.renderFence == UINT64_MAX)
            fence.renderFence = 0;
        else
            fence.renderFence++;

        ResetEvent(fence.renderFinished);
        fence.d3dRenderFence->SetEventOnCompletion(fence.renderFence, fence.renderFinished);
        fence.d3dcontext4->Signal(fence.d3dRenderFence.Get(), fence.renderFence);

        WaitForSingleObject(fence.renderFinished, INFINITE);
        return VLC_SUCCESS;
    }
    return VLC_ENOTSUP;
}
#endif // HAVE_D3D11_4_H
