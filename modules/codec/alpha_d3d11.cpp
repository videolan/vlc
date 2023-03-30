// SPDX-License-Identifier: LGPL-2.1-or-later

// alpha_d3d11.cpp : helper to combine D3D11 planes to generate pictures with alpha
// Copyright © 2023 VideoLabs, VLC authors and VideoLAN

// Authors: Steve Lhomme <robux4@videolabs.io>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "alpha_combine.h"
#include "../video_chroma/d3d11_fmt.h"

struct d3d11_alpha_context
{
    d3d11_pic_context  ctx;
    picture_t          *opaque;
    picture_t          *alpha;
};

static void d3d11_alpha_pic_context_destroy(picture_context_t *ctx)
{
    d3d11_alpha_context *pic_ctx = container_of(ctx, d3d11_alpha_context, ctx);
    picture_Release(pic_ctx->opaque);
    picture_Release(pic_ctx->alpha);

    auto *picsys_out = &pic_ctx->ctx.picsys;
    // texture objects have been be released by their parents
    picsys_out->renderSrc[0] = nullptr;
    picsys_out->renderSrc[1] = nullptr;
    picsys_out->renderSrc[2] = nullptr;
    picsys_out->renderSrc[3] = nullptr;

    picsys_out->texture[0] = nullptr;
    picsys_out->texture[1] = nullptr;
    picsys_out->texture[2] = nullptr;
    picsys_out->texture[3] = nullptr;

    d3d11_pic_context_destroy(ctx);
}

static picture_context_t *d3d11_alpha_pic_context_copy(picture_context_t *)
{
    assert(!"unsupported yet!");
    return nullptr;
}

picture_t *CombineD3D11(decoder_t *dec, picture_t *opaque, picture_t *alpha, vlc_video_context *vctx)
{
    auto *out = decoder_NewPicture(dec);
    if (out == nullptr)
        return nullptr;

    d3d11_alpha_context *pic_ctx = static_cast<d3d11_alpha_context *>(calloc(1, sizeof(*pic_ctx)));
    if (unlikely(pic_ctx == NULL))
    {
        picture_Release(out);
        return nullptr;
    }
    pic_ctx->ctx.picsys.sharedHandle = INVALID_HANDLE_VALUE;
    pic_ctx->ctx.s.copy = d3d11_alpha_pic_context_copy;
    pic_ctx->ctx.s.destroy = d3d11_alpha_pic_context_destroy;
    pic_ctx->ctx.s.vctx = vlc_video_context_Hold(vctx);
    pic_ctx->opaque = picture_Hold(opaque);
    pic_ctx->alpha = picture_Hold(alpha);
    out->context = &pic_ctx->ctx.s;

    auto *picsys_out = &pic_ctx->ctx.picsys;
    auto *picsys_opaque = ActiveD3D11PictureSys(opaque);
    auto *picsys_alpha = ActiveD3D11PictureSys(alpha);

    picsys_out->renderSrc[0] = picsys_opaque->renderSrc[0]; // opaque Y
    picsys_out->renderSrc[1] = picsys_opaque->renderSrc[1]; // opaque UV
    picsys_out->renderSrc[2] = picsys_alpha->renderSrc[0];  // alpha Y
    picsys_out->renderSrc[3] = picsys_alpha->renderSrc[1];  // alpha UV

    picsys_out->texture[0] = picsys_opaque->texture[0];
    picsys_out->texture[1] = picsys_opaque->texture[1];
    picsys_out->texture[2] = picsys_alpha->texture[0];
    picsys_out->texture[3] = picsys_alpha->texture[1];

    return out;
}

int SetupD3D11(decoder_t *dec, vlc_video_context *vctx, vlc_video_context **vctx_out)
{
    auto *vctx_sys = GetD3D11ContextPrivate(vctx);
    assert(vctx_sys->secondary == DXGI_FORMAT_UNKNOWN);
    assert(*vctx_out == nullptr); // TODO handle multiple format updates

    auto *dec_dev = vlc_video_context_HoldDevice(vctx);
    *vctx_out = D3D11CreateVideoContext(dec_dev, vctx_sys->format, DXGI_FORMAT_NV12);
    vlc_decoder_device_Release(dec_dev);
    if (unlikely(*vctx_out == nullptr))
    {
        msg_Dbg(dec,"Failed to create output vctx.");
        return VLC_EGENERIC;
    }
    dec->fmt_out.video.i_chroma = dec->fmt_out.i_codec = VLC_CODEC_D3D11_OPAQUE_ALPHA;

    return VLC_SUCCESS;
}
