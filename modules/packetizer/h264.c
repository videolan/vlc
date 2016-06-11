/*****************************************************************************
 * h264.c: h264/avc video packetizer
 *****************************************************************************
 * Copyright (C) 2001, 2002, 2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
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
#include <vlc_sout.h>
#include <vlc_codec.h>
#include <vlc_block.h>

#include <vlc_block_helper.h>
#include <vlc_bits.h>
#include "../codec/cc.h"
#include "h264_nal.h"
#include "hxxx_nal.h"
#include "hxxx_common.h"
#include "packetizer_helper.h"
#include "startcode_helper.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_PACKETIZER )
    set_description( N_("H.264 video packetizer") )
    set_capability( "packetizer", 50 )
    set_callbacks( Open, Close )
vlc_module_end ()


/****************************************************************************
 * Local prototypes
 ****************************************************************************/
typedef struct
{
    int i_nal_type;
    int i_nal_ref_idc;

    int i_frame_type;
    int i_pic_parameter_set_id;
    int i_frame_num;

    int i_field_pic_flag;
    int i_bottom_field_flag;

    int i_idr_pic_id;

    int i_pic_order_cnt_lsb;
    int i_delta_pic_order_cnt_bottom;

    int i_delta_pic_order_cnt0;
    int i_delta_pic_order_cnt1;
} slice_t;

struct decoder_sys_t
{
    /* */
    packetizer_t packetizer;

    /* */
    bool    b_slice;
    block_t *p_frame;
    block_t **pp_frame_last;
    bool    b_frame_sps;
    bool    b_frame_pps;

    bool   b_header;
    bool   b_sps;
    bool   b_pps;
    block_t *pp_sps[H264_SPS_MAX];
    block_t *pp_pps[H264_PPS_MAX];
    int    i_recovery_frames;  /* -1 = no recovery */

    /* avcC data */
    uint8_t i_avcC_length_size;

    /* Useful values of the Sequence Parameter Set */
    int i_log2_max_frame_num;
    int b_frame_mbs_only;
    int i_pic_order_cnt_type;
    int i_delta_pic_order_always_zero_flag;
    int i_log2_max_pic_order_cnt_lsb;

    /* Value from Picture Parameter Set */
    int i_pic_order_present_flag;

    /* VUI */
    bool b_timing_info_present_flag;
    uint32_t i_num_units_in_tick;
    uint32_t i_time_scale;
    bool b_fixed_frame_rate;
    bool b_pic_struct_present_flag;
    uint8_t i_pic_struct;
    bool b_cpb_dpb_delays_present_flag;
    uint8_t i_cpb_removal_delay_length_minus1;
    uint8_t i_dpb_output_delay_length_minus1;

    /* Useful values of the Slice Header */
    slice_t slice;

    /* */
    bool b_even_frame;
    mtime_t i_frame_pts;
    mtime_t i_frame_dts;
    mtime_t i_prev_pts;
    mtime_t i_prev_dts;

    /* */
    uint32_t i_cc_flags;
    mtime_t i_cc_pts;
    mtime_t i_cc_dts;
    cc_data_t cc;

    cc_data_t cc_next;
};

#define BLOCK_FLAG_PRIVATE_AUD (1 << BLOCK_FLAG_PRIVATE_SHIFT)

static block_t *Packetize( decoder_t *, block_t ** );
static block_t *PacketizeAVC1( decoder_t *, block_t ** );
static block_t *GetCc( decoder_t *p_dec, bool pb_present[4] );
static void PacketizeFlush( decoder_t * );

static void PacketizeReset( void *p_private, bool b_broken );
static block_t *PacketizeParse( void *p_private, bool *pb_ts_used, block_t * );
static int PacketizeValidate( void *p_private, block_t * );

static block_t *ParseNALBlock( decoder_t *, bool *pb_ts_used, block_t * );

static block_t *OutputPicture( decoder_t *p_dec );
static void PutSPS( decoder_t *p_dec, block_t *p_frag );
static void PutPPS( decoder_t *p_dec, block_t *p_frag );
static bool ParseSlice( decoder_t *p_dec, bool *pb_new_picture, slice_t *p_slice,
                        int i_nal_ref_idc, int i_nal_type, const block_t *p_frag );
static void ParseSei( decoder_t *, block_t * );


static const uint8_t p_h264_startcode[3] = { 0x00, 0x00, 0x01 };

/*****************************************************************************
 * Open: probe the packetizer and return score
 * When opening after demux, the packetizer is only loaded AFTER the decoder
 * That means that what you set in fmt_out is ignored by the decoder in this special case
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;
    int i;

    const bool b_avc = (p_dec->fmt_in.i_original_fourcc == VLC_FOURCC( 'a', 'v', 'c', '1' ));

    if( p_dec->fmt_in.i_codec != VLC_CODEC_H264 )
        return VLC_EGENERIC;
    if( b_avc && p_dec->fmt_in.i_extra < 7 )
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = malloc( sizeof(decoder_sys_t) ) ) == NULL )
    {
        return VLC_ENOMEM;
    }

    packetizer_Init( &p_sys->packetizer,
                     p_h264_startcode, sizeof(p_h264_startcode), startcode_FindAnnexB,
                     p_h264_startcode, 1, 5,
                     PacketizeReset, PacketizeParse, PacketizeValidate, p_dec );

    p_sys->b_slice = false;
    p_sys->p_frame = NULL;
    p_sys->pp_frame_last = &p_sys->p_frame;
    p_sys->b_frame_sps = false;
    p_sys->b_frame_pps = false;

    p_sys->b_header= false;
    p_sys->b_sps   = false;
    p_sys->b_pps   = false;
    for( i = 0; i < H264_SPS_MAX; i++ )
        p_sys->pp_sps[i] = NULL;
    for( i = 0; i < H264_PPS_MAX; i++ )
        p_sys->pp_pps[i] = NULL;
    p_sys->i_recovery_frames = -1;

    p_sys->slice.i_nal_type = -1;
    p_sys->slice.i_nal_ref_idc = -1;
    p_sys->slice.i_idr_pic_id = -1;
    p_sys->slice.i_frame_num = -1;
    p_sys->slice.i_frame_type = 0;
    p_sys->slice.i_pic_parameter_set_id = -1;
    p_sys->slice.i_field_pic_flag = 0;
    p_sys->slice.i_bottom_field_flag = -1;
    p_sys->slice.i_pic_order_cnt_lsb = -1;
    p_sys->slice.i_delta_pic_order_cnt_bottom = -1;

    p_sys->b_timing_info_present_flag = false;
    p_sys->b_pic_struct_present_flag = false;
    p_sys->b_cpb_dpb_delays_present_flag = false;
    p_sys->i_cpb_removal_delay_length_minus1 = 0;
    p_sys->i_dpb_output_delay_length_minus1 = 0;

    p_sys->b_even_frame = false;
    p_sys->i_frame_dts = VLC_TS_INVALID;
    p_sys->i_frame_pts = VLC_TS_INVALID;
    p_sys->i_prev_dts = VLC_TS_INVALID;
    p_sys->i_prev_pts = VLC_TS_INVALID;

    /* Setup properties */
    es_format_Copy( &p_dec->fmt_out, &p_dec->fmt_in );
    p_dec->fmt_out.i_codec = VLC_CODEC_H264;
    p_dec->fmt_out.b_packetized = true;

    if( b_avc )
    {
        /* This type of stream is produced by mp4 and matroska
         * when we want to store it in another streamformat, you need to convert
         * The fmt_in.p_extra should ALWAYS contain the avcC
         * The fmt_out.p_extra should contain all the SPS and PPS with 4 byte startcodes */
        if( h264_isavcC( p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra ) )
        {
            free( p_dec->fmt_out.p_extra );
            size_t i_size;
            p_dec->fmt_out.p_extra = h264_avcC_to_AnnexB_NAL( p_dec->fmt_in.p_extra,
                                                              p_dec->fmt_in.i_extra,
                                                             &i_size,
                                                             &p_sys->i_avcC_length_size );
            p_dec->fmt_out.i_extra = i_size;
            p_sys->b_header = !!p_dec->fmt_out.i_extra;

            if(!p_dec->fmt_out.p_extra)
            {
                msg_Err( p_dec, "Invalid AVC extradata");
                return VLC_EGENERIC;
            }
        }
        else
        {
            msg_Err( p_dec, "Invalid or missing AVC extradata");
            return VLC_EGENERIC;
        }

        /* Set callback */
        p_dec->pf_packetize = PacketizeAVC1;
    }
    else
    {
        /* This type of stream contains data with 3 of 4 byte startcodes
         * The fmt_in.p_extra MAY contain SPS/PPS with 4 byte startcodes
         * The fmt_out.p_extra should be the same */

        /* Set callback */
        p_dec->pf_packetize = Packetize;
    }

    /* */
    if( p_dec->fmt_out.i_extra > 0 )
    {
        packetizer_Header( &p_sys->packetizer,
                           p_dec->fmt_out.p_extra, p_dec->fmt_out.i_extra );
    }

    if( b_avc )
    {
        if( !p_sys->b_sps || !p_sys->b_pps )
        {
            msg_Err( p_dec, "Invalid or missing SPS %d or PPS %d in AVC extradata",
                     p_sys->b_sps, p_sys->b_pps );
            return VLC_EGENERIC;
        }

        msg_Dbg( p_dec, "Packetizer fed with AVC, nal length size=%d",
                         p_sys->i_avcC_length_size );
    }

    /* CC are the same for H264/AVC in T35 sections (ETSI TS 101 154)  */
    p_dec->pf_get_cc = GetCc;
    p_dec->pf_flush = PacketizeFlush;

    /* */
    p_sys->i_cc_pts = VLC_TS_INVALID;
    p_sys->i_cc_dts = VLC_TS_INVALID;
    p_sys->i_cc_flags = 0;
    cc_Init( &p_sys->cc );
    cc_Init( &p_sys->cc_next );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: clean up the packetizer
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i;

    if( p_sys->p_frame )
        block_ChainRelease( p_sys->p_frame );
    for( i = 0; i < H264_SPS_MAX; i++ )
    {
        if( p_sys->pp_sps[i] )
            block_Release( p_sys->pp_sps[i] );
    }
    for( i = 0; i < H264_PPS_MAX; i++ )
    {
        if( p_sys->pp_pps[i] )
            block_Release( p_sys->pp_pps[i] );
    }
    packetizer_Clean( &p_sys->packetizer );

    cc_Exit( &p_sys->cc_next );
    cc_Exit( &p_sys->cc );

    free( p_sys );
}

static void PacketizeFlush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    packetizer_Flush( &p_sys->packetizer );
}

/****************************************************************************
 * Packetize: the whole thing
 * Search for the startcodes 3 or more bytes
 * Feed ParseNALBlock ALWAYS with 4 byte startcode prepended NALs
 ****************************************************************************/
static block_t *Packetize( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    return packetizer_Packetize( &p_sys->packetizer, pp_block );
}

/****************************************************************************
 * PacketizeAVC1: Takes VCL blocks of data and creates annexe B type NAL stream
 * Will always use 4 byte 0 0 0 1 startcodes
 * Will prepend a SPS and PPS before each keyframe
 ****************************************************************************/
static block_t *PacketizeAVC1( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    return PacketizeXXC1( p_dec, p_sys->i_avcC_length_size,
                          pp_block, ParseNALBlock );
}

/*****************************************************************************
 * GetCc:
 *****************************************************************************/
static block_t *GetCc( decoder_t *p_dec, bool pb_present[4] )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_cc;

    for( int i = 0; i < 4; i++ )
        pb_present[i] = p_sys->cc.pb_present[i];

    if( p_sys->cc.i_data <= 0 )
        return NULL;

    p_cc = block_Alloc( p_sys->cc.i_data);
    if( p_cc )
    {
        memcpy( p_cc->p_buffer, p_sys->cc.p_data, p_sys->cc.i_data );
        p_cc->i_dts =
        p_cc->i_pts = p_sys->cc.b_reorder ? p_sys->i_cc_pts : p_sys->i_cc_dts;
        p_cc->i_flags = ( p_sys->cc.b_reorder  ? p_sys->i_cc_flags : BLOCK_FLAG_TYPE_P ) & BLOCK_FLAG_TYPE_MASK;
    }
    cc_Flush( &p_sys->cc );
    return p_cc;
}

/****************************************************************************
 * Helpers
 ****************************************************************************/
static void PacketizeReset( void *p_private, bool b_broken )
{
    decoder_t *p_dec = p_private;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( b_broken )
    {
        if( p_sys->p_frame )
            block_ChainRelease( p_sys->p_frame );
        p_sys->p_frame = NULL;
        p_sys->pp_frame_last = &p_sys->p_frame;
        p_sys->b_frame_sps = false;
        p_sys->b_frame_pps = false;
        p_sys->slice.i_frame_type = 0;
        p_sys->b_slice = false;
    }
    p_sys->i_frame_pts = VLC_TS_INVALID;
    p_sys->i_frame_dts = VLC_TS_INVALID;
    p_sys->i_prev_dts = VLC_TS_INVALID;
    p_sys->i_prev_pts = VLC_TS_INVALID;
    p_sys->b_even_frame = false;
}
static block_t *PacketizeParse( void *p_private, bool *pb_ts_used, block_t *p_block )
{
    decoder_t *p_dec = p_private;

    /* Remove trailing 0 bytes */
    while( p_block->i_buffer > 5 && p_block->p_buffer[p_block->i_buffer-1] == 0x00 )
        p_block->i_buffer--;

    return ParseNALBlock( p_dec, pb_ts_used, p_block );
}
static int PacketizeValidate( void *p_private, block_t *p_au )
{
    VLC_UNUSED(p_private);
    VLC_UNUSED(p_au);
    return VLC_SUCCESS;
}

/*****************************************************************************
 * ParseNALBlock: parses annexB type NALs
 * All p_frag blocks are required to start with 0 0 0 1 4-byte startcode
 *****************************************************************************/
static block_t *ParseNALBlock( decoder_t *p_dec, bool *pb_ts_used, block_t *p_frag )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_pic = NULL;

    const int i_nal_ref_idc = (p_frag->p_buffer[4] >> 5)&0x03;
    const int i_nal_type = p_frag->p_buffer[4]&0x1f;
    const mtime_t i_frag_dts = p_frag->i_dts;
    const mtime_t i_frag_pts = p_frag->i_pts;

    if( p_sys->b_slice && ( !p_sys->b_sps || !p_sys->b_pps ) )
    {
        block_ChainRelease( p_sys->p_frame );
        msg_Warn( p_dec, "waiting for SPS/PPS" );

        /* Reset context */
        p_sys->slice.i_frame_type = 0;
        p_sys->p_frame = NULL;
        p_sys->pp_frame_last = &p_sys->p_frame;
        p_sys->b_frame_sps = false;
        p_sys->b_frame_pps = false;
        p_sys->b_slice = false;
        cc_Flush( &p_sys->cc_next );
    }

    if( ( !p_sys->b_sps || !p_sys->b_pps ) &&
        i_nal_type >= H264_NAL_SLICE && i_nal_type <= H264_NAL_SLICE_IDR )
    {
        p_sys->b_slice = true;
        /* Fragment will be discarded later on */
    }
    else if( i_nal_type >= H264_NAL_SLICE && i_nal_type <= H264_NAL_SLICE_IDR )
    {
        slice_t slice;
        bool  b_new_picture;

        if(ParseSlice( p_dec, &b_new_picture, &slice, i_nal_ref_idc, i_nal_type, p_frag ))
        {
            /* */
            if( b_new_picture && p_sys->b_slice )
                p_pic = OutputPicture( p_dec );

            /* */
            p_sys->slice = slice;
            p_sys->b_slice = true;
        }
    }
    else if( i_nal_type == H264_NAL_SPS )
    {
        if( p_sys->b_slice )
            p_pic = OutputPicture( p_dec );
        p_sys->b_frame_sps = true;

        PutSPS( p_dec, p_frag );

        /* Do not append the SPS because we will insert it on keyframes */
        p_frag = NULL;
    }
    else if( i_nal_type == H264_NAL_PPS )
    {
        if( p_sys->b_slice )
            p_pic = OutputPicture( p_dec );
        p_sys->b_frame_pps = true;

        PutPPS( p_dec, p_frag );

        /* Do not append the PPS because we will insert it on keyframes */
        p_frag = NULL;
    }
    else if( i_nal_type == H264_NAL_AU_DELIMITER ||
             i_nal_type == H264_NAL_SEI ||
             ( i_nal_type >= H264_NAL_PREFIX && i_nal_type <= H264_NAL_RESERVED_18 ) )
    {
        if( p_sys->b_slice )
            p_pic = OutputPicture( p_dec );

        /* Parse SEI for CC support */
        if( i_nal_type == H264_NAL_SEI )
        {
            ParseSei( p_dec, p_frag );
        }
        else if( i_nal_type == H264_NAL_AU_DELIMITER )
        {
            if( p_sys->p_frame && (p_sys->p_frame->i_flags & BLOCK_FLAG_PRIVATE_AUD) )
            {
                block_Release( p_frag );
                p_frag = NULL;
            }
            else
            {
                p_frag->i_flags |= BLOCK_FLAG_PRIVATE_AUD;
            }
        }
    }

    /* Append the block */
    if( p_frag )
        block_ChainLastAppend( &p_sys->pp_frame_last, p_frag );

    *pb_ts_used = false;
    if( p_sys->i_frame_dts <= VLC_TS_INVALID &&
        p_sys->i_frame_pts <= VLC_TS_INVALID )
    {
        p_sys->i_frame_dts = i_frag_dts;
        p_sys->i_frame_pts = i_frag_pts;
        *pb_ts_used = true;
    }
    return p_pic;
}

static block_t *OutputPicture( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_pic;

    if ( !p_sys->b_header && p_sys->i_recovery_frames != -1 )
    {
        if( p_sys->i_recovery_frames == 0 )
        {
            msg_Dbg( p_dec, "Recovery from SEI recovery point complete" );
            p_sys->b_header = true;
        }
        --p_sys->i_recovery_frames;
    }

    if( !p_sys->b_header && p_sys->i_recovery_frames == -1 &&
         p_sys->slice.i_frame_type != BLOCK_FLAG_TYPE_I)
        return NULL;

    const bool b_sps_pps_i = p_sys->slice.i_frame_type == BLOCK_FLAG_TYPE_I &&
                             p_sys->b_sps &&
                             p_sys->b_pps;
    if( b_sps_pps_i || p_sys->b_frame_sps || p_sys->b_frame_pps )
    {
        block_t *p_head = NULL;
        if( p_sys->p_frame->i_flags & BLOCK_FLAG_PRIVATE_AUD )
        {
            p_head = p_sys->p_frame;
            p_sys->p_frame = p_sys->p_frame->p_next;
            if( p_sys->p_frame == NULL )
                p_sys->pp_frame_last = &p_sys->p_frame;
            p_head->p_next = NULL;
        }

        block_t *p_list = NULL;
        block_t **pp_list_tail = &p_list;
        for( int i = 0; i < H264_SPS_MAX && (b_sps_pps_i || p_sys->b_frame_sps); i++ )
        {
            if( p_sys->pp_sps[i] )
                block_ChainLastAppend( &pp_list_tail, block_Duplicate( p_sys->pp_sps[i] ) );
        }
        for( int i = 0; i < H264_PPS_MAX && (b_sps_pps_i || p_sys->b_frame_pps); i++ )
        {
            if( p_sys->pp_pps[i] )
                block_ChainLastAppend( &pp_list_tail, block_Duplicate( p_sys->pp_pps[i] ) );
        }
        if( b_sps_pps_i && p_list )
            p_sys->b_header = true;

        if( p_list )
            block_ChainAppend( &p_head, p_list );

        if( p_sys->p_frame )
            block_ChainAppend( &p_head, p_sys->p_frame );

        p_pic = block_ChainGather( p_head );
    }
    else
    {
        p_pic = block_ChainGather( p_sys->p_frame );
    }

    unsigned i_num_clock_ts = 1;
    if( p_sys->b_frame_mbs_only == 0 && p_sys->b_pic_struct_present_flag )
    {
        if( p_sys->i_pic_struct < 9 )
        {
            const uint8_t rgi_numclock[9] = { 1, 1, 1, 2, 2, 3, 3, 2, 3 };
            i_num_clock_ts = rgi_numclock[ p_sys->i_pic_struct ];
        }
    }

    if( p_sys->i_time_scale )
    {
        p_pic->i_length = CLOCK_FREQ * i_num_clock_ts *
                          p_sys->i_num_units_in_tick / p_sys->i_time_scale;
    }

    mtime_t i_field_pts = VLC_TS_INVALID;
    if( p_sys->b_frame_mbs_only == 0 && p_sys->b_pic_struct_present_flag )
    {
        switch( p_sys->i_pic_struct )
        {
        /* Top and Bottom field slices */
        case 1:
        case 2:
            if( !p_sys->b_even_frame )
            {
                p_pic->i_flags |= (p_sys->i_pic_struct == 1) ? BLOCK_FLAG_TOP_FIELD_FIRST
                                                             : BLOCK_FLAG_BOTTOM_FIELD_FIRST;
            }
            else if( p_pic->i_pts <= VLC_TS_INVALID && p_sys->i_prev_pts > VLC_TS_INVALID && p_pic->i_length )
            {
                /* interpolate from even frame */
                i_field_pts = p_sys->i_prev_pts + p_pic->i_length;
            }

            p_sys->b_even_frame = !p_sys->b_even_frame;
            break;
        /* Each of the following slices contains multiple fields */
        case 3:
            p_pic->i_flags |= BLOCK_FLAG_TOP_FIELD_FIRST;
            p_sys->b_even_frame = false;
            break;
        case 4:
            p_pic->i_flags |= BLOCK_FLAG_BOTTOM_FIELD_FIRST;
            p_sys->b_even_frame = false;
            break;
        case 5:
            p_pic->i_flags |= BLOCK_FLAG_TOP_FIELD_FIRST;
            break;
        case 6:
            p_pic->i_flags |= BLOCK_FLAG_BOTTOM_FIELD_FIRST;
            break;
        default:
            p_sys->b_even_frame = false;
            break;
        }
    }

    /* set dts/pts to current block timestamps */
    p_pic->i_dts = p_sys->i_frame_dts;
    p_pic->i_pts = p_sys->i_frame_pts;

    /* Fixup missing timestamps after split (multiple AU/block)*/
    if( p_pic->i_dts <= VLC_TS_INVALID )
        p_pic->i_dts = p_sys->i_prev_dts;

    /* PTS Fixup, interlaced fields (multiple AU/block) */
    if( p_pic->i_pts <= VLC_TS_INVALID && i_field_pts != VLC_TS_INVALID )
        p_pic->i_pts = i_field_pts;

    /* save for next pic fixups */
    p_sys->i_prev_dts = p_pic->i_dts;
    p_sys->i_prev_pts = p_pic->i_pts;

    p_pic->i_flags |= p_sys->slice.i_frame_type;
    p_pic->i_flags &= ~BLOCK_FLAG_PRIVATE_AUD;
    if( !p_sys->b_header )
        p_pic->i_flags |= BLOCK_FLAG_PREROLL;

    /* reset after output */
    p_sys->i_frame_dts = VLC_TS_INVALID;
    p_sys->i_frame_pts = VLC_TS_INVALID;
    p_sys->slice.i_frame_type = 0;
    p_sys->p_frame = NULL;
    p_sys->pp_frame_last = &p_sys->p_frame;
    p_sys->b_frame_sps = false;
    p_sys->b_frame_pps = false;
    p_sys->b_slice = false;

    /* CC */
    p_sys->i_cc_pts = p_pic->i_pts;
    p_sys->i_cc_dts = p_pic->i_dts;
    p_sys->i_cc_flags = p_pic->i_flags;

    p_sys->cc = p_sys->cc_next;
    cc_Flush( &p_sys->cc_next );

    return p_pic;
}

static void PutSPS( decoder_t *p_dec, block_t *p_frag )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    const uint8_t *p_buffer = p_frag->p_buffer;
    size_t i_buffer = p_frag->i_buffer;

    if( !hxxx_strip_AnnexB_startcode( &p_buffer, &i_buffer ) )
        return;

    h264_sequence_parameter_set_t *p_sps = h264_decode_sps( p_buffer, i_buffer, true );
    if( !p_sps )
    {
        msg_Warn( p_dec, "invalid SPS" );
        block_Release( p_frag );
        return;
    }

    p_dec->fmt_out.i_profile = p_sps->i_profile;
    p_dec->fmt_out.i_level = p_sps->i_level;

    (void) h264_get_picture_size( p_sps, &p_dec->fmt_out.video.i_width,
                                         &p_dec->fmt_out.video.i_height,
                                         &p_dec->fmt_out.video.i_visible_width,
                                         &p_dec->fmt_out.video.i_visible_height );

    if( p_sps->vui.i_sar_num != 0 && p_sps->vui.i_sar_den != 0 )
    {
        p_dec->fmt_out.video.i_sar_num = p_sps->vui.i_sar_num;
        p_dec->fmt_out.video.i_sar_den = p_sps->vui.i_sar_den;
    }

    p_sys->i_log2_max_frame_num = p_sps->i_log2_max_frame_num;
    p_sys->b_frame_mbs_only = p_sps->frame_mbs_only_flag;
    p_sys->i_pic_order_cnt_type = p_sps->i_pic_order_cnt_type;
    p_sys->i_delta_pic_order_always_zero_flag = p_sps->i_delta_pic_order_always_zero_flag;
    p_sys->i_log2_max_pic_order_cnt_lsb = p_sps->i_log2_max_pic_order_cnt_lsb;

    if( p_sps->vui.b_valid )
    {
        p_sys->b_timing_info_present_flag = p_sps->vui.b_timing_info_present_flag;
        p_sys->i_num_units_in_tick =  p_sps->vui.i_num_units_in_tick;
        p_sys->i_time_scale = p_sps->vui.i_time_scale;
        p_sys->b_fixed_frame_rate = p_sps->vui.b_fixed_frame_rate;
        p_sys->b_pic_struct_present_flag = p_sps->vui.b_pic_struct_present_flag;
        p_sys->b_cpb_dpb_delays_present_flag = p_sps->vui.b_hrd_parameters_present_flag;
        p_sys->i_cpb_removal_delay_length_minus1 = p_sps->vui.i_cpb_removal_delay_length_minus1;
        p_sys->i_dpb_output_delay_length_minus1 = p_sps->vui.i_dpb_output_delay_length_minus1;

        if( p_sps->vui.b_fixed_frame_rate && !p_dec->fmt_out.video.i_frame_rate_base )
        {
            p_dec->fmt_out.video.i_frame_rate_base = p_sps->vui.i_num_units_in_tick;
            p_dec->fmt_out.video.i_frame_rate = p_sps->vui.i_time_scale;
        }
        p_dec->fmt_out.video.primaries =
            hxxx_colour_primaries_to_vlc( p_sps->vui.colour.i_colour_primaries );
        p_dec->fmt_out.video.transfer =
            hxxx_transfer_characteristics_to_vlc( p_sps->vui.colour.i_transfer_characteristics );
        p_dec->fmt_out.video.space =
            hxxx_matrix_coeffs_to_vlc( p_sps->vui.colour.i_matrix_coefficients );
        p_dec->fmt_out.video.b_color_range_full = p_sps->vui.colour.b_full_range;
    }
    /* We have a new SPS */
    if( !p_sys->b_sps )
        msg_Dbg( p_dec, "found NAL_SPS (sps_id=%d)", p_sps->i_id );
    p_sys->b_sps = true;

    if( p_sys->pp_sps[p_sps->i_id] )
        block_Release( p_sys->pp_sps[p_sps->i_id] );
    p_sys->pp_sps[p_sps->i_id] = p_frag;

    h264_release_sps( p_sps );
}

static void PutPPS( decoder_t *p_dec, block_t *p_frag )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    const uint8_t *p_buffer = p_frag->p_buffer;
    size_t i_buffer = p_frag->i_buffer;

    if( !hxxx_strip_AnnexB_startcode( &p_buffer, &i_buffer ) )
        return;

    h264_picture_parameter_set_t *p_pps = h264_decode_pps( p_buffer, i_buffer, true );
    if( !p_pps )
    {
        msg_Warn( p_dec, "invalid PPS" );
        block_Release( p_frag );
        return;
    }
    p_sys->i_pic_order_present_flag = p_pps->i_pic_order_present_flag;

    /* We have a new PPS */
    if( !p_sys->b_pps )
        msg_Dbg( p_dec, "found NAL_PPS (pps_id=%d sps_id=%d)", p_pps->i_id, p_pps->i_sps_id );
    p_sys->b_pps = true;

    if( p_sys->pp_pps[p_pps->i_id] )
        block_Release( p_sys->pp_pps[p_pps->i_id] );
    p_sys->pp_pps[p_pps->i_id] = p_frag;

    h264_release_pps( p_pps );
}

static bool ParseSlice( decoder_t *p_dec, bool *pb_new_picture, slice_t *p_slice,
                        int i_nal_ref_idc, int i_nal_type, const block_t *p_frag )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_slice_type;
    slice_t slice;
    bs_t s;
    unsigned i_bitflow = 0;

    const uint8_t *p_stripped = p_frag->p_buffer;
    size_t i_stripped = p_frag->i_buffer;

    if( !hxxx_strip_AnnexB_startcode( &p_stripped, &i_stripped ) || i_stripped < 2 )
        return false;

    bs_init( &s, p_stripped, i_stripped );
    s.p_fwpriv = &i_bitflow;
    s.pf_forward = hxxx_bsfw_ep3b_to_rbsp;  /* Does the emulated 3bytes conversion to rbsp */
    bs_skip( &s, 8 ); /* nal unit header */

    /* first_mb_in_slice */
    /* int i_first_mb = */ bs_read_ue( &s );

    /* slice_type */
    switch( (i_slice_type = bs_read_ue( &s )) )
    {
    case 0: case 5:
        slice.i_frame_type = BLOCK_FLAG_TYPE_P;
        break;
    case 1: case 6:
        slice.i_frame_type = BLOCK_FLAG_TYPE_B;
        break;
    case 2: case 7:
        slice.i_frame_type = BLOCK_FLAG_TYPE_I;
        break;
    case 3: case 8: /* SP */
        slice.i_frame_type = BLOCK_FLAG_TYPE_P;
        break;
    case 4: case 9:
        slice.i_frame_type = BLOCK_FLAG_TYPE_I;
        break;
    default:
        slice.i_frame_type = 0;
        break;
    }

    /* */
    slice.i_nal_type = i_nal_type;
    slice.i_nal_ref_idc = i_nal_ref_idc;

    slice.i_pic_parameter_set_id = bs_read_ue( &s );
    slice.i_frame_num = bs_read( &s, p_sys->i_log2_max_frame_num + 4 );

    slice.i_field_pic_flag = 0;
    slice.i_bottom_field_flag = -1;
    if( !p_sys->b_frame_mbs_only )
    {
        /* field_pic_flag */
        slice.i_field_pic_flag = bs_read( &s, 1 );
        if( slice.i_field_pic_flag )
            slice.i_bottom_field_flag = bs_read( &s, 1 );
    }

    slice.i_idr_pic_id = p_sys->slice.i_idr_pic_id;
    if( slice.i_nal_type == H264_NAL_SLICE_IDR )
        slice.i_idr_pic_id = bs_read_ue( &s );

    slice.i_pic_order_cnt_lsb = -1;
    slice.i_delta_pic_order_cnt_bottom = -1;
    slice.i_delta_pic_order_cnt0 = 0;
    slice.i_delta_pic_order_cnt1 = 0;
    if( p_sys->i_pic_order_cnt_type == 0 )
    {
        slice.i_pic_order_cnt_lsb = bs_read( &s, p_sys->i_log2_max_pic_order_cnt_lsb + 4 );
        if( p_sys->i_pic_order_present_flag && !slice.i_field_pic_flag )
            slice.i_delta_pic_order_cnt_bottom = bs_read_se( &s );
    }
    else if( (p_sys->i_pic_order_cnt_type == 1) &&
             (!p_sys->i_delta_pic_order_always_zero_flag) )
    {
        slice.i_delta_pic_order_cnt0 = bs_read_se( &s );
        if( p_sys->i_pic_order_present_flag && !slice.i_field_pic_flag )
            slice.i_delta_pic_order_cnt1 = bs_read_se( &s );
    }

    /* Detection of the first VCL NAL unit of a primary coded picture
     * (cf. 7.4.1.2.4) */
    bool b_pic = false;
    if( slice.i_frame_num != p_sys->slice.i_frame_num ||
        slice.i_pic_parameter_set_id != p_sys->slice.i_pic_parameter_set_id ||
        slice.i_field_pic_flag != p_sys->slice.i_field_pic_flag ||
        !slice.i_nal_ref_idc != !p_sys->slice.i_nal_ref_idc )
        b_pic = true;
    if( (slice.i_bottom_field_flag != -1) &&
        (p_sys->slice.i_bottom_field_flag != -1) &&
        (slice.i_bottom_field_flag != p_sys->slice.i_bottom_field_flag) )
        b_pic = true;
    if( p_sys->i_pic_order_cnt_type == 0 &&
        ( slice.i_pic_order_cnt_lsb != p_sys->slice.i_pic_order_cnt_lsb ||
          slice.i_delta_pic_order_cnt_bottom != p_sys->slice.i_delta_pic_order_cnt_bottom ) )
        b_pic = true;
    else if( p_sys->i_pic_order_cnt_type == 1 &&
             ( slice.i_delta_pic_order_cnt0 != p_sys->slice.i_delta_pic_order_cnt0 ||
               slice.i_delta_pic_order_cnt1 != p_sys->slice.i_delta_pic_order_cnt1 ) )
        b_pic = true;
    if( ( slice.i_nal_type == H264_NAL_SLICE_IDR || p_sys->slice.i_nal_type == H264_NAL_SLICE_IDR ) &&
        ( slice.i_nal_type != p_sys->slice.i_nal_type || slice.i_idr_pic_id != p_sys->slice.i_idr_pic_id ) )
            b_pic = true;

    /* */
    *pb_new_picture = b_pic;
    *p_slice = slice;

    return true;
}

static void ParseSei( decoder_t *p_dec, block_t *p_frag )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    bs_t s;
    unsigned i_bitflow = 0;

    const uint8_t *p_stripped = p_frag->p_buffer;
    size_t i_stripped = p_frag->i_buffer;

    if( !hxxx_strip_AnnexB_startcode( &p_stripped, &i_stripped ) || i_stripped < 2 )
        return;

    bs_init( &s, p_stripped, i_stripped );
    s.p_fwpriv = &i_bitflow;
    s.pf_forward = hxxx_bsfw_ep3b_to_rbsp;  /* Does the emulated 3bytes conversion to rbsp */
    bs_skip( &s, 8 ); /* nal unit header */

    while( bs_remain( &s ) >= 8 && bs_aligned( &s ) )
    {
        /* Read type */
        unsigned i_type = 0;
        while( bs_remain( &s ) >= 8 )
        {
            const uint8_t i_byte = bs_read( &s, 8 );
            i_type += i_byte;
            if( i_byte != 0xff )
                break;
        }

        /* Read size */
        unsigned i_size = 0;
        while( bs_remain( &s ) >= 8 )
        {
            const uint8_t i_byte = bs_read( &s, 8 );
            i_size += i_byte;
            if( i_byte != 0xff )
                break;
        }

        /* Check room */
        if( bs_remain( &s ) < 8 )
            break;

        /* Save start offset */
        const unsigned i_start_bit_pos = bs_pos( &s );

        switch( i_type )
        {
            /* Look for pic timing */
            case H264_SEI_PIC_TIMING:
            {
                if( p_sys->b_cpb_dpb_delays_present_flag )
                {
                    bs_read( &s, p_sys->i_cpb_removal_delay_length_minus1 + 1 );
                    bs_read( &s, p_sys->i_dpb_output_delay_length_minus1 + 1 );
                }

                if( p_sys->b_pic_struct_present_flag )
                    p_sys->i_pic_struct = bs_read( &s, 4 );
                /* + unparsed remains */
            } break;

            /* Look for user_data_registered_itu_t_t35 */
            case H264_SEI_USER_DATA_REGISTERED_ITU_T_T35:
            {
                /* TS 101 154 Auxiliary Data and H264/AVC video */
                static const uint8_t p_DVB1_data_start_code[] = {
                    0xb5, /* United States */
                    0x00, 0x31, /* US provider code */
                    0x47, 0x41, 0x39, 0x34 /* user identifier */
                };

                static const uint8_t p_DIRECTV_data_start_code[] = {
                    0xb5, /* United States */
                    0x00, 0x2f, /* US provider code */
                    0x03  /* Captions */
                };

                const unsigned i_t35 = i_size;
                uint8_t *p_t35 = malloc( i_t35 );
                if( !p_t35 )
                    break;
                for( unsigned i=0; i<i_t35; i++ )
                    p_t35[i] = bs_read( &s, 8 );

                /* Check for we have DVB1_data() */
                if( i_t35 >= sizeof(p_DVB1_data_start_code) &&
                        !memcmp( p_t35, p_DVB1_data_start_code, sizeof(p_DVB1_data_start_code) ) )
                {
                    cc_Extract( &p_sys->cc_next, true, &p_t35[3], i_t35 - 3 );
                } else if( i_t35 >= sizeof(p_DIRECTV_data_start_code) &&
                           !memcmp( p_t35, p_DIRECTV_data_start_code, sizeof(p_DIRECTV_data_start_code) ) )
                {
                    cc_Extract( &p_sys->cc_next, true, &p_t35[3], i_t35 - 3 );
                }

                free( p_t35 );
            } break;

            /* Look for SEI recovery point */
            case H264_SEI_RECOVERY_POINT:
            {
                int i_recovery_frames = bs_read_ue( &s );
                //bool b_exact_match = bs_read( &s, 1 );
                //bool b_broken_link = bs_read( &s, 1 );
                //int i_changing_slice_group = bs_read( &s, 2 );
                if( !p_sys->b_header )
                {
                    msg_Dbg( p_dec, "Seen SEI recovery point, %d recovery frames", i_recovery_frames );
                    if ( p_sys->i_recovery_frames == -1 || i_recovery_frames < p_sys->i_recovery_frames )
                        p_sys->i_recovery_frames = i_recovery_frames;
                }
            } break;

            default:
                /* Will skip */
                break;
        }

        const unsigned i_end_bit_pos = bs_pos( &s );
        /* Skip unsparsed content */
        if( i_end_bit_pos - i_start_bit_pos > i_size * 8 ) /* Something went wrong with _ue reads */
            break;
        bs_skip( &s, i_size * 8 - ( i_end_bit_pos - i_start_bit_pos ) );
    }

}

