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

#include <vlc_bits.h>
#include <vlc_block_helper.h>
#include "packetizer_helper.h"

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
static block_t *Packetize(decoder_t *, block_t **);
static void PacketizeReset(void *p_private, bool b_broken);
static block_t *PacketizeParse(void *p_private, bool *pb_ts_used, block_t *);
static int PacketizeValidate(void *p_private, block_t *);

struct decoder_sys_t
{
    /* */
    packetizer_t packetizer;

    bool     b_vcl;
    block_t *p_frame;

};

/* NAL types from https://www.itu.int/rec/dologin_pub.asp?lang=e&id=T-REC-H.265-201304-I!!PDF-E&type=items */
enum nal_unit_type_e
{
    TRAIL_N    = 0,
    TRAIL_R    = 1,
    TSA_N      = 2,
    TSA_R      = 3,
    STSA_N     = 4,
    STSA_R     = 5,
    RADL_N     = 6,
    RADL_R     = 7,
    RASL_N     = 8,
    RASL_R     = 9,
    /* 10 to 15 reserved */
    /* Key frames */
    BLA_W_LP   = 16,
    BLA_W_RADL = 17,
    BLA_N_LP   = 18,
    IDR_W_RADL = 19,
    IDR_N_LP   = 20,
    CRA        = 21,
    /* 22 to 31 reserved */
    /* Non VCL NAL*/
    VPS        = 32,
    SPS        = 33,
    PPS        = 34,
    AUD        = 35, /* Access unit delimiter */
    EOS        = 36, /* End of sequence */
    EOB        = 37, /* End of bitstream */
    FD         = 38, /* Filler data*/
    PREF_SEI   = 39, /* Prefix SEI */
    SUFF_SEI   = 40, /* Suffix SEI */
    UNKNOWN_NAL
};

static const uint8_t p_hevc_startcode[3] = {0x00, 0x00, 0x01};

/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    decoder_t     *p_dec = (decoder_t*)p_this;

    if (p_dec->fmt_in.i_codec != VLC_CODEC_HEVC)
        return VLC_EGENERIC;

    p_dec->p_sys = calloc(1, sizeof(decoder_sys_t));
    if (!p_dec->p_sys)
        return VLC_ENOMEM;

    packetizer_Init(&p_dec->p_sys->packetizer,
                    p_hevc_startcode, sizeof(p_hevc_startcode),
                    p_hevc_startcode, 1, 5,
                    PacketizeReset, PacketizeParse, PacketizeValidate, p_dec);

    /* Copy properties */
    es_format_Copy(&p_dec->fmt_out, &p_dec->fmt_in);

    /* Set callback */
    p_dec->pf_packetize = Packetize;

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
static block_t *Packetize(decoder_t *p_dec, block_t **pp_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    return packetizer_Packetize(&p_sys->packetizer, pp_block);
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
    p_sys->b_vcl = false;
}

static block_t *PacketizeParse(void *p_private, bool *pb_ts_used, block_t *p_block)
{
    decoder_t *p_dec = p_private;
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_t * p_nal = NULL;

    while (p_block->i_buffer > 5 && p_block->p_buffer[p_block->i_buffer-1] == 0x00 )
        p_block->i_buffer--;

    bs_t bs;
    bs_init(&bs, p_block->p_buffer+4, p_block->i_buffer-4);

    /* Get NALU type */
    uint32_t forbidden_zero_bit = bs_read1(&bs);

    if (forbidden_zero_bit)
    {
        msg_Err(p_dec,"Forbidden zero bit not null, corrupted NAL");
        p_sys->p_frame = NULL;
        p_sys->b_vcl = false;
        return NULL;
    }
    uint32_t nalu_type = bs_read(&bs,6);
    bs_skip(&bs, 9);

    if (nalu_type < VPS)
    {
        /* NAL is a VCL NAL */
        p_sys->b_vcl = true;

        uint32_t first_slice_in_pic = bs_read1(&bs);

        if (first_slice_in_pic && p_sys->p_frame)
        {
            p_nal = block_ChainGather(p_sys->p_frame);
            p_sys->p_frame = NULL;
        }

        block_ChainAppend(&p_sys->p_frame, p_block);
    }
    else
    {
        if (p_sys->b_vcl)
        {
            p_nal = block_ChainGather(p_sys->p_frame);
            p_nal->p_next = p_block;
            p_sys->p_frame = NULL;
            p_sys->b_vcl =false;
        }
        else
            p_nal = p_block;
    }

    *pb_ts_used = false;
    return p_nal;
}

static int PacketizeValidate( void *p_private, block_t *p_au )
{
    VLC_UNUSED(p_private);
    VLC_UNUSED(p_au);
    return VLC_SUCCESS;
}
