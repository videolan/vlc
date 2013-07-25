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

static int Open(vlc_va_t *, int, const es_format_t *);
static void Close(vlc_va_t *);

vlc_module_begin()
    set_description(N_("Dummy video decoding accelerator"))
    set_capability("hw decoder", 0)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)
    set_callbacks(Open, Close)
    add_shortcut("dummy")
vlc_module_end()

#define DECODER_MAGIC 0xdec0dea0
#define DATA_MAGIC    0xda1a0000
#define OPAQUE_MAGIC  0x0da00e00

struct vlc_va_sys_t
{
    AVVDPAUContext context;
};

static int Lock(vlc_va_t *va, AVFrame *ff)
{
    for (unsigned i = 0; i < AV_NUM_DATA_POINTERS; i++)
    {
        ff->data[i] = NULL;
        ff->linesize[i] = 0;
    }

    ff->data[0] = (void *)(uintptr_t)DATA_MAGIC; /* must be non-NULL */
    ff->data[3] = (void *)(uintptr_t)DATA_MAGIC;
    ff->opaque = (void *)(uintptr_t)OPAQUE_MAGIC;
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

static int Copy(vlc_va_t *va, picture_t *pic, AVFrame *ff)
{
    (void) va; (void) ff;

    assert((uintptr_t)ff->data[3] == DATA_MAGIC);
    assert((uintptr_t)ff->opaque == OPAQUE_MAGIC);

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
    vlc_va_sys_t *sys = va->sys;

    (void) width; (void) height;

    *ctxp = &sys->context;
    *chromap = VLC_CODEC_YV12;
    return VLC_SUCCESS;
}

static int Open(vlc_va_t *va, int codec, const es_format_t *fmt)
{
    union
    {
        char str[4];
        vlc_fourcc_t fourcc;
    } u = { .fourcc = fmt->i_codec };

    vlc_va_sys_t *sys = calloc(1, sizeof (*sys));
    if (unlikely(sys == NULL))
       return VLC_ENOMEM;

    msg_Dbg(va, "codec %d (%4.4s) profile %d level %d", codec, u.str,
            fmt->i_profile, fmt->i_level);

    sys->context.decoder = DECODER_MAGIC;
    sys->context.render = Render;

    va->sys = sys;
    va->description = (char *)"Dummy video decoding accelerator";
    va->pix_fmt = AV_PIX_FMT_VDPAU;
    va->setup = Setup;
    va->get = Lock;
    va->release = Unlock;
    va->extract = Copy;
    return VLC_SUCCESS;
}

static void Close(vlc_va_t *va)
{
    vlc_va_sys_t *sys = va->sys;

    av_freep(&sys->context.bitstream_buffers);
    free(sys);
}
