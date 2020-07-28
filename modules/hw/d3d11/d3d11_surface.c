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
#include <vlc_filter.h>
#include <vlc_picture.h>
#include <vlc_modules.h>

#include <assert.h>

#include "../../video_chroma/copy.h"

#include <windows.h>
#define COBJMACROS
#include <d3d11.h>

#include "d3d11_filters.h"
#include "d3d11_processor.h"
#include "../../video_chroma/d3d11_fmt.h"

#ifdef ID3D11VideoContext_VideoProcessorBlt
#define CAN_PROCESSOR 1
#else
#define CAN_PROCESSOR 0
#endif

typedef struct
{
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
    ID3D11VideoProcessorOutputView *processorOutput;
    d3d11_processor_t              d3d_proc;
#endif
    d3d11_device_t                 *d3d_dev;

    /* CPU to GPU */
    filter_t   *filter;
    picture_t  *staging_pic;
} filter_sys_t;

#if CAN_PROCESSOR
static int SetupProcessor(filter_t *p_filter, d3d11_device_t *d3d_dev,
                          DXGI_FORMAT srcFormat, DXGI_FORMAT dstFormat)
{
    filter_sys_t *sys = p_filter->p_sys;
    HRESULT hr;

    if (D3D11_CreateProcessor(p_filter, d3d_dev, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE,
                              &p_filter->fmt_in.video, &p_filter->fmt_out.video, &sys->d3d_proc) != VLC_SUCCESS)
        goto error;

    UINT flags;
    /* shortcut for the rendering output */
    hr = ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(sys->d3d_proc.procEnumerator, srcFormat, &flags);
    if (FAILED(hr) || !(flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT))
    {
        msg_Dbg(p_filter, "processor format %s not supported for output", DxgiFormatToStr(srcFormat));
        goto error;
    }
    hr = ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(sys->d3d_proc.procEnumerator, dstFormat, &flags);
    if (FAILED(hr) || !(flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT))
    {
        msg_Dbg(p_filter, "processor format %s not supported for input", DxgiFormatToStr(dstFormat));
        goto error;
    }

    D3D11_VIDEO_PROCESSOR_CAPS processorCaps;
    hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorCaps(sys->d3d_proc.procEnumerator, &processorCaps);
    for (UINT type = 0; type < processorCaps.RateConversionCapsCount; ++type)
    {
        hr = ID3D11VideoDevice_CreateVideoProcessor(sys->d3d_proc.d3dviddev,
                                                    sys->d3d_proc.procEnumerator, type, &sys->d3d_proc.videoProcessor);
        if (SUCCEEDED(hr))
        {
            D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outDesc = {
                .ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D,
            };

            hr = ID3D11VideoDevice_CreateVideoProcessorOutputView(sys->d3d_proc.d3dviddev,
                                                             sys->procOutResource,
                                                             sys->d3d_proc.procEnumerator,
                                                             &outDesc,
                                                             &sys->processorOutput);
            if (FAILED(hr))
                msg_Err(p_filter, "Failed to create the processor output. (hr=0x%lX)", hr);
            else
            {
                return VLC_SUCCESS;
            }
        }
        if (sys->d3d_proc.videoProcessor)
        {
            ID3D11VideoProcessor_Release(sys->d3d_proc.videoProcessor);
            sys->d3d_proc.videoProcessor = NULL;
        }
    }

error:
    D3D11_ReleaseProcessor(&sys->d3d_proc);
    return VLC_EGENERIC;
}
#endif

static HRESULT can_map(filter_sys_t *sys, ID3D11DeviceContext *context)
{
    D3D11_MAPPED_SUBRESOURCE lock;
    HRESULT hr = ID3D11DeviceContext_Map(context, sys->staging_resource, 0,
                                         D3D11_MAP_READ, 0, &lock);
    ID3D11DeviceContext_Unmap(context, sys->staging_resource, 0);
    return hr;
}

static int assert_staging(filter_t *p_filter, filter_sys_t *sys, DXGI_FORMAT format)
{
    HRESULT hr;

    if (sys->staging)
        goto ok;

    D3D11_TEXTURE2D_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(texDesc));
    texDesc.Width  = p_filter->fmt_in.video.i_width;
    texDesc.Height = p_filter->fmt_in.video.i_height;
    texDesc.Format = format;
    texDesc.MipLevels = 1;
    texDesc.SampleDesc.Count = 1;
    texDesc.MiscFlags = 0;
    texDesc.ArraySize = 1;
    texDesc.Usage = D3D11_USAGE_STAGING;
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    texDesc.BindFlags = 0;

    d3d11_device_t *d3d_dev = sys->d3d_dev;
    sys->staging = NULL;
    hr = ID3D11Device_CreateTexture2D( d3d_dev->d3ddevice, &texDesc, NULL, &sys->staging);
    /* test if mapping the texture works ref #18746 */
    if (SUCCEEDED(hr) && FAILED(hr = can_map(sys, d3d_dev->d3dcontext)))
        msg_Dbg(p_filter, "can't map default staging texture (hr=0x%lX)", hr);
#if CAN_PROCESSOR
    if (FAILED(hr)) {
        /* failed with the this format, try a different one */
        UINT supportFlags = D3D11_FORMAT_SUPPORT_SHADER_LOAD | D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_OUTPUT;
        const d3d_format_t *new_fmt =
                FindD3D11Format( p_filter, d3d_dev, 0, D3D11_RGB_FORMAT|D3D11_YUV_FORMAT, 0, 0, 0, D3D11_CHROMA_CPU, supportFlags );
        if (new_fmt && texDesc.Format != new_fmt->formatTexture)
        {
            DXGI_FORMAT srcFormat = texDesc.Format;
            texDesc.Format = new_fmt->formatTexture;
            hr = ID3D11Device_CreateTexture2D( d3d_dev->d3ddevice, &texDesc, NULL, &sys->staging);
            if (SUCCEEDED(hr))
            {
                texDesc.Usage = D3D11_USAGE_DEFAULT;
                texDesc.CPUAccessFlags = 0;
                texDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
                hr = ID3D11Device_CreateTexture2D( d3d_dev->d3ddevice, &texDesc, NULL, &sys->procOutTexture);
                if (SUCCEEDED(hr) && SUCCEEDED(hr = can_map(sys, d3d_dev->d3dcontext)))
                {
                    if (SetupProcessor(p_filter, d3d_dev, srcFormat, new_fmt->formatTexture))
                    {
                        ID3D11Texture2D_Release(sys->procOutTexture);
                        ID3D11Texture2D_Release(sys->staging);
                        sys->staging = NULL;
                        hr = E_FAIL;
                    }
                    else
                        msg_Dbg(p_filter, "Using shader+processor format %s", new_fmt->name);
                }
                else
                {
                    msg_Dbg(p_filter, "can't create intermediate texture (hr=0x%lX)", hr);
                    ID3D11Texture2D_Release(sys->staging);
                    sys->staging = NULL;
                }
            }
        }
    }
#endif
    if (FAILED(hr)) {
        msg_Err(p_filter, "Failed to create a %s staging texture to extract surface pixels (hr=0x%lX)", DxgiFormatToStr(texDesc.Format), hr );
        return VLC_EGENERIC;
    }
ok:
    return VLC_SUCCESS;
}

static void D3D11_YUY2(filter_t *p_filter, picture_t *src, picture_t *dst)
{
    if (src->context == NULL)
    {
        /* the previous stages creating a D3D11 picture should always fill the context */
        msg_Err(p_filter, "missing source context");
        return;
    }

    filter_sys_t *sys = p_filter->p_sys;
    picture_sys_d3d11_t *p_sys = ActiveD3D11PictureSys(src);

    D3D11_TEXTURE2D_DESC desc;
    D3D11_MAPPED_SUBRESOURCE lock;

    vlc_mutex_lock(&sys->staging_lock);
    UINT srcSlice = p_sys->slice_index;
    ID3D11Resource *srcResource = p_sys->resource[KNOWN_DXGI_INDEX];

#if CAN_PROCESSOR
    if (sys->d3d_proc.procEnumerator)
    {
        HRESULT hr;
        if (FAILED( D3D11_Assert_ProcessorInput(p_filter, &sys->d3d_proc, p_sys) ))
            return;

        D3D11_VIDEO_PROCESSOR_STREAM stream = {
            .Enable = TRUE,
            .pInputSurface = p_sys->processorInput,
        };

        hr = ID3D11VideoContext_VideoProcessorBlt(sys->d3d_proc.d3dvidctx, sys->d3d_proc.videoProcessor,
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
    ID3D11DeviceContext_CopySubresourceRegion(sys->d3d_dev->d3dcontext, sys->staging_resource,
                                              0, 0, 0, 0,
                                              srcResource,
                                              srcSlice,
                                              NULL);

    HRESULT hr = ID3D11DeviceContext_Map(sys->d3d_dev->d3dcontext, sys->staging_resource,
                                         0, D3D11_MAP_READ, 0, &lock);
    if (FAILED(hr)) {
        msg_Err(p_filter, "Failed to map source surface. (hr=0x%lX)", hr);
        vlc_mutex_unlock(&sys->staging_lock);
        return;
    }

    if (dst->format.i_chroma == VLC_CODEC_I420)
        picture_SwapUV( dst );

    ID3D11Texture2D_GetDesc(sys->staging, &desc);

    if (desc.Format == DXGI_FORMAT_YUY2) {
        size_t chroma_pitch = (lock.RowPitch / 2);

        const size_t pitch[3] = {
            lock.RowPitch,
            chroma_pitch,
            chroma_pitch,
        };

        const uint8_t *plane[3] = {
            (uint8_t*)lock.pData,
            (uint8_t*)lock.pData + pitch[0] * desc.Height,
            (uint8_t*)lock.pData + pitch[0] * desc.Height
                                 + pitch[1] * desc.Height / 2,
        };

        Copy420_P_to_P(dst, plane, pitch,
                       src->format.i_visible_height + src->format.i_y_offset,
                       &sys->cache);
    } else if (desc.Format == DXGI_FORMAT_NV12 ||
               desc.Format == DXGI_FORMAT_P010) {
        const uint8_t *plane[2] = {
            lock.pData,
            (uint8_t*)lock.pData + lock.RowPitch * desc.Height
        };
        const size_t  pitch[2] = {
            lock.RowPitch,
            lock.RowPitch,
        };
        if (desc.Format == DXGI_FORMAT_NV12)
            Copy420_SP_to_P(dst, plane, pitch,
                            __MIN(desc.Height, src->format.i_y_offset + src->format.i_visible_height),
                            &sys->cache);
        else
            Copy420_16_SP_to_P(dst, plane, pitch,
                               __MIN(desc.Height, src->format.i_y_offset + src->format.i_visible_height),
                               6, &sys->cache);
        picture_SwapUV(dst);
    } else {
        msg_Err(p_filter, "Unsupported D3D11VA conversion from 0x%08X to YV12", desc.Format);
    }

    if (dst->format.i_chroma == VLC_CODEC_I420 || dst->format.i_chroma == VLC_CODEC_I420_10L)
        picture_SwapUV( dst );

    /* */
    ID3D11DeviceContext_Unmap(sys->d3d_dev->d3dcontext, sys->staging_resource, 0);
    vlc_mutex_unlock(&sys->staging_lock);
}

static void D3D11_NV12(filter_t *p_filter, picture_t *src, picture_t *dst)
{
    if (src->context == NULL)
    {
        /* the previous stages creating a D3D11 picture should always fill the context */
        msg_Err(p_filter, "missing source context");
        return;
    }

    filter_sys_t *sys = p_filter->p_sys;
    picture_sys_d3d11_t *p_sys = ActiveD3D11PictureSys(src);

    D3D11_TEXTURE2D_DESC desc;
    D3D11_MAPPED_SUBRESOURCE lock;

    vlc_mutex_lock(&sys->staging_lock);
    UINT srcSlice = p_sys->slice_index;
    ID3D11Resource *srcResource = p_sys->resource[KNOWN_DXGI_INDEX];

#if CAN_PROCESSOR
    if (sys->d3d_proc.procEnumerator)
    {
        HRESULT hr;
        if (FAILED( D3D11_Assert_ProcessorInput(p_filter, &sys->d3d_proc, p_sys) ))
            return;

        D3D11_VIDEO_PROCESSOR_STREAM stream = {
            .Enable = TRUE,
            .pInputSurface = p_sys->processorInput,
        };

        hr = ID3D11VideoContext_VideoProcessorBlt(sys->d3d_proc.d3dvidctx, sys->d3d_proc.videoProcessor,
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
    ID3D11DeviceContext_CopySubresourceRegion(sys->d3d_dev->d3dcontext, sys->staging_resource,
                                              0, 0, 0, 0,
                                              srcResource,
                                              srcSlice,
                                              NULL);

    HRESULT hr = ID3D11DeviceContext_Map(sys->d3d_dev->d3dcontext, sys->staging_resource,
                                         0, D3D11_MAP_READ, 0, &lock);
    if (FAILED(hr)) {
        msg_Err(p_filter, "Failed to map source surface. (hr=0x%lX)", hr);
        vlc_mutex_unlock(&sys->staging_lock);
        return;
    }

    ID3D11Texture2D_GetDesc(sys->staging, &desc);

    if (desc.Format == DXGI_FORMAT_NV12 || desc.Format == DXGI_FORMAT_P010) {
        const uint8_t *plane[2] = {
            lock.pData,
            (uint8_t*)lock.pData + lock.RowPitch * desc.Height
        };
        size_t  pitch[2] = {
            lock.RowPitch,
            lock.RowPitch,
        };
        Copy420_SP_to_SP(dst, plane, pitch,
                         __MIN(desc.Height, src->format.i_y_offset + src->format.i_visible_height),
                         &sys->cache);
    } else {
        msg_Err(p_filter, "Unsupported D3D11VA conversion from 0x%08X to NV12", desc.Format);
    }

    /* */
    ID3D11DeviceContext_Unmap(sys->d3d_dev->d3dcontext, sys->staging_resource, 0);
    vlc_mutex_unlock(&sys->staging_lock);
}

static void D3D11_RGBA(filter_t *p_filter, picture_t *src, picture_t *dst)
{
    filter_sys_t *sys = p_filter->p_sys;
    assert(src->context != NULL);
    picture_sys_d3d11_t *p_sys = ActiveD3D11PictureSys(src);

    D3D11_TEXTURE2D_DESC desc;
    D3D11_MAPPED_SUBRESOURCE lock;

    vlc_mutex_lock(&sys->staging_lock);
    ID3D11DeviceContext_CopySubresourceRegion(sys->d3d_dev->d3dcontext, sys->staging_resource,
                                              0, 0, 0, 0,
                                              p_sys->resource[KNOWN_DXGI_INDEX],
                                              p_sys->slice_index,
                                              NULL);

    HRESULT hr = ID3D11DeviceContext_Map(sys->d3d_dev->d3dcontext, sys->staging_resource,
                                         0, D3D11_MAP_READ, 0, &lock);
    if (FAILED(hr)) {
        msg_Err(p_filter, "Failed to map source surface. (hr=0x%lX)", hr);
        vlc_mutex_unlock(&sys->staging_lock);
        return;
    }

    ID3D11Texture2D_GetDesc(sys->staging, &desc);

    plane_t src_planes  = dst->p[0];
    src_planes.i_lines  = desc.Height;
    src_planes.i_pitch  = lock.RowPitch;
    src_planes.p_pixels = lock.pData;
    plane_CopyPixels( dst->p, &src_planes );

    /* */
    ID3D11DeviceContext_Unmap(sys->d3d_dev->d3dcontext,
                              p_sys->resource[KNOWN_DXGI_INDEX], p_sys->slice_index);
    vlc_mutex_unlock(&sys->staging_lock);
}

static void DeleteFilter( filter_t * p_filter )
{
    if( p_filter->p_module )
        module_unneed( p_filter, p_filter->p_module );

    es_format_Clean( &p_filter->fmt_in );
    es_format_Clean( &p_filter->fmt_out );

    vlc_object_delete(p_filter);
}

static picture_t *NewBuffer(filter_t *p_filter)
{
    filter_t *p_parent = p_filter->owner.sys;
    filter_sys_t *p_sys = p_parent->p_sys;
    return p_sys->staging_pic;
}

static vlc_decoder_device * HoldD3D11DecoderDevice(vlc_object_t *o, void *sys)
{
    VLC_UNUSED(o);
    filter_t *p_this = sys;
    return filter_HoldDecoderDevice(p_this);
}

static filter_t *CreateCPUtoGPUFilter( filter_t *p_this, const es_format_t *p_fmt_in,
                               vlc_fourcc_t dst_chroma )
{
    filter_t *p_filter;

    p_filter = vlc_object_create( p_this, sizeof(filter_t) );
    if (unlikely(p_filter == NULL))
        return NULL;

    static const struct filter_video_callbacks cbs = { NewBuffer, HoldD3D11DecoderDevice };
    p_filter->b_allow_fmt_out_change = false;
    p_filter->owner.video = &cbs;
    p_filter->owner.sys = p_this;

    es_format_InitFromVideo( &p_filter->fmt_in,  &p_fmt_in->video );
    es_format_InitFromVideo( &p_filter->fmt_out, &p_fmt_in->video );
    p_filter->fmt_out.i_codec = p_filter->fmt_out.video.i_chroma = dst_chroma;
    p_filter->p_module = module_need( p_filter, "video converter", NULL, false );

    if( !p_filter->p_module )
    {
        msg_Dbg( p_filter, "no video converter found" );
        DeleteFilter( p_filter );
        return NULL;
    }

    return p_filter;
}

static void NV12_D3D11(filter_t *p_filter, picture_t *src, picture_t *dst)
{
    filter_sys_t *sys = p_filter->p_sys;
    picture_sys_d3d11_t *p_sys = ActiveD3D11PictureSys(dst);
    if (unlikely(p_sys==NULL))
    {
        /* the output filter configuration may have changed since the filter
         * was opened */
        return;
    }

    d3d11_device_lock( sys->d3d_dev );
    if (sys->filter == NULL)
    {
        D3D11_MAPPED_SUBRESOURCE lock;
        HRESULT hr = ID3D11DeviceContext_Map(sys->d3d_dev->d3dcontext, p_sys->resource[KNOWN_DXGI_INDEX],
                                            0, D3D11_MAP_WRITE_DISCARD, 0, &lock);
        if (FAILED(hr)) {
            msg_Err(p_filter, "Failed to map source surface. (hr=0x%lX)", hr);
            d3d11_device_unlock( sys->d3d_dev );
            return;
        }

        picture_UpdatePlanes(dst, lock.pData, lock.RowPitch);
        picture_context_t *dst_pic_ctx = dst->context;
        dst->context = NULL; // some CPU filters won't like the mix of CPU/GPU

        picture_CopyPixels(dst, src);

        dst->context = dst_pic_ctx;
        ID3D11DeviceContext_Unmap(sys->d3d_dev->d3dcontext, p_sys->resource[KNOWN_DXGI_INDEX], 0);
    }
    else
    {
        picture_sys_d3d11_t *p_staging_sys = p_sys;

        D3D11_TEXTURE2D_DESC texDesc;
        ID3D11Texture2D_GetDesc( p_staging_sys->texture[KNOWN_DXGI_INDEX], &texDesc);

        D3D11_MAPPED_SUBRESOURCE lock;
        HRESULT hr = ID3D11DeviceContext_Map(sys->d3d_dev->d3dcontext, p_staging_sys->resource[KNOWN_DXGI_INDEX],
                                            0, D3D11_MAP_WRITE_DISCARD, 0, &lock);
        if (FAILED(hr)) {
            msg_Err(p_filter, "Failed to map source surface. (hr=0x%lX)", hr);
            d3d11_device_unlock( sys->d3d_dev );
            return;
        }

        picture_UpdatePlanes(sys->staging_pic, lock.pData, lock.RowPitch);
        picture_context_t *staging_pic_ctx = sys->staging_pic->context;
        sys->staging_pic->context = NULL; // some CPU filters won't like the mix of CPU/GPU

        picture_Hold( src );
        sys->filter->pf_video_filter(sys->filter, src);

        sys->staging_pic->context = staging_pic_ctx;
        ID3D11DeviceContext_Unmap(sys->d3d_dev->d3dcontext, p_staging_sys->resource[KNOWN_DXGI_INDEX], 0);

        D3D11_BOX copyBox = {
            .right = dst->format.i_width, .bottom = dst->format.i_height, .back = 1,
        };
        ID3D11DeviceContext_CopySubresourceRegion(sys->d3d_dev->d3dcontext,
                                                p_sys->resource[KNOWN_DXGI_INDEX],
                                                p_sys->slice_index,
                                                0, 0, 0,
                                                p_staging_sys->resource[KNOWN_DXGI_INDEX], 0,
                                                &copyBox);
    }
    d3d11_device_unlock( sys->d3d_dev );
    // stop pretending this is a CPU picture
    dst->format.i_chroma = p_filter->fmt_out.video.i_chroma;
    dst->i_planes = 0;
}

VIDEO_FILTER_WRAPPER (D3D11_NV12)
VIDEO_FILTER_WRAPPER (D3D11_YUY2)
VIDEO_FILTER_WRAPPER (D3D11_RGBA)

static picture_t *AllocateCPUtoGPUTexture(filter_t *p_filter, filter_sys_t *p_sys)
{
    video_format_t fmt_staging;

    d3d11_video_context_t *vctx_sys = GetD3D11ContextPrivate( p_filter->vctx_out );

    const d3d_format_t *cfg = NULL;
    for (const d3d_format_t *output_format = GetRenderFormatList();
            output_format->name != NULL; ++output_format)
    {
        if (output_format->formatTexture == vctx_sys->format &&
            !is_d3d11_opaque(output_format->fourcc))
        {
            cfg = output_format;
            break;
        }
    }
    if (unlikely(cfg == NULL))
        return NULL;

    struct d3d11_pic_context *pic_ctx = calloc(1, sizeof(*pic_ctx));
    if (unlikely(pic_ctx == NULL))
        goto done;

    video_format_Copy(&fmt_staging, &p_filter->fmt_out.video);
    fmt_staging.i_chroma = cfg->fourcc;

    picture_resource_t dummy_res = {};
    picture_t *p_dst = picture_NewFromResource(&fmt_staging, &dummy_res);
    if (p_dst == NULL) {
        msg_Err(p_filter, "Failed to map create the temporary picture.");
        goto done;
    }

    if (AllocateTextures(p_filter, p_sys->d3d_dev, cfg,
                         &p_dst->format, pic_ctx->picsys.texture, p_dst->p) != VLC_SUCCESS)
        goto done;

    if (unlikely(D3D11_AllocateResourceView(p_filter, p_sys->d3d_dev->d3ddevice, cfg,
                                            pic_ctx->picsys.texture, 0, pic_ctx->picsys.renderSrc) != VLC_SUCCESS))
        goto done;

    pic_ctx->s = (picture_context_t) {
        d3d11_pic_context_destroy, d3d11_pic_context_copy,
        vlc_video_context_Hold(p_filter->vctx_out),
    };
    AcquireD3D11PictureSys(&pic_ctx->picsys);
    ID3D11Texture2D_Release(pic_ctx->picsys.texture[KNOWN_DXGI_INDEX]);

    p_dst->context = &pic_ctx->s;

    return p_dst;
done:
    free(pic_ctx);
    return NULL;
}

static picture_t *NV12_D3D11_Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic = AllocateCPUtoGPUTexture( p_filter, p_filter->p_sys );
    if( p_outpic )
    {
        NV12_D3D11( p_filter, p_pic, p_outpic );
        picture_CopyProperties( p_outpic, p_pic );
    }
    picture_Release( p_pic );
    return p_outpic;
}

int D3D11OpenConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;

    if ( !is_d3d11_opaque(p_filter->fmt_in.video.i_chroma) )
        return VLC_EGENERIC;
    if ( GetD3D11ContextPrivate(p_filter->vctx_in) == NULL )
        return VLC_EGENERIC;

    if ( p_filter->fmt_in.video.i_visible_height != p_filter->fmt_out.video.i_visible_height
         || p_filter->fmt_in.video.i_width != p_filter->fmt_out.video.i_width )
        return VLC_EGENERIC;

    uint8_t pixel_bytes = 1;
    switch( p_filter->fmt_out.video.i_chroma ) {
    case VLC_CODEC_I420:
    case VLC_CODEC_YV12:
        if( p_filter->fmt_in.video.i_chroma != VLC_CODEC_D3D11_OPAQUE )
            return VLC_EGENERIC;
        p_filter->pf_video_filter = D3D11_YUY2_Filter;
        break;
    case VLC_CODEC_I420_10L:
        if( p_filter->fmt_in.video.i_chroma != VLC_CODEC_D3D11_OPAQUE_10B )
            return VLC_EGENERIC;
        p_filter->pf_video_filter = D3D11_YUY2_Filter;
        pixel_bytes = 2;
        break;
    case VLC_CODEC_NV12:
        if( p_filter->fmt_in.video.i_chroma != VLC_CODEC_D3D11_OPAQUE )
            return VLC_EGENERIC;
        p_filter->pf_video_filter = D3D11_NV12_Filter;
        break;
    case VLC_CODEC_P010:
        if( p_filter->fmt_in.video.i_chroma != VLC_CODEC_D3D11_OPAQUE_10B )
            return VLC_EGENERIC;
        p_filter->pf_video_filter = D3D11_NV12_Filter;
        pixel_bytes = 2;
        break;
    case VLC_CODEC_RGBA:
        if( p_filter->fmt_in.video.i_chroma != VLC_CODEC_D3D11_OPAQUE_RGBA )
            return VLC_EGENERIC;
        p_filter->pf_video_filter = D3D11_RGBA_Filter;
        break;
    case VLC_CODEC_BGRA:
        if( p_filter->fmt_in.video.i_chroma != VLC_CODEC_D3D11_OPAQUE_BGRA )
            return VLC_EGENERIC;
        p_filter->pf_video_filter = D3D11_RGBA_Filter;
        break;
    default:
        return VLC_EGENERIC;
    }

    filter_sys_t *p_sys = vlc_obj_calloc(obj, 1, sizeof(filter_sys_t));
    if (!p_sys)
        return VLC_ENOMEM;

    d3d11_video_context_t *vctx_sys = GetD3D11ContextPrivate(p_filter->vctx_in);
    d3d11_decoder_device_t *dev_sys = GetD3D11OpaqueContext(p_filter->vctx_in);
    p_sys->d3d_dev = &dev_sys->d3d_dev;

    if (assert_staging(p_filter, p_sys, vctx_sys->format) != VLC_SUCCESS)
    {
        return VLC_EGENERIC;
    }

    if (CopyInitCache(&p_sys->cache, p_filter->fmt_in.video.i_width * pixel_bytes))
        return VLC_ENOMEM;

    vlc_mutex_init(&p_sys->staging_lock);
    p_filter->p_sys = p_sys;
    return VLC_SUCCESS;
}

int D3D11OpenCPUConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;
    int err = VLC_EGENERIC;
    filter_sys_t *p_sys = NULL;

    if ( p_filter->fmt_out.video.i_chroma != VLC_CODEC_D3D11_OPAQUE
     &&  p_filter->fmt_out.video.i_chroma != VLC_CODEC_D3D11_OPAQUE_10B )
        return VLC_EGENERIC;

    if ( p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height
         || p_filter->fmt_in.video.i_width != p_filter->fmt_out.video.i_width )
        return VLC_EGENERIC;

    switch( p_filter->fmt_in.video.i_chroma ) {
    case VLC_CODEC_I420:
    case VLC_CODEC_I420_10L:
    case VLC_CODEC_YV12:
    case VLC_CODEC_NV12:
    case VLC_CODEC_P010:
        p_filter->pf_video_filter = NV12_D3D11_Filter;
        break;
    default:
        return VLC_EGENERIC;
    }

    vlc_decoder_device *dec_device = filter_HoldDecoderDeviceType( p_filter, VLC_DECODER_DEVICE_D3D11VA );
    if (dec_device == NULL)
    {
        msg_Err(p_filter, "Missing decoder device");
        return VLC_EGENERIC;
    }
    d3d11_decoder_device_t *devsys = GetD3D11OpaqueDevice(dec_device);
    if (unlikely(devsys == NULL))
    {
        msg_Err(p_filter, "Incompatible decoder device %d", dec_device->type);
        vlc_decoder_device_Release(dec_device);
        return VLC_EGENERIC;
    }

    p_sys = vlc_obj_calloc(obj, 1, sizeof(filter_sys_t));
    if (!p_sys) {
        vlc_decoder_device_Release(dec_device);
        return VLC_ENOMEM;
    }
    p_sys->d3d_dev = &devsys->d3d_dev;

    DXGI_FORMAT vctx_fmt;
    switch( p_filter->fmt_in.video.i_chroma ) {
    case VLC_CODEC_I420:
    case VLC_CODEC_YV12:
    case VLC_CODEC_NV12:
        vctx_fmt = DXGI_FORMAT_NV12;
        break;
    case VLC_CODEC_I420_10L:
    case VLC_CODEC_P010:
        vctx_fmt = DXGI_FORMAT_P010;
        break;
    default:
        vlc_assert_unreachable();
    }
    p_filter->vctx_out = D3D11CreateVideoContext(dec_device, vctx_fmt);
    if ( p_filter->vctx_out == NULL )
    {
        msg_Dbg(p_filter, "no video context");
        goto done;
    }

    vlc_fourcc_t d3d_fourcc = DxgiFormatFourcc(vctx_fmt);

    if ( p_filter->fmt_in.video.i_chroma != d3d_fourcc )
    {
        p_sys->staging_pic = AllocateCPUtoGPUTexture(p_filter, p_sys);
        if (p_sys->staging_pic == NULL)
            goto done;

        p_sys->filter = CreateCPUtoGPUFilter(p_filter, &p_filter->fmt_in, d3d_fourcc);
        if (!p_sys->filter)
        {
            picture_Release(p_sys->staging_pic);
            goto done;
        }
    }

    p_filter->p_sys = p_sys;
    err = VLC_SUCCESS;

done:
    if (err != VLC_SUCCESS)
    {
        vlc_video_context_Release(p_filter->vctx_out);
    }
    return err;
}

void D3D11CloseConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;
    filter_sys_t *p_sys = p_filter->p_sys;
#if CAN_PROCESSOR
    if (p_sys->procOutTexture)
        ID3D11Texture2D_Release(p_sys->procOutTexture);
    D3D11_ReleaseProcessor( &p_sys->d3d_proc );
#endif
    CopyCleanCache(&p_sys->cache);
    if (p_sys->staging)
        ID3D11Texture2D_Release(p_sys->staging);
}

void D3D11CloseCPUConverter( vlc_object_t *obj )
{
    filter_t *p_filter = (filter_t *)obj;
    filter_sys_t *p_sys = p_filter->p_sys;
    if (p_sys->filter)
        DeleteFilter(p_sys->filter);
    if (p_sys->staging_pic)
        picture_Release(p_sys->staging_pic);
    vlc_video_context_Release(p_filter->vctx_out);
}
