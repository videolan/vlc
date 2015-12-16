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

    uint8_t  i_nal_length_size;
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
}

/*****************************************************************************
 * ParseNALBlock: parses annexB type NALs
 * All p_frag blocks are required to start with 0 0 0 1 4-byte startcode
 *****************************************************************************/
static block_t *ParseNALBlock(decoder_t *p_dec, bool *pb_ts_used, block_t *p_frag)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_t * p_nal = NULL;

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
        return NULL;
    }

    /* Get NALU type */
    uint8_t nalu_type = ((p_frag->p_buffer[4] & 0x7E) >> 1);
    if (nalu_type < HEVC_NAL_VPS)
    {
        /* NAL is a VCL NAL */
        if(likely(p_frag->i_buffer > 6))
        {
            bool first_slice_in_pic = p_frag->p_buffer[6] & 0x80;
            if (first_slice_in_pic && p_sys->p_frame)
            {
                p_nal = block_ChainGather(p_sys->p_frame);
                p_sys->p_frame = NULL;
            }
        }
        block_ChainAppend(&p_sys->p_frame, p_frag);
    }
    else
    {
        if (p_sys->p_frame)
        {
            p_nal = block_ChainGather(p_sys->p_frame);
            p_nal->p_next = p_frag;
            p_sys->p_frame = NULL;
        }
        else
            p_nal = p_frag;
    }

    *pb_ts_used = false;
    return p_nal;
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
