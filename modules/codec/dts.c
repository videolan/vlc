/*****************************************************************************
 * dts.c: parse DTS audio sync info and packetize the stream
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: dts.c,v 1.5 2003/11/16 21:07:30 gbazin Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
 * Preamble
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>                                              /* memcpy() */

#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include <vlc/input.h>

#include "vlc_block_helper.h"

#define DTS_HEADER_SIZE 10

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /* Module mode */
    vlc_bool_t b_packetizer;

    /*
     * Input properties
     */
    int i_state;

    block_bytestream_t bytestream;

    /*
     * Common properties
     */
    audio_date_t   end_date;

    mtime_t i_pts;

    int i_frame_size, i_bit_rate;
    unsigned int i_frame_length, i_rate, i_channels, i_channels_conf;

    /* This is very hacky. For DTS over S/PDIF we apparently need to send
     * 3 frames at a time. This should likely be moved to the output stage. */
    int i_frames_in_buf;
    aout_buffer_t *p_aout_buffer;        /* current aout buffer being filled */

};

enum {

    STATE_NOSYNC,
    STATE_SYNC,
    STATE_HEADER,
    STATE_NEXT_SYNC,
    STATE_GET_DATA,
    STATE_SEND_DATA
};

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static int  OpenPacketizer( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );
static void *DecodeBlock  ( decoder_t *, block_t ** );

static int  SyncInfo      ( const byte_t *, unsigned int *, unsigned int *,
                            unsigned int *, unsigned int *, unsigned int * );

static uint8_t       *GetOutBuffer ( decoder_t *, void ** );
static aout_buffer_t *GetAoutBuffer( decoder_t * );
static block_t       *GetSoutBuffer( decoder_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("DTS parser") );
    set_capability( "decoder", 100 );
    set_callbacks( OpenDecoder, CloseDecoder );

    add_submodule();
    set_description( _("DTS audio packetizer") );
    set_capability( "packetizer", 10 );
    set_callbacks( OpenPacketizer, NULL );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('d','t','s',' ')
         && p_dec->fmt_in.i_codec != VLC_FOURCC('d','t','s','b') )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
    {
        msg_Err( p_dec, "out of memory" );
        return VLC_EGENERIC;
    }

    /* Misc init */
    p_sys->b_packetizer = VLC_FALSE;
    p_sys->i_state = STATE_NOSYNC;
    aout_DateSet( &p_sys->end_date, 0 );
    p_sys->i_frames_in_buf = 0;

    p_sys->bytestream = block_BytestreamInit( p_dec );

    /* Set output properties */
    p_dec->fmt_out.i_cat = AUDIO_ES;
    p_dec->fmt_out.i_codec = VLC_FOURCC('d','t','s',' ');

    /* Set callback */
    p_dec->pf_decode_audio = (aout_buffer_t *(*)(decoder_t *, block_t **))
        DecodeBlock;
    p_dec->pf_packetize    = (block_t *(*)(decoder_t *, block_t **))
        DecodeBlock;

    return VLC_SUCCESS;
}

static int OpenPacketizer( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    int i_ret = OpenDecoder( p_this );

    if( i_ret == VLC_SUCCESS ) p_dec->p_sys->b_packetizer = VLC_TRUE;

    return i_ret;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function is called just after the thread is launched.
 ****************************************************************************/
static void *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t p_header[DTS_HEADER_SIZE];
    uint8_t *p_buf;
    void *p_out_buffer;

    if( !pp_block || !*pp_block ) return NULL;

    if( !aout_DateGet( &p_sys->end_date ) && !(*pp_block)->i_pts )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( *pp_block );
        return NULL;
    }

    if( (*pp_block)->b_discontinuity )
    {
        p_sys->i_state = STATE_NOSYNC;
    }

    block_BytestreamPush( &p_sys->bytestream, *pp_block );

    while( 1 )
    {
        switch( p_sys->i_state )
        {
        case STATE_NOSYNC:
            /* Look for sync dword - should be 0x7ffe8001 */
            while( block_PeekBytes( &p_sys->bytestream, p_header, 4 )
                   == VLC_SUCCESS )
            {
                if( p_header[0] == 0x7f && p_header[1] == 0xfe &&
                    p_header[2] == 0x80 && p_header[3] == 0x01 )
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
            if( p_sys->i_pts != 0 &&
                p_sys->i_pts != aout_DateGet( &p_sys->end_date ) )
            {
                aout_DateSet( &p_sys->end_date, p_sys->i_pts );
            }
            p_sys->i_state = STATE_HEADER;

        case STATE_HEADER:
            /* Get DTS frame header (DTS_HEADER_SIZE bytes) */
            if( block_PeekBytes( &p_sys->bytestream, p_header,
                                 DTS_HEADER_SIZE ) != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }

            /* Check if frame is valid and get frame info */
            p_sys->i_frame_size = SyncInfo( p_header,
                                            &p_sys->i_channels,
                                            &p_sys->i_channels_conf,
                                            &p_sys->i_rate,
                                            &p_sys->i_bit_rate,
                                            &p_sys->i_frame_length );
            if( !p_sys->i_frame_size )
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
                                       p_sys->i_frame_size, p_header, 4 )
                != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }

            if( p_header[0] == 0x7f && p_header[1] == 0xfe &&
                p_header[2] == 0x80 && p_header[3] == 0x01 )
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
                                 p_sys->i_frame_size ) != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }
            p_sys->i_state = STATE_SEND_DATA;

        case STATE_SEND_DATA:
            if( !(p_buf = GetOutBuffer( p_dec, &p_out_buffer )) )
            {
                //p_dec->b_error = VLC_TRUE;
                return NULL;
            }

            /* Copy the whole frame into the buffer. When we reach this point
             * we already know we have enough data available. */
            block_GetBytes( &p_sys->bytestream, p_buf, p_sys->i_frame_size );

            /* Make sure we don't reuse the same pts twice */
            if( p_sys->i_pts == p_sys->bytestream.p_block->i_pts )
                p_sys->i_pts = p_sys->bytestream.p_block->i_pts = 0;

            /* So p_block doesn't get re-added several times */
            *pp_block = block_BytestreamPop( &p_sys->bytestream );

            p_sys->i_state = STATE_NOSYNC;

            if( !p_sys->b_packetizer )
            {
                if( p_sys->i_frames_in_buf != 3 ) return NULL;
                else
                {
                    p_sys->i_frames_in_buf = 0;
                    p_sys->p_aout_buffer = 0;
                }
            }

            return p_out_buffer;
        }
    }

    return NULL;
}

/*****************************************************************************
 * CloseDecoder: clean up the decoder
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_BytestreamRelease( &p_sys->bytestream );

    free( p_sys );
}

/*****************************************************************************
 * GetOutBuffer:
 *****************************************************************************/
static uint8_t *GetOutBuffer( decoder_t *p_dec, void **pp_out_buffer )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t *p_buf;

    if( p_dec->fmt_out.audio.i_rate != p_sys->i_rate )
    {
        msg_Info( p_dec, "DTS channels:%d samplerate:%d bitrate:%d",
                  p_sys->i_channels, p_sys->i_rate, p_sys->i_bit_rate );

        aout_DateInit( &p_sys->end_date, p_sys->i_rate );
        aout_DateSet( &p_sys->end_date, p_sys->i_pts );
    }

    p_dec->fmt_out.audio.i_rate     = p_sys->i_rate;
    p_dec->fmt_out.audio.i_channels = p_sys->i_channels;
    p_dec->fmt_out.audio.i_bitrate  = p_sys->i_bit_rate;
    p_dec->fmt_out.audio.i_bytes_per_frame = p_sys->i_frame_size;
    p_dec->fmt_out.audio.i_frame_length = p_sys->i_frame_length;

    p_dec->fmt_out.audio.i_original_channels = p_sys->i_channels_conf;
    p_dec->fmt_out.audio.i_physical_channels =
        p_sys->i_channels_conf & AOUT_CHAN_PHYSMASK;

    if( p_sys->b_packetizer )
    {
        block_t *p_sout_buffer = GetSoutBuffer( p_dec );
        p_buf = p_sout_buffer ? p_sout_buffer->p_buffer : NULL;
        *pp_out_buffer = p_sout_buffer;
    }
    else
    {
        if( !p_sys->i_frames_in_buf )
        {
            p_sys->p_aout_buffer = GetAoutBuffer( p_dec );
        }
        p_buf = p_sys->p_aout_buffer ? p_sys->p_aout_buffer->p_buffer +
            p_sys->i_frames_in_buf * p_sys->i_frame_size : NULL;
        *pp_out_buffer = p_sys->p_aout_buffer;
    }

    p_sys->i_frames_in_buf++;

    return p_buf;
}

/*****************************************************************************
 * GetAoutBuffer:
 *****************************************************************************/
static aout_buffer_t *GetAoutBuffer( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    aout_buffer_t *p_buf;

    p_buf = p_dec->pf_aout_buffer_new( p_dec, p_sys->i_frame_length * 3 );
    if( p_buf == NULL ) return NULL;

    p_buf->start_date = aout_DateGet( &p_sys->end_date );
    p_buf->end_date =
        aout_DateIncrement( &p_sys->end_date, p_sys->i_frame_length * 3 );

    return p_buf;
}

/*****************************************************************************
 * GetSoutBuffer:
 *****************************************************************************/
static block_t *GetSoutBuffer( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;

    p_block = block_New( p_dec, p_sys->i_frame_size );
    if( p_block == NULL ) return NULL;

    p_block->i_pts = p_block->i_dts = aout_DateGet( &p_sys->end_date );

    p_block->i_length = aout_DateIncrement( &p_sys->end_date,
        p_sys->i_frame_length ) - p_block->i_pts;

    return p_block;
}

/*****************************************************************************
 * SyncInfo: parse DTS sync info
 *****************************************************************************/
static int SyncInfo( const byte_t * p_buf,
                     unsigned int * pi_channels,
                     unsigned int * pi_channels_conf,
                     unsigned int * pi_sample_rate,
                     unsigned int * pi_bit_rate,
                     unsigned int * pi_frame_length )
{
    unsigned int i_bit_rate;
    unsigned int i_audio_mode;
    unsigned int i_sample_rate;
    unsigned int i_frame_size;
    unsigned int i_frame_length;

    static const unsigned int ppi_dts_samplerate[] =
    {
        0, 8000, 16000, 32000, 64000, 128000,
        11025, 22050, 44010, 88020, 176400,
        12000, 24000, 48000, 96000, 192000
    };

    static const unsigned int ppi_dts_bitrate[] =
    {
        32000, 56000, 64000, 96000, 112000, 128000,
        192000, 224000, 256000, 320000, 384000,
        448000, 512000, 576000, 640000, 768000,
        896000, 1024000, 1152000, 1280000, 1344000,
        1408000, 1411200, 1472000, 1536000, 1920000,
        2048000, 3072000, 3840000, 4096000, 0, 0
    };

    if( (p_buf[0] != 0x7f) || (p_buf[1] != 0xfe) ||
        (p_buf[2] != 0x80) || (p_buf[3] != 0x01) )
    {
        return( 0 );
    }

    i_frame_length = (p_buf[4] & 0x01) << 6 | (p_buf[5] >> 2);
    i_frame_size = (p_buf[5] & 0x03) << 12 | (p_buf[6] << 4) |
                   (p_buf[7] >> 4);

    i_audio_mode = (p_buf[7] & 0x0f) << 2 | (p_buf[8] >> 6);
    i_sample_rate = (p_buf[8] >> 2) & 0x0f;
    i_bit_rate = (p_buf[8] & 0x03) << 3 | ((p_buf[9] >> 5) & 0x07);

    switch( i_audio_mode )
    {
        case 0x0:
            /* Mono */
            *pi_channels_conf = AOUT_CHAN_CENTER;
            break;
        case 0x1:
            /* Dual-mono = stereo + dual-mono */
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                           AOUT_CHAN_DUALMONO;
            break;
        case 0x2:
        case 0x3:
        case 0x4:
            /* Stereo */
            *pi_channels = 2;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
            break;
        case 0x5:
            /* 3F */
            *pi_channels = 3;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_CENTER;
            break;
        case 0x6:
            /* 2F/LFE */
            *pi_channels = 3;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_LFE;
            break;
        case 0x7:
            /* 3F/LFE */
            *pi_channels = 4;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_CENTER | AOUT_CHAN_LFE;
            break;
        case 0x8:
            /* 2F2R */
            *pi_channels = 4;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
            break;
        case 0x9:
            /* 3F2R */
            *pi_channels = 5;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT |
                                AOUT_CHAN_REARRIGHT;
            break;
        case 0xA:
        case 0xB:
            /* 2F2M2R */
            *pi_channels = 6;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT |
                                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
            break;
        case 0xC:
            /* 3F2M2R */
            *pi_channels = 7;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_CENTER | AOUT_CHAN_MIDDLELEFT |
                                AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_REARLEFT |
                                AOUT_CHAN_REARRIGHT;
            break;
        case 0xD:
        case 0xE:
            /* 3F2M2R/LFE */
            *pi_channels = 8;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_CENTER | AOUT_CHAN_MIDDLELEFT |
                                AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_REARLEFT |
                                AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE;
            break;

        default:
            if( i_audio_mode <= 63 )
            {
                /* User defined */
                *pi_channels = 0;
                *pi_channels_conf = 0; 
            }
            else
            {
                return( 0 );
            }
            break;
    }

    if( i_sample_rate >= sizeof( ppi_dts_samplerate ) /
                         sizeof( ppi_dts_samplerate[0] ) )
    {
        return( 0 );
    }

    *pi_sample_rate = ppi_dts_samplerate[ i_sample_rate ];

    if( i_bit_rate >= sizeof( ppi_dts_bitrate ) /
                      sizeof( ppi_dts_bitrate[0] ) )
    {
        return( 0 );
    }

    *pi_bit_rate = ppi_dts_bitrate[ i_bit_rate ];

    *pi_frame_length = (i_frame_length + 1) * 32;

    return( i_frame_size + 1 );
}
