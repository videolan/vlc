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

static int Open(vlc_va_t *, int, const es_format_t *);
static void Close(vlc_va_t *);

vlc_module_begin()
    set_description(N_("Video Decode and Presentation API for Unix (VDPAU)"))
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
    VdpDecoderProfile profile;
    AVVDPAUContext context;
    uint16_t width;
    uint16_t height;
};

static int Lock(vlc_va_t *va, void **opaque, uint8_t **data)
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
        return VLC_EGENERIC;
    }

    vlc_vdp_video_field_t *field = vlc_vdp_video_create(sys->vdp, surface);
    if (unlikely(field == NULL))
        return VLC_ENOMEM;

    *data = (void *)(uintptr_t)surface;
    *opaque = field;
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

static int Init(vlc_va_t *va, void **ctxp, vlc_fourcc_t *chromap,
                int width, int height)
{
    vlc_va_sys_t *sys = va->sys;
    VdpStatus err;

    width = (width + 1) & ~1;
    height = (height + 3) & ~3;
    sys->width = width;
    sys->height = height;

    unsigned surfaces = 2;
    switch (sys->profile)
    {
      case VDP_DECODER_PROFILE_H264_BASELINE:
      case VDP_DECODER_PROFILE_H264_MAIN:
      case VDP_DECODER_PROFILE_H264_HIGH:
        surfaces = 16;
        break;
    }

    err = vdp_decoder_create(sys->vdp, sys->device, sys->profile, width,
                             height, surfaces, &sys->context.decoder);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(va, "%s creation failure: %s", "decoder",
                vdp_get_error_string(sys->vdp, err));
        sys->context.decoder = VDP_INVALID_HANDLE;
        return VLC_EGENERIC;
    }

    *ctxp = &sys->context;
    /* TODO: select better chromas when appropriate */
    *chromap = VLC_CODEC_VDPAU_VIDEO_420;
    return VLC_SUCCESS;
}

static void Deinit(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;

    assert(sys->context.decoder != VDP_INVALID_HANDLE);
    vdp_decoder_destroy(sys->vdp, sys->context.decoder);
#if (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55, 13, 0))
    av_freep(&sys->context.bitstream_buffers);
#endif
}

static int Setup(vlc_va_t *va, void **ctxp, vlc_fourcc_t *chromap,
                 int width, int height)
{
    vlc_va_sys_t *sys = va->sys;

    if (sys->context.decoder != VDP_INVALID_HANDLE)
    {
        if (sys->width == width && sys->height == height)
            return VLC_SUCCESS;
        Deinit(va);
        sys->context.decoder = VDP_INVALID_HANDLE;
    }

    return Init(va, ctxp, chromap, width, height);
}

static int Open(vlc_va_t *va, int codec, const es_format_t *fmt)
{
    VdpStatus err;
    VdpDecoderProfile profile;
    int level;

    switch (codec)
    {
      case AV_CODEC_ID_MPEG1VIDEO:
        profile = VDP_DECODER_PROFILE_MPEG1;
        level = VDP_DECODER_LEVEL_MPEG1_NA;
        break;

      case AV_CODEC_ID_MPEG2VIDEO:
        switch (fmt->i_profile)
        {
          case FF_PROFILE_MPEG2_MAIN:
            profile = VDP_DECODER_PROFILE_MPEG2_MAIN;
            break;
          case FF_PROFILE_MPEG2_SIMPLE:
            profile = VDP_DECODER_PROFILE_MPEG2_SIMPLE;
            break;
          default:
            msg_Err(va, "unsupported %s profile %d", "MPEG2", fmt->i_profile);
            return VLC_EGENERIC;
        }
        level = VDP_DECODER_LEVEL_MPEG2_HL;
        break;

      case AV_CODEC_ID_H263:
        profile = VDP_DECODER_PROFILE_MPEG4_PART2_ASP;
        level = VDP_DECODER_LEVEL_MPEG4_PART2_ASP_L5;
        break;
      case AV_CODEC_ID_MPEG4:
        switch (fmt->i_profile)
        {
          case FF_PROFILE_MPEG4_SIMPLE:
            profile = VDP_DECODER_PROFILE_MPEG4_PART2_SP;
            break;
          case FF_PROFILE_MPEG4_ADVANCED_SIMPLE:
            profile = VDP_DECODER_PROFILE_MPEG4_PART2_ASP;
            break;
          default:
            msg_Err(va, "unsupported %s profile %d", "MPEG4", fmt->i_profile);
            return VLC_EGENERIC;
        }
        level = fmt->i_level;
        break;

      case AV_CODEC_ID_H264:
        switch (fmt->i_profile
                        & ~(FF_PROFILE_H264_CONSTRAINED|FF_PROFILE_H264_INTRA))
        {
          case FF_PROFILE_H264_BASELINE:
            profile = VDP_DECODER_PROFILE_H264_BASELINE;
            break;
          case FF_PROFILE_H264_MAIN:
            profile = VDP_DECODER_PROFILE_H264_MAIN;
            break;
          case FF_PROFILE_H264_HIGH:
            profile = VDP_DECODER_PROFILE_H264_HIGH;
            break;
          case FF_PROFILE_H264_EXTENDED:
          default:
            msg_Err(va, "unsupported %s profile %d", "H.264", fmt->i_profile);
            return VLC_EGENERIC;
        }
        level = fmt->i_level;
        if ((fmt->i_profile & FF_PROFILE_H264_INTRA) && (fmt->i_level == 11))
            level = VDP_DECODER_LEVEL_H264_1b;
        break;

      case AV_CODEC_ID_WMV3:
      case AV_CODEC_ID_VC1:
        switch (fmt->i_profile)
        {
          case FF_PROFILE_VC1_SIMPLE:
            profile = VDP_DECODER_PROFILE_VC1_SIMPLE;
            break;
          case FF_PROFILE_VC1_MAIN:
            profile = VDP_DECODER_PROFILE_VC1_MAIN;
            break;
          case FF_PROFILE_VC1_ADVANCED:
            profile = VDP_DECODER_PROFILE_VC1_ADVANCED;
            break;
          default:
            msg_Err(va, "unsupported %s profile %d", "VC-1", fmt->i_profile);
            return VLC_EGENERIC;
        }
        level = fmt->i_level;
        break;

      default:
        msg_Err(va, "unknown codec %d", codec);
        return VLC_EGENERIC;
    }

    if (!vlc_xlib_init(VLC_OBJECT(va)))
    {
        msg_Err(va, "Xlib is required for VDPAU");
        return VLC_EGENERIC;
    }

    vlc_va_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
       return VLC_ENOMEM;

    sys->profile = profile;
    memset(&sys->context, 0, sizeof (sys->context));
    sys->context.decoder = VDP_INVALID_HANDLE;

    err = vdp_get_x11(NULL, -1, &sys->vdp, &sys->device);
    if (err != VDP_STATUS_OK)
    {
        free(sys);
        return VLC_EGENERIC;
    }

    void *func;
    err = vdp_get_proc_address(sys->vdp, sys->device,
                               VDP_FUNC_ID_DECODER_RENDER, &func);
    if (err != VDP_STATUS_OK)
        goto error;
    sys->context.render = func;

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
    va->description = (char *)infos;
    va->pix_fmt = AV_PIX_FMT_VDPAU;
    va->setup = Setup;
    va->get = Lock;
    va->release = Unlock;
    va->extract = Copy;
    return VLC_SUCCESS;

error:
    vdp_release_x11(sys->vdp);
    free(sys);
    return VLC_EGENERIC;
}

static void Close(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;

    if (sys->context.decoder != VDP_INVALID_HANDLE)
        Deinit(va);
    vdp_release_x11(sys->vdp);
    free(sys);
}
