// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * amf_vqenhancer: Enhance video with low bitrates
 *****************************************************************************
 * Copyright © 2024 Videolabs, VLC authors and VideoLAN
 *
 * Authors: Steve Lhomme <robux4@videolabs.io>
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_filter.h>
#include <vlc_plugin.h>

#define COBJMACROS
#include "../hw/amf/amf_helper.h"
#include <AMF/components/VQEnhancer.h>

#include "../video_chroma/d3d11_fmt.h"
#include "../hw/d3d11/d3d11_filters.h"

#include <assert.h>

struct filter_sys_t
{
    d3d11_handle_t                 hd3d;
    d3d11_device_t                 d3d_dev;
    struct vlc_amf_context         amf;
    AMFComponent                   *amf_vqe;
    AMFSurface                     *amfInput;
    const d3d_format_t             *cfg;
};

static picture_t *PictureFromTexture(filter_t *filter, d3d11_device_t *d3d_dev, ID3D11Texture2D *out)
{
    struct filter_sys_t *sys = filter->p_sys;

    picture_t *p_outpic = filter_NewPicture( filter );
    if( !p_outpic )
    {
        return NULL;
    }
    picture_sys_t *src_sys = ActivePictureSys(p_outpic);
    if (unlikely(!src_sys))
    {
        /* the output filter configuration may have changed since the filter
         * was opened */
        picture_Release(p_outpic);
        return NULL;
    }

    d3d11_device_lock( d3d_dev );
    ID3D11DeviceContext_CopySubresourceRegion(src_sys->context,
                                              src_sys->resource[KNOWN_DXGI_INDEX],
                                              src_sys->slice_index,
                                              0, 0, 0,
                                              (ID3D11Resource*)out,
                                              0,
                                              NULL);
    d3d11_device_unlock( d3d_dev );
    return p_outpic;
}

static picture_t * Filter(filter_t *filter, picture_t *p_pic)
{
    struct filter_sys_t *sys = filter->p_sys;

    picture_sys_t *src_sys = ActivePictureSys(p_pic);

    AMF_RESULT res;
    AMFSurface *submitSurface;

    AMFPlane *packedStaging = sys->amfInput->pVtbl->GetPlane(sys->amfInput, AMF_PLANE_PACKED);
    ID3D11Resource *amfStaging = packedStaging->pVtbl->GetNative(packedStaging);

#ifndef NDEBUG
    ID3D11Texture2D *staging = (ID3D11Texture2D *)amfStaging;
    D3D11_TEXTURE2D_DESC stagingDesc, inputDesc;
    ID3D11Texture2D_GetDesc(staging, &stagingDesc);
    ID3D11Texture2D_GetDesc(src_sys->texture[KNOWN_DXGI_INDEX], &inputDesc);
    assert(stagingDesc.Width == inputDesc.Width);
    assert(stagingDesc.Height == inputDesc.Height);
    assert(stagingDesc.Format == inputDesc.Format);
#endif

    struct filter_sys_t *dev_sys = sys;

#if 0
    if (src_sys->slice_index == 0)
    sys->amf.Context->pVtbl->CreateSurfaceFromDX11Native(sys->amf.Context, )
#endif
    // copy source into staging as it may not be shared and we can't select a slice
    d3d11_device_lock( &dev_sys->d3d_dev );
    ID3D11DeviceContext_CopySubresourceRegion(src_sys->context, amfStaging,
                                            0,
                                            0, 0, 0,
                                            src_sys->resource[KNOWN_DXGI_INDEX],
                                            src_sys->slice_index,
                                            NULL);
    d3d11_device_unlock( &dev_sys->d3d_dev );
    submitSurface = sys->amfInput;

    res = sys->amf_vqe->pVtbl->SubmitInput(sys->amf_vqe, (AMFData*)submitSurface);
    if (res == AMF_INPUT_FULL)
    {
        msg_Dbg(filter, "filter input full, skip this frame");
        return p_pic;
    }
    if (res != AMF_OK)
    {
        msg_Err(filter, "filter input failed (err=%d)", res);
        return p_pic;
    }

    AMFData *amfOutput = NULL;
    res = sys->amf_vqe->pVtbl->QueryOutput(sys->amf_vqe, &amfOutput);
    if (res != AMF_OK)
    {
        msg_Err(filter, "filter gave no output (err=%d)", res);
        return p_pic;
    }

    AMFSurface *amfOutputSurface = (AMFSurface*)amfOutput;
    AMFPlane *packed = amfOutputSurface->pVtbl->GetPlane(amfOutputSurface, AMF_PLANE_PACKED);

    assert(amfOutput->pVtbl->GetMemoryType(amfOutput) == AMF_MEMORY_DX11);
    ID3D11Texture2D *out = packed->pVtbl->GetNative(packed);
    picture_t *dst = PictureFromTexture(filter, &dev_sys->d3d_dev, out);
    amfOutput->pVtbl->Release(amfOutput);
    if (dst == NULL)
        return p_pic;

    picture_CopyProperties(dst, p_pic);
    picture_Release(p_pic);
    return dst;
}

void D3D11CloseAMFVQE(vlc_object_t *p_this)
{
    filter_t *filter = container_of(p_this, filter_t, obj);
    struct filter_sys_t *sys = filter->p_sys;
    sys->amfInput->pVtbl->Release(sys->amfInput);
    sys->amf_vqe->pVtbl->Release(sys->amf_vqe);
    if (sys->d3d_dev.d3dcontext)
        D3D11_FilterReleaseInstance(&sys->d3d_dev);
    D3D11_Destroy(&sys->hd3d);
}

int D3D11CreateAMFVQE(vlc_object_t *p_this)
{
    filter_t *filter = container_of(p_this, filter_t, obj);
    if (!is_d3d11_opaque(filter->fmt_in.video.i_chroma))
        return VLC_EGENERIC;
    if (!video_format_IsSimilar(&filter->fmt_in.video, &filter->fmt_out.video))
        return VLC_EGENERIC;

    struct filter_sys_t *sys = vlc_obj_calloc(VLC_OBJECT(filter), 1, sizeof(*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    if ( unlikely(D3D11_Create(filter, &sys->hd3d, false) != VLC_SUCCESS ))
    {
       msg_Err(filter, "Could not access the d3d11.");
       goto error;
    }

    D3D11_TEXTURE2D_DESC dstDesc;
    D3D11_FilterHoldInstance(filter, &sys->d3d_dev, &dstDesc);
    if (unlikely(sys->d3d_dev.d3dcontext==NULL))
    {
        msg_Dbg(filter, "Filter without a context");
        return VLC_ENOOBJ;
    }

    DXGI_FORMAT input_format = dstDesc.Format;
    const d3d_format_t *cfg;
    for (cfg = GetRenderFormatList(); cfg->name != NULL; ++cfg)
    {
        if (cfg->formatTexture == input_format &&
            is_d3d11_opaque(cfg->fourcc))
            break;
    }
    assert(cfg != NULL);

    AMF_SURFACE_FORMAT amf_fmt = DXGIToAMF(input_format);
    if (amf_fmt == AMF_SURFACE_UNKNOWN)
    {
        msg_Err(filter, "Unsupported DXGI format %s", cfg->name);
        return VLC_EGENERIC;
    }

    struct filter_sys_t *dev_sys = sys;

    HRESULT hr;
    HANDLE context_lock = INVALID_HANDLE_VALUE;
    UINT dataSize = sizeof(context_lock);
    hr = ID3D11DeviceContext_GetPrivateData(sys->d3d_dev.d3dcontext, &GUID_CONTEXT_MUTEX, &dataSize, &context_lock);
    if (FAILED(hr))
        msg_Warn(filter, "No mutex found to lock the decoder");
    sys->d3d_dev.context_mutex = context_lock;

    int err = vlc_AMFCreateContext(&sys->amf);
    if (err != VLC_SUCCESS)
        return err;

    AMF_RESULT res;
    res = sys->amf.Context->pVtbl->InitDX11(sys->amf.Context, dev_sys->d3d_dev.d3ddevice, AMF_DX11_0);
    if (res != AMF_OK)
        goto error;

    res = sys->amf.pFactory->pVtbl->CreateComponent(sys->amf.pFactory, sys->amf.Context, AMFVQEnhancer, &sys->amf_vqe);
    if (res != AMF_OK || sys->amf_vqe == NULL)
        goto error;

#if 0
    AMFVariantStruct val;
    val.int64Value = AMF_MEMORY_DX11;
    val.type = AMF_VARIANT_INT64;
    res = sys->amf_vqe->pVtbl->SetProperty(sys->amf_vqe, AMF_VIDEO_ENHANCER_ENGINE_TYPE, val);
    if (res != AMF_OK)
        goto error;

#if 0 // this parameter doesn't exist
    val.sizeValue = AMFConstructSize(filter->fmt_in.video.i_width, filter->fmt_in.video.i_height);
    val.type = AMF_VARIANT_SIZE;
    res = sys->amf_vqe->pVtbl->SetProperty(sys->amf_vqe, AMF_VIDEO_ENHANCER_OUTPUT_SIZE, val);
    if (res != AMF_OK)
        goto error;
#endif

#if 0 // debug test
    val.boolValue = 1;
    val.type = AMF_VARIANT_BOOL;
    res = sys->amf_vqe->pVtbl->SetProperty(sys->amf_vqe, AMF_VE_FCR_SPLIT_VIEW, val);
    if (res != AMF_OK)
        goto error;
#endif
#endif

    res = sys->amf_vqe->pVtbl->Init(sys->amf_vqe, amf_fmt,
                                    filter->fmt_in.video.i_width,
                                    filter->fmt_in.video.i_height);
    if (res != AMF_OK)
        goto error;

    res = sys->amf.Context->pVtbl->AllocSurface(sys->amf.Context, AMF_MEMORY_DX11,
                                                amf_fmt,
                                                filter->fmt_in.video.i_width,
                                                filter->fmt_in.video.i_height,
                                                &sys->amfInput);
    if (res != AMF_OK)
        goto error;

    sys->cfg = cfg;
    filter->pf_video_filter = Filter;
    filter->p_sys = sys;

    return VLC_SUCCESS;
error:
    if (sys->d3d_dev.d3dcontext)
        D3D11_FilterReleaseInstance(&sys->d3d_dev);
    if (sys->amfInput)
        sys->amfInput->pVtbl->Release(sys->amfInput);
    if (sys->amf_vqe != NULL)
        sys->amf_vqe->pVtbl->Release(sys->amf_vqe);
    vlc_AMFReleaseContext(&sys->amf);
    D3D11_Destroy(&sys->hd3d);
    return VLC_EGENERIC;
}
