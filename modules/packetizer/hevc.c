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
#include "startcode_helper.h"
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

    struct
    {
        block_t *p_chain;
        block_t **pp_chain_last;
    } frame, pre, post;

    uint8_t  i_nal_length_size;
    hevc_video_parameter_set_t    *rgi_p_decvps[HEVC_VPS_MAX];
    hevc_sequence_parameter_set_t *rgi_p_decsps[HEVC_SPS_MAX];
    hevc_picture_parameter_set_t  *rgi_p_decpps[HEVC_PPS_MAX];
    bool b_init_sequence_complete;
};

static const uint8_t p_hevc_startcode[3] = {0x00, 0x00, 0x01};
/****************************************************************************
 * Helpers
 ****************************************************************************/
static inline void InitQueue( block_t **pp_head, block_t ***ppp_tail )
{
    *pp_head = NULL;
    *ppp_tail = pp_head;
}
#define INITQ(name) InitQueue(&p_sys->name.p_chain, &p_sys->name.pp_chain_last)

static block_t * OutputQueues(decoder_sys_t *p_sys, bool b_valid)
{
    block_t *p_output = NULL;
    block_t **pp_output_last = &p_output;
    uint32_t i_flags = 0; /* Because block_ChainGather does not merge flags or times */

    if(p_sys->pre.p_chain)
    {
        i_flags |= p_sys->pre.p_chain->i_flags;
        block_ChainLastAppend(&pp_output_last, p_sys->pre.p_chain);
        INITQ(pre);
    }

    if(p_sys->frame.p_chain)
    {
        i_flags |= p_sys->frame.p_chain->i_flags;
        if(p_output && p_output->i_dts == 0)
        {
            p_output->i_dts = p_sys->frame.p_chain->i_dts;
            p_output->i_pts = p_sys->frame.p_chain->i_pts;
        }
        block_ChainLastAppend(&pp_output_last, p_sys->frame.p_chain);
        INITQ(frame);
    }

    if(p_sys->post.p_chain)
    {
        i_flags |= p_sys->post.p_chain->i_flags;
        block_ChainLastAppend(&pp_output_last, p_sys->post.p_chain);
        INITQ(post);
    }

    if(p_output)
    {
        p_output->i_flags |= i_flags;
        if(!b_valid)
            p_output->i_flags |= BLOCK_FLAG_CORRUPTED;
    }

    return p_output;
}


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

    INITQ(pre);
    INITQ(frame);
    INITQ(post);

    packetizer_Init(&p_dec->p_sys->packetizer,
                    p_hevc_startcode, sizeof(p_hevc_startcode), startcode_FindAnnexB,
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

    block_ChainRelease(p_sys->frame.p_chain);
    block_ChainRelease(p_sys->pre.p_chain);
    block_ChainRelease(p_sys->post.p_chain);

    for(unsigned i=0;i<HEVC_PPS_MAX; i++)
    {
        if(p_sys->rgi_p_decpps[i])
            hevc_rbsp_release_pps(p_sys->rgi_p_decpps[i]);
    }

    for(unsigned i=0;i<HEVC_SPS_MAX; i++)
    {
        if(p_sys->rgi_p_decsps[i])
            hevc_rbsp_release_sps(p_sys->rgi_p_decsps[i]);
    }

    for(unsigned i=0;i<HEVC_VPS_MAX; i++)
    {
        if(p_sys->rgi_p_decvps[i])
            hevc_rbsp_release_vps(p_sys->rgi_p_decvps[i]);
    }

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

    block_t *p_out = OutputQueues(p_sys, false);
    if(p_out)
        block_ChainRelease(p_out);

    p_sys->b_init_sequence_complete = false;
}

static bool InsertXPS(decoder_t *p_dec, uint8_t i_nal_type, uint8_t i_id,
                      const block_t *p_nalb)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    switch(i_nal_type)
    {
        case HEVC_NAL_VPS:
            if(i_id >= HEVC_VPS_MAX)
                return false;
            break;
        case HEVC_NAL_SPS:
            if(i_id >= HEVC_SPS_MAX)
                return false;
            break;
        case HEVC_NAL_PPS:
            if(i_id >= HEVC_PPS_MAX)
                return false;
            break;
        default:
            return false;
    }

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
    else if(i_nal_type == HEVC_NAL_VPS && p_sys->rgi_p_decvps[i_id])
    {
        hevc_rbsp_release_vps(p_sys->rgi_p_decvps[i_id]);
        p_sys->rgi_p_decvps[i_id] = NULL;
    }

    const uint8_t *p_buffer = p_nalb->p_buffer;
    size_t i_buffer = p_nalb->i_buffer;
    if( hxxx_strip_AnnexB_startcode( &p_buffer, &i_buffer ) )
    {
        /* Create decoded entries */
        if(i_nal_type == HEVC_NAL_SPS)
        {
            p_sys->rgi_p_decsps[i_id] = hevc_decode_sps(p_buffer, i_buffer, true);
            if(!p_sys->rgi_p_decsps[i_id])
            {
                msg_Err(p_dec, "Failed decoding SPS id %d", i_id);
                return false;
            }
        }
        else if(i_nal_type == HEVC_NAL_PPS)
        {
            p_sys->rgi_p_decpps[i_id] = hevc_decode_pps(p_buffer, i_buffer, true);
            if(!p_sys->rgi_p_decpps[i_id])
            {
                msg_Err(p_dec, "Failed decoding PPS id %d", i_id);
                return false;
            }
        }
        else if(i_nal_type == HEVC_NAL_VPS)
        {
            p_sys->rgi_p_decvps[i_id] = hevc_decode_vps(p_buffer, i_buffer, true);
            if(!p_sys->rgi_p_decvps[i_id])
            {
                msg_Err(p_dec, "Failed decoding VPS id %d", i_id);
                return false;
            }
        }
        return true;

    }

    return false;
}

static bool XPSReady(decoder_sys_t *p_sys)
{
    for(unsigned i=0;i<HEVC_PPS_MAX; i++)
    {
        const hevc_picture_parameter_set_t *p_pps = p_sys->rgi_p_decpps[i];
        if (p_pps)
        {
            uint8_t id_sps = hevc_get_pps_sps_id(p_pps);
            const hevc_sequence_parameter_set_t *p_sps = p_sys->rgi_p_decsps[id_sps];
            if(p_sps)
            {
                uint8_t id_vps = hevc_get_sps_vps_id(p_sps);
                if(p_sys->rgi_p_decvps[id_vps])
                    return true;
            }
        }
    }
    return false;
}

static block_t *ParseVCL(decoder_t *p_dec, uint8_t i_nal_type, block_t *p_frag)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_outputchain = NULL;

    const uint8_t *p_buffer = p_frag->p_buffer;
    size_t i_buffer = p_frag->i_buffer;

    if(unlikely(!hxxx_strip_AnnexB_startcode(&p_buffer, &i_buffer) || i_buffer < 3))
    {
        block_ChainAppend(&p_sys->frame.p_chain, p_frag); /* might be corrupted */
        return NULL;
    }

    const uint8_t i_layer = hevc_getNALLayer( p_buffer );
    bool b_first_slice_in_pic = p_buffer[2] & 0x80;
    if (b_first_slice_in_pic)
    {
        if(p_sys->frame.p_chain)
        {
            /* Starting new frame: return previous frame data for output */
            p_outputchain = OutputQueues(p_sys, p_sys->b_init_sequence_complete);
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
                else p_frag->i_flags |= BLOCK_FLAG_TYPE_B;
            }
            break;
        }
    }

    if(!p_sys->b_init_sequence_complete && i_layer == 0 &&
       (p_frag->i_flags & BLOCK_FLAG_TYPE_I) && XPSReady(p_sys))
    {
        p_sys->b_init_sequence_complete = true;
    }

    block_ChainLastAppend(&p_sys->frame.pp_chain_last, p_frag);

    return p_outputchain;
}

static block_t * ParseAUHead(decoder_t *p_dec, uint8_t i_nal_type, block_t *p_nalb)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_ret = NULL;

    if(p_sys->post.p_chain || p_sys->frame.p_chain)
        p_ret = OutputQueues(p_sys, true);

    switch(i_nal_type)
    {
        case HEVC_NAL_AUD:
            if(!p_ret && p_sys->pre.p_chain)
                p_ret = OutputQueues(p_sys, true);
            break;

        case HEVC_NAL_VPS:
        case HEVC_NAL_SPS:
        case HEVC_NAL_PPS:
        {
            uint8_t i_id;
            if( hevc_get_xps_id(p_nalb->p_buffer, p_nalb->i_buffer, &i_id) &&
                InsertXPS(p_dec, i_nal_type, i_id, p_nalb) )
            {
                const hevc_sequence_parameter_set_t *p_sps;
                if( i_nal_type == HEVC_NAL_SPS &&
                   (p_sps = p_dec->p_sys->rgi_p_decsps[i_id]) )
                {
                    if(!p_dec->fmt_out.video.i_frame_rate)
                    {
                        (void) hevc_get_frame_rate( p_sps, p_dec->p_sys->rgi_p_decvps,
                                                    &p_dec->fmt_out.video.i_frame_rate,
                                                    &p_dec->fmt_out.video.i_frame_rate_base );
                    }

                    if(p_dec->fmt_out.video.primaries == COLOR_PRIMARIES_UNDEF)
                    {
                        (void) hevc_get_colorimetry( p_sps,
                                                     &p_dec->fmt_out.video.primaries,
                                                     &p_dec->fmt_out.video.transfer,
                                                     &p_dec->fmt_out.video.space,
                                                     &p_dec->fmt_out.video.b_color_range_full);
                    }

                    unsigned sizes[4];
                    if( hevc_get_picture_size( p_sps, &sizes[0], &sizes[1],
                                                      &sizes[2], &sizes[3] ) )
                    {
                        if( p_dec->fmt_out.video.i_width != sizes[0] ||
                            p_dec->fmt_out.video.i_height != sizes[1] )
                        {
                            p_dec->fmt_out.video.i_width = sizes[0];
                            p_dec->fmt_out.video.i_height = sizes[1];
                        }
                    }

                    if(p_dec->fmt_out.i_profile == -1)
                    {
                        uint8_t i_profile, i_level;
                        if( hevc_get_sps_profile_tier_level( p_sps, &i_profile, &i_level ) )
                        {
                            p_dec->fmt_out.i_profile = i_profile;
                            p_dec->fmt_out.i_level = i_level;
                        }
                    }
                }
            }
        }
        // ft
        default:
            break;
    }

    block_ChainLastAppend(&p_sys->pre.pp_chain_last, p_nalb);

    return p_ret;
}

static block_t * ParseAUTail(decoder_t *p_dec, uint8_t i_nal_type, block_t *p_nalb)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_ret = NULL;

    block_ChainLastAppend(&p_sys->post.pp_chain_last, p_nalb);

    switch(i_nal_type)
    {
        case HEVC_NAL_EOS:
        case HEVC_NAL_EOB:
            p_ret = OutputQueues(p_sys, true);
            break;
    }

    if(!p_ret && p_sys->frame.p_chain == NULL)
        p_ret = OutputQueues(p_sys, false);

    return p_ret;
}

static block_t * ParseNonVCL(decoder_t *p_dec, uint8_t i_nal_type, block_t *p_nalb)
{
    block_t *p_ret = NULL;

    if ( (i_nal_type >= HEVC_NAL_VPS        && i_nal_type <= HEVC_NAL_AUD) ||
          i_nal_type == HEVC_NAL_PREF_SEI ||
         (i_nal_type >= HEVC_NAL_RSV_NVCL41 && i_nal_type <= HEVC_NAL_RSV_NVCL44) ||
         (i_nal_type >= HEVC_NAL_UNSPEC48   && i_nal_type <= HEVC_NAL_UNSPEC55) )
    {
        p_ret = ParseAUHead(p_dec, i_nal_type, p_nalb);
    }
    else
    {
        p_ret = ParseAUTail(p_dec, i_nal_type, p_nalb);
    }

    return p_ret;
}

static block_t *GatherAndValidateChain(block_t *p_outputchain)
{
    block_t *p_output = NULL;

    if(p_outputchain)
    {
        if(p_outputchain->i_flags & BLOCK_FLAG_CORRUPTED)
            p_output = p_outputchain; /* Avoid useless gather */
        else
            p_output = block_ChainGather(p_outputchain);
    }

    if(p_output && (p_output->i_flags & BLOCK_FLAG_CORRUPTED))
    {
        block_ChainRelease(p_output); /* Chain! see above */
        p_output = NULL;
    }

    return p_output;
}

/*****************************************************************************
 * ParseNALBlock: parses annexB type NALs
 * All p_frag blocks are required to start with 0 0 0 1 4-byte startcode
 *****************************************************************************/
static block_t *ParseNALBlock(decoder_t *p_dec, bool *pb_ts_used, block_t *p_frag)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if(unlikely(p_frag->i_buffer < 5))
    {
        msg_Warn(p_dec,"NAL too small");
        block_Release(p_frag);
        *pb_ts_used = false;
        return NULL;
    }

    if(p_frag->p_buffer[4] & 0x80)
    {
        msg_Warn(p_dec,"Forbidden zero bit not null, corrupted NAL");
        block_Release(p_frag);
        *pb_ts_used = false;
        return GatherAndValidateChain(OutputQueues(p_sys, false)); /* will drop */
    }

    /* Get NALU type */
    block_t * p_output = NULL;
    uint8_t i_nal_type = hevc_getNALType(&p_frag->p_buffer[4]);
    if (i_nal_type < HEVC_NAL_VPS)
    {
        /* NAL is a VCL NAL */
        p_output = ParseVCL(p_dec, i_nal_type, p_frag);
        if (p_output && (p_output->i_flags & BLOCK_FLAG_CORRUPTED))
            msg_Info(p_dec, "Waiting for VPS/SPS/PPS");
    }
    else
    {
        p_output = ParseNonVCL(p_dec, i_nal_type, p_frag);
    }

    p_output = GatherAndValidateChain(p_output);
    *pb_ts_used = (p_output != NULL);
    return p_output;
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
