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
#include <libavcodec/vaapi.h>
#include <libavutil/hwcontext_vaapi.h>

#include "avcodec.h"
#include "va.h"
#include "../../hw/vaapi/vlc_vaapi.h"
#include "va_surface.h"

struct vaapi_vctx
{
    VADisplay va_dpy;
    AVBufferRef *hwdev_ref;
    bool pool_sem_init;
    vlc_sem_t pool_sem;
};

static int GetVaProfile(const AVCodecContext *ctx, const es_format_t *fmt_in,
                        VAProfile *va_profile, int *vlc_chroma,
                        unsigned *pic_count)
{
    VAProfile i_profile;
    unsigned count = 3;
    int i_vlc_chroma = VLC_CODEC_VAAPI_420;

    switch(ctx->codec_id)
    {
    case AV_CODEC_ID_MPEG1VIDEO:
    case AV_CODEC_ID_MPEG2VIDEO:
        i_profile = VAProfileMPEG2Main;
        count = 4;
        break;
    case AV_CODEC_ID_MPEG4:
        i_profile = VAProfileMPEG4AdvancedSimple;
        break;
    case AV_CODEC_ID_WMV3:
        i_profile = VAProfileVC1Main;
        break;
    case AV_CODEC_ID_VC1:
        i_profile = VAProfileVC1Advanced;
        break;
    case AV_CODEC_ID_H264:
        i_profile = VAProfileH264High;
        count = 18;
        break;
    case AV_CODEC_ID_HEVC:
        if (fmt_in->i_profile == FF_PROFILE_HEVC_MAIN)
            i_profile = VAProfileHEVCMain;
        else if (fmt_in->i_profile == FF_PROFILE_HEVC_MAIN_10)
        {
            i_profile = VAProfileHEVCMain10;
            i_vlc_chroma = VLC_CODEC_VAAPI_420_10BPP;
        }
        else
            return VLC_EGENERIC;
        count = 18;
        break;
    case AV_CODEC_ID_VP8:
        i_profile = VAProfileVP8Version0_3;
        count = 5;
        break;
    case AV_CODEC_ID_VP9:
        if (ctx->profile == FF_PROFILE_VP9_0)
            i_profile = VAProfileVP9Profile0;
#if VA_CHECK_VERSION( 0, 39, 0 )
        else if (ctx->profile == FF_PROFILE_VP9_2)
        {
            i_profile = VAProfileVP9Profile2;
            i_vlc_chroma = VLC_CODEC_VAAPI_420_10BPP;
        }
#endif
        else
            return VLC_EGENERIC;
        count = 10;
        break;
    default:
        return VLC_EGENERIC;
    }

    *va_profile = i_profile;
    *pic_count = count + ctx->thread_count;
    *vlc_chroma = i_vlc_chroma;
    return VLC_SUCCESS;
}

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

    assert(vaapi_vctx->pool_sem_init);

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
    AVHWFramesContext *hwframes = (AVHWFramesContext*)ctx->hw_frames_ctx->data;
    struct vaapi_vctx *vaapi_vctx =
        vlc_video_context_GetPrivate(vctx, VLC_VIDEO_CONTEXT_VAAPI);

    assert(ctx->hw_frames_ctx);

    /* The pool size can only be known after the hw context has been
     * initialized (internally by ffmpeg), so between Create() and the first
     * Get() */
    if (!vaapi_vctx->pool_sem_init)
    {
        vlc_sem_init(&vaapi_vctx->pool_sem,
                     hwframes->initial_pool_size +
                     ctx->thread_count +
                     3 /* cf. ff_decode_get_hw_frames_ctx */);
        vaapi_vctx->pool_sem_init = true;
    }

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

    av_buffer_unref(&vaapi_vctx->hwdev_ref);
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
    if ( hwfmt != AV_PIX_FMT_VAAPI || dec_device == NULL ||
        dec_device->type != VLC_DECODER_DEVICE_VAAPI)
        return VLC_EGENERIC;

    VAProfile i_profile;
    unsigned count;
    int i_vlc_chroma;
    if (GetVaProfile(ctx, fmt_in, &i_profile, &i_vlc_chroma, &count) != VLC_SUCCESS)
        return VLC_EGENERIC;

    VADisplay va_dpy = dec_device->opaque;

    AVBufferRef *hwdev_ref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);
    if (hwdev_ref == NULL)
        goto error;

    AVHWDeviceContext *hwdev_ctx = (void *) hwdev_ref->data;
    AVVAAPIDeviceContext *vadev_ctx = hwdev_ctx->hwctx;
    vadev_ctx->display = va_dpy;

    if (av_hwdevice_ctx_init(hwdev_ref) < 0)
        goto error;

    vlc_video_context *vctx =
        vlc_video_context_Create(dec_device, VLC_VIDEO_CONTEXT_VAAPI,
                                 sizeof(struct vaapi_vctx), &vaapi_ctx_ops);
    if (vctx == NULL)
        goto error;

    struct vaapi_vctx *vaapi_vctx =
        vlc_video_context_GetPrivate(vctx, VLC_VIDEO_CONTEXT_VAAPI);

    vaapi_vctx->va_dpy = va_dpy;
    vaapi_vctx->hwdev_ref = hwdev_ref;
    vaapi_vctx->pool_sem_init = false;

    msg_Info(va, "Using %s", vaQueryVendorString(va_dpy));

    fmt_out->i_chroma = i_vlc_chroma;

    ctx->hw_device_ctx = hwdev_ref;

    va->ops = &ops;
    va->sys = vctx;
    *vtcx_out = vctx;
    return VLC_SUCCESS;

error:
    if (hwdev_ref != NULL)
        av_buffer_unref(&hwdev_ref);
    return VLC_EGENERIC;
}

vlc_module_begin ()
    set_description( N_("VA-API video decoder") )
    set_va_callback( Create, 100 )
    add_shortcut( "vaapi" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
vlc_module_end ()
