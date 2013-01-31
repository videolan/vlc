/*****************************************************************************
 * mpegvideo.c: parse and packetize an MPEG1/2 video stream
 *****************************************************************************
 * Copyright (C) 2001-2006 VLC authors and VideoLAN
 * $Id$
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
#include "../codec/cc.h"
#include "packetizer_helper.h"

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

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct decoder_sys_t
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
    mtime_t i_pts;
    mtime_t i_dts;

    /* Sequence properties */
    int         i_frame_rate;
    int         i_frame_rate_base;
    bool  b_seq_progressive;
    bool  b_low_delay;
    int         i_aspect_ratio_info;
    bool  b_inited;

    /* Picture properties */
    int i_temporal_ref;
    int i_picture_type;
    int i_picture_structure;
    int i_top_field_first;
    int i_repeat_first_field;
    int i_progressive_frame;

    mtime_t i_interpolated_dts;
    mtime_t i_last_ref_pts;
    bool b_second_field;

    /* Number of pictures since last sequence header */
    int i_seq_old;

    /* Sync behaviour */
    bool  b_sync_on_intra_frame;
    bool  b_discontinuity;

    /* */
    bool b_cc_reset;
    uint32_t i_cc_flags;
    mtime_t i_cc_pts;
    mtime_t i_cc_dts;
    cc_data_t cc;
};

static block_t *Packetize( decoder_t *, block_t ** );
static block_t *GetCc( decoder_t *p_dec, bool pb_present[4] );

static void PacketizeReset( void *p_private, bool b_broken );
static block_t *PacketizeParse( void *p_private, bool *pb_ts_used, block_t * );
static int PacketizeValidate( void *p_private, block_t * );

static block_t *ParseMPEGBlock( decoder_t *, block_t * );

static const uint8_t p_mp2v_startcode[3] = { 0x00, 0x00, 0x01 };

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_MPGV )
        return VLC_EGENERIC;

    es_format_Init( &p_dec->fmt_out, VIDEO_ES, VLC_CODEC_MPGV );
    p_dec->fmt_out.i_original_fourcc = p_dec->fmt_in.i_original_fourcc;

    p_dec->pf_packetize = Packetize;
    p_dec->pf_get_cc = GetCc;

    p_dec->p_sys = p_sys = malloc( sizeof( decoder_sys_t ) );
    if( !p_dec->p_sys )
        return VLC_ENOMEM;
    memset( p_dec->p_sys, 0, sizeof( decoder_sys_t ) );

    /* Misc init */
    packetizer_Init( &p_sys->packetizer,
                     p_mp2v_startcode, sizeof(p_mp2v_startcode),
                     NULL, 0, 4,
                     PacketizeReset, PacketizeParse, PacketizeValidate, p_dec );

    p_sys->p_seq = NULL;
    p_sys->p_ext = NULL;
    p_sys->p_frame = NULL;
    p_sys->pp_last = &p_sys->p_frame;
    p_sys->b_frame_slice = false;

    p_sys->i_dts = p_sys->i_pts = VLC_TS_INVALID;

    p_sys->i_frame_rate = 1;
    p_sys->i_frame_rate_base = 1;
    p_sys->b_seq_progressive = true;
    p_sys->b_low_delay = true;
    p_sys->i_seq_old = 0;

    p_sys->i_temporal_ref = 0;
    p_sys->i_picture_type = 0;
    p_sys->i_picture_structure = 0x03; /* frame */
    p_sys->i_top_field_first = 0;
    p_sys->i_repeat_first_field = 0;
    p_sys->i_progressive_frame = 0;
    p_sys->b_inited = 0;

    p_sys->i_interpolated_dts = VLC_TS_INVALID;
    p_sys->i_last_ref_pts = VLC_TS_INVALID;
    p_sys->b_second_field = 0;

    p_sys->b_discontinuity = false;
    p_sys->b_sync_on_intra_frame = var_CreateGetBool( p_dec, "packetizer-mpegvideo-sync-iframe" );
    if( p_sys->b_sync_on_intra_frame )
        msg_Dbg( p_dec, "syncing on intra frame now" );

    p_sys->b_cc_reset = false;
    p_sys->i_cc_pts = 0;
    p_sys->i_cc_dts = 0;
    p_sys->i_cc_flags = 0;
    cc_Init( &p_sys->cc );

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

/*****************************************************************************
 * GetCc:
 *****************************************************************************/
static block_t *GetCc( decoder_t *p_dec, bool pb_present[4] )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_cc;
    int i;

    for( i = 0; i < 4; i++ )
        pb_present[i] = p_sys->cc.pb_present[i];

    if( p_sys->cc.i_data <= 0 )
        return NULL;

    p_cc = block_Alloc( p_sys->cc.i_data );
    if( p_cc )
    {
        memcpy( p_cc->p_buffer, p_sys->cc.p_data, p_sys->cc.i_data );
        p_cc->i_dts = 
        p_cc->i_pts = p_sys->cc.b_reorder ? p_sys->i_cc_pts : p_sys->i_cc_dts;
        p_cc->i_flags = ( p_sys->cc.b_reorder  ? p_sys->i_cc_flags : BLOCK_FLAG_TYPE_P ) & ( BLOCK_FLAG_TYPE_I|BLOCK_FLAG_TYPE_P|BLOCK_FLAG_TYPE_B);
    }
    cc_Flush( &p_sys->cc );
    return p_cc;
}

/*****************************************************************************
 * Helpers:
 *****************************************************************************/
static void PacketizeReset( void *p_private, bool b_broken )
{
    decoder_t *p_dec = p_private;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( b_broken )
    {
        p_sys->b_discontinuity = true;
        if( p_sys->p_frame )
            block_ChainRelease( p_sys->p_frame );
        p_sys->p_frame = NULL;
        p_sys->pp_last = &p_sys->p_frame;
        p_sys->b_frame_slice = false;
    }
    p_sys->i_dts =
    p_sys->i_pts =
    p_sys->i_interpolated_dts =
    p_sys->i_last_ref_pts = VLC_TS_INVALID;
}

static block_t *PacketizeParse( void *p_private, bool *pb_ts_used, block_t *p_block )
{
    decoder_t *p_dec = p_private;

    /* Check if we have a picture start code */
    *pb_ts_used = p_block->p_buffer[3] == 0x00;

    return ParseMPEGBlock( p_dec, p_block );
}


static int PacketizeValidate( void *p_private, block_t *p_au )
{
    decoder_t *p_dec = p_private;
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* If a discontinuity has been encountered, then wait till
     * the next Intra frame before continuing with packetizing */
    if( p_sys->b_discontinuity &&
        p_sys->b_sync_on_intra_frame )
    {
        if( (p_au->i_flags & BLOCK_FLAG_TYPE_I) == 0 )
        {
            msg_Dbg( p_dec, "waiting on intra frame" );
            return VLC_EGENERIC;
        }
        msg_Dbg( p_dec, "synced on intra frame" );
        p_sys->b_discontinuity = false;
        p_au->i_flags |= BLOCK_FLAG_DISCONTINUITY;
    }

    /* We've just started the stream, wait for the first PTS.
     * We discard here so we can still get the sequence header. */
    if( p_sys->i_dts <= VLC_TS_INVALID && p_sys->i_pts <= VLC_TS_INVALID &&
        p_sys->i_interpolated_dts <= VLC_TS_INVALID )
    {
        msg_Dbg( p_dec, "need a starting pts/dts" );
        return VLC_EGENERIC;
    }

    /* When starting the stream we can have the first frame with
     * an invalid DTS (i_interpolated_pts is initialized to VLC_TS_INVALID) */
    if( p_au->i_dts <= VLC_TS_INVALID )
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

    /*
     * Check if previous picture is finished
     */
    if( ( p_sys->b_frame_slice &&
          (p_frag->p_buffer[3] == 0x00 || p_frag->p_buffer[3] > 0xaf) ) &&
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
             (p_frag->p_buffer[3] == 0x00 || p_frag->p_buffer[3] > 0xaf) )
    {
        const bool b_eos = p_frag->p_buffer[3] == 0xb7;

        mtime_t i_duration;

        if( b_eos )
        {
            block_ChainLastAppend( &p_sys->pp_last, p_frag );
            p_frag = NULL;
        }

        p_pic = block_ChainGather( p_sys->p_frame );

        if( b_eos )
            p_pic->i_flags |= BLOCK_FLAG_END_OF_SEQUENCE;

        i_duration = (mtime_t)( 1000000 * p_sys->i_frame_rate_base /
                                p_sys->i_frame_rate );

        if( !p_sys->b_seq_progressive && p_sys->i_picture_structure != 0x03 )
        {
            i_duration /= 2;
        }

        if( p_sys->b_seq_progressive )
        {
            if( p_sys->i_top_field_first == 0 &&
                p_sys->i_repeat_first_field == 1 )
            {
                i_duration *= 2;
            }
            else if( p_sys->i_top_field_first == 1 &&
                     p_sys->i_repeat_first_field == 1 )
            {
                i_duration *= 3;
            }
        }
        else
        {
            if( p_sys->i_picture_structure == 0x03 )
            {
                if( p_sys->i_progressive_frame && p_sys->i_repeat_first_field )
                {
                    i_duration += i_duration / 2;
                }
            }
        }

        if( p_sys->b_low_delay || p_sys->i_picture_type == 0x03 )
        {
            /* Trivial case (DTS == PTS) */
            /* Correct interpolated dts when we receive a new pts/dts */
            if( p_sys->i_pts > VLC_TS_INVALID )
                p_sys->i_interpolated_dts = p_sys->i_pts;
            if( p_sys->i_dts > VLC_TS_INVALID )
                p_sys->i_interpolated_dts = p_sys->i_dts;
        }
        else
        {
            /* Correct interpolated dts when we receive a new pts/dts */
            if(p_sys->i_last_ref_pts > VLC_TS_INVALID && !p_sys->b_second_field)
                p_sys->i_interpolated_dts = p_sys->i_last_ref_pts;
            if( p_sys->i_dts > VLC_TS_INVALID )
                p_sys->i_interpolated_dts = p_sys->i_dts;

            if( !p_sys->b_second_field )
                p_sys->i_last_ref_pts = p_sys->i_pts;
        }

        p_pic->i_dts = p_sys->i_interpolated_dts;
        p_sys->i_interpolated_dts += i_duration;

        /* Set PTS only if we have a B frame or if it comes from the stream */
        if( p_sys->i_pts > VLC_TS_INVALID )
        {
            p_pic->i_pts = p_sys->i_pts;
        }
        else if( p_sys->i_picture_type == 0x03 )
        {
            p_pic->i_pts = p_pic->i_dts;
        }
        else
        {
            p_pic->i_pts = VLC_TS_INVALID;
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

        p_pic->i_length = p_sys->i_interpolated_dts - p_pic->i_dts;

#if 0
        msg_Dbg( p_dec, "pic: type=%d dts=%"PRId64" pts-dts=%"PRId64,
        p_sys->i_picture_type, p_pic->i_dts, p_pic->i_pts - p_pic->i_dts);
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
        p_sys->i_cc_flags = p_pic->i_flags;
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
    if( p_frag->p_buffer[3] == 0xb8 )
    {
        /* Group start code */
        if( p_sys->p_seq &&
            p_sys->i_seq_old > p_sys->i_frame_rate/p_sys->i_frame_rate_base )
        {
            /* Useful for mpeg1: repeat sequence header every second */
            block_ChainLastAppend( &p_sys->pp_last, block_Duplicate( p_sys->p_seq ) );
            if( p_sys->p_ext )
            {
                block_ChainLastAppend( &p_sys->pp_last, block_Duplicate( p_sys->p_ext ) );
            }

            p_sys->i_seq_old = 0;
        }
    }
    else if( p_frag->p_buffer[3] == 0xb3 && p_frag->i_buffer >= 8 )
    {
        /* Sequence header code */
        static const int code_to_frame_rate[16][2] =
        {
            { 1, 1 },  /* invalid */
            { 24000, 1001 }, { 24, 1 }, { 25, 1 },       { 30000, 1001 },
            { 30, 1 },       { 50, 1 }, { 60000, 1001 }, { 60, 1 },
            /* Unofficial 15fps from Xing*/
            { 15, 1001 },
            /* Unofficial economy rates from libmpeg3 */
            { 5000, 1001 }, { 1000, 1001 }, { 12000, 1001 }, { 15000, 1001 },
            { 1, 1 },  { 1, 1 }  /* invalid */
        };

        if( p_sys->p_seq ) block_Release( p_sys->p_seq );
        if( p_sys->p_ext ) block_Release( p_sys->p_ext );

        p_sys->p_seq = block_Duplicate( p_frag );
        p_sys->i_seq_old = 0;
        p_sys->p_ext = NULL;

        p_dec->fmt_out.video.i_width =
            ( p_frag->p_buffer[4] << 4)|(p_frag->p_buffer[5] >> 4 );
        p_dec->fmt_out.video.i_height =
            ( (p_frag->p_buffer[5]&0x0f) << 8 )|p_frag->p_buffer[6];
        p_sys->i_aspect_ratio_info = p_frag->p_buffer[7] >> 4;

        /* TODO: MPEG1 aspect ratio */

        p_sys->i_frame_rate = code_to_frame_rate[p_frag->p_buffer[7]&0x0f][0];
        p_sys->i_frame_rate_base =
            code_to_frame_rate[p_frag->p_buffer[7]&0x0f][1];

        p_dec->fmt_out.video.i_frame_rate = p_sys->i_frame_rate;
        p_dec->fmt_out.video.i_frame_rate_base = p_sys->i_frame_rate_base;

        p_sys->b_seq_progressive = true;
        p_sys->b_low_delay = true;

        if ( !p_sys->b_inited )
        {
            msg_Dbg( p_dec, "size %dx%d fps=%.3f",
                 p_dec->fmt_out.video.i_width, p_dec->fmt_out.video.i_height,
                 p_sys->i_frame_rate / (float)p_sys->i_frame_rate_base );
            p_sys->b_inited = 1;
        }
    }
    else if( p_frag->p_buffer[3] == 0xb5 )
    {
        int i_type = p_frag->p_buffer[4] >> 4;

        /* Extension start code */
        if( i_type == 0x01 )
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
                p_sys->b_seq_progressive =
                    p_frag->p_buffer[5]&0x08 ? true : false;
                p_sys->b_low_delay =
                    p_frag->p_buffer[9]&0x80 ? true : false;
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
        else if( i_type == 0x08 )
        {
            /* picture extension */
            p_sys->i_picture_structure = p_frag->p_buffer[6]&0x03;
            p_sys->i_top_field_first   = p_frag->p_buffer[7] >> 7;
            p_sys->i_repeat_first_field= (p_frag->p_buffer[7]>>1)&0x01;
            p_sys->i_progressive_frame = p_frag->p_buffer[8] >> 7;
        }
    }
    else if( p_frag->p_buffer[3] == 0xb2 && p_frag->i_buffer > 4 )
    {
        cc_Extract( &p_sys->cc, p_sys->i_top_field_first,
                    &p_frag->p_buffer[4], p_frag->i_buffer - 4 );
    }
    else if( p_frag->p_buffer[3] == 0x00 )
    {
        /* Picture start code */
        p_sys->i_seq_old++;

        if( p_frag->i_buffer >= 6 )
        {
            p_sys->i_temporal_ref =
                ( p_frag->p_buffer[4] << 2 )|(p_frag->p_buffer[5] >> 6);
            p_sys->i_picture_type = ( p_frag->p_buffer[5] >> 3 ) & 0x03;
        }

        p_sys->i_dts = p_frag->i_dts;
        p_sys->i_pts = p_frag->i_pts;
    }
    else if( p_frag->p_buffer[3] >= 0x01 && p_frag->p_buffer[3] <= 0xaf )
    {
        /* Slice start code */
        p_sys->b_frame_slice = true;
    }

    /* Append the block */
    block_ChainLastAppend( &p_sys->pp_last, p_frag );

    return p_pic;
}
