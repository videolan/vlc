// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * amf_frc: Frame Rate doubler video with low frame rate
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
#include <vlc_configuration.h>

#define COBJMACROS
#include "../hw/amf/amf_helper.h"
#include <AMF/components/FRC.h>

#include "../video_chroma/d3d11_fmt.h"
#include "../hw/d3d11/d3d11_filters.h"

#include <assert.h>

static GUID  AMFVLCTextureArrayIndexGUID = { 0x28115527, 0xe7c3, 0x4b66, {0x99, 0xd3, 0x4f, 0x2a, 0xe6, 0xb4, 0x7f, 0xaf} };

static const char *const ppsz_filter_options[] = {
    "frc-indicator", NULL
};

struct filter_sys_t
{
    d3d11_handle_t                 hd3d;
    d3d11_device_t                 d3d_dev;

    struct vlc_amf_context         amf;
    AMFComponent                   *amf_frc;
    const d3d_format_t             *cfg;

    enum AMF_FRC_MODE_TYPE         mode;
    bool                           source_rate;
    date_t                         next_output_pts;
};

static picture_t *PictureFromTexture(filter_t *filter, d3d11_device_t *d3d_dev, ID3D11Texture2D *out)
{
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
    AMFSurface *submitSurface = NULL;

    struct filter_sys_t *dev_sys = sys;

    res = sys->amf.Context->pVtbl->CreateSurfaceFromDX11Native(sys->amf.Context, (void*)src_sys->resource[KNOWN_DXGI_INDEX], &submitSurface, NULL);
    if (res != AMF_OK)
    {
        msg_Err(filter, "filter surface allocation failed (err=%d)", res);
        if (submitSurface)
            submitSurface->pVtbl->Release(submitSurface);
        return p_pic;
    }
    amf_int subResourceIndex = src_sys->slice_index;
    ID3D11Resource_SetPrivateData(src_sys->resource[KNOWN_DXGI_INDEX], &AMFVLCTextureArrayIndexGUID, sizeof(subResourceIndex), &subResourceIndex);

    res = sys->amf_frc->pVtbl->SubmitInput(sys->amf_frc, (AMFData*)submitSurface);
    submitSurface->pVtbl->Release(submitSurface);
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

    date_Set( &sys->next_output_pts, p_pic->date );
    bool got_output = sys->mode != FRC_x2_PRESENT;
    do {
        AMFData *amfOutput = NULL;
        d3d11_device_lock( &dev_sys->d3d_dev ); // may consider to connect with AMFContext::LockDX11()/UnlockDX11()
        res = sys->amf_frc->pVtbl->QueryOutput(sys->amf_frc, &amfOutput);
        d3d11_device_unlock( &dev_sys->d3d_dev );
        if (res != AMF_OK && res != AMF_REPEAT)
        {
            msg_Err(filter, "filter gave no output (err=%d)", res);
            break;
        }

        AMFSurface *amfOutputSurface = (AMFSurface*)amfOutput;
        AMFPlane *packed = amfOutputSurface->pVtbl->GetPlane(amfOutputSurface, AMF_PLANE_PACKED);

        assert(amfOutput->pVtbl->GetMemoryType(amfOutput) == AMF_MEMORY_DX11);
        ID3D11Texture2D *out = packed->pVtbl->GetNative(packed);
        picture_t *dst = PictureFromTexture(filter, &dev_sys->d3d_dev, out);
        amfOutput->pVtbl->Release(amfOutput);
        if (dst == NULL)
            break;

        picture_CopyProperties(dst, p_pic);
        if (!got_output)
        {
            picture_Release(p_pic);
            p_pic = dst;
            got_output = true;
        }
        else
        {
            if (sys->mode == FRC_x2_PRESENT)
            {
                // teh first frame is the interpolated one with the previous frame
                dst->date = date_Get( &sys->next_output_pts );
                p_pic->date = date_Decrement( &sys->next_output_pts, 1 );
                p_pic->p_next = dst;
            }
            else
            {
                // past interpolated then source
                dst->p_next = p_pic;
                dst->date = date_Decrement( &sys->next_output_pts, 1 );
                p_pic = dst;
            }
        }
    } while (res == AMF_REPEAT);

    return p_pic;
}

void D3D11CloseAMFFRC(vlc_object_t *p_this)
{
    filter_t *filter = container_of(p_this, filter_t, obj);
    struct filter_sys_t *sys = filter->p_sys;
    sys->amf_frc->pVtbl->Release(sys->amf_frc);
    if (sys->d3d_dev.d3dcontext)
        D3D11_FilterReleaseInstance(&sys->d3d_dev);
    D3D11_Destroy(&sys->hd3d);
}

int D3D11CreateAMFFRC(vlc_object_t *p_this)
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

    struct filter_sys_t *dev_sys = sys;

    if (dev_sys->d3d_dev.adapterDesc.VendorId != GPU_MANUFACTURER_AMD)
    {
        msg_Err(filter, "AMF filter only supported with AMD GPUs");
        D3D11_FilterReleaseInstance(&sys->d3d_dev);
        D3D11_Destroy(&sys->hd3d);
        return VLC_EGENERIC;
    }

    config_ChainParse( filter, "", ppsz_filter_options, filter->p_cfg );

    sys->mode = FRC_x2_PRESENT; //FRC_ONLY_INTERPOLATED;

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

    int err = vlc_AMFCreateContext(&sys->amf);
    if (err != VLC_SUCCESS)
        return err;

#if 0
    if (sys->amf.Version < AMF_MAKE_FULL_VERSION(1,4,34,0))
    {
        msg_Dbg(filter, "AMF version %u.%u.%u too old for FRC (1.4.34 needed)",
            (unsigned)AMF_GET_MAJOR_VERSION(sys->amf.Version),
            (unsigned)AMF_GET_MINOR_VERSION(sys->amf.Version),
            (unsigned)AMF_GET_SUBMINOR_VERSION(sys->amf.Version));
        goto error;
    }
#endif

    AMF_RESULT res;
    res = sys->amf.Context->pVtbl->InitDX11(sys->amf.Context, dev_sys->d3d_dev.d3ddevice, AMF_DX11_0);
    if (res != AMF_OK)
        goto error;

    res = sys->amf.pFactory->pVtbl->CreateComponent(sys->amf.pFactory, sys->amf.Context, AMFFRC, &sys->amf_frc);
    if (res != AMF_OK || sys->amf_frc == NULL)
        goto error;

// TODO AMF_STREAM_VIDEO_FRAME_RATE

    AMFVariantStruct val;
    val.int64Value = FRC_ENGINE_DX11;
    val.type = AMF_VARIANT_INT64;
    res = sys->amf_frc->pVtbl->SetProperty(sys->amf_frc, AMF_FRC_ENGINE_TYPE, val);
    if (unlikely(res != AMF_OK))
    {
        msg_Err(filter, "Failed to set D3D11 engine type (err=%d)", res);
        goto error;
    }

    val.boolValue = false;
    val.type = AMF_VARIANT_BOOL;
    res = sys->amf_frc->pVtbl->SetProperty(sys->amf_frc, AMF_FRC_ENABLE_FALLBACK, val);
    if (unlikely(res != AMF_OK))
    {
        msg_Err(filter, "Failed to disable fallback (err=%d)", res);
        goto error;
    }

    val.int64Value = FRC_PROFILE_HIGH;
    val.type = AMF_VARIANT_INT64;
    res = sys->amf_frc->pVtbl->SetProperty(sys->amf_frc, AMF_FRC_PROFILE, val);
    if (unlikely(res != AMF_OK))
    {
        msg_Err(filter, "Failed to set FRC profile to %" PRId64 " (err=%d)", val.int64Value, res);
        goto error;
    }

    val.int64Value = FRC_MV_SEARCH_NATIVE;
    val.type = AMF_VARIANT_INT64;
    res = sys->amf_frc->pVtbl->SetProperty(sys->amf_frc, AMF_FRC_MV_SEARCH_MODE, val);
    if (unlikely(res != AMF_OK))
    {
        msg_Err(filter, "Failed to set FRC mv search to %" PRId64 " (err=%d)", val.int64Value, res);
        goto error;
    }

    val.int64Value = sys->mode;
    val.type = AMF_VARIANT_INT64;
    res = sys->amf_frc->pVtbl->SetProperty(sys->amf_frc, AMF_FRC_MODE, val);
    if (unlikely(res != AMF_OK))
    {
        msg_Err(filter, "Failed to set FRC mode to %" PRId64 " (err=%d)", val.int64Value, res);
        goto error;
    }

    val.boolValue = var_GetBool (filter, "frc-indicator");
    val.type = AMF_VARIANT_BOOL;
    res = sys->amf_frc->pVtbl->SetProperty(sys->amf_frc, AMF_FRC_INDICATOR, val);
    if (res != AMF_OK)
        goto error;

    res = sys->amf_frc->pVtbl->Init(sys->amf_frc, amf_fmt,
                                    filter->fmt_in.video.i_width,
                                    filter->fmt_in.video.i_height);
    if (res != AMF_OK)
        goto error;

    sys->cfg = cfg;
    filter->pf_video_filter = Filter;
    filter->p_sys = sys;

    sys->source_rate = filter->fmt_out.video.i_frame_rate_base != 0 &&
                       filter->fmt_out.video.i_frame_rate != 0;

    if (!sys->source_rate)
    {
        msg_Warn( filter, "Missing frame rate, assuming 25fps source" );
        filter->fmt_out.video.i_frame_rate = 25;
        filter->fmt_out.video.i_frame_rate_base = 1;
    }

    filter->fmt_out.video.i_frame_rate = 2 * filter->fmt_out.video.i_frame_rate;

    date_Init( &sys->next_output_pts,
               filter->fmt_out.video.i_frame_rate, filter->fmt_out.video.i_frame_rate_base );

    return VLC_SUCCESS;
error:
    if (sys->d3d_dev.d3dcontext)
        D3D11_FilterReleaseInstance(&sys->d3d_dev);
    if (sys->amf_frc != NULL)
        sys->amf_frc->pVtbl->Release(sys->amf_frc);
    vlc_AMFReleaseContext(&sys->amf);
    D3D11_Destroy(&sys->hd3d);
    return VLC_EGENERIC;
}
