// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * d3d11_tonemap: Direct3D11 VideoProcessor to handle tonemapping
 *****************************************************************************
 * Copyright © 2024 Videolabs, VLC authors and VideoLAN
 *
 * Authors: Steve Lhomme <robux4@videolabs.io>
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "d3d11_tonemap.h"

#include <cassert>

#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

struct d3d11_tonemapper
{
    ComPtr<ID3D11VideoDevice>               d3dviddev;
    ComPtr<ID3D11VideoContext>              d3dvidctx;
    ComPtr<ID3D11VideoProcessorEnumerator>  enumerator;
    ComPtr<ID3D11VideoProcessor>            processor;

    ComPtr<ID3D11VideoProcessorOutputView>  outputView;
    ComPtr<ID3D11ShaderResourceView>        SRV;
    picture_sys_t                           picsys{};

    const d3d_format_t              *d3d_fmt = nullptr;
    UINT                            Width  = 0;
    UINT                            Height = 0;

    HRESULT UpdateTexture(vlc_object_t *, d3d11_device_t *, UINT width, UINT height);
};

d3d11_tonemapper *D3D11_TonemapperCreate(vlc_object_t *vd, d3d11_device_t *d3d_dev,
                                         const video_format_t *in)
{
    if (!is_d3d11_opaque(in->i_chroma))
    {
        msg_Dbg(vd, "VideoProcessor tone mapping not supported by CPU formats");
        return nullptr;
    }

    if (in->transfer == TRANSFER_FUNC_SMPTE_ST2084 ||
        in->transfer == TRANSFER_FUNC_HLG)
    {
        return nullptr; // the source is already in HDR
    }

    d3d11_tonemapper *tonemapProc = new d3d11_tonemapper();
    tonemapProc->d3d_fmt = D3D11_RenderFormat(DXGI_FORMAT_R10G10B10A2_UNORM, false);
    assert(tonemapProc->d3d_fmt != nullptr);

    HRESULT hr;
    hr = d3d_dev->d3ddevice->QueryInterface(IID_GRAPHICS_PPV_ARGS(&tonemapProc->d3dviddev));
    if (unlikely(FAILED(hr)))
    {
        msg_Err(vd, "Could not Query ID3D11VideoDevice Interface. (hr=0x%lX)", hr);
        goto error;
    }

    hr = d3d_dev->d3dcontext->QueryInterface(IID_GRAPHICS_PPV_ARGS(&tonemapProc->d3dvidctx));
    if (unlikely(FAILED(hr)))
    {
        msg_Err(vd, "Could not Query ID3D11VideoContext Interface. (hr=0x%lX)", hr);
        goto error;
    }

    {
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC processorDesc{};
        processorDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
        processorDesc.InputFrameRate = {
            in->i_frame_rate, in->i_frame_rate_base,
        };
        processorDesc.InputWidth   = in->i_width;
        processorDesc.InputHeight  = in->i_height;
        processorDesc.OutputWidth  = in->i_width;
        processorDesc.OutputHeight = in->i_height;
        processorDesc.OutputFrameRate = {
            in->i_frame_rate, in->i_frame_rate_base,
        };
        processorDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
        hr = tonemapProc->d3dviddev->CreateVideoProcessorEnumerator(&processorDesc, &tonemapProc->enumerator);
        if (FAILED(hr))
        {
            msg_Dbg(vd, "Can't get a video processor for the video (error 0x%lx).", hr);
            goto error;
        }

        hr = tonemapProc->d3dviddev->CreateVideoProcessor(tonemapProc->enumerator.Get(), 0,
                                                            &tonemapProc->processor);
        if (FAILED(hr))
        {
            msg_Dbg(vd, "failed to create the processor (error 0x%lx).", hr);
            goto error;
        }
    }

    // we can only use this filter with the NVIDIA extension as the VideoProcessor
    // doesn't provide a proper API to set the input and output colorimetry

    // NVIDIA 545+ driver
    if (d3d_dev->adapterDesc.VendorId != GPU_MANUFACTURER_NVIDIA ||
        (d3d_dev->WDDM.revision * 10000 + d3d_dev->WDDM.build) < 154500)
            goto error;

    {
        constexpr GUID kNvidiaTrueHDRInterfaceGUID{ 0xfdd62bb4, 0x620b, 0x4fd7, {0x9a, 0xb3, 0x1e, 0x59, 0xd0, 0xd5, 0x44, 0xb3} };
        UINT available = 0;
        d3d11_device_lock(d3d_dev);
        hr = tonemapProc->d3dvidctx->VideoProcessorGetStreamExtension(tonemapProc->processor.Get(),
                    0, &kNvidiaTrueHDRInterfaceGUID, sizeof(available), &available);

        if (!available)
        {
            msg_Warn(vd, "True HDR not supported");
            d3d11_device_unlock(d3d_dev);
            goto error;
        }

        constexpr UINT kStreamExtensionMethodTrueHDR = 0x3;
        constexpr UINT TrueHDRVersion4 = 4;
        struct {
            UINT version;
            UINT method;
            UINT enable : 1;
            UINT reserved : 31;
        } stream_extension_info = {TrueHDRVersion4,
                                    kStreamExtensionMethodTrueHDR,
                                    1u,
                                    0u};
        hr = tonemapProc->d3dvidctx->VideoProcessorSetStreamExtension(
                tonemapProc->processor.Get(),
                0, &kNvidiaTrueHDRInterfaceGUID,
                sizeof(stream_extension_info), &stream_extension_info);
        if (unlikely(FAILED(hr)))
        {
            msg_Warn(vd, "Failed to enable NVIDIA True HDR");
        }
        d3d11_device_unlock(d3d_dev);
    }

    hr = tonemapProc->UpdateTexture(vd, d3d_dev, in->i_width, in->i_height);
    if (FAILED(hr))
        goto error;

    return tonemapProc;
error:
    delete tonemapProc;
    return nullptr;
}

HRESULT d3d11_tonemapper::UpdateTexture(vlc_object_t *vd, d3d11_device_t *d3d_dev, UINT width, UINT height)
{
    ComPtr<ID3D11Texture2D> texture;
    D3D11_TEXTURE2D_DESC texDesc { };
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outDesc{ };
    ID3D11Texture2D *_texture[D3D11_MAX_SHADER_VIEW] = {};
    HRESULT hr;

    // we need a texture that will receive the upscale version
    texDesc.MipLevels = 1;
    texDesc.SampleDesc.Count = 1;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.CPUAccessFlags = 0;
    texDesc.ArraySize = 1;
    texDesc.Format = d3d_fmt->formatTexture;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MiscFlags = 0;
    hr = d3d_dev->d3ddevice->CreateTexture2D(&texDesc, nullptr, texture.GetAddressOf());
    if (FAILED(hr))
    {
        msg_Err(vd, "Failed to create the tonemap texture. (hr=0x%lX)", hr);
        return hr;
    }

    outDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    outDesc.Texture2D.MipSlice = 0;

    hr = d3dviddev->CreateVideoProcessorOutputView(texture.Get(),
                                                   enumerator.Get(),
                                                   &outDesc,
                                                   outputView.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        msg_Dbg(vd,"Failed to create processor output. (hr=0x%lX)", hr);
        return hr;
    }

    if (picsys.processorInput)
    {
        picsys.processorInput->Release();
        picsys.processorInput = NULL;
    }
    if (picsys.processorOutput)
    {
        picsys.processorOutput->Release();
        picsys.processorOutput = NULL;
    }
    _texture[0] = texture.Get();
    _texture[1] = texture.Get();
    _texture[2] = texture.Get();
    _texture[3] = texture.Get();
    if ((D3D11_AllocateShaderView)(vd, d3d_dev->d3ddevice, d3d_fmt,
                                   _texture, 0, SRV.ReleaseAndGetAddressOf()) != VLC_SUCCESS)
        return hr;

    {
        RECT srcRect;
        srcRect.left   = 0;
        srcRect.top    = 0;
        srcRect.right  = texDesc.Width;
        srcRect.bottom = texDesc.Height;

        d3d11_device_lock(d3d_dev);
        d3dvidctx->VideoProcessorSetStreamSourceRect(processor.Get(),
                                                                  0, TRUE, &srcRect);

        d3dvidctx->VideoProcessorSetStreamDestRect(processor.Get(),
                                                                0, TRUE, &srcRect);

        d3d11_device_unlock(d3d_dev);
    }

    picsys.texture[0] = texture.Get();
    picsys.resourceView[0] = SRV.Get();
    picsys.formatTexture = texDesc.Format;

    Width  = texDesc.Width;
    Height = texDesc.Height;
    msg_Dbg(vd, "tonemap resolution %ux%u", Width, Height);

    return S_OK;
}

void D3D11_TonemapperDestroy(d3d11_tonemapper *tonemapProc)
{
    delete tonemapProc;
}

picture_sys_t *D3D11_TonemapperGetOutput(d3d11_tonemapper *tonemapProc)
{
    return &tonemapProc->picsys;
}

static HRESULT assert_ProcessorInput(vlc_object_t *vd, d3d11_tonemapper *tonemapProc, picture_sys_t *p_sys_src)
{
    if (!p_sys_src->processorInput)
    {
        D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inDesc{};
        inDesc.FourCC = 0;
        inDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
        inDesc.Texture2D.MipSlice = 0;
        inDesc.Texture2D.ArraySlice = p_sys_src->slice_index;

        HRESULT hr;

        hr = tonemapProc->d3dviddev->CreateVideoProcessorInputView(
                                                             p_sys_src->resource[KNOWN_DXGI_INDEX],
                                                             tonemapProc->enumerator.Get(),
                                                             &inDesc,
                                                             &p_sys_src->processorInput);
        if (FAILED(hr))
        {
#ifndef NDEBUG
            msg_Dbg(vd,"Failed to create processor input for slice %d. (hr=0x%lX)", p_sys_src->slice_index, hr);
#endif
            return hr;
        }
    }
    return S_OK;
}

HRESULT D3D11_TonemapperProcess(vlc_object_t *vd, d3d11_tonemapper *tonemapProc, picture_sys_t *in)
{
    HRESULT hr = assert_ProcessorInput(vd, tonemapProc, in);
    if (FAILED(hr))
        return hr;

    D3D11_VIDEO_PROCESSOR_STREAM stream{};
    stream.Enable = TRUE;
    stream.pInputSurface = in->processorInput;

    hr = tonemapProc->d3dvidctx->VideoProcessorBlt(tonemapProc->processor.Get(),
                                                   tonemapProc->outputView.Get(),
                                                   0, 1, &stream);
    if (FAILED(hr))
        msg_Err(vd, "Failed to render the texture. (hr=0x%lX)", hr);
    return hr;
}

int D3D11_TonemapperUpdate(vlc_object_t *vd, d3d11_tonemapper *tonemapProc, d3d11_device_t*d3d_dev,
                           video_format_t *quad_fmt)
{
    HRESULT hr;
    ID3D11Texture2D *_texture[D3D11_MAX_SHADER_VIEW];
    ComPtr<ID3D11Texture2D> texture;
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outDesc{ };
    D3D11_VIDEO_COLOR black{};
    black.RGBA.A = 1.f;

    unsigned out_width, out_height;
    out_width  = quad_fmt->i_width;
    out_height = quad_fmt->i_height;

    quad_fmt->i_sar_num = 1;
    quad_fmt->i_sar_den = 1;
    quad_fmt->b_color_range_full = true;

    if (tonemapProc->Width == out_width && tonemapProc->Height == out_height)
        // do nothing
        return VLC_SUCCESS;

    hr = tonemapProc->UpdateTexture(vd, d3d_dev, out_width, out_height);
    if (FAILED(hr))
        goto done_super;

    RECT srcRect;
    srcRect.left   = 0;
    srcRect.top    = 0;
    srcRect.right  = tonemapProc->Width;
    srcRect.bottom = tonemapProc->Height;

    d3d11_device_lock(d3d_dev);
    tonemapProc->d3dvidctx->VideoProcessorSetStreamSourceRect(tonemapProc->processor.Get(),
                                                            0, TRUE, &srcRect);

    tonemapProc->d3dvidctx->VideoProcessorSetStreamDestRect(tonemapProc->processor.Get(),
                                                          0, TRUE, &srcRect);
    d3d11_device_unlock(d3d_dev);

    return VLC_SUCCESS;
done_super:
    tonemapProc->SRV.Reset();
    tonemapProc->processor.Reset();
    tonemapProc->enumerator.Reset();
    return VLC_EGENERIC;
}
