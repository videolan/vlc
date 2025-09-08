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
    bool dynamic_pool;
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

    if (!pic_ctx->cloned && !vaapi_vctx->dynamic_pool)
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
    if(!vaapi_vctx->dynamic_pool)
        vlc_sem_wait(&vaapi_vctx->pool_sem);

    int ret = av_hwframe_get_buffer(ctx->hw_frames_ctx, frame, 0);
    if (ret)
    {
        msg_Err(va, "vaapi_va: av_hwframe_get_buffer failed: %d\n", ret);
        if(!vaapi_vctx->dynamic_pool)
            vlc_sem_post(&vaapi_vctx->pool_sem);
        return ret;
    }

    vaapi_dec_pic_context *vaapi_pic_ctx = malloc(sizeof(*vaapi_pic_ctx));
    if (unlikely(vaapi_pic_ctx == NULL))
    {
        if(!vaapi_vctx->dynamic_pool)
            vlc_sem_post(&vaapi_vctx->pool_sem);
        return VLC_ENOMEM;
    }

    vaapi_pic_ctx->ctx.s = (picture_context_t) {
        vaapi_dec_pic_context_destroy, vaapi_dec_pic_context_copy, vctx,
    };

    vaapi_pic_ctx->ctx.surface = (uintptr_t) frame->data[3];
    vaapi_pic_ctx->ctx.va_dpy = vaapi_vctx->va_dpy;
    vaapi_pic_ctx->avframe = av_frame_clone(frame);
#if LIBAVCODEC_VERSION_CHECK(61, 03, 100)
    av_frame_side_data_free(&vaapi_pic_ctx->avframe->side_data, &vaapi_pic_ctx->avframe->nb_side_data);
#endif
    av_buffer_unref(&vaapi_pic_ctx->avframe->opaque_ref);
    vaapi_pic_ctx->avframe->opaque = NULL;
    vaapi_pic_ctx->cloned = false;
    vlc_vaapi_PicSetContext(pic, &vaapi_pic_ctx->ctx);

    return VLC_SUCCESS;
}

static void Delete(vlc_va_t *va, AVCodecContext* ctx)
{
    if (ctx)
        av_buffer_unref(&ctx->hw_frames_ctx);
    vlc_video_context_Release(va->sys);
}

static void vaapi_ctx_destroy(void *priv)
{
    struct vaapi_vctx *vaapi_vctx = priv;

    av_buffer_unref(&vaapi_vctx->hwframes_ref);
}

static const struct vlc_va_operations ops =
{
    .get = Get,
    .close = Delete,
};

static const struct vlc_video_context_operations vaapi_ctx_ops =
{
    .destroy = vaapi_ctx_destroy,
};

static int CheckCodecConfig(vlc_va_t *va, AVCodecContext *ctx, VADisplay va_dpy,
                            const AVPixFmtDescriptor *sw_desc)
{
    assert(ctx->codec_id == AV_CODEC_ID_AV1);

    if (sw_desc->nb_components < 2)
        return VLC_EGENERIC;

    int va_rt_format, vc_fourcc;
    switch (sw_desc->comp[0].depth)
    {
        case 8:
            va_rt_format = VA_RT_FORMAT_YUV420;
            vc_fourcc = VA_FOURCC_NV12;
            break;
        case 10:
            va_rt_format = VA_RT_FORMAT_YUV420_10BPP;
            vc_fourcc = VA_FOURCC_P010;
            break;
        default:
            return VLC_EGENERIC;
    }

    VAProfile profile;
    switch (ctx->profile)
    {
        case AVPROFILE(AV1_MAIN):
            profile = VAProfileAV1Profile0;
            break;
        case AVPROFILE(AV1_HIGH):
            profile = VAProfileAV1Profile1;
            break;
        default:
            return VLC_EGENERIC;
    }

    VAConfigID config_id = VA_INVALID_ID;
    VASurfaceID render_target = VA_INVALID_ID;
    VAContextID context_id = VA_INVALID_ID;

    config_id = vlc_vaapi_CreateConfigChecked(VLC_OBJECT(va), va_dpy, profile,
                                              VAEntrypointVLD, 0);
    if (config_id == VA_INVALID_ID)
        goto error;

    VASurfaceAttrib fourcc_attribs[1] = {
        {
            .type = VASurfaceAttribPixelFormat,
            .flags = VA_SURFACE_ATTRIB_SETTABLE,
            .value.type    = VAGenericValueTypeInteger,
            .value.value.i = vc_fourcc,
        }
    };

    VA_CALL(VLC_OBJECT(va), vaCreateSurfaces, va_dpy, va_rt_format,
            ctx->coded_width, ctx->coded_height, &render_target, 1,
            fourcc_attribs, 1);

    context_id = vlc_vaapi_CreateContext(VLC_OBJECT(va), va_dpy, config_id,
                                         ctx->coded_width, ctx->coded_height,
                                         VA_PROGRESSIVE, &render_target, 1);

error:
    if (render_target != VA_INVALID_ID)
        VA_CALL(VLC_OBJECT(va), vaDestroySurfaces, va_dpy, &render_target, 1);

    if (config_id != VA_INVALID_ID)
        vlc_vaapi_DestroyConfig(VLC_OBJECT(va), va_dpy, config_id);

    if (context_id != VA_INVALID_ID)
    {
        vlc_vaapi_DestroyContext(VLC_OBJECT(va), va_dpy, context_id);
        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

static int AVHWFramesContextCompare(const AVHWFramesContext *c1,
                                    const AVHWFramesContext *c2)
{
    return c1->width == c2->width && c1->height == c2->height &&
           c1->format == c2->format && c1->sw_format == c2->sw_format &&
           c1->initial_pool_size == c2->initial_pool_size ? 0 : 1;
}

static vlc_video_context *
ReuseVideoContext(vlc_video_context *vctx, AVBufferRef *hwframes_ref_new)
{
    if (vctx == NULL)
        return NULL;

    struct vaapi_vctx *vaapi_vctx =
    vlc_video_context_GetPrivate(vctx, VLC_VIDEO_CONTEXT_VAAPI);

    AVHWFramesContext *hwframes_ctx_prev =
        (AVHWFramesContext*)vaapi_vctx->hwframes_ref->data;

    AVHWFramesContext *hwframes_ctx_new =
        (AVHWFramesContext*)hwframes_ref_new->data;

    if (AVHWFramesContextCompare(hwframes_ctx_prev, hwframes_ctx_new) != 0)
        return NULL;

    vctx = vlc_video_context_Hold(vctx);
    av_buffer_unref(&vaapi_vctx->hwframes_ref);
    vaapi_vctx->hwframes_ref = hwframes_ref_new;
    return vctx;
}

static int Create(vlc_va_t *va, struct vlc_va_cfg *cfg)
{
    AVCodecContext *ctx = cfg->avctx;
    enum AVPixelFormat hwfmt = cfg->hwfmt;
    vlc_decoder_device *dec_device = cfg->dec_device;
    video_format_t *fmt_out = cfg->video_fmt_out;

    if ( hwfmt != AV_PIX_FMT_VAAPI || dec_device == NULL ||
        dec_device->type != VLC_DECODER_DEVICE_VAAPI)
        return VLC_EGENERIC;

    VADisplay va_dpy = dec_device->opaque;

    switch (ctx->codec_id)
    {
        case AV_CODEC_ID_AV1:
        {
            /* Tested with ffmpeg @2d077f9, and few AV1 samples, 10 extra
             * pictures are needed to avoid deadlocks (cf. commit description)
             * when playing and seeking. */
            cfg->extra_pictures = 10;

            int ret = CheckCodecConfig(va, ctx, va_dpy, cfg->desc);
            if (ret != VLC_SUCCESS)
                return ret;
            break;
        }
        default:
            break;
    }

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
#if LIBAVUTIL_VERSION_CHECK( 57, 36, 100 )
            case AV_PIX_FMT_P012LE:
            case AV_PIX_FMT_P012BE:
                vlc_chroma = VLC_CODEC_VAAPI_420_12BPP;
                break;
#endif
            default:
                break;
        }
    }

    if (vlc_chroma == 0)
    {
        msg_Warn(va, "ffmpeg chroma not compatible with vlc: hw: %s, sw: %s",
                 av_get_pix_fmt_name(hwframes_ctx->format),
                 av_get_pix_fmt_name(hwframes_ctx->sw_format));
        av_buffer_unref(&hwframes_ref);
        return VLC_EGENERIC;
    }

    ctx->hw_frames_ctx = av_buffer_ref(hwframes_ref);
    if (!ctx->hw_frames_ctx)
    {
        av_buffer_unref(&hwframes_ref);
        return VLC_EGENERIC;
    }

    vlc_video_context *vctx = ReuseVideoContext(cfg->vctx_prev, hwframes_ref);
    if (vctx == NULL)
    {
        vctx = 
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
        vaapi_vctx->dynamic_pool = hwframes_ctx->initial_pool_size < 1;
    }

    msg_Info(va, "Using %s", vaQueryVendorString(va_dpy));

    fmt_out->i_chroma = vlc_chroma;

    va->ops = &ops;
    va->sys = vctx;
    cfg->vctx_out = vctx;
    cfg->use_hwframes = true;
    return VLC_SUCCESS;
}

vlc_module_begin ()
    set_description( N_("VA-API video decoder") )
    set_va_callback( Create, 100 )
    add_shortcut( "vaapi" )
    set_subcategory( SUBCAT_INPUT_VCODEC )
vlc_module_end ()
