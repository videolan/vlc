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

#include <assert.h>

struct filter_sys_t
{
    struct vlc_amf_context         amf;
    AMFComponent                   *amf_vqe;
    AMFSurface                     *amfInput;
    const d3d_format_t             *cfg;
};

static picture_t *PictureFromTexture(filter_t *filter, d3d11_device_t *d3d_dev, ID3D11Texture2D *out)
{
    struct filter_sys_t *sys = filter->p_sys;

    struct d3d11_pic_context *pic_ctx = calloc(1, sizeof(*pic_ctx));
    if (unlikely(pic_ctx == NULL))
        return NULL;

    picture_resource_t dummy_res = { .p_sys = NULL };
    picture_t *p_dst = picture_NewFromResource(&filter->fmt_out.video, &dummy_res);
    if (p_dst == NULL) {
        msg_Err(filter, "Failed to map create the temporary picture.");
        goto done;
    }
    p_dst->context = &pic_ctx->s;

    D3D11_PictureAttach(p_dst, out, sys->cfg);

    if (unlikely(D3D11_AllocateResourceView(vlc_object_logger(filter), d3d_dev->d3ddevice, sys->cfg,
                                            pic_ctx->picsys.texture, 0, pic_ctx->picsys.renderSrc) != VLC_SUCCESS))
        goto done;

    pic_ctx->s = (picture_context_t) {
        d3d11_pic_context_destroy, d3d11_pic_context_copy,
        vlc_video_context_Hold(filter->vctx_out),
    };
    pic_ctx->picsys.sharedHandle = INVALID_HANDLE_VALUE;

    return p_dst;
done:
    if (p_dst != NULL)
        picture_Release(p_dst);
    free(pic_ctx);
    return NULL;
}

static picture_t * Filter(filter_t *filter, picture_t *p_pic)
{
    struct filter_sys_t *sys = filter->p_sys;

    picture_sys_d3d11_t *src_sys = ActiveD3D11PictureSys(p_pic);

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

    d3d11_decoder_device_t *dev_sys = GetD3D11OpaqueContext( filter->vctx_in );

#if 0
    if (src_sys->slice_index == 0)
    sys->amf.Context->pVtbl->CreateSurfaceFromDX11Native(sys->amf.Context, )
#endif
    // copy source into staging as it may not be shared and we can't select a slice
    d3d11_device_lock( &dev_sys->d3d_dev );
    ID3D11DeviceContext_CopySubresourceRegion(dev_sys->d3d_dev.d3dcontext, amfStaging,
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

static void D3D11CloseAMFVQE(filter_t *filter)
{
    struct filter_sys_t *sys = filter->p_sys;
    sys->amfInput->pVtbl->Release(sys->amfInput);
    sys->amf_vqe->pVtbl->Release(sys->amf_vqe);
    vlc_video_context_Release(filter->vctx_out);
}

static int D3D11CreateAMFVQE(filter_t *filter)
{
    if (!is_d3d11_opaque(filter->fmt_in.video.i_chroma))
        return VLC_EGENERIC;
    if ( GetD3D11ContextPrivate(filter->vctx_in) == NULL )
        return VLC_EGENERIC;
    if (!video_format_IsSimilar(&filter->fmt_in.video, &filter->fmt_out.video))
        return VLC_EGENERIC;

    d3d11_decoder_device_t *dev_sys = GetD3D11OpaqueContext( filter->vctx_in );
    if (dev_sys->d3d_dev.adapterDesc.VendorId != GPU_MANUFACTURER_AMD)
    {
        msg_Err(filter, "AMF filter only supported with AMD GPUs");
        return VLC_ENOTSUP;
    }

    struct filter_sys_t *sys = vlc_obj_calloc(VLC_OBJECT(filter), 1, sizeof(*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    d3d11_video_context_t *vctx_sys = GetD3D11ContextPrivate( filter->vctx_in );
    DXGI_FORMAT input_format = vctx_sys->format;
    const d3d_format_t *cfg;
    for (cfg = DxgiGetRenderFormatList(); cfg->name != NULL; ++cfg)
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
    static const struct vlc_filter_operations filter_ops =
    {
        .filter_video = Filter,
        .close = D3D11CloseAMFVQE,
    };
    filter->ops = &filter_ops;
    filter->p_sys = sys;
    filter->vctx_out = vlc_video_context_Hold(filter->vctx_in);

    return VLC_SUCCESS;
error:
    if (sys->amfInput)
        sys->amfInput->pVtbl->Release(sys->amfInput);
    if (sys->amf_vqe != NULL)
        sys->amf_vqe->pVtbl->Release(sys->amf_vqe);
    vlc_AMFReleaseContext(&sys->amf);
    return VLC_EGENERIC;
}

vlc_module_begin()
    set_description(N_("AMD VQ Enhancer"))
    add_shortcut("amf_vqenhancer")
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_callback_video_filter(D3D11CreateAMFVQE)
vlc_module_end()
