/**
 * @file h26x.h
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
#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_strings.h>
#include <vlc_codec.h>
#include <vlc_demux.h>

#include "rtp.h"
#include "../live555_dtsgen.h"

static const uint8_t annexbheader[] = { 0, 0, 0, 1 };

struct rtp_h26x_sys
{
    unsigned flags;
    vlc_tick_t pts;
    block_t **pp_packets_next;
    block_t *p_packets;
    block_t *xps;
    struct vlc_rtp_es *es;
    decoder_t *p_packetizer;
    struct dtsgen_t dtsgen;
};

static void rtp_h26x_clear(struct rtp_h26x_sys *sys)
{
    block_ChainRelease(sys->p_packets);
    if(sys->xps)
        block_Release(sys->xps);
}

static void rtp_h26x_init(struct rtp_h26x_sys *sys)
{
    sys->flags = 0;
    sys->pts = VLC_TICK_INVALID;
    sys->p_packets = NULL;
    sys->pp_packets_next = &sys->p_packets;
    sys->xps = NULL;
    sys->es = NULL;
    sys->p_packetizer = NULL;
    dtsgen_Init(&sys->dtsgen);
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

static void h26x_add_xps(void *priv, uint8_t *xps, size_t xpssz)
{
    block_t *b = block_heap_Alloc(xps, xpssz);
    if(!b || !(b = h26x_wrap_prefix(b, true)))
        return;

    block_t ***ppp_append = priv;
    **ppp_append = b;
    *ppp_append = &((**ppp_append)->p_next);
}

static inline block_t * h26x_fillextradata (const char *psz)
{
    block_t *xps = NULL;
    block_t **pxps = &xps;
    h26x_extractbase64xps(psz, strchr(psz, ';'), h26x_add_xps, &pxps);
    if(xps)
        xps = block_ChainGather(xps);
    return xps;
}

static void h26x_output(struct rtp_h26x_sys *sys,
                        block_t *block,
                        vlc_tick_t pts, bool pcr, bool au_end)
{
    if(!block)
        return;

    if(sys->xps)
    {
        block_t *xps = sys->xps;
        sys->xps = NULL;
        h26x_output(sys, xps, pts, pcr, false);
    }

    if(block->i_flags & BLOCK_FLAG_DISCONTINUITY)
        dtsgen_Resync(&sys->dtsgen);

    block->i_pts = pts;
    block->i_dts = VLC_TICK_INVALID; /* RTP does not specify this */
    if(au_end)
        block->i_flags |= BLOCK_FLAG_AU_END;

    block_t *p_out;
    for(int i=0; i<(1+!!au_end); i++)
    {
        while((p_out = sys->p_packetizer->pf_packetize(sys->p_packetizer,
                                                       block ? &block : NULL)))
        {
            dtsgen_AddNextPTS(&sys->dtsgen, p_out->i_pts);
            vlc_tick_t dts = dtsgen_GetDTS(&sys->dtsgen);
            p_out->i_dts = dts;
            vlc_rtp_es_send(sys->es, p_out);
        }
        block = NULL; // for drain iteration
    }
}

static inline void h26x_output_blocks(struct rtp_h26x_sys *sys, bool b_annexb)
{
    if(!sys->p_packets)
        return;
    block_t *out = block_ChainGather(sys->p_packets);
    sys->p_packets = NULL;
    sys->pp_packets_next = &sys->p_packets;
    out = h26x_wrap_prefix(out, b_annexb);
    h26x_output(sys, out, sys->pts, true, false);
}
