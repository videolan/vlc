/*****************************************************************************
 * mpegvideo.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: mpegvideo.c,v 1.23 2003/11/27 22:44:50 massiot Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
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

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("MPEG-I/II video packetizer") );
    set_capability( "packetizer", 50 );
    set_callbacks( Open, Close );
vlc_module_end();


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static block_t *Packetize( decoder_t *, block_t ** );

static int mpgv_FindStartCode( uint8_t **pp_start, uint8_t *p_end );

struct decoder_sys_t
{
    /* sequence header and extention */
    block_t *p_seq;
    block_t *p_ext;

    /* current frame being building */
    block_t    *p_frame;
    vlc_bool_t b_frame_slice;
    vlc_bool_t b_frame_corrupted;
    vlc_bool_t b_gop;

    /* pts of current picture */
    mtime_t i_pts;
    mtime_t i_dts;

    /* gathering buffer */
    int         i_buffer;
    int         i_buffer_size;
    uint8_t     *p_buffer;
    uint8_t     *p_start, *p_old;

    /* */
    int         i_frame_rate;
    int         i_frame_rate_base;
    vlc_bool_t  b_seq_progressive;
    vlc_bool_t  b_low_delay;

    /* */
    int i_temporal_ref;
    int i_picture_type;
    int i_picture_structure;
    int i_top_field_first;
    int i_repeat_first_field;
    int i_progressive_frame;

    /* */
    int     i_seq_old;  /* How many picture from last seq */

    /* */

    mtime_t i_interpolated_dts;
    mtime_t i_old_duration;
    mtime_t i_last_ref_pts;
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

    es_format_Init( &p_dec->fmt_out, VIDEO_ES, VLC_FOURCC( 'm', 'p', 'g', 'v' ) );
    p_dec->pf_packetize = Packetize;

    p_dec->p_sys = p_sys = malloc( sizeof( decoder_sys_t ) );

    p_sys->p_seq = NULL;
    p_sys->p_ext = NULL;
    p_sys->p_frame = NULL;
    p_sys->b_frame_slice = VLC_FALSE;
    p_sys->b_frame_corrupted = VLC_FALSE;
    p_sys->b_gop = VLC_FALSE;

    p_sys->i_buffer = 0;
    p_sys->i_buffer_size = 10000;
    p_sys->p_buffer = malloc( p_sys->i_buffer_size );
    p_sys->p_start = p_sys->p_buffer;
    p_sys->p_old = NULL;

    p_sys->i_dts = 0;
    p_sys->i_pts = 0;

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
        block_Release( p_sys->p_frame );
    }
    free( p_sys->p_buffer );
    free( p_sys );
}

/*****************************************************************************
 * Packetize:
 *****************************************************************************/
static block_t *Packetize( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t       *p_chain_out = NULL;
    block_t       *p_block;

    if( pp_block == NULL || *pp_block == NULL )
    {
        return NULL;
    }
    p_block = *pp_block;
    *pp_block = NULL;

    if( p_block->b_discontinuity )
    {
        p_sys->b_frame_corrupted = VLC_TRUE;
    }

    /* Append data */
    if( p_sys->i_buffer + p_block->i_buffer > p_sys->i_buffer_size )
    {
        uint8_t *p_buffer = p_sys->p_buffer;

        p_sys->i_buffer_size += p_block->i_buffer + 1024;
        p_sys->p_buffer = realloc( p_sys->p_buffer, p_sys->i_buffer_size );

        if( p_sys->p_start )
        {
            p_sys->p_start = p_sys->p_start - p_buffer + p_sys->p_buffer;
        }
        if( p_sys->p_old )
        {
            p_sys->p_old = p_sys->p_old - p_buffer + p_sys->p_buffer;
        }

    }
    memcpy( &p_sys->p_buffer[p_sys->i_buffer], p_block->p_buffer,
            p_block->i_buffer );
    p_sys->i_buffer += p_block->i_buffer;


    if( p_sys->i_buffer > 10*1000000 )
    {
        msg_Err( p_dec, "mmh reseting context" );
        p_sys->i_buffer = 0;
    }

    /* Split data in block */
    for( ;; )
    {
        if( mpgv_FindStartCode( &p_sys->p_start, &p_sys->p_buffer[p_sys->i_buffer] ) )
        {
            block_Release( p_block );

            if( p_sys->p_seq == NULL )
            {
                block_ChainRelease( p_chain_out );
                return NULL;
            }
            return p_chain_out;
        }

        if( p_sys->p_old )
        {
            /* Extract the data */
            int i_frag = p_sys->p_start - p_sys->p_old;
            block_t *p_frag = block_New( p_dec, i_frag );

            memcpy( p_frag->p_buffer, p_sys->p_old, i_frag );
            if( i_frag < p_sys->i_buffer )
            {
                memmove( p_sys->p_buffer, &p_sys->p_buffer[i_frag],
                         p_sys->i_buffer - i_frag );
            }
            p_sys->i_buffer -= i_frag;
            p_sys->p_start -= i_frag;
            p_sys->p_old   -= i_frag;

            if( p_sys->b_frame_slice && ( p_frag->p_buffer[3] == 0x00 || p_frag->p_buffer[3] > 0xaf ) )
            {
                /* We have a complete picture output it */
                if( p_sys->p_seq == NULL )
                {
                    msg_Dbg( p_dec, "waiting sequence start" );
                    block_ChainRelease( p_sys->p_frame );
                }
                else if( p_sys->i_dts <= 0 && p_sys->i_pts <= 0 && p_sys->i_interpolated_dts <= 0 )
                {
                    msg_Dbg( p_dec, "need a starting pts/dts" );
                    block_ChainRelease( p_sys->p_frame );
                }
                else if( p_sys->b_frame_corrupted )
                {
                    msg_Warn( p_dec, "trashing a corrupted picture" );
                    block_ChainRelease( p_sys->p_frame );
                    p_sys->b_frame_corrupted = VLC_FALSE;
                }
                else
                {
                    block_t *p_pic = block_ChainGather( p_sys->p_frame );
                    mtime_t i_duration = (mtime_t)( 1000000 * p_sys->i_frame_rate_base / p_sys->i_frame_rate);

                    if( !p_sys->b_seq_progressive && p_sys->i_picture_structure != 0x03 )
                    {
                        i_duration /= 2;
                    }

                    if( p_sys->b_seq_progressive )
                    {
                        if( p_sys->i_top_field_first == 0 && p_sys->i_repeat_first_field == 1 )
                        {
                            i_duration *= 2;
                        }
                        else if( p_sys->i_top_field_first == 1 && p_sys->i_repeat_first_field == 1 )
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

                    p_pic->i_dts    = p_sys->i_interpolated_dts;
                    /* Set PTS only if I frame or come from stream */
                    if( p_sys->i_pts > 0 )
                    {
                        p_pic->i_pts    = p_sys->i_pts;
                    }
                    else if( p_sys->i_picture_type == 0x03 )
                    {
                        p_pic->i_pts = p_pic->i_dts;
                    }
                    else
                    {
                        p_pic->i_pts = -1;
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

                    p_pic->i_length = p_sys->i_interpolated_dts - p_pic->i_dts;

                    //msg_Dbg( p_dec, "pic: type=%d dts=%lld pts-dts=%lld", p_sys->i_picture_type, p_pic->i_dts, p_pic->i_pts - p_pic->i_dts);

                    block_ChainAppend( &p_chain_out, p_pic );

                }

                /* reset context */
                p_sys->p_frame = NULL;
                p_sys->b_frame_slice = VLC_FALSE;
                p_sys->b_gop = VLC_FALSE;
                p_sys->i_pts = 0;
                p_sys->i_dts = 0;
            }

            if( p_frag->p_buffer[3] == 0xb8 )
            {
                if( p_sys->p_seq &&
                    p_sys->i_seq_old > p_sys->i_frame_rate/p_sys->i_frame_rate_base )
                {
                    /* Usefull for mpeg1: repeat sequence header every second */
                    block_ChainAppend( &p_sys->p_frame,
                                       block_Duplicate( p_sys->p_seq ) );
                    if( p_sys->p_ext )
                    {
                        block_ChainAppend( &p_sys->p_frame,
                                           block_Duplicate( p_sys->p_ext ) );
                    }

                    p_sys->i_seq_old = 0;
                }
                p_sys->b_gop = VLC_TRUE;
            }
            else if( p_frag->p_buffer[3] == 0xb3 )
            {
                static const int code_to_frame_rate[16][2] =
                {
                    { 1, 1 },   /* invalid */
                    { 24000, 1001 }, { 24, 1 }, { 25, 1 },       { 30000, 1001 },
                    { 30, 1 },       { 50, 1 }, { 60000, 1001 }, { 60, 1 },
                    { 1, 1 },        { 1, 1 },  { 1, 1 },        { 1, 1 },  /* invalid */
                    { 1, 1 },        { 1, 1 },  { 1, 1 }                    /* invalid */
                };

                /* sequence header */
                if( p_sys->p_seq )
                {
                    block_Release( p_sys->p_seq );
                }
                if( p_sys->p_ext )
                {
                    block_Release( p_sys->p_ext );
                    p_sys->p_ext = NULL;
                }
                p_sys->p_seq = block_Duplicate( p_frag );
                p_sys->i_seq_old = 0;

                p_dec->fmt_out.video.i_width = ( p_frag->p_buffer[4] << 4)|(p_frag->p_buffer[5] >> 4 );
                p_dec->fmt_out.video.i_height= ( (p_frag->p_buffer[5]&0x0f) << 8 )|p_frag->p_buffer[6];

                p_sys->i_frame_rate = code_to_frame_rate[p_frag->p_buffer[7]&0x0f][0];
                p_sys->i_frame_rate_base = code_to_frame_rate[p_frag->p_buffer[7]&0x0f][1];


                p_sys->b_seq_progressive = VLC_TRUE;
                p_sys->b_low_delay = VLC_TRUE;

#if 0
                msg_Dbg( p_dec, "Size %dx%d fps=%.3f",
                         p_dec->fmt_out.video.i_width,
                         p_dec->fmt_out.video.i_height,
                         (float)p_sys->i_frame_rate / (float)p_sys->i_frame_rate_base );
#endif
            }
            else if( p_frag->p_buffer[3] == 0xb5 )
            {
                int i_type = p_frag->p_buffer[4] >> 4;
                /* extention start code */
                if( i_type == 0x01 )
                {
                    /* sequence extention */
                    if( p_sys->p_ext)
                    {
                        block_Release( p_sys->p_ext );
                    }
                    p_sys->p_ext = block_Duplicate( p_frag );

                    if( p_frag->i_buffer >= 10 )
                    {
                        p_sys->b_seq_progressive = p_frag->p_buffer[5]&0x08 ? VLC_TRUE : VLC_FALSE;
                        p_sys->b_low_delay = p_frag->p_buffer[9]&0x80 ? VLC_TRUE : VLC_FALSE;
                    }
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
                /* picture */
                p_sys->i_seq_old++;

                if( p_frag->i_buffer >= 6 )
                {
                    p_sys->i_temporal_ref = ( p_frag->p_buffer[4] << 2 )|(p_frag->p_buffer[5] >> 6);
                    p_sys->i_picture_type = ( p_frag->p_buffer[5] >> 3 )&0x03;
                }
                if( !p_sys->b_frame_slice )
                {
                    p_sys->i_dts = p_block->i_dts; p_block->i_dts = 0;
                    p_sys->i_pts = p_block->i_pts; p_block->i_pts = 0;
                }
            }
            else if( p_frag->p_buffer[3] >= 0x01 && p_frag->p_buffer[3] <= 0xaf )
            {
                /* Slice */
                p_sys->b_frame_slice = VLC_TRUE;
            }

            /* Append the block */
            block_ChainAppend( &p_sys->p_frame, p_frag );
        }
        p_sys->p_old = p_sys->p_start;
        p_sys->p_start += 4;
    }
}

static int mpgv_FindStartCode( uint8_t **pp_start, uint8_t *p_end )
{
    uint8_t *p = *pp_start;

    for( p = *pp_start; p < p_end - 4; p++ )
    {
        if( p[0] == 0 && p[1] == 0 && p[2] == 1 )
        {
            *pp_start = p;
            return VLC_SUCCESS;
        }
    }

    *pp_start = p;
    return VLC_EGENERIC;
}


