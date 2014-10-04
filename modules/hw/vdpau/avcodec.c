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
#include <vlc_xlib.h>
#include "vlc_vdpau.h"
#include "../../codec/avcodec/va.h"

static int Open(vlc_va_t *, AVCodecContext *, const es_format_t *);
static void Close(vlc_va_t *, AVCodecContext *);

vlc_module_begin()
    set_description(N_("VDPAU video decoder"))
    set_capability("hw decoder", 100)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)
    set_callbacks(Open, Close)
    add_shortcut("vdpau")
vlc_module_end()

struct vlc_va_sys_t
{
    vdp_t *vdp;
    VdpDevice device;
    uint16_t width;
    uint16_t height;
};

static vlc_vdp_video_field_t *CreateSurface(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;
    VdpVideoSurface surface;
    VdpStatus err;

    err = vdp_video_surface_create(sys->vdp, sys->device, VDP_CHROMA_TYPE_420,
                                   sys->width, sys->height, &surface);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(va, "%s creation failure: %s", "video surface",
                vdp_get_error_string(sys->vdp, err));
        return NULL;
    }

    vlc_vdp_video_field_t *field = vlc_vdp_video_create(sys->vdp, surface);
    if (unlikely(field == NULL))
        vdp_video_surface_destroy(sys->vdp, surface);
    return field;
}

static int Lock(vlc_va_t *va, void **opaque, uint8_t **data)
{
    vlc_vdp_video_field_t *field = CreateSurface(va);
    if (unlikely(field == NULL))
        return VLC_ENOMEM;

    *opaque = field;
    *data = (void *)(uintptr_t)field->frame->surface;
    return VLC_SUCCESS;
}

static void Unlock(void *opaque, uint8_t *data)
{
    vlc_vdp_video_field_t *field = opaque;

    assert(field != NULL);
    field->destroy(field);
    (void) data;
}

static int Copy(vlc_va_t *va, picture_t *pic, void *opaque, uint8_t *data)
{
    vlc_vdp_video_field_t *field = opaque;

    assert(field != NULL);
    field = vlc_vdp_video_copy(field);
    if (unlikely(field == NULL))
        return VLC_ENOMEM;

    assert(pic->context == NULL);
    pic->context = field;
    (void) va; (void) data;
    return VLC_SUCCESS;
}

static int Setup(vlc_va_t *va, AVCodecContext *avctx, vlc_fourcc_t *chromap)
{
    vlc_va_sys_t *sys = va->sys;
    AVVDPAUContext *hwctx = avctx->hwaccel_context;
    VdpDecoderProfile profile;
    VdpStatus err;

    if (hwctx->decoder != VDP_INVALID_HANDLE)
    {
        if (sys->width == avctx->coded_width
         && sys->height == avctx->coded_height)
            return VLC_SUCCESS;

        vdp_decoder_destroy(sys->vdp, hwctx->decoder);
        hwctx->decoder = VDP_INVALID_HANDLE;
    }

    sys->width = (avctx->coded_width + 1) & ~1;
    sys->height = (avctx->coded_height + 3) & ~3;

    if (av_vdpau_get_profile(avctx, &profile))
    {
        msg_Err(va, "no longer supported codec profile");
        return VLC_EGENERIC;
    }

    err = vdp_decoder_create(sys->vdp, sys->device, profile, sys->width,
                             sys->height, avctx->refs, &hwctx->decoder);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(va, "%s creation failure: %s", "decoder",
                vdp_get_error_string(sys->vdp, err));
        hwctx->decoder = VDP_INVALID_HANDLE;
        return VLC_EGENERIC;
    }

    /* TODO: select better chromas when appropriate */
    *chromap = VLC_CODEC_VDPAU_VIDEO_420;
    return VLC_SUCCESS;
}

static int Open(vlc_va_t *va, AVCodecContext *avctx, const es_format_t *fmt)
{
    VdpStatus err;
    VdpDecoderProfile profile;
    int level = fmt->i_level;

    if (av_vdpau_get_profile(avctx, &profile))
    {
        msg_Err(va, "unsupported codec %d or profile %d", avctx->codec_id,
                fmt->i_profile);
        return VLC_EGENERIC;
    }

    switch (avctx->codec_id)
    {
        case AV_CODEC_ID_MPEG1VIDEO:
            level = VDP_DECODER_LEVEL_MPEG1_NA;
            break;
        case AV_CODEC_ID_MPEG2VIDEO:
            level = VDP_DECODER_LEVEL_MPEG2_HL;
            break;
        case AV_CODEC_ID_H263:
            level = VDP_DECODER_LEVEL_MPEG4_PART2_ASP_L5;
            break;
        case AV_CODEC_ID_H264:
            if ((fmt->i_profile & FF_PROFILE_H264_INTRA)
             && (fmt->i_level == 11))
                level = VDP_DECODER_LEVEL_H264_1b;
        default:
            break;
    }

    if (!vlc_xlib_init(VLC_OBJECT(va)))
    {
        msg_Err(va, "Xlib is required for VDPAU");
        return VLC_EGENERIC;
    }

    vlc_va_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
       return VLC_ENOMEM;

    avctx->hwaccel_context = av_vdpau_alloc_context();
    if (unlikely(avctx->hwaccel_context == NULL))
    {
        free(sys);
        return VLC_ENOMEM;
    }

    err = vdp_get_x11(NULL, -1, &sys->vdp, &sys->device);
    if (err != VDP_STATUS_OK)
    {
        av_freep(&avctx->hwaccel_context);
        free(sys);
        return VLC_EGENERIC;
    }

    void *func;
    err = vdp_get_proc_address(sys->vdp, sys->device,
                               VDP_FUNC_ID_DECODER_RENDER, &func);
    if (err != VDP_STATUS_OK)
        goto error;

    AVVDPAUContext *hwctx = avctx->hwaccel_context;

    hwctx->decoder = VDP_INVALID_HANDLE;
    hwctx->render = func;

    /* Check capabilities */
    VdpBool support;
    uint32_t l, mb, w, h;

    if (vdp_video_surface_query_capabilities(sys->vdp, sys->device,
              VDP_CHROMA_TYPE_420, &support, &w, &h) != VDP_STATUS_OK)
        support = VDP_FALSE;
    if (!support)
    {
        msg_Err(va, "video surface format not supported: %s", "YUV 4:2:0");
        goto error;
    }
    msg_Dbg(va, "video surface limits: %"PRIu32"x%"PRIu32, w, h);
    if (w < fmt->video.i_width || h < fmt->video.i_height)
    {
        msg_Err(va, "video surface above limits: %ux%u",
                fmt->video.i_width, fmt->video.i_height);
        goto error;
    }

    if (vdp_decoder_query_capabilities(sys->vdp, sys->device, profile,
                                   &support, &l, &mb, &w, &h) != VDP_STATUS_OK)
        support = VDP_FALSE;
    if (!support)
    {
        msg_Err(va, "decoder profile not supported: %u", profile);
        goto error;
    }
    msg_Dbg(va, "decoder profile limits: level %"PRIu32" mb %"PRIu32" "
            "%"PRIu32"x%"PRIu32, l, mb, w, h);
    if ((int)l < level || w < fmt->video.i_width || h < fmt->video.i_height)
    {
        msg_Err(va, "decoder profile above limits: level %d %ux%u",
                level, fmt->video.i_width, fmt->video.i_height);
        goto error;
    }

    const char *infos;
    if (vdp_get_information_string(sys->vdp, &infos) != VDP_STATUS_OK)
        infos = "VDPAU";

    va->sys = sys;
    va->description = infos;
    va->pix_fmt = AV_PIX_FMT_VDPAU;
    va->setup = Setup;
    va->get = Lock;
    va->release = Unlock;
    va->extract = Copy;
    return VLC_SUCCESS;

error:
    vdp_release_x11(sys->vdp);
    av_freep(&avctx->hwaccel_context);
    free(sys);
    return VLC_EGENERIC;
}

static void Close(vlc_va_t *va, AVCodecContext *avctx)
{
    vlc_va_sys_t *sys = va->sys;
    AVVDPAUContext *hwctx = avctx->hwaccel_context;

    if (hwctx->decoder != VDP_INVALID_HANDLE)
        vdp_decoder_destroy(sys->vdp, hwctx->decoder);
    vdp_release_x11(sys->vdp);
    av_freep(&avctx->hwaccel_context);
    free(sys);
}
