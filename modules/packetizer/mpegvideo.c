/*****************************************************************************
 * mpegvideo.c: parse and packetize an MPEG1/2 video stream
 *****************************************************************************
 * Copyright (C) 2001-2006 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
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
 * Problem with this implementation:
 *
 * Although we should time-stamp each picture with a PTS, this isn't possible
 * with the current implementation.
 * The problem comes from the fact that for non-low-delay streams we can't
 * calculate the PTS of pictures used as backward reference. Even the temporal
 * reference number doesn't help here because all the pictures don't
 * necessarily have the same duration (eg. 3:2 pulldown).
 *
 * However this doesn't really matter as far as the MPEG muxers are concerned
 * because they allow having empty PTS fields. --gibalou
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_block.h>
#include <vlc_codec.h>
#include <vlc_block_helper.h>
#include "mpegvideo.h"
#include "../codec/cc.h"
#include "packetizer_helper.h"
#include "startcode_helper.h"
#include "iso_color_tables.h"

#include <limits.h>

#define SYNC_INTRAFRAME_TEXT N_("Sync on Intra Frame")
#define SYNC_INTRAFRAME_LONGTEXT N_("Normally the packetizer would " \
    "sync on the next full frame. This flags instructs the packetizer " \
    "to sync on the first Intra Frame found.")

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_PACKETIZER )
    set_description( N_("MPEG-I/II video packetizer") )
    set_shortname( N_("MPEG Video") )
    set_capability( "packetizer", 50 )
    set_callbacks( Open, Close )

    add_bool( "packetizer-mpegvideo-sync-iframe", false, SYNC_INTRAFRAME_TEXT,
              SYNC_INTRAFRAME_LONGTEXT, true )
vlc_module_end ()

enum mpeg_startcode_e
{
    PICTURE_STARTCODE          = 0x00,
    SLICE_STARTCODE_FIRST      = 0x01,
    SLICE_STARTCODE_LAST       = 0xAF,
    USER_DATA_STARTCODE        = 0xB2,
    SEQUENCE_HEADER_STARTCODE  = 0xB3,
    SEQUENCE_ERROR_STARTCODE   = 0xB4,
    EXTENSION_STARTCODE        = 0xB5,
    SEQUENCE_END_STARTCODE     = 0xB7,
    GROUP_STARTCODE            = 0xB8,
    SYSTEM_STARTCODE_FIRST     = 0xB9,
    SYSTEM_STARTCODE_LAST      = 0xFF,
};

enum extension_start_code_identifier_e
{
    SEQUENCE_EXTENSION_ID                   = 0x01,
    SEQUENCE_DISPLAY_EXTENSION_ID           = 0x02,
    QUANT_MATRIX_EXTENSION_ID               = 0x03,
    COPYRIGHT_EXTENSION_ID                  = 0x04,
    SEQUENCE_SCALABLE_EXTENSION_ID          = 0x05,
    PICTURE_DISPLAY_EXTENSION_ID            = 0x07,
    PICTURE_CODING_EXTENSION_ID             = 0x08,
    PICTURE_SPATIAL_SCALABLE_EXTENSION_ID   = 0x09,
    PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID  = 0x0A,
    CAMERA_PARAMETERS_EXTENSION_ID          = 0x0B,
    ITU_T_EXTENSION_ID                      = 0x0C,
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct
{
    /*
     * Input properties
     */
    packetizer_t packetizer;

    /* Sequence header and extension */
    block_t *p_seq;
    block_t *p_ext;

    /* Current frame being built */
    block_t    *p_frame;
    block_t    **pp_last;

    bool b_frame_slice;
    vlc_tick_t i_pts;
    vlc_tick_t i_dts;

    date_t  dts;
    date_t  prev_iframe_dts;

    /* Sequence properties */
    uint16_t i_h_size_value;
    uint16_t i_v_size_value;
    uint8_t  i_aspect_ratio_info;
    uint8_t  i_frame_rate_value;
    uint32_t i_bitratelower18;
    /* Extended Sequence properties (MPEG2) */
    uint8_t  i_h_size_ext;
    uint8_t  i_v_size_ext;
    uint16_t i_bitrateupper12;
    bool  b_seq_progressive;
    bool  b_low_delay;
    uint8_t i_frame_rate_ext_n;
    uint8_t i_frame_rate_ext_d;

    /* Picture properties */
    int i_temporal_ref;
    int i_prev_temporal_ref;
    int i_picture_type;
    int i_picture_structure;
    int i_top_field_first;
    int i_repeat_first_field;
    int i_progressive_frame;

    vlc_tick_t i_last_ref_pts;

    vlc_tick_t i_last_frame_pts;
    uint16_t i_last_frame_refid;

    bool b_second_field;

    /* Number of pictures since last sequence header */
    unsigned i_seq_old;

    /* Sync behaviour */
    bool  b_sync_on_intra_frame;
    bool  b_waiting_iframe;
    int   i_next_block_flags;

    /* */
    bool b_cc_reset;
    uint32_t i_cc_flags;
    vlc_tick_t i_cc_pts;
    vlc_tick_t i_cc_dts;
    cc_data_t cc;
} decoder_sys_t;

static block_t *Packetize( decoder_t *, block_t ** );
static void PacketizeFlush( decoder_t * );
static block_t *GetCc( decoder_t *p_dec, decoder_cc_desc_t * );

static void PacketizeReset( void *p_private, bool b_broken );
static block_t *PacketizeParse( void *p_private, bool *pb_ts_used, block_t * );
static int PacketizeValidate( void *p_private, block_t * );
static block_t * PacketizeDrain( void *p_private );

static block_t *ParseMPEGBlock( decoder_t *, block_t * );

static const uint8_t p_mp2v_startcode[3] = { 0x00, 0x00, 0x01 };

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_MPGV &&
        p_dec->fmt_in.i_codec != VLC_CODEC_MP2V )
        return VLC_EGENERIC;

    p_dec->p_sys = p_sys = malloc( sizeof( decoder_sys_t ) );
    if( !p_dec->p_sys )
        return VLC_ENOMEM;
    memset( p_dec->p_sys, 0, sizeof( decoder_sys_t ) );

    p_dec->fmt_out.i_codec = VLC_CODEC_MPGV;
    p_dec->fmt_out.i_original_fourcc = p_dec->fmt_in.i_original_fourcc;

    /* Misc init */
    packetizer_Init( &p_sys->packetizer,
                     p_mp2v_startcode, sizeof(p_mp2v_startcode), startcode_FindAnnexB,
                     NULL, 0, 4,
                     PacketizeReset, PacketizeParse, PacketizeValidate, PacketizeDrain,
                     p_dec );

    p_sys->p_seq = NULL;
    p_sys->p_ext = NULL;
    p_sys->p_frame = NULL;
    p_sys->pp_last = &p_sys->p_frame;
    p_sys->b_frame_slice = false;

    p_sys->i_dts =
    p_sys->i_pts = VLC_TICK_INVALID;

    unsigned num, den;
    if( p_dec->fmt_in.video.i_frame_rate && p_dec->fmt_in.video.i_frame_rate_base )
    {
        num = p_dec->fmt_in.video.i_frame_rate;
        den = p_dec->fmt_in.video.i_frame_rate_base;
    }
    else
    {
        num = 30000;
        den = 1001;
    }
    date_Init( &p_sys->dts, 2 * num, den ); /* fields / den */
    date_Init( &p_sys->prev_iframe_dts, 2 * num, den );

    p_sys->i_h_size_value = 0;
    p_sys->i_v_size_value = 0;
    p_sys->i_aspect_ratio_info = 0;
    p_sys->i_frame_rate_value = 0;
    p_sys->i_bitratelower18 = 0;
    p_sys->i_bitrateupper12 = 0;
    p_sys->i_h_size_ext = 0;
    p_sys->i_v_size_ext = 0;
    p_sys->b_seq_progressive = true;
    p_sys->b_low_delay = false;
    p_sys->i_frame_rate_ext_n = 0;
    p_sys->i_frame_rate_ext_d = 0;
    p_sys->i_seq_old = 0;

    p_sys->i_temporal_ref = 0;
    p_sys->i_prev_temporal_ref = 2048;
    p_sys->i_picture_type = 0;
    p_sys->i_picture_structure = 0x03; /* frame */
    p_sys->i_top_field_first = 0;
    p_sys->i_repeat_first_field = 0;
    p_sys->i_progressive_frame = 0;

    p_sys->i_last_ref_pts = VLC_TICK_INVALID;
    p_sys->b_second_field = 0;

    p_sys->i_next_block_flags = 0;

    p_sys->i_last_frame_refid = 0;

    p_sys->b_waiting_iframe =
    p_sys->b_sync_on_intra_frame = var_CreateGetBool( p_dec, "packetizer-mpegvideo-sync-iframe" );
    if( p_sys->b_sync_on_intra_frame )
        msg_Dbg( p_dec, "syncing on intra frame now" );

    p_sys->b_cc_reset = false;
    p_sys->i_cc_pts = 0;
    p_sys->i_cc_dts = 0;
    p_sys->i_cc_flags = 0;
    cc_Init( &p_sys->cc );

    p_dec->pf_packetize = Packetize;
    p_dec->pf_flush = PacketizeFlush;
    p_dec->pf_get_cc = GetCc;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_seq )
    {
        block_Release( p_sys->p_seq );
    }
    if( p_sys->p_ext )
    {
        block_Release( p_sys->p_ext );
    }
    if( p_sys->p_frame )
    {
        block_ChainRelease( p_sys->p_frame );
    }
    packetizer_Clean( &p_sys->packetizer );

    var_Destroy( p_dec, "packetizer-mpegvideo-sync-iframe" );

    free( p_sys );
}

/*****************************************************************************
 * Packetize:
 *****************************************************************************/
static block_t *Packetize( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    return packetizer_Packetize( &p_sys->packetizer, pp_block );
}

static void PacketizeFlush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    packetizer_Flush( &p_sys->packetizer );
}

/*****************************************************************************
 * GetCc:
 *****************************************************************************/
static block_t *GetCc( decoder_t *p_dec, decoder_cc_desc_t *p_desc )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_cc;

    if( !p_sys->cc.b_reorder && p_sys->cc.i_data <= 0 )
        return NULL;

    p_cc = block_Alloc( p_sys->cc.i_data );
    if( p_cc )
    {
        memcpy( p_cc->p_buffer, p_sys->cc.p_data, p_sys->cc.i_data );
        p_cc->i_dts = 
        p_cc->i_pts = p_sys->cc.b_reorder ? p_sys->i_cc_pts : p_sys->i_cc_dts;
        p_cc->i_flags = p_sys->i_cc_flags & BLOCK_FLAG_TYPE_MASK;

        p_desc->i_608_channels = p_sys->cc.i_608channels;
        p_desc->i_708_channels = p_sys->cc.i_708channels;
        p_desc->i_reorder_depth = p_sys->cc.b_reorder ? 0 : -1;
    }
    cc_Flush( &p_sys->cc );
    return p_cc;
}

/*****************************************************************************
 * ProcessSequenceParameters
 *****************************************************************************/
static void ProcessSequenceParameters( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    video_format_t *fmt = &p_dec->fmt_out.video;

    /* Picture size */
    fmt->i_visible_width = p_sys->i_h_size_value + (p_sys->i_h_size_ext << 14);
    fmt->i_width = (fmt->i_visible_width + 0x0F) & ~0x0F;
    fmt->i_visible_height = p_sys->i_v_size_value + (p_sys->i_v_size_ext << 14);
    if( p_sys->b_seq_progressive )
        fmt->i_height = (fmt->i_visible_height + 0x0F) & ~0x0F;
    else
        fmt->i_height = (fmt->i_visible_height + 0x1F) & ~0x1F;

    /* Bitrate */
    if( p_dec->fmt_out.i_bitrate == 0 )
        p_dec->fmt_out.i_bitrate = ((p_sys->i_bitrateupper12 << 18) |
                                    p_sys->i_bitratelower18) * 400;

    /* Frame Rate */

    /* Only of not specified by container */
    if ( !p_dec->fmt_in.video.i_frame_rate ||
         !p_dec->fmt_in.video.i_frame_rate_base )
    {
        static const int code_to_frame_rate[16][2] =
        {
            { 1, 1 },  /* invalid */
            { 24000, 1001 }, { 24, 1 }, { 25, 1 },       { 30000, 1001 },
            { 30, 1 },       { 50, 1 }, { 60000, 1001 }, { 60, 1 },
            /* Unofficial 15fps from Xing*/
            { 15, 1 },
            /* Unofficial economy rates from libmpeg3 */
            { 5, 1 }, { 10, 1 }, { 12, 1 }, { 15, 1 },
            { 1, 1 },  { 1, 1 }  /* invalid */
        };

        /* frames / den */
        unsigned num = code_to_frame_rate[p_sys->i_frame_rate_value][0] << 1;
        unsigned den = code_to_frame_rate[p_sys->i_frame_rate_value][1];
        if( p_sys->i_frame_rate_ext_n || p_sys->i_frame_rate_ext_d )
        {
            vlc_ureduce( &num, &den,
                         num * (1 + p_sys->i_frame_rate_ext_n),
                         den * (1 + p_sys->i_frame_rate_ext_d),
                         CLOCK_FREQ );
        }

        if( num && den ) /* fields / den */
        {
            date_Change( &p_sys->dts, num, den );
            date_Change( &p_sys->prev_iframe_dts, num, den );
        }
    }

    if( fmt->i_frame_rate != (p_sys->dts.i_divider_num >> 1) ||
        fmt->i_frame_rate_base != p_sys->dts.i_divider_den )
    {
        fmt->i_frame_rate = p_sys->dts.i_divider_num >> 1;
        fmt->i_frame_rate_base = p_sys->dts.i_divider_den;

        msg_Dbg( p_dec, "size %ux%u/%ux%u fps %u:%u",
             fmt->i_visible_width, fmt->i_visible_height,
             fmt->i_width, fmt->i_height,
             fmt->i_frame_rate, fmt->i_frame_rate_base);
    }
}

/*****************************************************************************
 * OutputFrame: assemble and tag frame
 *****************************************************************************/
static block_t *OutputFrame( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_pic = NULL;

    if( !p_sys->p_frame )
        return NULL;

    ProcessSequenceParameters( p_dec );

    p_pic = block_ChainGather( p_sys->p_frame );
    if( p_pic == NULL )
    {
        p_sys->p_frame = NULL;
        p_sys->pp_last = &p_sys->p_frame;
        p_sys->b_frame_slice = false;
        return p_pic;
    }

    unsigned i_num_fields;

    if( !p_sys->b_seq_progressive && p_sys->i_picture_structure != 0x03 /* Field Picture */ )
        i_num_fields = 1;
    else
        i_num_fields = 2;

    if( p_sys->b_seq_progressive )
    {
        if( p_sys->i_top_field_first == 0 &&
            p_sys->i_repeat_first_field == 1 )
        {
            i_num_fields *= 2;
        }
        else if( p_sys->i_top_field_first == 1 &&
                 p_sys->i_repeat_first_field == 1 )
        {
            i_num_fields *= 3;
        }
    }
    else
    {
        if( p_sys->i_picture_structure == 0x03 /* Frame Picture */ )
        {
            if( p_sys->i_progressive_frame && p_sys->i_repeat_first_field )
            {
                i_num_fields += 1;
            }
        }
    }

    switch ( p_sys->i_picture_type )
    {
    case 0x01:
        p_pic->i_flags |= BLOCK_FLAG_TYPE_I;
        break;
    case 0x02:
        p_pic->i_flags |= BLOCK_FLAG_TYPE_P;
        break;
    case 0x03:
        p_pic->i_flags |= BLOCK_FLAG_TYPE_B;
        break;
    }

    if( !p_sys->b_seq_progressive )
    {
        if( p_sys->i_picture_structure < 0x03 )
        {
            p_pic->i_flags |= BLOCK_FLAG_SINGLE_FIELD;
            p_pic->i_flags |= (p_sys->i_picture_structure == 0x01) ? BLOCK_FLAG_TOP_FIELD_FIRST
                                                                   : BLOCK_FLAG_BOTTOM_FIELD_FIRST;
        }
        else /* if( p_sys->i_picture_structure == 0x03 ) */
        {
            p_pic->i_flags |= (p_sys->i_top_field_first) ? BLOCK_FLAG_TOP_FIELD_FIRST
                                                         : BLOCK_FLAG_BOTTOM_FIELD_FIRST;
        }
    }

    /* Special case for DVR-MS where we need to fully build pts from scratch
     * and only use first dts as it does not monotonically increase
     * This will NOT work with frame repeats and such, as we would need to fully
     * fill the DPB to get accurate pts timings. */
    if( unlikely( p_dec->fmt_in.i_original_fourcc == VLC_FOURCC( 'D','V','R',' ') ) )
    {
        const bool b_first_xmited = (p_sys->i_prev_temporal_ref != p_sys->i_temporal_ref );

        if( ( p_pic->i_flags & BLOCK_FLAG_TYPE_I ) && b_first_xmited )
        {
            if( date_Get( &p_sys->prev_iframe_dts ) == VLC_TICK_INVALID )
            {
                if( p_sys->i_dts != VLC_TICK_INVALID )
                {
                    date_Set( &p_sys->dts, p_sys->i_dts );
                }
                else
                {
                    if( date_Get( &p_sys->dts ) == VLC_TICK_INVALID )
                    {
                        date_Set( &p_sys->dts, VLC_TICK_0 );
                    }
                }
            }
            p_sys->prev_iframe_dts = p_sys->dts;
        }

        p_pic->i_dts = date_Get( &p_sys->dts );

        /* Compute pts from poc */
        date_t datepts = p_sys->prev_iframe_dts;
        date_Increment( &datepts, (1 + p_sys->i_temporal_ref) * 2 );

        /* Field picture second field case */
        if( p_sys->i_picture_structure != 0x03 )
        {
            /* first sent is not the first in display order */
            if( (p_sys->i_picture_structure >> 1) != !p_sys->i_top_field_first &&
                    b_first_xmited )
            {
                date_Increment( &datepts, 2 );
            }
        }

        p_pic->i_pts = date_Get( &datepts );

        if( date_Get( &p_sys->dts ) != VLC_TICK_INVALID )
        {
            date_Increment( &p_sys->dts,  i_num_fields );

            p_pic->i_length = date_Get( &p_sys->dts ) - p_pic->i_dts;
        }
        p_sys->i_prev_temporal_ref = p_sys->i_temporal_ref;
    }
    else /* General case, use demuxer's dts/pts when set or interpolate */
    {
        if( p_sys->b_low_delay || p_sys->i_picture_type == 0x03 )
        {
            /* Trivial case (DTS == PTS) */
            /* Correct interpolated dts when we receive a new pts/dts */
            if( p_sys->i_pts != VLC_TICK_INVALID )
                date_Set( &p_sys->dts, p_sys->i_pts );
            if( p_sys->i_dts != VLC_TICK_INVALID )
                date_Set( &p_sys->dts, p_sys->i_dts );
        }
        else
        {
            /* Correct interpolated dts when we receive a new pts/dts */
            if(p_sys->i_last_ref_pts != VLC_TICK_INVALID && !p_sys->b_second_field)
                date_Set( &p_sys->dts, p_sys->i_last_ref_pts );
            if( p_sys->i_dts != VLC_TICK_INVALID )
                date_Set( &p_sys->dts, p_sys->i_dts );

            if( !p_sys->b_second_field )
                p_sys->i_last_ref_pts = p_sys->i_pts;
        }

        p_pic->i_dts = date_Get( &p_sys->dts );

        /* Set PTS only if we have a B frame or if it comes from the stream */
        if( p_sys->i_pts != VLC_TICK_INVALID )
        {
            p_pic->i_pts = p_sys->i_pts;
        }
        else if( p_sys->i_picture_type == 0x03 )
        {
            p_pic->i_pts = p_pic->i_dts;
        }
        else
        {
            p_pic->i_pts = VLC_TICK_INVALID;
        }

        if( date_Get( &p_sys->dts ) != VLC_TICK_INVALID )
        {
            date_Increment( &p_sys->dts,  i_num_fields );

            p_pic->i_length = date_Get( &p_sys->dts ) - p_pic->i_dts;
        }
    }

#if 0
    msg_Dbg( p_dec, "pic: type=%d struct=%d ref=%d nf=%d tff=%d dts=%"PRId64" ptsdiff=%"PRId64" len=%"PRId64,
             p_sys->i_picture_type, p_sys->i_picture_structure, p_sys->i_temporal_ref, i_num_fields,
             p_sys->i_top_field_first,
             p_pic->i_dts , (p_pic->i_pts != VLC_TICK_INVALID) ? p_pic->i_pts - p_pic->i_dts : 0, p_pic->i_length );
#endif


    /* Reset context */
    p_sys->p_frame = NULL;
    p_sys->pp_last = &p_sys->p_frame;
    p_sys->b_frame_slice = false;

    if( p_sys->i_picture_structure != 0x03 )
    {
        p_sys->b_second_field = !p_sys->b_second_field;
    }
    else
    {
        p_sys->b_second_field = 0;
    }

    /* CC */
    p_sys->b_cc_reset = true;
    p_sys->i_cc_pts = p_pic->i_pts;
    p_sys->i_cc_dts = p_pic->i_dts;
    p_sys->i_cc_flags = p_pic->i_flags & BLOCK_FLAG_TYPE_MASK;

    return p_pic;
}

/*****************************************************************************
 * Helpers:
 *****************************************************************************/
static void PacketizeReset( void *p_private, bool b_flush )
{
    VLC_UNUSED(b_flush);
    decoder_t *p_dec = p_private;
    decoder_sys_t *p_sys = p_dec->p_sys;

    p_sys->i_next_block_flags = BLOCK_FLAG_DISCONTINUITY;
    if( p_sys->p_frame )
    {
        block_ChainRelease( p_sys->p_frame );
        p_sys->p_frame = NULL;
        p_sys->pp_last = &p_sys->p_frame;
        p_sys->b_frame_slice = false;
    }
    date_Set( &p_sys->dts, VLC_TICK_INVALID );
    date_Set( &p_sys->prev_iframe_dts, VLC_TICK_INVALID );
    p_sys->i_dts =
    p_sys->i_pts =
    p_sys->i_last_ref_pts = VLC_TICK_INVALID;
    p_sys->b_waiting_iframe = p_sys->b_sync_on_intra_frame;
    p_sys->i_prev_temporal_ref = 2048;
}

static block_t *PacketizeParse( void *p_private, bool *pb_ts_used, block_t *p_block )
{
    decoder_t *p_dec = p_private;
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* Check if we have a picture start code */
    *pb_ts_used = p_block->p_buffer[3] == PICTURE_STARTCODE;

    p_block = ParseMPEGBlock( p_dec, p_block );
    if( p_block )
    {
        p_block->i_flags |= p_sys->i_next_block_flags;
        p_sys->i_next_block_flags = 0;
    }
    else *pb_ts_used = false; /* only clear up if output */

    return p_block;
}

static block_t * PacketizeDrain( void *p_private )
{
    decoder_t *p_dec = p_private;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->b_waiting_iframe || !p_sys->b_frame_slice )
        return NULL;

    block_t *p_out = OutputFrame( p_dec );
    if( p_out )
    {
        p_out->i_flags |= p_sys->i_next_block_flags;
        p_sys->i_next_block_flags = 0;
    }

    return p_out;
}

static int PacketizeValidate( void *p_private, block_t *p_au )
{
    decoder_t *p_dec = p_private;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( unlikely( p_sys->b_waiting_iframe ) )
    {
        if( (p_au->i_flags & BLOCK_FLAG_TYPE_I) == 0 )
        {
            msg_Dbg( p_dec, "waiting on intra frame" );
            return VLC_EGENERIC;
        }
        msg_Dbg( p_dec, "synced on intra frame" );
        p_sys->b_waiting_iframe = false;
    }

    /* We've just started the stream, wait for the first PTS.
     * We discard here so we can still get the sequence header. */
    if( unlikely( p_sys->i_dts == VLC_TICK_INVALID && p_sys->i_pts == VLC_TICK_INVALID &&
        date_Get( &p_sys->dts ) == VLC_TICK_INVALID ))
    {
        msg_Dbg( p_dec, "need a starting pts/dts" );
        return VLC_EGENERIC;
    }

    /* When starting the stream we can have the first frame with
     * an invalid DTS (i_interpolated_pts is initialized to VLC_TICK_INVALID) */
    if( unlikely( p_au->i_dts == VLC_TICK_INVALID ) )
        p_au->i_dts = p_au->i_pts;

    return VLC_SUCCESS;
}
/*****************************************************************************
 * ParseMPEGBlock: Re-assemble fragments into a block containing a picture
 *****************************************************************************/
static block_t *ParseMPEGBlock( decoder_t *p_dec, block_t *p_frag )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_pic = NULL;

    const enum mpeg_startcode_e startcode = p_frag->p_buffer[3];
    /*
     * Check if previous picture is finished
     */
    if( ( p_sys->b_frame_slice &&
          (startcode == PICTURE_STARTCODE || startcode >  SLICE_STARTCODE_LAST ) ) &&
          p_sys->p_seq == NULL )
    {
        /* We have a picture but without a sequence header we can't
         * do anything */
        msg_Dbg( p_dec, "waiting for sequence start" );
        if( p_sys->p_frame ) block_ChainRelease( p_sys->p_frame );
        p_sys->p_frame = NULL;
        p_sys->pp_last = &p_sys->p_frame;
        p_sys->b_frame_slice = false;

    }
    else if( p_sys->b_frame_slice &&
             (startcode == PICTURE_STARTCODE || startcode >  SLICE_STARTCODE_LAST) )
    {
        const bool b_eos = startcode == SEQUENCE_END_STARTCODE;

        if( b_eos )
        {
            block_ChainLastAppend( &p_sys->pp_last, p_frag );
            p_frag = NULL;
        }

        p_pic = OutputFrame( p_dec );

        if( p_pic && b_eos )
            p_pic->i_flags |= BLOCK_FLAG_END_OF_SEQUENCE;
    }

    if( !p_pic && p_sys->b_cc_reset )
    {
        p_sys->b_cc_reset = false;
        cc_Flush( &p_sys->cc );
    }

    if( !p_frag )
        return p_pic;
    /*
     * Check info of current fragment
     */
    if( startcode == GROUP_STARTCODE )
    {
        /* Group start code */
        unsigned i_fps = p_sys->dts.i_divider_num / (p_sys->dts.i_divider_den << 1);
        if( p_sys->p_seq && p_sys->i_seq_old > i_fps )
        {
            /* Useful for mpeg1: repeat sequence header every second */
            const block_t * params[2] = { p_sys->p_seq, p_sys->p_ext };
            for( int i=0; i<2; i++ )
            {
                if( params[i] == NULL )
                    break;
                block_t *p_dup = block_Duplicate( params[i] );
                if( p_dup )
                    block_ChainLastAppend( &p_sys->pp_last, p_dup );
            }
            p_sys->i_seq_old = 0;
        }
    }
    else if( startcode == SEQUENCE_HEADER_STARTCODE && p_frag->i_buffer >= 11 )
    {
        /* Sequence header code */
        if( p_sys->p_seq ) block_Release( p_sys->p_seq );
        if( p_sys->p_ext ) block_Release( p_sys->p_ext );

        p_sys->p_seq = block_Duplicate( p_frag );
        p_sys->i_seq_old = 0;
        p_sys->p_ext = NULL;

        p_sys->i_h_size_value = ( p_frag->p_buffer[4] << 4)|(p_frag->p_buffer[5] >> 4 );
        p_sys->i_v_size_value = ( (p_frag->p_buffer[5]&0x0f) << 8 )|p_frag->p_buffer[6];
        p_sys->i_aspect_ratio_info = p_frag->p_buffer[7] >> 4;

        /* TODO: MPEG1 aspect ratio */

        p_sys->i_frame_rate_value = p_frag->p_buffer[7] & 0x0f;

        p_sys->i_bitratelower18 = (p_frag->p_buffer[ 8] << 12) |
                                  (p_frag->p_buffer[ 9] <<  4) |
                                  (p_frag->p_buffer[10] & 0x0F);
    }
    else if( startcode == EXTENSION_STARTCODE && p_frag->i_buffer > 4 )
    {
        /* extension_start_code_identifier */
        const enum extension_start_code_identifier_e extid = p_frag->p_buffer[4] >> 4;

        /* Extension start code */
        if( extid == SEQUENCE_EXTENSION_ID )
        {
#if 0
            static const int mpeg2_aspect[16][2] =
            {
                {0,1}, {1,1}, {4,3}, {16,9}, {221,100},
                {0,1}, {0,1}, {0,1}, {0,1}, {0,1}, {0,1}, {0,1}, {0,1}, {0,1},
                {0,1}, {0,1}
            };
#endif
            /* sequence extension */
            if( p_sys->p_ext) block_Release( p_sys->p_ext );
            p_sys->p_ext = block_Duplicate( p_frag );

            if( p_frag->i_buffer >= 10 )
            {
                /* profile and level indication */
                if( p_dec->fmt_out.i_profile == -1 )
                {
                    const uint16_t profilelevel = ((p_frag->p_buffer[4] << 4) |
                                                   (p_frag->p_buffer[5] >> 4)) & 0xFF;
                    int i_profile = -1;
                    int i_level = -1;
                    switch( profilelevel )
                    {
                        case 0x82:
                            i_profile = PROFILE_MPEG2_422;
                            i_level = LEVEL_MPEG2_HIGH;
                            break;
                        case 0x85:
                            i_profile = PROFILE_MPEG2_422;
                            i_level = LEVEL_MPEG2_MAIN;
                            break;
                        case 0x8A:
                            i_profile = PROFILE_MPEG2_MULTIVIEW;
                            i_level = LEVEL_MPEG2_HIGH;
                            break;
                        case 0x8B:
                            i_profile = PROFILE_MPEG2_MULTIVIEW;
                            i_level = LEVEL_MPEG2_HIGH_1440;
                            break;
                        case 0x8D:
                            i_profile = PROFILE_MPEG2_MULTIVIEW;
                            i_level = LEVEL_MPEG2_MAIN;
                            break;
                        case 0x8E:
                            i_profile = PROFILE_MPEG2_MULTIVIEW;
                            i_level = LEVEL_MPEG2_LOW;
                            break;
                        default:
                            if( (profilelevel & 0x80) == 0 ) /* escape bit */
                            {
                                i_profile = (profilelevel >> 4) & 0x07;
                                i_level = profilelevel & 0x0F;
                            }
                            break;
                    }
                    p_dec->fmt_out.i_profile = i_profile;
                    p_dec->fmt_out.i_level = i_level;
                }

                p_sys->b_seq_progressive =
                    p_frag->p_buffer[5]&0x08 ? true : false;

                p_sys->i_h_size_ext = ((p_frag->p_buffer[5] & 0x01) << 1) | (p_frag->p_buffer[6] >> 7);
                p_sys->i_v_size_ext = (p_frag->p_buffer[6] >> 5) & 0x03;

                p_sys->i_bitrateupper12 = ((p_frag->p_buffer[6] & 0x1F) << 8) | (p_frag->p_buffer[7] >> 1);

                p_sys->b_low_delay =
                    p_frag->p_buffer[9]&0x80 ? true : false;

                p_sys->i_frame_rate_ext_n = (p_frag->p_buffer[9] >> 5) & 0x03;
                p_sys->i_frame_rate_ext_d = p_frag->p_buffer[9] & 0x1F;
            }

            /* Do not set aspect ratio : in case we're transcoding,
             * transcode will take our fmt_out as a fmt_in to libmpeg2.
             * libmpeg2.c will then believe that the user has requested
             * a specific aspect ratio, which she hasn't. Thus in case
             * of aspect ratio change, we're screwed. --Meuuh
             */
#if 0
            p_dec->fmt_out.video.i_sar_num =
                mpeg2_aspect[p_sys->i_aspect_ratio_info][0] *
                p_dec->fmt_out.video.i_height;
            p_dec->fmt_out.video.i_sar_den =
                mpeg2_aspect[p_sys->i_aspect_ratio_info][1] *
                p_dec->fmt_out.video.i_width;
#endif

        }
        else if( extid == PICTURE_CODING_EXTENSION_ID && p_frag->i_buffer > 8 )
        {
            /* picture extension */
            p_sys->i_picture_structure = p_frag->p_buffer[6]&0x03;
            p_sys->i_top_field_first   = p_frag->p_buffer[7] >> 7;
            p_sys->i_repeat_first_field= (p_frag->p_buffer[7]>>1)&0x01;
            p_sys->i_progressive_frame = p_frag->p_buffer[8] >> 7;
        }
        else if( extid == SEQUENCE_DISPLAY_EXTENSION_ID && p_frag->i_buffer > 8 )
        {
            /* Sequence display extension */
            bool contains_color_description = (p_frag->p_buffer[4] & 0x01);
            //uint8_t video_format = (p_frag->p_buffer[4] & 0x0f) >> 1;

            if( contains_color_description && p_frag->i_buffer > 11 )
            {
                p_dec->fmt_out.video.primaries =
                        iso_23001_8_cp_to_vlc_primaries( p_frag->p_buffer[5] );
                p_dec->fmt_out.video.transfer =
                        iso_23001_8_tc_to_vlc_xfer( p_frag->p_buffer[6] );
                p_dec->fmt_out.video.space =
                        iso_23001_8_mc_to_vlc_coeffs( p_frag->p_buffer[7] );
            }

        }
    }
    else if( startcode == USER_DATA_STARTCODE && p_frag->i_buffer > 8 )
    {
        /* Frame Packing extension identifier as H262 2012 Amd4 Annex L */
        if( !memcmp( &p_frag->p_buffer[4], "JP3D", 4 ) &&
            p_frag->i_buffer > 11 && p_frag->p_buffer[8] == 0x03 &&
            p_dec->fmt_in.video.multiview_mode == MULTIVIEW_2D )
        {
            video_multiview_mode_t mode;
            switch( p_frag->p_buffer[9] & 0x7F )
            {
                case 0x03:
                    mode = MULTIVIEW_STEREO_SBS; break;
                case 0x04:
                    mode = MULTIVIEW_STEREO_TB; break;
                case 0x08:
                default:
                    mode = MULTIVIEW_2D; break;
            }
            p_dec->fmt_out.video.multiview_mode = mode;
        }
        else
        cc_ProbeAndExtract( &p_sys->cc, p_sys->i_top_field_first,
                    &p_frag->p_buffer[4], p_frag->i_buffer - 4 );
    }
    else if( startcode == PICTURE_STARTCODE )
    {
        /* Picture start code */
        p_sys->i_seq_old++;

        if( p_frag->i_buffer >= 6 )
        {
            p_sys->i_temporal_ref =
                ( p_frag->p_buffer[4] << 2 )|(p_frag->p_buffer[5] >> 6);
            p_sys->i_picture_type = ( p_frag->p_buffer[5] >> 3 ) & 0x03;
        }

        /* Check if we can use timestamps */
        if(p_frag->i_dts != VLC_TICK_INVALID &&
           p_frag->i_dts <= p_sys->i_dts)
        {
            date_t next = p_sys->dts;
            date_Set(&next, p_frag->i_dts);
            /* Because the prev timestamp could have been repeated though
             * helper, clear up if we are within 2 frames backward */
            if(date_Increment(&next, 4) >= p_sys->i_dts)
                p_frag->i_dts = p_frag->i_pts = VLC_TICK_INVALID; /* do not reuse */
        }

        p_sys->i_dts = p_frag->i_dts;
        p_sys->i_pts = p_frag->i_pts;
    }
    else if( startcode >= SLICE_STARTCODE_FIRST &&
             startcode <= SLICE_STARTCODE_LAST )
    {
        /* Slice start code */
        p_sys->b_frame_slice = true;
    }

    /* Append the block */
    block_ChainLastAppend( &p_sys->pp_last, p_frag );

    return p_pic;
}
