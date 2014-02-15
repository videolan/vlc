/*****************************************************************************
 * a52.c: parse A/52 audio sync info and packetize the stream
 *****************************************************************************
 * Copyright (C) 2001-2002 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <vlc_block_helper.h>
#include <vlc_modules.h>

#include "a52.h"

#include "../packetizer/packetizer_helper.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static int  OpenPacketizer( vlc_object_t * );
static void CloseCommon   ( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("A/52 parser") )
    set_capability( "decoder", 100 )
    set_callbacks( OpenDecoder, CloseCommon )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )

    add_submodule ()
    set_description( N_("A/52 audio packetizer") )
    set_capability( "packetizer", 10 )
    set_callbacks( OpenPacketizer, CloseCommon )
vlc_module_end ()

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/

struct decoder_sys_t
{
    /* Module mode */
    bool b_packetizer;

    /*
     * Input properties
     */
    int i_state;

    block_bytestream_t bytestream;

    /*
     * Common properties
     */
    date_t  end_date;

    mtime_t i_pts;

    vlc_a52_header_t frame;
};

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static block_t *DecodeBlock  ( decoder_t *, block_t ** );

static uint8_t *GetOutBuffer ( decoder_t *, block_t ** );
static block_t *GetAoutBuffer( decoder_t * );
static block_t *GetSoutBuffer( decoder_t * );

/*****************************************************************************
 * OpenCommon: probe the decoder/packetizer and return score
 *****************************************************************************/
static int OpenCommon( vlc_object_t *p_this, bool b_packetizer )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;
    vlc_fourcc_t i_codec;

    switch( p_dec->fmt_in.i_codec )
    {
    case VLC_CODEC_A52:
        i_codec = VLC_CODEC_A52;
        break;
    case VLC_CODEC_EAC3:
        /* XXX ugly hack, a52 does not support eac3 so no eac3 pass-through
         * support */
        if( !b_packetizer )
            return VLC_EGENERIC;
        i_codec = VLC_CODEC_EAC3;
        break;
    default:
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;

    /* Misc init */
    p_sys->b_packetizer = b_packetizer;
    p_sys->i_state = STATE_NOSYNC;
    date_Set( &p_sys->end_date, 0 );
    p_sys->i_pts = VLC_TS_INVALID;

    block_BytestreamInit( &p_sys->bytestream );

    /* Set output properties */
    p_dec->fmt_out.i_cat = AUDIO_ES;
    p_dec->fmt_out.i_codec = i_codec;
    p_dec->fmt_out.audio.i_rate = 0; /* So end_date gets initialized */
    p_dec->fmt_out.audio.i_bytes_per_frame = 0;

    /* Set callback */
    if( b_packetizer )
        p_dec->pf_packetize    = DecodeBlock;
    else
        p_dec->pf_decode_audio = DecodeBlock;
    return VLC_SUCCESS;
}

static int OpenDecoder( vlc_object_t *p_this )
{
    /* HACK: Don't use this codec if we don't have an a52 audio filter */
    if( !module_exists( "a52tofloat32" ) )
        return VLC_EGENERIC;
    return OpenCommon( p_this, false );
}

static int OpenPacketizer( vlc_object_t *p_this )
{
    return OpenCommon( p_this, true );
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function is called just after the thread is launched.
 ****************************************************************************/
static block_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t p_header[VLC_A52_HEADER_SIZE];
    uint8_t *p_buf;
    block_t *p_out_buffer;

    if( !pp_block || !*pp_block ) return NULL;

    if( (*pp_block)->i_flags&(BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
        if( (*pp_block)->i_flags&BLOCK_FLAG_CORRUPTED )
        {
            p_sys->i_state = STATE_NOSYNC;
            block_BytestreamEmpty( &p_sys->bytestream );
        }
        date_Set( &p_sys->end_date, 0 );
        block_Release( *pp_block );
        return NULL;
    }

    if( !date_Get( &p_sys->end_date ) && (*pp_block)->i_pts <= VLC_TS_INVALID)
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( *pp_block );
        return NULL;
    }

    block_BytestreamPush( &p_sys->bytestream, *pp_block );

    while( 1 )
    {
        switch( p_sys->i_state )
        {
        case STATE_NOSYNC:
            while( block_PeekBytes( &p_sys->bytestream, p_header, 2 )
                   == VLC_SUCCESS )
            {
                if( p_header[0] == 0x0b && p_header[1] == 0x77 )
                {
                    p_sys->i_state = STATE_SYNC;
                    break;
                }
                block_SkipByte( &p_sys->bytestream );
            }
            if( p_sys->i_state != STATE_SYNC )
            {
                block_BytestreamFlush( &p_sys->bytestream );

                /* Need more data */
                return NULL;
            }

        case STATE_SYNC:
            /* New frame, set the Presentation Time Stamp */
            p_sys->i_pts = p_sys->bytestream.p_block->i_pts;
            if( p_sys->i_pts > VLC_TS_INVALID &&
                p_sys->i_pts != date_Get( &p_sys->end_date ) )
            {
                date_Set( &p_sys->end_date, p_sys->i_pts );
            }
            p_sys->i_state = STATE_HEADER;

        case STATE_HEADER:
            /* Get A/52 frame header (VLC_A52_HEADER_SIZE bytes) */
            if( block_PeekBytes( &p_sys->bytestream, p_header,
                                 VLC_A52_HEADER_SIZE ) != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }

            /* Check if frame is valid and get frame info */
            if( vlc_a52_header_Parse( &p_sys->frame, p_header, VLC_A52_HEADER_SIZE ) )
            {
                msg_Dbg( p_dec, "emulated sync word" );
                block_SkipByte( &p_sys->bytestream );
                p_sys->i_state = STATE_NOSYNC;
                break;
            }

            p_sys->i_state = STATE_NEXT_SYNC;

        case STATE_NEXT_SYNC:
            /* TODO: If pp_block == NULL, flush the buffer without checking the
             * next sync word */

            /* Check if next expected frame contains the sync word */
            if( block_PeekOffsetBytes( &p_sys->bytestream,
                                       p_sys->frame.i_size, p_header, 2 )
                != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }

            if( p_sys->b_packetizer &&
                p_header[0] == 0 && p_header[1] == 0 )
            {
                /* A52 wav files and audio CD's use stuffing */
                p_sys->i_state = STATE_GET_DATA;
                break;
            }

            if( p_header[0] != 0x0b || p_header[1] != 0x77 )
            {
                msg_Dbg( p_dec, "emulated sync word "
                         "(no sync on following frame)" );
                p_sys->i_state = STATE_NOSYNC;
                block_SkipByte( &p_sys->bytestream );
                break;
            }
            p_sys->i_state = STATE_SEND_DATA;
            break;

        case STATE_GET_DATA:
            /* Make sure we have enough data.
             * (Not useful if we went through NEXT_SYNC) */
            if( block_WaitBytes( &p_sys->bytestream,
                                 p_sys->frame.i_size ) != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }
            p_sys->i_state = STATE_SEND_DATA;

        case STATE_SEND_DATA:
            if( !(p_buf = GetOutBuffer( p_dec, &p_out_buffer )) )
            {
                //p_dec->b_error = true;
                return NULL;
            }

            /* Copy the whole frame into the buffer. When we reach this point
             * we already know we have enough data available. */
            block_GetBytes( &p_sys->bytestream,
                            p_buf, __MIN( p_sys->frame.i_size, p_out_buffer->i_buffer ) );

            /* Make sure we don't reuse the same pts twice */
            if( p_sys->i_pts == p_sys->bytestream.p_block->i_pts )
                p_sys->i_pts = p_sys->bytestream.p_block->i_pts = VLC_TS_INVALID;

            /* So p_block doesn't get re-added several times */
            *pp_block = block_BytestreamPop( &p_sys->bytestream );

            p_sys->i_state = STATE_NOSYNC;

            return p_out_buffer;
        }
    }
}

/*****************************************************************************
 * CloseCommon: clean up the decoder
 *****************************************************************************/
static void CloseCommon( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_BytestreamRelease( &p_sys->bytestream );

    free( p_sys );
}

/*****************************************************************************
 * GetOutBuffer:
 *****************************************************************************/
static uint8_t *GetOutBuffer( decoder_t *p_dec, block_t **pp_out_buffer )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t *p_buf;

    if( p_dec->fmt_out.audio.i_rate != p_sys->frame.i_rate )
    {
        msg_Dbg( p_dec, "A/52 channels:%d samplerate:%d bitrate:%d",
                 p_sys->frame.i_channels, p_sys->frame.i_rate, p_sys->frame.i_bitrate );

        date_Init( &p_sys->end_date, p_sys->frame.i_rate, 1 );
        date_Set( &p_sys->end_date, p_sys->i_pts );
    }

    p_dec->fmt_out.audio.i_rate     = p_sys->frame.i_rate;
    p_dec->fmt_out.audio.i_channels = p_sys->frame.i_channels;
    if( p_dec->fmt_out.audio.i_bytes_per_frame < p_sys->frame.i_size )
        p_dec->fmt_out.audio.i_bytes_per_frame = p_sys->frame.i_size;
    p_dec->fmt_out.audio.i_frame_length = p_sys->frame.i_samples;

    p_dec->fmt_out.audio.i_original_channels = p_sys->frame.i_channels_conf;
    p_dec->fmt_out.audio.i_physical_channels =
        p_sys->frame.i_channels_conf & AOUT_CHAN_PHYSMASK;

    p_dec->fmt_out.i_bitrate = p_sys->frame.i_bitrate;

    if( p_sys->b_packetizer )
    {
        block_t *p_sout_buffer = GetSoutBuffer( p_dec );
        p_buf = p_sout_buffer ? p_sout_buffer->p_buffer : NULL;
        *pp_out_buffer = p_sout_buffer;
    }
    else
    {
        block_t *p_aout_buffer = GetAoutBuffer( p_dec );
        p_buf = p_aout_buffer ? p_aout_buffer->p_buffer : NULL;
        *pp_out_buffer = p_aout_buffer;
    }

    return p_buf;
}

/*****************************************************************************
 * GetAoutBuffer:
 *****************************************************************************/
static block_t *GetAoutBuffer( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_t *p_buf = decoder_NewAudioBuffer( p_dec, p_sys->frame.i_samples );
    if( p_buf )
    {
        p_buf->i_pts = date_Get( &p_sys->end_date );
        p_buf->i_length = date_Increment( &p_sys->end_date,
                                          p_sys->frame.i_samples ) - p_buf->i_pts;
    }

    return p_buf;
}

/*****************************************************************************
 * GetSoutBuffer:
 *****************************************************************************/
static block_t *GetSoutBuffer( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_t *p_block = block_Alloc( p_sys->frame.i_size );
    if( p_block )
    {
        p_block->i_pts = p_block->i_dts = date_Get( &p_sys->end_date );
        p_block->i_length =
            date_Increment( &p_sys->end_date, p_sys->frame.i_samples ) - p_block->i_pts;
    }

    return p_block;
}

