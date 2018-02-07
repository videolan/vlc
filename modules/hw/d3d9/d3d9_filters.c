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

#include <stdlib.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include <vlc_atomic.h>

#define COBJMACROS
#include <initguid.h>
#include <d3d9.h>
#include <dxva2api.h>
#include "../../video_chroma/d3d9_fmt.h"

#include "d3d9_filters.h"

struct filter_level
{
    atomic_int   level;
    float  default_val;
    float  min;
    float  max;
    DXVA2_ValueRange Range;
};

struct filter_sys_t
{
    HINSTANCE                      hdecoder_dll;
    /* keep a reference in case the vout is released first */
    HINSTANCE                      d3d9_dll;
    d3d9_device_t                  d3d_dev;
    IDirectXVideoProcessor         *processor;
    IDirect3DSurface9              *hw_surface;

    struct filter_level Brightness;
    struct filter_level Contrast;
    struct filter_level Hue;
    struct filter_level Saturation;
};

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
    picture_sys_t *p_sys_src = ActivePictureSys(p_pic);

    p_sample->SrcSurface = p_sys_src->surface;
    p_sample->SampleFormat.SampleFormat = DXVA2_SampleProgressiveFrame;
    p_sample->Start = 0;
    p_sample->End =0;
    p_sample->SampleData = 0;
    p_sample->DstRect = p_sample->SrcRect = *p_area;
    p_sample->PlanarAlpha    = DXVA2_Fixed32OpaqueAlpha();
}

static picture_t *Filter(filter_t *p_filter, picture_t *p_pic)
{
    filter_sys_t *p_sys = p_filter->p_sys;

    picture_sys_t *p_src_sys = ActivePictureSys(p_pic);

    picture_t *p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic || !p_outpic->p_sys || !p_outpic->p_sys->surface )
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

    params.ProcAmpValues.Brightness.Value = atomic_load( &p_sys->Brightness.level );
    params.ProcAmpValues.Contrast.Value   = atomic_load( &p_sys->Contrast.level );
    params.ProcAmpValues.Hue.Value        = atomic_load( &p_sys->Hue.level );
    params.ProcAmpValues.Saturation.Value = atomic_load( &p_sys->Saturation.level );
    params.TargetFrame = 0;
    params.TargetRect  = area;
    params.DestData    = 0;
    params.Alpha       = DXVA2_Fixed32OpaqueAlpha();
    params.DestFormat.SampleFormat = DXVA2_SampleProgressiveFrame;
    params.BackgroundColor.Alpha = 0xFFFF;

    hr = IDirectXVideoProcessor_VideoProcessBlt( p_sys->processor,
                                                 p_sys->hw_surface,
                                                 &params,
                                                 &sample,
                                                 1, NULL );
    hr = IDirect3DDevice9_StretchRect( p_sys->d3d_dev.dev,
                                       p_sys->hw_surface, NULL,
                                       p_outpic->p_sys->surface, NULL,
                                       D3DTEXF_NONE);
    if (FAILED(hr))
        goto failed;

    picture_Release( p_pic );
    return p_outpic;
failed:
    picture_Release( p_pic );
    return NULL;
}

static void SetLevel(struct filter_level *range, float val)
{
    int level;
    if (val > range->default_val)
        level = (range->Range.MaxValue.Value - range->Range.DefaultValue.Value) * (val - range->default_val) /
                (range->max - range->default_val);
    else if (val < range->default_val)
        level = (range->Range.MinValue.Value - range->Range.DefaultValue.Value) * (val - range->default_val) /
                (range->min - range->default_val);
    else
        level = 0;

    atomic_store( &range->level, range->Range.DefaultValue.Value + level );
}

static void InitLevel(filter_t *filter, struct filter_level *range, const char *p_name, float def)
{
    int level;

    module_config_t *cfg = config_FindConfig(p_name);
    range->min = cfg->min.f;
    range->max = cfg->max.f;
    range->default_val = def;

    float val = var_CreateGetFloatCommand( filter, p_name );

    if (val > range->default_val)
        level = (range->Range.MaxValue.Value - range->Range.DefaultValue.Value) * (val - range->default_val) /
                (range->max - range->default_val);
    else if (val < range->default_val)
        level = (range->Range.MinValue.Value - range->Range.DefaultValue.Value) * (val - range->default_val) /
                (range->min - range->default_val);
    else
        level = 0;

    atomic_init( &range->level, range->Range.DefaultValue.Value + level );
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

static int D3D9OpenAdjust(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    filter_sys_t *sys = NULL;
    HINSTANCE hdecoder_dll = NULL;
    HINSTANCE d3d9_dll = NULL;
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

    d3d9_dll = LoadLibrary(TEXT("D3D9.DLL"));
    if (!d3d9_dll)
        goto error;

    hdecoder_dll = LoadLibrary(TEXT("DXVA2.DLL"));
    if (!hdecoder_dll)
        goto error;

    D3DSURFACE_DESC dstDesc;
    D3D9_FilterHoldInstance(filter, &sys->d3d_dev, &dstDesc);
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

    DXVA2_ExtendedFormat *pFormat = &dsc.SampleFormat;
    pFormat->SampleFormat = DXVA2_SampleProgressiveFrame;

    UINT count = 0;
    hr = IDirectXVideoProcessorService_GetVideoProcessorDeviceGuids( processor,
                                                                &dsc,
                                                                &count,
                                                                &processorGUIDs);
    if (FAILED(hr))
        goto error;

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
                                                        dstDesc.Format, DXVA2_ProcAmp_Brightness,
                                                        &sys->Brightness.Range );
    if (FAILED(hr))
        goto error;

    hr = IDirectXVideoProcessorService_GetProcAmpRange( processor, processorGUID, &dsc,
                                                        dstDesc.Format, DXVA2_ProcAmp_Contrast,
                                                        &sys->Contrast.Range );
    if (FAILED(hr))
        goto error;

    hr = IDirectXVideoProcessorService_GetProcAmpRange( processor, processorGUID, &dsc,
                                                        dstDesc.Format, DXVA2_ProcAmp_Hue,
                                                        &sys->Hue.Range );
    if (FAILED(hr))
        goto error;

    hr = IDirectXVideoProcessorService_GetProcAmpRange( processor, processorGUID, &dsc,
                                                        dstDesc.Format, DXVA2_ProcAmp_Saturation,
                                                        &sys->Saturation.Range );
    if (FAILED(hr))
        goto error;

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

    CoTaskMemFree(processorGUIDs);
    IDirectXVideoProcessorService_Release(processor);

    sys->hdecoder_dll = hdecoder_dll;
    sys->d3d9_dll     = d3d9_dll;

    filter->pf_video_filter = Filter;
    filter->p_sys = sys;

    return VLC_SUCCESS;
error:
    CoTaskMemFree(processorGUIDs);
    if (sys && sys->processor)
        IDirectXVideoProcessor_Release( sys->processor );
    if (processor)
        IDirectXVideoProcessorService_Release(processor);
    if (sys)
        D3D9_FilterReleaseInstance( &sys->d3d_dev );
    if (hdecoder_dll)
        FreeLibrary(hdecoder_dll);
    if (d3d9_dll)
        FreeLibrary(d3d9_dll);
    free(sys);

    return VLC_EGENERIC;
}

static void D3D9CloseAdjust(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    filter_sys_t *sys = filter->p_sys;

    IDirect3DSurface9_Release( sys->hw_surface );
    IDirectXVideoProcessor_Release( sys->processor );
    D3D9_FilterReleaseInstance( &sys->d3d_dev );
    FreeLibrary( sys->hdecoder_dll );
    FreeLibrary( sys->d3d9_dll );

    free(sys);
}

vlc_module_begin()
    set_description(N_("Direct3D9 adjust filter"))
    set_capability("video filter", 0)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_callbacks(D3D9OpenAdjust, D3D9CloseAdjust)
    add_shortcut( "adjust" )

    add_float_with_range( "contrast", 1.0, 0.0, 2.0,
                          CONT_TEXT, CONT_LONGTEXT, false )
        change_safe()
    add_float_with_range( "brightness", 1.0, 0.0, 2.0,
                           LUM_TEXT, LUM_LONGTEXT, false )
        change_safe()
    add_float_with_range( "hue", 0, -180., +180.,
                            HUE_TEXT, HUE_LONGTEXT, false )
        change_safe()
    add_float_with_range( "saturation", 1.0, 0.0, 3.0,
                          SAT_TEXT, SAT_LONGTEXT, false )
        change_safe()
    add_float_with_range( "gamma", 1.0, 0.01, 10.0,
                          GAMMA_TEXT, GAMMA_LONGTEXT, false )
        change_safe()
    add_bool( "brightness-threshold", false,
              THRES_TEXT, THRES_LONGTEXT, false )
        change_safe()

    add_submodule()
    set_description("Direct3D9 deinterlace filter")
    set_callbacks(D3D9OpenDeinterlace, D3D9CloseDeinterlace)
    add_shortcut ("deinterlace")

    add_submodule()
    set_capability( "video converter", 10 )
    set_callbacks( D3D9OpenConverter, D3D9CloseConverter )

    add_submodule()
    set_callbacks( D3D9OpenCPUConverter, D3D9CloseCPUConverter )
    set_capability( "video converter", 10 )
vlc_module_end()
