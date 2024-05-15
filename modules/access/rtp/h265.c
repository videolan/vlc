/**
 * @file h265.c
 */
/*****************************************************************************
 * Copyright (C) 2022 VideoLabs, VLC authors and VideoLAN
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

#include "h26x.h"
#include "fmtp.h"

#define FLAG_DONL 1

#include <vlc_plugin.h>
#include <vlc_codec.h>

struct h265_pt_opaque
{
    block_t *sdpxps;
    bool b_donl;
    vlc_object_t *obj;
};

static void *rtp_h265_init(struct vlc_rtp_pt *pt)
{
    struct h265_pt_opaque *opaque = pt->opaque;
    struct rtp_h26x_sys *sys = malloc(sizeof(*sys));
    if(!sys)
        return NULL;
    rtp_h26x_init(sys);

    es_format_t fmt;
    es_format_Init (&fmt, VIDEO_ES, VLC_CODEC_HEVC);
    fmt.b_packetized = false;

    sys->p_packetizer = demux_PacketizerNew(opaque->obj, &fmt, "rtp packetizer");
    if(!sys->p_packetizer)
    {
        free(sys);
        return NULL;
    }

    sys->es = vlc_rtp_pt_request_es(pt, &sys->p_packetizer->fmt_out);
    if(opaque->sdpxps)
        sys->xps = block_Duplicate(opaque->sdpxps);
    if(opaque->b_donl)
        sys->flags = FLAG_DONL;

    return sys;
}

static void rtp_h265_destroy(struct vlc_rtp_pt *pt, void *data)
{
    VLC_UNUSED(pt);
    struct rtp_h26x_sys *sys = data;
    if(sys)
    {
        if(sys->p_packetizer)
            demux_PacketizerDestroy(sys->p_packetizer);
        vlc_rtp_es_destroy(sys->es);
        rtp_h26x_clear(sys);
        free(sys);
    }
}

static block_t * h265_deaggregate_AP(block_t *block, bool b_donl, bool b_annexb)
{
    size_t total = 0;

    if(b_donl)
    {
        if(block->i_buffer < 4)
        {
            block_Release(block);
            return NULL;
        }
        /* Skip Half DONL, so we always get an extra 1 byte extra prefix */
        block->p_buffer += 1;
        block->i_buffer -= 1;
    }

    const uint8_t *p = block->p_buffer;
    size_t sz = block->i_buffer;

    /* first pass, compute final size */
    while(sz > (b_donl ? 3 : 2))
    {
        /* skip 1/2 DONL or DOND here */
        if(b_donl)
        {
            p += 1;
            sz -= 1;
        }
        size_t nalsz = GetWBE(p);
        if(nalsz + 2 > sz)
            break;
        total += nalsz + 4;
        sz -= (nalsz + 2);
        p += (nalsz + 2);
    }
    block_t *newblock = block_Alloc(total);
    if(newblock)
    {
        uint8_t *dst = newblock->p_buffer;
        p = block->p_buffer;
        sz = block->i_buffer;
        while(sz > (b_donl ? 3 : 2))
        {
            /* skip 1/2 DONL or DOND here */
            if(b_donl)
            {
                p += 1;
                sz -= 1;
            }
            size_t nalsz = GetWBE(p);
            if(nalsz + 2 > sz)
                break;
            if(b_annexb)
                memcpy(dst, annexbheader, 4);
            else
                SetDWBE(dst, nalsz);
            dst += 4;
            memcpy(dst, p + 2, nalsz);
            dst += nalsz;
            sz -= (nalsz + 2);
            p += (nalsz + 2);
        }
        block_Release(block);
        block = newblock;
    }
    else
    {
        block_Release(block);
        block = NULL;
    }
    return block;
}

static void rtp_h265_decode (struct vlc_rtp_pt *pt, void *data, block_t *block,
                             const struct vlc_rtp_pktinfo *restrict info)
{
    VLC_UNUSED(pt);
    struct rtp_h26x_sys *sys = data;
    const bool b_au_end = info->m;

    if(block->i_buffer < 2)
    {
        block_Release(block);
        return;
    }

    const uint8_t type = (block->p_buffer[0] & 0x7E) >> 1;
    const uint8_t layerID = ((block->p_buffer[0] & 0x01) << 5) | (block->p_buffer[1] >> 3);
    const uint8_t temporalID = block->p_buffer[1] & 0x07;
    const vlc_tick_t pts = block->i_pts;

    switch(type)
    {
        case 48: /* AP */
            /* end unfinished aggregates */
            h26x_output_blocks(sys, true);
            /* skip header */
            block->i_buffer -= 2;
            block->p_buffer += 2;
            block = h265_deaggregate_AP(block, sys->flags & FLAG_DONL, true);
            h26x_output(sys, block, pts, true, b_au_end);
            break;
        case 49: /* FU */
        {
            if(block->i_buffer < ((sys->flags & FLAG_DONL) ? 6 : 4))
                goto drop;
            const bool start = block->p_buffer[2] & 0x80;
            const bool end = block->p_buffer[2] & 0x40;
            const uint8_t futype = block->p_buffer[2] & 0x3f;

            /* skip FU header and rebuild NAL header */
            if(start)
            {
                /* end unfinished aggregates */
                h26x_output_blocks(sys, true);
                /* rebuild NAL header, overwriting last PAYL[DON]FU 2 bytes */
                if(sys->flags & FLAG_DONL)
                {
                    block->i_buffer -= 3;
                    block->p_buffer += 3;
                }
                else
                {
                    block->i_buffer -= 1;
                    block->p_buffer += 1;
                }
                block->p_buffer[0] = (futype << 1) | (layerID >> 5);
                block->p_buffer[1] = (layerID << 3) | temporalID;
                sys->pts = pts;
            }
            else /* trail data */
            {
                block->i_buffer -= (sys->flags & FLAG_DONL) ? 5 : 3;
                block->p_buffer += (sys->flags & FLAG_DONL) ? 5 : 3;
            }
            block_ChainLastAppend(&sys->pp_packets_next, block);
            if(end)
            {
                block_t *out = block_ChainGather(sys->p_packets);
                sys->p_packets = NULL;
                sys->pp_packets_next = &sys->p_packets;
                out = h26x_wrap_prefix(out, true);
                h26x_output(sys, out, pts, true, b_au_end);
            }
            break;
        }
        case 50: /* PACI */
        {
            if(block->i_buffer < 5 || (block->p_buffer[3] & 0x01))
                goto drop;
            const uint8_t cType = ((block->p_buffer[2] & 0x7f) >> 1);
            const uint8_t PHSize = ((block->p_buffer[2] & 0x01) << 7)
                                 | (block->p_buffer[3] >> 4);
            if(PHSize >= block->i_buffer - 4)
                goto drop;
            /* skip extension, and add the payload header */
            block->p_buffer[0] = (block->p_buffer[0] & 0x81) | (cType << 1);
            block->p_buffer[4 + PHSize - 2 + 0] = block->p_buffer[0];
            block->p_buffer[4 + PHSize - 2 + 1] = block->p_buffer[1];
            block->p_buffer += 4 + PHSize;
            block->i_buffer -= 4 + PHSize;
            /* pass to original payload handler */
            rtp_h265_decode(pt, data, block, info);
            break;
        }
        default:
            /* end unfinished aggregates */
            h26x_output_blocks(sys, true);
            /* raw single NAL */
            if(sys->flags & FLAG_DONL) /* optional DONL */
            {
                if(block->i_buffer < 4)
                    goto drop;
                /* NALHDR DONL NALPAYL -> NALHDR NALPAYL */
                block->p_buffer[2] = block->p_buffer[0];
                block->p_buffer[3] = block->p_buffer[1];
                block->p_buffer += 2;
                block->i_buffer -= 2;
            }
            block = h26x_wrap_prefix(block, true);
            h26x_output(sys, block, pts, true, b_au_end);
    }
    return;

drop:
    if(block)
        block_Release(block);
}

static void rtp_h265_release(struct vlc_rtp_pt *pt)
{
    struct h265_pt_opaque *opaque = pt->opaque;
    if(opaque->sdpxps)
        block_Release(opaque->sdpxps);
    free(opaque);
}

static const struct vlc_rtp_pt_operations rtp_h265_ops = {
    rtp_h265_release, rtp_h265_init, rtp_h265_destroy, rtp_h265_decode,
};

static int rtp_h265_open(vlc_object_t *obj, struct vlc_rtp_pt *pt,
                         const struct vlc_sdp_pt *desc)
{
    if (vlc_ascii_strcasecmp(desc->name, "H265") == 0)
        pt->ops = &rtp_h265_ops;
    else
        return VLC_ENOTSUP;

    struct h265_pt_opaque *opaque = calloc(1, sizeof(*opaque));
    if(!opaque)
        return VLC_ENOMEM;
    pt->opaque = opaque;

    opaque->obj = obj;

    uint16_t don_diff;
    if(!vlc_sdp_fmtp_get(desc, "sprop-max-don-diff", &don_diff))
        opaque->b_donl = (don_diff > 0);

    block_t **append = &opaque->sdpxps;
    const char *props[] = { "sprop-vps", "sprop-sps", "sprop-pps" };
    for(size_t i=0; i<ARRAY_SIZE(props); i++) {
        size_t len;
        const char *value = vlc_sdp_fmtp_get_str(desc, props[i], &len);
        if(!value || len == 0)
            continue;
        block_t *xps = h26x_fillextradata(value);
        if(xps)
            block_ChainLastAppend(&append, xps);
    }
    if(opaque->sdpxps)
        opaque->sdpxps = block_ChainGather(opaque->sdpxps);

    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname(N_("RTP H265"))
    set_description(N_("RTP H265 payload parser"))
    set_subcategory(SUBCAT_INPUT_DEMUX)
    set_rtp_parser_callback(rtp_h265_open)
    add_shortcut("video/H265")
vlc_module_end()
