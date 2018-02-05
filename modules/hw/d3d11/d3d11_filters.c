/*****************************************************************************
 * d3d11_adjust.c: D3D11 filters module callbacks
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
#include <vlc_configuration.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include <vlc_atomic.h>

#define COBJMACROS
#include <initguid.h>
#include <d3d11.h>

#include "d3d11_filters.h"
#include "../../video_chroma/d3d11_fmt.h"

#ifdef __MINGW32__
#define D3D11_VIDEO_PROCESSOR_FILTER_CAPS_BRIGHTNESS   0x1
#define D3D11_VIDEO_PROCESSOR_FILTER_CAPS_CONTRAST     0x2
#define D3D11_VIDEO_PROCESSOR_FILTER_CAPS_HUE          0x4
#define D3D11_VIDEO_PROCESSOR_FILTER_CAPS_SATURATION   0x8
#endif

#define PROCESSOR_SLICES 2

struct filter_level
{
    atomic_int   level;
    float  default_val;
    float  min;
    float  max;
    D3D11_VIDEO_PROCESSOR_FILTER_RANGE Range;
};

struct filter_sys_t
{
    float f_gamma;
    bool  b_brightness_threshold;

    struct filter_level Brightness;
    struct filter_level Contrast;
    struct filter_level Hue;
    struct filter_level Saturation;

    d3d11_device_t                 d3d_dev;
    ID3D11VideoDevice              *d3dviddev;
    ID3D11VideoContext             *d3dvidctx;
    ID3D11VideoProcessor           *videoProcessor;
    ID3D11VideoProcessorEnumerator *procEnumerator;

    HANDLE                         context_mutex;
    union {
        ID3D11Texture2D            *texture;
        ID3D11Resource             *resource;
    } out[PROCESSOR_SLICES];
    ID3D11VideoProcessorInputView  *procInput[PROCESSOR_SLICES];
    ID3D11VideoProcessorOutputView *procOutput[PROCESSOR_SLICES];
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

static int assert_ProcessorInput(filter_t *p_filter, picture_sys_t *p_sys_src)
{
    filter_sys_t *p_sys = p_filter->p_sys;
    if (!p_sys_src->processorInput)
    {
        D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inDesc = {
            .FourCC = 0,
            .ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D,
            .Texture2D.MipSlice = 0,
            .Texture2D.ArraySlice = p_sys_src->slice_index,
        };
        HRESULT hr;

        hr = ID3D11VideoDevice_CreateVideoProcessorInputView(p_sys->d3dviddev,
                                                             p_sys_src->resource[KNOWN_DXGI_INDEX],
                                                             p_sys->procEnumerator,
                                                             &inDesc,
                                                             &p_sys_src->processorInput);
        if (FAILED(hr))
        {
#ifndef NDEBUG
            msg_Dbg(p_filter,"Failed to create processor input for slice %d. (hr=0x%lX)", p_sys_src->slice_index, hr);
#endif
            return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
}

static bool ApplyFilter( filter_sys_t *p_sys,
                         D3D11_VIDEO_PROCESSOR_FILTER filter,
                         const struct filter_level *p_level,
                         ID3D11VideoProcessorInputView *input,
                         ID3D11VideoProcessorOutputView *output,
                         const video_format_t *fmt)
{
    HRESULT hr;

    int level = atomic_load(&p_level->level);
    if (level == p_level->Range.Default)
        return false;

    ID3D11VideoContext_VideoProcessorSetStreamFilter(p_sys->d3dvidctx,
                                                     p_sys->videoProcessor,
                                                     0,
                                                     filter,
                                                     TRUE,
                                                     level);

    RECT srcRect;
    srcRect.left   = fmt->i_x_offset;
    srcRect.top    = fmt->i_y_offset;
    srcRect.right  = srcRect.left + fmt->i_visible_width;
    srcRect.bottom = srcRect.top  + fmt->i_visible_height;
    ID3D11VideoContext_VideoProcessorSetStreamSourceRect(p_sys->d3dvidctx, p_sys->videoProcessor,
                                                         0, TRUE, &srcRect);
    ID3D11VideoContext_VideoProcessorSetStreamDestRect(p_sys->d3dvidctx, p_sys->videoProcessor,
                                                       0, TRUE, &srcRect);

    D3D11_VIDEO_PROCESSOR_STREAM stream = {0};
    stream.Enable = TRUE;
    stream.pInputSurface = input;

    hr = ID3D11VideoContext_VideoProcessorBlt(p_sys->d3dvidctx,
                                              p_sys->videoProcessor,
                                              output,
                                              0, 1, &stream);
    return SUCCEEDED(hr);
}

static void SetLevel(struct filter_level *range, float val)
{
    int level;
    if (val > range->default_val)
        level = (range->Range.Maximum - range->Range.Default) * (val - range->default_val) /
                (range->max - range->default_val);
    else if (val < range->default_val)
        level = (range->Range.Minimum - range->Range.Default) * (val - range->default_val) /
                (range->min - range->default_val);
    else
        level = 0;

    atomic_store( &range->level, range->Range.Default + level );
}

static void InitLevel(filter_t *filter, struct filter_level *range, const char *p_name, float def)
{
    int level = 0;

    module_config_t *cfg = config_FindConfig(p_name);
    if (unlikely(cfg == NULL))
    {
        range->min         = 0.;
        range->max         = 2.;
        range->default_val = 1.;
    }
    else
    {
        range->min         = cfg->min.f;
        range->max         = cfg->max.f;
        range->default_val = def;

        float val = var_CreateGetFloatCommand( filter, p_name );

        if (val > range->default_val)
            level = (range->Range.Maximum - range->Range.Default) * (val - range->default_val) /
                    (range->max - range->default_val);
        else if (val < range->default_val)
            level = (range->Range.Minimum - range->Range.Default) * (val - range->default_val) /
                    (range->min - range->default_val);
    }

    atomic_init( &range->level, range->Range.Default + level );
}

static picture_t *Filter(filter_t *p_filter, picture_t *p_pic)
{
    filter_sys_t *p_sys = p_filter->p_sys;

    picture_sys_t *p_src_sys = ActivePictureSys(p_pic);
    if ( assert_ProcessorInput(p_filter, ActivePictureSys(p_pic) ) )
    {
        picture_Release( p_pic );
        return NULL;
    }

    picture_t *p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }
    if (unlikely(!p_outpic->p_sys))
    {
        /* the output filter configuration may have changed since the filter
         * was opened */
        picture_Release( p_pic );
        return NULL;
    }

    picture_CopyProperties( p_outpic, p_pic );

    if( p_sys->context_mutex != INVALID_HANDLE_VALUE )
        WaitForSingleObjectEx( p_sys->context_mutex, INFINITE, FALSE );

    ID3D11VideoProcessorInputView *inputs[4] = {
        p_src_sys->processorInput,
        p_sys->procInput[0],
        p_sys->procInput[1],
        p_sys->procInput[0]
    };

    ID3D11VideoProcessorOutputView *outputs[4] = {
        p_sys->procOutput[0],
        p_sys->procOutput[1],
        p_sys->procOutput[0],
        p_sys->procOutput[1]
    };

    size_t idx = 0, count = 0;
    /* contrast */
    if ( ApplyFilter( p_sys,
                      D3D11_VIDEO_PROCESSOR_FILTER_CONTRAST, &p_sys->Contrast,
                      inputs[idx], outputs[idx], &p_filter->fmt_out.video ) )
    {
        idx++;
        count++;
    }
    /* brightness */
    if ( ApplyFilter( p_sys,
                      D3D11_VIDEO_PROCESSOR_FILTER_BRIGHTNESS, &p_sys->Brightness,
                      inputs[idx], outputs[idx], &p_filter->fmt_out.video ) )
    {
        idx++;
        count++;
    }
    /* hue */
    if ( ApplyFilter( p_sys,
                      D3D11_VIDEO_PROCESSOR_FILTER_HUE, &p_sys->Hue,
                      inputs[idx], outputs[idx], &p_filter->fmt_out.video ) )
    {
        idx++;
        count++;
    }
    /* saturation */
    if ( ApplyFilter( p_sys,
                      D3D11_VIDEO_PROCESSOR_FILTER_SATURATION, &p_sys->Saturation,
                      inputs[idx], outputs[idx], &p_filter->fmt_out.video ) )
    {
        idx++;
        count++;
    }

    if (count == 0)
    {
        ID3D11DeviceContext_CopySubresourceRegion(p_outpic->p_sys->context,
                                                  p_outpic->p_sys->resource[KNOWN_DXGI_INDEX],
                                                  p_outpic->p_sys->slice_index,
                                                  0, 0, 0,
                                                  p_src_sys->resource[KNOWN_DXGI_INDEX],
                                                  p_src_sys->slice_index,
                                                  NULL);
    }
    else
    {
        ID3D11DeviceContext_CopySubresourceRegion(p_outpic->p_sys->context,
                                                  p_outpic->p_sys->resource[KNOWN_DXGI_INDEX],
                                                  p_outpic->p_sys->slice_index,
                                                  0, 0, 0,
                                                  p_sys->out[outputs[idx] == p_sys->procOutput[0] ? 1 : 0].resource,
                                                  0,
                                                  NULL);
    }

    if( p_sys->context_mutex  != INVALID_HANDLE_VALUE )
        ReleaseMutex( p_sys->context_mutex );

    picture_Release( p_pic );
    return p_outpic;
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

static int D3D11OpenAdjust(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    HRESULT hr;
    ID3D11VideoProcessorEnumerator *processorEnumerator = NULL;

    if (!is_d3d11_opaque(filter->fmt_in.video.i_chroma))
        return VLC_EGENERIC;
    if (!video_format_IsSimilar(&filter->fmt_in.video, &filter->fmt_out.video))
        return VLC_EGENERIC;

    filter_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    memset(sys, 0, sizeof (*sys));

    D3D11_TEXTURE2D_DESC dstDesc;
    D3D11_FilterHoldInstance(filter, &sys->d3d_dev, &dstDesc);
    if (unlikely(sys->d3d_dev.d3dcontext==NULL))
    {
        msg_Dbg(filter, "Filter without a context");
        free(sys);
        return VLC_ENOOBJ;
    }

    hr = ID3D11Device_QueryInterface(sys->d3d_dev.d3ddevice, &IID_ID3D11VideoDevice, (void **)&sys->d3dviddev);
    if (FAILED(hr)) {
       msg_Err(filter, "Could not Query ID3D11VideoDevice Interface. (hr=0x%lX)", hr);
       goto error;
    }

    hr = ID3D11DeviceContext_QueryInterface(sys->d3d_dev.d3dcontext, &IID_ID3D11VideoContext, (void **)&sys->d3dvidctx);
    if (FAILED(hr)) {
       msg_Err(filter, "Could not Query ID3D11VideoContext Interface from the picture. (hr=0x%lX)", hr);
       goto error;
    }

    HANDLE context_lock = INVALID_HANDLE_VALUE;
    UINT dataSize = sizeof(context_lock);
    hr = ID3D11Device_GetPrivateData(sys->d3d_dev.d3ddevice, &GUID_CONTEXT_MUTEX, &dataSize, &context_lock);
    if (FAILED(hr))
        msg_Warn(filter, "No mutex found to lock the decoder");
    sys->context_mutex = context_lock;

    const video_format_t *fmt = &filter->fmt_out.video;

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC processorDesc = {
        .InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE,
        .InputFrameRate = {
            .Numerator   = fmt->i_frame_rate,
            .Denominator = fmt->i_frame_rate_base,
        },
        .InputWidth   = fmt->i_width,
        .InputHeight  = fmt->i_height,
        .OutputWidth  = dstDesc.Width,
        .OutputHeight = dstDesc.Height,
        .OutputFrameRate = {
            .Numerator   = fmt->i_frame_rate,
            .Denominator = fmt->i_frame_rate_base,
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
    D3D11_LogProcessorSupport(filter, processorEnumerator);
#endif
    hr = ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(processorEnumerator, dstDesc.Format, &flags);
    if (!SUCCEEDED(hr))
    {
        msg_Dbg(filter, "can't read processor support for %s", DxgiFormatToStr(dstDesc.Format));
        goto error;
    }
    if ( !(flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT) ||
         !(flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT) )
    {
        msg_Dbg(filter, "input/output %s is not supported", DxgiFormatToStr(dstDesc.Format));
        goto error;
    }

    D3D11_VIDEO_PROCESSOR_CAPS processorCaps;
    hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorCaps(processorEnumerator, &processorCaps);
    if (FAILED(hr))
        goto error;

    const UINT neededCaps = D3D11_VIDEO_PROCESSOR_FILTER_CAPS_BRIGHTNESS |
                            D3D11_VIDEO_PROCESSOR_FILTER_CAPS_CONTRAST |
                            D3D11_VIDEO_PROCESSOR_FILTER_CAPS_HUE |
                            D3D11_VIDEO_PROCESSOR_FILTER_CAPS_SATURATION;
    if ((processorCaps.FilterCaps & neededCaps) != neededCaps)
    {
        msg_Dbg(filter, "missing capabilities 0x%x", neededCaps - (processorCaps.FilterCaps & neededCaps));
        goto error;
    }

    hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorFilterRange(processorEnumerator,
                                                                     D3D11_VIDEO_PROCESSOR_FILTER_BRIGHTNESS,
                                                                     &sys->Brightness.Range);
    if (FAILED(hr))
        goto error;

    hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorFilterRange(processorEnumerator,
                                                                     D3D11_VIDEO_PROCESSOR_FILTER_CONTRAST,
                                                                     &sys->Contrast.Range);
    if (FAILED(hr))
        goto error;

    hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorFilterRange(processorEnumerator,
                                                                     D3D11_VIDEO_PROCESSOR_FILTER_HUE,
                                                                     &sys->Hue.Range);
    if (FAILED(hr))
        goto error;

    hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorFilterRange(processorEnumerator,
                                                                     D3D11_VIDEO_PROCESSOR_FILTER_SATURATION,
                                                                     &sys->Saturation.Range);
    if (FAILED(hr))
        goto error;

    /* needed to get options passed in transcode using the
     * adjust{name=value} syntax */
    config_ChainParse( filter, "", ppsz_filter_options, filter->p_cfg );

    InitLevel(filter, &sys->Contrast,   "contrast",   1.0 );
    InitLevel(filter, &sys->Brightness, "brightness", 1.0 );
    InitLevel(filter, &sys->Hue,        "hue",        0.0 );
    InitLevel(filter, &sys->Saturation, "saturation", 1.0 );
    sys->f_gamma = var_CreateGetFloatCommand( filter, "gamma" );
    sys->b_brightness_threshold =
        var_CreateGetBoolCommand( filter, "brightness-threshold" );

    var_AddCallback( filter, "contrast",   AdjustCallback, sys );
    var_AddCallback( filter, "brightness", AdjustCallback, sys );
    var_AddCallback( filter, "hue",        AdjustCallback, sys );
    var_AddCallback( filter, "saturation", AdjustCallback, sys );
    var_AddCallback( filter, "gamma",      AdjustCallback, sys );
    var_AddCallback( filter, "brightness-threshold",
                                             AdjustCallback, sys );

    hr = ID3D11VideoDevice_CreateVideoProcessor(sys->d3dviddev,
                                                processorEnumerator, 0,
                                                &sys->videoProcessor);
    if (FAILED(hr) || sys->videoProcessor == NULL)
    {
        msg_Dbg(filter, "failed to create the processor");
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

    hr = ID3D11Device_CreateTexture2D( sys->d3d_dev.d3ddevice, &texDesc, NULL, &sys->out[0].texture );
    if (FAILED(hr)) {
        msg_Err(filter, "CreateTexture2D failed. (hr=0x%0lx)", hr);
        goto error;
    }
    hr = ID3D11Device_CreateTexture2D( sys->d3d_dev.d3ddevice, &texDesc, NULL, &sys->out[1].texture );
    if (FAILED(hr)) {
        ID3D11Texture2D_Release(sys->out[0].texture);
        msg_Err(filter, "CreateTexture2D failed. (hr=0x%0lx)", hr);
        goto error;
    }

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outDesc = {
        .ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2DARRAY,
        .Texture2DArray.MipSlice = 0,
        .Texture2DArray.ArraySize = 1,
    };

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inDesc = {
        .FourCC = 0,
        .ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D,
        .Texture2D.MipSlice = 0,
    };

    for (int i=0; i<PROCESSOR_SLICES; i++)
    {
        hr = ID3D11VideoDevice_CreateVideoProcessorOutputView(sys->d3dviddev,
                                                             sys->out[i].resource,
                                                             processorEnumerator,
                                                             &outDesc,
                                                             &sys->procOutput[i]);
        if (FAILED(hr))
        {
            msg_Dbg(filter,"Failed to create processor output. (hr=0x%lX)", hr);
            goto error;
        }

        hr = ID3D11VideoDevice_CreateVideoProcessorInputView(sys->d3dviddev,
                                                             sys->out[0].resource,
                                                             processorEnumerator,
                                                             &inDesc,
                                                             &sys->procInput[i]);

        if (FAILED(hr))
        {
            msg_Dbg(filter,"Failed to create processor input. (hr=0x%lX)", hr);
            goto error;
        }
    }

    sys->procEnumerator  = processorEnumerator;

    filter->pf_video_filter = Filter;
    filter->p_sys = sys;

    return VLC_SUCCESS;
error:
    for (int i=0; i<PROCESSOR_SLICES; i++)
    {
        if (sys->procInput[i])
            ID3D11VideoProcessorInputView_Release(sys->procInput[i]);
        if (sys->procOutput[i])
            ID3D11VideoProcessorOutputView_Release(sys->procOutput[i]);
    }

    if (sys->out[0].texture)
        ID3D11Texture2D_Release(sys->out[0].texture);
    if (sys->out[1].texture)
        ID3D11Texture2D_Release(sys->out[1].texture);
    if (sys->videoProcessor)
        ID3D11VideoProcessor_Release(sys->videoProcessor);
    if (processorEnumerator)
        ID3D11VideoProcessorEnumerator_Release(processorEnumerator);
    if (sys->d3dvidctx)
        ID3D11VideoContext_Release(sys->d3dvidctx);
    if (sys->d3dviddev)
        ID3D11VideoDevice_Release(sys->d3dviddev);
    if (sys->d3d_dev.d3dcontext)
        D3D11_FilterReleaseInstance(&sys->d3d_dev);
    free(sys);

    return VLC_EGENERIC;
}

static void D3D11CloseAdjust(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    filter_sys_t *sys = filter->p_sys;

    var_DelCallback( filter, "contrast",   AdjustCallback, sys );
    var_DelCallback( filter, "brightness", AdjustCallback, sys );
    var_DelCallback( filter, "hue",        AdjustCallback, sys );
    var_DelCallback( filter, "saturation", AdjustCallback, sys );
    var_DelCallback( filter, "gamma",      AdjustCallback, sys );
    var_DelCallback( filter, "brightness-threshold",
                                             AdjustCallback, sys );

    for (int i=0; i<PROCESSOR_SLICES; i++)
    {
        ID3D11VideoProcessorInputView_Release(sys->procInput[i]);
        ID3D11VideoProcessorOutputView_Release(sys->procOutput[i]);
    }
    ID3D11Texture2D_Release(sys->out[0].texture);
    ID3D11Texture2D_Release(sys->out[1].texture);
    ID3D11VideoProcessor_Release(sys->videoProcessor);
    ID3D11VideoProcessorEnumerator_Release(sys->procEnumerator);
    ID3D11VideoContext_Release(sys->d3dvidctx);
    ID3D11VideoDevice_Release(sys->d3dviddev);

    D3D11_FilterReleaseInstance(&sys->d3d_dev);

    free(sys);
}

vlc_module_begin()
    set_description("Direct3D11 adjust filter")
    set_capability("video filter", 0)
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    set_callbacks(D3D11OpenAdjust, D3D11CloseAdjust)
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
    set_description("Direct3D11 deinterlace filter")
    set_callbacks( D3D11OpenDeinterlace, D3D11CloseDeinterlace )
    add_shortcut ("deinterlace")

    add_submodule()
    set_capability( "video converter", 10 )
    set_callbacks( D3D11OpenConverter, D3D11CloseConverter )

    add_submodule()
    set_callbacks( D3D11OpenCPUConverter, D3D11CloseCPUConverter )
    set_capability( "video converter", 10 )

vlc_module_end()
