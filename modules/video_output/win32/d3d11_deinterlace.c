/*****************************************************************************
 * d3d11_deinterlace.c: D3D11 deinterlacing filter
 *****************************************************************************
 * Copyright (C) 2017 Videolabs SAS
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

#include <stdlib.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>

#define COBJMACROS
#include <initguid.h>
#include <d3d11.h>

#include "../../video_chroma/d3d11_fmt.h"

#ifdef __MINGW32__
#define D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BOB  2
#endif

struct filter_sys_t
{
    ID3D11VideoDevice              *d3dviddev;
    ID3D11VideoContext             *d3dvidctx;
    ID3D11VideoProcessor           *videoProcessor;
    ID3D11VideoProcessorEnumerator *procEnumerator;

    HANDLE                         context_mutex;
    union {
        ID3D11Texture2D            *outTexture;
        ID3D11Resource             *outResource;
    };
    ID3D11VideoProcessorOutputView *processorOutput;
};

static picture_t *Deinterlace(filter_t *filter, picture_t *src)
{
    filter_sys_t *sys = filter->p_sys;
    HRESULT hr;

    struct va_pic_context *pic_ctx = (struct va_pic_context*)src->context;
    picture_sys_t *p_sys = pic_ctx ? &pic_ctx->picsys : src->p_sys;
    if (!p_sys->processorInput)
    {
        D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inDesc = {
            .FourCC = 0,
            .ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D,
            .Texture2D.MipSlice = 0,
            .Texture2D.ArraySlice = p_sys->slice_index,
        };

        hr = ID3D11VideoDevice_CreateVideoProcessorInputView(sys->d3dviddev,
                                                             p_sys->resource[KNOWN_DXGI_INDEX],
                                                             sys->procEnumerator,
                                                             &inDesc,
                                                             &p_sys->processorInput);
        if (FAILED(hr))
        {
#ifndef NDEBUG
            msg_Dbg(filter,"Failed to create processor input for slice %d. (hr=0x%lX)", p_sys->slice_index, hr);
#endif
            return src;
        }
    }

    picture_t *dst = filter_NewPicture(filter);
    if (dst == NULL)
        return src; /* cannot deinterlace without copying fields */

    D3D11_VIDEO_PROCESSOR_STREAM stream = {
        .Enable = TRUE,
        .pInputSurface = p_sys->processorInput,
    };

    D3D11_VIDEO_FRAME_FORMAT frameFormat = src->b_top_field_first ?
                D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST :
                D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST;

    if( sys->context_mutex != INVALID_HANDLE_VALUE )
        WaitForSingleObjectEx( sys->context_mutex, INFINITE, FALSE );

    ID3D11VideoContext_VideoProcessorSetStreamFrameFormat(sys->d3dvidctx, sys->videoProcessor, 0, frameFormat);

    hr = ID3D11VideoContext_VideoProcessorBlt(sys->d3dvidctx, sys->videoProcessor,
                                              sys->processorOutput,
                                              0, 1, &stream);
    if (FAILED(hr))
    {
        if( sys->context_mutex  != INVALID_HANDLE_VALUE )
            ReleaseMutex( sys->context_mutex );
        goto error;
    }

    ID3D11DeviceContext_CopySubresourceRegion(dst->p_sys->context,
                                              dst->p_sys->resource[KNOWN_DXGI_INDEX],
                                              dst->p_sys->slice_index,
                                              0, 0, 0,
                                              sys->outResource,
                                              0, NULL);
    if( sys->context_mutex  != INVALID_HANDLE_VALUE )
        ReleaseMutex( sys->context_mutex );

    picture_CopyProperties(dst, src);
    picture_Release(src);
    dst->b_progressive = true;
    dst->i_nb_fields = 1;
    return dst;
error:
    picture_Release(dst);
    return src;
}

static int Open(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    HRESULT hr;
    ID3D11Device *d3ddevice = NULL;
    ID3D11VideoProcessorEnumerator *processorEnumerator = NULL;

    if (filter->fmt_in.video.i_chroma != VLC_CODEC_D3D11_OPAQUE
     && filter->fmt_in.video.i_chroma != VLC_CODEC_D3D11_OPAQUE_10B)
        return VLC_EGENERIC;
    if (!video_format_IsSimilar(&filter->fmt_in.video, &filter->fmt_out.video))
        return VLC_EGENERIC;

    picture_t *dst = filter_NewPicture(filter);
    if (dst == NULL)
        return VLC_EGENERIC;
    if (!dst->p_sys)
    {
        msg_Dbg(filter, "D3D11 opaque without a texture");
        return VLC_EGENERIC;
    }

    D3D11_TEXTURE2D_DESC dstDesc;
    ID3D11Texture2D_GetDesc(dst->p_sys->texture[KNOWN_DXGI_INDEX], &dstDesc);

    filter_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        goto error;
    memset(sys, 0, sizeof (*sys));

    ID3D11DeviceContext_GetDevice(dst->p_sys->context, &d3ddevice);

    hr = ID3D11Device_QueryInterface(d3ddevice, &IID_ID3D11VideoDevice, (void **)&sys->d3dviddev);
    if (FAILED(hr)) {
       msg_Err(filter, "Could not Query ID3D11VideoDevice Interface. (hr=0x%lX)", hr);
       goto error;
    }

    hr = ID3D11DeviceContext_QueryInterface(dst->p_sys->context, &IID_ID3D11VideoContext, (void **)&sys->d3dvidctx);
    if (FAILED(hr)) {
       msg_Err(filter, "Could not Query ID3D11VideoContext Interface from the picture. (hr=0x%lX)", hr);
       goto error;
    }

    HANDLE context_lock = INVALID_HANDLE_VALUE;
    UINT dataSize = sizeof(context_lock);
    hr = ID3D11Device_GetPrivateData(d3ddevice, &GUID_CONTEXT_MUTEX, &dataSize, &context_lock);
    if (FAILED(hr))
        msg_Warn(filter, "No mutex found to lock the decoder");
    sys->context_mutex = context_lock;

    const video_format_t *fmt = &dst->format;

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC processorDesc = {
        .InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST,
        .InputFrameRate = {
            .Numerator   = fmt->i_frame_rate,
            .Denominator = fmt->i_frame_rate_base,
        },
        .InputWidth   = fmt->i_width,
        .InputHeight  = fmt->i_height,
        .OutputWidth  = dst->format.i_width,
        .OutputHeight = dst->format.i_height,
        .OutputFrameRate = {
            .Numerator   = dst->format.i_frame_rate,
            .Denominator = dst->format.i_frame_rate_base,
        },
        .Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL,
    };
    hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(sys->d3dviddev, &processorDesc, &processorEnumerator);
    if ( processorEnumerator == NULL )
    {
        msg_Dbg(filter, "Can't get a video processor for the video.");
        goto error;
    }

    UINT flags;
#ifndef NDEBUG
    for (int format = 0; format < 188; format++) {
        hr = ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(processorEnumerator, format, &flags);
        if (SUCCEEDED(hr) && (flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT))
            msg_Dbg(filter, "processor format %s (%d) is supported for input", DxgiFormatToStr(format),format);
        if (SUCCEEDED(hr) && (flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT))
            msg_Dbg(filter, "processor format %s (%d) is supported for output", DxgiFormatToStr(format),format);
    }
#endif
    hr = ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(processorEnumerator, dst->p_sys->formatTexture, &flags);
    if (!SUCCEEDED(hr))
    {
        msg_Dbg(filter, "can't read processor support for %s", DxgiFormatToStr(dst->p_sys->formatTexture));
        goto error;
    }
    if ( !(flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT) ||
         !(flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT) )
    {
        msg_Dbg(filter, "deinterlacing %s is not supported", DxgiFormatToStr(dst->p_sys->formatTexture));
        goto error;
    }

    D3D11_VIDEO_PROCESSOR_CAPS processorCaps;
    hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorCaps(processorEnumerator, &processorCaps);
    if (FAILED(hr))
        goto error;

    for (UINT type = 0; type < processorCaps.RateConversionCapsCount; ++type)
    {
        D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS rateCaps;
        ID3D11VideoProcessorEnumerator_GetVideoProcessorRateConversionCaps(processorEnumerator, type, &rateCaps);
        if (!(rateCaps.ProcessorCaps & D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BOB))
            continue;

        hr = ID3D11VideoDevice_CreateVideoProcessor(sys->d3dviddev,
                                                    processorEnumerator, type, &sys->videoProcessor);
        if (SUCCEEDED(hr))
            break;
        sys->videoProcessor = NULL;
    }

    if (sys->videoProcessor == NULL)
    {
        msg_Dbg(filter, "couldn't find a deinterlacing filter");
        goto error;
    }

    D3D11_TEXTURE2D_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(texDesc));
    texDesc.MipLevels = 1;
    texDesc.SampleDesc.Count = 1;
    texDesc.MiscFlags = 0; //D3D11_RESOURCE_MISC_SHARED;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.CPUAccessFlags = 0;
    texDesc.Format = dstDesc.Format;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.CPUAccessFlags = 0;
    texDesc.ArraySize = 1;
    texDesc.Height = dstDesc.Height;
    texDesc.Width = dstDesc.Width;

    hr = ID3D11Device_CreateTexture2D( d3ddevice, &texDesc, NULL, &sys->outTexture );
    if (FAILED(hr)) {
        msg_Err(filter, "CreateTexture2D failed. (hr=0x%0lx)", hr);
        goto error;
    }

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outDesc = {
        .ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D,
        .Texture2D.MipSlice = 0,
    };

    hr = ID3D11VideoDevice_CreateVideoProcessorOutputView(sys->d3dviddev,
                                                         sys->outResource,
                                                         processorEnumerator,
                                                         &outDesc,
                                                         &sys->processorOutput);
    if (FAILED(hr))
    {
        msg_Dbg(filter,"Failed to create processor output. (hr=0x%lX)", hr);
        goto error;
    }

    sys->procEnumerator  = processorEnumerator;

    filter->pf_video_filter = Deinterlace;
    filter->p_sys = sys;

    ID3D11Device_Release(d3ddevice);
    picture_Release(dst);
    return VLC_SUCCESS;
error:
    if (d3ddevice)
        ID3D11Device_Release(d3ddevice);
    picture_Release(dst);

    if (sys->outTexture)
        ID3D11Texture2D_Release(sys->outTexture);
    if (sys->videoProcessor)
        ID3D11VideoProcessor_Release(sys->videoProcessor);
    if (processorEnumerator)
        ID3D11VideoProcessorEnumerator_Release(processorEnumerator);
    if (sys->d3dvidctx)
        ID3D11VideoContext_Release(sys->d3dvidctx);
    if (sys->d3dviddev)
        ID3D11VideoDevice_Release(sys->d3dviddev);

    return VLC_EGENERIC;
}

static void Close(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    filter_sys_t *sys = filter->p_sys;

    ID3D11VideoProcessorInputView_Release(sys->processorOutput);
    ID3D11Texture2D_Release(sys->outTexture);
    ID3D11VideoProcessor_Release(sys->videoProcessor);
    ID3D11VideoProcessorEnumerator_Release(sys->procEnumerator);
    ID3D11VideoContext_Release(sys->d3dvidctx);
    ID3D11VideoDevice_Release(sys->d3dviddev);

    free(sys);
}

vlc_module_begin()
    set_description(N_("Direct3D11 deinterlacing filter"))
    set_capability("video filter", 0)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_callbacks(Open, Close)
    add_shortcut ("deinterlace")
vlc_module_end()
