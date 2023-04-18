/**
 * @file pcm.c
 * @brief Real-Time Protocol (RTP) sample-based audio
 * This files defines the same-based RTP payload formats (mostly from RFC3551)
 * including linear, logarithmic and adaptive audio formats.
 */
/*****************************************************************************
 * Copyright © 2021 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <assert.h>
#include <stdbit.h>
#include <stdint.h>
#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_block.h>
#include <vlc_es.h>
#include <vlc_plugin.h>
#include <vlc_strings.h>

#include "rtp.h"
#define RTP_MAX_CHANS 6

struct rtp_pcm {
    vlc_fourcc_t fourcc;
    uint16_t channel_mask;
    uint8_t sample_bits;
    uint8_t channel_count;
    bool channel_reorder;
    uint8_t channel_map[RTP_MAX_CHANS];
};

static void *rtp_pcm_init(struct vlc_rtp_pt *pt)
{
    struct rtp_pcm *sys = pt->opaque;
    es_format_t fmt;

    es_format_Init(&fmt, AUDIO_ES, sys->fourcc);
    fmt.audio.i_rate = pt->frequency;
    fmt.audio.i_physical_channels = sys->channel_mask;
    fmt.audio.i_channels = sys->channel_count;
    aout_FormatPrepare(&fmt.audio);
    return vlc_rtp_pt_request_es(pt, &fmt);
}

static void rtp_pcm_destroy(struct vlc_rtp_pt *pt, void *data)
{
    struct vlc_rtp_es *es = data;

    vlc_rtp_es_destroy(es);
    (void) pt;
}

static void rtp_pcm_reorder(void *restrict out, const void *restrict in,
                            size_t frames, size_t sample_size,
                            size_t channels, const uint8_t *restrict map)
{
    unsigned char *outp = out;
    const unsigned char *inp = in;
    const size_t frame_size = sample_size * channels;

    if (sample_size == 0 || sample_size > 3)
        vlc_assert_unreachable(); /* Let compiler optimise the memcpy(). */

    for (size_t i = 0; i < frames; i++) {
        for (size_t j = 0; j < channels; j++) {
             memcpy(outp + (sample_size * map[j]), inp, sample_size);
             inp += sample_size;
        }

        outp += frame_size;
    }
}

static void rtp_pcm_decode(struct vlc_rtp_pt *pt, void *data, block_t *block,
                           const struct vlc_rtp_pktinfo *restrict info)
{
    struct rtp_pcm *sys = pt->opaque;
    struct vlc_rtp_es *es = data;
    const size_t frame_bits = sys->channel_count * sys->sample_bits;
    size_t frames = (8 * block->i_buffer) / frame_bits;

    block->i_buffer = ((frames * frame_bits) + 7) / 8;
    block->i_dts = VLC_TICK_INVALID;

    if (info->m)
        block->i_flags |= BLOCK_FLAG_DISCONTINUITY;

    if (sys->channel_reorder) {
        block_t *reordered = block_Alloc(block->i_buffer);

        assert((sys->sample_bits % 8) == 0);

        if (likely(reordered != NULL)) {
            block_CopyProperties(reordered, block);
            rtp_pcm_reorder(reordered->p_buffer, block->p_buffer, frames,
                            sys->sample_bits / 8, sys->channel_count,
                            sys->channel_map);

        }

        block_Release(block);
        block = reordered;

        if (unlikely(block == NULL))
            return;
    }

    vlc_rtp_es_send(es, block);
}

static void rtp_pcm_release(struct vlc_rtp_pt *pt)
{
    struct rtp_pcm *sys = pt->opaque;

    free(sys);
}

static const struct vlc_rtp_pt_operations rtp_pcm_ops = {
    rtp_pcm_release, rtp_pcm_init, rtp_pcm_destroy, rtp_pcm_decode,
};


static void *rtp_g722_init(struct vlc_rtp_pt *pt)
{
    struct rtp_pcm *sys = pt->opaque;
    es_format_t fmt;

    es_format_Init(&fmt, AUDIO_ES, VLC_CODEC_ADPCM_G722);
    /* For historical reasons, the RTP clock rate is 8000 Hz
     * but the sampling rate really is 16000 Hz. */
    fmt.audio.i_rate = 16000;
    fmt.audio.i_physical_channels = sys->channel_mask;
    fmt.audio.i_channels = sys->channel_count;
    aout_FormatPrepare(&fmt.audio);
    return vlc_rtp_pt_request_es(pt, &fmt);
}

static const struct vlc_rtp_pt_operations rtp_g722_ops = {
    rtp_pcm_release, rtp_g722_init, rtp_pcm_destroy, rtp_pcm_decode,
};


static void *rtp_g726_init(struct vlc_rtp_pt *pt)
{
    struct rtp_pcm *sys = pt->opaque;
    es_format_t fmt;

    es_format_Init(&fmt, AUDIO_ES, sys->fourcc);
    fmt.audio.i_rate = 8000;
    fmt.audio.i_physical_channels = sys->channel_mask;
    fmt.audio.i_channels = sys->channel_count;
    /* By VLC convention, the decoder knows the bit depth from the bit rate. */
    fmt.i_bitrate = sys->sample_bits * sys->channel_count * 8000;
    aout_FormatPrepare(&fmt.audio);
    return vlc_rtp_pt_request_es(pt, &fmt);
}

static const struct vlc_rtp_pt_operations rtp_g726_ops = {
    rtp_pcm_release, rtp_g726_init, rtp_pcm_destroy, rtp_pcm_decode,
};


static const uint32_t channel_masks[] = {
    /* By default, there is only one channel. */
    AOUT_CHAN_CENTER,
    /*
     * RTP/AVP recommends AIFF-C channel order by default (RFC3551 §4.1).
     * For 1-4 channel(s), this works well.
     */
    AOUT_CHAN_CENTER, AOUT_CHANS_2_0, AOUT_CHANS_3_0, AOUT_CHANS_3_1,
    /* AIFF-C says for 5 channels, and RFC says 3.2. We assume normal 5.0. */
    AOUT_CHANS_5_0,
    /* Accordingly for 6 channels, we assume normal 5.1 instead of AIFF-C's. */
    AOUT_CHANS_5_1,
};

static const uint32_t channel_order[] = {
    AOUT_CHAN_LEFT, AOUT_CHAN_REARLEFT, AOUT_CHAN_CENTER,
    AOUT_CHAN_RIGHT, AOUT_CHAN_REARRIGHT, AOUT_CHAN_LFE, 0,
};

/* RTP puts right before center for 3.0, but center before right for 3.1! */
static const uint32_t channel_order_3[RTP_MAX_CHANS] = {
    AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT, AOUT_CHAN_CENTER,
};

static_assert (ARRAY_SIZE(channel_masks) == RTP_MAX_CHANS + 1, "Bad masks");
static_assert (ARRAY_SIZE(channel_order) == RTP_MAX_CHANS + 1, "Bad order");

static int rtp_pcm_open(vlc_object_t *obj, struct vlc_rtp_pt *pt,
                        const struct vlc_sdp_pt *desc)
{
    vlc_fourcc_t fourcc;
    unsigned bits;
    const struct vlc_rtp_pt_operations *ops = &rtp_pcm_ops;

    if (vlc_ascii_strcasecmp(desc->name, "L8") == 0) {
        fourcc = VLC_CODEC_U8; /* RFC3551 §4.5.10 */
        bits = 8;

    } else if (vlc_ascii_strcasecmp(desc->name, "L16") == 0) {
        fourcc = VLC_CODEC_S16B; /* RFC3551 §4.5.11 */
        bits = 16;

    } else if (vlc_ascii_strcasecmp(desc->name, "L20") == 0) {
        fourcc = VLC_CODEC_S20B; /* RFC3190 §4 */
        bits = 20;

    } else if (vlc_ascii_strcasecmp(desc->name, "L24") == 0) {
        fourcc = VLC_CODEC_S24B; /* RFC3190 §4 */
        bits = 24;

    } else if (vlc_ascii_strcasecmp(desc->name, "PCMA") == 0) {
        fourcc = VLC_CODEC_ALAW; /* RFC3551 §4.5.14 */
        bits = 8;

    } else if (vlc_ascii_strcasecmp(desc->name, "PCMU") == 0) {
        fourcc = VLC_CODEC_MULAW; /* RFC3551 §4.5.14 */
        bits = 8;

    } else if (vlc_ascii_strcasecmp(desc->name, "G722") == 0) {
        fourcc = VLC_CODEC_ADPCM_G722; /* RFC3551 §4.5.2 */
        bits = 8;
        ops = &rtp_g722_ops;

    } else if (sscanf(desc->name, "G726-%u", &bits) == 1
            || sscanf(desc->name, "g726-%u", &bits) == 1) {
        fourcc = VLC_CODEC_ADPCM_G726_LE; /* RFC3551 §4.5.4 */
        bits /= 8;
        ops = &rtp_g726_ops;

        if (unlikely(bits < 16 || bits % 8 || bits > 40))
            return VLC_ENOTSUP;

    } else if (vlc_ascii_strcasecmp(desc->name, "DAT12") == 0) {
        fourcc = VLC_CODEC_DAT12; /* RFC3190 §3 */
        bits = 12;

    } else if (vlc_ascii_strcasecmp(desc->name, "32kadpcm") == 0) {
        fourcc = VLC_CODEC_ADPCM_G726; /* RFC2422 */
        bits = 4;
        ops = &rtp_g726_ops;

    } else
        return VLC_ENOTSUP;

    struct rtp_pcm *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->fourcc = fourcc;
    sys->channel_count = desc->channel_count ? desc->channel_count : 1;
    sys->channel_reorder = false;
    sys->sample_bits = bits;

    if (desc->channel_count < ARRAY_SIZE(channel_masks)) {
        sys->channel_mask = channel_masks[desc->channel_count];
        assert(stdc_count_ones(sys->channel_mask) == sys->channel_count);

        /* Octet-unaligned formats cannot readily be reordered, especially in
         * foreign endianness. The decoder will take care of that. */
        if ((bits % 8) == 0) {
            const uint32_t *order = channel_order;

            if (desc->channel_count == 3)
                order = channel_order_3;

            if (aout_CheckChannelReorder(order, NULL, sys->channel_mask,
                                         sys->channel_map))
                sys->channel_reorder = true;
        }
    } else {
        msg_Warn(obj, "unknown %hhu-channels layout", desc->channel_count);
        sys->channel_mask = 0;
    }

    pt->opaque = sys;
    pt->ops = ops;
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname(N_("RTP PCM"))
    set_description(N_("RTP PCM payload parser"))
    set_subcategory(SUBCAT_INPUT_DEMUX)
    set_rtp_parser_callback(rtp_pcm_open)
    add_shortcut("audio/L8", "audio/L16", "audio/L20", "audio/L24",
                 "audio/DAT12", "audio/PCMA", "audio/PCMU", "audio/G722",
                 "audio/G726-16", "audio/G726-24", "audio/G726-32",
                 "audio/G726-40", "audio/32kadpcm")
    /* TODO? DVI4, VDVI */
vlc_module_end()
