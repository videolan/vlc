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
#include <vlc_filter.h>
#include <vlc_picture.h>

#define COBJMACROS
#include <d3d11.h>

#include "d3d11_filters.h"
#include "d3d11_processor.h"
#include "../../video_chroma/d3d11_fmt.h"
#include "../../video_filter/deinterlace/common.h"

typedef struct
{
    d3d11_device_t                 *d3d_dev;
    d3d11_processor_t              d3d_proc;

    union {
        ID3D11Texture2D            *outTexture;
        ID3D11Resource             *outResource;
    };
    ID3D11VideoProcessorOutputView *processorOutput;

    struct deinterlace_ctx         context;
} filter_sys_t;

struct filter_mode_t
{
    const char                           *psz_mode;
    D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS  i_mode;
    deinterlace_algo                      settings;
};
static struct filter_mode_t filter_mode [] = {
    { "blend",   D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BLEND,
                 { false, false, false, false } },
    { "bob",     D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BOB,
                 { true,  false, false, false } },
    { "x",       D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_MOTION_COMPENSATION,
                 { true,  true,  false, false } },
    { "ivtc",    D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_INVERSE_TELECINE,
                 { false, true,  true, false } },
    { "yadif2x", D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_ADAPTIVE,
                 { true,  true,  false, false } },
};

static void Flush(filter_t *filter)
{
    filter_sys_t *p_sys = filter->p_sys;
    FlushDeinterlacing(&p_sys->context);
}

static int RenderPic( filter_t *p_filter, picture_t *p_outpic, picture_t *p_pic,
                      int order, int i_field )
{
    VLC_UNUSED(order);
    HRESULT hr;
    filter_sys_t *p_sys = p_filter->p_sys;
    picture_sys_d3d11_t *p_out_sys = ActiveD3D11PictureSys(p_outpic);

    picture_t *p_prev = p_sys->context.pp_history[0];
    picture_t *p_cur  = p_sys->context.pp_history[1];
    picture_t *p_next = p_sys->context.pp_history[2];

    /* TODO adjust the format if it's the first or second field ? */
    D3D11_VIDEO_FRAME_FORMAT frameFormat = !i_field ?
                D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST :
                D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST;

    ID3D11VideoContext_VideoProcessorSetStreamFrameFormat(p_sys->d3d_proc.d3dvidctx, p_sys->d3d_proc.videoProcessor, 0, frameFormat);
    ID3D11VideoContext_VideoProcessorSetStreamAutoProcessingMode(p_sys->d3d_proc.d3dvidctx, p_sys->d3d_proc.videoProcessor, 0, FALSE);

    D3D11_VIDEO_PROCESSOR_STREAM stream = {0};
    stream.Enable = TRUE;
    stream.InputFrameOrField = i_field ? 1 : 0;

    if( p_cur && p_next )
    {
        picture_sys_d3d11_t *picsys_next = ActiveD3D11PictureSys(p_next);
        if ( unlikely(!picsys_next) || FAILED(D3D11_Assert_ProcessorInput(p_filter, &p_sys->d3d_proc, picsys_next) ))
            return VLC_EGENERIC;

        picture_sys_d3d11_t *picsys_cur = ActiveD3D11PictureSys(p_cur);
        if ( unlikely(!picsys_cur) || FAILED( D3D11_Assert_ProcessorInput(p_filter, &p_sys->d3d_proc, picsys_cur) ))
            return VLC_EGENERIC;

        if ( p_prev )
        {
            picture_sys_d3d11_t *picsys_prev = ActiveD3D11PictureSys(p_prev);
            if ( unlikely(!picsys_prev) || FAILED( D3D11_Assert_ProcessorInput(p_filter, &p_sys->d3d_proc, picsys_prev) ))
                return VLC_EGENERIC;

            stream.pInputSurface    = picsys_cur->processorInput;
            stream.ppFutureSurfaces = &picsys_next->processorInput;
            stream.ppPastSurfaces   = &picsys_prev->processorInput;

            stream.PastFrames   = 1;
            stream.FutureFrames = 1;
        }
        else
        {
            /* p_next is the current, p_cur is the previous frame */
            stream.pInputSurface  = picsys_next->processorInput;
            stream.ppPastSurfaces = &picsys_cur->processorInput;
            stream.PastFrames = 1;
        }
    }
    else
    {
        picture_sys_d3d11_t *p_sys_src = ActiveD3D11PictureSys(p_pic);
        if ( unlikely(!p_sys_src) || FAILED( D3D11_Assert_ProcessorInput(p_filter, &p_sys->d3d_proc, p_sys_src) ))
            return VLC_EGENERIC;

        /* first single frame */
        stream.pInputSurface = p_sys_src->processorInput;
    }

    RECT srcRect;
    srcRect.left   = p_pic->format.i_x_offset;
    srcRect.top    = p_pic->format.i_y_offset;
    srcRect.right  = srcRect.left + p_pic->format.i_visible_width;
    srcRect.bottom = srcRect.top  + p_pic->format.i_visible_height;
    ID3D11VideoContext_VideoProcessorSetStreamSourceRect(p_sys->d3d_proc.d3dvidctx, p_sys->d3d_proc.videoProcessor,
                                                         0, TRUE, &srcRect);
    ID3D11VideoContext_VideoProcessorSetStreamDestRect(p_sys->d3d_proc.d3dvidctx, p_sys->d3d_proc.videoProcessor,
                                                         0, TRUE, &srcRect);

    hr = ID3D11VideoContext_VideoProcessorBlt(p_sys->d3d_proc.d3dvidctx, p_sys->d3d_proc.videoProcessor,
                                              p_sys->processorOutput,
                                              0, 1, &stream);
    if (FAILED(hr))
        return VLC_EGENERIC;

    D3D11_BOX box = {
        .top = 0,
        .bottom = p_outpic->format.i_height,
        .left = 0,
        .right = p_outpic->format.i_width,
        .back = 1,
    };

    ID3D11DeviceContext_CopySubresourceRegion(p_sys->d3d_dev->d3dcontext,
                                              p_out_sys->resource[KNOWN_DXGI_INDEX],
                                              p_out_sys->slice_index,
                                              0, 0, 0,
                                              p_sys->outResource,
                                              0, &box);
    return VLC_SUCCESS;
}

static int RenderSinglePic( filter_t *p_filter, picture_t *p_outpic, picture_t *p_pic )
{
    return RenderPic( p_filter, p_outpic, p_pic, 0, 0 );
}

static picture_t *Deinterlace(filter_t *p_filter, picture_t *p_pic)
{
    filter_sys_t *p_sys = p_filter->p_sys;

    d3d11_device_lock( p_sys->d3d_dev );

    picture_t *res = DoDeinterlacing( p_filter, &p_sys->context, p_pic );

    d3d11_device_unlock( p_sys->d3d_dev );

    return res;
}

static const struct filter_mode_t *GetFilterMode(const char *mode)
{
    if ( mode == NULL || !strcmp( mode, "auto" ) )
        mode = "x";

    for (size_t i=0; i<ARRAY_SIZE(filter_mode); i++)
    {
        if( !strcmp( mode, filter_mode[i].psz_mode ) )
            return &filter_mode[i];
    }
    return NULL;
}

picture_t *AllocPicture( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    d3d11_video_context_t *vctx_sys = GetD3D11ContextPrivate( p_filter->vctx_out );

    const d3d_format_t *cfg = NULL;
    for (const d3d_format_t *output_format = GetRenderFormatList();
            output_format->name != NULL; ++output_format)
    {
        if (output_format->formatTexture == vctx_sys->format &&
            is_d3d11_opaque(output_format->fourcc))
        {
            cfg = output_format;
            break;
        }
    }
    if (unlikely(cfg == NULL))
        return NULL;

    return D3D11_AllocPicture(VLC_OBJECT(p_filter), &p_filter->fmt_out.video, p_filter->vctx_out, cfg);
}

int D3D11OpenDeinterlace(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    HRESULT hr;

    if (!is_d3d11_opaque(filter->fmt_in.video.i_chroma))
        return VLC_EGENERIC;
    if ( GetD3D11ContextPrivate(filter->vctx_in) == NULL )
        return VLC_EGENERIC;
    if (!video_format_IsSimilar(&filter->fmt_in.video, &filter->fmt_out.video))
        return VLC_EGENERIC;

    d3d11_video_context_t *vtcx_sys = GetD3D11ContextPrivate( filter->vctx_in );

    filter_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    memset(sys, 0, sizeof (*sys));

    d3d11_decoder_device_t *dev_sys = GetD3D11OpaqueContext( filter->vctx_in );
    sys->d3d_dev = &dev_sys->d3d_dev;
    if (D3D11_CreateProcessor(filter, sys->d3d_dev, D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST,
                              &filter->fmt_out.video, &filter->fmt_out.video, &sys->d3d_proc) != VLC_SUCCESS)
        goto error;

    UINT flags;
    hr = ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(sys->d3d_proc.procEnumerator, vtcx_sys->format, &flags);
    if (!SUCCEEDED(hr))
    {
        msg_Dbg(filter, "can't read processor support for %s", DxgiFormatToStr(vtcx_sys->format));
        goto error;
    }
    if ( !(flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT) ||
         !(flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT) )
    {
        msg_Dbg(filter, "deinterlacing %s is not supported", DxgiFormatToStr(vtcx_sys->format));
        goto error;
    }

    D3D11_VIDEO_PROCESSOR_CAPS processorCaps;
    hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorCaps(sys->d3d_proc.procEnumerator, &processorCaps);
    if (FAILED(hr))
        goto error;

    char *psz_mode = var_InheritString( filter, "deinterlace-mode" );
    const struct filter_mode_t *p_mode = GetFilterMode(psz_mode);
    if (p_mode == NULL)
    {
        msg_Dbg(filter, "unknown mode %s, trying blend", psz_mode);
        p_mode = GetFilterMode("blend");
    }
    if (strcmp(p_mode->psz_mode, psz_mode))
        msg_Dbg(filter, "using %s deinterlacing mode", p_mode->psz_mode);

    D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS rateCaps;
    for (UINT type = 0; type < processorCaps.RateConversionCapsCount; ++type)
    {
        ID3D11VideoProcessorEnumerator_GetVideoProcessorRateConversionCaps(sys->d3d_proc.procEnumerator, type, &rateCaps);
        if (!(rateCaps.ProcessorCaps & p_mode->i_mode))
            continue;

        hr = ID3D11VideoDevice_CreateVideoProcessor(sys->d3d_proc.d3dviddev,
                                                    sys->d3d_proc.procEnumerator, type, &sys->d3d_proc.videoProcessor);
        if (SUCCEEDED(hr))
            break;
        sys->d3d_proc.videoProcessor = NULL;
    }
    if ( sys->d3d_proc.videoProcessor==NULL &&
         p_mode->i_mode != D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BOB )
    {
        msg_Dbg(filter, "mode %s not available, trying bob", psz_mode);
        p_mode = GetFilterMode("bob");
        for (UINT type = 0; type < processorCaps.RateConversionCapsCount; ++type)
        {
            ID3D11VideoProcessorEnumerator_GetVideoProcessorRateConversionCaps(sys->d3d_proc.procEnumerator, type, &rateCaps);
            if (!(rateCaps.ProcessorCaps & D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BOB))
                continue;

            hr = ID3D11VideoDevice_CreateVideoProcessor(sys->d3d_proc.d3dviddev,
                                                        sys->d3d_proc.procEnumerator, type, &sys->d3d_proc.videoProcessor);
            if (SUCCEEDED(hr))
                break;
            sys->d3d_proc.videoProcessor = NULL;
        }
    }

    if (sys->d3d_proc.videoProcessor == NULL)
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
    texDesc.Format = vtcx_sys->format;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.ArraySize = 1;
    texDesc.Height = filter->fmt_out.video.i_height;
    texDesc.Width  = filter->fmt_out.video.i_width;

    hr = ID3D11Device_CreateTexture2D( sys->d3d_dev->d3ddevice, &texDesc, NULL, &sys->outTexture );
    if (FAILED(hr)) {
        msg_Err(filter, "CreateTexture2D failed. (hr=0x%lX)", hr);
        goto error;
    }

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outDesc = {
        .ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D,
    };

    hr = ID3D11VideoDevice_CreateVideoProcessorOutputView(sys->d3d_proc.d3dviddev,
                                                         sys->outResource,
                                                         sys->d3d_proc.procEnumerator,
                                                         &outDesc,
                                                         &sys->processorOutput);
    if (FAILED(hr))
    {
        msg_Dbg(filter,"Failed to create processor output. (hr=0x%lX)", hr);
        goto error;
    }

    InitDeinterlacingContext( &sys->context );

    sys->context.settings = p_mode->settings;
    sys->context.settings.b_use_frame_history = rateCaps.PastFrames != 0 ||
        rateCaps.FutureFrames != 0;
    if (sys->context.settings.b_use_frame_history != p_mode->settings.b_use_frame_history)
        msg_Dbg(filter, "deinterlacing not using frame history as requested");
    if (sys->context.settings.b_double_rate)
        sys->context.pf_render_ordered = RenderPic;
    else
        sys->context.pf_render_single_pic = RenderSinglePic;

    video_format_t out_fmt;
    GetDeinterlacingOutput( &sys->context, &out_fmt, &filter->fmt_in.video );
    if( !filter->b_allow_fmt_out_change &&
         out_fmt.i_height != filter->fmt_in.video.i_height )
    {
       goto error;
    }

    filter->fmt_out.video   = out_fmt;
    filter->vctx_out        = vlc_video_context_Hold(filter->vctx_in);
    filter->pf_video_filter = Deinterlace;
    filter->pf_flush        = Flush;
    filter->p_sys = sys;

    return VLC_SUCCESS;
error:
    if (sys->processorOutput)
        ID3D11VideoProcessorOutputView_Release(sys->processorOutput);
    if (sys->outTexture)
        ID3D11Texture2D_Release(sys->outTexture);
    D3D11_ReleaseProcessor(&sys->d3d_proc);
    free(sys);

    return VLC_EGENERIC;
}

void D3D11CloseDeinterlace(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    filter_sys_t *sys = filter->p_sys;

    if (likely(sys->processorOutput))
        ID3D11VideoProcessorOutputView_Release(sys->processorOutput);
    ID3D11Texture2D_Release(sys->outTexture);
    D3D11_ReleaseProcessor( &sys->d3d_proc );
    vlc_video_context_Release(filter->vctx_out);

    free(sys);
}

