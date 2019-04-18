/**
 * @file ac3.c
 * @brief RTP AC-3 and E-AC-3 payload format parser
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
#include "../../packetizer/a52.h"

struct rtp_ac3 {
    struct vlc_logger *logger;
    bool enhanced;
};

struct rtp_ac3_source {
    block_t *frags;
    block_t **frag_end;
    unsigned char missing;
    struct vlc_rtp_es *es;
};

static void *rtp_ac3_begin(struct vlc_rtp_pt *pt)
{
    struct rtp_ac3 *sys = pt->opaque;
    struct rtp_ac3_source *src = malloc(sizeof (*src));
    if (unlikely(src == NULL))
        return NULL;

    src->frags = NULL;
    src->frag_end = &src->frags;

    es_format_t fmt;

    es_format_Init(&fmt, AUDIO_ES, sys->enhanced ? VLC_CODEC_EAC3
                                                 : VLC_CODEC_A52);
    fmt.audio.i_rate = pt->frequency;
    if (!sys->enhanced)
        fmt.audio.i_channels = pt->channel_count ? pt->channel_count : 6;
    src->es = vlc_rtp_pt_request_es(pt, &fmt);
    return src;
}

static void rtp_ac3_end(struct vlc_rtp_pt *pt, void *data)
{
    struct rtp_ac3_source *src = data;

    if (unlikely(src == NULL))
        return;

    vlc_rtp_es_destroy(src->es);
    block_ChainRelease(src->frags);
    free(src);
    (void) pt;
}

static void rtp_ac3_decode_compound(struct vlc_logger *log, bool enhanced,
                                    struct rtp_ac3_source *src,
                                    block_t *block, unsigned int frames)
{
    vlc_a52_header_t hdr;

    if (frames == 0) {
        block_Release(block);
        return;
    }

    while (frames > 1) {
        if (vlc_a52_header_Parse(&hdr, block->p_buffer, block->i_buffer)
         || (hdr.b_eac3 && !enhanced)
         || hdr.i_size > block->i_buffer) {
            vlc_warning(log, "corrupt packet: %u frames, %zu bytes remaining",
                        frames, block->i_buffer);
            block_Release(block);
            return;
        }

        block_t *frame = block_Alloc(hdr.i_size);

        if (likely(frame != NULL)) {
            memcpy(frame->p_buffer, block->p_buffer, hdr.i_size);
            frame->i_pts = block->i_pts;
            vlc_rtp_es_send(src->es, frame);
        }

        block->p_buffer += hdr.i_size;
        block->i_buffer -= hdr.i_size;
        block->i_pts = VLC_TICK_INVALID;
        frames--;
    }

    /* Send the last frame, no need to copy */
    vlc_rtp_es_send(src->es, block);
}

static void rtp_ac3_send(struct rtp_ac3_source *src)
{
    block_t *frags = src->frags;
    block_t *frame = block_ChainGather(frags);

    if (likely(frame != NULL))
        vlc_rtp_es_send(src->es, frame);
    else
        block_ChainRelease(frags);

    src->frags = NULL;
    src->frag_end = &src->frags;
}

static void rtp_ac3_decode(struct vlc_rtp_pt *pt, void *data, block_t *block,
                           const struct vlc_rtp_pktinfo *restrict info)
{
    struct rtp_ac3 *sys = pt->opaque;
    struct rtp_ac3_source *src = data;
    struct vlc_logger *log = sys->logger;

    if (unlikely(src == NULL)) {
drop:   block_Release(block);
        return;
    }

    if (block->i_buffer < 2) {
        vlc_warning(log, "corrupt packet (%zu bytes)", block->i_buffer);
        goto drop;
    }

    /* Payload header: 2 bytes, AC-3 (RFC4184 §4.1.1), E-AC-3 (RFC4598 §4.1) */
    uint_fast8_t ftmask = sys->enhanced ? 1 : 3;
    uint_fast8_t frametype = block->p_buffer[0] & ftmask;
    uint_fast8_t framenum = block->p_buffer[1];

    block->p_buffer += 2;
    block->i_buffer -= 2;

    if (src->frags != NULL && (frametype != ftmask || src->missing == 0)) {
        /* Missed end of previous frame. */
        src->frags->i_flags |= BLOCK_FLAG_CORRUPTED;
        rtp_ac3_send(src);
        vlc_warning(log, "reassembly error: %hhu missing fragments",
                    src->missing);
    }

    if (frametype == 0) {
        rtp_ac3_decode_compound(log, sys->enhanced, src, block, framenum);
        return;
    }

    if (src->frags == NULL) {
        if (frametype == 3) {
            /* Missed start of current frame. Not much to do without header. */
            assert(!sys->enhanced);
            block_Release(block);
            vlc_warning(log, "reassembly error: missing initial fragment");
            return;
        }

        /* Initial fragment of a new frame */
        if (framenum == 0) {
            block_Release(block);
            return;
        }

        src->missing = framenum;
    }

    assert(src->missing > 0);
    src->missing--;
    *src->frag_end = block;
    src->frag_end = &block->p_next;

    if (info->m) {
        if (src->missing != 0) {
            src->frags->i_flags |= BLOCK_FLAG_CORRUPTED;
            vlc_warning(log, "reassembly error: %hhu missing fragments",
                        src->missing);
        }
        rtp_ac3_send(src);
    }
}

static void rtp_ac3_destroy(struct vlc_rtp_pt *pt)
{
    struct rtp_ac3 *sys = pt->opaque;

    free(sys);
}

static const struct vlc_rtp_pt_operations rtp_ac3_ops = {
    rtp_ac3_destroy, rtp_ac3_begin, rtp_ac3_end, rtp_ac3_decode,
};


static int rtp_ac3_open(vlc_object_t *obj, struct vlc_rtp_pt *pt,
                        const struct vlc_sdp_pt *desc)
{
    bool enhanced;

    if (vlc_ascii_strcasecmp(desc->name, "ac3") == 0) /* RFC4184 */
        enhanced = false;
    else
    if (vlc_ascii_strcasecmp(desc->name, "eac3") == 0) /* RFC4598 */
        enhanced = true;
    else
        return VLC_ENOTSUP;

    struct rtp_ac3 *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->logger = vlc_object_logger(obj);
    sys->enhanced = enhanced;
    pt->opaque = sys;
    pt->ops = &rtp_ac3_ops;
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname(N_("RTP AC3"))
    set_description(N_("RTP AC-3 payload parser"))
    set_subcategory(SUBCAT_INPUT_DEMUX)
    set_rtp_parser_callback(rtp_ac3_open)
    add_shortcut("audio/ac3", "audio/eac3")
vlc_module_end()
