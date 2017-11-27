/*****************************************************************************
 * dxva2_deinterlace.c: DxVA2 deinterlacing filter
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
#include <initguid.h>
#include <d3d9.h>
#include <dxva2api.h>
#include "../../video_chroma/d3d9_fmt.h"
#include "../../video_filter/deinterlace/common.h"

#include "d3d9_filters.h"

struct filter_sys_t
{
    HINSTANCE                      hdecoder_dll;
    /* keep a reference in case the vout is released first */
    d3d9_handle_t                  hd3d;
    d3d9_device_t                  d3d_dev;
    IDirectXVideoProcessor         *processor;
    IDirect3DSurface9              *hw_surface;

    DXVA2_VideoProcessorCaps       decoder_caps;

    SHORT Brightness;
    SHORT Contrast;
    SHORT Hue;
    SHORT Saturation;

    struct deinterlace_ctx         context;
    picture_t *                    (*buffer_new)( filter_t * );
};

struct filter_mode_t
{
    const char       *psz_mode;
    UINT              i_mode;
    deinterlace_algo  settings;
};
static struct filter_mode_t filter_mode [] = {
    { "blend",   DXVA2_DeinterlaceTech_BOBLineReplicate,
                 { false, false, false, false } },
    { "bob",     DXVA2_DeinterlaceTech_BOBVerticalStretch,
                 { true,  false, false, false } },
    { "x",       DXVA2_DeinterlaceTech_BOBVerticalStretch4Tap,
                 { true, true, false, false } },
    { "ivtc",    DXVA2_DeinterlaceTech_InverseTelecine,
                 { false, true, true, false } },
    { "yadif2x", DXVA2_DeinterlaceTech_PixelAdaptive,
                 { true,  true, false, false } },
};

static void Flush(filter_t *filter)
{
    FlushDeinterlacing(&filter->p_sys->context);
}

static void FillSample( DXVA2_VideoSample *p_sample,
                        const struct deinterlace_ctx *p_context,
                        picture_t *p_pic,
                        const video_format_t *p_fmt,
                        const RECT *p_area,
                        int i_field )
{
    picture_sys_t *p_sys_src = ActivePictureSys(p_pic);

    p_sample->SrcSurface = p_sys_src->surface;
    p_sample->SampleFormat.SampleFormat = p_pic->b_top_field_first ?
                DXVA2_SampleFieldInterleavedEvenFirst :
                DXVA2_SampleFieldInterleavedOddFirst;
    p_sample->Start = 0;
    p_sample->End = GetFieldDuration(p_context, p_fmt, p_pic) * 10;
    p_sample->SampleData = DXVA2_SampleData_RFF_TFF_Present;
    if (!i_field)
        p_sample->SampleData |= DXVA2_SampleData_TFF;
    else
        p_sample->SampleData |= DXVA2_SampleData_RFF;
    p_sample->DstRect = p_sample->SrcRect = *p_area;
    p_sample->PlanarAlpha    = DXVA2_Fixed32OpaqueAlpha();
}

static void FillBlitParams( filter_sys_t *sys,
                            DXVA2_VideoProcessBltParams *params, const RECT *area,
                            const DXVA2_VideoSample *samples, int order )
{
    memset(params, 0, sizeof(*params));
    params->TargetFrame = (samples->End - samples->Start) * order / 2;
    params->TargetRect  = *area;
    params->DestData    = 0;
    params->Alpha       = DXVA2_Fixed32OpaqueAlpha();
    params->DestFormat.SampleFormat = DXVA2_SampleProgressiveFrame;
    params->BackgroundColor.Alpha = 0xFFFF;
    params->ConstrictionSize.cx = params->TargetRect.right;
    params->ConstrictionSize.cy = params->TargetRect.bottom;

    params->ProcAmpValues.Brightness.Value = sys->Brightness;
    params->ProcAmpValues.Contrast.Value   = sys->Contrast;
    params->ProcAmpValues.Hue.Value        = sys->Hue;
    params->ProcAmpValues.Saturation.Value = sys->Saturation;
}

static int RenderPic( filter_t *filter, picture_t *p_outpic, picture_t *src,
                      int order, int i_field )
{
    filter_sys_t *sys = filter->p_sys;
    const int i_samples = sys->decoder_caps.NumBackwardRefSamples + 1 +
                          sys->decoder_caps.NumForwardRefSamples;
    HRESULT hr;
    DXVA2_VideoProcessBltParams params;
    DXVA2_VideoSample samples[i_samples];
    picture_t         *pictures[i_samples];
    D3DSURFACE_DESC srcDesc, dstDesc;
    RECT area;

    picture_t *p_prev = sys->context.pp_history[0];
    picture_t *p_cur  = sys->context.pp_history[1];
    picture_t *p_next = sys->context.pp_history[2];

    picture_sys_t *p_sys_src = ActivePictureSys(src);

    hr = IDirect3DSurface9_GetDesc( p_sys_src->surface, &srcDesc );
    if (unlikely(FAILED(hr)))
        return VLC_EGENERIC;
    hr = IDirect3DSurface9_GetDesc( sys->hw_surface, &dstDesc );
    if (unlikely(FAILED(hr)))
        return VLC_EGENERIC;

    area.top = area.left = 0;
    area.bottom = __MIN(srcDesc.Height, dstDesc.Height);
    area.right  = __MIN(srcDesc.Width,  dstDesc.Width);

    int idx = i_samples - 1;
    if (p_next)
    {
        pictures[idx--] = p_next;
        if (p_cur)
            pictures[idx--] = p_cur;
        if (p_prev)
            pictures[idx--] = p_prev;
    }
    else
        pictures[idx--] = src;
    while (idx >= 0)
        pictures[idx--] = NULL;

    for (idx = 0; idx <= i_samples-1; idx++)
    {
        if (pictures[idx])
            FillSample( &samples[idx], &sys->context, pictures[idx], &filter->fmt_out.video, &area, i_field);
        else
        {
            FillSample( &samples[idx], &sys->context, src, &filter->fmt_out.video, &area, i_field);
            samples[idx].SampleFormat.SampleFormat = DXVA2_SampleUnknown;
        }
    }

    FillBlitParams( sys, &params, &area, samples, order );

    hr = IDirectXVideoProcessor_VideoProcessBlt( sys->processor,
                                                 sys->hw_surface,
                                                 &params,
                                                 samples,
                                                 i_samples, NULL );
    if (FAILED(hr))
        return VLC_EGENERIC;

    hr = IDirect3DDevice9_StretchRect( sys->d3d_dev.dev,
                                       sys->hw_surface, NULL,
                                       p_outpic->p_sys->surface, NULL,
                                       D3DTEXF_NONE);
    if (FAILED(hr))
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static int RenderSinglePic( filter_t *p_filter, picture_t *p_outpic, picture_t *p_pic )
{
    return RenderPic( p_filter, p_outpic, p_pic, 0, 0 );
}

static picture_t *Deinterlace(filter_t *p_filter, picture_t *p_pic)
{
    return DoDeinterlacing( p_filter, &p_filter->p_sys->context, p_pic );
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

static void d3d9_pic_context_destroy(struct picture_context_t *ctx)
{
    struct va_pic_context *pic_ctx = (struct va_pic_context*)ctx;
    ReleasePictureSys(&pic_ctx->picsys);
    free(pic_ctx);
}

static struct picture_context_t *d3d9_pic_context_copy(struct picture_context_t *ctx)
{
    struct va_pic_context *src_ctx = (struct va_pic_context*)ctx;
    struct va_pic_context *pic_ctx = calloc(1, sizeof(*pic_ctx));
    if (unlikely(pic_ctx==NULL))
        return NULL;
    pic_ctx->s.destroy = d3d9_pic_context_destroy;
    pic_ctx->s.copy    = d3d9_pic_context_copy;
    pic_ctx->picsys = src_ctx->picsys;
    AcquirePictureSys(&pic_ctx->picsys);
    return &pic_ctx->s;
}

static picture_t *NewOutputPicture( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    picture_t *pic = p_sys->buffer_new( p_filter );
    if ( !pic->context )
    {
        bool b_local_texture = false;

        if (!pic->p_sys )
        {
            D3DSURFACE_DESC dstDesc;
            if ( !p_sys->hw_surface ||
                 FAILED(IDirect3DSurface9_GetDesc( p_sys->hw_surface, &dstDesc )) )
                return NULL;

            pic->p_sys = calloc(1, sizeof(*pic->p_sys));
            if (unlikely(pic->p_sys == NULL))
                return NULL;

            HRESULT hr = IDirect3DDevice9_CreateOffscreenPlainSurface(p_sys->d3d_dev.dev,
                                                              p_filter->fmt_out.video.i_width,
                                                              p_filter->fmt_out.video.i_height,
                                                              dstDesc.Format,
                                                              D3DPOOL_DEFAULT,
                                                              &pic->p_sys->surface,
                                                              NULL);

            if (FAILED(hr))
            {
                free(pic->p_sys);
                pic->p_sys = NULL;
                return NULL;
            }
            b_local_texture = true;
        }
        /* the picture might be duplicated for snapshots so it needs a context */
        struct va_pic_context *pic_ctx = calloc(1, sizeof(*pic_ctx));
        if (likely(pic_ctx!=NULL))
        {
            pic_ctx->s.destroy = d3d9_pic_context_destroy;
            pic_ctx->s.copy    = d3d9_pic_context_copy;
            pic_ctx->picsys = *pic->p_sys;
            AcquirePictureSys( &pic_ctx->picsys );
            pic->context = &pic_ctx->s;
        }
        if (b_local_texture)
            IDirect3DSurface9_Release(pic->p_sys->surface);
    }
    return pic;
}

int D3D9OpenDeinterlace(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    filter_sys_t *sys;
    HINSTANCE hdecoder_dll = NULL;
    HRESULT hr;
    GUID *processorGUIDs = NULL;
    GUID *processorGUID = NULL;
    IDirectXVideoProcessorService *processor = NULL;

    if (filter->fmt_in.video.i_chroma != VLC_CODEC_D3D9_OPAQUE
     && filter->fmt_in.video.i_chroma != VLC_CODEC_D3D9_OPAQUE_10B)
        return VLC_EGENERIC;
    if (!video_format_IsSimilar(&filter->fmt_in.video, &filter->fmt_out.video))
        return VLC_EGENERIC;

    sys = calloc(1, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    if (unlikely(D3D9_Create( filter, &sys->hd3d ) != VLC_SUCCESS)) {
        msg_Warn(filter, "cannot load d3d9.dll, aborting");
        free(sys);
        return VLC_EGENERIC;
    }

    hdecoder_dll = LoadLibrary(TEXT("DXVA2.DLL"));
    if (!hdecoder_dll)
        goto error;

    D3DSURFACE_DESC dstDesc;
    D3D9_FilterHoldInstance( filter, &sys->d3d_dev, &dstDesc );
    if (!sys->d3d_dev.dev)
    {
        msg_Dbg(filter, "Filter without a context");
        goto error;
    }

    HRESULT (WINAPI *CreateVideoService)(IDirect3DDevice9 *,
                                         REFIID riid,
                                         void **ppService);
    CreateVideoService =
      (void *)GetProcAddress(hdecoder_dll, "DXVA2CreateVideoService");
    if (CreateVideoService == NULL)
        goto error;
    hr = CreateVideoService( sys->d3d_dev.dev, &IID_IDirectXVideoProcessorService,
                            (void**)&processor);
    if (FAILED(hr))
        goto error;

    DXVA2_VideoDesc dsc;
    ZeroMemory(&dsc, sizeof(dsc));
    dsc.SampleWidth     = dstDesc.Width;
    dsc.SampleHeight    = dstDesc.Height;
    dsc.Format          = dstDesc.Format;
    if (filter->fmt_in.video.i_frame_rate && filter->fmt_in.video.i_frame_rate_base) {
        dsc.InputSampleFreq.Numerator   = filter->fmt_in.video.i_frame_rate;
        dsc.InputSampleFreq.Denominator = filter->fmt_in.video.i_frame_rate_base;
    } else {
        dsc.InputSampleFreq.Numerator   = 0;
        dsc.InputSampleFreq.Denominator = 0;
    }
    dsc.OutputFrameFreq = dsc.InputSampleFreq;
    dsc.SampleFormat.SampleFormat = DXVA2_SampleFieldInterleavedEvenFirst;

    UINT count = 0;
    hr = IDirectXVideoProcessorService_GetVideoProcessorDeviceGuids( processor,
                                                                &dsc,
                                                                &count,
                                                                &processorGUIDs);
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

    DXVA2_VideoProcessorCaps caps, best_caps;
    unsigned best_score = 0;
    for (UINT i=0; i<count; ++i) {
        hr = IDirectXVideoProcessorService_GetVideoProcessorCaps( processor,
                                                                  processorGUIDs+i,
                                                                  &dsc,
                                                                  dsc.Format,
                                                                  &caps);
        if ( FAILED(hr) || !caps.DeinterlaceTechnology )
            continue;

        unsigned score = (caps.DeinterlaceTechnology & p_mode->i_mode) ? 10 : 1;
        if (best_score < score) {
            best_score = score;
            best_caps = caps;
            processorGUID = processorGUIDs + i;
        }
    }

    if (processorGUID == NULL)
    {
        msg_Dbg(filter, "Could not find a filter to output the required format");
        goto error;
    }

    hr = IDirectXVideoProcessorService_CreateVideoProcessor( processor,
                                                             processorGUID,
                                                             &dsc,
                                                             dsc.Format,
                                                             1,
                                                             &sys->processor );
    if (FAILED(hr))
        goto error;

    hr = IDirectXVideoProcessorService_CreateSurface( processor,
                                                      dstDesc.Width,
                                                      dstDesc.Height,
                                                      0,
                                                      dstDesc.Format,
                                                      D3DPOOL_DEFAULT,
                                                      0,
                                                      DXVA2_VideoProcessorRenderTarget,
                                                      &sys->hw_surface,
                                                      NULL);
    if (FAILED(hr))
        goto error;

    DXVA2_ValueRange Range;
    hr = IDirectXVideoProcessorService_GetProcAmpRange( processor, processorGUID, &dsc,
                                                        dstDesc.Format, DXVA2_ProcAmp_Brightness,
                                                        &Range );
    if (FAILED(hr))
        goto error;
    sys->Brightness = Range.DefaultValue.Value;

    hr = IDirectXVideoProcessorService_GetProcAmpRange( processor, processorGUID, &dsc,
                                                        dstDesc.Format, DXVA2_ProcAmp_Contrast,
                                                        &Range );
    if (FAILED(hr))
        goto error;
    sys->Contrast = Range.DefaultValue.Value;

    hr = IDirectXVideoProcessorService_GetProcAmpRange( processor, processorGUID, &dsc,
                                                        dstDesc.Format, DXVA2_ProcAmp_Hue,
                                                        &Range );
    if (FAILED(hr))
        goto error;
    sys->Hue = Range.DefaultValue.Value;

    hr = IDirectXVideoProcessorService_GetProcAmpRange( processor, processorGUID, &dsc,
                                                        dstDesc.Format, DXVA2_ProcAmp_Saturation,
                                                        &Range );
    if (FAILED(hr))
        goto error;
    sys->Saturation = Range.DefaultValue.Value;

    sys->hdecoder_dll = hdecoder_dll;
    sys->decoder_caps = best_caps;

    InitDeinterlacingContext( &sys->context );

    sys->context.settings = p_mode->settings;
    sys->context.settings.b_use_frame_history = best_caps.NumBackwardRefSamples != 0 ||
                                       best_caps.NumForwardRefSamples  != 0;
    if (sys->context.settings.b_use_frame_history != p_mode->settings.b_use_frame_history)
        msg_Dbg( filter, "deinterlacing not using frame history as requested");
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

    CoTaskMemFree(processorGUIDs);
    IDirectXVideoProcessorService_Release(processor);

    sys->buffer_new = filter->owner.video.buffer_new;
    filter->owner.video.buffer_new = NewOutputPicture;
    filter->fmt_out.video   = out_fmt;
    filter->pf_video_filter = Deinterlace;
    filter->pf_flush        = Flush;
    filter->p_sys = sys;

    return VLC_SUCCESS;
error:
    if (processorGUIDs)
        CoTaskMemFree(processorGUIDs);
    if (sys && sys->processor)
        IDirectXVideoProcessor_Release( sys->processor );
    if (processor)
        IDirectXVideoProcessorService_Release(processor);
    D3D9_FilterReleaseInstance( &sys->d3d_dev );
    if (hdecoder_dll)
        FreeLibrary(hdecoder_dll);
    D3D9_Destroy( &sys->hd3d );
    free(sys);

    return VLC_EGENERIC;
}

void D3D9CloseDeinterlace(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    filter_sys_t *sys = filter->p_sys;

    IDirect3DSurface9_Release( sys->hw_surface );
    IDirectXVideoProcessor_Release( sys->processor );
    D3D9_FilterReleaseInstance( &sys->d3d_dev );
    FreeLibrary( sys->hdecoder_dll );
    D3D9_Destroy( &sys->hd3d );

    free(sys);
}
