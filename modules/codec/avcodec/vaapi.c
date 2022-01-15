/*****************************************************************************
 * vaapi.c: VAAPI helpers for the libavcodec decoder
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
 * Copyright (C) 2009-2010 Laurent Aimar
 * Copyright (C) 2012-2014 Rémi Denis-Courmont
 *
 * Authors: Laurent Aimar <fenrir_AT_ videolan _DOT_ org>
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

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_fourcc.h>
#include <vlc_picture.h>
#include <vlc_picture_pool.h>

#ifdef VLC_VA_BACKEND_DRM
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <vlc_fs.h>
# include <va/va_drm.h>
#endif
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext_vaapi.h>

#include "avcodec.h"
#include "va.h"
#include "../../hw/vaapi/vlc_vaapi.h"
#include "va_surface.h"

struct vaapi_vctx
{
    VADisplay va_dpy;
    AVBufferRef *hwframes_ref;
    vlc_sem_t pool_sem;
};

typedef struct {
    struct vaapi_pic_context ctx;
    AVFrame *avframe;
    bool cloned;
} vaapi_dec_pic_context;

static void vaapi_dec_pic_context_destroy(picture_context_t *context)
{
    vaapi_dec_pic_context *pic_ctx = container_of(context, vaapi_dec_pic_context, ctx.s);

    struct vaapi_vctx *vaapi_vctx =
        vlc_video_context_GetPrivate(pic_ctx->ctx.s.vctx, VLC_VIDEO_CONTEXT_VAAPI);

    av_frame_free(&pic_ctx->avframe);

    if (!pic_ctx->cloned)
        vlc_sem_post(&vaapi_vctx->pool_sem);

    free(pic_ctx);
}

static picture_context_t *vaapi_dec_pic_context_copy(picture_context_t *src)
{
    vaapi_dec_pic_context *src_ctx = container_of(src, vaapi_dec_pic_context, ctx.s);
    vaapi_dec_pic_context *pic_ctx = malloc(sizeof(*pic_ctx));
    if (unlikely(pic_ctx == NULL))
        return NULL;

    pic_ctx->ctx = src_ctx->ctx;

    pic_ctx->avframe = av_frame_clone(src_ctx->avframe);
    if (!pic_ctx->avframe)
    {
        free(pic_ctx);
        return NULL;
    }
    pic_ctx->cloned = true;

    vlc_video_context_Hold(pic_ctx->ctx.s.vctx);
    return &pic_ctx->ctx.s;
}

static int Get(vlc_va_t *va, picture_t *pic, AVCodecContext *ctx, AVFrame *frame)
{
    vlc_video_context *vctx = va->sys;
    struct vaapi_vctx *vaapi_vctx =
        vlc_video_context_GetPrivate(vctx, VLC_VIDEO_CONTEXT_VAAPI);

    /* If all frames are out, wait for a frame to be released. */
    vlc_sem_wait(&vaapi_vctx->pool_sem);

    int ret = av_hwframe_get_buffer(ctx->hw_frames_ctx, frame, 0);
    if (ret)
    {
        msg_Err(va, "vaapi_va: av_hwframe_get_buffer failed: %d\n", ret);
        vlc_sem_post(&vaapi_vctx->pool_sem);
        return ret;
    }

    vaapi_dec_pic_context *vaapi_pic_ctx = malloc(sizeof(*vaapi_pic_ctx));
    if (unlikely(vaapi_pic_ctx == NULL))
    {
        vlc_sem_post(&vaapi_vctx->pool_sem);
        return VLC_ENOMEM;
    }

    vaapi_pic_ctx->ctx.s = (picture_context_t) {
        vaapi_dec_pic_context_destroy, vaapi_dec_pic_context_copy, vctx,
    };

    vaapi_pic_ctx->ctx.surface = (uintptr_t) frame->data[3];
    vaapi_pic_ctx->ctx.va_dpy = vaapi_vctx->va_dpy;
    vaapi_pic_ctx->avframe = av_frame_clone(frame);
    vaapi_pic_ctx->cloned = false;
    vlc_vaapi_PicSetContext(pic, &vaapi_pic_ctx->ctx);

    return VLC_SUCCESS;
}

static void Delete(vlc_va_t *va)
{
    vlc_video_context_Release(va->sys);
}

static void vaapi_ctx_destroy(void *priv)
{
    struct vaapi_vctx *vaapi_vctx = priv;

    av_buffer_unref(&vaapi_vctx->hwframes_ref);
}

static const struct vlc_va_operations ops = { Get, Delete, };

static const struct vlc_video_context_operations vaapi_ctx_ops =
{
    .destroy = vaapi_ctx_destroy,
};

static int Create(vlc_va_t *va, AVCodecContext *ctx, enum AVPixelFormat hwfmt,
                  const AVPixFmtDescriptor *desc,
                  const es_format_t *fmt_in, vlc_decoder_device *dec_device,
                  video_format_t *fmt_out, vlc_video_context **vtcx_out)
{
    VLC_UNUSED(desc);
    VLC_UNUSED(fmt_in);
    if ( hwfmt != AV_PIX_FMT_VAAPI || dec_device == NULL ||
        dec_device->type != VLC_DECODER_DEVICE_VAAPI)
        return VLC_EGENERIC;

    VADisplay va_dpy = dec_device->opaque;

    AVBufferRef *hwdev_ref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);
    if (hwdev_ref == NULL)
        return VLC_EGENERIC;

    AVHWDeviceContext *hwdev_ctx = (void *) hwdev_ref->data;
    AVVAAPIDeviceContext *vadev_ctx = hwdev_ctx->hwctx;
    vadev_ctx->display = va_dpy;

    if (av_hwdevice_ctx_init(hwdev_ref) < 0)
    {
        av_buffer_unref(&hwdev_ref);
        return VLC_EGENERIC;
    }

    AVBufferRef *hwframes_ref;
    int ret = avcodec_get_hw_frames_parameters(ctx, hwdev_ref, hwfmt, &hwframes_ref);
    av_buffer_unref(&hwdev_ref);
    if (ret < 0)
    {
        msg_Err(va, "avcodec_get_hw_frames_parameters failed: %d", ret);
        return VLC_EGENERIC;
    }

    AVHWFramesContext *hwframes_ctx = (AVHWFramesContext*)hwframes_ref->data;

    if (hwframes_ctx->initial_pool_size)
    {
        // cf. ff_decode_get_hw_frames_ctx()
        // We guarantee 4 base work surfaces. The function above guarantees 1
        // (the absolute minimum), so add the missing count.
        hwframes_ctx->initial_pool_size += 3;
    }

    ret = av_hwframe_ctx_init(hwframes_ref);
    if (ret < 0)
    {
        msg_Err(va, "av_hwframe_ctx_init failed: %d", ret);
        av_buffer_unref(&hwframes_ref);
        return VLC_EGENERIC;
    }

    int vlc_chroma = 0;
    if (hwframes_ctx->format == AV_PIX_FMT_VAAPI)
    {
        switch (hwframes_ctx->sw_format)
        {
            case AV_PIX_FMT_YUV420P:
            case AV_PIX_FMT_NV12:
                vlc_chroma = VLC_CODEC_VAAPI_420;
                break;
            case AV_PIX_FMT_P010LE:
            case AV_PIX_FMT_P010BE:
            case AV_PIX_FMT_YUV420P10BE:
            case AV_PIX_FMT_YUV420P10LE:
                vlc_chroma = VLC_CODEC_VAAPI_420_10BPP;
                break;
            default:
                break;
        }
    }

    if (vlc_chroma == 0)
    {
        msg_Warn(va, "ffmpeg chroma not compatible with vlc: hw: %d, sw: %d",
                 hwframes_ctx->format, hwframes_ctx->sw_format);
        av_buffer_unref(&hwframes_ref);
        return VLC_EGENERIC;
    }

    ctx->hw_frames_ctx = av_buffer_ref(hwframes_ref);
    if (!ctx->hw_frames_ctx)
    {
        av_buffer_unref(&hwframes_ref);
        return VLC_EGENERIC;
    }

    vlc_video_context *vctx =
        vlc_video_context_Create(dec_device, VLC_VIDEO_CONTEXT_VAAPI,
                                 sizeof(struct vaapi_vctx), &vaapi_ctx_ops);
    if (vctx == NULL)
    {
        av_buffer_unref(&hwframes_ref);
        av_buffer_unref(&ctx->hw_frames_ctx);
        return VLC_EGENERIC;
    }

    struct vaapi_vctx *vaapi_vctx =
        vlc_video_context_GetPrivate(vctx, VLC_VIDEO_CONTEXT_VAAPI);

    vaapi_vctx->va_dpy = va_dpy;
    vaapi_vctx->hwframes_ref = hwframes_ref;
    vlc_sem_init(&vaapi_vctx->pool_sem, hwframes_ctx->initial_pool_size);

    msg_Info(va, "Using %s", vaQueryVendorString(va_dpy));

    fmt_out->i_chroma = vlc_chroma;

    va->ops = &ops;
    va->sys = vctx;
    *vtcx_out = vctx;
    return VLC_SUCCESS;
}

vlc_module_begin ()
    set_description( N_("VA-API video decoder") )
    set_va_callback( Create, 100 )
    add_shortcut( "vaapi" )
    set_subcategory( SUBCAT_INPUT_VCODEC )
vlc_module_end ()
