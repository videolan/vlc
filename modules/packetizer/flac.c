/*****************************************************************************
 * flac.c: flac packetizer module.
 *****************************************************************************
 * Copyright (C) 1999-2001 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Sigmund Augdal Helberg <dnumgis@videolan.org>
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
#include <vlc_bits.h>
#include "packetizer_helper.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin()
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_PACKETIZER )
    set_description( N_("Flac audio packetizer") )
    set_capability( "packetizer", 50 )
    set_callbacks( Open, Close )
vlc_module_end()

/*****************************************************************************
 * decoder_sys_t : FLAC decoder descriptor
 *****************************************************************************/
#define MAX_FLAC_HEADER_SIZE 16
struct decoder_sys_t
{
    /*
     * Input properties
     */
    int i_state;

    block_bytestream_t bytestream;

    /*
     * FLAC properties
     */
    struct
    {
        unsigned min_blocksize, max_blocksize;
        unsigned min_framesize, max_framesize;
        unsigned sample_rate;
        unsigned channels;
        unsigned bits_per_sample;

    } stream_info;
    bool b_stream_info;

    /*
     * Common properties
     */
    date_t  end_date;
    mtime_t i_pts;

    int i_frame_length;
    size_t i_frame_size;
    unsigned int i_rate, i_channels, i_bits_per_sample;
};

static const int pi_channels_maps[9] =
{
    0,
    AOUT_CHAN_CENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT
     | AOUT_CHAN_MIDDLERIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT
     | AOUT_CHAN_LFE
};


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static block_t *Packetize( decoder_t *, block_t ** );

static int SyncInfo( decoder_t *, uint8_t *, unsigned int *,
                     unsigned int *, unsigned int * );

static uint64_t read_utf8( const uint8_t *p_buf, int *pi_read );
static uint8_t flac_crc8( const uint8_t *data, unsigned len );

static int Open( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_FLAC )
        return VLC_EGENERIC;

    /* */
    p_dec->p_sys = p_sys = malloc(sizeof(*p_sys));
    if( !p_sys )
        return VLC_ENOMEM;

    date_Set( &p_sys->end_date, 0 );
    p_sys->i_state       = STATE_NOSYNC;
    p_sys->b_stream_info = false;
    p_sys->i_pts         = VLC_TS_INVALID;
    block_BytestreamInit( &p_sys->bytestream );

    /* */
    es_format_Copy( &p_dec->fmt_out, &p_dec->fmt_in );
    p_dec->fmt_out.i_cat   = AUDIO_ES;
    p_dec->fmt_out.i_codec = VLC_CODEC_FLAC;

    /* */
    p_dec->pf_decode_audio = NULL;
    p_dec->pf_packetize    = Packetize;

    return VLC_SUCCESS;
}

static void Close( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_BytestreamRelease( &p_sys->bytestream );
    free( p_sys );
}

/*****************************************************************************
 * ProcessHeader: process Flac header.
 *****************************************************************************/
static void ProcessHeader( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    bs_t bs;
    int i_extra = p_dec->fmt_in.i_extra;
    char *p_extra = p_dec->fmt_in.p_extra;

    if (i_extra > 8 && !memcmp(p_extra, "fLaC", 4)) {
        i_extra -= 8;
        p_extra += 8;
    }

    if( p_dec->fmt_in.i_extra < 14 )
        return;

    bs_init( &bs, p_extra, i_extra);

    p_sys->stream_info.min_blocksize = bs_read( &bs, 16 );
    p_sys->stream_info.max_blocksize = bs_read( &bs, 16 );

    p_sys->stream_info.min_framesize = bs_read( &bs, 24 );
    p_sys->stream_info.max_framesize = bs_read( &bs, 24 );

    p_sys->stream_info.sample_rate = bs_read( &bs, 20 );
    p_sys->stream_info.channels = bs_read( &bs, 3 ) + 1;
    p_sys->stream_info.bits_per_sample = bs_read( &bs, 5 ) + 1;

    p_sys->b_stream_info = true;

    p_dec->fmt_out.i_extra = i_extra;
    p_dec->fmt_out.p_extra = xrealloc( p_dec->fmt_out.p_extra, i_extra );
    memcpy( p_dec->fmt_out.p_extra, p_extra, i_extra );
}

/* */
static block_t *Packetize( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t p_header[MAX_FLAC_HEADER_SIZE];
    block_t *p_sout_block;

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

    if( !p_sys->b_stream_info )
        ProcessHeader( p_dec );

    if( p_sys->stream_info.channels > 8 )
    {
        msg_Err( p_dec, "This stream uses too many audio channels" );
        return NULL;
    }

    if( !date_Get( &p_sys->end_date ) && (*pp_block)->i_pts <= VLC_TS_INVALID )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( *pp_block );
        return NULL;
    }
    else if( !date_Get( &p_sys->end_date ) )
    {
        /* The first PTS is as good as anything else. */
        p_sys->i_rate = p_dec->fmt_out.audio.i_rate;
        date_Init( &p_sys->end_date, p_sys->i_rate, 1 );
        date_Set( &p_sys->end_date, (*pp_block)->i_pts );
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
                if( p_header[0] == 0xFF && (p_header[1] & 0xFE) == 0xF8 )
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
            /* Get FLAC frame header (MAX_FLAC_HEADER_SIZE bytes) */
            if( block_PeekBytes( &p_sys->bytestream, p_header,
                                 MAX_FLAC_HEADER_SIZE ) != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }

            /* Check if frame is valid and get frame info */
            p_sys->i_frame_length = SyncInfo( p_dec, p_header,
                                              &p_sys->i_channels,
                                              &p_sys->i_rate,
                                              &p_sys->i_bits_per_sample );
            if( !p_sys->i_frame_length )
            {
                msg_Dbg( p_dec, "emulated sync word" );
                block_SkipByte( &p_sys->bytestream );
                p_sys->i_state = STATE_NOSYNC;
                break;
            }
            if( p_sys->i_rate != p_dec->fmt_out.audio.i_rate )
            {
                p_dec->fmt_out.audio.i_rate = p_sys->i_rate;
                const mtime_t i_end_date = date_Get( &p_sys->end_date );
                date_Init( &p_sys->end_date, p_sys->i_rate, 1 );
                date_Set( &p_sys->end_date, i_end_date );
            }
            p_sys->i_state = STATE_NEXT_SYNC;
            p_sys->i_frame_size = p_sys->b_stream_info && p_sys->stream_info.min_framesize > 0 ?
                                                            p_sys->stream_info.min_framesize : 1;

        case STATE_NEXT_SYNC:
            /* TODO: If pp_block == NULL, flush the buffer without checking the
             * next sync word */

            /* Check if next expected frame contains the sync word */
            while( block_PeekOffsetBytes( &p_sys->bytestream,
                                          p_sys->i_frame_size, p_header,
                                          MAX_FLAC_HEADER_SIZE )
                   == VLC_SUCCESS )
            {
                if( p_header[0] == 0xFF && (p_header[1] & 0xFE) == 0xF8 )
                {
                    /* Check if frame is valid and get frame info */
                    int i_frame_length =
                        SyncInfo( p_dec, p_header,
                                  &p_sys->i_channels,
                                  &p_sys->i_rate,
                                  &p_sys->i_bits_per_sample );

                    if( i_frame_length )
                    {
                        p_sys->i_state = STATE_SEND_DATA;
                        break;
                    }
                }
                p_sys->i_frame_size++;
            }

            if( p_sys->i_state != STATE_SEND_DATA )
            {
                if( p_sys->b_stream_info && p_sys->stream_info.max_framesize > 0 &&
                    p_sys->i_frame_size > p_sys->stream_info.max_framesize )
                {
                    block_SkipByte( &p_sys->bytestream );
                    p_sys->i_state = STATE_NOSYNC;
                    return NULL;
                }
                /* Need more data */
                return NULL;
            }

        case STATE_SEND_DATA:
            p_sout_block = block_Alloc( p_sys->i_frame_size );

            /* Copy the whole frame into the buffer. When we reach this point
             * we already know we have enough data available. */
            block_GetBytes( &p_sys->bytestream, p_sout_block->p_buffer,
                            p_sys->i_frame_size );

            /* Make sure we don't reuse the same pts twice */
            if( p_sys->i_pts == p_sys->bytestream.p_block->i_pts )
                p_sys->i_pts = p_sys->bytestream.p_block->i_pts = VLC_TS_INVALID;

            p_dec->fmt_out.audio.i_channels = p_sys->i_channels;
            p_dec->fmt_out.audio.i_physical_channels =
                p_dec->fmt_out.audio.i_original_channels =
                    pi_channels_maps[p_sys->stream_info.channels];

            /* So p_block doesn't get re-added several times */
            *pp_block = block_BytestreamPop( &p_sys->bytestream );

            p_sys->i_state = STATE_NOSYNC;

            /* Date management */
            p_sout_block->i_pts =
                p_sout_block->i_dts = date_Get( &p_sys->end_date );
            date_Increment( &p_sys->end_date, p_sys->i_frame_length );
            p_sout_block->i_length =
                date_Get( &p_sys->end_date ) - p_sout_block->i_pts;

            return p_sout_block;
        }
    }

    return NULL;
}

/*****************************************************************************
 * SyncInfo: parse FLAC sync info
 *****************************************************************************/
static int SyncInfo( decoder_t *p_dec, uint8_t *p_buf,
                     unsigned int * pi_channels,
                     unsigned int * pi_sample_rate,
                     unsigned int * pi_bits_per_sample )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_header, i_temp, i_read;
    unsigned i_blocksize = 0;
    int i_blocksize_hint = 0, i_sample_rate_hint = 0;

    /* Check syncword */
    if( p_buf[0] != 0xFF || (p_buf[1] & 0xFE) != 0xF8 ) return 0;

    /* Check there is no emulated sync code in the rest of the header */
    if( p_buf[2] == 0xff || p_buf[3] == 0xFF ) return 0;

    /* Find blocksize (framelength) */
    switch( i_temp = p_buf[2] >> 4 )
    {
    case 0:
        if( p_sys->b_stream_info &&
            p_sys->stream_info.min_blocksize == p_sys->stream_info.max_blocksize )
            i_blocksize = p_sys->stream_info.min_blocksize;
        else return 0; /* We can't do anything with this */
        break;

    case 1:
        i_blocksize = 192;
        break;

    case 2:
    case 3:
    case 4:
    case 5:
        i_blocksize = 576 << (i_temp - 2);
        break;

    case 6:
    case 7:
        i_blocksize_hint = i_temp;
        break;

    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
        i_blocksize = 256 << (i_temp - 8);
        break;
    }
    if( p_sys->b_stream_info &&
        ( i_blocksize < p_sys->stream_info.min_blocksize ||
          i_blocksize > p_sys->stream_info.max_blocksize ) )
        return 0;

    /* Find samplerate */
    switch( i_temp = p_buf[2] & 0x0f )
    {
    case 0:
        if( p_sys->b_stream_info )
            *pi_sample_rate = p_sys->stream_info.sample_rate;
        else return 0; /* We can't do anything with this */
        break;

    case 1:
        *pi_sample_rate = 88200;
        break;

    case 2:
        *pi_sample_rate = 176400;
        break;

    case 3:
        *pi_sample_rate = 192000;
        break;

    case 4:
        *pi_sample_rate = 8000;
        break;

    case 5:
        *pi_sample_rate = 16000;
        break;

    case 6:
        *pi_sample_rate = 22050;
        break;

    case 7:
        *pi_sample_rate = 24000;
        break;

    case 8:
        *pi_sample_rate = 32000;
        break;

    case 9:
        *pi_sample_rate = 44100;
        break;

    case 10:
        *pi_sample_rate = 48000;
        break;

    case 11:
        *pi_sample_rate = 96000;
        break;

    case 12:
    case 13:
    case 14:
        i_sample_rate_hint = i_temp;
        break;

    case 15:
        return 0;
    }

    /* Find channels */
    i_temp = (unsigned)(p_buf[3] >> 4);
    if( i_temp & 8 )
    {
        if( ( i_temp & 7 ) >= 3 )
            return 0;
        *pi_channels = 2;
    }
    else
    {
        *pi_channels = i_temp + 1;
    }

    /* Find bits per sample */
    switch( i_temp = (unsigned)(p_buf[3] & 0x0e) >> 1 )
    {
    case 0:
        if( p_sys->b_stream_info )
            *pi_bits_per_sample = p_sys->stream_info.bits_per_sample;
        else
            return 0;
        break;

    case 1:
        *pi_bits_per_sample = 8;
        break;

    case 2:
        *pi_bits_per_sample = 12;
        break;

    case 4:
        *pi_bits_per_sample = 16;
        break;

    case 5:
        *pi_bits_per_sample = 20;
        break;

    case 6:
        *pi_bits_per_sample = 24;
        break;

    case 3:
    case 7:
        return 0;
        break;
    }

    /* Zero padding bit */
    if( p_buf[3] & 0x01 ) return 0;

    /* End of fixed size header */
    i_header = 4;

    /* Check Sample/Frame number */
    if( read_utf8( &p_buf[i_header++], &i_read ) == INT64_C(0xffffffffffffffff) )
        return 0;

    i_header += i_read;

    /* Read blocksize */
    if( i_blocksize_hint )
    {
        int i_val1 = p_buf[i_header++];
        if( i_blocksize_hint == 7 )
        {
            int i_val2 = p_buf[i_header++];
            i_val1 = (i_val1 << 8) | i_val2;
        }
        i_blocksize = i_val1 + 1;
    }

    /* Read sample rate */
    if( i_sample_rate_hint )
    {
        int i_val1 = p_buf[i_header++];
        if( i_sample_rate_hint != 12 )
        {
            int i_val2 = p_buf[i_header++];
            i_val1 = (i_val1 << 8) | i_val2;
        }
        if( i_sample_rate_hint == 12 ) *pi_sample_rate = i_val1 * 1000;
        else if( i_sample_rate_hint == 13 ) *pi_sample_rate = i_val1;
        else *pi_sample_rate = i_val1 * 10;
    }

    /* Check the CRC-8 byte */
    if( flac_crc8( p_buf, i_header ) != p_buf[i_header] )
    {
        return 0;
    }

    /* Sanity check using stream info header when possible */
    if( p_sys->b_stream_info )
    {
        if( i_blocksize < p_sys->stream_info.min_blocksize ||
            i_blocksize > p_sys->stream_info.max_blocksize )
            return 0;
        if( *pi_bits_per_sample != p_sys->stream_info.bits_per_sample )
            return 0;
        if( *pi_sample_rate != p_sys->stream_info.sample_rate )
            return 0;
    }
    return i_blocksize;
}

/* Will return 0xffffffffffffffff for an invalid utf-8 sequence */
static uint64_t read_utf8( const uint8_t *p_buf, int *pi_read )
{
    uint64_t i_result = 0;
    unsigned i, j;

    if( !(p_buf[0] & 0x80) ) /* 0xxxxxxx */
    {
        i_result = p_buf[0];
        i = 0;
    }
    else if( p_buf[0] & 0xC0 && !(p_buf[0] & 0x20) ) /* 110xxxxx */
    {
        i_result = p_buf[0] & 0x1F;
        i = 1;
    }
    else if( p_buf[0] & 0xE0 && !(p_buf[0] & 0x10) ) /* 1110xxxx */
    {
        i_result = p_buf[0] & 0x0F;
        i = 2;
    }
    else if( p_buf[0] & 0xF0 && !(p_buf[0] & 0x08) ) /* 11110xxx */
    {
        i_result = p_buf[0] & 0x07;
        i = 3;
    }
    else if( p_buf[0] & 0xF8 && !(p_buf[0] & 0x04) ) /* 111110xx */
    {
        i_result = p_buf[0] & 0x03;
        i = 4;
    }
    else if( p_buf[0] & 0xFC && !(p_buf[0] & 0x02) ) /* 1111110x */
    {
        i_result = p_buf[0] & 0x01;
        i = 5;
    }
    else if( p_buf[0] & 0xFE && !(p_buf[0] & 0x01) ) /* 11111110 */
    {
        i_result = 0;
        i = 6;
    }
    else {
        return INT64_C(0xffffffffffffffff);
    }

    for( j = 1; j <= i; j++ )
    {
        if( !(p_buf[j] & 0x80) || (p_buf[j] & 0x40) ) /* 10xxxxxx */
        {
            return INT64_C(0xffffffffffffffff);
        }
        i_result <<= 6;
        i_result |= (p_buf[j] & 0x3F);
    }

    *pi_read = i;
    return i_result;
}

/* CRC-8, poly = x^8 + x^2 + x^1 + x^0, init = 0 */
static const uint8_t flac_crc8_table[256] = {
        0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
        0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
        0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
        0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
        0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
        0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
        0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
        0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
        0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
        0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
        0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
        0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
        0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
        0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
        0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
        0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
        0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
        0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
        0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
        0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
        0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
        0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
        0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
        0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
        0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
        0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
        0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
        0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
        0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
        0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
        0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
        0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

static uint8_t flac_crc8( const uint8_t *data, unsigned len )
{
    uint8_t crc = 0;

    while(len--)
        crc = flac_crc8_table[crc ^ *data++];

    return crc;
}

