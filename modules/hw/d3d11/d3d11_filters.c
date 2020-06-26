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

#include <stdatomic.h>
#include <stdlib.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_configuration.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include <vlc_codec.h>

#define COBJMACROS
#include <d3d11.h>

#include "d3d11_filters.h"
#include "d3d11_processor.h"
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

typedef struct
{
    float f_gamma;
    bool  b_brightness_threshold;

    struct filter_level Brightness;
    struct filter_level Contrast;
    struct filter_level Hue;
    struct filter_level Saturation;

    d3d11_device_t                 *d3d_dev;
    d3d11_processor_t              d3d_proc;

    union {
        ID3D11Texture2D            *texture;
        ID3D11Resource             *resource;
    } out[PROCESSOR_SLICES];
    ID3D11VideoProcessorInputView  *procInput[PROCESSOR_SLICES];
    ID3D11VideoProcessorOutputView *procOutput[PROCESSOR_SLICES];
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

static bool ApplyFilter( filter_sys_t *p_sys,
                         D3D11_VIDEO_PROCESSOR_FILTER filter,
                         struct filter_level *p_level,
                         ID3D11VideoProcessorInputView *input,
                         ID3D11VideoProcessorOutputView *output,
                         const video_format_t *fmt)
{
    HRESULT hr;

    int level = atomic_load(&p_level->level);
    if (level == p_level->Range.Default)
        return false;

    ID3D11VideoContext_VideoProcessorSetStreamFilter(p_sys->d3d_proc.d3dvidctx,
                                                     p_sys->d3d_proc.videoProcessor,
                                                     0,
                                                     filter,
                                                     TRUE,
                                                     level);
    ID3D11VideoContext_VideoProcessorSetStreamAutoProcessingMode(p_sys->d3d_proc.d3dvidctx,
                                                                 p_sys->d3d_proc.videoProcessor,
                                                                 0, FALSE);

    RECT srcRect;
    srcRect.left   = fmt->i_x_offset;
    srcRect.top    = fmt->i_y_offset;
    srcRect.right  = srcRect.left + fmt->i_visible_width;
    srcRect.bottom = srcRect.top  + fmt->i_visible_height;
    ID3D11VideoContext_VideoProcessorSetStreamSourceRect(p_sys->d3d_proc.d3dvidctx, p_sys->d3d_proc.videoProcessor,
                                                         0, TRUE, &srcRect);
    ID3D11VideoContext_VideoProcessorSetStreamDestRect(p_sys->d3d_proc.d3dvidctx, p_sys->d3d_proc.videoProcessor,
                                                       0, TRUE, &srcRect);

    D3D11_VIDEO_PROCESSOR_STREAM stream = {0};
    stream.Enable = TRUE;
    stream.pInputSurface = input;

    hr = ID3D11VideoContext_VideoProcessorBlt(p_sys->d3d_proc.d3dvidctx,
                                              p_sys->d3d_proc.videoProcessor,
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

static picture_t *AllocPicture( filter_t *p_filter )
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

static picture_t *Filter(filter_t *p_filter, picture_t *p_pic)
{
    filter_sys_t *p_sys = p_filter->p_sys;

    picture_sys_d3d11_t *p_src_sys = ActiveD3D11PictureSys(p_pic);
    if (FAILED( D3D11_Assert_ProcessorInput(p_filter, &p_sys->d3d_proc, p_src_sys) ))
    {
        picture_Release( p_pic );
        return NULL;
    }

    picture_t *p_outpic = AllocPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }
    picture_sys_d3d11_t *p_out_sys = ActiveD3D11PictureSys(p_outpic);
    if (unlikely(!p_out_sys))
    {
        /* the output filter configuration may have changed since the filter
         * was opened */
        picture_Release( p_pic );
        return NULL;
    }

    picture_CopyProperties( p_outpic, p_pic );

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

    d3d11_device_lock( p_sys->d3d_dev );

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
        ID3D11DeviceContext_CopySubresourceRegion(p_sys->d3d_dev->d3dcontext,
                                                  p_out_sys->resource[KNOWN_DXGI_INDEX],
                                                  p_out_sys->slice_index,
                                                  0, 0, 0,
                                                  p_src_sys->resource[KNOWN_DXGI_INDEX],
                                                  p_src_sys->slice_index,
                                                  NULL);
    }
    else
    {
        ID3D11DeviceContext_CopySubresourceRegion(p_sys->d3d_dev->d3dcontext,
                                                  p_out_sys->resource[KNOWN_DXGI_INDEX],
                                                  p_out_sys->slice_index,
                                                  0, 0, 0,
                                                  p_sys->out[outputs[idx] == p_sys->procOutput[0] ? 1 : 0].resource,
                                                  0,
                                                  NULL);
    }

    d3d11_device_unlock( p_sys->d3d_dev );

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

    if (!is_d3d11_opaque(filter->fmt_in.video.i_chroma))
        return VLC_EGENERIC;
    if ( GetD3D11ContextPrivate(filter->vctx_in) == NULL )
        return VLC_EGENERIC;
    if (!video_format_IsSimilar(&filter->fmt_in.video, &filter->fmt_out.video))
        return VLC_EGENERIC;

    filter_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    memset(sys, 0, sizeof (*sys));

    d3d11_video_context_t *vtcx_sys = GetD3D11ContextPrivate( filter->vctx_in );
    d3d11_decoder_device_t *dev_sys = GetD3D11OpaqueContext( filter->vctx_in );
    sys->d3d_dev = &dev_sys->d3d_dev;
    DXGI_FORMAT format = vtcx_sys->format;

    if (D3D11_CreateProcessor(filter, sys->d3d_dev, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE,
                              &filter->fmt_out.video, &filter->fmt_out.video, &sys->d3d_proc) != VLC_SUCCESS)
        goto error;

    UINT flags;
    hr = ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(sys->d3d_proc.procEnumerator, format, &flags);
    if (!SUCCEEDED(hr))
    {
        msg_Dbg(filter, "can't read processor support for %s", DxgiFormatToStr(format));
        goto error;
    }
    if ( !(flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT) ||
         !(flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT) )
    {
        msg_Dbg(filter, "input/output %s is not supported", DxgiFormatToStr(format));
        goto error;
    }

    D3D11_VIDEO_PROCESSOR_CAPS processorCaps;
    hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorCaps(sys->d3d_proc.procEnumerator, &processorCaps);
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

    hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorFilterRange(sys->d3d_proc.procEnumerator,
                                                                     D3D11_VIDEO_PROCESSOR_FILTER_BRIGHTNESS,
                                                                     &sys->Brightness.Range);
    if (FAILED(hr))
        goto error;

    hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorFilterRange(sys->d3d_proc.procEnumerator,
                                                                     D3D11_VIDEO_PROCESSOR_FILTER_CONTRAST,
                                                                     &sys->Contrast.Range);
    if (FAILED(hr))
        goto error;

    hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorFilterRange(sys->d3d_proc.procEnumerator,
                                                                     D3D11_VIDEO_PROCESSOR_FILTER_HUE,
                                                                     &sys->Hue.Range);
    if (FAILED(hr))
        goto error;

    hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorFilterRange(sys->d3d_proc.procEnumerator,
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

    hr = ID3D11VideoDevice_CreateVideoProcessor(sys->d3d_proc.d3dviddev,
                                                sys->d3d_proc.procEnumerator, 0,
                                                &sys->d3d_proc.videoProcessor);
    if (FAILED(hr) || sys->d3d_proc.videoProcessor == NULL)
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
    texDesc.Format = format;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.ArraySize = 1;
    texDesc.Height = filter->fmt_out.video.i_height;
    texDesc.Width  = filter->fmt_out.video.i_width;

    hr = ID3D11Device_CreateTexture2D( sys->d3d_dev->d3ddevice, &texDesc, NULL, &sys->out[0].texture );
    if (FAILED(hr)) {
        msg_Err(filter, "CreateTexture2D failed. (hr=0x%lX)", hr);
        goto error;
    }
    hr = ID3D11Device_CreateTexture2D( sys->d3d_dev->d3ddevice, &texDesc, NULL, &sys->out[1].texture );
    if (FAILED(hr)) {
        ID3D11Texture2D_Release(sys->out[0].texture);
        msg_Err(filter, "CreateTexture2D failed. (hr=0x%lX)", hr);
        goto error;
    }

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outDesc = {
        .ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D,
    };

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inDesc = {
        .FourCC = 0,
        .ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D,
        .Texture2D.MipSlice = 0,
    };

    for (int i=0; i<PROCESSOR_SLICES; i++)
    {
        hr = ID3D11VideoDevice_CreateVideoProcessorOutputView(sys->d3d_proc.d3dviddev,
                                                             sys->out[i].resource,
                                                             sys->d3d_proc.procEnumerator,
                                                             &outDesc,
                                                             &sys->procOutput[i]);
        if (FAILED(hr))
        {
            msg_Dbg(filter,"Failed to create processor output. (hr=0x%lX)", hr);
            goto error;
        }

        hr = ID3D11VideoDevice_CreateVideoProcessorInputView(sys->d3d_proc.d3dviddev,
                                                             sys->out[i].resource,
                                                             sys->d3d_proc.procEnumerator,
                                                             &inDesc,
                                                             &sys->procInput[i]);

        if (FAILED(hr))
        {
            msg_Dbg(filter,"Failed to create processor input. (hr=0x%lX)", hr);
            goto error;
        }
    }

    filter->pf_video_filter = Filter;
    filter->p_sys = sys;
    filter->vctx_out = vlc_video_context_Hold(filter->vctx_in);

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
    D3D11_ReleaseProcessor(&sys->d3d_proc);
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
        if (sys->procInput[i])
            ID3D11VideoProcessorInputView_Release(sys->procInput[i]);
        if (sys->procOutput[i])
            ID3D11VideoProcessorOutputView_Release(sys->procOutput[i]);
    }
    ID3D11Texture2D_Release(sys->out[0].texture);
    ID3D11Texture2D_Release(sys->out[1].texture);
    D3D11_ReleaseProcessor( &sys->d3d_proc );
    vlc_video_context_Release(filter->vctx_out);

    free(sys);
}

vlc_module_begin()
    set_description(N_("Direct3D11 adjust filter"))
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
    set_description(N_("Direct3D11 deinterlace filter"))
    set_callbacks( D3D11OpenDeinterlace, D3D11CloseDeinterlace )
    add_shortcut ("deinterlace")

    add_submodule()
    set_capability( "video converter", 10 )
    set_callbacks( D3D11OpenConverter, D3D11CloseConverter )

    add_submodule()
    set_callbacks( D3D11OpenCPUConverter, D3D11CloseCPUConverter )
    set_capability( "video converter", 10 )

    add_submodule()
    set_description(N_("Direct3D11"))
    set_callback_dec_device( D3D11OpenDecoderDeviceW8, 20 )

    add_submodule()
    set_description(N_("Direct3D11"))
    set_callback_dec_device( D3D11OpenDecoderDeviceAny, 8 )
#if VLC_WINSTORE_APP
    /* LEGACY, the d3dcontext and swapchain were given by the host app */
    add_integer("winrt-d3dcontext",    0x0, NULL, NULL, true) /* ID3D11DeviceContext* */
#endif /* VLC_WINSTORE_APP */
    add_shortcut ("d3d11")

vlc_module_end()
