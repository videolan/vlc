/*****************************************************************************
 * d3d11_surface.c : D3D11 GPU surface conversion module for vlc
 *****************************************************************************
 * Copyright © 2015 VLC authors, VideoLAN and VideoLabs
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>

#include "copy.h"

static int  OpenConverter( vlc_object_t * );
static void CloseConverter( vlc_object_t * );

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Conversions from D3D11 to YUV") )
    set_capability( "video converter", 10 )
    set_callbacks( OpenConverter, CloseConverter )
vlc_module_end ()

#include <windows.h>
#define COBJMACROS
#include <d3d11.h>
#include "d3d11_fmt.h"

#ifdef ID3D11VideoContext_VideoProcessorBlt
#define CAN_PROCESSOR 1
#else
#define CAN_PROCESSOR 0
#endif

struct filter_sys_t {
    copy_cache_t     cache;
    union {
        ID3D11Texture2D  *staging;
        ID3D11Resource   *staging_resource;
    };
    vlc_mutex_t      staging_lock;

#if CAN_PROCESSOR
    union {
        ID3D11Texture2D  *procOutTexture;
        ID3D11Resource   *procOutResource;
    };
    /* 420_OPAQUE processor */
    ID3D11VideoDevice              *d3dviddev;
    ID3D11VideoContext             *d3dvidctx;
    ID3D11VideoProcessorOutputView *processorOutput;
    ID3D11VideoProcessorEnumerator *procEnumerator;
    ID3D11VideoProcessor           *videoProcessor;
#endif
};

#if CAN_PROCESSOR
static int SetupProcessor(filter_t *p_filter, ID3D11Device *d3ddevice,
                          ID3D11DeviceContext *d3dctx,
                          DXGI_FORMAT srcFormat, DXGI_FORMAT dstFormat)
{
    filter_sys_t *sys = (filter_sys_t*) p_filter->p_sys;
    HRESULT hr;
    ID3D11VideoProcessorEnumerator *processorEnumerator = NULL;

    hr = ID3D11DeviceContext_QueryInterface(d3dctx, &IID_ID3D11VideoContext, (void **)&sys->d3dvidctx);
    if (unlikely(FAILED(hr)))
        goto error;

    hr = ID3D11Device_QueryInterface( d3ddevice, &IID_ID3D11VideoDevice, (void **)&sys->d3dviddev);
    if (unlikely(FAILED(hr)))
        goto error;

    const video_format_t *fmt = &p_filter->fmt_in.video;
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC processorDesc = {
        .InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE,
        .InputFrameRate = {
            .Numerator   = fmt->i_frame_rate_base > 0 ? fmt->i_frame_rate : 0,
            .Denominator = fmt->i_frame_rate_base,
        },
        .InputWidth   = fmt->i_width,
        .InputHeight  = fmt->i_height,
        .OutputWidth  = fmt->i_width,
        .OutputHeight = fmt->i_height,
        .Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL,
    };
    hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(sys->d3dviddev, &processorDesc, &processorEnumerator);
    if ( processorEnumerator == NULL )
    {
        msg_Dbg(p_filter, "Can't get a video processor for the video.");
        goto error;
    }

    UINT flags;
#ifndef NDEBUG
    for (int format = 0; format < 188; format++) {
        hr = ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(processorEnumerator, format, &flags);
        if (SUCCEEDED(hr) && (flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT))
            msg_Dbg(p_filter, "processor format %s (%d) is supported for input", DxgiFormatToStr(format),format);
        if (SUCCEEDED(hr) && (flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT))
            msg_Dbg(p_filter, "processor format %s (%d) is supported for output", DxgiFormatToStr(format),format);
    }
#endif
    /* shortcut for the rendering output */
    hr = ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(processorEnumerator, srcFormat, &flags);
    if (FAILED(hr) || !(flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT))
    {
        msg_Dbg(p_filter, "processor format %s not supported for output", DxgiFormatToStr(srcFormat));
        goto error;
    }
    hr = ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(processorEnumerator, dstFormat, &flags);
    if (FAILED(hr) || !(flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT))
    {
        msg_Dbg(p_filter, "processor format %s not supported for input", DxgiFormatToStr(dstFormat));
        goto error;
    }

    D3D11_VIDEO_PROCESSOR_CAPS processorCaps;
    hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorCaps(processorEnumerator, &processorCaps);
    for (UINT type = 0; type < processorCaps.RateConversionCapsCount; ++type)
    {
        hr = ID3D11VideoDevice_CreateVideoProcessor(sys->d3dviddev,
                                                    processorEnumerator, type, &sys->videoProcessor);
        if (SUCCEEDED(hr))
        {
            D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outDesc = {
                .ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D,
            };

            hr = ID3D11VideoDevice_CreateVideoProcessorOutputView(sys->d3dviddev,
                                                             sys->procOutResource,
                                                             processorEnumerator,
                                                             &outDesc,
                                                             &sys->processorOutput);
            if (FAILED(hr))
                msg_Err(p_filter, "Failed to create the processor output. (hr=0x%lX)", hr);
            else
            {
                sys->procEnumerator  = processorEnumerator;
                return VLC_SUCCESS;
            }
        }
        if (sys->videoProcessor)
        {
            ID3D11VideoProcessor_Release(sys->videoProcessor);
            sys->videoProcessor = NULL;
        }
    }

error:
    if (processorEnumerator)
        ID3D11VideoProcessorEnumerator_Release(processorEnumerator);
    if (sys->d3dvidctx)
        ID3D11VideoContext_Release(sys->d3dvidctx);
    if (sys->d3dviddev)
        ID3D11VideoDevice_Release(sys->d3dviddev);
    return VLC_EGENERIC;
}
#endif

static int assert_staging(filter_t *p_filter, picture_sys_t *p_sys)
{
    filter_sys_t *sys = (filter_sys_t*) p_filter->p_sys;
    HRESULT hr;

    if (sys->staging)
        goto ok;

    D3D11_TEXTURE2D_DESC texDesc;
    ID3D11Texture2D_GetDesc( p_sys->texture[KNOWN_DXGI_INDEX], &texDesc);

    texDesc.MipLevels = 1;
    //texDesc.SampleDesc.Count = 1;
    texDesc.MiscFlags = 0;
    texDesc.ArraySize = 1;
    texDesc.Usage = D3D11_USAGE_STAGING;
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    texDesc.BindFlags = 0;

    ID3D11Device *p_device;
    ID3D11DeviceContext_GetDevice(p_sys->context, &p_device);
    sys->staging = NULL;
    hr = ID3D11Device_CreateTexture2D( p_device, &texDesc, NULL, &sys->staging);
#if CAN_PROCESSOR
    if (FAILED(hr)) {
        /* failed with the this format, try a different one */
        UINT supportFlags = D3D11_FORMAT_SUPPORT_SHADER_LOAD | D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_OUTPUT;
        const d3d_format_t *new_fmt =
                FindD3D11Format( p_device, 0, 0, false, supportFlags );
        if (new_fmt && texDesc.Format != new_fmt->formatTexture)
        {
            DXGI_FORMAT srcFormat = texDesc.Format;
            texDesc.Format = new_fmt->formatTexture;
            hr = ID3D11Device_CreateTexture2D( p_device, &texDesc, NULL, &sys->staging);
            if (SUCCEEDED(hr))
            {
                texDesc.Usage = D3D11_USAGE_DEFAULT;
                texDesc.CPUAccessFlags = 0;
                texDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
                hr = ID3D11Device_CreateTexture2D( p_device, &texDesc, NULL, &sys->procOutTexture);
                if (SUCCEEDED(hr))
                {
                    if (SetupProcessor(p_filter, p_device, p_sys->context, srcFormat, new_fmt->formatTexture))
                    {
                        ID3D11Texture2D_Release(sys->procOutTexture);
                        ID3D11Texture2D_Release(sys->staging);
                        sys->staging = NULL;
                        hr = E_FAIL;
                    }
                }
                else
                {
                    ID3D11Texture2D_Release(sys->staging);
                    sys->staging = NULL;
                    hr = E_FAIL;
                }
            }
        }
    }
#endif
    ID3D11Device_Release(p_device);
    if (FAILED(hr)) {
        msg_Err(p_filter, "Failed to create a %s staging texture to extract surface pixels (hr=0x%0lx)", DxgiFormatToStr(texDesc.Format), hr );
        return VLC_EGENERIC;
    }
ok:
    return VLC_SUCCESS;
}

static void D3D11_YUY2(filter_t *p_filter, picture_t *src, picture_t *dst)
{
    filter_sys_t *sys = (filter_sys_t*) p_filter->p_sys;
    picture_sys_t *p_sys = &((struct va_pic_context*)src->context)->picsys;

    D3D11_TEXTURE2D_DESC desc;
    D3D11_MAPPED_SUBRESOURCE lock;

    vlc_mutex_lock(&sys->staging_lock);
    if (assert_staging(p_filter, p_sys) != VLC_SUCCESS)
    {
        vlc_mutex_unlock(&sys->staging_lock);
        return;
    }

    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC viewDesc;
    ID3D11VideoDecoderOutputView_GetDesc( p_sys->decoder, &viewDesc );

    ID3D11Resource *srcResource = p_sys->resource[KNOWN_DXGI_INDEX];
    UINT srcSlice = viewDesc.Texture2D.ArraySlice;

#if CAN_PROCESSOR
    if (sys->procEnumerator)
    {
        HRESULT hr;
        if (!p_sys->processorInput)
        {
            D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inDesc = {
                .FourCC = 0,
                .ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D,
                .Texture2D.MipSlice = 0,
                .Texture2D.ArraySlice = viewDesc.Texture2D.ArraySlice,
            };

            hr = ID3D11VideoDevice_CreateVideoProcessorInputView(sys->d3dviddev,
                                                                 p_sys->resource[KNOWN_DXGI_INDEX],
                                                                 sys->procEnumerator,
                                                                 &inDesc,
                                                                 &p_sys->processorInput);
            if (FAILED(hr))
            {
#ifndef NDEBUG
                msg_Dbg(p_filter,"Failed to create processor input for slice %d. (hr=0x%lX)", p_sys->slice_index, hr);
#endif
                return;
            }
        }
        D3D11_VIDEO_PROCESSOR_STREAM stream = {
            .Enable = TRUE,
            .pInputSurface = p_sys->processorInput,
        };

        hr = ID3D11VideoContext_VideoProcessorBlt(sys->d3dvidctx, sys->videoProcessor,
                                                          sys->processorOutput,
                                                          0, 1, &stream);
        if (FAILED(hr))
        {
            msg_Err(p_filter, "Failed to process the video. (hr=0x%lX)", hr);
            vlc_mutex_unlock(&sys->staging_lock);
            return;
        }

        srcResource = sys->procOutResource;
        srcSlice = 0;
    }
#endif
    ID3D11DeviceContext_CopySubresourceRegion(p_sys->context, sys->staging_resource,
                                              0, 0, 0, 0,
                                              srcResource,
                                              srcSlice,
                                              NULL);

    HRESULT hr = ID3D11DeviceContext_Map(p_sys->context, sys->staging_resource,
                                         0, D3D11_MAP_READ, 0, &lock);
    if (FAILED(hr)) {
        msg_Err(p_filter, "Failed to map source surface. (hr=0x%0lx)", hr);
        vlc_mutex_unlock(&sys->staging_lock);
        return;
    }

    if (dst->format.i_chroma == VLC_CODEC_I420) {
        uint8_t *tmp = dst->p[1].p_pixels;
        dst->p[1].p_pixels = dst->p[2].p_pixels;
        dst->p[2].p_pixels = tmp;
    }

    ID3D11Texture2D_GetDesc(sys->staging, &desc);

    if (desc.Format == DXGI_FORMAT_YUY2) {
        size_t chroma_pitch = (lock.RowPitch / 2);

        size_t pitch[3] = {
            lock.RowPitch,
            chroma_pitch,
            chroma_pitch,
        };

        uint8_t *plane[3] = {
            (uint8_t*)lock.pData,
            (uint8_t*)lock.pData + pitch[0] * desc.Height,
            (uint8_t*)lock.pData + pitch[0] * desc.Height
                                 + pitch[1] * desc.Height / 2,
        };

        CopyFromYv12ToYv12(dst, plane, pitch,
                           src->format.i_visible_height + src->format.i_y_offset, &sys->cache);
    } else if (desc.Format == DXGI_FORMAT_NV12) {
        uint8_t *plane[2] = {
            lock.pData,
            (uint8_t*)lock.pData + lock.RowPitch * desc.Height
        };
        size_t  pitch[2] = {
            lock.RowPitch,
            lock.RowPitch,
        };
        CopyFromNv12ToYv12(dst, plane, pitch,
                           src->format.i_visible_height + src->format.i_y_offset, &sys->cache);
    } else {
        msg_Err(p_filter, "Unsupported D3D11VA conversion from 0x%08X to YV12", desc.Format);
    }

    if (dst->format.i_chroma == VLC_CODEC_I420) {
        uint8_t *tmp = dst->p[1].p_pixels;
        dst->p[1].p_pixels = dst->p[2].p_pixels;
        dst->p[2].p_pixels = tmp;
    }

    /* */
    ID3D11DeviceContext_Unmap(p_sys->context, sys->staging_resource, 0);
    vlc_mutex_unlock(&sys->staging_lock);
}

static void D3D11_NV12(filter_t *p_filter, picture_t *src, picture_t *dst)
{
    filter_sys_t *sys = (filter_sys_t*) p_filter->p_sys;
    picture_sys_t *p_sys = &((struct va_pic_context*)src->context)->picsys;

    D3D11_TEXTURE2D_DESC desc;
    D3D11_MAPPED_SUBRESOURCE lock;

    vlc_mutex_lock(&sys->staging_lock);
    if (assert_staging(p_filter, p_sys) != VLC_SUCCESS)
    {
        vlc_mutex_unlock(&sys->staging_lock);
        return;
    }

    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC viewDesc;
    ID3D11VideoDecoderOutputView_GetDesc( p_sys->decoder, &viewDesc );

    ID3D11Resource *srcResource = p_sys->resource[KNOWN_DXGI_INDEX];
    UINT srcSlice = viewDesc.Texture2D.ArraySlice;

#if CAN_PROCESSOR
    if (sys->procEnumerator)
    {
        HRESULT hr;
        if (!p_sys->processorInput)
        {
            D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inDesc = {
                .FourCC = 0,
                .ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D,
                .Texture2D.MipSlice = 0,
                .Texture2D.ArraySlice = viewDesc.Texture2D.ArraySlice,
            };

            hr = ID3D11VideoDevice_CreateVideoProcessorInputView(sys->d3dviddev,
                                                                 p_sys->resource[KNOWN_DXGI_INDEX],
                                                                 sys->procEnumerator,
                                                                 &inDesc,
                                                                 &p_sys->processorInput);
            if (FAILED(hr))
            {
#ifndef NDEBUG
                msg_Dbg(p_filter,"Failed to create processor input for slice %d. (hr=0x%lX)", p_sys->slice_index, hr);
#endif
                return;
            }
        }
        D3D11_VIDEO_PROCESSOR_STREAM stream = {
            .Enable = TRUE,
            .pInputSurface = p_sys->processorInput,
        };

        hr = ID3D11VideoContext_VideoProcessorBlt(sys->d3dvidctx, sys->videoProcessor,
                                                          sys->processorOutput,
                                                          0, 1, &stream);
        if (FAILED(hr))
        {
            msg_Err(p_filter, "Failed to process the video. (hr=0x%lX)", hr);
            vlc_mutex_unlock(&sys->staging_lock);
            return;
        }

        srcResource = sys->procOutResource;
        srcSlice = 0;
    }
#endif
    ID3D11DeviceContext_CopySubresourceRegion(p_sys->context, sys->staging_resource,
                                              0, 0, 0, 0,
                                              srcResource,
                                              srcSlice,
                                              NULL);

    HRESULT hr = ID3D11DeviceContext_Map(p_sys->context, sys->staging_resource,
                                         0, D3D11_MAP_READ, 0, &lock);
    if (FAILED(hr)) {
        msg_Err(p_filter, "Failed to map source surface. (hr=0x%0lx)", hr);
        vlc_mutex_unlock(&sys->staging_lock);
        return;
    }

    ID3D11Texture2D_GetDesc(sys->staging, &desc);

    if (desc.Format == DXGI_FORMAT_NV12) {
        uint8_t *plane[2] = {
            lock.pData,
            (uint8_t*)lock.pData + lock.RowPitch * desc.Height
        };
        size_t  pitch[2] = {
            lock.RowPitch,
            lock.RowPitch,
        };
        CopyFromNv12ToNv12(dst, plane, pitch,
                           src->format.i_visible_height + src->format.i_y_offset, &sys->cache);
    } else {
        msg_Err(p_filter, "Unsupported D3D11VA conversion from 0x%08X to NV12", desc.Format);
    }

    /* */
    ID3D11DeviceContext_Unmap(p_sys->context, sys->staging_resource, 0);
    vlc_mutex_unlock(&sys->staging_lock);
}

VIDEO_FILTER_WRAPPER (D3D11_NV12)
VIDEO_FILTER_WRAPPER (D3D11_YUY2)

static int OpenConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;
    if ( p_filter->fmt_in.video.i_chroma != VLC_CODEC_D3D11_OPAQUE )
        return VLC_EGENERIC;

    if ( p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height
         || p_filter->fmt_in.video.i_width != p_filter->fmt_out.video.i_width )
        return VLC_EGENERIC;

    switch( p_filter->fmt_out.video.i_chroma ) {
    case VLC_CODEC_I420:
    case VLC_CODEC_YV12:
        p_filter->pf_video_filter = D3D11_YUY2_Filter;
        break;
    case VLC_CODEC_NV12:
        p_filter->pf_video_filter = D3D11_NV12_Filter;
        break;
    default:
        return VLC_EGENERIC;
    }

    filter_sys_t *p_sys = calloc(1, sizeof(filter_sys_t));
    if (!p_sys)
         return VLC_ENOMEM;
    CopyInitCache(&p_sys->cache, p_filter->fmt_in.video.i_width );
    vlc_mutex_init(&p_sys->staging_lock);
    p_filter->p_sys = p_sys;

    return VLC_SUCCESS;
}

static void CloseConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;
    filter_sys_t *p_sys = (filter_sys_t*) p_filter->p_sys;
#if CAN_PROCESSOR
    if (p_sys->d3dviddev)
        ID3D11VideoDevice_Release(p_sys->d3dviddev);
    if (p_sys->d3dvidctx)
        ID3D11VideoContext_Release(p_sys->d3dvidctx);
    if (p_sys->procEnumerator)
        ID3D11VideoProcessorEnumerator_Release(p_sys->procEnumerator);
    if (p_sys->videoProcessor)
        ID3D11VideoProcessor_Release(p_sys->videoProcessor);
#endif
    CopyCleanCache(&p_sys->cache);
    vlc_mutex_destroy(&p_sys->staging_lock);
    if (p_sys->staging)
        ID3D11Texture2D_Release(p_sys->staging);
    free( p_sys );
    p_filter->p_sys = NULL;
}
