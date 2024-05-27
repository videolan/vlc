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
#include <assert.h>

#include "d3d11_scaler.h"
#ifdef HAVE_AMF_SCALER
#include "../../hw/amf/amf_helper.h"
#include <AMF/components/HQScaler.h>
#endif

#include <new>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

struct d3d11_scaler
{
    bool                            usable = false;
    const d3d_format_t              *d3d_fmt = nullptr;
    vout_display_place_t            place = {};
    bool                            super_res = false;
    bool                            upscaling = false;
    UINT                            Width  = 0;
    UINT                            Height = 0;
    ComPtr<ID3D11VideoDevice>               d3dviddev;
    ComPtr<ID3D11VideoContext>              d3dvidctx;
    ComPtr<ID3D11VideoProcessorEnumerator>  enumerator;
    ComPtr<ID3D11VideoProcessor>            processor;
    ComPtr<ID3D11VideoProcessorOutputView>  outputView;
    ID3D11ShaderResourceView                *SRVs[D3D11_MAX_SHADER_VIEW] = {};
    picture_sys_t                           picsys{};

#ifdef HAVE_AMF_SCALER
    vlc_amf_context                 amf = {};
    amf::AMFComponent               *amf_scaler = nullptr;
    bool                            amf_initialized{false};
    amf::AMFSurface                 *amfInput = nullptr;
    d3d11_device_t                  *d3d_dev = nullptr;
#endif
};

static const d3d_format_t *GetDirectRenderingFormat(vlc_object_t *vd, d3d11_device_t *d3d_dev, vlc_fourcc_t i_src_chroma)
{
    UINT supportFlags = D3D11_FORMAT_SUPPORT_SHADER_LOAD | D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_OUTPUT;
    return (FindD3D11Format)( vd, d3d_dev, i_src_chroma, false, 0, 0, 0, is_d3d11_opaque(i_src_chroma), supportFlags );
}

d3d11_scaler *D3D11_UpscalerCreate(vlc_object_t *vd, d3d11_device_t *d3d_dev, vlc_fourcc_t i_chroma,
                                   bool super_res, const d3d_format_t **out_fmt)
{
    if ((*out_fmt)->formatTexture == DXGI_FORMAT_UNKNOWN)
    {
        msg_Warn(vd, "chroma upscale of %4.4s not supported", (char*)&i_chroma);
        return nullptr;
    }

    bool canProcess = !super_res;
#ifdef HAVE_AMF_SCALER
    struct vlc_amf_context amf = {};
    amf::AMFComponent *amf_scaler = nullptr;
#endif
    if (super_res)
    {
    // NVIDIA 530+ driver
    if (d3d_dev->adapterDesc.VendorId == GPU_MANUFACTURER_NVIDIA &&
        (d3d_dev->WDDM.revision * 10000 + d3d_dev->WDDM.build) > 153000)
    {
        constexpr GUID kNvidiaPPEInterfaceGUID{ 0xd43ce1b3, 0x1f4b, 0x48ac, {0xba, 0xee, 0xc3, 0xc2, 0x53, 0x75, 0xe6, 0xf7} };
        HRESULT hr;
        UINT available = 0;
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC processorDesc{};

        ComPtr<ID3D11VideoContext>              d3dvidctx;
        ComPtr<ID3D11VideoDevice>               d3dviddev;
        ComPtr<ID3D11VideoProcessorEnumerator>  enumerator;
        ComPtr<ID3D11VideoProcessor>            processor;

        d3d11_device_lock(d3d_dev);
        hr = d3d_dev->d3dcontext->QueryInterface(IID_GRAPHICS_PPV_ARGS(&d3dvidctx));
        if (unlikely(FAILED(hr)))
            goto checked;
        hr = d3d_dev->d3ddevice->QueryInterface(IID_GRAPHICS_PPV_ARGS(&d3dviddev));
        if (unlikely(FAILED(hr)))
            goto checked;

        processorDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
        processorDesc.InputFrameRate = { 1, 25 };
        processorDesc.InputWidth   = 1280;
        processorDesc.InputHeight  = 720;
        processorDesc.OutputWidth  = 1920;
        processorDesc.OutputHeight = 1080;
        processorDesc.OutputFrameRate = { 1, 25 };
        processorDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
        hr = d3dviddev->CreateVideoProcessorEnumerator(&processorDesc, &enumerator);
        if (unlikely(FAILED(hr)))
            goto checked;
        hr = d3dviddev->CreateVideoProcessor(enumerator.Get(), 0, &processor);
        if (unlikely(FAILED(hr)))
            goto checked;

        hr = d3dvidctx->VideoProcessorGetStreamExtension(processor.Get(),
                    0, &kNvidiaPPEInterfaceGUID, sizeof(available), &available);
checked:
        d3d11_device_unlock(d3d_dev);
        canProcess = available != 0;
    }
    else if (d3d_dev->adapterDesc.VendorId == GPU_MANUFACTURER_INTEL)
    {
        canProcess = true; // detection doesn't work
    }
#ifdef HAVE_AMF_SCALER
    else if (d3d_dev->adapterDesc.VendorId == GPU_MANUFACTURER_AMD)
    {
        int res = vlc_AMFCreateContext(&amf);
        if (res == VLC_SUCCESS)
        {
            AMF_RESULT res = amf.pFactory->CreateComponent(amf.Context, AMFHQScaler, &amf_scaler);
            if (res == AMF_OK && amf_scaler)
            {
                res = amf.Context->InitDX11(d3d_dev->d3ddevice);
                canProcess = res == AMF_OK;
            }
        }
    }
#endif
    }

    d3d11_scaler *scaleProc = nullptr;
    const d3d_format_t *fmt = nullptr;
    if (!canProcess)
    {
        msg_Err(vd, "Super Resolution filter not supported");
        goto error;
    }

#ifdef HAVE_AMF_SCALER
    if (amf_scaler != nullptr)
    {
        auto amf_fmt = DXGIToAMF((*out_fmt)->formatTexture);
        if (amf_fmt == amf::AMF_SURFACE_UNKNOWN)
        {
            msg_Warn(vd, "upscale of DXGI %s not supported", DxgiFormatToStr((*out_fmt)->formatTexture));
            goto error;
        }
        fmt = *out_fmt;
    }
#endif
    if (fmt == nullptr && (*out_fmt)->bitsPerChannel > 10)
        fmt = GetDirectRenderingFormat(vd, d3d_dev, VLC_CODEC_RGBA64);
    if (fmt == nullptr && (*out_fmt)->bitsPerChannel > 8)
        fmt = GetDirectRenderingFormat(vd, d3d_dev, VLC_CODEC_RGBA10);
    if (fmt == nullptr)
        fmt = GetDirectRenderingFormat(vd, d3d_dev, VLC_CODEC_BGRA);
    if (fmt == nullptr)
        fmt = GetDirectRenderingFormat(vd, d3d_dev, VLC_CODEC_RGBA);
    if (fmt == nullptr || fmt->formatTexture == DXGI_FORMAT_UNKNOWN)
    {
        msg_Warn(vd, "chroma upscale of %4.4s not supported", (char*)&i_chroma);
        goto error;
    }

    scaleProc = new (std::nothrow) d3d11_scaler;
    if (unlikely(scaleProc == nullptr))
        goto error;

#ifdef HAVE_AMF_SCALER
    if (amf_scaler != nullptr)
    {
        scaleProc->amf = amf;
        scaleProc->amf_scaler = amf_scaler;
        scaleProc->d3d_dev = d3d_dev;
    }
    else
    {
#endif
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
#ifdef HAVE_AMF_SCALER
    }
#endif

    scaleProc->d3d_fmt = fmt;
    scaleProc->super_res = super_res;
    *out_fmt = scaleProc->d3d_fmt;
    return scaleProc;
error:
#ifdef HAVE_AMF_SCALER
    if (amf_scaler)
        amf_scaler->Release();
    if (amf.Context)
        vlc_AMFReleaseContext(&amf);
#endif
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

picture_sys_t *D3D11_UpscalerGetOutput(d3d11_scaler *scaleProc)
{
    return &scaleProc->picsys;
}

int D3D11_UpscalerUpdate(vlc_object_t *vd, d3d11_scaler *scaleProc, d3d11_device_t*d3d_dev,
                         const video_format_t *fmt, video_format_t *quad_fmt,
                         const vout_display_cfg_t *cfg)
{
    HRESULT hr;
    ID3D11Texture2D *_upscaled[D3D11_MAX_SHADER_VIEW];
    ComPtr<ID3D11Texture2D> upscaled;
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outDesc{ };
    D3D11_VIDEO_COLOR black{};
    black.RGBA.A = 1.f;
    bool upscale = false;

    vout_display_place_t place{};
    auto display = *cfg;
    display.is_display_filled = true;
    vout_display_PlacePicture(&place, fmt, &display, true);

    unsigned out_width, out_height;
    out_width  = (display.display.width + (scaleProc->d3d_fmt->widthDenominator-1)) & ~(scaleProc->d3d_fmt->widthDenominator-1);
    out_height = (display.display.height + (scaleProc->d3d_fmt->heightDenominator-1)) & ~(scaleProc->d3d_fmt->heightDenominator-1);

    quad_fmt->i_x_offset = 0;
    quad_fmt->i_width = quad_fmt->i_visible_width = out_width;
    quad_fmt->i_y_offset = 0;
    quad_fmt->i_height = quad_fmt->i_visible_height = out_height;
    quad_fmt->i_sar_num = 1;
    quad_fmt->i_sar_den = 1;
    quad_fmt->b_color_range_full = true;

    if (scaleProc->Width == out_width && scaleProc->Height == out_height &&
        memcmp(&scaleProc->place, &place, sizeof(place)) == 0)
        // do nothing
        return VLC_SUCCESS;
    scaleProc->place = place;

    scaleProc->usable = false;

#ifdef HAVE_AMF_SCALER
    if (!scaleProc->amf_scaler)
#endif
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
        texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.CPUAccessFlags = 0;
        texDesc.ArraySize = 1;
        texDesc.Format = scaleProc->d3d_fmt->formatTexture;
        texDesc.Width = scaleProc->Width;
        texDesc.Height = scaleProc->Height;
        texDesc.MiscFlags = 0;
#ifdef HAVE_AMF_SCALER
        if (scaleProc->amf_scaler)
            texDesc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED;
#endif
        hr = d3d_dev->d3ddevice->CreateTexture2D(&texDesc, nullptr, upscaled.GetAddressOf());
        if (FAILED(hr))
        {
            msg_Err(vd, "Failed to create the upscale texture. (hr=0x%lX)", hr);
            goto done_super;
        }
        msg_Dbg(vd, "upscale resolution %ux%u", texDesc.Width, texDesc.Height);

        outDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
        outDesc.Texture2D.MipSlice = 0;

#ifdef HAVE_AMF_SCALER
        if (scaleProc->amf_scaler)
        {
            AMF_RESULT res;

            texDesc.Width  = fmt->i_x_offset + fmt->i_visible_width;
            texDesc.Height = fmt->i_y_offset + fmt->i_visible_height;
            if (scaleProc->amfInput != nullptr)
            {
                D3D11_TEXTURE2D_DESC stagingDesc;
                auto packed = scaleProc->amfInput->GetPlane(amf::AMF_PLANE_PACKED);
                ID3D11Texture2D *amfStaging = reinterpret_cast<ID3D11Texture2D *>(packed->GetNative());
                amfStaging->GetDesc(&stagingDesc);
                if (stagingDesc.Width != texDesc.Width || stagingDesc.Height != texDesc.Height)
                {
                    scaleProc->amfInput->Release();
                    scaleProc->amfInput = nullptr;
                }
            }

            if (scaleProc->amfInput == nullptr)
            {
                res = scaleProc->amf.Context->AllocSurface(amf::AMF_MEMORY_DX11,
                                                           DXGIToAMF(scaleProc->d3d_fmt->formatTexture),
                                                           texDesc.Width, texDesc.Height,
                                                           &scaleProc->amfInput);
                if (unlikely(res != AMF_OK || scaleProc->amfInput == nullptr))
                {
                    msg_Err(vd, "Failed to wrap D3D11 output texture. %d", res);
                    goto done_super;
                }
            }
        }
        else
        {
#endif
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
#ifdef HAVE_AMF_SCALER
        }
#endif

        ReleaseSRVs(scaleProc);
        _upscaled[0] = upscaled.Get();
        _upscaled[1] = upscaled.Get();
        _upscaled[2] = upscaled.Get();
        _upscaled[3] = upscaled.Get();
        if ((D3D11_AllocateShaderView)(vd, d3d_dev->d3ddevice, scaleProc->d3d_fmt,
                                    _upscaled, 0, scaleProc->SRVs) != VLC_SUCCESS)
            goto done_super;

#ifdef HAVE_AMF_SCALER
        if (scaleProc->amf_scaler)
        {
            AMF_RESULT res;
            res = scaleProc->amf_scaler->SetProperty(AMF_HQ_SCALER_OUTPUT_SIZE, ::AMFConstructSize(out_width, out_height));
            res = scaleProc->amf_scaler->SetProperty(AMF_HQ_SCALER_ENGINE_TYPE, amf::AMF_MEMORY_DX11);
            res = scaleProc->amf_scaler->SetProperty(AMF_HQ_SCALER_ALGORITHM, AMF_HQ_SCALER_ALGORITHM_VIDEOSR1_0);
            res = scaleProc->amf_scaler->SetProperty(AMF_HQ_SCALER_FROM_SRGB, 0);
            res = scaleProc->amf_scaler->SetProperty(AMF_HQ_SCALER_SHARPNESS, 0.5);
            res = scaleProc->amf_scaler->SetProperty(AMF_HQ_SCALER_FILL, 1);
            AMFColor black{0,0,0,255};
            res = scaleProc->amf_scaler->SetProperty(AMF_HQ_SCALER_FILL_COLOR, black);
            // res = scaleProc->amf_scaler->SetProperty(AMF_HQ_SCALER_FRAME_RATE, oFrameRate);
            auto amf_fmt = DXGIToAMF(scaleProc->d3d_fmt->formatTexture);
            if (scaleProc->amf_initialized)
                res = scaleProc->amf_scaler->ReInit(
                    fmt->i_x_offset + fmt->i_visible_width,
                    fmt->i_y_offset + fmt->i_visible_height);
            else
                res = scaleProc->amf_scaler->Init(amf_fmt,
                    fmt->i_x_offset + fmt->i_visible_width,
                    fmt->i_y_offset + fmt->i_visible_height);
            if (res != AMF_OK)
            {
                msg_Err(vd, "Failed to (re)initialize scaler, (err=%d)", res);
                return false;
            }
            scaleProc->amf_initialized = true;
        }
#endif

        if (scaleProc->picsys.processorInput)
        {
            scaleProc->picsys.processorInput->Release();
            scaleProc->picsys.processorInput = NULL;
        }
        if (scaleProc->picsys.processorOutput)
        {
            scaleProc->picsys.processorOutput->Release();
            scaleProc->picsys.processorOutput = NULL;
        }
        scaleProc->picsys.texture[0] = upscaled.Get();
        for (size_t i=0; i<ARRAY_SIZE(scaleProc->picsys.resourceView); i++)
            scaleProc->picsys.resourceView[i] = scaleProc->SRVs[i];
        scaleProc->picsys.formatTexture = texDesc.Format;
    }

#ifdef HAVE_AMF_SCALER
    if (!scaleProc->amf_scaler)
    {
#endif
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

    if (scaleProc->super_res)
    {
        // only use super resolution when source is smaller than display
        upscale = fmt->i_visible_width < place.width
               || fmt->i_visible_height < place.height;

        if (d3d_dev->adapterDesc.VendorId == GPU_MANUFACTURER_NVIDIA)
        {
            constexpr GUID kNvidiaPPEInterfaceGUID{ 0xd43ce1b3, 0x1f4b, 0x48ac, {0xba, 0xee, 0xc3, 0xc2, 0x53, 0x75, 0xe6, 0xf7} };
            constexpr UINT kStreamExtensionVersionV1 = 0x1;
            constexpr UINT kStreamExtensionMethodRTXVSR = 0x2;
            struct {
                UINT version;
                UINT method;
                UINT enable;
            } stream_extension_info = {
                kStreamExtensionVersionV1,
                kStreamExtensionMethodRTXVSR,
                upscale ? 1u : 0u,
            };

            hr = scaleProc->d3dvidctx->VideoProcessorSetStreamExtension(
                scaleProc->processor.Get(),
                0, &kNvidiaPPEInterfaceGUID, sizeof(stream_extension_info),
                &stream_extension_info);

            if (FAILED(hr)) {
                msg_Err(vd, "Failed to set the NVIDIA video process stream extension. (hr=0x%lX)", hr);
                d3d11_device_unlock(d3d_dev);
                goto done_super;
            }
        }
        else if (d3d_dev->adapterDesc.VendorId == GPU_MANUFACTURER_INTEL)
        {
            constexpr GUID GUID_INTEL_VPE_INTERFACE{ 0xedd1d4b9, 0x8659, 0x4cbc, {0xa4, 0xd6, 0x98, 0x31, 0xa2, 0x16, 0x3a, 0xc3}};

            constexpr UINT kIntelVpeFnVersion = 0x01;
            constexpr UINT kIntelVpeFnMode    = 0x20;
            constexpr UINT kIntelVpeFnScaling = 0x37;

            // values for kIntelVpeFnVersion
            constexpr UINT kIntelVpeVersion3 = 0x0003;

            // values for kIntelVpeFnMode
            constexpr UINT kIntelVpeModeNone    = 0x0;
            constexpr UINT kIntelVpeModePreproc = 0x1;

            // values for kIntelVpeFnScaling
            constexpr UINT kIntelVpeScalingDefault         = 0x0;
            constexpr UINT kIntelVpeScalingSuperResolution = 0x2;

            UINT param;
            struct {
                UINT function;
                void *param;
            } ext = {
                0,
                &param,
            };

            ext.function = kIntelVpeFnVersion;
            param = kIntelVpeVersion3;
            hr = scaleProc->d3dvidctx->VideoProcessorSetOutputExtension(
                scaleProc->processor.Get(),
                &GUID_INTEL_VPE_INTERFACE, sizeof(ext), &ext);

            if (FAILED(hr)) {
                msg_Err(vd, "Failed to set the Intel VPE version. (hr=0x%lX)", hr);
                d3d11_device_unlock(d3d_dev);
                goto done_super;
            }

            ext.function = kIntelVpeFnMode;
            param = upscale ? kIntelVpeModePreproc : kIntelVpeModeNone;
            hr = scaleProc->d3dvidctx->VideoProcessorSetOutputExtension(
                scaleProc->processor.Get(),
                &GUID_INTEL_VPE_INTERFACE, sizeof(ext), &ext);
            if (FAILED(hr)) {
                msg_Err(vd, "Failed to set the Intel VPE mode. (hr=0x%lX)", hr);
                d3d11_device_unlock(d3d_dev);
                goto done_super;
            }

            ext.function = kIntelVpeFnScaling;
            param = upscale ? kIntelVpeScalingSuperResolution : kIntelVpeScalingDefault;
            hr = scaleProc->d3dvidctx->VideoProcessorSetStreamExtension(
                scaleProc->processor.Get(), 0,
                &GUID_INTEL_VPE_INTERFACE, sizeof(ext), &ext);
            if (FAILED(hr)) {
                msg_Err(vd, "Failed to set the Intel VPE scaling type. (hr=0x%lX)", hr);
                d3d11_device_unlock(d3d_dev);
                goto done_super;
            }
        }
    }
    d3d11_device_unlock(d3d_dev);
#ifdef HAVE_AMF_SCALER
    }
#endif

    if (scaleProc->upscaling != upscale)
    {
        msg_Dbg(vd, "turning VSR %s", upscale ? "ON" : "OFF");
        scaleProc->upscaling = upscale;
    }

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
#ifdef HAVE_AMF_SCALER
    if (scaleProc->amfInput != nullptr)
    {
        scaleProc->amfInput->Release();
        scaleProc->amfInput = nullptr;
    }
    if (scaleProc->amf_scaler)
    {
        scaleProc->amf_scaler->Terminate();
        scaleProc->amf_scaler->Release();
    }
    if (scaleProc->amf.Context)
        vlc_AMFReleaseContext(&scaleProc->amf);
#endif
    delete scaleProc;
}


static int assert_ProcessorInput(vlc_object_t *vd, d3d11_scaler *scaleProc, picture_sys_t *p_sys_src)
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

int D3D11_UpscalerScale(vlc_object_t *vd, d3d11_scaler *scaleProc, picture_sys_t *p_sys)
{
    HRESULT hr;

#ifdef HAVE_AMF_SCALER
    if (scaleProc->amf_scaler)
    {
        AMF_RESULT res;
        amf::AMFSurface *submitSurface;

        auto packedStaging = scaleProc->amfInput->GetPlane(amf::AMF_PLANE_PACKED);
        ID3D11Texture2D *amfStaging = reinterpret_cast<ID3D11Texture2D *>(packedStaging->GetNative());

#ifndef NDEBUG
        D3D11_TEXTURE2D_DESC stagingDesc, inputDesc;
        amfStaging->GetDesc(&stagingDesc);
        p_sys->texture[KNOWN_DXGI_INDEX]->GetDesc(&inputDesc);
        assert(stagingDesc.Width == inputDesc.Width);
        assert(stagingDesc.Height == inputDesc.Height);
        assert(stagingDesc.Format == inputDesc.Format);
#endif

        // copy source into staging as it may not be shared
        d3d11_device_lock( scaleProc->d3d_dev );
        scaleProc->d3d_dev->d3dcontext->CopySubresourceRegion(amfStaging,
                                                0,
                                                0, 0, 0,
                                                p_sys->texture[KNOWN_DXGI_INDEX],
                                                p_sys->slice_index,
                                                NULL);
        d3d11_device_unlock( scaleProc->d3d_dev );
        submitSurface = scaleProc->amfInput;

        res = scaleProc->amf_scaler->SubmitInput(submitSurface);
        if (res == AMF_INPUT_FULL)
        {
            msg_Dbg(vd, "scaler input full, skip this frame");
            return VLC_SUCCESS;
        }
        if (res != AMF_OK)
        {
            msg_Err(vd, "scaler input failed, (err=%d)", res);
            return VLC_EGENERIC;
        }

        amf::AMFData *amfOutput = nullptr;
        res = scaleProc->amf_scaler->QueryOutput(&amfOutput);
        if (res != AMF_OK)
        {
            msg_Err(vd, "scaler gave no output full, (err=%d)", res);
            return VLC_EGENERIC;
        }

        assert(amfOutput->GetMemoryType() == amf::AMF_MEMORY_DX11);
        amf::AMFSurface *amfOutputSurface = reinterpret_cast<amf::AMFSurface*>(amfOutput);
        auto packed = amfOutputSurface->GetPlane(amf::AMF_PLANE_PACKED);

        ID3D11Texture2D *out = reinterpret_cast<ID3D11Texture2D *>(packed->GetNative());

#ifndef NDEBUG
        D3D11_TEXTURE2D_DESC outDesc;
        out->GetDesc(&outDesc);
        assert(outDesc.Width == scaleProc->Width);
        assert(outDesc.Height == scaleProc->Height);
        assert(outDesc.Format == inputDesc.Format);
#endif

        ReleaseSRVs(scaleProc);
        ID3D11Texture2D *_upscaled[D3D11_MAX_SHADER_VIEW];
        _upscaled[0] = out;
        _upscaled[1] = out;
        _upscaled[2] = out;
        _upscaled[3] = out;
        if (D3D11_AllocateShaderView(vd, scaleProc->d3d_dev->d3ddevice, scaleProc->d3d_fmt,
                                    _upscaled, 0, scaleProc->SRVs) != VLC_SUCCESS)
        {
            return (-ENOTSUP);
        }

        amfOutput->Release();

        return VLC_SUCCESS;
    }
#endif
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

void D3D11_UpscalerGetSRV(const d3d11_scaler *scaleProc, ID3D11ShaderResourceView *SRV[D3D11_MAX_SHADER_VIEW])
{
    for (size_t i=0; i<D3D11_MAX_SHADER_VIEW; i++)
        SRV[i] = scaleProc->SRVs[i];
}
