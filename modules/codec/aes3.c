/*****************************************************************************
 * aes3.c: aes3 decoder/packetizer module
 *****************************************************************************
 * Copyright (C) 2008 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@videolan.org>
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
    set_capability( "audio decoder", 100 )
    set_callbacks( OpenDecoder, Close )

    add_submodule ()
    set_description( N_("AES3/SMPTE 302M audio packetizer") )
    set_capability( "packetizer", 100 )
    set_callbacks( OpenPacketizer, Close )

vlc_module_end ()

/*****************************************************************************
 * decoder_sys_t : aes3 decoder descriptor
 *****************************************************************************/
typedef struct
{
    /*
     * Output properties
     */
    date_t end_date;
} decoder_sys_t;

#define AES3_HEADER_LEN 4

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Open( decoder_t *p_dec, bool b_packetizer );

static block_t *Parse( decoder_t *p_dec, int *pi_frame_length, int *pi_bits,
                       block_t *p_block, bool b_packetizer );

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

static const uint8_t reverse[256] = {
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0,
    0x30, 0xb0, 0x70, 0xf0, 0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8, 0x04, 0x84, 0x44, 0xc4,
    0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc,
    0x3c, 0xbc, 0x7c, 0xfc, 0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2, 0x0a, 0x8a, 0x4a, 0xca,
    0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6,
    0x36, 0xb6, 0x76, 0xf6, 0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe, 0x01, 0x81, 0x41, 0xc1,
    0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9,
    0x39, 0xb9, 0x79, 0xf9, 0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5, 0x0d, 0x8d, 0x4d, 0xcd,
    0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3,
    0x33, 0xb3, 0x73, 0xf3, 0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb, 0x07, 0x87, 0x47, 0xc7,
    0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf,
    0x3f, 0xbf, 0x7f, 0xff
};

/*****************************************************************************
 * Decode: decodes an aes3 frame.
 ****************************************************************************
 * Beware, this function must be fed with complete frames (PES packet).
 *****************************************************************************/
static int Decode( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t       *p_aout_buffer;
    int            i_frame_length, i_bits;

    p_block = Parse( p_dec, &i_frame_length, &i_bits, p_block, false );
    if( !p_block )
        return VLCDEC_SUCCESS;

    if( decoder_UpdateAudioFormat( p_dec ) )
    {
        p_aout_buffer = NULL;
        goto exit;
    }

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
        uint32_t *p_out = (uint32_t *)p_aout_buffer->p_buffer;

        while( p_block->i_buffer / 7 )
        {
            *(p_out++) =  (reverse[p_block->p_buffer[0]] <<  8)
                        | (reverse[p_block->p_buffer[1]] << 16)
                        | (reverse[p_block->p_buffer[2]] << 24);
            *(p_out++) = ((reverse[p_block->p_buffer[3]] <<  4)
                        | (reverse[p_block->p_buffer[4]] << 12)
                        | (reverse[p_block->p_buffer[5]] << 20)
                        | (reverse[p_block->p_buffer[6]] << 28)) & 0xFFFFFF00;

            p_block->i_buffer -= 7;
            p_block->p_buffer += 7;
        }

    }
    else if( i_bits == 20 )
    {
        uint32_t *p_out = (uint32_t *)p_aout_buffer->p_buffer;

        while( p_block->i_buffer / 6 )
        {
            *(p_out++) = (reverse[p_block->p_buffer[0]] << 12)
                       | (reverse[p_block->p_buffer[1]] << 20)
                       | (reverse[p_block->p_buffer[2]] << 28);
            *(p_out++) = (reverse[p_block->p_buffer[3]] << 12)
                       | (reverse[p_block->p_buffer[4]] << 20)
                       | (reverse[p_block->p_buffer[5]] << 28);

            p_block->i_buffer -= 6;
            p_block->p_buffer += 6;
        }
    }
    else
    {
        uint16_t *p_out = (uint16_t *)p_aout_buffer->p_buffer;

        assert( i_bits == 16 );

        while( p_block->i_buffer / 5 )
        {
            *(p_out++) =  reverse[p_block->p_buffer[0]]
                        |(reverse[p_block->p_buffer[1]] <<  8);
            *(p_out++) = (reverse[p_block->p_buffer[2]] >>  4)
                       | (reverse[p_block->p_buffer[3]] <<  4)
                       | (reverse[p_block->p_buffer[4]] << 12);

            p_block->i_buffer -= 5;
            p_block->p_buffer += 5;
        }
    }

exit:
    block_Release( p_block );
    if( p_aout_buffer != NULL )
        decoder_QueueAudio( p_dec, p_aout_buffer );
    return VLCDEC_SUCCESS;
}

/*****************************************************************************
 * Flush:
 *****************************************************************************/
static void Flush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    date_Set( &p_sys->end_date, VLC_TICK_INVALID );
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

    if( !pp_block ) /* No Drain */
        return NULL;
    p_block = *pp_block;
    *pp_block = NULL; /* So the packet doesn't get re-sent */

    p_block = Parse( p_dec, &i_frame_length, &i_bits, p_block, true );
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

    if( unlikely( !p_sys ) )
        return VLC_EGENERIC;

    /* Misc init */
    date_Init( &p_sys->end_date, 48000, 1 );

    /* Set output properties */
    p_dec->fmt_out.audio.i_rate = 48000;

    /* Set callback */
    if( b_packetizer )
    {
        p_dec->fmt_out.i_codec = VLC_CODEC_302M;

        p_dec->pf_packetize    = Packetize;
    }
    else
    {
        p_dec->fmt_out.i_codec = VLC_CODEC_S16N;
        p_dec->fmt_out.audio.i_bitspersample = 16;

        p_dec->pf_decode    = Decode;
    }
    p_dec->pf_flush            = Flush;
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

static block_t * Parse( decoder_t *p_dec, int *pi_frame_length, int *pi_bits,
                        block_t *p_block, bool b_packetizer )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint32_t h;
    unsigned int i_size;
    int i_channels;
    int i_bits;

    if( !p_block ) /* No drain */
        return NULL;

    if( p_block->i_flags & (BLOCK_FLAG_CORRUPTED|BLOCK_FLAG_DISCONTINUITY) )
    {
        Flush( p_dec );
        if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
        {
            block_Release( p_block );
            return NULL;
        }
    }

    /* Date management */
    if( p_block->i_pts != VLC_TICK_INVALID &&
        p_block->i_pts != date_Get( &p_sys->end_date ) )
    {
        date_Set( &p_sys->end_date, p_block->i_pts );
    }

    if( date_Get( &p_sys->end_date ) == VLC_TICK_INVALID )
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
        p_dec->fmt_out.i_codec = i_bits == 16 ? VLC_CODEC_S16N : VLC_CODEC_S32N;
        p_dec->fmt_out.audio.i_bitspersample = i_bits == 16 ? 16 : 32;
    }

    p_dec->fmt_out.audio.i_channels = i_channels;
    p_dec->fmt_out.audio.i_physical_channels = pi_original_channels[i_channels/2-1];

    *pi_frame_length = (p_block->i_buffer - AES3_HEADER_LEN) / ( (4+i_bits) * i_channels / 8 );
    *pi_bits = i_bits;
    return p_block;
}
