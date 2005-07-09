/*****************************************************************************
 * mpeg4audio.c: parse and packetize an MPEG 4 audio stream
 *****************************************************************************
 * Copyright (C) 2001, 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/decoder.h>
#include <vlc/input.h>
#include <vlc/sout.h>
#include "codecs.h"

#include "vlc_block_helper.h"

/* AAC Config in ES:
 *
 * AudioObjectType          5 bits
 * samplingFrequencyIndex   4 bits
 * if (samplingFrequencyIndex == 0xF)
 *  samplingFrequency   24 bits
 * channelConfiguration     4 bits
 * GA_SpecificConfig
 *  FrameLengthFlag         1 bit 1024 or 960
 *  DependsOnCoreCoder      1 bit (always 0)
 *  ExtensionFlag           1 bit (always 0)
 */

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /*
     * Input properties
     */
    int i_state;

    block_bytestream_t bytestream;

    /*
     * Common properties
     */
    audio_date_t end_date;
    mtime_t i_pts;

    int i_frame_size, i_raw_blocks;
    unsigned int i_channels;
    unsigned int i_rate, i_frame_length, i_header_size;
};

enum {

    STATE_NOSYNC,
    STATE_SYNC,
    STATE_HEADER,
    STATE_NEXT_SYNC,
    STATE_GET_DATA,
    STATE_SEND_DATA
};

static int i_sample_rates[] =
{
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
    16000, 12000, 11025, 8000,  7350,  0,     0,     0
};

#define ADTS_HEADER_SIZE 9

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenPacketizer( vlc_object_t * );
static void ClosePacketizer( vlc_object_t * );

static block_t *PacketizeBlock    ( decoder_t *, block_t ** );
static block_t *ADTSPacketizeBlock( decoder_t *, block_t ** );

static uint8_t *GetOutBuffer ( decoder_t *, void ** );

static int ADTSSyncInfo( decoder_t *, const byte_t * p_buf,
                         unsigned int * pi_channels,
                         unsigned int * pi_sample_rate,
                         unsigned int * pi_frame_length,
                         unsigned int * pi_header_size,
                         unsigned int * pi_raw_blocks );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_PACKETIZER );
    set_description( _("MPEG4 audio packetizer") );
    set_capability( "packetizer", 50 );
    set_callbacks( OpenPacketizer, ClosePacketizer );
vlc_module_end();

/*****************************************************************************
 * OpenPacketizer: probe the packetizer and return score
 *****************************************************************************/
static int OpenPacketizer( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC( 'm', 'p', '4', 'a' ) )
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
    p_sys->i_state = STATE_NOSYNC;
    aout_DateSet( &p_sys->end_date, 0 );
    p_sys->bytestream = block_BytestreamInit( p_dec );

    /* Set output properties */
    p_dec->fmt_out.i_cat = AUDIO_ES;
    p_dec->fmt_out.i_codec = VLC_FOURCC('m','p','4','a');

    /* Set callback */
    p_dec->pf_packetize = PacketizeBlock;

    msg_Info( p_dec, "running MPEG4 audio packetizer" );

    if( p_dec->fmt_in.i_extra > 0 )
    {
        uint8_t *p_config = (uint8_t*)p_dec->fmt_in.p_extra;
        int     i_index;

        i_index = ( ( p_config[0] << 1 ) | ( p_config[1] >> 7 ) ) & 0x0f;
        if( i_index != 0x0f )
        {
            p_dec->fmt_out.audio.i_rate = i_sample_rates[i_index];
            p_dec->fmt_out.audio.i_frame_length =
                (( p_config[1] >> 2 ) & 0x01) ? 960 : 1024;
        }
        else
        {
            p_dec->fmt_out.audio.i_rate = ( ( p_config[1] & 0x7f ) << 17 ) |
                ( p_config[2] << 9 ) | ( p_config[3] << 1 ) |
                ( p_config[4] >> 7 );
            p_dec->fmt_out.audio.i_frame_length =
                (( p_config[4] >> 2 ) & 0x01) ? 960 : 1024;
        }

        msg_Dbg( p_dec, "AAC %dHz %d samples/frame",
                 p_dec->fmt_out.audio.i_rate,
                 p_dec->fmt_out.audio.i_frame_length );

        aout_DateInit( &p_sys->end_date, p_dec->fmt_out.audio.i_rate );

        p_dec->fmt_out.audio.i_channels = p_dec->fmt_in.audio.i_channels;
        p_dec->fmt_out.i_extra = p_dec->fmt_in.i_extra;
        p_dec->fmt_out.p_extra = malloc( p_dec->fmt_in.i_extra );
        memcpy( p_dec->fmt_out.p_extra, p_dec->fmt_in.p_extra,
                p_dec->fmt_in.i_extra );
    }
    else
    {
        msg_Dbg( p_dec, "no decoder specific info, must be an ADTS stream" );

        /* We will try to create a AAC Config from adts */
        p_dec->fmt_out.i_extra = 0;
        p_dec->fmt_out.p_extra = NULL;
        p_dec->pf_packetize = ADTSPacketizeBlock;
    }

    return VLC_SUCCESS;
}

/****************************************************************************
 * PacketizeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with complete frames.
 ****************************************************************************/
static block_t *PacketizeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;

    if( !pp_block || !*pp_block ) return NULL;

    p_block = *pp_block;
    *pp_block = NULL; /* Don't reuse this block */

    if( !aout_DateGet( &p_sys->end_date ) && !p_block->i_pts )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( p_block );
        return NULL;
    }
    else if( p_block->i_pts != 0 &&
             p_block->i_pts != aout_DateGet( &p_sys->end_date ) )
    {
        aout_DateSet( &p_sys->end_date, p_block->i_pts );
    }

    p_block->i_pts = p_block->i_dts = aout_DateGet( &p_sys->end_date );

    p_block->i_length = aout_DateIncrement( &p_sys->end_date,
        p_dec->fmt_out.audio.i_frame_length ) - p_block->i_pts;

    return p_block;
}

/****************************************************************************
 * DTSPacketizeBlock: the whole thing
 ****************************************************************************/
static block_t *ADTSPacketizeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t p_header[ADTS_HEADER_SIZE];
    void *p_out_buffer;
    uint8_t *p_buf;

    if( !pp_block || !*pp_block ) return NULL;

    if( !aout_DateGet( &p_sys->end_date ) && !(*pp_block)->i_pts )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( *pp_block );
        return NULL;
    }

    if( (*pp_block)->i_flags&BLOCK_FLAG_DISCONTINUITY )
    {
        p_sys->i_state = STATE_NOSYNC;
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
                /* Look for sync word - should be 0xfff + 2 layer bits */
                if( p_header[0] == 0xff && (p_header[1] & 0xf6) == 0xf0 )
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
            break;

        case STATE_HEADER:
            /* Get ADTS frame header (ADTS_HEADER_SIZE bytes) */
            if( block_PeekBytes( &p_sys->bytestream, p_header,
                                 ADTS_HEADER_SIZE ) != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }

            /* Check if frame is valid and get frame info */
            p_sys->i_frame_size = ADTSSyncInfo( p_dec, p_header,
                                                &p_sys->i_channels,
                                                &p_sys->i_rate,
                                                &p_sys->i_frame_length,
                                                &p_sys->i_header_size,
                                                &p_sys->i_raw_blocks );
            if( p_sys->i_frame_size <= 0 )
            {
                msg_Dbg( p_dec, "emulated sync word" );
                block_SkipByte( &p_sys->bytestream );
                p_sys->i_state = STATE_NOSYNC;
                break;
            }

            p_sys->i_state = STATE_NEXT_SYNC;

        case STATE_NEXT_SYNC:
            /* TODO: If p_block == NULL, flush the buffer without checking the
             * next sync word */

            /* Check if next expected frame contains the sync word */
            if( block_PeekOffsetBytes( &p_sys->bytestream, p_sys->i_frame_size
                                       + p_sys->i_header_size, p_header, 2 )
                != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }

            if( p_header[0] != 0xff || (p_header[1] & 0xf6) != 0xf0 )
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
            if( block_WaitBytes( &p_sys->bytestream, p_sys->i_frame_size +
                                 p_sys->i_header_size) != VLC_SUCCESS )
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

            /* When we reach this point we already know we have enough
             * data available. */

            /* Skip the ADTS header */
            block_SkipBytes( &p_sys->bytestream, p_sys->i_header_size );

            /* Copy the whole frame into the buffer */
            block_GetBytes( &p_sys->bytestream, p_buf, p_sys->i_frame_size );

            /* Make sure we don't reuse the same pts twice */
            if( p_sys->i_pts == p_sys->bytestream.p_block->i_pts )
                p_sys->i_pts = p_sys->bytestream.p_block->i_pts = 0;

            /* So p_block doesn't get re-added several times */
            *pp_block = block_BytestreamPop( &p_sys->bytestream );

            p_sys->i_state = STATE_NOSYNC;

            return p_out_buffer;
        }
    }

    return NULL;
}

/*****************************************************************************
 * GetOutBuffer:
 *****************************************************************************/
static uint8_t *GetOutBuffer( decoder_t *p_dec, void **pp_out_buffer )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;

    if( p_dec->fmt_out.audio.i_rate != p_sys->i_rate )
    {
        msg_Info( p_dec, "AAC channels: %d samplerate: %d",
                  p_sys->i_channels, p_sys->i_rate );

        aout_DateInit( &p_sys->end_date, p_sys->i_rate );
        aout_DateSet( &p_sys->end_date, p_sys->i_pts );
    }

    p_dec->fmt_out.audio.i_rate     = p_sys->i_rate;
    p_dec->fmt_out.audio.i_channels = p_sys->i_channels;
    p_dec->fmt_out.audio.i_bytes_per_frame = p_sys->i_frame_size;
    p_dec->fmt_out.audio.i_frame_length = p_sys->i_frame_length;

#if 0
    p_dec->fmt_out.audio.i_original_channels = p_sys->i_channels_conf;
    p_dec->fmt_out.audio.i_physical_channels =
        p_sys->i_channels_conf & AOUT_CHAN_PHYSMASK;
#endif

    p_block = block_New( p_dec, p_sys->i_frame_size );
    if( p_block == NULL ) return NULL;

    p_block->i_pts = p_block->i_dts = aout_DateGet( &p_sys->end_date );

    p_block->i_length = aout_DateIncrement( &p_sys->end_date,
                            p_sys->i_frame_length ) - p_block->i_pts;

    *pp_out_buffer = p_block;
    return p_block->p_buffer;
}

/*****************************************************************************
 * ClosePacketizer: clean up the packetizer
 *****************************************************************************/
static void ClosePacketizer( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_BytestreamRelease( &p_sys->bytestream );

    free( p_dec->p_sys );
}

/*****************************************************************************
 * ADTSSyncInfo: parse MPEG 4 audio ADTS sync info
 *****************************************************************************/
static int ADTSSyncInfo( decoder_t * p_dec, const byte_t * p_buf,
                         unsigned int * pi_channels,
                         unsigned int * pi_sample_rate,
                         unsigned int * pi_frame_length,
                         unsigned int * pi_header_size,
                         unsigned int * pi_raw_blocks_in_frame )
{
    int i_id, i_profile, i_sample_rate_idx, i_frame_size;
    vlc_bool_t b_crc;

    /* Fixed header between frames */
    i_id = ( (p_buf[1] >> 3) & 0x01 ) ? 2 : 4;
    b_crc = !(p_buf[1] & 0x01);
    i_profile = p_buf[2] >> 6;
    i_sample_rate_idx = (p_buf[2] >> 2) & 0x0f;
    *pi_sample_rate = i_sample_rates[i_sample_rate_idx];
    *pi_channels = ((p_buf[2] & 0x01) << 2) | ((p_buf[3] >> 6) & 0x03);

    /* Variable header */
    i_frame_size = ((p_buf[3] & 0x03) << 11) | (p_buf[4] << 3) |
                   ((p_buf[5] >> 5) & 0x7);
    *pi_raw_blocks_in_frame = (p_buf[6] & 0x02) + 1;

    if( !*pi_sample_rate || !*pi_channels || !i_frame_size )
    {
        return 0;
    }

    /* Fixme */
    *pi_frame_length = 1024;

    /* Build the decoder specific info header */
    if( !p_dec->fmt_out.i_extra )
    {
        p_dec->fmt_out.i_extra = 2;
        p_dec->fmt_out.p_extra = malloc( 2 );
        ((uint8_t *)p_dec->fmt_out.p_extra)[0] =
            (i_profile + 1) << 3 | (i_sample_rate_idx >> 1);
        ((uint8_t *)p_dec->fmt_out.p_extra)[1] =
            ((i_sample_rate_idx & 0x01) << 7) | (*pi_channels <<3);
    }

    /* ADTS header length */
    *pi_header_size = b_crc ? 9 : 7;

    return i_frame_size - *pi_header_size;
}
