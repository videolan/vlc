// SPDX-License-Identifier: LGPL-2.1-or-later

// vpx_alpha.c : pseudo-decoder for VP8/VP9 streams with alpha side-channel
// Copyright © 2023 VideoLabs, VLC authors and VideoLAN

// Authors: Steve Lhomme <robux4@videolabs.io>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_ancillary.h>
#include <vlc_modules.h>
#include <vlc_atomic.h>
#include <vlc_picture_pool.h>

#include "alpha_combine.h"

static int OpenDecoder(vlc_object_t *);
static void CloseDecoder(vlc_object_t *);

vlc_module_begin ()
    set_description(N_("VPx+alpha video decoder"))
    set_capability("video decoder", 150)
    set_callbacks(OpenDecoder, CloseDecoder)
    set_subcategory(SUBCAT_INPUT_VCODEC)
vlc_module_end ()

struct vp_decoder
{
    decoder_t           dec;
    es_format_t         fmt_in;
    es_format_t         fmt_out;
    vlc_picture_chain_t decoded;
};

typedef struct
{
    struct vp_decoder *opaque;
    struct vp_decoder *alpha;
    vlc_mutex_t       lock;
    vlc_video_context *vctx;
    picture_t *(*pf_combine)(decoder_t *, picture_t *opaque, picture_t *alpha, vlc_video_context *);

    picture_pool_t    *pool;
} vpx_alpha;


static vlc_decoder_device *GetDevice( decoder_t *dec )
{
    decoder_t *bdec = container_of(vlc_object_parent(dec), decoder_t, obj);
    vpx_alpha *p_sys = bdec->p_sys;
    vlc_mutex_lock(&p_sys->lock);
    vlc_decoder_device *res = decoder_GetDecoderDevice(bdec);
    vlc_mutex_unlock(&p_sys->lock);
    return res;
}

struct cpu_alpha_context
{
    picture_context_t  ctx;
    picture_t          *opaque;
    picture_t          *alpha;
};

static void cpu_alpha_destroy(picture_context_t *ctx)
{
    struct cpu_alpha_context *pctx = container_of(ctx, struct cpu_alpha_context, ctx);
    picture_Release(pctx->opaque);
    picture_Release(pctx->alpha);
    free(pctx);
}

static picture_context_t *cpu_alpha_copy(picture_context_t *src)
{
    struct cpu_alpha_context *pctx = container_of(src, struct cpu_alpha_context, ctx);
    struct cpu_alpha_context *alpha_ctx = calloc(1, sizeof(*alpha_ctx));
    if (unlikely(alpha_ctx == NULL))
        return NULL;
    alpha_ctx->ctx = *src;
    alpha_ctx->opaque = picture_Hold(pctx->opaque);
    alpha_ctx->alpha  = picture_Hold(pctx->alpha);
    return &alpha_ctx->ctx;
}

static picture_t *CombinePicturesCPU(decoder_t *bdec, picture_t *opaque, picture_t *alpha, vlc_video_context *vctx)
{
    assert(vctx == NULL); VLC_UNUSED(vctx);
    vpx_alpha *p_sys = bdec->p_sys;
    picture_t *out = picture_pool_Wait(p_sys->pool);
    if (out == NULL)
        return NULL;

    struct cpu_alpha_context *alpha_ctx = calloc(1, sizeof(*alpha_ctx));
    if (unlikely(alpha_ctx == NULL))
    {
        picture_Release(out);
        return NULL;
    }
    alpha_ctx->ctx = (picture_context_t) {
        cpu_alpha_destroy, cpu_alpha_copy, NULL
    };
    alpha_ctx->opaque = picture_Hold(opaque);
    alpha_ctx->alpha  = picture_Hold(alpha);
    out->context = &alpha_ctx->ctx;

    for (int i=0; i<opaque->i_planes; i++)
        out->p[i] = opaque->p[i];
    out->p[opaque->i_planes] = alpha->p[0];
    return out;
}

static int SetupCPU(decoder_t *bdec)
{
    vpx_alpha *p_sys = bdec->p_sys;
    picture_t *pics[4];
    size_t i=0;

    if (p_sys->pool)
    {
        picture_pool_Release(p_sys->pool);
        p_sys->pool = NULL;
    }

    for (; i<ARRAY_SIZE(pics); i++)
    {
        pics[i] = picture_NewFromResource(&bdec->fmt_out.video, &(picture_resource_t){0});
        if (pics[i] == NULL)
            goto error;
    }
    p_sys->pool = picture_pool_New(ARRAY_SIZE(pics), pics);
    if (p_sys->pool)
        return VLC_SUCCESS;

error:
    while (i-- > 0)
        picture_Release(pics[i]);
    return VLC_EGENERIC;
}

static picture_t *CombineKeepAlpha(decoder_t *bdec, picture_t *opaque, picture_t *alpha, vlc_video_context *vctx)
{
    VLC_UNUSED(bdec); VLC_UNUSED(opaque); VLC_UNUSED(vctx);
    return picture_Hold(alpha);
}

static picture_t *CombineKeepOpaque(decoder_t *bdec, picture_t *opaque, picture_t *alpha, vlc_video_context *vctx)
{
    VLC_UNUSED(bdec); VLC_UNUSED(alpha); VLC_UNUSED(vctx);
    return picture_Hold(opaque);
}

static int FormatUpdate( decoder_t *dec, vlc_video_context *vctx )
{
    decoder_t *bdec = container_of(vlc_object_parent(dec), decoder_t, obj);
    vpx_alpha *p_sys = bdec->p_sys;
    vlc_mutex_lock(&p_sys->lock);

    int res = VLC_SUCCESS;
    if (dec == &p_sys->alpha->dec)
    {
        if (es_format_IsSimilar(&p_sys->alpha->fmt_out, &dec->fmt_out))
            // nothing changed
            goto done;
        es_format_Clean(&p_sys->alpha->fmt_out);
        es_format_Copy(&p_sys->alpha->fmt_out, &dec->fmt_out);
        if (p_sys->opaque->dec.fmt_out.video.i_chroma == 0)
        {
            // not ready
            bdec->fmt_out.video.i_chroma = bdec->fmt_out.i_codec = dec->fmt_out.video.i_chroma;
            p_sys->pf_combine = CombineKeepAlpha;
            goto done;
        }
    }
    else
    {
        if (es_format_IsSimilar(&p_sys->opaque->fmt_out, &dec->fmt_out))
            // nothing changed
            goto done;
        es_format_Clean(&p_sys->opaque->fmt_out);
        es_format_Copy(&p_sys->opaque->fmt_out, &dec->fmt_out);
        if (p_sys->alpha->dec.fmt_out.video.i_chroma == 0)
        {
            // not ready
            bdec->fmt_out.video.i_chroma = bdec->fmt_out.i_codec = dec->fmt_out.video.i_chroma;
            p_sys->pf_combine = CombineKeepOpaque;
            goto done;
        }
    }
    es_format_Clean(&bdec->fmt_out);
    es_format_Copy(&bdec->fmt_out, &dec->fmt_out);

    switch (dec->fmt_out.video.i_chroma)
    {
        case VLC_CODEC_I420:
            // TODO support other formats
            bdec->fmt_out.video.i_chroma = bdec->fmt_out.i_codec = VLC_CODEC_YUV420A;
            res = SetupCPU(bdec);
            if (res == VLC_SUCCESS)
            {
                p_sys->pf_combine = CombinePicturesCPU;
            }
            break;
#ifdef _WIN32
        case VLC_CODEC_D3D11_OPAQUE:
            if (dec == &p_sys->alpha->dec)
            {
                switch (p_sys->opaque->dec.fmt_out.video.i_chroma)
                {
                    case VLC_CODEC_D3D11_OPAQUE:
                        res = SetupD3D11(bdec, vctx, &p_sys->vctx);
                        if (res == VLC_SUCCESS)
                        {
                            p_sys->pf_combine = CombineD3D11;
                            vctx = p_sys->vctx;
                        }
                        break;
                    default:
                        msg_Err(dec, "unsupported opaque D3D11 combination %4.4s", (char*)&p_sys->opaque->dec.fmt_out.video.i_chroma);
                        res = VLC_EGENERIC;
                }
            }
            else
            {
                switch (p_sys->alpha->dec.fmt_out.video.i_chroma)
                {
                    case VLC_CODEC_D3D11_OPAQUE:
                        res = SetupD3D11(bdec, vctx, &p_sys->vctx);
                        if (res == VLC_SUCCESS)
                        {
                            p_sys->pf_combine = CombineD3D11;
                            vctx = p_sys->vctx;
                        }
                        break;
                    default:
                        msg_Err(dec, "unsupported opaque D3D11 combination %4.4s", (char*)&p_sys->alpha->dec.fmt_out.video.i_chroma);
                        res = VLC_EGENERIC;
                }
            }
            break;
#endif // _WIN32
        default:
            msg_Err(dec, "unsupported decoder output %4.4s", (char*)&dec->fmt_out.video.i_chroma);
            res = VLC_EGENERIC;
            break;
    }
    if (res == VLC_SUCCESS)
        res = decoder_UpdateVideoOutput(bdec, vctx);

done:
    vlc_mutex_unlock(&p_sys->lock);
    return res;
}

static bool SendMergedLocked(decoder_t *bdec)
{
    vpx_alpha *p_sys = bdec->p_sys;

    picture_t *opaque = vlc_picture_chain_PeekFront(&p_sys->opaque->decoded);
    picture_t *alpha  = vlc_picture_chain_PeekFront(&p_sys->alpha->decoded);
    while (opaque != NULL && alpha != NULL)
    {
        if (opaque->date == alpha->date)
        {
            // dequeue if both first of the queue match DTS/PTS
            // merge alpha and opaque pictures with same DTS/PTS and send them
            picture_t *out = p_sys->pf_combine(bdec, opaque, alpha, p_sys->vctx);
            if (out != NULL)
            {
                video_format_CopyCropAr(&out->format, &opaque->format);
                picture_CopyProperties(out, opaque);
            }

            vlc_picture_chain_PopFront(&p_sys->opaque->decoded);
            picture_Release(opaque);

            vlc_picture_chain_PopFront(&p_sys->alpha->decoded);
            picture_Release(alpha);

            if (out == NULL)
                return false;

            decoder_QueueVideo(bdec, out);
            return true;
        }

        // in case decoders drop some frames
        if (opaque->date > alpha->date)
        {
            msg_Dbg(bdec, "missing decoded opaque at %" PRId64 " dropping alpha", alpha->date);
            vlc_picture_chain_PopFront(&p_sys->alpha->decoded);
            picture_Release(alpha);
        }
        else
        {
            msg_Dbg(bdec, "missing decoded alpha at %" PRId64 " dropping opaque", opaque->date);
            vlc_picture_chain_PopFront(&p_sys->opaque->decoded);
            picture_Release(opaque);
        }
        opaque = vlc_picture_chain_PeekFront(&p_sys->opaque->decoded);
        alpha  = vlc_picture_chain_PeekFront(&p_sys->alpha->decoded);
    }
    return false;
}

static void QueuePic( decoder_t *dec, picture_t *pic )
{
    decoder_t *bdec = container_of(vlc_object_parent(dec), decoder_t, obj);
    vpx_alpha *p_sys = bdec->p_sys;

    vlc_mutex_lock(&p_sys->lock);
    if (dec == &p_sys->alpha->dec)
    {
        vlc_picture_chain_Append(&p_sys->alpha->decoded, pic);
    }
    else
    {
        vlc_picture_chain_Append(&p_sys->opaque->decoded, pic);
    }

    SendMergedLocked(bdec);
    vlc_mutex_unlock(&p_sys->lock);
}

static vlc_tick_t GetDisplayDate( decoder_t *dec, vlc_tick_t sys_now, vlc_tick_t ts)
{
    decoder_t *bdec = container_of(vlc_object_parent(dec), decoder_t, obj);
    return decoder_GetDisplayDate(bdec, sys_now, ts);
}

static float GetDisplayRate( decoder_t *dec )
{
    decoder_t *bdec = container_of(vlc_object_parent(dec), decoder_t, obj);
    return decoder_GetDisplayRate(bdec);
}

static int GetAttachments( decoder_t *dec,
                            input_attachment_t ***ppp_attachment,
                            int *pi_attachment )
{
    decoder_t *bdec = container_of(vlc_object_parent(dec), decoder_t, obj);
    return decoder_GetInputAttachments(bdec, ppp_attachment, pi_attachment);
}

struct alpha_frame
{
    vlc_atomic_rc_t rc;
    vlc_frame_t     *frame; // source frame
    vlc_frame_t     opaque; // opaque bitstream
    vlc_frame_t     alpha;  // alpha bitstream
};

static void ReleaseAlphaFrame(vlc_frame_t *frame)
{
    struct alpha_frame *alpha_frame = container_of(frame, struct alpha_frame, alpha);

    if (vlc_atomic_rc_dec(&alpha_frame->rc))
    {
        vlc_frame_Release(alpha_frame->frame);
        free(alpha_frame);
    }
}

static void ReleaseOpaqueFrame(vlc_frame_t *frame)
{
    struct alpha_frame *alpha_frame = container_of(frame, struct alpha_frame, opaque);

    if (vlc_atomic_rc_dec(&alpha_frame->rc))
    {
        vlc_frame_Release(alpha_frame->frame);
        free(alpha_frame);
    }
}

static int Decode( decoder_t *dec, vlc_frame_t *frame )
{
    vpx_alpha *p_sys = dec->p_sys;

    int res;
    if (frame != NULL)
    {
        struct vlc_ancillary *p_alpha;
        p_alpha = vlc_frame_GetAncillary(frame, VLC_ANCILLARY_ID_VPX_ALPHA);
        if (p_alpha == NULL)
        {
            msg_Err(dec, "missing alpha data");
            return VLCDEC_ECRITICAL;
        }

        struct alpha_frame *alpha_frame = malloc(sizeof(*alpha_frame));
        if (unlikely(alpha_frame == NULL))
            return VLCDEC_ECRITICAL;

        vlc_vpx_alpha_t *alpha = vlc_ancillary_GetData(p_alpha);

        static const struct vlc_frame_callbacks cbs_alpha = {
            ReleaseAlphaFrame,
        };

        static const struct vlc_frame_callbacks cbs_opaque = {
            ReleaseOpaqueFrame,
        };

        alpha_frame->frame = frame;
        vlc_atomic_rc_init(&alpha_frame->rc);

        vlc_frame_Init(&alpha_frame->alpha, &cbs_alpha, alpha->data, alpha->size);
        vlc_atomic_rc_inc(&alpha_frame->rc);
        alpha_frame->alpha.i_dts = frame->i_dts;
        alpha_frame->alpha.i_pts = frame->i_pts;
        alpha_frame->alpha.i_length = frame->i_length;
        alpha_frame->alpha.i_flags = frame->i_flags;

        vlc_frame_Init(&alpha_frame->opaque, &cbs_opaque, frame->p_buffer, frame->i_buffer);
        alpha_frame->opaque.i_dts = frame->i_dts;
        alpha_frame->opaque.i_pts = frame->i_pts;
        alpha_frame->opaque.i_length = frame->i_length;
        alpha_frame->opaque.i_flags = frame->i_flags;

        res = p_sys->opaque->dec.pf_decode(&p_sys->opaque->dec, &alpha_frame->opaque);
        if (res != VLCDEC_SUCCESS)
        {
            ReleaseOpaqueFrame(&alpha_frame->opaque);
            return VLCDEC_ECRITICAL;
        }

        res = p_sys->alpha->dec.pf_decode(&p_sys->alpha->dec, &alpha_frame->alpha);
        if (res != VLCDEC_SUCCESS)
        {
            ReleaseAlphaFrame(&alpha_frame->alpha);
            return VLCDEC_ECRITICAL;
        }
    }
    else
    {
        // drain
        vlc_mutex_lock(&p_sys->lock);
        while ( !vlc_picture_chain_IsEmpty(&p_sys->opaque->decoded) &&
                !vlc_picture_chain_IsEmpty(&p_sys->alpha->decoded) )
            SendMergedLocked(dec);

        // drain remaining pushed pictures from one decoder
        picture_t *picture;
        while ((picture = vlc_picture_chain_PopFront(&p_sys->alpha->decoded)) != NULL)
            picture_Release(picture);
        while ((picture = vlc_picture_chain_PopFront(&p_sys->opaque->decoded)) != NULL)
            picture_Release(picture);
        vlc_mutex_unlock(&p_sys->lock);
    }

    return VLCDEC_SUCCESS;
}

static void Flush( decoder_t *dec )
{
    vpx_alpha *p_sys = dec->p_sys;
    vlc_mutex_lock(&p_sys->lock);

    if ( p_sys->opaque->dec.pf_flush != NULL )
        p_sys->opaque->dec.pf_flush( &p_sys->opaque->dec );

    if ( p_sys->alpha->dec.pf_flush != NULL )
        p_sys->alpha->dec.pf_flush( &p_sys->alpha->dec );

    picture_t *picture;
    while ((picture = vlc_picture_chain_PopFront(&p_sys->opaque->decoded)) != NULL)
        picture_Release(picture);
    while ((picture = vlc_picture_chain_PopFront(&p_sys->alpha->decoded)) != NULL)
        picture_Release(picture);
    vlc_mutex_unlock(&p_sys->lock);
}

int OpenDecoder(vlc_object_t *o)
{
    decoder_t *dec = container_of(o, decoder_t, obj);
    if (dec->fmt_in->i_codec != VLC_CODEC_VP8 && dec->fmt_in->i_codec != VLC_CODEC_VP9)
        return VLC_ENOTSUP;
    if (dec->fmt_in->i_level == 0 || dec->fmt_in->i_level == -1)
        return VLC_ENOTSUP;

    vpx_alpha *p_sys = vlc_obj_calloc(o, 1, sizeof(*p_sys));
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    p_sys->opaque = vlc_object_create( o, sizeof( *p_sys->opaque ) );
    if (unlikely(p_sys->opaque == NULL))
        return VLC_EGENERIC;
    p_sys->alpha = vlc_object_create( o, sizeof( *p_sys->alpha ) );
    if (unlikely(p_sys->alpha == NULL))
    {
        vlc_object_delete(&p_sys->opaque->dec);
        return VLC_EGENERIC;
    }

    es_format_t fmt;
    es_format_Copy(&fmt, dec->fmt_in);
    if (dec->fmt_in->i_codec == VLC_CODEC_VP8)
        fmt.i_codec = VLC_CODEC_VP8;
    else
        fmt.i_codec = VLC_CODEC_VP9;
    fmt.i_level = 0;
    decoder_Init( &p_sys->opaque->dec, &p_sys->opaque->fmt_in, &fmt );
    vlc_picture_chain_Init(&p_sys->opaque->decoded);
    es_format_Init(&p_sys->opaque->fmt_out, VIDEO_ES, 0);

    if (dec->fmt_in->i_codec == VLC_CODEC_VP8)
        fmt.i_codec = VLC_CODEC_VP8ALPHA_ES;
    else
        fmt.i_codec = VLC_CODEC_VP9ALPHA_ES;
    decoder_Init( &p_sys->alpha->dec,  &p_sys->alpha->fmt_in,  &fmt );
    vlc_picture_chain_Init(&p_sys->alpha->decoded);
    es_format_Init(&p_sys->alpha->fmt_out, VIDEO_ES, 0);

    vlc_mutex_init(&p_sys->lock);
    dec->p_sys = p_sys;

    static const struct decoder_owner_callbacks dec_cbs =
    {
        .video = {
            .get_device = GetDevice,
            .format_update = FormatUpdate,
            .queue = QueuePic,
            .get_display_date = GetDisplayDate,
            .get_display_rate = GetDisplayRate,
        },
        .get_attachments = GetAttachments,
    };

    p_sys->opaque->dec.cbs = &dec_cbs;
    p_sys->opaque->dec.p_module =
        module_need_var( &p_sys->opaque->dec, "video decoder", "codec" );
    if (p_sys->opaque->dec.p_module == NULL)
    {
        decoder_Destroy(&p_sys->alpha->dec);
        decoder_Destroy(&p_sys->opaque->dec);
        return VLC_EGENERIC;
    }
    p_sys->alpha->dec.cbs = &dec_cbs;
    p_sys->alpha->dec.p_module =
        module_need_var( &p_sys->alpha->dec, "video decoder", "codec" );
    if (p_sys->alpha->dec.p_module == NULL)
    {
        decoder_Destroy(&p_sys->alpha->dec);
        decoder_Destroy(&p_sys->opaque->dec);
        return VLC_EGENERIC;
    }

    dec->pf_decode = Decode;
    dec->pf_flush = Flush;

    return VLC_SUCCESS;
}

void CloseDecoder(vlc_object_t *o)
{
    decoder_t *dec = container_of(o, decoder_t, obj);
    vpx_alpha *p_sys = dec->p_sys;

    es_format_Clean(&p_sys->opaque->fmt_out);
    decoder_Destroy(&p_sys->opaque->dec);
    es_format_Clean(&p_sys->alpha->fmt_out);
    decoder_Destroy(&p_sys->alpha->dec);

    if (p_sys->pool)
        picture_pool_Release(p_sys->pool);

    if (p_sys->vctx)
        vlc_video_context_Release(p_sys->vctx);
}
