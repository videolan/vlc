/*****************************************************************************
 * mpegvideo.c: parse and packetize an MPEG1/2 video stream
 *****************************************************************************
 * Copyright (C) 2001, 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include <vlc/input.h>

#include "vlc_block_helper.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_PACKETIZER );
    set_description( _("MPEG-I/II video packetizer") );
    set_capability( "packetizer", 50 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static block_t *Packetize( decoder_t *, block_t ** );
static block_t *ParseMPEGBlock( decoder_t *, block_t * );

struct decoder_sys_t
{
    /*
     * Input properties
     */
    block_bytestream_t bytestream;
    int i_state;
    int i_offset;
    uint8_t p_startcode[3];

    /* Sequence header and extension */
    block_t *p_seq;
    block_t *p_ext;

    /* Current frame being built */
    block_t    *p_frame;
    block_t    **pp_last;

    vlc_bool_t b_frame_slice;
    mtime_t i_pts;
    mtime_t i_dts;

    /* Sequence properties */
    int         i_frame_rate;
    int         i_frame_rate_base;
    vlc_bool_t  b_seq_progressive;
    vlc_bool_t  b_low_delay;
    int         i_aspect_ratio_info;
    vlc_bool_t  b_inited;

    /* Picture properties */
    int i_temporal_ref;
    int i_picture_type;
    int i_picture_structure;
    int i_top_field_first;
    int i_repeat_first_field;
    int i_progressive_frame;

    mtime_t i_interpolated_dts;
    mtime_t i_old_duration;
    mtime_t i_last_ref_pts;

    /* Number of pictures since last sequence header */
    int i_seq_old;

};

enum {
    STATE_NOSYNC,
    STATE_NEXT_SYNC
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC( 'm', 'p', 'g', '1' ) &&
        p_dec->fmt_in.i_codec != VLC_FOURCC( 'm', 'p', 'g', '2' ) &&
        p_dec->fmt_in.i_codec != VLC_FOURCC( 'm', 'p', 'g', 'v' ) )
    {
        return VLC_EGENERIC;
    }

    es_format_Init( &p_dec->fmt_out, VIDEO_ES, VLC_FOURCC('m','p','g','v') );
    p_dec->pf_packetize = Packetize;

    p_dec->p_sys = p_sys = malloc( sizeof( decoder_sys_t ) );

    /* Misc init */
    p_sys->i_state = STATE_NOSYNC;
    p_sys->bytestream = block_BytestreamInit( p_dec );
    p_sys->p_startcode[0] = 0;
    p_sys->p_startcode[1] = 0;
    p_sys->p_startcode[2] = 1;
    p_sys->i_offset = 0;

    p_sys->p_seq = NULL;
    p_sys->p_ext = NULL;
    p_sys->p_frame = NULL;
    p_sys->pp_last = &p_sys->p_frame;
    p_sys->b_frame_slice = VLC_FALSE;

    p_sys->i_dts = p_sys->i_pts = 0;

    p_sys->i_frame_rate = 1;
    p_sys->i_frame_rate_base = 1;
    p_sys->b_seq_progressive = VLC_TRUE;
    p_sys->b_low_delay = VLC_TRUE;
    p_sys->i_seq_old = 0;

    p_sys->i_temporal_ref = 0;
    p_sys->i_picture_type = 0;
    p_sys->i_picture_structure = 0x03; /* frame */
    p_sys->i_top_field_first = 0;
    p_sys->i_repeat_first_field = 0;
    p_sys->i_progressive_frame = 0;
    p_sys->b_inited = 0;

    p_sys->i_interpolated_dts = 0;
    p_sys->i_old_duration = 0;
    p_sys->i_last_ref_pts = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_BytestreamRelease( &p_sys->bytestream );

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

    free( p_sys );
}

/*****************************************************************************
 * Packetize:
 *****************************************************************************/
static block_t *Packetize( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t       *p_pic;

    if( pp_block == NULL || *pp_block == NULL )
    {
        return NULL;
    }

    if( (*pp_block)->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
        p_sys->i_state = STATE_NOSYNC;
        if( p_sys->p_frame ) block_ChainRelease( p_sys->p_frame );
        p_sys->p_frame = NULL;
        p_sys->pp_last = &p_sys->p_frame;
        p_sys->b_frame_slice = VLC_FALSE;
        block_Release( *pp_block );
        return NULL;
    }

    block_BytestreamPush( &p_sys->bytestream, *pp_block );

    while( 1 )
    {
        switch( p_sys->i_state )
        {

        case STATE_NOSYNC:
            if( block_FindStartcodeFromOffset( &p_sys->bytestream,
                    &p_sys->i_offset, p_sys->p_startcode, 3 ) == VLC_SUCCESS )
            {
                p_sys->i_state = STATE_NEXT_SYNC;
            }

            if( p_sys->i_offset )
            {
                block_SkipBytes( &p_sys->bytestream, p_sys->i_offset );
                p_sys->i_offset = 0;
                block_BytestreamFlush( &p_sys->bytestream );
            }

            if( p_sys->i_state != STATE_NEXT_SYNC )
            {
                /* Need more data */
                return NULL;
            }

            p_sys->i_offset = 1; /* To find next startcode */

        case STATE_NEXT_SYNC:
            /* TODO: If p_block == NULL, flush the buffer without checking the
             * next sync word */

            /* Find the next startcode */
            if( block_FindStartcodeFromOffset( &p_sys->bytestream,
                    &p_sys->i_offset, p_sys->p_startcode, 3 ) != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }

            /* Get the new fragment and set the pts/dts */
            p_pic = block_New( p_dec, p_sys->i_offset );
            block_BytestreamFlush( &p_sys->bytestream );
            p_pic->i_pts = p_sys->bytestream.p_block->i_pts;
            p_pic->i_dts = p_sys->bytestream.p_block->i_dts;

            block_GetBytes( &p_sys->bytestream, p_pic->p_buffer,
                            p_pic->i_buffer );

            /* don't reuse the same timestamps several times */
            if( p_pic->i_buffer >= 4 && p_pic->p_buffer[3] == 0x00 )
            {
                /* We have a picture start code */
                p_sys->bytestream.p_block->i_pts = 0;
                p_sys->bytestream.p_block->i_dts = 0;
            }

            p_sys->i_offset = 0;

            /* Get picture if any */
            if( !( p_pic = ParseMPEGBlock( p_dec, p_pic ) ) )
            {
                p_sys->i_state = STATE_NOSYNC;
                break;
            }

            /* We've just started the stream, wait for the first PTS.
             * We discard here so we can still get the sequence header. */
            if( p_sys->i_dts <= 0 && p_sys->i_pts <= 0 &&
                p_sys->i_interpolated_dts <= 0 )
            {
                msg_Dbg( p_dec, "need a starting pts/dts" );
                p_sys->i_state = STATE_NOSYNC;
                block_Release( p_pic );
                break;
            }

            /* When starting the stream we can have the first frame with
             * a null DTS (i_interpolated_pts is initialized to 0) */
            if( !p_pic->i_dts ) p_pic->i_dts = p_pic->i_pts;

            /* So p_block doesn't get re-added several times */
            *pp_block = block_BytestreamPop( &p_sys->bytestream );

            p_sys->i_state = STATE_NOSYNC;

            return p_pic;
        }
    }
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
        p_sys->b_frame_slice = VLC_FALSE;

    }
    else if( p_sys->b_frame_slice &&
             (p_frag->p_buffer[3] == 0x00 || p_frag->p_buffer[3] > 0xaf) )
    {
        mtime_t i_duration;

        p_pic = block_ChainGather( p_sys->p_frame );

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
            if( p_sys->i_pts > 0 ) p_sys->i_interpolated_dts = p_sys->i_pts;
            if( p_sys->i_dts > 0 ) p_sys->i_interpolated_dts = p_sys->i_dts;
        }
        else
        {
            /* Correct interpolated dts when we receive a new pts/dts */
            if( p_sys->i_last_ref_pts > 0 )
                p_sys->i_interpolated_dts = p_sys->i_last_ref_pts;
            if( p_sys->i_dts > 0 ) p_sys->i_interpolated_dts = p_sys->i_dts;

            p_sys->i_last_ref_pts = p_sys->i_pts;
        }

        p_pic->i_dts = p_sys->i_interpolated_dts;

        /* Set PTS only if we have a B frame or if it comes from the stream */
        if( p_sys->i_pts > 0 )
        {
            p_pic->i_pts = p_sys->i_pts;
        }
        else if( p_sys->i_picture_type == 0x03 )
        {
            p_pic->i_pts = p_pic->i_dts;
        }
        else
        {
            p_pic->i_pts = 0;
        }

        if( p_sys->b_low_delay || p_sys->i_picture_type == 0x03 )
        {
            /* Trivial case (DTS == PTS) */
            p_sys->i_interpolated_dts += i_duration;
        }
        else
        {
            p_sys->i_interpolated_dts += p_sys->i_old_duration;
            p_sys->i_old_duration = i_duration;
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
        msg_Dbg( p_dec, "pic: type=%d dts="I64Fd" pts-dts="I64Fd,
        p_sys->i_picture_type, p_pic->i_dts, p_pic->i_pts - p_pic->i_dts);
#endif

        /* Reset context */
        p_sys->p_frame = NULL;
        p_sys->pp_last = &p_sys->p_frame;
        p_sys->b_frame_slice = VLC_FALSE;
    }

    /*
     * Check info of current fragment
     */
    if( p_frag->p_buffer[3] == 0xb8 )
    {
        /* Group start code */
        if( p_sys->p_seq &&
            p_sys->i_seq_old > p_sys->i_frame_rate/p_sys->i_frame_rate_base )
        {
            /* Usefull for mpeg1: repeat sequence header every second */
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
            { 5, 1001 }, { 10, 1001 }, { 12, 1001 }, { 15, 1001 },
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

        p_sys->b_seq_progressive = VLC_TRUE;
        p_sys->b_low_delay = VLC_TRUE;

        if ( !p_sys->b_inited )
        {
            msg_Dbg( p_dec, "Size %dx%d fps=%.3f",
                 p_dec->fmt_out.video.i_width, p_dec->fmt_out.video.i_height,
                 p_sys->i_frame_rate / (float)p_sys->i_frame_rate_base );
            p_sys->b_inited = 1;
        }
    }
    else if( p_frag->p_buffer[3] == 0xb5 )
    {
        int i_type = p_frag->p_buffer[4] >> 4;

        /* Extention start code */
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

            /* sequence extention */
            if( p_sys->p_ext) block_Release( p_sys->p_ext );
            p_sys->p_ext = block_Duplicate( p_frag );

            if( p_frag->i_buffer >= 10 )
            {
                p_sys->b_seq_progressive =
                    p_frag->p_buffer[5]&0x08 ? VLC_TRUE : VLC_FALSE;
                p_sys->b_low_delay =
                    p_frag->p_buffer[9]&0x80 ? VLC_TRUE : VLC_FALSE;
            }

            /* Do not set aspect ratio : in case we're transcoding,
             * transcode will take our fmt_out as a fmt_in to libmpeg2.
             * libmpeg2.c will then believe that the user has requested
             * a specific aspect ratio, which she hasn't. Thus in case
             * of aspect ratio change, we're screwed. --Meuuh
             */
#if 0
            p_dec->fmt_out.video.i_aspect =
                mpeg2_aspect[p_sys->i_aspect_ratio_info][0] *
                VOUT_ASPECT_FACTOR /
                mpeg2_aspect[p_sys->i_aspect_ratio_info][1];
#endif

        }
        else if( i_type == 0x08 )
        {
            /* picture extention */
            p_sys->i_picture_structure = p_frag->p_buffer[6]&0x03;
            p_sys->i_top_field_first   = p_frag->p_buffer[7] >> 7;
            p_sys->i_repeat_first_field= (p_frag->p_buffer[7]>>1)&0x01;
            p_sys->i_progressive_frame = p_frag->p_buffer[8] >> 7;
        }
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
        p_sys->b_frame_slice = VLC_TRUE;
    }

    /* Append the block */
    block_ChainLastAppend( &p_sys->pp_last, p_frag );

    return p_pic;
}
