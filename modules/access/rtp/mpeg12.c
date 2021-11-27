/**
 * @file mpeg12.c
 * @brief Real-Time Protocol MPEG 1/2 payload format parser
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
#include <stdint.h>
#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_es.h>
#include <vlc_plugin.h>
#include <vlc_strings.h>

#include "rtp.h"
#include "../../packetizer/mpegaudio.h"

/* audio/MPA: MPEG-1/2 Audio layer I/II/III ES */
struct rtp_mpa {
    block_t *frags;
    block_t **frag_end;
    size_t offset;
    struct vlc_rtp_es *es;
};

static void *rtp_mpa_init(struct vlc_rtp_pt *pt)
{
    struct rtp_mpa *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return NULL;

    sys->frags = NULL;
    sys->frag_end = &sys->frags;
    sys->offset = 0;

    es_format_t fmt;

    es_format_Init(&fmt, AUDIO_ES, VLC_CODEC_MPGA);
    sys->es = vlc_rtp_pt_request_es(pt, &fmt);
    return sys;
}

static void rtp_mpa_send(struct rtp_mpa *sys)
{
    block_t *frags = sys->frags;
    block_t *frame = block_ChainGather(frags);

    if (likely(frame != NULL))
        vlc_rtp_es_send(sys->es, frame);
    else
        block_ChainRelease(frags);

    sys->frags = NULL;
    sys->frag_end = &sys->frags;
    sys->offset = 0;
}

static void rtp_mpa_destroy(struct vlc_rtp_pt *pt, void *data)
{
    struct rtp_mpa *sys = data;

    if (unlikely(sys == NULL))
        return;

    if (sys->frags != NULL)
        rtp_mpa_send(sys);
    vlc_rtp_es_destroy(sys->es);
    free(sys);
    (void) pt;
}

static void rtp_mpa_decode(struct vlc_rtp_pt *pt, void *data, block_t *block,
                           const struct vlc_rtp_pktinfo *restrict info)
{
    struct vlc_logger *log = pt->opaque;
    struct rtp_mpa *sys = data;

    if (unlikely(sys == NULL))
        goto drop;
    if (block->i_buffer < 4) {
        vlc_warning(log, "malformatted packet (%zu bytes)", block->i_buffer);
        goto drop;
    }

    /* 2 bytes unused, 2 bytes fragment offset, then MPEG frame fragment */
    uint_fast16_t offset = GetWBE(block->p_buffer + 2);

    block->p_buffer += 4;
    block->i_buffer -= 4;

    if (offset < sys->offset)
        rtp_mpa_send(sys); /* New frame started: send pending frame if any */

    if (offset != sys->offset) {
        /* Discontinuous offset, probably packet loss. */
        vlc_warning(log, "offset discontinuity: expected %zu, got %"PRIuFAST16,
                    sys->offset, offset);
        block->i_flags |= BLOCK_FLAG_CORRUPTED;
    }

    if (block->i_buffer == 0)
        goto drop;

    if (info->m)
        block->i_flags |= BLOCK_FLAG_DISCONTINUITY;

    *sys->frag_end = block;
    sys->frag_end = &block->p_next;
    sys->offset = offset + block->i_buffer;
    block = sys->frags;

    /* Extract full MPEG Audio frames */
    for (;;) {
        uint32_t header;

        if (block_ChainExtract(block, &header, 4) < 4)
           break;

        /* This RTP payload format does atypically not flag end-of-frames, so
         * we have to parse the MPEG Audio frame header to find out. */
        unsigned chans, conf, mode, srate, brate, samples, maxsize, layer;
        int frame_size = SyncInfo(ntoh32(header), &chans, &conf, &mode, &srate,
                                  &brate, &samples, &maxsize, &layer);
        /* If the frame size is unknown due to free bit rate, then we can only
         * sense the completion of the frame when the next frame starts. */
        if (frame_size <= 0)
            break;

        if ((size_t)frame_size == sys->offset) {
            rtp_mpa_send(sys);
            break;
        }

        /* If the frame size is larger than the current offset, then we need to
         * wait for the next fragment. */
        if ((size_t)frame_size > sys->offset)
            break;

        if (offset != 0) {
            vlc_warning(log, "invalid frame fragmentation");
            block->i_flags |= BLOCK_FLAG_CORRUPTED;
            break;
        }

        /* The RTP packet contains multiple small frames. */
        block_t *frame = block_Alloc(frame_size);
        if (likely(frame != NULL)) {
            assert(block->p_next == NULL); /* Only one block to copy from */
            memcpy(frame->p_buffer, block->p_buffer, frame_size);
            frame->i_flags = block->i_flags;
            frame->i_pts = block->i_pts;
            vlc_rtp_es_send(sys->es, frame);

            block->i_flags = 0;
        } else
            block->i_flags = BLOCK_FLAG_DISCONTINUITY;

        block->p_buffer += frame_size;
        block->i_buffer -= frame_size;
        block->i_pts = VLC_TICK_INVALID;
    }
    return;

drop:
    block_Release(block);
}

static const struct vlc_rtp_pt_operations rtp_mpa_ops = {
    NULL, rtp_mpa_init, rtp_mpa_destroy, rtp_mpa_decode,
};

static int rtp_mpeg12_open(vlc_object_t *obj, struct vlc_rtp_pt *pt,
                        const struct vlc_sdp_pt *desc)
{
    pt->opaque = vlc_object_logger(obj);

    if (vlc_ascii_strcasecmp(desc->name, "MPA") == 0) /* RFC2250 §3.2 */
        pt->ops = &rtp_mpa_ops;
    else
        return VLC_ENOTSUP;

    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname(N_("RTP MPEG"))
    set_description(N_("RTP MPEG-1/2 payload parser"))
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_DEMUX)
    set_rtp_parser_callback(rtp_mpeg12_open)
    add_shortcut("audio/MPA")
vlc_module_end()
