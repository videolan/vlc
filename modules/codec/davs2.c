/*****************************************************************************
 * davs2.c: AVS2-P2 video decoder using libdavs2
 *****************************************************************************
 * Copyright © 2021 Rémi Denis-Courmont
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

/*****************************************************************************
 * NOTA BENE: this module requires the linking against a library which is
 * known to require licensing under the GNU General Public License version 2
 * (or later). Therefore, the result of compiling this module will normally
 * be subject to the terms of that later license.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <davs2.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_frame.h>
#include <vlc_codec.h>
#include <vlc_cpu.h>

static void UpdateFormat(decoder_t *dec, const davs2_seq_info_t *info)
{
    vlc_fourcc_t chroma = 0;
    video_format_t *fmt = &dec->fmt_out.video;

    switch (info->output_bit_depth) {
         case 8:
             switch (info->chroma_format) {
                 case 1:
                     chroma = VLC_CODEC_I420;
                     break;
                 case 2:
                     chroma = VLC_CODEC_I422;
                     break;
                 default:
                     msg_Err(dec, "unsupported chromatic sampling: %"PRIu32,
                             info->chroma_format);
             }
             break;
         case 10:
             switch (info->chroma_format) {
#ifdef WORDS_BIGENDIAN
                 case 1:
                     chroma = VLC_CODEC_I420_10B;
                     break;
                 case 2:
                     chroma = VLC_CODEC_I422_10B;
                     break;
#else
                 case 1:
                     chroma = VLC_CODEC_I420_10L;
                     break;
                 case 2:
                     chroma = VLC_CODEC_I422_10L;
                     break;
#endif
                 default:
                     msg_Err(dec, "unsupported chromatic sampling: %"PRIu32,
                             info->chroma_format);
             }
             break;
         default:
             msg_Err(dec, "unsupported bit depth: %"PRIu32,
                     info->output_bit_depth);
    }

    dec->fmt_out.i_codec = chroma;
    video_format_Init(fmt, chroma);
    fmt->i_width = info->width;
    fmt->i_height = info->height;
    fmt->i_visible_width = info->width;
    fmt->i_visible_height = info->height;
    fmt->i_sar_num = 1;
    fmt->i_sar_den = 1;
    assert(info->frame_rate_id != 0);

    static const struct {
        uint16_t num;
        uint16_t den;
    } frame_rates[] = {
        { 24000, 1001 }, { 24, 1 }, { 25, 1 }, { 30000, 1001 }, { 30, 1 },
        { 50, 1 }, { 60000, 1001 }, { 60, 1 }
    };

    if (info->frame_rate_id <= ARRAY_SIZE(frame_rates)) {
        fmt->i_frame_rate = frame_rates[info->frame_rate_id - 1].num;
        fmt->i_frame_rate_base = frame_rates[info->frame_rate_id - 1].den;
    } else {
        msg_Warn(dec, "unknown frame rate %f (ID: %"PRIu32")",
                 info->frame_rate, info->frame_rate_id);
        fmt->i_frame_rate = lroundf(ldexpf(info->frame_rate, 16));
        fmt->i_frame_rate_base = 1 << 16;
    }
}

static void SendPicture(decoder_t *dec, davs2_picture_t *frame)
{
    picture_t *pic = decoder_NewPicture(dec);

    if (likely(pic != NULL)) {
        for (int i = 0; i < pic->i_planes; i++) {
            /*
             * As of davs2 1.6.0, passing custom output picture buffers is not
             * supported, so we have to copy here.
             */
            plane_t plane = {
                .p_pixels = frame->planes[i],
                .i_lines = frame->lines[i],
                .i_pitch = frame->strides[i],
                .i_pixel_pitch = (frame->bit_depth + 7) / 8,
                .i_visible_lines = frame->lines[i],
                .i_visible_pitch = frame->strides[i],
            };

            plane_CopyPixels(pic->p + i, &plane);
        }

        pic->date = frame->pts;
        decoder_QueueVideo(dec, pic);
    }
}

static void Flush(decoder_t *dec)
{
    void *hd = dec->p_sys;
    int ret;

    do {
        davs2_seq_info_t info;
        davs2_picture_t frame;

        ret = davs2_decoder_flush(hd, &info, &frame);
        davs2_decoder_frame_unref(hd, &frame);
    } while (ret != DAVS2_ERROR && ret != DAVS2_END);
}

static int Dequeue(decoder_t *dec,
    int (*func)(void *, davs2_seq_info_t *, davs2_picture_t *))
{
    davs2_seq_info_t info;
    davs2_picture_t frame;
    int ret = func(dec->p_sys, &info, &frame);

    switch (ret) {
        case DAVS2_ERROR:
            msg_Err(dec, "decoding error");
            break;

        case DAVS2_DEFAULT:
        case DAVS2_END:
            break;

        case DAVS2_GOT_HEADER:
            UpdateFormat(dec, &info);
            break;

        case DAVS2_GOT_FRAME:
            if (decoder_UpdateVideoFormat(dec) == 0)
                SendPicture(dec, &frame);
            break;

        default:
            vlc_assert_unreachable();
    }

    davs2_decoder_frame_unref(dec->p_sys, &frame);
    return ret;
}

static int Decode(decoder_t *dec, vlc_frame_t *block)
{
    if (block == NULL) {
        /* Drain */
        int ret;

        do
            ret = Dequeue(dec, davs2_decoder_flush);
        while (ret != DAVS2_ERROR && ret != DAVS2_END);

        return VLCDEC_SUCCESS;
    }

    if (!(block->i_flags & BLOCK_FLAG_CORRUPTED)) {
        assert(block->i_buffer <= INT_MAX);

        davs2_packet_t packet = {
            .data = block->p_buffer,
            .len = block->i_buffer,
            .pts = block->i_pts,
            .dts = block->i_dts,
        };

        if (davs2_decoder_send_packet(dec->p_sys, &packet) == DAVS2_ERROR)
            msg_Err(dec, "decoding error");
    }

    block_Release(block);

    Dequeue(dec, davs2_decoder_recv_frame);
    return VLCDEC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    decoder_t *dec = (decoder_t *)obj;

    davs2_decoder_close(dec->p_sys);
}

static int Open(vlc_object_t *obj)
{
    decoder_t *dec = (decoder_t *)obj;

    if (dec->fmt_in->i_codec != VLC_CODEC_CAVS2)
        return VLC_EGENERIC;

    davs2_param_t params = {
        .threads = vlc_GetCPUCount(),
    };

    if (params.threads > 8)
        params.threads = 8;

    dec->p_sys = davs2_decoder_open(&params);
    if (dec->p_sys == NULL) {
        msg_Err(obj, "decoder opening error");
        return VLC_EGENERIC;
    }

    dec->pf_decode = Decode;
    dec->pf_flush = Flush;
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname("davs2")
    set_description(N_("AVS2 decoder (using davs2)"))
    set_capability("video decoder", 200)
    set_callbacks(Open, Close)
    set_subcategory(SUBCAT_INPUT_VCODEC)
vlc_module_end()
