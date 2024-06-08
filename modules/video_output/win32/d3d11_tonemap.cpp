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
    picture_sys_d3d11_t                     picsys{};
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

    ComPtr<ID3D11Texture2D> texture;
    ID3D11Texture2D *_texture[DXGI_MAX_SHADER_VIEW] = {};
    D3D11_TEXTURE2D_DESC texDesc { };
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outDesc{ };
    d3d11_tonemapper *tonemapProc = new d3d11_tonemapper();
    const auto d3d_fmt = D3D11_RenderFormat(DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_UNKNOWN, false);
    assert(d3d_fmt != nullptr);

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

    // we need a texture that will receive the upscale version
    texDesc.MipLevels = 1;
    texDesc.SampleDesc.Count = 1;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.CPUAccessFlags = 0;
    texDesc.ArraySize = 1;
    texDesc.Format = d3d_fmt->formatTexture;
    texDesc.Width = in->i_width;
    texDesc.Height = in->i_height;
    texDesc.MiscFlags = 0;
    hr = d3d_dev->d3ddevice->CreateTexture2D(&texDesc, nullptr, texture.GetAddressOf());
    if (FAILED(hr))
    {
        msg_Err(vd, "Failed to create the tonemap texture. (hr=0x%lX)", hr);
        goto error;
    }

    outDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    outDesc.Texture2D.MipSlice = 0;

    hr = tonemapProc->d3dviddev->CreateVideoProcessorOutputView(
                                                            texture.Get(),
                                                            tonemapProc->enumerator.Get(),
                                                            &outDesc,
                                                            tonemapProc->outputView.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        msg_Dbg(vd,"Failed to create processor output. (hr=0x%lX)", hr);
        goto error;
    }

    _texture[0] = texture.Get();
    _texture[1] = texture.Get();
    _texture[2] = texture.Get();
    _texture[3] = texture.Get();
    if (D3D11_AllocateResourceView(vlc_object_logger(vd), d3d_dev->d3ddevice, d3d_fmt,
                                   _texture, 0, tonemapProc->SRV.GetAddressOf()) != VLC_SUCCESS)
        goto error;

    {
        RECT srcRect;
        srcRect.left   = 0;
        srcRect.top    = 0;
        srcRect.right  = texDesc.Width;
        srcRect.bottom = texDesc.Height;

        RECT dstRect = srcRect;

        d3d11_device_lock(d3d_dev);
        tonemapProc->d3dvidctx->VideoProcessorSetStreamSourceRect(tonemapProc->processor.Get(),
                                                                  0, TRUE, &srcRect);

        tonemapProc->d3dvidctx->VideoProcessorSetStreamDestRect(tonemapProc->processor.Get(),
                                                                0, TRUE, &dstRect);

        d3d11_device_unlock(d3d_dev);
    }

    tonemapProc->picsys.texture[0] = texture.Get();
    tonemapProc->picsys.renderSrc[0] = tonemapProc->SRV.Get();

    return tonemapProc;
error:
    delete tonemapProc;
    return nullptr;
}

void D3D11_TonemapperDestroy(d3d11_tonemapper *tonemapProc)
{
    delete tonemapProc;
}

picture_sys_d3d11_t *D3D11_TonemapperGetOutput(d3d11_tonemapper *tonemapProc)
{
    return &tonemapProc->picsys;
}

static HRESULT assert_ProcessorInput(vlc_object_t *vd, d3d11_tonemapper *tonemapProc, picture_sys_d3d11_t *p_sys_src)
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

HRESULT D3D11_TonemapperProcess(vlc_object_t *vd, d3d11_tonemapper *tonemapProc, picture_sys_d3d11_t *in)
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
