/*****************************************************************************
 * h264.c: h264/avc video packetizer
 *****************************************************************************
 * Copyright (C) 2001, 2002, 2006 VLC authors and VideoLAN
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
#include "h264_nal.h"
#include "h264_slice.h"
#include "hxxx_nal.h"
#include "hxxx_sei.h"
#include "hxxx_common.h"
#include "packetizer_helper.h"
#include "startcode_helper.h"

#include <limits.h>

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
    /* */
    packetizer_t packetizer;

    /* */
    bool    b_slice;
    struct
    {
        block_t *p_head;
        block_t **pp_append;
    } frame, leading;

    /* a new sps/pps can be transmitted outside of iframes */
    bool    b_new_sps;
    bool    b_new_pps;

    struct
    {
        block_t *p_block;
        h264_sequence_parameter_set_t *p_sps;
    } sps[H264_SPS_ID_MAX + 1];
    struct
    {
        block_t *p_block;
        h264_picture_parameter_set_t *p_pps;
    } pps[H264_PPS_ID_MAX + 1];
    const h264_sequence_parameter_set_t *p_active_sps;
    const h264_picture_parameter_set_t *p_active_pps;

    /* avcC data */
    uint8_t i_avcC_length_size;

    /* From SEI for current frame */
    uint8_t i_pic_struct;
    uint8_t i_dpb_output_delay;
    unsigned i_recovery_frame_cnt;

    /* Useful values of the Slice Header */
    h264_slice_t slice;

    /* */
    int i_next_block_flags;
    bool b_recovered;
    unsigned i_recoveryfnum;
    unsigned i_recoverystartfnum;

    /* POC */
    h264_poc_context_t pocctx;
    struct
    {
        vlc_tick_t pts;
        int num;
    } prevdatedpoc;

    vlc_tick_t i_frame_pts;
    vlc_tick_t i_frame_dts;

    date_t dts;

    /* */
    cc_storage_t *p_ccs;
} decoder_sys_t;

#define BLOCK_FLAG_PRIVATE_AUD (1 << BLOCK_FLAG_PRIVATE_SHIFT)
#define BLOCK_FLAG_PRIVATE_SEI (2 << BLOCK_FLAG_PRIVATE_SHIFT)
#define BLOCK_FLAG_DROP        (4 << BLOCK_FLAG_PRIVATE_SHIFT)

static block_t *Packetize( decoder_t *, block_t ** );
static block_t *PacketizeAVC1( decoder_t *, block_t ** );
static block_t *GetCc( decoder_t *p_dec, decoder_cc_desc_t * );
static void PacketizeFlush( decoder_t * );

static void PacketizeReset( void *p_private, bool b_broken );
static block_t *PacketizeParse( void *p_private, bool *pb_ts_used, block_t * );
static int PacketizeValidate( void *p_private, block_t * );
static block_t * PacketizeDrain( void *p_private );

static block_t *ParseNALBlock( decoder_t *, bool *pb_ts_used, block_t * );

static block_t *OutputPicture( decoder_t *p_dec );
static void PutSPS( decoder_t *p_dec, block_t *p_frag );
static void PutPPS( decoder_t *p_dec, block_t *p_frag );
static bool ParseSliceHeader( decoder_t *p_dec, const block_t *p_frag, h264_slice_t *p_slice );
static bool ParseSeiCallback( const hxxx_sei_data_t *, void * );


static const uint8_t p_h264_startcode[3] = { 0x00, 0x00, 0x01 };

/*****************************************************************************
 * Helpers
 *****************************************************************************/

static void StoreSPS( decoder_sys_t *p_sys, uint8_t i_id,
                      block_t *p_block, h264_sequence_parameter_set_t *p_sps )
{
    if( p_sys->sps[i_id].p_block )
        block_Release( p_sys->sps[i_id].p_block );
    if( p_sys->sps[i_id].p_sps )
        h264_release_sps( p_sys->sps[i_id].p_sps );
    if( p_sys->sps[i_id].p_sps == p_sys->p_active_sps )
        p_sys->p_active_sps = NULL;
    p_sys->sps[i_id].p_block = p_block;
    p_sys->sps[i_id].p_sps = p_sps;
}

static void StorePPS( decoder_sys_t *p_sys, uint8_t i_id,
                      block_t *p_block, h264_picture_parameter_set_t *p_pps )
{
    if( p_sys->pps[i_id].p_block )
        block_Release( p_sys->pps[i_id].p_block );
    if( p_sys->pps[i_id].p_pps )
        h264_release_pps( p_sys->pps[i_id].p_pps );
    if( p_sys->pps[i_id].p_pps == p_sys->p_active_pps )
        p_sys->p_active_pps = NULL;
    p_sys->pps[i_id].p_block = p_block;
    p_sys->pps[i_id].p_pps = p_pps;
}

static void ActivateSets( decoder_t *p_dec, const h264_sequence_parameter_set_t *p_sps,
                                            const h264_picture_parameter_set_t *p_pps )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    p_sys->p_active_pps = p_pps;
    p_sys->p_active_sps = p_sps;

    if( p_sps )
    {
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

        if( !p_dec->fmt_out.video.i_frame_rate ||
            !p_dec->fmt_out.video.i_frame_rate_base )
        {
            /* on first run == if fmt_in does not provide frame rate info */
            /* If we have frame rate info in the stream */
            if(p_sps->vui.b_valid &&
               p_sps->vui.i_num_units_in_tick > 0 &&
               p_sps->vui.i_time_scale > 1 )
            {
                date_Change( &p_sys->dts, p_sps->vui.i_time_scale,
                                          p_sps->vui.i_num_units_in_tick );
            }
            /* else use the default num/den */
            p_dec->fmt_out.video.i_frame_rate = p_sys->dts.i_divider_num >> 1; /* num_clock_ts == 2 */
            p_dec->fmt_out.video.i_frame_rate_base = p_sys->dts.i_divider_den;
        }

        if( p_dec->fmt_in.video.primaries == COLOR_PRIMARIES_UNDEF &&
            p_sps->vui.b_valid )
        {
            h264_get_colorimetry( p_sps, &p_dec->fmt_out.video.primaries,
                                  &p_dec->fmt_out.video.transfer,
                                  &p_dec->fmt_out.video.space,
                                  &p_dec->fmt_out.video.color_range );
        }

        if( p_dec->fmt_out.i_extra == 0 && p_pps )
        {
            const block_t *p_spsblock = NULL;
            const block_t *p_ppsblock = NULL;
            for( size_t i=0; i<=H264_SPS_ID_MAX && !p_spsblock; i++ )
                if( p_sps == p_sys->sps[i].p_sps )
                    p_spsblock = p_sys->sps[i].p_block;

            for( size_t i=0; i<=H264_PPS_ID_MAX && !p_ppsblock; i++ )
                if( p_pps == p_sys->pps[i].p_pps )
                    p_ppsblock = p_sys->pps[i].p_block;

            if( p_spsblock && p_ppsblock )
            {
                size_t i_alloc = p_ppsblock->i_buffer + p_spsblock->i_buffer;
                p_dec->fmt_out.p_extra = malloc( i_alloc );
                if( p_dec->fmt_out.p_extra )
                {
                    uint8_t*p_buf = p_dec->fmt_out.p_extra;
                    p_dec->fmt_out.i_extra = i_alloc;
                    memcpy( &p_buf[0], p_spsblock->p_buffer, p_spsblock->i_buffer );
                    memcpy( &p_buf[p_spsblock->i_buffer], p_ppsblock->p_buffer,
                            p_ppsblock->i_buffer );
                }
            }
        }
    }
}

static bool IsFirstVCLNALUnit( const h264_slice_t *p_prev, const h264_slice_t *p_cur )
{
    /* Detection of the first VCL NAL unit of a primary coded picture
     * (cf. 7.4.1.2.4) */
    if( p_cur->i_frame_num != p_prev->i_frame_num ||
        p_cur->i_pic_parameter_set_id != p_prev->i_pic_parameter_set_id ||
        p_cur->i_field_pic_flag != p_prev->i_field_pic_flag ||
       !p_cur->i_nal_ref_idc != !p_prev->i_nal_ref_idc )
        return true;
    if( (p_cur->i_bottom_field_flag != -1) &&
        (p_prev->i_bottom_field_flag != -1) &&
        (p_cur->i_bottom_field_flag != p_prev->i_bottom_field_flag) )
        return true;
    if( p_cur->i_pic_order_cnt_type == 0 &&
       ( p_cur->i_pic_order_cnt_lsb != p_prev->i_pic_order_cnt_lsb ||
         p_cur->i_delta_pic_order_cnt_bottom != p_prev->i_delta_pic_order_cnt_bottom ) )
        return true;
    else if( p_cur->i_pic_order_cnt_type == 1 &&
           ( p_cur->i_delta_pic_order_cnt0 != p_prev->i_delta_pic_order_cnt0 ||
             p_cur->i_delta_pic_order_cnt1 != p_prev->i_delta_pic_order_cnt1 ) )
        return true;
    if( ( p_cur->i_nal_type == H264_NAL_SLICE_IDR || p_prev->i_nal_type == H264_NAL_SLICE_IDR ) &&
        ( p_cur->i_nal_type != p_prev->i_nal_type || p_cur->i_idr_pic_id != p_prev->i_idr_pic_id ) )
        return true;

    return false;
}

static void DropStoredNAL( decoder_sys_t *p_sys )
{
    block_ChainRelease( p_sys->frame.p_head );
    block_ChainRelease( p_sys->leading.p_head );
    p_sys->frame.p_head = NULL;
    p_sys->frame.pp_append = &p_sys->frame.p_head;
    p_sys->leading.p_head = NULL;
    p_sys->leading.pp_append = &p_sys->leading.p_head;
}

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

    p_sys->p_ccs = cc_storage_new();
    if( unlikely(!p_sys->p_ccs) )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }

    packetizer_Init( &p_sys->packetizer,
                     p_h264_startcode, sizeof(p_h264_startcode), startcode_FindAnnexB,
                     p_h264_startcode, 1, 5,
                     PacketizeReset, PacketizeParse, PacketizeValidate, PacketizeDrain,
                     p_dec );

    p_sys->b_slice = false;
    p_sys->frame.p_head = NULL;
    p_sys->frame.pp_append = &p_sys->frame.p_head;
    p_sys->leading.p_head = NULL;
    p_sys->leading.pp_append = &p_sys->leading.p_head;
    p_sys->b_new_sps = false;
    p_sys->b_new_pps = false;

    for( i = 0; i <= H264_SPS_ID_MAX; i++ )
    {
        p_sys->sps[i].p_sps = NULL;
        p_sys->sps[i].p_block = NULL;
    }
    p_sys->p_active_sps = NULL;
    for( i = 0; i <= H264_PPS_ID_MAX; i++ )
    {
        p_sys->pps[i].p_pps = NULL;
        p_sys->pps[i].p_block = NULL;
    }
    p_sys->p_active_pps = NULL;
    p_sys->i_recovery_frame_cnt = UINT_MAX;

    h264_slice_init( &p_sys->slice );

    p_sys->i_next_block_flags = 0;
    p_sys->b_recovered = false;
    p_sys->i_recoveryfnum = UINT_MAX;
    p_sys->i_frame_dts = VLC_TICK_INVALID;
    p_sys->i_frame_pts = VLC_TICK_INVALID;
    p_sys->i_dpb_output_delay = 0;

    /* POC */
    h264_poc_context_init( &p_sys->pocctx );
    p_sys->prevdatedpoc.pts = VLC_TICK_INVALID;

    date_Init( &p_sys->dts, 30000 * 2, 1001 );

    /* Setup properties */
    es_format_Copy( &p_dec->fmt_out, &p_dec->fmt_in );
    p_dec->fmt_out.i_codec = VLC_CODEC_H264;
    p_dec->fmt_out.b_packetized = true;

    if( p_dec->fmt_in.video.i_frame_rate_base &&
        p_dec->fmt_in.video.i_frame_rate &&
        p_dec->fmt_in.video.i_frame_rate <= UINT_MAX / 2 )
    {
        date_Change( &p_sys->dts, p_dec->fmt_in.video.i_frame_rate * 2,
                                  p_dec->fmt_in.video.i_frame_rate_base );
    }

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
            p_sys->b_recovered = !!p_dec->fmt_out.i_extra;

            if(!p_dec->fmt_out.p_extra)
            {
                msg_Err( p_dec, "Invalid AVC extradata");
                Close( p_this );
                return VLC_EGENERIC;
            }
        }
        else
        {
            msg_Err( p_dec, "Invalid or missing AVC extradata");
            Close( p_this );
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
        /* FIXME: that's not correct for every AVC */
        if( !p_sys->b_new_pps || !p_sys->b_new_sps )
        {
            msg_Err( p_dec, "Invalid or missing SPS %d or PPS %d in AVC extradata",
                     p_sys->b_new_sps, p_sys->b_new_pps );
            Close( p_this );
            return VLC_EGENERIC;
        }

        msg_Dbg( p_dec, "Packetizer fed with AVC, nal length size=%d",
                         p_sys->i_avcC_length_size );
    }

    /* CC are the same for H264/AVC in T35 sections (ETSI TS 101 154)  */
    p_dec->pf_get_cc = GetCc;
    p_dec->pf_flush = PacketizeFlush;

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

    DropStoredNAL( p_sys );
    for( i = 0; i <= H264_SPS_ID_MAX; i++ )
        StoreSPS( p_sys, i, NULL, NULL );
    for( i = 0; i <= H264_PPS_ID_MAX; i++ )
        StorePPS( p_sys, i, NULL, NULL );

    packetizer_Clean( &p_sys->packetizer );

    cc_storage_delete( p_sys->p_ccs );

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
static block_t *GetCc( decoder_t *p_dec, decoder_cc_desc_t *p_desc )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    return cc_storage_get_current( p_sys->p_ccs, p_desc );
}

/****************************************************************************
 * Helpers
 ****************************************************************************/
static void ResetOutputVariables( decoder_sys_t *p_sys )
{
    p_sys->i_frame_dts = VLC_TICK_INVALID;
    p_sys->i_frame_pts = VLC_TICK_INVALID;
    p_sys->slice.type = H264_SLICE_TYPE_UNKNOWN;
    p_sys->b_new_sps = false;
    p_sys->b_new_pps = false;
    p_sys->b_slice = false;
    /* From SEI */
    p_sys->i_dpb_output_delay = 0;
    p_sys->i_pic_struct = UINT8_MAX;
    p_sys->i_recovery_frame_cnt = UINT_MAX;
}

static void PacketizeReset( void *p_private, bool b_flush )
{
    decoder_t *p_dec = p_private;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( b_flush || !p_sys->b_slice )
    {
        DropStoredNAL( p_sys );
        ResetOutputVariables( p_sys );
        p_sys->p_active_pps = NULL;
        p_sys->p_active_sps = NULL;
        p_sys->b_recovered = false;
        p_sys->i_recoveryfnum = UINT_MAX;
        /* POC */
        h264_poc_context_init( &p_sys->pocctx );
        p_sys->prevdatedpoc.pts = VLC_TICK_INVALID;
    }
    p_sys->i_next_block_flags = BLOCK_FLAG_DISCONTINUITY;
    date_Set( &p_sys->dts, VLC_TICK_INVALID );
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

static block_t * PacketizeDrain( void *p_private )
{
    decoder_t *p_dec = p_private;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( !p_sys->b_slice )
        return NULL;

    block_t *p_out = OutputPicture( p_dec );
    if( p_out && (p_out->i_flags & BLOCK_FLAG_DROP) )
    {
        block_Release( p_out );
        p_out = NULL;
    }

    return p_out;
}

/*****************************************************************************
 * ParseNALBlock: parses annexB type NALs
 * All p_frag blocks are required to start with 0 0 0 1 4-byte startcode
 *****************************************************************************/
static block_t *ParseNALBlock( decoder_t *p_dec, bool *pb_ts_used, block_t *p_frag )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_pic = NULL;

    const int i_nal_type = p_frag->p_buffer[4]&0x1f;
    const vlc_tick_t i_frag_dts = p_frag->i_dts;
    const vlc_tick_t i_frag_pts = p_frag->i_pts;
    bool b_au_end = p_frag->i_flags & BLOCK_FLAG_AU_END;
    p_frag->i_flags &= ~BLOCK_FLAG_AU_END;

    if( p_sys->b_slice && (!p_sys->p_active_pps || !p_sys->p_active_sps) )
    {
        msg_Warn( p_dec, "waiting for SPS/PPS" );

        /* Reset context */
        DropStoredNAL( p_sys );
        ResetOutputVariables( p_sys );
        cc_storage_reset( p_sys->p_ccs );
    }

    switch( i_nal_type )
    {
        /*** Slices ***/
        case H264_NAL_SLICE:
        case H264_NAL_SLICE_DPA:
        case H264_NAL_SLICE_DPB:
        case H264_NAL_SLICE_DPC:
        case H264_NAL_SLICE_IDR:
        {
            h264_slice_t newslice;

            if( i_nal_type == H264_NAL_SLICE_IDR )
            {
                p_sys->b_recovered = true;
                p_sys->i_recovery_frame_cnt = UINT_MAX;
                p_sys->i_recoveryfnum = UINT_MAX;
            }

            if( ParseSliceHeader( p_dec, p_frag, &newslice ) )
            {
                /* Only IDR carries the id, to be propagated */
                if( newslice.i_idr_pic_id == -1 )
                    newslice.i_idr_pic_id = p_sys->slice.i_idr_pic_id;

                bool b_new_picture = IsFirstVCLNALUnit( &p_sys->slice, &newslice );
                if( b_new_picture )
                {
                    /* Parse SEI for that frame now we should have matched SPS/PPS */
                    for( block_t *p_sei = p_sys->leading.p_head; p_sei; p_sei = p_sei->p_next )
                    {
                        if( (p_sei->i_flags & BLOCK_FLAG_PRIVATE_SEI) == 0 )
                            continue;
                        HxxxParse_AnnexB_SEI( p_sei->p_buffer, p_sei->i_buffer,
                                              1 /* nal header */, ParseSeiCallback, p_dec );
                    }

                    if( p_sys->b_slice )
                        p_pic = OutputPicture( p_dec );
                }

                /* */
                p_sys->slice = newslice;
            }
            else
            {
                p_sys->p_active_pps = NULL;
                /* Fragment will be discarded later on */
            }
            p_sys->b_slice = true;

            block_ChainLastAppend( &p_sys->frame.pp_append, p_frag );
        } break;

        /*** Prefix NALs ***/

        case H264_NAL_AU_DELIMITER:
            if( p_sys->b_slice )
                p_pic = OutputPicture( p_dec );

            /* clear junk if no pic, we're always the first nal */
            DropStoredNAL( p_sys );

            p_frag->i_flags |= BLOCK_FLAG_PRIVATE_AUD;

            block_ChainLastAppend( &p_sys->leading.pp_append, p_frag );
        break;

        case H264_NAL_SPS:
        case H264_NAL_PPS:
            if( p_sys->b_slice )
                p_pic = OutputPicture( p_dec );

            /* Stored for insert on keyframes */
            if( i_nal_type == H264_NAL_SPS )
            {
                PutSPS( p_dec, p_frag );
                p_sys->b_new_sps = true;
            }
            else
            {
                PutPPS( p_dec, p_frag );
                p_sys->b_new_pps = true;
            }
        break;

        case H264_NAL_SEI:
            if( p_sys->b_slice )
                p_pic = OutputPicture( p_dec );

            p_frag->i_flags |= BLOCK_FLAG_PRIVATE_SEI;
            block_ChainLastAppend( &p_sys->leading.pp_append, p_frag );
        break;

        case H264_NAL_SPS_EXT:
        case H264_NAL_PREFIX: /* first slice/VCL associated data */
        case H264_NAL_SUBSET_SPS:
        case H264_NAL_DEPTH_PS:
        case H264_NAL_RESERVED_17:
        case H264_NAL_RESERVED_18:
            if( p_sys->b_slice )
                p_pic = OutputPicture( p_dec );

            block_ChainLastAppend( &p_sys->leading.pp_append, p_frag );
        break;

        /*** Suffix NALs ***/

        case H264_NAL_END_OF_SEQ:
        case H264_NAL_END_OF_STREAM:
            /* Early end of packetization */
            block_ChainLastAppend( &p_sys->frame.pp_append, p_frag );

            /* important for still pictures/menus */
            p_sys->i_next_block_flags |= BLOCK_FLAG_END_OF_SEQUENCE;
            if( p_sys->b_slice )
                p_pic = OutputPicture( p_dec );
        break;

        case H264_NAL_SLICE_WP: // post
        case H264_NAL_UNKNOWN:
        case H264_NAL_FILLER_DATA:
        case H264_NAL_SLICE_EXT:
        case H264_NAL_SLICE_3D_EXT:
        case H264_NAL_RESERVED_22:
        case H264_NAL_RESERVED_23:
        default: /* others 24..31, including unknown */
            block_ChainLastAppend( &p_sys->frame.pp_append, p_frag );
        break;
    }

    *pb_ts_used = false;
    if( p_sys->i_frame_dts == VLC_TICK_INVALID &&
        p_sys->i_frame_pts == VLC_TICK_INVALID )
    {
        p_sys->i_frame_dts = i_frag_dts;
        p_sys->i_frame_pts = i_frag_pts;
        *pb_ts_used = true;
        if( i_frag_dts != VLC_TICK_INVALID )
            date_Set( &p_sys->dts, i_frag_dts );
    }

    if( p_sys->b_slice && b_au_end && !p_pic )
    {
        p_pic = OutputPicture( p_dec );
    }

    if( p_pic && (p_pic->i_flags & BLOCK_FLAG_DROP) )
    {
        block_Release( p_pic );
        p_pic = NULL;
    }

    return p_pic;
}

static bool CanSwapPTSwithDTS( const h264_slice_t *p_slice,
                               const h264_sequence_parameter_set_t *p_sps )
{
    if( p_slice->i_nal_ref_idc == 0 && p_slice->type == H264_SLICE_TYPE_B )
        return true;
    else if( p_sps->vui.b_valid )
        return p_sps->vui.i_max_num_reorder_frames == 0;
    else
        return p_sps->i_profile == PROFILE_H264_CAVLC_INTRA;
}

static block_t *OutputPicture( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_pic = NULL;
    block_t **pp_pic_last = &p_pic;

    if( unlikely(!p_sys->frame.p_head) )
    {
        assert( p_sys->frame.p_head );
        DropStoredNAL( p_sys );
        ResetOutputVariables( p_sys );
        cc_storage_reset( p_sys->p_ccs );
        return NULL;
    }

    /* Bind matched/referred PPS and SPS */
    const h264_picture_parameter_set_t *p_pps = p_sys->p_active_pps;
    const h264_sequence_parameter_set_t *p_sps = p_sys->p_active_sps;
    if( !p_pps || !p_sps )
    {
        DropStoredNAL( p_sys );
        ResetOutputVariables( p_sys );
        cc_storage_reset( p_sys->p_ccs );
        return NULL;
    }

    if( !p_sys->b_recovered && p_sys->i_recoveryfnum == UINT_MAX &&
         p_sys->i_recovery_frame_cnt == UINT_MAX && p_sys->slice.type == H264_SLICE_TYPE_I )
    {
        /* No way to recover using SEI, just sync on I Slice */
        p_sys->b_recovered = true;
    }

    bool b_need_sps_pps = p_sys->slice.type == H264_SLICE_TYPE_I &&
                          p_sys->p_active_pps && p_sys->p_active_sps;

    /* Handle SEI recovery */
    if ( !p_sys->b_recovered && p_sys->i_recovery_frame_cnt != UINT_MAX &&
         p_sys->i_recoveryfnum == UINT_MAX )
    {
        p_sys->i_recoveryfnum = p_sys->slice.i_frame_num + p_sys->i_recovery_frame_cnt;
        p_sys->i_recoverystartfnum = p_sys->slice.i_frame_num;
        b_need_sps_pps = true; /* SPS/PPS must be inserted for SEI recovery */
        msg_Dbg( p_dec, "Recovering using SEI, prerolling %u reference pics", p_sys->i_recovery_frame_cnt );
    }

    if( p_sys->i_recoveryfnum != UINT_MAX )
    {
        assert(p_sys->b_recovered == false);
        const unsigned maxFrameNum = 1 << (p_sps->i_log2_max_frame_num + 4);

        if( ( p_sys->i_recoveryfnum > maxFrameNum &&
              p_sys->slice.i_frame_num < p_sys->i_recoverystartfnum &&
              p_sys->slice.i_frame_num >= p_sys->i_recoveryfnum % maxFrameNum ) ||
            ( p_sys->i_recoveryfnum <= maxFrameNum &&
              p_sys->slice.i_frame_num >= p_sys->i_recoveryfnum ) )
        {
            p_sys->i_recoveryfnum = UINT_MAX;
            p_sys->b_recovered = true;
            msg_Dbg( p_dec, "Recovery from SEI recovery point complete" );
        }
    }

    /* Gather PPS/SPS if required */
    block_t *p_xpsnal = NULL;
    block_t **pp_xpsnal_tail = &p_xpsnal;
    if( b_need_sps_pps || p_sys->b_new_sps || p_sys->b_new_pps )
    {
        for( int i = 0; i <= H264_SPS_ID_MAX && (b_need_sps_pps || p_sys->b_new_sps); i++ )
        {
            if( p_sys->sps[i].p_block )
                block_ChainLastAppend( &pp_xpsnal_tail, block_Duplicate( p_sys->sps[i].p_block ) );
        }
        for( int i = 0; i < H264_PPS_ID_MAX && (b_need_sps_pps || p_sys->b_new_pps); i++ )
        {
            if( p_sys->pps[i].p_block )
                block_ChainLastAppend( &pp_xpsnal_tail, block_Duplicate( p_sys->pps[i].p_block ) );
        }
    }

    /* Now rebuild NAL Sequence, inserting PPS/SPS if any */
    if( p_sys->leading.p_head &&
       (p_sys->leading.p_head->i_flags & BLOCK_FLAG_PRIVATE_AUD) )
    {
        block_t *p_au = p_sys->leading.p_head;
        p_sys->leading.p_head = p_au->p_next;
        p_au->p_next = NULL;
        block_ChainLastAppend( &pp_pic_last, p_au );
    }

    if( p_xpsnal )
        block_ChainLastAppend( &pp_pic_last, p_xpsnal );

    if( p_sys->leading.p_head )
        block_ChainLastAppend( &pp_pic_last, p_sys->leading.p_head );

    assert( p_sys->frame.p_head );
    if( p_sys->frame.p_head )
        block_ChainLastAppend( &pp_pic_last, p_sys->frame.p_head );

    /* Reset chains, now empty */
    p_sys->frame.p_head = NULL;
    p_sys->frame.pp_append = &p_sys->frame.p_head;
    p_sys->leading.p_head = NULL;
    p_sys->leading.pp_append = &p_sys->leading.p_head;

    p_pic = block_ChainGather( p_pic );

    if( !p_pic )
    {
        ResetOutputVariables( p_sys );
        cc_storage_reset( p_sys->p_ccs );
        return NULL;
    }

    /* clear up flags gathered */
    p_pic->i_flags &= ~BLOCK_FLAG_PRIVATE_MASK;

    /* for PTS Fixup, interlaced fields (multiple AU/block) */
    int tFOC = 0, bFOC = 0, PictureOrderCount = 0;
    h264_compute_poc( p_sps, &p_sys->slice, &p_sys->pocctx, &PictureOrderCount, &tFOC, &bFOC );

    unsigned i_num_clock_ts = h264_get_num_ts( p_sps, &p_sys->slice, p_sys->i_pic_struct, tFOC, bFOC );

    if( p_sps->frame_mbs_only_flag == 0 && p_sps->vui.b_pic_struct_present_flag )
    {
        switch( p_sys->i_pic_struct )
        {
        /* Top and Bottom field slices */
        case 1:
        case 2:
            p_pic->i_flags |= BLOCK_FLAG_SINGLE_FIELD;
            p_pic->i_flags |= (!p_sys->slice.i_bottom_field_flag) ? BLOCK_FLAG_TOP_FIELD_FIRST
                                                                  : BLOCK_FLAG_BOTTOM_FIELD_FIRST;
            break;
        /* Each of the following slices contains multiple fields */
        case 3:
            p_pic->i_flags |= BLOCK_FLAG_TOP_FIELD_FIRST;
            break;
        case 4:
            p_pic->i_flags |= BLOCK_FLAG_BOTTOM_FIELD_FIRST;
            break;
        case 5:
            p_pic->i_flags |= BLOCK_FLAG_TOP_FIELD_FIRST;
            break;
        case 6:
            p_pic->i_flags |= BLOCK_FLAG_BOTTOM_FIELD_FIRST;
            break;
        default:
            break;
        }
    }

    /* set dts/pts to current block timestamps */
    p_pic->i_dts = p_sys->i_frame_dts;
    p_pic->i_pts = p_sys->i_frame_pts;

    /* Fixup missing timestamps after split (multiple AU/block)*/
    if( p_pic->i_dts == VLC_TICK_INVALID )
        p_pic->i_dts = date_Get( &p_sys->dts );

    if( p_sys->slice.type == H264_SLICE_TYPE_I )
        p_sys->prevdatedpoc.pts = VLC_TICK_INVALID;

    if( p_pic->i_pts == VLC_TICK_INVALID )
    {
        if( p_sys->prevdatedpoc.pts != VLC_TICK_INVALID &&
            date_Get( &p_sys->dts ) != VLC_TICK_INVALID )
        {
            date_t pts = p_sys->dts;
            date_Set( &pts, p_sys->prevdatedpoc.pts );

            int diff = tFOC - p_sys->prevdatedpoc.num;
            if( diff > 0 )
                date_Increment( &pts, diff );
            else
                date_Decrement( &pts, -diff );

            p_pic->i_pts = date_Get( &pts );
            /* non monotonically increasing dts on some videos 33333 33333...35000 */
            if( p_pic->i_pts < p_pic->i_dts )
                p_pic->i_pts = p_pic->i_dts;
        }
        /* In case there's no PTS at all */
        else if( CanSwapPTSwithDTS( &p_sys->slice, p_sps ) )
        {
            p_pic->i_pts = p_pic->i_dts;
        }
        else if( p_sys->slice.type == H264_SLICE_TYPE_I &&
                 date_Get( &p_sys->dts ) != VLC_TICK_INVALID )
        {
            /* Hell no PTS on IDR. We're totally blind */
            date_t pts = p_sys->dts;
            date_Increment( &pts, 2 );
            p_pic->i_pts = date_Get( &pts );
        }
    }
    else if( p_pic->i_dts == VLC_TICK_INVALID &&
             CanSwapPTSwithDTS( &p_sys->slice, p_sps ) )
    {
        p_pic->i_dts = p_pic->i_pts;
        if( date_Get( &p_sys->dts ) == VLC_TICK_INVALID )
            date_Set( &p_sys->dts, p_pic->i_pts );
    }

    if( p_pic->i_pts != VLC_TICK_INVALID )
    {
        p_sys->prevdatedpoc.pts = p_pic->i_pts;
        p_sys->prevdatedpoc.num = PictureOrderCount;
    }

    if( p_pic->i_length == 0 )
    {
        date_t next = p_sys->dts;
        date_Increment( &next, i_num_clock_ts );
        p_pic->i_length = date_Get( &next ) - date_Get( &p_sys->dts );
    }

#if 0
    msg_Err(p_dec, "F/BOC %d/%d POC %d %d rec %d flags %x ref%d fn %d fp %d %d pts %ld len %ld",
                    tFOC, bFOC, PictureOrderCount,
                    p_sys->slice.type, p_sys->b_recovered, p_pic->i_flags,
                    p_sys->slice.i_nal_ref_idc, p_sys->slice.i_frame_num, p_sys->slice.i_field_pic_flag,
                    p_pic->i_pts - p_pic->i_dts, p_pic->i_pts % VLC_TICK_FROM_SEC(100), p_pic->i_length);
#endif

    /* save for next pic fixups */
    if( date_Get( &p_sys->dts ) != VLC_TICK_INVALID )
    {
        if( p_sys->i_next_block_flags & BLOCK_FLAG_DISCONTINUITY )
            date_Set( &p_sys->dts, VLC_TICK_INVALID );
        else
            date_Increment( &p_sys->dts, i_num_clock_ts );
    }

    if( p_pic )
    {
        p_pic->i_flags |= p_sys->i_next_block_flags;
        p_sys->i_next_block_flags = 0;
    }

    switch( p_sys->slice.type )
    {
        case H264_SLICE_TYPE_P:
            p_pic->i_flags |= BLOCK_FLAG_TYPE_P;
            break;
        case H264_SLICE_TYPE_B:
            p_pic->i_flags |= BLOCK_FLAG_TYPE_B;
            break;
        case H264_SLICE_TYPE_I:
            p_pic->i_flags |= BLOCK_FLAG_TYPE_I;
        default:
            break;
    }

    if( !p_sys->b_recovered )
    {
        if( p_sys->i_recoveryfnum != UINT_MAX ) /* recovering from SEI */
            p_pic->i_flags |= BLOCK_FLAG_PREROLL;
        else
            p_pic->i_flags |= BLOCK_FLAG_DROP;
    }

    p_pic->i_flags &= ~BLOCK_FLAG_PRIVATE_AUD;

    /* reset after output */
    ResetOutputVariables( p_sys );

    /* CC */
    cc_storage_commit( p_sys->p_ccs, p_pic );

    return p_pic;
}

static void PutSPS( decoder_t *p_dec, block_t *p_frag )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    const uint8_t *p_buffer = p_frag->p_buffer;
    size_t i_buffer = p_frag->i_buffer;

    if( !hxxx_strip_AnnexB_startcode( &p_buffer, &i_buffer ) )
    {
        block_Release( p_frag );
        return;
    }

    h264_sequence_parameter_set_t *p_sps = h264_decode_sps( p_buffer, i_buffer, true );
    if( !p_sps )
    {
        msg_Warn( p_dec, "invalid SPS" );
        block_Release( p_frag );
        return;
    }

    /* We have a new SPS */
    if( !p_sys->sps[p_sps->i_id].p_sps )
        msg_Dbg( p_dec, "found NAL_SPS (sps_id=%d)", p_sps->i_id );

    StoreSPS( p_sys, p_sps->i_id, p_frag, p_sps );
}

static void PutPPS( decoder_t *p_dec, block_t *p_frag )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    const uint8_t *p_buffer = p_frag->p_buffer;
    size_t i_buffer = p_frag->i_buffer;

    if( !hxxx_strip_AnnexB_startcode( &p_buffer, &i_buffer ) )
    {
        block_Release( p_frag );
        return;
    }

    h264_picture_parameter_set_t *p_pps = h264_decode_pps( p_buffer, i_buffer, true );
    if( !p_pps )
    {
        msg_Warn( p_dec, "invalid PPS" );
        block_Release( p_frag );
        return;
    }

    /* We have a new PPS */
    if( !p_sys->pps[p_pps->i_id].p_pps )
        msg_Dbg( p_dec, "found NAL_PPS (pps_id=%d sps_id=%d)", p_pps->i_id, p_pps->i_sps_id );

    StorePPS( p_sys, p_pps->i_id, p_frag, p_pps );
}

static void GetSPSPPS( uint8_t i_pps_id, void *priv,
                       const h264_sequence_parameter_set_t **pp_sps,
                       const h264_picture_parameter_set_t **pp_pps )
{
    decoder_sys_t *p_sys = priv;

    *pp_pps = p_sys->pps[i_pps_id].p_pps;
    if( *pp_pps == NULL )
        *pp_sps = NULL;
    else
        *pp_sps = p_sys->sps[(*pp_pps)->i_sps_id].p_sps;
}

static bool ParseSliceHeader( decoder_t *p_dec, const block_t *p_frag, h264_slice_t *p_slice )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    const uint8_t *p_stripped = p_frag->p_buffer;
    size_t i_stripped = p_frag->i_buffer;

    if( !hxxx_strip_AnnexB_startcode( &p_stripped, &i_stripped ) || i_stripped < 2 )
        return false;

    if( !h264_decode_slice( p_stripped, i_stripped, GetSPSPPS, p_sys, p_slice ) )
        return false;

    const h264_sequence_parameter_set_t *p_sps;
    const h264_picture_parameter_set_t *p_pps;
    GetSPSPPS( p_slice->i_pic_parameter_set_id, p_sys, &p_sps, &p_pps );
    if( unlikely( !p_sps || !p_pps) )
        return false;

    ActivateSets( p_dec, p_sps, p_pps );

    return true;
}

static bool ParseSeiCallback( const hxxx_sei_data_t *p_sei_data, void *cbdata )
{
    decoder_t *p_dec = (decoder_t *) cbdata;
    decoder_sys_t *p_sys = p_dec->p_sys;

    switch( p_sei_data->i_type )
    {
        /* Look for pic timing */
        case HXXX_SEI_PIC_TIMING:
        {
            const h264_sequence_parameter_set_t *p_sps = p_sys->p_active_sps;
            if( unlikely( p_sps == NULL ) )
            {
                assert( p_sps );
                break;
            }

            if( p_sps->vui.b_valid )
            {
                if( p_sps->vui.b_hrd_parameters_present_flag )
                {
                    bs_read( p_sei_data->p_bs, p_sps->vui.i_cpb_removal_delay_length_minus1 + 1 );
                    p_sys->i_dpb_output_delay =
                            bs_read( p_sei_data->p_bs, p_sps->vui.i_dpb_output_delay_length_minus1 + 1 );
                }

                if( p_sps->vui.b_pic_struct_present_flag )
                    p_sys->i_pic_struct = bs_read( p_sei_data->p_bs, 4 );
                /* + unparsed remains */
            }
        } break;

            /* Look for user_data_registered_itu_t_t35 */
        case HXXX_SEI_USER_DATA_REGISTERED_ITU_T_T35:
        {
            if( p_sei_data->itu_t35.type == HXXX_ITU_T35_TYPE_CC )
            {
                cc_storage_append( p_sys->p_ccs, true, p_sei_data->itu_t35.u.cc.p_data,
                                                       p_sei_data->itu_t35.u.cc.i_data );
            }
        } break;

        case HXXX_SEI_FRAME_PACKING_ARRANGEMENT:
        {
            if( p_dec->fmt_in.video.multiview_mode == MULTIVIEW_2D )
            {
                video_multiview_mode_t mode;
                switch( p_sei_data->frame_packing.type )
                {
                    case FRAME_PACKING_INTERLEAVED_CHECKERBOARD:
                        mode = MULTIVIEW_STEREO_CHECKERBOARD; break;
                    case FRAME_PACKING_INTERLEAVED_COLUMN:
                        mode = MULTIVIEW_STEREO_COL; break;
                    case FRAME_PACKING_INTERLEAVED_ROW:
                        mode = MULTIVIEW_STEREO_ROW; break;
                    case FRAME_PACKING_SIDE_BY_SIDE:
                        mode = MULTIVIEW_STEREO_SBS; break;
                    case FRAME_PACKING_TOP_BOTTOM:
                        mode = MULTIVIEW_STEREO_TB; break;
                    case FRAME_PACKING_TEMPORAL:
                        mode = MULTIVIEW_STEREO_FRAME; break;
                    case FRAME_PACKING_TILED:
                    default:
                        mode = MULTIVIEW_2D; break;
                }
                p_dec->fmt_out.video.multiview_mode = mode;
            }
        } break;

            /* Look for SEI recovery point */
        case HXXX_SEI_RECOVERY_POINT:
        {
            if( !p_sys->b_recovered )
                msg_Dbg( p_dec, "Seen SEI recovery point, %d recovery frames", p_sei_data->recovery.i_frames );
            p_sys->i_recovery_frame_cnt = p_sei_data->recovery.i_frames;
        } break;

        default:
            /* Will skip */
            break;
    }

    return true;
}

