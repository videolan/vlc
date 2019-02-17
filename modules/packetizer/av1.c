/*****************************************************************************
 * av1.c: AV1 video packetizer
 *****************************************************************************
 * Copyright (C) 2018 VideoLabs, VLC authors and VideoLAN
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
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_block.h>
#include <vlc_bits.h>

#include <vlc_block_helper.h>

#include "av1.h"
#include "av1_obu.h"

//#define DEBUG_AV1_PACKETIZER

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
typedef struct
{
    struct
    {
        block_t *p_chain;
        block_t **pp_chain_last;
    } obus;

    block_t *p_sequence_header_block;
    av1_OBU_sequence_header_t *p_sequence_header;
    struct
    {
        bool b_has_visible_frame;
        struct
        {
            block_t *p_chain;
            block_t **pp_chain_last;
        } pre, frame, post;
        vlc_tick_t dts;
        vlc_tick_t pts;
    } tu;
    uint32_t i_seen;
    int i_next_block_flags;

} av1_sys_t;

#define BLOCK_FLAG_DROP (1 << BLOCK_FLAG_PRIVATE_SHIFT)

/****************************************************************************
 * Helpers
 ****************************************************************************/
static inline void InitQueue(block_t **pp_head, block_t ***ppp_tail)
{
    *pp_head = NULL;
    *ppp_tail = pp_head;
}

static bool block_Differs(const block_t *a, const block_t *b)
{
    return (a->i_buffer != b->i_buffer ||
            memcmp(a->p_buffer, b->p_buffer, a->i_buffer));
}

#define INITQ(name) InitQueue(&p_sys->name.p_chain, &p_sys->name.pp_chain_last)
#define PUSHQ(name,b) \
{\
    block_ChainLastAppend(&p_sys->name.pp_chain_last, b);\
    if(p_sys->tu.dts == VLC_TICK_INVALID)\
        p_sys->tu.dts = b->i_dts; p_sys->tu.pts = b->i_pts;\
}

static void UpdateDecoderFormat(decoder_t *p_dec)
{
    av1_sys_t *p_sys = p_dec->p_sys;
    if(!p_sys->p_sequence_header)
        return;

    if(p_dec->fmt_in.i_profile < AV1_PROFILE_MAIN)
    {
        int val[3];
        AV1_get_profile_level(p_sys->p_sequence_header, &val[0], &val[1], &val[2]);
        if(p_dec->fmt_out.i_profile != val[0] || p_dec->fmt_out.i_level != val[1])
        {
            p_dec->fmt_out.i_profile = val[0];
            p_dec->fmt_out.i_level = val[1];
        }
    }

    unsigned wnum, hden;
    AV1_get_frame_max_dimensions(p_sys->p_sequence_header, &wnum, &hden);
    if((!p_dec->fmt_in.video.i_visible_height ||
        !p_dec->fmt_in.video.i_visible_width) &&
       (p_dec->fmt_out.video.i_visible_width != wnum ||
        p_dec->fmt_out.video.i_visible_width != hden))
    {
        p_dec->fmt_out.video.i_width =
        p_dec->fmt_out.video.i_visible_width = wnum;
        p_dec->fmt_out.video.i_height =
        p_dec->fmt_out.video.i_visible_height = hden;
    }

    if((!p_dec->fmt_in.video.i_frame_rate ||
        !p_dec->fmt_in.video.i_frame_rate_base) &&
        AV1_get_frame_rate(p_sys->p_sequence_header, &wnum, &hden) &&
        (p_dec->fmt_out.video.i_frame_rate != wnum ||
         p_dec->fmt_out.video.i_frame_rate_base != hden))
    {
        p_dec->fmt_out.video.i_frame_rate = wnum;
        p_dec->fmt_out.video.i_frame_rate_base = hden;
    }

    video_color_primaries_t prim;
    video_color_space_t space;
    video_transfer_func_t xfer;
    video_color_range_t full;
    if(p_dec->fmt_in.video.primaries == COLOR_PRIMARIES_UNDEF &&
       AV1_get_colorimetry(p_sys->p_sequence_header, &prim, &xfer, &space, &full) &&
       prim != COLOR_PRIMARIES_UNDEF &&
       (p_dec->fmt_out.video.primaries != prim ||
        p_dec->fmt_out.video.transfer != xfer ||
        p_dec->fmt_out.video.space != space))
    {
        p_dec->fmt_out.video.primaries = prim;
        p_dec->fmt_out.video.transfer = xfer;
        p_dec->fmt_out.video.space = space;
        p_dec->fmt_out.video.color_range = full;
    }

    if(!p_dec->fmt_in.i_extra && !p_dec->fmt_out.i_extra)
    {
        p_dec->fmt_out.i_extra =
                AV1_create_DecoderConfigurationRecord((uint8_t **)&p_dec->fmt_out.p_extra,
                                                      p_sys->p_sequence_header,
                                                      1,
                                                      (const uint8_t **)&p_sys->p_sequence_header_block->p_buffer,
                                                      &p_sys->p_sequence_header_block->i_buffer);
    }
}

static block_t * OutputQueues(decoder_t *p_dec, bool b_valid)
{
    av1_sys_t *p_sys = p_dec->p_sys;
    block_t *p_output = NULL;
    block_t **pp_output_last = &p_output;
    uint32_t i_flags = 0; /* Because block_ChainGather does not merge flags or times */

    if(p_sys->tu.pre.p_chain)
    {
        block_ChainLastAppend(&pp_output_last, p_sys->tu.pre.p_chain);
        INITQ(tu.pre);
    }

    if(p_sys->tu.frame.p_chain)
    {
        i_flags |= p_sys->tu.frame.p_chain->i_flags;
        block_ChainLastAppend(&pp_output_last, p_sys->tu.frame.p_chain);
        INITQ(tu.frame);
    }

    if(p_sys->tu.post.p_chain)
    {
        block_ChainLastAppend(&pp_output_last, p_sys->tu.post.p_chain);
        INITQ(tu.post);
    }

    if(p_output)
    {
        p_output->i_dts = p_sys->tu.dts;
        p_output->i_pts = p_sys->tu.pts;
        p_output->i_flags |= i_flags;
        if(!b_valid)
            p_output->i_flags |= BLOCK_FLAG_DROP;
        else
        {
            p_output->i_flags |= p_sys->i_next_block_flags;
            p_sys->i_next_block_flags = 0;
        }
    }

    p_sys->tu.b_has_visible_frame = false;
    p_sys->tu.dts = VLC_TICK_INVALID;
    p_sys->tu.pts = VLC_TICK_INVALID;
    p_sys->i_seen = 0;

    return p_output;
}

/****************************************************************************
 * Packetizer Helpers
 ****************************************************************************/
static block_t *GatherAndValidateChain(decoder_t *p_dec, block_t *p_outputchain)
{
    block_t *p_output = NULL;
    av1_sys_t *p_sys = p_dec->p_sys;
    VLC_UNUSED(p_sys);

    if(p_outputchain)
    {
#ifdef DEBUG_AV1_PACKETIZER
        msg_Dbg(p_dec, "TU output %ld", p_outputchain->i_dts);
        for(block_t *p = p_outputchain; p; p=p->p_next)
        {
            enum av1_obu_type_e OBUtype = AV1_OBUGetType(p->p_buffer);
            if(OBUtype == AV1_OBU_FRAME || OBUtype == AV1_OBU_FRAME_HEADER)
            {
                av1_OBU_frame_header_t *p_fh = NULL;
                if(AV1_OBUIsBaseLayer(p->p_buffer, p->i_buffer) && p_sys->p_sequence_header)
                {
                    p_fh = AV1_OBU_parse_frame_header(p->p_buffer, p->i_buffer,
                                                      p_sys->p_sequence_header);
                    if(p_fh)
                    {
                        msg_Dbg(p_dec,"OBU TYPE %d sz %ld dts %ld type %d %d",
                                OBUtype, p->i_buffer, p->i_dts,
                                AV1_get_frame_type(p_fh),
                                AV1_get_frame_visibility(p_fh));
                    }
                    AV1_release_frame_header(p_fh);
                }
            }
            else msg_Dbg(p_dec, "OBU TYPE %d sz %ld dts %ld", OBUtype, p->i_buffer, p->i_dts);
        }
#endif
        if(p_outputchain->i_flags & BLOCK_FLAG_DROP)
            p_output = p_outputchain; /* Avoid useless gather */
        else
            p_output = block_ChainGather(p_outputchain);
    }

    if(p_output && (p_output->i_flags & BLOCK_FLAG_DROP))
    {
        block_ChainRelease(p_output); /* Chain! see above */
        p_output = NULL;
    }

    return p_output;
}

static block_t *ParseOBUBlock(decoder_t *p_dec, block_t *p_obu)
{
    av1_sys_t *p_sys = p_dec->p_sys;

    block_t * p_output = NULL;
    enum av1_obu_type_e OBUtype = AV1_OBUGetType(p_obu->p_buffer);
    const bool b_base_layer = AV1_OBUIsBaseLayer(p_obu->p_buffer, p_obu->i_buffer);

    switch(OBUtype)
    {
        case AV1_OBU_SEQUENCE_HEADER:
        {
            if(p_sys->tu.frame.p_chain || p_sys->tu.post.p_chain)
                p_output = OutputQueues(p_dec, p_sys->p_sequence_header != NULL);

            if(b_base_layer)
            {
                /* Save a copy for Extradata */
                if(!p_sys->p_sequence_header_block ||
                   block_Differs(p_sys->p_sequence_header_block, p_obu))
                {
                    if(p_sys->p_sequence_header_block)
                        block_Release(p_sys->p_sequence_header_block);
                    p_sys->p_sequence_header_block = block_Duplicate(p_obu);
                }

                if(p_sys->p_sequence_header)
                    AV1_release_sequence_header(p_sys->p_sequence_header);
                p_sys->p_sequence_header = AV1_OBU_parse_sequence_header(p_obu->p_buffer, p_obu->i_buffer);
            }
            PUSHQ(tu.pre, p_obu);
        } break;

        case AV1_OBU_TEMPORAL_DELIMITER:
        {
            p_output = OutputQueues(p_dec, p_sys->p_sequence_header_block != NULL);
            PUSHQ(tu.pre, p_obu);
        } break;

        case AV1_OBU_FRAME:
        case AV1_OBU_FRAME_HEADER:
        {
            if(b_base_layer)
            {
                av1_OBU_frame_header_t *p_fh = NULL;
                if(p_sys->p_sequence_header)
                {
                    p_fh = AV1_OBU_parse_frame_header(p_obu->p_buffer, p_obu->i_buffer,
                                                      p_sys->p_sequence_header);
                    if(p_fh)
                    {
                        if((p_sys->i_seen & AV1_OBU_TEMPORAL_DELIMITER) && p_sys->tu.b_has_visible_frame)
                            p_output = OutputQueues(p_dec, p_sys->p_sequence_header != NULL);

                        switch(AV1_get_frame_type(p_fh))
                        {
                            case AV1_FRAME_TYPE_KEY:
                            case AV1_FRAME_TYPE_INTRA_ONLY:
                                p_obu->i_flags |= BLOCK_FLAG_TYPE_I;
                                break;
                            case AV1_FRAME_TYPE_INTER:
                                p_obu->i_flags |= BLOCK_FLAG_TYPE_P;
                                break;
                            default:
                                break;
                        }

                        p_sys->tu.b_has_visible_frame |= AV1_get_frame_visibility(p_fh);
                        AV1_release_frame_header(p_fh);
                    }
                    else msg_Warn(p_dec, "could not parse frame header");
                }
            }

            if(!p_output && p_sys->tu.post.p_chain)
                p_output = OutputQueues(p_dec, p_sys->p_sequence_header != NULL);

            PUSHQ(tu.frame, p_obu);
        } break;

        case AV1_OBU_METADATA:
        {
            if(p_sys->tu.frame.p_chain || p_sys->tu.post.p_chain)
                p_output = OutputQueues(p_dec, p_sys->p_sequence_header != NULL);
            PUSHQ(tu.pre, p_obu);
        } break;

        case AV1_OBU_TILE_GROUP:
        case AV1_OBU_TILE_LIST:
            if(p_sys->tu.post.p_chain)
                p_output = OutputQueues(p_dec, p_sys->p_sequence_header != NULL);
            PUSHQ(tu.frame, p_obu);
            break;

        case AV1_OBU_REDUNDANT_FRAME_HEADER:
        case AV1_OBU_PADDING:
        default:
            block_Release(p_obu);
            break;
    }

    if(b_base_layer)
        p_sys->i_seen |= 1 << OBUtype;

    return p_output;
}

/****************************************************************************
 * Flush
 ****************************************************************************/
static void PacketizeFlush(decoder_t *p_dec)
{
    av1_sys_t *p_sys = p_dec->p_sys;

    block_ChainRelease(OutputQueues(p_dec, false));

    if(p_sys->p_sequence_header)
    {
        AV1_release_sequence_header(p_sys->p_sequence_header);
        p_sys->p_sequence_header = NULL;
    }
    if(p_sys->p_sequence_header_block)
    {
        block_Release(p_sys->p_sequence_header_block);
        p_sys->p_sequence_header_block = NULL;
    }

    block_ChainRelease(p_sys->obus.p_chain);
    INITQ(obus);

    p_sys->tu.dts = VLC_TICK_INVALID;
    p_sys->tu.pts = VLC_TICK_INVALID;
    p_sys->tu.b_has_visible_frame = false;
    p_sys->i_seen = 0;
    p_sys->i_next_block_flags = BLOCK_FLAG_DISCONTINUITY;
}

/****************************************************************************
 * Packetize
 ****************************************************************************/
static block_t *PacketizeOBU(decoder_t *p_dec, block_t **pp_block)
{
    av1_sys_t *p_sys = p_dec->p_sys;

    block_t *p_block = pp_block ? *pp_block : NULL;
    if(p_block)
    {
        if( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY | BLOCK_FLAG_CORRUPTED) )
        {
            /* First always drain complete blocks before discontinuity */
            block_t *p_drain = PacketizeOBU( p_dec, NULL );
            if(p_drain)
                return p_drain;

            PacketizeFlush( p_dec );

            if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
            {
                block_Release( p_block );
                return NULL;
            }
        }

        if(!AV1_OBUIsValid(p_block->p_buffer, p_block->i_buffer))
        {
            msg_Warn(p_dec,"fed with invalid OBU");
            block_Release(p_block);
            return NULL;
        }
        *pp_block = NULL;
        block_ChainLastAppend(&p_sys->obus.pp_chain_last, p_block);
    }

    block_t *p_output = NULL;
    while(p_sys->obus.p_chain)
    {
        block_t *p_frag = p_sys->obus.p_chain;

        AV1_OBU_iterator_ctx_t it;
        AV1_OBU_iterator_init(&it, p_frag->p_buffer, p_frag->i_buffer);
        const uint8_t *p_obu; size_t i_obu;

        if(!AV1_OBU_iterate_next(&it, &p_obu, &i_obu))
        {
            msg_Warn(p_dec,"Invalid OBU header in sequence, discarding");
            /* frag is not OBU, drop */
            p_sys->obus.p_chain = p_frag->p_next;
            if(p_frag->p_next == NULL)
                p_sys->obus.pp_chain_last = &p_sys->obus.p_chain;
            else
                p_frag->p_next = NULL;
            block_Release(p_frag);
            continue;
        }

        block_t *p_obublock;
        if(i_obu == p_frag->i_buffer)
        {
            p_sys->obus.p_chain = p_frag->p_next;
            if(p_frag->p_next == NULL)
                p_sys->obus.pp_chain_last = &p_sys->obus.p_chain;
            else
                p_frag->p_next = NULL;
            p_obublock = p_frag;
        }
        else
        {
            p_obublock = block_Alloc(i_obu);
            memcpy(p_obublock->p_buffer, p_frag->p_buffer, i_obu);
            p_frag->i_buffer -= i_obu;
            p_frag->p_buffer += i_obu;
            p_obublock->i_dts = p_frag->i_dts;
            p_obublock->i_pts = p_frag->i_pts;
            p_obublock->i_flags = p_frag->i_flags;
            p_frag->i_flags = 0;
            p_frag->i_dts = VLC_TICK_INVALID;
            p_frag->i_pts = VLC_TICK_INVALID;
        }

        p_output = ParseOBUBlock(p_dec, p_obublock);
        if(p_output)
            break;
    }


    if(!p_output && pp_block == NULL)
        p_output = OutputQueues(p_dec, p_sys->p_sequence_header_block != NULL);

    if(p_output)
    {
        p_output = GatherAndValidateChain(p_dec, p_output);
        UpdateDecoderFormat(p_dec);
    }

    return p_output;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
static void Close(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t*)p_this;
    av1_sys_t *p_sys = p_dec->p_sys;

    PacketizeFlush(p_dec);

    free(p_sys);
}

/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t*)p_this;
    av1_sys_t *p_sys;

    if (p_dec->fmt_in.i_codec != VLC_CODEC_AV1)
        return VLC_EGENERIC;

    p_dec->p_sys = p_sys = calloc(1, sizeof(av1_sys_t));
    if (!p_dec->p_sys)
        return VLC_ENOMEM;

    INITQ(obus);
    p_sys->p_sequence_header_block = NULL;
    p_sys->p_sequence_header = NULL;
    p_sys->tu.b_has_visible_frame = false;
    p_sys->tu.dts = VLC_TICK_INVALID;
    p_sys->tu.pts = VLC_TICK_INVALID;
    p_sys->i_seen = 0;
    p_sys->i_next_block_flags = 0;
    INITQ(tu.pre);
    INITQ(tu.frame);
    INITQ(tu.post);

    /* Copy properties */
    es_format_Copy(&p_dec->fmt_out, &p_dec->fmt_in);
    p_dec->fmt_out.b_packetized = true;

    p_dec->pf_packetize = PacketizeOBU;
    p_dec->pf_flush = PacketizeFlush;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin ()
    set_category(CAT_SOUT)
    set_subcategory(SUBCAT_SOUT_PACKETIZER)
    set_description(N_("AV1 video packetizer"))
    set_capability("packetizer", 50)
    set_callbacks(Open, Close)
vlc_module_end ()
