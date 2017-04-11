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
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>

#define COBJMACROS
#include <initguid.h>
#include <d3d9.h>
#include <dxva2api.h>

struct picture_sys_t
{
    LPDIRECT3DSURFACE9 surface;
};

struct filter_sys_t
{
    HINSTANCE                      hdecoder_dll;
    /* keep a reference in case the vout is released first */
    HINSTANCE                      d3d9_dll;
    IDirect3DDevice9               *d3ddev;
    IDirectXVideoProcessor         *processor;
    IDirect3DSurface9              *hw_surface;
};

static picture_t *Deinterlace(filter_t *filter, picture_t *src)
{
    filter_sys_t *sys = filter->p_sys;
    HRESULT hr;
    DXVA2_VideoProcessBltParams params = {0};
    DXVA2_VideoSample sample = {0};
    D3DSURFACE_DESC dstDesc;

    hr = IDirect3DSurface9_GetDesc( src->p_sys->surface, &dstDesc );
    if (FAILED(hr))
        return src; /* cannot deinterlace without copying fields */

    picture_t *dst = filter_NewPicture(filter);
    if (dst == NULL)
        return src; /* cannot deinterlace without copying fields */

    sample.SrcSurface = src->p_sys->surface;
    sample.SampleFormat.SampleFormat = src->b_top_field_first ?
                DXVA2_SampleFieldInterleavedEvenFirst :
                DXVA2_SampleFieldInterleavedOddFirst;
    sample.Start = 0;
    sample.End = sample.Start + 333666;
    sample.SampleData = DXVA2_SampleData_RFF_TFF_Present;
    if (src->b_top_field_first)
        sample.SampleData |= DXVA2_SampleData_TFF;
    sample.SrcRect.bottom = dstDesc.Height;
    sample.SrcRect.right  = dstDesc.Width;
    sample.DstRect        = sample.SrcRect;
    sample.PlanarAlpha    = DXVA2_Fixed32OpaqueAlpha();

    params.TargetFrame = sample.Start;
    params.TargetRect  = sample.DstRect;
    params.DestData    = sample.SampleData;
    params.Alpha       = DXVA2_Fixed32OpaqueAlpha();
    params.DestFormat.SampleFormat = DXVA2_SampleProgressiveFrame;
    params.BackgroundColor.Alpha = 0xFFFF;

    hr = IDirectXVideoProcessor_VideoProcessBlt( sys->processor,
                                                 sys->hw_surface,
                                                 &params,
                                                 &sample,
                                                 1, NULL );
    if (FAILED(hr))
        goto error;

    hr = IDirect3DDevice9_StretchRect( sys->d3ddev,
                                       sys->hw_surface, NULL,
                                       dst->p_sys->surface, NULL,
                                       D3DTEXF_NONE);
    if (FAILED(hr))
        goto error;

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
    filter_sys_t *sys = NULL;
    HINSTANCE hdecoder_dll = NULL;
    HINSTANCE d3d9_dll = NULL;
    HRESULT hr;
    picture_t *dst = NULL;
    GUID *processorGUIDs = NULL;
    GUID *processorGUID = NULL;
    IDirectXVideoProcessorService *processor = NULL;

    if (filter->fmt_in.video.i_chroma != VLC_CODEC_D3D9_OPAQUE
     && filter->fmt_in.video.i_chroma != VLC_CODEC_D3D9_OPAQUE_10B)
        return VLC_EGENERIC;
    if (!video_format_IsSimilar(&filter->fmt_in.video, &filter->fmt_out.video))
        return VLC_EGENERIC;

    d3d9_dll = LoadLibrary(TEXT("D3D9.DLL"));
    if (!d3d9_dll)
        goto error;

    hdecoder_dll = LoadLibrary(TEXT("DXVA2.DLL"));
    if (!hdecoder_dll)
        goto error;

    dst = filter_NewPicture(filter);
    if (dst == NULL)
        goto error;

    sys = calloc(1, sizeof (*sys));
    if (unlikely(sys == NULL))
        goto error;

    HRESULT (WINAPI *CreateVideoService)(IDirect3DDevice9 *,
                                         REFIID riid,
                                         void **ppService);
    CreateVideoService =
      (void *)GetProcAddress(hdecoder_dll, "DXVA2CreateVideoService");
    if (CreateVideoService == NULL)
        goto error;

    hr = IDirect3DSurface9_GetDevice( dst->p_sys->surface, &sys->d3ddev );
    if (FAILED(hr))
        goto error;

    D3DSURFACE_DESC dstDesc;
    hr = IDirect3DSurface9_GetDesc( dst->p_sys->surface, &dstDesc );
    if (FAILED(hr))
        goto error;

    hr = CreateVideoService( sys->d3ddev, &IID_IDirectXVideoProcessorService,
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
    pFormat->SampleFormat = dst->b_top_field_first ?
                DXVA2_SampleFieldInterleavedEvenFirst :
                DXVA2_SampleFieldInterleavedOddFirst;

    UINT count = 0;
    hr = IDirectXVideoProcessorService_GetVideoProcessorDeviceGuids( processor,
                                                                &dsc,
                                                                &count,
                                                                &processorGUIDs);
    if (FAILED(hr))
        goto error;

    DXVA2_VideoProcessorCaps caps;
    for (UINT i=0; i<count && processorGUID==NULL; ++i) {
        hr = IDirectXVideoProcessorService_GetVideoProcessorCaps( processor,
                                                                  processorGUIDs+i,
                                                                  &dsc,
                                                                  dsc.Format,
                                                                  &caps);
        if ( SUCCEEDED(hr) && caps.DeinterlaceTechnology &&
             !caps.NumForwardRefSamples && !caps.NumBackwardRefSamples )
            processorGUID = processorGUIDs + i;
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

    CoTaskMemFree(processorGUIDs);
    picture_Release(dst);
    IDirectXVideoProcessorService_Release(processor);

    sys->hdecoder_dll = hdecoder_dll;
    sys->d3d9_dll     = d3d9_dll;
    filter->pf_video_filter = Deinterlace;
    filter->p_sys = sys;

    return VLC_SUCCESS;
error:
    CoTaskMemFree(processorGUIDs);
    if (sys && sys->processor)
        IDirectXVideoProcessor_Release( sys->processor );
    if (processor)
        IDirectXVideoProcessorService_Release(processor);
    if (sys && sys->d3ddev)
        IDirect3DDevice9_Release( sys->d3ddev );
    if (hdecoder_dll)
        FreeLibrary(hdecoder_dll);
    if (d3d9_dll)
        FreeLibrary(d3d9_dll);
    if (dst)
        picture_Release(dst);
    free(sys);

    return VLC_EGENERIC;
}

static void Close(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    filter_sys_t *sys = filter->p_sys;

    IDirect3DSurface9_Release( sys->hw_surface );
    IDirectXVideoProcessor_Release( sys->processor );
    IDirect3DDevice9_Release( sys->d3ddev );
    FreeLibrary( sys->hdecoder_dll );
    FreeLibrary( sys->d3d9_dll );

    free(sys);
}

vlc_module_begin()
    set_description(N_("DXVA2 deinterlacing filter"))
    set_capability("video filter", 0)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_callbacks(Open, Close)
    add_shortcut ("deinterlace")
vlc_module_end()
