/*****************************************************************************
 * avcodec.c: VDPAU decoder for libav
 *****************************************************************************
 * Copyright (C) 2012-2013 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <libavutil/mem.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/vdpau.h>
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_fourcc.h>
#include <vlc_picture.h>
#include <vlc_codec.h>
#include "vlc_vdpau.h"
#include "../../codec/avcodec/va.h"

typedef struct
{
    VdpChromaType type;
    void *hwaccel_context;
    uint32_t width;
    uint32_t height;
    vlc_video_context *vctx;
    vlc_vdp_video_field_t *pool[];
} vlc_va_sys_t;

static vlc_vdp_video_field_t *CreateSurface(vlc_va_t *va, vdpau_decoder_device_t *vdpau_decoder)
{
    vlc_va_sys_t *sys = va->sys;
    VdpVideoSurface surface;
    VdpStatus err;

    err = vdp_video_surface_create(vdpau_decoder->vdp, vdpau_decoder->device, sys->type,
                                   sys->width, sys->height, &surface);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(va, "%s creation failure: %s", "video surface",
                vdp_get_error_string(vdpau_decoder->vdp, err));
        return NULL;
    }

    vlc_vdp_video_field_t *field = vlc_vdp_video_create(sys->vctx, surface);
    if (unlikely(field == NULL))
        vdp_video_surface_destroy(vdpau_decoder->vdp, surface);
    return field;
}

static vlc_vdp_video_field_t *GetSurface(vlc_va_sys_t *sys)
{
    vlc_vdp_video_field_t *f;

    for (unsigned i = 0; (f = sys->pool[i]) != NULL; i++)
    {
        uintptr_t expected = 1;

        if (atomic_compare_exchange_strong(&f->frame->refs, &expected, 2))
        {
            vlc_vdp_video_field_t *field = vlc_vdp_video_copy(f);
            atomic_fetch_sub(&f->frame->refs, 1);
            return field;
        }
    }
    return NULL;
}

static vlc_vdp_video_field_t *Get(vlc_va_sys_t *sys)
{
    vlc_vdp_video_field_t *field;

    while ((field = GetSurface(sys)) == NULL)
    {
        /* Pool empty. Wait for some time as in src/input/decoder.c.
         * XXX: Both this and the core should use a semaphore or a CV. */
        vlc_tick_sleep(VOUT_OUTMEM_SLEEP);
    }

    return field;
}

static int Lock(vlc_va_t *va, picture_t *pic, AVCodecContext *ctx, AVFrame *frame)
{
    (void) ctx;

    vlc_va_sys_t *sys = va->sys;
    vlc_vdp_video_field_t *field = Get(sys);
    if (field == NULL)
        return VLC_ENOMEM;

    pic->context = &field->context;
    frame->data[3] = (void *)(uintptr_t)field->frame->surface;
    return VLC_SUCCESS;
}

static void Close(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;

    for (size_t i = 0; sys->pool[i] != NULL; i++)
        vlc_vdp_video_destroy(sys->pool[i]);

    vlc_video_context_Release(sys->vctx);
    if (sys->hwaccel_context)
        av_free(sys->hwaccel_context);
    free(sys);
}

static const struct vlc_va_operations ops = { Lock, Close, };

const struct vlc_video_context_operations vdpau_vctx_ops = {
    NULL,
};

static int Open(vlc_va_t *va, AVCodecContext *avctx, enum AVPixelFormat hwfmt, const AVPixFmtDescriptor *desc,
                const es_format_t *fmt_in, vlc_decoder_device *dec_device,
                video_format_t *fmt_out, vlc_video_context **vtcx_out)
{
    if ( hwfmt != AV_PIX_FMT_VDPAU || GetVDPAUOpaqueDevice(dec_device) == NULL)
        return VLC_EGENERIC;

    (void) fmt_in;
    (void) desc;
    void *func;
    VdpStatus err;
    VdpChromaType type, *chroma;
    uint32_t width, height;

    if (av_vdpau_get_surface_parameters(avctx, &type, &width, &height))
        return VLC_EGENERIC;

    fmt_out->i_chroma = VLC_CODEC_VDPAU_VIDEO;

    unsigned codec_refs;
    switch (avctx->codec_id)
    {
        case AV_CODEC_ID_H264:
            codec_refs = avctx->refs; // we can rely on this
            break;
        case AV_CODEC_ID_HEVC:
            codec_refs = 16;
            break;
        case AV_CODEC_ID_VP9:
            codec_refs = 8;
            break;
        default:
            codec_refs = 2;
            break;
    }

    const unsigned refs = codec_refs + 2 * avctx->thread_count + 5;
    size_t extra = (refs + 1) * sizeof (vlc_vdp_video_field_t *);

    vlc_va_sys_t *sys = malloc(sizeof (*sys) + extra);
    if (unlikely(sys == NULL))
       return VLC_ENOMEM;

    sys->vctx = vlc_video_context_Create(dec_device, VLC_VIDEO_CONTEXT_VDPAU,
                                         sizeof (VdpChromaType),
                                         &vdpau_vctx_ops);
    if (sys->vctx == NULL)
    {
        free(sys);
        return VLC_ENOMEM;
    }

    chroma = vlc_video_context_GetPrivate(sys->vctx, VLC_VIDEO_CONTEXT_VDPAU);
    *chroma = type;
    sys->type = type;
    sys->width = width;
    sys->height = height;
    sys->hwaccel_context = NULL;
    vdpau_decoder_device_t *vdpau_decoder = GetVDPAUOpaqueDevice(dec_device);

    unsigned flags = AV_HWACCEL_FLAG_ALLOW_HIGH_DEPTH;

    err = vdp_get_proc_address(vdpau_decoder->vdp, vdpau_decoder->device,
                               VDP_FUNC_ID_GET_PROC_ADDRESS, &func);
    if (err != VDP_STATUS_OK)
        goto error;

    if (av_vdpau_bind_context(avctx, vdpau_decoder->device, func, flags))
        goto error;
    sys->hwaccel_context = avctx->hwaccel_context;
    va->sys = sys;

    unsigned i = 0;
    while (i < refs)
    {
        sys->pool[i] = CreateSurface(va, vdpau_decoder);
        if (sys->pool[i] == NULL)
            break;
        i++;
    }
    sys->pool[i] = NULL;

    if (i < refs)
    {
        msg_Err(va, "not enough video RAM");
        Close(va);
        return VLC_ENOMEM;
    }

    *vtcx_out = sys->vctx;
    va->ops = &ops;
    return VLC_SUCCESS;

error:
    if (sys->vctx)
        vlc_video_context_Release(sys->vctx);
    if (sys->hwaccel_context)
        av_free(sys->hwaccel_context);
    free(sys);
    return VLC_EGENERIC;
}

vlc_module_begin()
    set_description(N_("VDPAU video decoder"))
    set_subcategory(SUBCAT_INPUT_VCODEC)
    set_va_callback(Open, 100)
    add_shortcut("vdpau")
vlc_module_end()
