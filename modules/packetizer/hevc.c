/*****************************************************************************
 * hevc.c: h.265/hevc video packetizer
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Denis Charmet <typx@videolan.org>
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

#include <vlc_block_helper.h>
#include "packetizer_helper.h"
#include "hevc_nal.h"
#include "hxxx_nal.h"
#include "hxxx_common.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin ()
    set_category(CAT_SOUT)
    set_subcategory(SUBCAT_SOUT_PACKETIZER)
    set_description(N_("HEVC/H.265 video packetizer"))
    set_capability("packetizer", 50)
    set_callbacks(Open, Close)
vlc_module_end ()


/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static block_t *PacketizeAnnexB(decoder_t *, block_t **);
static block_t *PacketizeHVC1(decoder_t *, block_t **);
static void PacketizeFlush( decoder_t * );
static void PacketizeReset(void *p_private, bool b_broken);
static block_t *PacketizeParse(void *p_private, bool *pb_ts_used, block_t *);
static block_t *ParseNALBlock(decoder_t *, bool *pb_ts_used, block_t *);
static int PacketizeValidate(void *p_private, block_t *);

struct decoder_sys_t
{
    /* */
    packetizer_t packetizer;

    block_t *p_frame;
    block_t **pp_frame_last;

    uint8_t  i_nal_length_size;
    block_t *rgi_p_vps[HEVC_VPS_MAX];
    block_t *rgi_p_sps[HEVC_SPS_MAX];
    block_t *rgi_p_pps[HEVC_PPS_MAX];
    hevc_sequence_parameter_set_t *rgi_p_decsps[HEVC_SPS_MAX];
    hevc_picture_parameter_set_t  *rgi_p_decpps[HEVC_PPS_MAX];
};

static const uint8_t p_hevc_startcode[3] = {0x00, 0x00, 0x01};

/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if (p_dec->fmt_in.i_codec != VLC_CODEC_HEVC)
        return VLC_EGENERIC;

    p_dec->p_sys = p_sys = calloc(1, sizeof(decoder_sys_t));
    if (!p_dec->p_sys)
        return VLC_ENOMEM;

    p_sys->pp_frame_last = &p_sys->p_frame;

    packetizer_Init(&p_dec->p_sys->packetizer,
                    p_hevc_startcode, sizeof(p_hevc_startcode),
                    p_hevc_startcode, 1, 5,
                    PacketizeReset, PacketizeParse, PacketizeValidate, p_dec);

    /* Copy properties */
    es_format_Copy(&p_dec->fmt_out, &p_dec->fmt_in);
    p_dec->fmt_out.b_packetized = true;

    /* Set callbacks */
    const uint8_t *p_extra = p_dec->fmt_in.p_extra;
    const size_t i_extra = p_dec->fmt_in.i_extra;
    /* Check if we have hvcC as extradata */
    if(hevc_ishvcC(p_extra, i_extra))
    {
        p_dec->pf_packetize = PacketizeHVC1;

        /* Clear hvcC/HVC1 extra, to be replaced with AnnexB */
        free(p_dec->fmt_out.p_extra);
        p_dec->fmt_out.i_extra = 0;

        size_t i_new_extra = 0;
        p_dec->fmt_out.p_extra =
                hevc_hvcC_to_AnnexB_NAL(p_extra, i_extra,
                                        &i_new_extra, &p_sys->i_nal_length_size);
        if(p_dec->fmt_out.p_extra)
            p_dec->fmt_out.i_extra = i_new_extra;
    }
    else
    {
        p_dec->pf_packetize = PacketizeAnnexB;
    }
    p_dec->pf_flush = PacketizeFlush;

    if(p_dec->fmt_out.i_extra)
    {
        /* Feed with AnnexB VPS/SPS/PPS/SEI extradata */
        packetizer_Header(&p_sys->packetizer,
                          p_dec->fmt_out.p_extra, p_dec->fmt_out.i_extra);
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
static void Close(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;
    packetizer_Clean(&p_sys->packetizer);

    for(unsigned i=0;i<HEVC_PPS_MAX; i++)
    {
        if(p_sys->rgi_p_pps[i])
            block_Release(p_sys->rgi_p_pps[i]);
        if(p_sys->rgi_p_decpps[i])
            hevc_rbsp_release_pps(p_sys->rgi_p_decpps[i]);
    }

    for(unsigned i=0;i<HEVC_SPS_MAX; i++)
    {
        if(p_sys->rgi_p_sps[i])
            block_Release(p_sys->rgi_p_sps[i]);
        if(p_sys->rgi_p_decsps[i])
            hevc_rbsp_release_sps(p_sys->rgi_p_decsps[i]);
    }

    for(unsigned i=0;i<HEVC_VPS_MAX; i++)
        if(p_sys->rgi_p_vps[i])
            block_Release(p_sys->rgi_p_vps[i]);

    free(p_sys);
}

/****************************************************************************
 * Packetize
 ****************************************************************************/
static block_t *PacketizeHVC1(decoder_t *p_dec, block_t **pp_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    return PacketizeXXC1( p_dec, p_sys->i_nal_length_size,
                          pp_block, ParseNALBlock );
}

static block_t *PacketizeAnnexB(decoder_t *p_dec, block_t **pp_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    return packetizer_Packetize(&p_sys->packetizer, pp_block);
}

static void PacketizeFlush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    packetizer_Flush( &p_sys->packetizer );
}

/****************************************************************************
 * Packetizer Helpers
 ****************************************************************************/
static void PacketizeReset(void *p_private, bool b_broken)
{
    VLC_UNUSED(b_broken);

    decoder_t *p_dec = p_private;
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_ChainRelease(p_sys->p_frame);

    p_sys->p_frame = NULL;
    p_sys->pp_frame_last = &p_sys->p_frame;
}

static void InsertXPS(decoder_t *p_dec, uint8_t i_nal_type, uint8_t i_id,
                      block_t *p_nalb)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t **pp_xps = NULL;

    switch(i_nal_type)
    {
        case HEVC_NAL_VPS:
            if(i_id < HEVC_VPS_MAX)
                pp_xps = p_sys->rgi_p_vps;
            break;
        case HEVC_NAL_SPS:
            if(i_id < HEVC_SPS_MAX)
                pp_xps = p_sys->rgi_p_sps;
            break;
        case HEVC_NAL_PPS:
            if(i_id < HEVC_PPS_MAX)
                pp_xps = p_sys->rgi_p_pps;
            break;
        default: /* That shouln't happen */
            return;
    }
    if(!pp_xps)
        return;

    if(pp_xps[i_id])
    {
        if( p_nalb->i_buffer != pp_xps[i_id]->i_buffer ||
            !memcmp(pp_xps[i_id]->p_buffer, p_nalb->p_buffer,
             __MIN(pp_xps[i_id]->i_buffer, p_nalb->i_buffer)) )
            return;

        block_Release(pp_xps[i_id]);

        /* Free associated decoded version */
        if(i_nal_type == HEVC_NAL_SPS && p_sys->rgi_p_decsps[i_id])
        {
            hevc_rbsp_release_sps(p_sys->rgi_p_decsps[i_id]);
            p_sys->rgi_p_decsps[i_id] = NULL;
        }
        else if(i_nal_type == HEVC_NAL_PPS && p_sys->rgi_p_decpps[i_id])
        {
            hevc_rbsp_release_pps(p_sys->rgi_p_decpps[i_id]);
            p_sys->rgi_p_decpps[i_id] = NULL;
        }
    }

    pp_xps[i_id] = block_Duplicate(p_nalb);
    if(pp_xps[i_id] && i_nal_type != HEVC_NAL_VPS)
    {
        const uint8_t *p_buffer = p_nalb->p_buffer;
        size_t i_buffer = p_nalb->i_buffer;
        if( hxxx_strip_AnnexB_startcode( &p_buffer, &i_buffer ) )
        {
            /* Create decoded entries */
            if(i_nal_type == HEVC_NAL_SPS)
            {
                p_sys->rgi_p_decsps[i_id] = hevc_decode_sps(p_buffer, i_buffer, true);
                if(!p_sys->rgi_p_decsps[i_id])
                    msg_Err(p_dec, "Failed decoding SPS id %d", i_id);
            }
            else if(i_nal_type == HEVC_NAL_PPS)
            {
                p_sys->rgi_p_decpps[i_id] = hevc_decode_pps(p_buffer, i_buffer, true);
                if(!p_sys->rgi_p_decpps[i_id])
                    msg_Err(p_dec, "Failed decoding PPS id %d", i_id);
            }
        }
    }
}

static block_t *CopyXPS(decoder_sys_t *p_sys)
{
    block_t *p_chain = NULL;
    block_t **p_chain_tail = &p_chain;
    block_t *p_dup;

    for(unsigned i=0;i<HEVC_VPS_MAX; i++)
    {
        if(p_sys->rgi_p_vps[i] && (p_dup = block_Duplicate(p_sys->rgi_p_vps[i])))
            block_ChainLastAppend(&p_chain_tail, p_dup);
    }

    for(unsigned i=0;i<HEVC_SPS_MAX; i++)
    {
        if(p_sys->rgi_p_sps[i] && (p_dup = block_Duplicate(p_sys->rgi_p_sps[i])))
            block_ChainLastAppend(&p_chain_tail, p_dup);
    }

    for(unsigned i=0;i<HEVC_PPS_MAX; i++)
    {
        if(p_sys->rgi_p_pps[i] && (p_dup = block_Duplicate(p_sys->rgi_p_pps[i])))
            block_ChainLastAppend(&p_chain_tail, p_dup);
    }

    return p_chain;
}

static block_t *ParseVCL(decoder_t *p_dec, uint8_t i_nal_type, block_t *p_frag)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_frame = NULL;

    const uint8_t *p_buffer = p_frag->p_buffer;
    size_t i_buffer = p_frag->i_buffer;

    if(unlikely(!hxxx_strip_AnnexB_startcode(&p_buffer, &i_buffer) || i_buffer < 7))
        return NULL;

    bool b_first_slice_in_pic = p_buffer[2] & 0x80;
    if (b_first_slice_in_pic)
    {
        if(p_sys->p_frame)
        {
            /* Starting new frame, gather and return previous frame data */
            p_frame = block_ChainGather(p_sys->p_frame);
            p_sys->p_frame = NULL;
            p_sys->pp_frame_last = &p_sys->p_frame;
        }

        switch(i_nal_type)
        {
            case HEVC_NAL_BLA_W_LP:
            case HEVC_NAL_BLA_W_RADL:
            case HEVC_NAL_BLA_N_LP:
            case HEVC_NAL_IDR_W_RADL:
            case HEVC_NAL_IDR_N_LP:
            case HEVC_NAL_CRA:
                p_frag->i_flags |= BLOCK_FLAG_TYPE_I;
                break;

            default:
            {
                hevc_slice_segment_header_t *p_sli = hevc_decode_slice_header( p_buffer, i_buffer, true,
                                                                               p_sys->rgi_p_decsps, p_sys->rgi_p_decpps );
                if( p_sli )
                {
                    enum hevc_slice_type_e type;
                    if( hevc_get_slice_type( p_sli, &type ) )
                    {
                        if( type == HEVC_SLICE_TYPE_P )
                            p_frag->i_flags |= BLOCK_FLAG_TYPE_P;
                        else
                            p_frag->i_flags |= BLOCK_FLAG_TYPE_B;
                    }
                    hevc_rbsp_release_slice_header( p_sli );
                }
            }
            break;
        }
    }

    if( p_frag->i_flags & BLOCK_FLAG_TYPE_I )
    {
        block_t *p_header = CopyXPS(p_sys);
        if(p_header)
            block_ChainLastAppend(&p_sys->pp_frame_last, p_header);
    }

    block_ChainLastAppend(&p_sys->pp_frame_last, p_frag);

    return p_frame;
}

static block_t *ParseNonVCL(decoder_t *p_dec, uint8_t i_nal_type, block_t *p_nalb)
{
    block_t *p_ret = p_nalb;

    switch(i_nal_type)
    {
        case HEVC_NAL_VPS:
        case HEVC_NAL_SPS:
        case HEVC_NAL_PPS:
        {
            uint8_t i_id;
            if( hevc_get_xps_id(p_nalb->p_buffer, p_nalb->i_buffer, &i_id) )
                InsertXPS(p_dec, i_nal_type, i_id, p_nalb);
            block_Release( p_nalb );
            p_ret = NULL;
        }
            break;

        case HEVC_NAL_AUD:
        case HEVC_NAL_EOS:
        case HEVC_NAL_EOB:
        case HEVC_NAL_FD:
            break;
    }

    return p_ret;
}

/*****************************************************************************
 * ParseNALBlock: parses annexB type NALs
 * All p_frag blocks are required to start with 0 0 0 1 4-byte startcode
 *****************************************************************************/
static block_t *ParseNALBlock(decoder_t *p_dec, bool *pb_ts_used, block_t *p_frag)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_t * p_ret = NULL;

    if(unlikely(p_frag->i_buffer < 5))
    {
        msg_Warn(p_dec,"NAL too small");
        block_Release(p_frag);
        return NULL;
    }

    if(p_frag->p_buffer[4] & 0x80)
    {
        msg_Warn(p_dec,"Forbidden zero bit not null, corrupted NAL");
        block_ChainRelease(p_sys->p_frame);
        block_Release(p_frag);
        p_sys->p_frame = NULL;
        p_sys->pp_frame_last = &p_sys->p_frame;
        return NULL;
    }

    /* Get NALU type */
    uint8_t i_nal_type = ((p_frag->p_buffer[4] & 0x7E) >> 1);
    if (i_nal_type < HEVC_NAL_VPS)
    {
        /* NAL is a VCL NAL */
        p_ret = ParseVCL(p_dec, i_nal_type, p_frag);
    }
    else
    {
        p_ret = ParseNonVCL(p_dec, i_nal_type, p_frag);
        if (p_sys->p_frame)
        {
            p_frag = p_ret;
            if( (p_ret = block_ChainGather(p_sys->p_frame)) )
            {
                p_sys->p_frame = NULL;
                p_sys->pp_frame_last = &p_sys->p_frame;
                p_ret->p_next = p_frag;
            }
            else p_ret = p_frag;
        }
    }

    *pb_ts_used = false;
    return p_ret;
}

static block_t *PacketizeParse(void *p_private, bool *pb_ts_used, block_t *p_block)
{
    decoder_t *p_dec = p_private;

    /* Remove trailing 0 bytes */
    while (p_block->i_buffer > 5 && p_block->p_buffer[p_block->i_buffer-1] == 0x00 )
        p_block->i_buffer--;

    return ParseNALBlock( p_dec, pb_ts_used, p_block );
}

static int PacketizeValidate( void *p_private, block_t *p_au )
{
    VLC_UNUSED(p_private);
    VLC_UNUSED(p_au);
    return VLC_SUCCESS;
}
