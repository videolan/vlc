/*****************************************************************************
 * hwdummy.c: dummy hardware decoding acceleration plugin for VLC/libav
 *****************************************************************************
 * Copyright (C) 2013 Rémi Denis-Courmont
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
#include <assert.h>

#include <libavutil/mem.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/vdpau.h>
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_fourcc.h>
#include <vlc_picture.h>
#include "../../codec/avcodec/va.h"

static int Open(vlc_va_t *, AVCodecContext *, const es_format_t *);
static void Close(vlc_va_t *);

vlc_module_begin()
    set_description(N_("Dummy video decoder"))
    set_capability("hw decoder", 0)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)
    set_callbacks(Open, Close)
    add_shortcut("dummy")
vlc_module_end()

#define DECODER_MAGIC 0xdec0dea0
#define DATA_MAGIC    0xda1a0000
#define OPAQUE_MAGIC  0x0da00e00

static int Lock(vlc_va_t *va, void **opaque, uint8_t **data)
{
    *data = (void *)(uintptr_t)DATA_MAGIC;
    *opaque = (void *)(uintptr_t)OPAQUE_MAGIC;
    (void) va;
    return VLC_SUCCESS;
}

static void Unlock(void *opaque, uint8_t *data)
{
    assert((uintptr_t)opaque == OPAQUE_MAGIC);
    assert((uintptr_t)data == DATA_MAGIC);
}

static VdpStatus Render(VdpDecoder decoder, VdpVideoSurface target,
                        VdpPictureInfo const *picture_info,
                        uint32_t bitstream_buffer_count,
                        VdpBitstreamBuffer const *bitstream_buffers)
{
    (void) decoder; (void) target; (void) picture_info;
    (void) bitstream_buffer_count; (void) bitstream_buffers;
    assert(decoder == DECODER_MAGIC);
    assert(target == DATA_MAGIC);
    return VDP_STATUS_OK;
}

static int Copy(vlc_va_t *va, picture_t *pic, void *opaque, uint8_t *data)
{
    (void) va;

    assert((uintptr_t)opaque == OPAQUE_MAGIC);
    assert((uintptr_t)data == DATA_MAGIC);

    /* Put some dummy picture content */
    memset(pic->p[0].p_pixels, 0xF0,
           pic->p[0].i_pitch * pic->p[0].i_visible_lines);
    for (int i = 0; i < pic->p[1].i_visible_lines; i++)
        memset(pic->p[1].p_pixels + (i * pic->p[1].i_pitch), i,
               pic->p[1].i_visible_pitch);
    for (int i = 0; i < pic->p[2].i_visible_lines; i++)
        for (int j = 0; j < pic->p[2].i_visible_pitch; j++)
            pic->p[2].p_pixels[(i * pic->p[2].i_pitch) + j] = j;
    return VLC_SUCCESS;
}

static int Setup(vlc_va_t *va, void **ctxp, vlc_fourcc_t *chromap,
                 int width, int height)
{
    (void) width; (void) height;
    *ctxp = (AVVDPAUContext *)va->sys;
    *chromap = VLC_CODEC_YV12;
    return VLC_SUCCESS;
}

static int Open(vlc_va_t *va, AVCodecContext *ctx, const es_format_t *fmt)
{
    union
    {
        char str[4];
        vlc_fourcc_t fourcc;
    } u = { .fourcc = fmt->i_codec };

    AVVDPAUContext *hwctx = av_vdpau_alloc_context();
    if (unlikely(hwctx == NULL))
       return VLC_ENOMEM;

    msg_Dbg(va, "codec %d (%4.4s) profile %d level %d", ctx->codec_id, u.str,
            fmt->i_profile, fmt->i_level);

    hwctx->decoder = DECODER_MAGIC;
    hwctx->render = Render;

    va->sys = (vlc_va_sys_t *)hwctx;
    va->description = "Dummy video decoding accelerator";
    va->pix_fmt = AV_PIX_FMT_VDPAU;
    va->setup = Setup;
    va->get = Lock;
    va->release = Unlock;
    va->extract = Copy;
    return VLC_SUCCESS;
}

static void Close(vlc_va_t *va)
{
    av_free(va->sys);
}
