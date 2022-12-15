/**
 * @file h264.c
 */
/*****************************************************************************
 * Copyright (C) 2021 VideoLabs, VLC authors and VideoLAN
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_block.h>
#include <vlc_strings.h>
#include <vlc_codec.h>

#include "rtp.h"
#include "sdp.h"

struct rtp_h26x_sys
{
    vlc_tick_t pts;
    block_t **pp_packets_next;
    block_t *p_packets;
    block_t *xps;
    struct vlc_rtp_es *es;
};

static void rtp_h26x_clear(struct rtp_h26x_sys *sys)
{
    block_ChainRelease(sys->p_packets);
    if(sys->xps)
        block_Release(sys->xps);
}

static void rtp_h26x_init(struct rtp_h26x_sys *sys)
{
    sys->pts = VLC_TICK_INVALID;
    sys->p_packets = NULL;
    sys->pp_packets_next = &sys->p_packets;
    sys->xps = NULL;
    sys->es = NULL;
}

static const uint8_t annexbheader[] = { 0, 0, 0, 1 };

static block_t * h26x_wrap_prefix(block_t *block, bool b_annexb)
{
    block = block_Realloc(block, 4, block->i_buffer);
    if(block)
    {
        if(b_annexb)
            memcpy(block->p_buffer, annexbheader, 4);
        else
            SetDWBE(block->p_buffer, block->i_buffer - 4);
    }
    return block;
}

static void h26x_extractbase64xps(const char *psz64,
                                  const char *pszend,
                                  void(*pf_output)(void *, uint8_t *, size_t),
                                  void *outputsys)
{
    do
    {
        psz64 += strspn(psz64, " ");
        uint8_t *xps = NULL;
        size_t xpssz = vlc_b64_decode_binary(&xps, psz64);
        pf_output(outputsys, xps, xpssz);
        psz64 = strchr(psz64, ',');
        if(psz64)
            ++psz64;
    } while(psz64 && *psz64 && psz64 < pszend);
}

static void h264_add_xps(void *priv, uint8_t *xps, size_t xpssz)
{
    block_t *b = block_heap_Alloc(xps, xpssz);
    if(!b || !(b = h26x_wrap_prefix(b, true)))
        return;

    block_t ***ppp_append = priv;
    **ppp_append = b;
    *ppp_append = &((**ppp_append)->p_next);
}

static block_t * h264_fillextradata (const char *psz)
{
    block_t *xps = NULL;
    block_t **pxps = &xps;
    h26x_extractbase64xps(psz, strchr(psz, ';'), h264_add_xps, &pxps);
    if(xps)
        xps = block_ChainGather(xps);
    return xps;
}

static void *rtp_h264_init(struct vlc_rtp_pt *pt)
{
    block_t *sdpparams = pt->opaque;
    struct rtp_h26x_sys *sys = malloc(sizeof(*sys));
    if(!sys)
        return NULL;
    rtp_h26x_init(sys);

    es_format_t fmt;
    es_format_Init (&fmt, VIDEO_ES, VLC_CODEC_H264);
    fmt.b_packetized = false;

    sys->es = vlc_rtp_pt_request_es(pt, &fmt);
    if(sdpparams)
        sys->xps = block_Duplicate(sdpparams);

    return sys;
}

static void rtp_h264_destroy(struct vlc_rtp_pt *pt, void *data)
{
    VLC_UNUSED(pt);
    struct rtp_h26x_sys *sys = data;
    if(sys)
    {
        vlc_rtp_es_destroy(sys->es);
        rtp_h26x_clear(sys);
        free(sys);
    }
}

static block_t * h264_deaggregate_STAP(block_t *block, bool b_annexb)
{
    size_t total = 0;
    /* first pass, compute final size */
    const uint8_t *p = block->p_buffer;
    size_t sz = block->i_buffer;
    while(sz > 1)
    {
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
        while(sz > 1)
        {
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
    return block;
}

static block_t * h264_chainsplit_MTAP(block_t *block, bool b_24ext,
                                      bool b_annexb)
{
    const uint8_t *p = block->p_buffer;
    size_t sz = block->i_buffer;
    const uint8_t tssz = b_24ext ? 3 : 2;
    block_t *p_chain = NULL, **pp_chain_append = &p_chain;
    if(sz > 3)
    {
        /* Skip Header and DONB */
        p += 3;
        sz -= 3;
        /* Sz, DOND, TS Offset 16/24 */
        const uint8_t payloadhdrsz = 2 + 1 + tssz;
        while(sz > payloadhdrsz)
        {
            size_t nalsz = GetWBE(p);
            if(payloadhdrsz + nalsz > sz)
                break;
            uint32_t tsoffset = 0;
            for(uint8_t i=0; i< tssz; i++)
                tsoffset = (tsoffset << 8) | p[2 + 1 + i];
            block_t *out = block_Alloc(nalsz + 4);
            if(out)
            {
                memcpy(&out->p_buffer[4], &p[payloadhdrsz], nalsz);
                if(b_annexb)
                    memcpy(out->p_buffer, annexbheader, 4);
                else
                    SetDWBE(out->p_buffer, nalsz);
                /* store offset as length */
                out->i_pts = block->i_pts + vlc_tick_from_samples(tsoffset, 90000);
                block_ChainLastAppend(&pp_chain_append, out);
            }
            p += payloadhdrsz + nalsz;
            sz -= payloadhdrsz + nalsz;
        }
    }
    block_Release(block);
    return p_chain;
}

static void h26x_output(struct rtp_h26x_sys *sys,
                        block_t *block,
                        vlc_tick_t pts, bool pcr, bool au_end)
{
//    if(pcr)
//        es_out_SetPCR(out, pts);

    if(!block)
        return;

    if(sys->xps)
    {
        block_t *xps = sys->xps;
        sys->xps = NULL;
        h26x_output(sys, xps, pts, pcr, false);
    }

    block->i_pts = pts;
    block->i_dts = VLC_TICK_INVALID; /* RTP does not specify this */
    if(au_end)
        block->i_flags |= BLOCK_FLAG_AU_END;
    vlc_rtp_es_send(sys->es, block);
}

static void h26x_output_blocks(struct rtp_h26x_sys *sys, bool b_annexb)
{
    if(!sys->p_packets)
        return;
    block_t *out = block_ChainGather(sys->p_packets);
    sys->p_packets = NULL;
    sys->pp_packets_next = &sys->p_packets;
    out = h26x_wrap_prefix(out, b_annexb);
    h26x_output(sys, out, sys->pts, true, false);
}

static void rtp_h264_decode(struct vlc_rtp_pt *pt, void *data, block_t *block,
                            const struct vlc_rtp_pktinfo *restrict info)
{
    VLC_UNUSED(pt);
    struct rtp_h26x_sys *sys = data;
    const bool b_au_end = info->m;

    if(block->i_buffer < 2)
        goto drop;

    const uint8_t type = block->p_buffer[0] & 0x1F;
    const vlc_tick_t pts = block->i_pts;
    switch(type)
    {
        case 24: /* STAP-A */
            /* end unfinished aggregates */
            h26x_output_blocks(sys, true);
            /* skip header */
            block->i_buffer -= 1;
            block->p_buffer += 1;
            block = h264_deaggregate_STAP(block, true);
            h26x_output(sys, block, pts, true, b_au_end);
            break;
        case 25: /* STAP-B */
            /* TODO */
            goto drop;
        case 26: /* MTAP16 */
        case 27:
        {
            block = h264_chainsplit_MTAP(block, type == 27, true);
            /* result is a chain */
            while(block)
            {
                block_t *p_next = block->p_next;
                block->p_next = NULL;
                h26x_output(sys, block, block->i_pts, true, b_au_end && !p_next);
                block = p_next;
            }
            break;
        }
        case 28: /* FU-A */
        case 29: /* FU-B */
        {
            if(block->i_buffer < 3)
                goto drop;
            const bool start = block->p_buffer[1] & 0x80;
            const bool end = block->p_buffer[1] & 0x40;
            const uint8_t naltype = block->p_buffer[1] & 0x1f;
            /* skip FU header and rebuild NAL header */
            if(start)
            {
                /* end unfinished aggregates */
                h26x_output_blocks(sys, true);
                /* rebuild NAL header */
                block->p_buffer[1] = (block->p_buffer[0] & 0xE0) | naltype;
                block->i_buffer -= 1;
                block->p_buffer += 1;
                sys->pts = pts;
            }
            else /* trail data */
            {
                block->i_buffer -= 2;
                block->p_buffer += 2;
            }
            block_ChainLastAppend(&sys->pp_packets_next, block);
            if(end)
            {
                block_t *out = block_ChainGather(sys->p_packets);
                sys->p_packets = NULL;
                sys->pp_packets_next = &sys->p_packets;
                out = h26x_wrap_prefix(out, true);
                h26x_output(sys, out, sys->pts, true, b_au_end);
            }
            break;
        }
        default:
            /* end unfinished aggregates */
            h26x_output_blocks(sys, true);
            if(type > 0 && type < 24) /* raw single NAL */
            {
                block = h26x_wrap_prefix(block, true);
                h26x_output(sys, block, pts, true, b_au_end);
            }
            else goto drop;
    }
    return;

drop:
    block_Release(block);
}

static void rtp_h264_release(struct vlc_rtp_pt *pt)
{
    block_t *sdpparams = pt->opaque;
    if(sdpparams)
        block_Release(sdpparams);
}

static const struct vlc_rtp_pt_operations rtp_h264_ops = {
    rtp_h264_release, rtp_h264_init, rtp_h264_destroy, rtp_h264_decode,
};

static int rtp_h264_open(vlc_object_t *obj, struct vlc_rtp_pt *pt,
                         const struct vlc_sdp_pt *desc)
{
    VLC_UNUSED(obj);

    if(!desc->parameters)
        return VLC_ENOTSUP;

    const char *psz = strstr(desc->parameters, "packetization-mode=");
    if(!psz || psz[19] == '\0' || atoi(&psz[19]) > 1)
        return VLC_ENOTSUP;

    if (vlc_ascii_strcasecmp(desc->name, "H264") == 0)
        pt->ops = &rtp_h264_ops;
    else
        return VLC_ENOTSUP;

    pt->opaque = NULL;
    if(desc->parameters)
    {
        psz = strstr(desc->parameters, "sprop-parameter-sets=");
        if(psz)
            pt->opaque = h264_fillextradata(psz + 21);
    }

    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname(N_("RTP H264"))
    set_description(N_("RTP H264 payload parser"))
    set_subcategory(SUBCAT_INPUT_DEMUX)
    set_rtp_parser_callback(rtp_h264_open)
    add_shortcut("video/H264")
vlc_module_end()
