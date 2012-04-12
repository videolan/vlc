/*****************************************************************************
 * aes3.c: aes3 decoder/packetizer module
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
#include <assert.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static int  OpenPacketizer( vlc_object_t * );
static void Close         ( vlc_object_t * );

vlc_module_begin ()

    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )
    set_description( N_("AES3/SMPTE 302M audio decoder") )
    set_capability( "decoder", 100 )
    set_callbacks( OpenDecoder, Close )

    add_submodule ()
    set_description( N_("AES3/SMPTE 302M audio packetizer") )
    set_capability( "packetizer", 100 )
    set_callbacks( OpenPacketizer, Close )

vlc_module_end ()

/*****************************************************************************
 * decoder_sys_t : aes3 decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /*
     * Output properties
     */
    date_t end_date;
};

#define AES3_HEADER_LEN 4

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Open( decoder_t *p_dec, bool b_packetizer );

static block_t *Parse( decoder_t *p_dec, int *pi_frame_length, int *pi_bits,
                       block_t **pp_block, bool b_packetizer );

static inline uint8_t Reverse8( int n )
{
    n = ((n >> 1) & 0x55) | ((n << 1) & 0xaa);
    n = ((n >> 2) & 0x33) | ((n << 2) & 0xcc);
    n = ((n >> 4) & 0x0f) | ((n << 4) & 0xf0);
    return n;
}

/*****************************************************************************
 * OpenDecoder:
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    return Open( p_dec, false );
}

/*****************************************************************************
 * OpenPacketizer:
 *****************************************************************************/
static int OpenPacketizer( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    return Open( p_dec, true );
}

/*****************************************************************************
 * Close : aes3 decoder destruction
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    free( p_dec->p_sys );
}

/*****************************************************************************
 * Decode: decodes an aes3 frame.
 ****************************************************************************
 * Beware, this function must be fed with complete frames (PES packet).
 *****************************************************************************/
static aout_buffer_t *Decode( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t       *p_block;
    aout_buffer_t *p_aout_buffer;
    int            i_frame_length, i_bits;

    p_block = Parse( p_dec, &i_frame_length, &i_bits, pp_block, false );
    if( !p_block )
        return NULL;

    p_aout_buffer = decoder_NewAudioBuffer( p_dec, i_frame_length );
    if( p_aout_buffer == NULL )
        goto exit;

    p_aout_buffer->i_pts = date_Get( &p_sys->end_date );
    p_aout_buffer->i_length = date_Increment( &p_sys->end_date,
                                      i_frame_length ) - p_aout_buffer->i_pts;

    p_block->i_buffer -= AES3_HEADER_LEN;
    p_block->p_buffer += AES3_HEADER_LEN;

    if( i_bits == 24 )
    {
        uint8_t *p_out = p_aout_buffer->p_buffer;

        while( p_block->i_buffer / 7 )
        {
            p_out[0] = Reverse8( p_block->p_buffer[0] );
            p_out[1] = Reverse8( p_block->p_buffer[1] );
            p_out[2] = Reverse8( p_block->p_buffer[2] );

            p_out[3] = (Reverse8( p_block->p_buffer[3] ) >> 4) | ( (Reverse8( p_block->p_buffer[4] ) << 4) & 0xf0 );
            p_out[4] = (Reverse8( p_block->p_buffer[4] ) >> 4) | ( (Reverse8( p_block->p_buffer[5] ) << 4) & 0xf0 );
            p_out[5] = (Reverse8( p_block->p_buffer[5] ) >> 4) | ( (Reverse8( p_block->p_buffer[6] ) << 4) & 0xf0 );

            p_block->i_buffer -= 7;
            p_block->p_buffer += 7;
            p_out += 6;
        }

    }
    else if( i_bits == 20 )
    {
        uint8_t *p_out = p_aout_buffer->p_buffer;

        while( p_block->i_buffer / 6 )
        {
            p_out[0] =                                            ( (Reverse8( p_block->p_buffer[0] ) << 4) & 0xf0 );
            p_out[1] = ( Reverse8( p_block->p_buffer[0]) >> 4 ) | ( (Reverse8( p_block->p_buffer[1]) << 4 ) & 0xf0 );
            p_out[2] = ( Reverse8( p_block->p_buffer[1]) >> 4 ) | ( (Reverse8( p_block->p_buffer[2]) << 4 ) & 0xf0 );

            p_out[3] =                                            ( (Reverse8( p_block->p_buffer[3] ) << 4) & 0xf0 );
            p_out[4] = ( Reverse8( p_block->p_buffer[3]) >> 4 ) | ( (Reverse8( p_block->p_buffer[4]) << 4 ) & 0xf0 );
            p_out[5] = ( Reverse8( p_block->p_buffer[4]) >> 4 ) | ( (Reverse8( p_block->p_buffer[5]) << 4 ) & 0xf0 );

            p_block->i_buffer -= 6;
            p_block->p_buffer += 6;
            p_out += 6;
        }
    }
    else
    {
        uint8_t *p_out = p_aout_buffer->p_buffer;

        assert( i_bits == 16 );

        while( p_block->i_buffer / 5 )
        {
            p_out[0] = Reverse8( p_block->p_buffer[0] );
            p_out[1] = Reverse8( p_block->p_buffer[1] );

            p_out[2] = (Reverse8( p_block->p_buffer[2] ) >> 4) | ( (Reverse8( p_block->p_buffer[3] ) << 4) & 0xf0 );
            p_out[3] = (Reverse8( p_block->p_buffer[3] ) >> 4) | ( (Reverse8( p_block->p_buffer[4] ) << 4) & 0xf0 );

            p_block->i_buffer -= 5;
            p_block->p_buffer += 5;
            p_out += 4;
        }
    }

exit:
    block_Release( p_block );
    return p_aout_buffer;
}

/*****************************************************************************
 * Packetize: packetizes an aes3 frame.
 ****************************************************************************
 * Beware, this function must be fed with complete frames (PES packet).
 *****************************************************************************/
static block_t *Packetize( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t       *p_block;
    int           i_frame_length, i_bits;

    p_block = Parse( p_dec, &i_frame_length, &i_bits, pp_block, true );
    if( !p_block )
        return NULL;

    p_block->i_pts = p_block->i_dts = date_Get( &p_sys->end_date );
    p_block->i_length = date_Increment( &p_sys->end_date, i_frame_length ) - p_block->i_pts;

    /* Just pass on the incoming frame */
    return p_block;
}

/*****************************************************************************
 *
 ****************************************************************************/
static int Open( decoder_t *p_dec, bool b_packetizer )
{
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_302M )
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the decoder's structure */
    p_dec->p_sys = p_sys = malloc( sizeof(decoder_sys_t) );

    if( !p_sys )
        return VLC_EGENERIC;

    /* Misc init */
    date_Init( &p_sys->end_date, 48000, 1 );
    date_Set( &p_sys->end_date, 0 );

    /* Set output properties */
    p_dec->fmt_out.i_cat = AUDIO_ES;
    p_dec->fmt_out.audio.i_rate = 48000;

    /* Set callback */
    if( b_packetizer )
    {
        p_dec->fmt_out.i_codec = VLC_CODEC_302M;

        p_dec->pf_decode_audio = NULL;
        p_dec->pf_packetize    = Packetize;
    }
    else
    {
        p_dec->fmt_out.i_codec = VLC_CODEC_S16N;
        p_dec->fmt_out.audio.i_bitspersample = 16;

        p_dec->pf_decode_audio = Decode;
        p_dec->pf_packetize    = NULL;
    }
    return VLC_SUCCESS;
}

static const unsigned int pi_original_channels[4] = {
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
        AOUT_CHAN_CENTER | AOUT_CHAN_LFE,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
        AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT |
        AOUT_CHAN_CENTER | AOUT_CHAN_LFE,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
        AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT |
        AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT |
        AOUT_CHAN_CENTER | AOUT_CHAN_LFE,
};

static block_t *Parse( decoder_t *p_dec, int *pi_frame_length, int *pi_bits,
                       block_t **pp_block, bool b_packetizer )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t       *p_block;
    uint32_t h;
    unsigned int i_size;
    int i_channels;
    int i_bits;

    if( !pp_block || !*pp_block ) return NULL;

    p_block = *pp_block;
    *pp_block = NULL; /* So the packet doesn't get re-sent */

    /* Date management */
    if( p_block->i_pts > VLC_TS_INVALID &&
        p_block->i_pts != date_Get( &p_sys->end_date ) )
    {
        date_Set( &p_sys->end_date, p_block->i_pts );
    }

    if( !date_Get( &p_sys->end_date ) )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( p_block );
        return NULL;
    }

    if( p_block->i_buffer <= AES3_HEADER_LEN )
    {
        msg_Err(p_dec, "frame is too short");
        block_Release( p_block );
        return NULL;
    }

    /*
     * AES3 header :
     * size:            16
     * number channels   2
     * channel_id        8
     * bits per samples  2
     * alignments        4
     */

    h = GetDWBE( p_block->p_buffer );
    i_size = (h >> 16) & 0xffff;
    i_channels = 2 + 2*( (h >> 14) & 0x03 );
    i_bits = 16 + 4*( (h >> 4)&0x03 );

    if( AES3_HEADER_LEN + i_size != p_block->i_buffer || i_bits > 24 )
    {
        msg_Err(p_dec, "frame has invalid header");
        block_Release( p_block );
        return NULL;
    }

    /* Set output properties */
    if( b_packetizer )
    {
        p_dec->fmt_out.audio.i_bitspersample = i_bits;
    }
    else
    {
        p_dec->fmt_out.i_codec = i_bits == 16 ? VLC_CODEC_S16L : VLC_CODEC_S24L;
        p_dec->fmt_out.audio.i_bitspersample = i_bits == 16 ? 16 : 24;
    }

    p_dec->fmt_out.audio.i_channels = i_channels;
    p_dec->fmt_out.audio.i_original_channels = pi_original_channels[i_channels/2-1];
    p_dec->fmt_out.audio.i_physical_channels = pi_original_channels[i_channels/2-1] & AOUT_CHAN_PHYSMASK;

    *pi_frame_length = (p_block->i_buffer - AES3_HEADER_LEN) / ( (4+i_bits) * i_channels / 8 );
    *pi_bits = i_bits;
    return p_block;
}

