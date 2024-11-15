/*****************************************************************************
 * mft_d3d11.cpp : Media Foundation Transform audio/video decoder D3D11 helpers
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
 *
 * Author: Steve Lhomme <slhomme@videolabs.io>
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

#ifndef _MSC_VER // including mfapi with mingw-w64 is not clean for UWP yet
#include <process.h>
#include <winapifamily.h>
#undef WINAPI_FAMILY
#define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "mft_d3d11.h"
#include <mfidl.h>

using Microsoft::WRL::ComPtr;


struct mf_d3d11_pic_ctx
{
    struct d3d11_pic_context ctx;
    IMFDXGIBuffer *out_media;
    vlc_mft_ref  *mfdec;
};
#define MF_D3D11_PICCONTEXT_FROM_PICCTX(pic_ctx)  \
    container_of(pic_ctx, mf_d3d11_pic_ctx, ctx.s)

static void d3d11mf_pic_context_destroy(picture_context_t *ctx)
{
    mf_d3d11_pic_ctx *pic_ctx = MF_D3D11_PICCONTEXT_FROM_PICCTX(ctx);
    vlc_mft_ref *mfdec = pic_ctx->mfdec;
    pic_ctx->out_media->Release();
    static_assert(offsetof(mf_d3d11_pic_ctx, ctx.s) == 0, "Cast assumption failure");
    d3d11_pic_context_destroy(ctx);
    mfdec->Release();
}

static picture_context_t *d3d11mf_pic_context_copy(picture_context_t *ctx)
{
    mf_d3d11_pic_ctx *src_ctx = MF_D3D11_PICCONTEXT_FROM_PICCTX(ctx);
    mf_d3d11_pic_ctx *pic_ctx = static_cast<mf_d3d11_pic_ctx *>(malloc(sizeof(*pic_ctx)));
    if (unlikely(pic_ctx==nullptr))
        return nullptr;
    *pic_ctx = *src_ctx;
    vlc_video_context_Hold(pic_ctx->ctx.s.vctx);
    pic_ctx->out_media->AddRef();
    pic_ctx->mfdec->AddRef();
    for (int i=0;i<DXGI_MAX_SHADER_VIEW; i++)
    {
        pic_ctx->ctx.picsys.resource[i]  = src_ctx->ctx.picsys.resource[i];
        pic_ctx->ctx.picsys.renderSrc[i] = src_ctx->ctx.picsys.renderSrc[i];
    }
    AcquireD3D11PictureSys(&pic_ctx->ctx.picsys);
    return &pic_ctx->ctx.s;
}

static picture_context_t *NewPicContext(ComPtr<ID3D11Texture2D> & texture, UINT slice,
                                        ComPtr<IMFDXGIBuffer> &spDXGIBuffer,
                                        vlc_mft_ref *mfdec,
                                        ID3D11ShaderResourceView *renderSrc[DXGI_MAX_SHADER_VIEW],
                                        vlc_video_context *vctx)
{
    mf_d3d11_pic_ctx *pic_ctx = static_cast<mf_d3d11_pic_ctx *>(calloc(1, sizeof(*pic_ctx)));
    if (unlikely(pic_ctx==nullptr))
        return nullptr;

    spDXGIBuffer.CopyTo(&pic_ctx->out_media);
    pic_ctx->mfdec = mfdec;
    pic_ctx->mfdec->AddRef();

    pic_ctx->ctx.s.copy = d3d11mf_pic_context_copy;
    pic_ctx->ctx.s.destroy = d3d11mf_pic_context_destroy;
    pic_ctx->ctx.s.vctx = vlc_video_context_Hold(vctx);

    pic_ctx->ctx.picsys.slice_index = slice;
    pic_ctx->ctx.picsys.sharedHandle = INVALID_HANDLE_VALUE;
    for (int i=0;i<DXGI_MAX_SHADER_VIEW; i++)
    {
        pic_ctx->ctx.picsys.texture[i] = texture.Get();
        pic_ctx->ctx.picsys.renderSrc[i] = renderSrc ? renderSrc[i] : NULL;
    }
    AcquireD3D11PictureSys(&pic_ctx->ctx.picsys);
    return &pic_ctx->ctx.s;
}

HRESULT MFHW_d3d11::SetupVideoContext(vlc_logger *logger, ComPtr<IMFDXGIBuffer> &spDXGIBuffer,
                                      es_format_t & fmt_out)
{
    HRESULT hr;
    ComPtr<ID3D11Texture2D> d3d11Res;
    hr = spDXGIBuffer->GetResource(IID_GRAPHICS_PPV_ARGS(d3d11Res.GetAddressOf()));
    if (FAILED(hr))
    {
        vlc_warning(logger, "DXGI buffer is a not D3D11");
        return hr;
    }

    d3d11_decoder_device_t *dev_sys = GetD3D11OpaqueDevice(dec_dev);
    assert(dev_sys != nullptr);

    D3D11_TEXTURE2D_DESC desc;
    d3d11Res->GetDesc(&desc);
    vctx_out = D3D11CreateVideoContext( dec_dev, desc.Format, DXGI_FORMAT_UNKNOWN );
    if (unlikely(vctx_out == NULL))
    {
        vlc_error(logger, "failed to create a video context");
        return E_OUTOFMEMORY;
    }
    fmt_out.video.i_width = desc.Width;
    fmt_out.video.i_height = desc.Height;

    cfg = D3D11_RenderFormat(desc.Format, DXGI_FORMAT_UNKNOWN, true);

    fmt_out.i_codec = cfg->fourcc;
    fmt_out.video.i_chroma = cfg->fourcc;

    // pre allocate all the SRV for that texture
    for (size_t slice=0; slice < desc.ArraySize; slice++)
    {
        ID3D11Texture2D *tex[DXGI_MAX_SHADER_VIEW] = {
            d3d11Res.Get(), d3d11Res.Get(), d3d11Res.Get(), d3d11Res.Get()
        };

        if (D3D11_AllocateResourceView(logger, dev_sys->d3d_dev.d3ddevice, cfg,
                                        tex, slice, cachedSRV[slice]) != VLC_SUCCESS)
        {
            return E_OUTOFMEMORY;
        }
    }
    cached_tex = d3d11Res;

    return S_OK;
}

picture_context_t *MFHW_d3d11::CreatePicContext(vlc_logger *logger, ComPtr<IMFDXGIBuffer> &spDXGIBuffer,
                                                vlc_mft_ref *mfdec)
{
    assert(vctx_out != nullptr);
    HRESULT hr;
    ComPtr<ID3D11Texture2D> d3d11Res;
    hr = spDXGIBuffer->GetResource(IID_GRAPHICS_PPV_ARGS(d3d11Res.GetAddressOf()));
    assert(SUCCEEDED(hr));

    UINT sliceIndex = 0;
    D3D11_TEXTURE2D_DESC desc;
    d3d11Res->GetDesc(&desc);

    hr = spDXGIBuffer->GetSubresourceIndex(&sliceIndex);
    if (desc.ArraySize == 1)
    {
        // each output is a different texture
        assert(sliceIndex == 0);

        d3d11_decoder_device_t *dev_sys = GetD3D11OpaqueDevice(dec_dev);

        for (size_t j=0; j < ARRAY_SIZE(cachedSRV[sliceIndex]); j++)
        {
            if (cachedSRV[sliceIndex][j] != nullptr)
            {
                cachedSRV[sliceIndex][j]->Release();
                cachedSRV[sliceIndex][j] = nullptr;
            }
        }

        ID3D11Texture2D *tex[DXGI_MAX_SHADER_VIEW] = {
            d3d11Res.Get(), d3d11Res.Get(), d3d11Res.Get(), d3d11Res.Get()
        };

        if (D3D11_AllocateResourceView(logger, dev_sys->d3d_dev.d3ddevice, cfg,
                                        tex, sliceIndex, cachedSRV[sliceIndex]) != VLC_SUCCESS)
        {
            return nullptr;
        }
    }
    else if (cached_tex != d3d11Res)
    {
        vlc_error(logger, "separate texture not supported");
        return nullptr;
    }

    return NewPicContext(d3d11Res, sliceIndex, spDXGIBuffer, mfdec, cachedSRV[sliceIndex], vctx_out);
}

HRESULT MFHW_d3d11::SetD3D(vlc_logger *logger, vlc_decoder_device & dec_dev, ComPtr<IMFTransform> & mft)
{
    auto *devsys = GetD3D11OpaqueDevice(&dec_dev);
    if (unlikely(devsys == nullptr))
    {
        vlc_warning(logger, "invalid D3D11 decoder device");
        return E_INVALIDARG;
    }

    if (!(devsys->d3d_dev.d3ddevice->GetCreationFlags() & D3D11_CREATE_DEVICE_VIDEO_SUPPORT))
    {
        vlc_warning(logger, "the provided D3D11 device doesn't support decoding");
        return E_NOINTERFACE;
    }

    return MFHW_d3d::SetD3D(logger, dec_dev, devsys->d3d_dev.d3ddevice, mft);
}

void MFHW_d3d11::Release(ComPtr<IMFTransform> & mft)
{
    for (size_t i=0; i < ARRAY_SIZE(cachedSRV); i++)
    {
        for (size_t j=0; j < ARRAY_SIZE(cachedSRV[i]); j++)
        {
            if (cachedSRV[i][j] != nullptr)
            {
                cachedSRV[i][j]->Release();
                cachedSRV[i][j] = nullptr;
            }
        }
    }
    MFHW_d3d::Release(mft);
}