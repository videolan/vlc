/*****************************************************************************
 * d3d11_processor.c: D3D11 VideoProcessor helper
 *****************************************************************************
 * Copyright © 2017 VLC authors, VideoLAN and VideoLabs
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

#include <vlc_filter.h>
#include <vlc_picture.h>

#include <assert.h>

#define COBJMACROS
#include <initguid.h>
#include <d3d11.h>

#include "d3d11_processor.h"

#if defined(ID3D11VideoContext_VideoProcessorBlt)
#ifndef NDEBUG
static void LogProcessorSupport(vlc_object_t *o,
                                ID3D11VideoProcessorEnumerator *processorEnumerator)
{
    UINT flags;
    HRESULT hr;
    for (int format = 0; format < 188; format++) {
        hr = ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(processorEnumerator, format, &flags);
        if (FAILED(hr))
            continue;
        const char *name = DxgiFormatToStr(format);
        const char *support = NULL;
        if ((flags & (D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT|D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT))
                 == (D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT|D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT))
            support = "input/output";
        else if (flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT)
            support = "input";
        else if (flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT)
            support = "output";
        if (support)
        {
            if (name)
                msg_Dbg(o, "processor format %s is supported for %s", name, support);
            else
                msg_Dbg(o, "processor format (%d) is supported for %s", format, support);
        }
    }
}
#endif

#undef D3D11_CreateProcessor
int D3D11_CreateProcessor(vlc_object_t *o, d3d11_device_t *d3d_dev,
                          D3D11_VIDEO_FRAME_FORMAT srcFields,
                          const video_format_t *fmt_in, const video_format_t *fmt_out,
                          d3d11_processor_t *out)
{
    HRESULT hr;
    *out = (d3d11_processor_t) { 0 };

    hr = ID3D11Device_QueryInterface(d3d_dev->d3ddevice, &IID_ID3D11VideoDevice, (void **)&out->d3dviddev);
    if (FAILED(hr)) {
       msg_Err(o, "Could not Query ID3D11VideoDevice Interface. (hr=0x%lX)", hr);
       goto error;
    }

    hr = ID3D11DeviceContext_QueryInterface(d3d_dev->d3dcontext, &IID_ID3D11VideoContext, (void **)&out->d3dvidctx);
    if (FAILED(hr)) {
       msg_Err(o, "Could not Query ID3D11VideoContext Interface. (hr=0x%lX)", hr);
       goto error;
    }

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC processorDesc = {
        .InputFrameFormat = srcFields,
        .InputFrameRate = {
            .Numerator   = fmt_in->i_frame_rate_base ? fmt_in->i_frame_rate : 0,
            .Denominator = fmt_in->i_frame_rate_base,
        },
        .InputWidth   = fmt_in->i_width,
        .InputHeight  = fmt_in->i_height,
        .OutputWidth  = fmt_out->i_width,
        .OutputHeight = fmt_out->i_height,
        .OutputFrameRate = {
            .Numerator   = fmt_out->i_frame_rate_base ? fmt_out->i_frame_rate : 0,
            .Denominator = fmt_out->i_frame_rate_base,
        },
        .Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL,
    };
    hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(out->d3dviddev, &processorDesc, &out->procEnumerator);
    if ( FAILED(hr) || out->procEnumerator == NULL )
    {
        msg_Dbg(o, "Can't get a video processor for the video.");
        goto error;
    }

#ifndef NDEBUG
    LogProcessorSupport(o, out->procEnumerator);
#endif

    return VLC_SUCCESS;
error:
    D3D11_ReleaseProcessor(out);
    return VLC_ENOMOD;
}

void D3D11_ReleaseProcessor(d3d11_processor_t *out)
{
    if (out->videoProcessor)
    {
        ID3D11VideoProcessor_Release(out->videoProcessor);
        out->videoProcessor = NULL;
    }
    if (out->procEnumerator)
    {
        ID3D11VideoProcessorEnumerator_Release(out->procEnumerator);
        out->procEnumerator = NULL;
    }
    if (out->d3dviddev)
    {
        ID3D11VideoDevice_Release(out->d3dviddev);
        out->d3dviddev = NULL;
    }
    if (out->d3dvidctx)
    {
        ID3D11VideoContext_Release(out->d3dvidctx);
        out->d3dvidctx = NULL;
    }
}

#undef D3D11_Assert_ProcessorInput
HRESULT D3D11_Assert_ProcessorInput(vlc_object_t *o, d3d11_processor_t *d3d_proc, picture_sys_d3d11_t *p_sys)
{
    if (p_sys->processorInput)
        return S_OK;

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inDesc = {
        .FourCC = 0,
        .ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D,
        .Texture2D.MipSlice = 0,
        .Texture2D.ArraySlice = p_sys->slice_index,
    };
    HRESULT hr;

    hr = ID3D11VideoDevice_CreateVideoProcessorInputView(d3d_proc->d3dviddev,
                                                         p_sys->resource[KNOWN_DXGI_INDEX],
                                                         d3d_proc->procEnumerator,
                                                         &inDesc,
                                                         &p_sys->processorInput);
#ifndef NDEBUG
    if (FAILED(hr))
        msg_Dbg(o,"Failed to create processor input for slice %d. (hr=0x%lX)", p_sys->slice_index, hr);
#endif
    return hr;
}
#endif
