/*****************************************************************************
 * d3d9_filters.c: D3D9 filters module callbacks
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

#include <stdatomic.h>
#include <stdlib.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include <vlc_codec.h>

#define COBJMACROS
#include <initguid.h>
#include <d3d9.h>
#include <dxva2api.h>
#include "../../video_chroma/d3d9_fmt.h"

#include "d3d9_filters.h"

struct filter_level
{
    atomic_long  level;
    float  default_val;
    float  min;
    float  max;
    DXVA2_ValueRange Range;
};

typedef struct
{
    HINSTANCE                      hdecoder_dll;
    IDirectXVideoProcessor         *processor;
    IDirect3DSurface9              *hw_surface;

    struct filter_level Brightness;
    struct filter_level Contrast;
    struct filter_level Hue;
    struct filter_level Saturation;
} filter_sys_t;

#define THRES_TEXT N_("Brightness threshold")
#define THRES_LONGTEXT N_("When this mode is enabled, pixels will be " \
        "shown as black or white. The threshold value will be the brightness " \
        "defined below." )
#define CONT_TEXT N_("Image contrast (0-2)")
#define CONT_LONGTEXT N_("Set the image contrast, between 0 and 2. Defaults to 1.")
#define HUE_TEXT N_("Image hue (0-360)")
#define HUE_LONGTEXT N_("Set the image hue, between 0 and 360. Defaults to 0.")
#define SAT_TEXT N_("Image saturation (0-3)")
#define SAT_LONGTEXT N_("Set the image saturation, between 0 and 3. Defaults to 1.")
#define LUM_TEXT N_("Image brightness (0-2)")
#define LUM_LONGTEXT N_("Set the image brightness, between 0 and 2. Defaults to 1.")
#define GAMMA_TEXT N_("Image gamma (0-10)")
#define GAMMA_LONGTEXT N_("Set the image gamma, between 0.01 and 10. Defaults to 1.")

static const char *const ppsz_filter_options[] = {
    "contrast", "brightness", "hue", "saturation", "gamma",
    "brightness-threshold", NULL
};

static void FillSample( DXVA2_VideoSample *p_sample,
                        picture_t *p_pic,
                        const RECT *p_area )
{
    picture_sys_d3d9_t *p_sys_src = ActiveD3D9PictureSys(p_pic);

    p_sample->SrcSurface = p_sys_src->surface;
    p_sample->SampleFormat.SampleFormat = DXVA2_SampleProgressiveFrame;
    p_sample->Start = 0;
    p_sample->End =0;
    p_sample->SampleData = 0;
    p_sample->DstRect = p_sample->SrcRect = *p_area;
    p_sample->PlanarAlpha    = DXVA2_Fixed32OpaqueAlpha();
}

static picture_t *AllocPicture( filter_t *p_filter )
{
    struct d3d9_pic_context *pic_ctx = calloc(1, sizeof(*pic_ctx));
    if (unlikely(pic_ctx == NULL))
        return NULL;

    picture_t *pic = picture_NewFromFormat( &p_filter->fmt_out.video );
    if (unlikely(pic == NULL))
    {
        free(pic_ctx);
        return NULL;
    }

    d3d9_decoder_device_t *d3d9_decoder = GetD3D9OpaqueContext(p_filter->vctx_out);
    d3d9_video_context_t *vctx_sys = GetD3D9ContextPrivate( p_filter->vctx_out );

    HRESULT hr = IDirect3DDevice9_CreateOffscreenPlainSurface(d3d9_decoder->d3ddev.dev,
                                                        p_filter->fmt_out.video.i_width,
                                                        p_filter->fmt_out.video.i_height,
                                                        vctx_sys->format,
                                                        D3DPOOL_DEFAULT,
                                                        &pic_ctx->picsys.surface,
                                                        NULL);
    if (FAILED(hr))
    {
        free(pic_ctx);
        picture_Release(pic);
        return NULL;
    }
    AcquireD3D9PictureSys( &pic_ctx->picsys );
    IDirect3DSurface9_Release(pic_ctx->picsys.surface);
    pic_ctx->s = (picture_context_t) {
        d3d9_pic_context_destroy, d3d9_pic_context_copy,
        vlc_video_context_Hold(p_filter->vctx_out),
    };
    pic->context = &pic_ctx->s;
    return pic;
}

static picture_t *Filter(filter_t *p_filter, picture_t *p_pic)
{
    filter_sys_t *p_sys = p_filter->p_sys;

    picture_sys_d3d9_t *p_src_sys = ActiveD3D9PictureSys(p_pic);

    picture_t *p_outpic = AllocPicture( p_filter );
    if( !p_outpic )
        goto failed;

    picture_sys_d3d9_t *p_out_sys = ActiveD3D9PictureSys(p_outpic);
    if( !p_out_sys || !p_out_sys->surface )
        goto failed;

    picture_CopyProperties( p_outpic, p_pic );

    RECT area;
    D3DSURFACE_DESC srcDesc, dstDesc;
    HRESULT hr;

    hr = IDirect3DSurface9_GetDesc( p_src_sys->surface, &srcDesc );
    if (unlikely(FAILED(hr)))
        goto failed;
    hr = IDirect3DSurface9_GetDesc( p_sys->hw_surface, &dstDesc );
    if (unlikely(FAILED(hr)))
        goto failed;

    area.top = area.left = 0;
    area.bottom = __MIN(srcDesc.Height, dstDesc.Height);
    area.right  = __MIN(srcDesc.Width,  dstDesc.Width);

    DXVA2_VideoProcessBltParams params = {0};
    DXVA2_VideoSample sample = {0};
    FillSample( &sample, p_pic, &area );

    params.ProcAmpValues.Brightness.ll = atomic_load( &p_sys->Brightness.level );
    params.ProcAmpValues.Contrast.ll   = atomic_load( &p_sys->Contrast.level );
    params.ProcAmpValues.Hue.ll        = atomic_load( &p_sys->Hue.level );
    params.ProcAmpValues.Saturation.ll = atomic_load( &p_sys->Saturation.level );
    params.TargetFrame = 0;
    params.TargetRect  = area;
    params.DestData    = 0;
    params.Alpha       = DXVA2_Fixed32OpaqueAlpha();
    params.DestFormat.SampleFormat = DXVA2_SampleProgressiveFrame;
    params.BackgroundColor.Alpha = 0xFFFF;

    d3d9_decoder_device_t *d3d9_decoder = GetD3D9OpaqueContext(p_filter->vctx_out);

    hr = IDirectXVideoProcessor_VideoProcessBlt( p_sys->processor,
                                                 p_sys->hw_surface,
                                                 &params,
                                                 &sample,
                                                 1, NULL );
    hr = IDirect3DDevice9_StretchRect( d3d9_decoder->d3ddev.dev,
                                       p_sys->hw_surface, NULL,
                                       p_out_sys->surface, NULL,
                                       D3DTEXF_NONE);
    if (FAILED(hr))
        goto failed;

    picture_Release( p_pic );
    return p_outpic;
failed:
    picture_Release( p_pic );
    return NULL;
}

static LONG StoreLevel(const struct filter_level *range, const DXVA2_ValueRange *Range, float val)
{
    LONG level;
    if (val > range->default_val)
    {
        level = (Range->MaxValue.ll - Range->DefaultValue.ll) * (val - range->default_val) /
                (range->max - range->default_val);
    }
    else if (val < range->default_val)
    {
        level = (Range->MinValue.ll - Range->DefaultValue.ll) * (val - range->default_val) /
                (range->min - range->default_val);
    }
    else
        level = 0;

    return level + Range->DefaultValue.ll;
}

static void SetLevel(struct filter_level *range, float val)
{
    atomic_store( &range->level, StoreLevel( range, &range->Range, val ) );
}

static void InitLevel(filter_t *filter, struct filter_level *range, const char *p_name, float def)
{
    module_config_t *cfg = config_FindConfig(p_name);
    range->min = cfg->min.f;
    range->max = cfg->max.f;
    range->default_val = def;

    float val = var_CreateGetFloatCommand( filter, p_name );

    atomic_init( &range->level, StoreLevel( range, &range->Range, val ) );
}

static int AdjustCallback( vlc_object_t *p_this, char const *psz_var,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval);
    filter_sys_t *p_sys = (filter_sys_t *)p_data;

    if( !strcmp( psz_var, "contrast" ) )
        SetLevel( &p_sys->Contrast, newval.f_float );
    else if( !strcmp( psz_var, "brightness" ) )
        SetLevel( &p_sys->Brightness, newval.f_float );
    else if( !strcmp( psz_var, "hue" ) )
        SetLevel( &p_sys->Hue, newval.f_float );
    else if( !strcmp( psz_var, "saturation" ) )
        SetLevel( &p_sys->Saturation, newval.f_float );

    return VLC_SUCCESS;
}

static void D3D9CloseAdjust(filter_t *filter)
{
    filter_sys_t *sys = filter->p_sys;

    IDirect3DSurface9_Release( sys->hw_surface );
    IDirectXVideoProcessor_Release( sys->processor );
    FreeLibrary( sys->hdecoder_dll );
    vlc_video_context_Release(filter->vctx_out);

    free(sys);
}

static const struct vlc_filter_operations filter_ops = {
    .filter_video = Filter, .close = D3D9CloseAdjust,
};

static int D3D9OpenAdjust(filter_t *filter)
{
    filter_sys_t *sys = NULL;
    HINSTANCE hdecoder_dll = NULL;
    HRESULT hr;
    GUID *processorGUIDs = NULL;
    GUID *processorGUID = NULL;
    IDirectXVideoProcessorService *processor = NULL;

    if (filter->fmt_in.video.i_chroma != VLC_CODEC_D3D9_OPAQUE
     && filter->fmt_in.video.i_chroma != VLC_CODEC_D3D9_OPAQUE_10B)
        return VLC_EGENERIC;
    if ( GetD3D9ContextPrivate(filter->vctx_in) == NULL )
        return VLC_EGENERIC;
    if (!video_format_IsSimilar(&filter->fmt_in.video, &filter->fmt_out.video))
        return VLC_EGENERIC;

    sys = calloc(1, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    hdecoder_dll = LoadLibrary(TEXT("DXVA2.DLL"));
    if (!hdecoder_dll)
        goto error;

    d3d9_video_context_t *vtcx_sys = GetD3D9ContextPrivate( filter->vctx_in );
    D3DFORMAT format = vtcx_sys->format;

    HRESULT (WINAPI *CreateVideoService)(IDirect3DDevice9 *,
                                         REFIID riid,
                                         void **ppService);
    CreateVideoService =
      (void *)GetProcAddress(hdecoder_dll, "DXVA2CreateVideoService");
    if (CreateVideoService == NULL)
    {
        msg_Err(filter, "Can't create video service");
        goto error;
    }

    d3d9_decoder_device_t *d3d9_decoder = GetD3D9OpaqueContext(filter->vctx_in);
    hr = CreateVideoService( d3d9_decoder->d3ddev.dev, &IID_IDirectXVideoProcessorService,
                            (void**)&processor);
    if (FAILED(hr))
    {
        msg_Err(filter, "Failed to create the video processor. (hr=0x%lX)", hr);
        goto error;
    }

    DXVA2_VideoDesc dsc;
    ZeroMemory(&dsc, sizeof(dsc));
    dsc.SampleWidth     = filter->fmt_in.video.i_width;
    dsc.SampleHeight    = filter->fmt_in.video.i_height;
    dsc.Format          = format;
    if (filter->fmt_in.video.i_frame_rate && filter->fmt_in.video.i_frame_rate_base) {
        dsc.InputSampleFreq.Numerator   = filter->fmt_in.video.i_frame_rate;
        dsc.InputSampleFreq.Denominator = filter->fmt_in.video.i_frame_rate_base;
    } else {
        dsc.InputSampleFreq.Numerator   = 0;
        dsc.InputSampleFreq.Denominator = 0;
    }
    dsc.OutputFrameFreq = dsc.InputSampleFreq;

    DXVA2_ExtendedFormat *pFormat = &dsc.SampleFormat;
    pFormat->SampleFormat = DXVA2_SampleProgressiveFrame;

    UINT count = 0;
    hr = IDirectXVideoProcessorService_GetVideoProcessorDeviceGuids( processor,
                                                                &dsc,
                                                                &count,
                                                                &processorGUIDs);
    if (FAILED(hr))
    {
        msg_Err(filter, "Failed to get processor GUIDs. (hr=0x%lX)", hr);
        goto error;
    }

    const UINT neededCaps = DXVA2_ProcAmp_Brightness |
                            DXVA2_ProcAmp_Contrast |
                            DXVA2_ProcAmp_Hue |
                            DXVA2_ProcAmp_Saturation;
    DXVA2_VideoProcessorCaps caps;
    unsigned best_score = 0;
    for (UINT i=0; i<count; ++i) {
        hr = IDirectXVideoProcessorService_GetVideoProcessorCaps( processor,
                                                                  processorGUIDs+i,
                                                                  &dsc,
                                                                  dsc.Format,
                                                                  &caps);
        if ( FAILED(hr) || !caps.ProcAmpControlCaps )
            continue;

        unsigned score = (caps.ProcAmpControlCaps & neededCaps) ? 10 : 1;
        if (best_score < score) {
            best_score = score;
            processorGUID = processorGUIDs + i;
        }
    }

    if (processorGUID == NULL)
    {
        msg_Dbg(filter, "Could not find a filter to output the required format");
        goto error;
    }

    hr = IDirectXVideoProcessorService_GetProcAmpRange( processor, processorGUID, &dsc,
                                                        format, DXVA2_ProcAmp_Brightness,
                                                        &sys->Brightness.Range );
    if (FAILED(hr))
    {
        msg_Err(filter, "Failed to get the brightness range. (hr=0x%lX)", hr);
        goto error;
    }

    hr = IDirectXVideoProcessorService_GetProcAmpRange( processor, processorGUID, &dsc,
                                                        format, DXVA2_ProcAmp_Contrast,
                                                        &sys->Contrast.Range );
    if (FAILED(hr))
    {
        msg_Err(filter, "Failed to get the contrast range. (hr=0x%lX)", hr);
        goto error;
    }

    hr = IDirectXVideoProcessorService_GetProcAmpRange( processor, processorGUID, &dsc,
                                                        format, DXVA2_ProcAmp_Hue,
                                                        &sys->Hue.Range );
    if (FAILED(hr))
    {
        msg_Err(filter, "Failed to get the hue range. (hr=0x%lX)", hr);
        goto error;
    }

    hr = IDirectXVideoProcessorService_GetProcAmpRange( processor, processorGUID, &dsc,
                                                        format, DXVA2_ProcAmp_Saturation,
                                                        &sys->Saturation.Range );
    if (FAILED(hr))
    {
        msg_Err(filter, "Failed to get the saturation range. (hr=0x%lX)", hr);
        goto error;
    }

    /* needed to get options passed in transcode using the
     * adjust{name=value} syntax */
    config_ChainParse( filter, "", ppsz_filter_options, filter->p_cfg );

    InitLevel(filter, &sys->Contrast,   "contrast",   1.0 );
    InitLevel(filter, &sys->Brightness, "brightness", 1.0 );
    InitLevel(filter, &sys->Hue,        "hue",        0.0 );
    InitLevel(filter, &sys->Saturation, "saturation", 1.0 );

    var_AddCallback( filter, "contrast",   AdjustCallback, sys );
    var_AddCallback( filter, "brightness", AdjustCallback, sys );
    var_AddCallback( filter, "hue",        AdjustCallback, sys );
    var_AddCallback( filter, "saturation", AdjustCallback, sys );
    var_AddCallback( filter, "gamma",      AdjustCallback, sys );
    var_AddCallback( filter, "brightness-threshold",
                                             AdjustCallback, sys );

    hr = IDirectXVideoProcessorService_CreateVideoProcessor( processor,
                                                             processorGUID,
                                                             &dsc,
                                                             dsc.Format,
                                                             1,
                                                             &sys->processor );
    if (FAILED(hr))
    {
        msg_Err(filter, "Failed to create the video processor. (hr=0x%lX)", hr);
        goto error;
    }

    hr = IDirectXVideoProcessorService_CreateSurface( processor,
                                                      filter->fmt_out.video.i_width,
                                                      filter->fmt_out.video.i_height,
                                                      0,
                                                      format,
                                                      D3DPOOL_DEFAULT,
                                                      0,
                                                      DXVA2_VideoProcessorRenderTarget,
                                                      &sys->hw_surface,
                                                      NULL);
    if (FAILED(hr))
    {
        msg_Err(filter, "Failed to create the hardware surface. (hr=0x%lX)", hr);
        goto error;
    }

    CoTaskMemFree(processorGUIDs);
    IDirectXVideoProcessorService_Release(processor);

    sys->hdecoder_dll = hdecoder_dll;

    filter->ops = &filter_ops;
    filter->p_sys = sys;
    filter->vctx_out = vlc_video_context_Hold(filter->vctx_in);

    return VLC_SUCCESS;
error:
    CoTaskMemFree(processorGUIDs);
    if (sys && sys->processor)
        IDirectXVideoProcessor_Release( sys->processor );
    if (processor)
        IDirectXVideoProcessorService_Release(processor);
    if (hdecoder_dll)
        FreeLibrary(hdecoder_dll);
    free(sys);

    return VLC_EGENERIC;
}

vlc_module_begin()
    set_description(N_("Direct3D9 adjust filter"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_callback_video_filter(D3D9OpenAdjust)
    add_shortcut( "adjust" )

    add_float_with_range( "contrast", 1.0, 0.0, 2.0,
                          CONT_TEXT, CONT_LONGTEXT )
        change_safe()
    add_float_with_range( "brightness", 1.0, 0.0, 2.0,
                           LUM_TEXT, LUM_LONGTEXT )
        change_safe()
    add_float_with_range( "hue", 0, -180., +180.,
                            HUE_TEXT, HUE_LONGTEXT )
        change_safe()
    add_float_with_range( "saturation", 1.0, 0.0, 3.0,
                          SAT_TEXT, SAT_LONGTEXT )
        change_safe()
    add_float_with_range( "gamma", 1.0, 0.01, 10.0,
                          GAMMA_TEXT, GAMMA_LONGTEXT )
        change_safe()
    add_bool( "brightness-threshold", false,
              THRES_TEXT, THRES_LONGTEXT )
        change_safe()

    add_submodule()
    set_description(N_("Direct3D9 deinterlace filter"))
    set_deinterlace_callback( D3D9OpenDeinterlace )

    add_submodule()
    set_callback_video_converter( D3D9OpenConverter, 10 )

    add_submodule()
    set_callback_video_converter( D3D9OpenCPUConverter, 10 )

    add_submodule()
    set_description(N_("Direct3D9"))
    set_callback_dec_device( D3D9OpenDecoderDevice, 10 )
    add_shortcut ("dxva2")
vlc_module_end()
