/**
 * @file raw.c
 * @brief Real-Time Protocol raw video payload format parser
 */
/*****************************************************************************
 * Copyright © 2022 Rémi Denis-Courmont
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
#include "fmtp.h"

enum vlc_rtp_colorimetry {
    VLC_RTP_COLOR_UNKNOWN,
    VLC_RTP_COLOR_BT601_5,
    VLC_RTP_COLOR_BT709_2,
    VLC_RTP_COLOR_SMPTE240M,
};

/* video/raw: raw video ES */
struct rtp_raw {
    struct vlc_logger *log;
    char *sampling;
    uint16_t width;
    uint16_t height;
    uint8_t depth;
    enum vlc_rtp_colorimetry colorimetry;
};

static void *rtp_raw_begin(struct vlc_rtp_pt *pt)
{
    struct rtp_raw *sys = pt->opaque;
    es_format_t fmt;

    es_format_Init(&fmt, VIDEO_ES, VLC_CODEC_RTP_VIDEO_RAW);
    fmt.video.i_width = sys->width;
    fmt.video.i_height = sys->height;

    switch (sys->colorimetry) {
        case VLC_RTP_COLOR_UNKNOWN:
            fmt.video.primaries = COLOR_PRIMARIES_UNDEF;
            fmt.video.transfer = TRANSFER_FUNC_UNDEF;
            fmt.video.space = COLOR_SPACE_UNDEF;
            break;
        case VLC_RTP_COLOR_BT601_5:
            fmt.video.primaries = COLOR_PRIMARIES_BT601_525;
            fmt.video.transfer = TRANSFER_FUNC_BT709;
            fmt.video.space = COLOR_SPACE_BT601;
            break;
        case VLC_RTP_COLOR_BT709_2:
            fmt.video.primaries = COLOR_PRIMARIES_BT709;
            fmt.video.transfer = TRANSFER_FUNC_BT709;
            fmt.video.space = COLOR_SPACE_BT709;
            break;
        case VLC_RTP_COLOR_SMPTE240M:
            fmt.video.primaries = COLOR_PRIMARIES_SMTPE_240;
            fmt.video.transfer = TRANSFER_FUNC_SMPTE_240;
            fmt.video.space = COLOR_SPACE_SMPTE_240;
            break;
    }

    fmt.i_level = sys->depth;
    /* Do not allocate anything and do not free anything there: */
    fmt.p_extra = sys->sampling;
    fmt.i_extra = strlen(sys->sampling) + 1;

    return vlc_rtp_pt_request_es(pt, &fmt);
}

static void rtp_raw_end(struct vlc_rtp_pt *pt, void *data)
{
    struct vlc_rtp_es *es = data;

    vlc_rtp_es_destroy(es);
    (void) pt;
}

static void rtp_raw_unwrap(struct vlc_rtp_pt *pt, void *data, block_t *block,
                           const struct vlc_rtp_pktinfo *restrict info)
{
    struct rtp_raw *sys = pt->opaque;
    struct vlc_rtp_es *es = data;

    if (block->i_buffer < 8) {
        vlc_warning(sys->log, "malformatted packet (%zu bytes)",
                    block->i_buffer);
        block_Release(block);
        return;
    }

    /* NOTE: The M bit flags the last packet of a frame or field.
     * It is not currently needed for anything. */
    (void) info;

    /* NOTE: The extended sequence header is ignored here. This would need to
     * be processed in the generic RTP code along the baseline sequence header.
     * We assume that reordering will not too severe to need that. */
    block->p_buffer += 2;
    block->i_buffer -= 2;
    vlc_rtp_es_send(es, block);
    return;
}

static void rtp_raw_close(struct vlc_rtp_pt *pt)
{
    struct rtp_raw *sys = pt->opaque;

    free(sys->sampling);
    free(sys);
}

static const struct vlc_rtp_pt_operations rtp_raw_ops = {
    rtp_raw_close, rtp_raw_begin, rtp_raw_end, rtp_raw_unwrap,
};

static int rtp_raw_open(vlc_object_t *obj, struct vlc_rtp_pt *pt,
                        const struct vlc_sdp_pt *desc)
{
    if (vlc_ascii_strcasecmp(desc->name, "raw") != 0 /* RFC4175 */)
        return VLC_ENOTSUP;

    struct rtp_raw *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->log = obj->logger;

    size_t slen, clen;
    const char *s = vlc_sdp_fmtp_get_str(desc, "sampling", &slen);
    const char *c = vlc_sdp_fmtp_get_str(desc, "colorimetry", &clen);
    if (s == NULL || c == NULL
     || vlc_sdp_fmtp_get(desc, "width", &sys->width)
     || vlc_sdp_fmtp_get(desc, "height", &sys->height)
     || vlc_sdp_fmtp_get(desc, "depth", &sys->depth)) {
        vlc_error(obj->logger, "missing parameters for raw video");
        free(sys);
        return VLC_EINVAL;
    }

    sys->sampling = strndup(s, slen);

    if (clen == 7 && strncmp(c, "BT601-5", 7) == 0)
        sys->colorimetry = VLC_RTP_COLOR_BT601_5;
    else if (clen == 7 && strncmp(c, "BT709-2", 7) == 0)
        sys->colorimetry = VLC_RTP_COLOR_BT709_2;
    else if (clen == 9 && strncmp(c, "SMPTE240M", 9) == 0)
        sys->colorimetry = VLC_RTP_COLOR_SMPTE240M;
    else {
        vlc_warning(sys->log, "unknown colorimetry %.*s", (int)clen, c);
        sys->colorimetry = VLC_RTP_COLOR_UNKNOWN;
    }
    /* TODO: interlacing, chroma location (optional parameters) */

    pt->opaque = sys;
    pt->ops = &rtp_raw_ops;
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname(N_("RTP raw"))
    set_description(N_("RTP raw video payload parser"))
    set_subcategory(SUBCAT_INPUT_DEMUX)
    set_rtp_parser_callback(rtp_raw_open)
    add_shortcut("video/raw")
vlc_module_end()
