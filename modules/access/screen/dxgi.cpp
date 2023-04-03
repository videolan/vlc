/*****************************************************************************
 * dxgi.cpp: Screen capture module for DXGI.
 *****************************************************************************
 * Copyright (C) 2018-2022 VLC authors and VideoLAN
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

#include <vlc_common.h>

#include "screen.h"

#include "../../video_chroma/d3d11_fmt.h"

#include <d3d11_1.h>

#include <new>

#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

struct screen_data_t
{
    const d3d_format_t             *output_format = nullptr;
    vlc_video_context              *vctx = nullptr;

    int screen_x = 0;
    int screen_y = 0;

    ComPtr<IDXGIOutputDuplication> duplication;

    ~screen_data_t()
    {
        if (vctx)
            vlc_video_context_Release(vctx);
    }
};

static void CaptureBlockRelease( block_t *p_block )
{
    block_sys_d3d11_t *d3d11_block = D3D11BLOCK_FROM_BLOCK(p_block);
    picture_Release(d3d11_block->d3d11_pic);
    delete d3d11_block;
}

static block_t *screen_Capture(demux_t *p_demux)
{
    demux_sys_t *p_sys = static_cast<demux_sys_t*>(p_demux->p_sys);
    screen_data_t *p_data = p_sys->p_data;
    block_sys_d3d11_t *d3d11_block = new (std::nothrow) block_sys_d3d11_t();
    ComPtr<IDXGIResource> resource;
    ComPtr<ID3D11Resource> d3d11res;
    picture_sys_d3d11_t *pic_sys;
    D3D11_BOX copyBox;

    if( unlikely(d3d11_block == nullptr) )
        return nullptr;

    d3d11_decoder_device_t *d3d_dev = GetD3D11OpaqueContext(p_data->vctx);

    static const struct vlc_frame_callbacks cbs = {
        CaptureBlockRelease,
    };
    block_Init( &d3d11_block->self, &cbs, nullptr, 1 );

    d3d11_block->d3d11_pic = D3D11_AllocPicture(VLC_OBJECT(p_demux),
                                                &p_sys->fmt.video, p_data->vctx,
                                                true, p_data->output_format);
    if ( d3d11_block->d3d11_pic == nullptr )
    {
        msg_Err(p_demux, "Failed to allocate the output texture");
        goto error;
    }

    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    HRESULT hr;
    hr = p_data->duplication->AcquireNextFrame(1000, &frameInfo, &resource);
    if (FAILED(hr))
    {
        msg_Err(p_demux, "Failed to capture a frame. (hr=0x%lX)", hr);
        goto error;
    }

#if defined(SCREEN_SUBSCREEN) && !defined(VLC_WINSTORE_APP)
    if( p_sys->b_follow_mouse )
    {
        POINT pos;
        GetCursorPos( &pos );
        pos.x -= p_data->screen_x;
        pos.y -= p_data->screen_y;
        FollowMouse( p_sys, pos.x, pos.y );
    }
#endif // SCREEN_SUBSCREEN && !VLC_WINSTORE_APP

    /* copy the texture into the block texture */
    hr = resource.As(&d3d11res);
    if (unlikely(FAILED(hr)))
    {
        msg_Err(p_demux, "Failed to get the texture. (hr=0x%lX)", hr);
        goto error;
    }
    pic_sys = ActiveD3D11PictureSys(d3d11_block->d3d11_pic);
#ifdef SCREEN_SUBSCREEN
    copyBox.left   = p_sys->i_left;
    copyBox.right  = copyBox.left + p_sys->i_width;
    copyBox.top    = p_sys->i_top;
    copyBox.bottom = copyBox.top + p_sys->i_height;
#else // !SCREEN_SUBSCREEN
    copyBox.left   = 0;
    copyBox.right  = p_sys->fmt.video.i_width;
    copyBox.top    = 0;
    copyBox.bottom = p_sys->fmt.video.i_height;
#endif // !SCREEN_SUBSCREEN
    copyBox.front = 0;
    copyBox.back = 1;
    d3d11_device_lock( &d3d_dev->d3d_dev );
    d3d_dev->d3d_dev.d3dcontext->CopySubresourceRegion(
                                              pic_sys->resource[KNOWN_DXGI_INDEX], 0,
                                              0, 0, 0,
                                              d3d11res.Get(), 0,
                                              &copyBox);
    p_data->duplication->ReleaseFrame();

    // TODO mouse blending

    d3d11_device_unlock( &d3d_dev->d3d_dev );

    return &d3d11_block->self;
error:
    if (d3d11_block->d3d11_pic)
        picture_Release(d3d11_block->d3d11_pic);
    delete d3d11_block;
    return nullptr;
}

static void screen_CloseCapture(screen_data_t *p_data)
{
    delete p_data;
}

int screen_InitCaptureDXGI(demux_t *p_demux)
{
    demux_sys_t *p_sys = static_cast<demux_sys_t*>(p_demux->p_sys);
    screen_data_t *p_data;
    vlc_decoder_device *dec_dev;
    HRESULT hr;

    if (var_CreateGetInteger( p_demux, "screen-fragment-size" ) != 0)
    {
        msg_Dbg(p_demux, "screen-fragment-size not supported in DXGI");
        return VLC_ENOTSUP;
    }

    char *mousefile = var_InheritString( p_demux, "screen-mouse-image" );
    free( mousefile );
    if (mousefile)
    {
        msg_Dbg(p_demux, "screen-mouse-image not supported in DXGI");
        return VLC_ENOTSUP;
    }

#ifdef VLC_WINSTORE_APP
    if (p_sys->b_follow_mouse)
    {
        msg_Dbg(p_demux, "screen-follow-mouse not supported in UWP DXGI");
        return VLC_ENOTSUP;
    }
#endif // VLC_WINSTORE_APP

    p_data = new (std::nothrow) screen_data_t();
    if (unlikely(p_data == nullptr))
        return VLC_ENOMEM;

#ifndef VLC_WINSTORE_APP
    p_data->screen_x = GetSystemMetrics( SM_XVIRTUALSCREEN );
    p_data->screen_y = GetSystemMetrics( SM_YVIRTUALSCREEN );
#endif

    dec_dev = vlc_decoder_device_Create(VLC_OBJECT(p_demux), nullptr);
    d3d11_decoder_device_t *d3d11_dev = GetD3D11OpaqueDevice(dec_dev);
    if (d3d11_dev == nullptr)
    {
        msg_Err(p_demux, "incompatible decoder device.");
    }
    else
    {
        ComPtr<ID3D11Device1> d3d11VLC1;
        hr = d3d11_dev->d3d_dev.d3ddevice->QueryInterface(IID_GRAPHICS_PPV_ARGS(&d3d11VLC1));
        if (FAILED(hr)) {
            msg_Warn(p_demux, "D3D11 device cannot share textures. (hr=0x%lX)", hr);
            goto error;
        }

        ComPtr<IDXGIDevice> pDXGIDevice;
        HRESULT hr = d3d11_dev->d3d_dev.d3ddevice->QueryInterface(IID_GRAPHICS_PPV_ARGS(&pDXGIDevice));
        if (FAILED(hr)) {
            goto error;
        }

        ComPtr<IDXGIAdapter> adapter;
        hr = pDXGIDevice->GetAdapter(&adapter);
        if (FAILED(hr)) {
            goto error;
        }

        ComPtr<IDXGIOutput1> output1;
        for (UINT j=0; output1.Get()==nullptr; j++)
        {
            ComPtr<IDXGIOutput> output;
            hr = adapter->EnumOutputs(j, &output);
            if (FAILED(hr))
                break;

            hr = output.As(&output1);
            if (FAILED(hr))
                continue;

            hr = output1->DuplicateOutput(d3d11_dev->d3d_dev.d3ddevice, &p_data->duplication);
            if (FAILED(hr))
                output1.Reset();
        }
    }

    if (!p_data->duplication)
        goto error;

    DXGI_OUTDUPL_DESC outDesc;
    p_data->duplication->GetDesc(&outDesc);

    p_data->output_format = D3D11_RenderFormat(outDesc.ModeDesc.Format ,true);
    if (unlikely(!p_data->output_format->name))
    {
        msg_Err(p_demux, "Unknown texture format %d", outDesc.ModeDesc.Format);
        goto error;
    }

    p_data->vctx = D3D11CreateVideoContext(dec_dev, p_data->output_format->formatTexture);
    vlc_decoder_device_Release(dec_dev);
    dec_dev = nullptr;
    if (unlikely(p_data->vctx == nullptr))
        goto error;

    es_format_Init( &p_sys->fmt, VIDEO_ES, p_data->output_format->fourcc );
    p_sys->fmt.video.i_visible_width    =
    p_sys->fmt.video.i_width            = outDesc.ModeDesc.Width;
    p_sys->fmt.video.i_visible_height   =
    p_sys->fmt.video.i_height           = outDesc.ModeDesc.Height;
    p_sys->fmt.video.i_bits_per_pixel   = 4 * p_data->output_format->bitsPerChannel; /* FIXME */
    p_sys->fmt.video.i_sar_num = p_sys->fmt.video.i_sar_den = 1;
    p_sys->fmt.video.i_chroma           = p_sys->fmt.i_codec;
    p_sys->fmt.video.color_range        = COLOR_RANGE_FULL;
    p_sys->fmt.video.i_frame_rate       = outDesc.ModeDesc.RefreshRate.Numerator;
    p_sys->fmt.video.i_frame_rate_base  = outDesc.ModeDesc.RefreshRate.Denominator;
    DxgiFormatMask( outDesc.ModeDesc.Format, &p_sys->fmt.video );

    p_sys->p_data = p_data;
    static const screen_capture_operations ops = {
        screen_Capture, screen_CloseCapture,
    };
    p_sys->ops = &ops;

    return VLC_SUCCESS;
error:
    if (dec_dev)
        vlc_decoder_device_Release(dec_dev);
    delete p_data;
    return VLC_EGENERIC;
}

int screen_InitCapture(demux_t *p_demux)
{
    int ret = screen_InitCaptureDXGI(p_demux);
    if (ret == VLC_SUCCESS)
        return VLC_SUCCESS;
#if defined(_WIN32) && !defined(VLC_WINSTORE_APP)
    return screen_InitCaptureGDI(p_demux);
#else
    return ret;
#endif
}
