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

#include <assert.h>

static GUID  AMFVLCTextureArrayIndexGUID = { 0x28115527, 0xe7c3, 0x4b66, {0x99, 0xd3, 0x4f, 0x2a, 0xe6, 0xb4, 0x7f, 0xaf} };

static const char *const ppsz_filter_options[] = {
    "frc-indicator", NULL
};

static int D3D11CreateAMFFRC(filter_t *);

vlc_module_begin()
    set_description(N_("AMD Frame Rate Doubler"))
    add_shortcut("amf_frc")
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_callback_video_filter(D3D11CreateAMFFRC)

    add_bool( "frc-indicator", false, N_("Show indicator"), NULL )
vlc_module_end()


struct filter_sys_t
{
    struct vlc_amf_context         amf;
    AMFComponent                   *amf_frc;
    const d3d_format_t             *cfg;

    enum AMF_FRC_MODE_TYPE         mode;
    bool                           source_rate;
    date_t                         next_output_pts;
};

struct d3d11amf_pic_context
{
    struct d3d11_pic_context  ctx;
    AMFData                   *data;
};

#define D3D11AMF_PICCONTEXT_FROM_PICCTX(pic_ctx)  \
    container_of((pic_ctx), struct d3d11amf_pic_context, ctx.s)


static void d3d11amf_pic_context_destroy(picture_context_t *ctx)
{
    struct d3d11amf_pic_context *pic_ctx = D3D11AMF_PICCONTEXT_FROM_PICCTX(ctx);
    struct AMFData *data = pic_ctx->data;
    static_assert(offsetof(struct d3d11amf_pic_context, ctx.s) == 0,
        "Cast assumption failure");
    d3d11_pic_context_destroy(ctx);
    data->pVtbl->Release(data);
}

static picture_context_t *d3d11amf_pic_context_copy(picture_context_t *ctx)
{
    struct d3d11amf_pic_context *src_ctx = D3D11AMF_PICCONTEXT_FROM_PICCTX(ctx);
    struct d3d11amf_pic_context *pic_ctx = malloc(sizeof(*pic_ctx));
    if (unlikely(pic_ctx==NULL))
        return NULL;
    *pic_ctx = *src_ctx;
    vlc_video_context_Hold(pic_ctx->ctx.s.vctx);
    pic_ctx->data->pVtbl->Acquire(pic_ctx->data);
    for (int i=0;i<DXGI_MAX_SHADER_VIEW; i++)
    {
        pic_ctx->ctx.picsys.resource[i]  = src_ctx->ctx.picsys.resource[i];
        pic_ctx->ctx.picsys.renderSrc[i] = src_ctx->ctx.picsys.renderSrc[i];
    }
    AcquireD3D11PictureSys(&pic_ctx->ctx.picsys);
    return &pic_ctx->ctx.s;
}

static picture_t *PictureFromTexture(filter_t *filter, d3d11_device_t *d3d_dev, ID3D11Texture2D *out, AMFData* data)
{
    struct filter_sys_t *sys = filter->p_sys;

    struct d3d11amf_pic_context *pic_ctx = calloc(1, sizeof(*pic_ctx));
    if (unlikely(pic_ctx == NULL))
        return NULL;

    picture_resource_t dummy_res = { .p_sys = NULL };
    picture_t *p_dst = picture_NewFromResource(&filter->fmt_out.video, &dummy_res);
    if (p_dst == NULL) {
        msg_Err(filter, "Failed to map create the temporary picture.");
        goto done;
    }
    p_dst->context = &pic_ctx->ctx.s;

    D3D11_PictureAttach(p_dst, out, sys->cfg);

    if (unlikely(D3D11_AllocateResourceView(vlc_object_logger(filter), d3d_dev->d3ddevice, sys->cfg,
                                            pic_ctx->ctx.picsys.texture, 0, pic_ctx->ctx.picsys.renderSrc) != VLC_SUCCESS))
        goto done;

    pic_ctx->ctx.s = (picture_context_t) {
        d3d11amf_pic_context_destroy, d3d11amf_pic_context_copy,
        vlc_video_context_Hold(filter->vctx_out),
    };
    pic_ctx->ctx.picsys.sharedHandle = INVALID_HANDLE_VALUE;
    pic_ctx->data = data;

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
    AMFSurface *submitSurface = NULL;

    d3d11_decoder_device_t *dev_sys = GetD3D11OpaqueContext( filter->vctx_in );

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
        picture_t *dst = PictureFromTexture(filter, &dev_sys->d3d_dev, out, amfOutput);
        if (dst == NULL)
        {
            amfOutput->pVtbl->Release(amfOutput);
            break;
        }

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
                vlc_picture_chain_AppendChain(p_pic, dst);
            }
            else
            {
                // past interpolated then source
                vlc_picture_chain_AppendChain(dst, p_pic);
                dst->date = date_Decrement( &sys->next_output_pts, 1 );
                p_pic = dst;
            }
        }
    } while (res == AMF_REPEAT);

    return p_pic;
}

static void D3D11CloseAMFFRC(filter_t *filter)
{
    struct filter_sys_t *sys = filter->p_sys;
    sys->amf_frc->pVtbl->Release(sys->amf_frc);
    vlc_video_context_Release(filter->vctx_out);
}

static int D3D11CreateAMFFRC(filter_t *filter)
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

    config_ChainParse( filter, "", ppsz_filter_options, filter->p_cfg );

    struct filter_sys_t *sys = vlc_obj_calloc(VLC_OBJECT(filter), 1, sizeof(*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->mode = FRC_x2_PRESENT; //FRC_ONLY_INTERPOLATED;

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
    static const struct vlc_filter_operations filter_ops =
    {
        .filter_video = Filter,
        .close = D3D11CloseAMFFRC,
    };
    filter->ops = &filter_ops;
    filter->p_sys = sys;
    filter->vctx_out = vlc_video_context_Hold(filter->vctx_in);

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
    if (sys->amf_frc != NULL)
        sys->amf_frc->pVtbl->Release(sys->amf_frc);
    vlc_AMFReleaseContext(&sys->amf);
    return VLC_EGENERIC;
}
