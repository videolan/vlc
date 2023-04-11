// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * d3d11_scaler: Direct3D11 VideoProcessor based output scaling
 *****************************************************************************
 * Copyright © 2023 Videolabs, VLC authors and VideoLAN
 *
 * Authors: Chilledheart <hukeyue@hotmail.com>
 *          Steve Lhomme <robux4@videolabs.io>
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include "d3d11_scaler.h"

#include <new>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

struct d3d11_scaler
{
    bool                            usable = false;
    const d3d_format_t              *d3d_fmt = nullptr;
    vout_display_place_t            place = {};
    UINT                            Width  = 0;
    UINT                            Height = 0;
    ComPtr<ID3D11VideoDevice>               d3dviddev;
    ComPtr<ID3D11VideoContext>              d3dvidctx;
    ComPtr<ID3D11VideoProcessorEnumerator>  enumerator;
    ComPtr<ID3D11VideoProcessor>            processor;
    ComPtr<ID3D11VideoProcessorOutputView>  outputView;
    ID3D11ShaderResourceView                *SRVs[DXGI_MAX_SHADER_VIEW] = {};
};

static const d3d_format_t *GetDirectRenderingFormat(vlc_object_t *vd, d3d11_device_t *d3d_dev, vlc_fourcc_t i_src_chroma)
{
    UINT supportFlags = D3D11_FORMAT_SUPPORT_SHADER_LOAD | D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_INPUT;
    return FindD3D11Format( vd, d3d_dev, i_src_chroma, DXGI_RGB_FORMAT|DXGI_YUV_FORMAT, 0, 0, 0, DXGI_CHROMA_GPU, supportFlags );
}

d3d11_scaler *D3D11_UpscalerCreate(vlc_object_t *vd, d3d11_device_t *d3d_dev, vlc_fourcc_t i_chroma)
{
    const d3d_format_t *fmt = GetDirectRenderingFormat(vd, d3d_dev, i_chroma);
    if (fmt == nullptr || fmt->formatTexture == DXGI_FORMAT_UNKNOWN)
    {
        msg_Warn(vd, "chroma upscale of %4.4s not supported", (char*)&i_chroma);
        return nullptr;
    }

    d3d11_scaler *scaleProc = new (std::nothrow) d3d11_scaler;
    if (unlikely(scaleProc == nullptr))
        return nullptr;

    HRESULT hr;
    hr = d3d_dev->d3ddevice->QueryInterface(IID_GRAPHICS_PPV_ARGS(&scaleProc->d3dviddev));
    if (unlikely(FAILED(hr)))
    {
        msg_Err(vd, "Could not Query ID3D11VideoDevice Interface. (hr=0x%lX)", hr);
        goto error;
    }

    hr = d3d_dev->d3dcontext->QueryInterface(IID_GRAPHICS_PPV_ARGS(&scaleProc->d3dvidctx));
    if (unlikely(FAILED(hr)))
    {
        msg_Err(vd, "Could not Query ID3D11VideoContext Interface. (hr=0x%lX)", hr);
        goto error;
    }

    scaleProc->d3d_fmt = fmt;
    return scaleProc;
error:
    delete scaleProc;
    return nullptr;
}

static void ReleaseSRVs(d3d11_scaler *scaleProc)
{
    for (size_t i=0; i<ARRAY_SIZE(scaleProc->SRVs); i++)
    {
        if (scaleProc->SRVs[i])
        {
            scaleProc->SRVs[i]->Release();
            scaleProc->SRVs[i] = nullptr;
        }
    }
}

int D3D11_UpscalerUpdate(vlc_object_t *vd, d3d11_scaler *scaleProc, d3d11_device_t*d3d_dev,
                         const video_format_t *fmt, video_format_t *quad_fmt,
                         const vout_display_placement *cfg)
{
    HRESULT hr;
    ID3D11Texture2D *_upscaled[DXGI_MAX_SHADER_VIEW];
    ComPtr<ID3D11Texture2D> upscaled;
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outDesc{ };
    D3D11_VIDEO_COLOR black{};
    black.RGBA.A = 1.f;

    vout_display_place_t place{};
    auto display = *cfg;
    display.fitting = VLC_VIDEO_FIT_SMALLER;
    vout_display_PlacePicture(&place, fmt, &display);

    unsigned out_width, out_height;
    out_width  = (display.width + (scaleProc->d3d_fmt->widthDenominator-1)) & ~(scaleProc->d3d_fmt->widthDenominator-1);
    out_height = (display.height + (scaleProc->d3d_fmt->heightDenominator-1)) & ~(scaleProc->d3d_fmt->heightDenominator-1);

    quad_fmt->i_x_offset = 0;
    quad_fmt->i_width = quad_fmt->i_visible_width = out_width;
    quad_fmt->i_y_offset = 0;
    quad_fmt->i_height = quad_fmt->i_visible_height = out_height;

    if (scaleProc->Width == out_width && scaleProc->Height == out_height &&
        vout_display_PlaceEquals(&scaleProc->place, &place))
        // do nothing
        return VLC_SUCCESS;
    scaleProc->place = place;

    scaleProc->usable = false;

    if (scaleProc->enumerator.Get() == nullptr)
    {
        d3d11_device_lock(d3d_dev);
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC processorDesc{};
        processorDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
        processorDesc.InputFrameRate = {
            .Numerator   = fmt->i_frame_rate,
            .Denominator = fmt->i_frame_rate_base,
        };
        processorDesc.InputWidth   = fmt->i_width;
        processorDesc.InputHeight  = fmt->i_height;
        processorDesc.OutputWidth  = out_width;
        processorDesc.OutputHeight = out_height;
        processorDesc.OutputFrameRate = {
            .Numerator   = fmt->i_frame_rate,
            .Denominator = fmt->i_frame_rate_base,
        };
        processorDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
        hr = scaleProc->d3dviddev->CreateVideoProcessorEnumerator(&processorDesc, &scaleProc->enumerator);
        if (FAILED(hr))
        {
            msg_Dbg(vd, "Can't get a video processor for the video (error 0x%lx).", hr);
            d3d11_device_unlock(d3d_dev);
            goto done_super;
        }

        hr = scaleProc->d3dviddev->CreateVideoProcessor(scaleProc->enumerator.Get(), 0,
                                                        &scaleProc->processor);
        d3d11_device_unlock(d3d_dev);
        if (FAILED(hr))
        {
            msg_Dbg(vd, "failed to create the processor (error 0x%lx).", hr);
            goto done_super;
        }
    }

    if (scaleProc->Width != out_width || scaleProc->Height != out_height )
    {
        scaleProc->Width  = out_width;
        scaleProc->Height = out_height;

        // we need a texture that will receive the upscale version
        D3D11_TEXTURE2D_DESC texDesc;
        ZeroMemory(&texDesc, sizeof(texDesc));
        texDesc.MipLevels = 1;
        texDesc.SampleDesc.Count = 1;
        texDesc.MiscFlags = 0;
        texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.CPUAccessFlags = 0;
        texDesc.ArraySize = 1;
        texDesc.Format = scaleProc->d3d_fmt->formatTexture;
        texDesc.Width = scaleProc->Width;
        texDesc.Height = scaleProc->Height;
        hr = d3d_dev->d3ddevice->CreateTexture2D(&texDesc, nullptr, upscaled.GetAddressOf());
        if (FAILED(hr))
        {
            msg_Err(vd, "Failed to create the upscale texture. (hr=0x%lX)", hr);
            goto done_super;
        }
        msg_Dbg(vd, "upscale resolution %ux%u", texDesc.Width, texDesc.Height);

        outDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
        outDesc.Texture2D.MipSlice = 0;

        hr = scaleProc->d3dviddev->CreateVideoProcessorOutputView(
                                                                upscaled.Get(),
                                                                scaleProc->enumerator.Get(),
                                                                &outDesc,
                                                                scaleProc->outputView.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            msg_Dbg(vd,"Failed to create processor output. (hr=0x%lX)", hr);
            goto done_super;
        }

        ReleaseSRVs(scaleProc);
        _upscaled[0] = upscaled.Get();
        _upscaled[1] = upscaled.Get();
        _upscaled[2] = upscaled.Get();
        _upscaled[3] = upscaled.Get();
        if (D3D11_AllocateResourceView(vlc_object_logger(vd), d3d_dev->d3ddevice, scaleProc->d3d_fmt,
                                    _upscaled, 0, scaleProc->SRVs) != VLC_SUCCESS)
            goto done_super;
    }

    RECT srcRect;
    srcRect.left   = fmt->i_x_offset;
    srcRect.top    = fmt->i_y_offset;
    srcRect.right  = srcRect.left + fmt->i_visible_width;
    srcRect.bottom = srcRect.top  + fmt->i_visible_height;

    RECT dstRect;
    dstRect.left   = place.x;
    dstRect.top    = place.y;
    dstRect.right  = dstRect.left + place.width;
    dstRect.bottom = dstRect.top  + place.height;

    d3d11_device_lock(d3d_dev);
    scaleProc->d3dvidctx->VideoProcessorSetStreamSourceRect(scaleProc->processor.Get(),
                                                            0, TRUE, &srcRect);

    scaleProc->d3dvidctx->VideoProcessorSetStreamDestRect(scaleProc->processor.Get(),
                                                          0, TRUE, &dstRect);

    scaleProc->d3dvidctx->VideoProcessorSetOutputBackgroundColor(scaleProc->processor.Get(),
                                                                 0, &black);

    d3d11_device_unlock(d3d_dev);

    scaleProc->usable = true;
    return VLC_SUCCESS;
done_super:
    ReleaseSRVs(scaleProc);
    scaleProc->processor.Reset();
    scaleProc->enumerator.Reset();
    return VLC_EGENERIC;
}

void D3D11_UpscalerDestroy(d3d11_scaler *scaleProc)
{
    ReleaseSRVs(scaleProc);
    delete scaleProc;
}


static int assert_ProcessorInput(vlc_object_t *vd, d3d11_scaler *scaleProc, picture_sys_d3d11_t *p_sys_src)
{
    if (!p_sys_src->processorInput)
    {
        D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inDesc{};
        inDesc.FourCC = 0;
        inDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
        inDesc.Texture2D.MipSlice = 0;
        inDesc.Texture2D.ArraySlice = p_sys_src->slice_index;

        HRESULT hr;

        hr = scaleProc->d3dviddev->CreateVideoProcessorInputView(
                                                             p_sys_src->resource[KNOWN_DXGI_INDEX],
                                                             scaleProc->enumerator.Get(),
                                                             &inDesc,
                                                             &p_sys_src->processorInput);
        if (FAILED(hr))
        {
#ifndef NDEBUG
            msg_Dbg(vd,"Failed to create processor input for slice %d. (hr=0x%lX)", p_sys_src->slice_index, hr);
#endif
            return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
}

int D3D11_UpscalerScale(vlc_object_t *vd, d3d11_scaler *scaleProc, picture_sys_d3d11_t *p_sys)
{
    HRESULT hr;

    if (assert_ProcessorInput(vd, scaleProc, p_sys) != VLC_SUCCESS)
    {
        msg_Err(vd, "fail to create upscaler input");
        return VLC_EGENERIC;
    }

    D3D11_VIDEO_PROCESSOR_STREAM stream{};
    stream.Enable = TRUE;
    stream.pInputSurface = p_sys->processorInput;

    hr = scaleProc->d3dvidctx->VideoProcessorBlt(scaleProc->processor.Get(),
                                                 scaleProc->outputView.Get(),
                                                 0, 1, &stream);
    if (FAILED(hr))
    {
        msg_Err(vd, "Failed to render the upscaled texture. (hr=0x%lX)", hr);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

void D3D11_UpscalerGetSize(const d3d11_scaler *scaleProc, unsigned *i_width, unsigned *i_height)
{
    *i_width  = scaleProc->Width;
    *i_height = scaleProc->Height;
}

bool D3D11_UpscalerUsed(const d3d11_scaler *scaleProc)
{
    return scaleProc->usable;
}

void D3D11_UpscalerGetSRV(const d3d11_scaler *scaleProc, ID3D11ShaderResourceView *SRV[DXGI_MAX_SHADER_VIEW])
{
    for (size_t i=0; i<DXGI_MAX_SHADER_VIEW; i++)
        SRV[i] = scaleProc->SRVs[i];
}
