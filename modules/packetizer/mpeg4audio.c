/*****************************************************************************
 * mpeg4audio.c: parse and packetize an MPEG 4 audio stream
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: mpeg4audio.c,v 1.9 2003/10/05 18:09:36 gbazin Exp $
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
    int        i_state;
    vlc_bool_t b_synchro;

    block_t *p_chain;
    block_bytestream_t bytestream;

    /*
     * Packetizer output properties
     */
    sout_packetizer_input_t *p_sout_input;
    sout_format_t           sout_format;
    sout_buffer_t *         p_sout_buffer;            /* current sout buffer */

    /*
     * Common properties
     */
    audio_date_t          end_date;
    mtime_t pts;

    int i_frame_size, i_raw_blocks;
    unsigned int i_channels;
    unsigned int i_rate, i_frame_length;
};

enum {

    STATE_NOSYNC,
    STATE_SYNC,
    STATE_HEADER,
    STATE_NEXT_SYNC,
    STATE_DATA
};

static int i_sample_rates[] = 
{
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 
    16000, 12000, 11025, 8000,  7350,  0,     0,     0
};

#define ADTS_HEADER_SIZE 6

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int OpenPacketizer( vlc_object_t * );
static int InitPacketizer( decoder_t * );
static int RunFramePacketizer ( decoder_t *, block_t * );
static int RunADTSPacketizer ( decoder_t *, block_t * );
static int EndPacketizer ( decoder_t * );
static int GetSoutBuffer( decoder_t *, sout_buffer_t ** );

static int ADTSSyncInfo( decoder_t *, const byte_t * p_buf,
                         unsigned int * pi_channels,
                         unsigned int * pi_sample_rate,
                         unsigned int * pi_frame_length,
                         unsigned int * pi_raw_blocks );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("MPEG4 Audio packetizer") );
    set_capability( "packetizer", 50 );
    set_callbacks( OpenPacketizer, NULL );
vlc_module_end();

/*****************************************************************************
 * OpenPacketizer: probe the packetizer and return score
 *****************************************************************************/
static int OpenPacketizer( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    if( p_dec->p_fifo->i_fourcc != VLC_FOURCC( 'm', 'p', '4', 'a' ) )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
    {
        msg_Err( p_dec, "out of memory" );
        return VLC_EGENERIC;
    }

    p_dec->pf_init = InitPacketizer;
    p_dec->pf_decode = RunFramePacketizer;
    p_dec->pf_end = EndPacketizer;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * InitPacketizer: Initalize the packetizer
 *****************************************************************************/
static int InitPacketizer( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    WAVEFORMATEX *p_wf;

    p_sys->i_state = STATE_NOSYNC;
    p_sys->b_synchro = VLC_FALSE;

    aout_DateSet( &p_sys->end_date, 0 );

    p_sys->p_sout_input = NULL;
    p_sys->p_sout_buffer = NULL;
    p_sys->p_chain = NULL;

    msg_Info( p_dec, "Running MPEG4 audio packetizer" );

    p_wf = (WAVEFORMATEX*)p_dec->p_fifo->p_waveformatex;

    if( p_wf && p_wf->cbSize > 0)
    {
        uint8_t *p_config = (uint8_t*)&p_wf[1];
        int     i_index;

        i_index = ( ( p_config[0] << 1 ) | ( p_config[1] >> 7 ) ) & 0x0f;
        if( i_index != 0x0f )
        {
            p_sys->i_rate = i_sample_rates[i_index];
            p_sys->i_frame_length = (( p_config[1] >> 2 ) & 0x01) ? 960 : 1024;
        }
        else
        {
            p_sys->i_rate = ( ( p_config[1] & 0x7f ) << 17 ) |
                ( p_config[2] << 9 ) | ( p_config[3] << 1 ) |
                ( p_config[4] >> 7 );
            p_sys->i_frame_length = (( p_config[4] >> 2 ) & 0x01) ? 960 : 1024;
        }

        msg_Dbg( p_dec, "AAC %dHz %d samples/frame",
                 p_sys->i_rate, p_sys->i_frame_length );

        p_sys->i_channels = p_wf->nChannels;
        p_sys->sout_format.i_extra_data = p_wf->cbSize;
        p_sys->sout_format.p_extra_data = malloc( p_wf->cbSize );
        memcpy( p_sys->sout_format.p_extra_data, &p_wf[1], p_wf->cbSize );
    }
    else
    {
        msg_Dbg( p_dec, "No decoder specific info, must be an ADTS stream" );

        /* We will try to create a AAC Config from adts */
        p_sys->sout_format.i_extra_data = 0;
        p_sys->sout_format.p_extra_data = NULL;
        p_dec->pf_decode = RunADTSPacketizer;
    }

    return VLC_SUCCESS;
}

/****************************************************************************
 * RunFramePacketizer: the whole thing
 ****************************************************************************
 * This function must be fed with complete frames.
 ****************************************************************************/
static int RunFramePacketizer( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( !aout_DateGet( &p_sys->end_date ) && !p_block->i_pts )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( p_block );
        return VLC_SUCCESS;
    }

    p_sys->pts = p_block->i_pts;
    p_sys->i_frame_size = p_block->i_buffer;

    if( GetSoutBuffer( p_dec, &p_sys->p_sout_buffer ) != VLC_SUCCESS )
    {
        return VLC_EGENERIC;
    }

    /* Copy the whole frame into the buffer */
    p_dec->p_vlc->pf_memcpy( p_sys->p_sout_buffer->p_buffer,
                             p_block->p_buffer, p_block->i_buffer );

    sout_InputSendBuffer( p_sys->p_sout_input, p_sys->p_sout_buffer );
    p_sys->p_sout_buffer = NULL;

    block_Release( p_block );
    return VLC_SUCCESS;
}

/****************************************************************************
 * RunADTSPacketizer: the whole thing
 ****************************************************************************/
static int RunADTSPacketizer( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t p_header[ADTS_HEADER_SIZE];

    if( (!aout_DateGet( &p_sys->end_date ) && !p_block->i_pts)
        || p_block->b_discontinuity )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( p_block );
        p_sys->b_synchro = VLC_FALSE;
        return VLC_SUCCESS;
    }

    if( p_sys->p_chain )
    {
        block_ChainAppend( &p_sys->p_chain, p_block );
    }
    else
    {
        block_ChainAppend( &p_sys->p_chain, p_block );
        p_sys->bytestream = block_BytestreamInit( p_dec, p_sys->p_chain, 0 );
    }

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
                p_sys->b_synchro = VLC_FALSE;
            }
            if( p_sys->i_state != STATE_SYNC )
            {
                block_ChainRelease( p_sys->p_chain );
                p_sys->p_chain = NULL;

                /* Need more data */
                return VLC_SUCCESS;
            }

        case STATE_SYNC:
            /* New frame, set the Presentation Time Stamp */
            p_sys->pts = p_sys->bytestream.p_block->i_pts;
            if( p_sys->pts != 0 &&
                p_sys->pts != aout_DateGet( &p_sys->end_date ) )
            {
                aout_DateSet( &p_sys->end_date, p_sys->pts );
            }
            p_sys->i_state = STATE_HEADER;
            break;

        case STATE_HEADER:
            /* Get ADTS frame header (ADTS_HEADER_SIZE bytes) */
            if( block_PeekBytes( &p_sys->bytestream, p_header,
                                 ADTS_HEADER_SIZE ) != VLC_SUCCESS )
            {
                /* Need more data */
                return VLC_SUCCESS;
            }

            /* Check if frame is valid and get frame info */
            p_sys->i_frame_size = ADTSSyncInfo( p_dec, p_header,
                                                &p_sys->i_channels,
                                                &p_sys->i_rate,
                                                &p_sys->i_frame_length,
                                                &p_sys->i_raw_blocks );
            if( !p_sys->i_frame_size )
            {
                msg_Dbg( p_dec, "emulated sync word" );
                block_SkipByte( &p_sys->bytestream );
                p_sys->i_state = STATE_NOSYNC;
                p_sys->b_synchro = VLC_FALSE;
                break;
            }

            p_sys->i_state = STATE_DATA;

        case STATE_DATA:
            /* TODO: If p_block == NULL, flush the buffer without checking the
             * next sync word */

            if( !p_sys->b_synchro )
            {
                /* Check if next expected frame contains the sync word */
                if( block_PeekOffsetBytes( &p_sys->bytestream,
                                           p_sys->i_frame_size, p_header, 2 )
                    != VLC_SUCCESS )
                {
                    /* Need more data */
                    return VLC_SUCCESS;
                }

                if( p_header[0] != 0xff || (p_header[1] & 0xf6) != 0xf0 )
                {
                    msg_Dbg( p_dec, "emulated sync word "
                             "(no sync on following frame)" );
                    p_sys->i_state = STATE_NOSYNC;
                    block_SkipByte( &p_sys->bytestream );
                    p_sys->b_synchro = VLC_FALSE;
                    break;
                }
            }

            if( !p_sys->p_sout_buffer )
            if( GetSoutBuffer( p_dec, &p_sys->p_sout_buffer ) != VLC_SUCCESS )
            {
                return VLC_EGENERIC;
            }

            /* Copy the whole frame into the buffer */
            if( block_GetBytes( &p_sys->bytestream,
                                p_sys->p_sout_buffer->p_buffer,
                                p_sys->i_frame_size ) != VLC_SUCCESS )
            {
                /* Need more data */
                return VLC_SUCCESS;
            }

            p_sys->p_chain = block_BytestreamFlush( &p_sys->bytestream );

            sout_InputSendBuffer( p_sys->p_sout_input, p_sys->p_sout_buffer );

            p_sys->i_state = STATE_NOSYNC;
            p_sys->p_sout_buffer = NULL;
            p_sys->b_synchro = VLC_TRUE;

            /* Make sure we don't reuse the same pts twice */
            if( p_sys->pts == p_sys->bytestream.p_block->i_pts )
                p_sys->pts = p_sys->bytestream.p_block->i_pts = 0;
        }
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * GetSoutBuffer:
 *****************************************************************************/
static int GetSoutBuffer( decoder_t *p_dec, sout_buffer_t **pp_buffer )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_sout_input != NULL &&
        ( p_sys->sout_format.i_sample_rate != (int)p_sys->i_rate
          || p_sys->sout_format.i_channels != (int)p_sys->i_channels ) )
    {
        /* Parameters changed - this should not happen. */
    }

    /* Creating the sout input if not created yet. */
    if( p_sys->p_sout_input == NULL )
    {
        p_sys->sout_format.i_cat = AUDIO_ES;
        p_sys->sout_format.i_fourcc = VLC_FOURCC( 'm', 'p', '4', 'a' );
        p_sys->sout_format.i_sample_rate = p_sys->i_rate;
        p_sys->sout_format.i_channels    = p_sys->i_channels;
        p_sys->sout_format.i_block_align = 0;
        p_sys->sout_format.i_bitrate     = 0;

        aout_DateInit( &p_sys->end_date, p_sys->i_rate );
        aout_DateSet( &p_sys->end_date, p_sys->pts );

        p_sys->p_sout_input = sout_InputNew( p_dec, &p_sys->sout_format );
        if( p_sys->p_sout_input == NULL )
        {
            msg_Err( p_dec, "cannot add a new stream" );
            *pp_buffer = NULL;
            return VLC_EGENERIC;
        }
        msg_Info( p_dec, "AAC channels: %d samplerate: %d",
                  p_sys->i_channels, p_sys->i_rate );
    }

    *pp_buffer = sout_BufferNew( p_sys->p_sout_input->p_sout,
                                 p_sys->i_frame_size );
    if( *pp_buffer == NULL )
    {
        return VLC_EGENERIC;
    }

    (*pp_buffer)->i_pts =
        (*pp_buffer)->i_dts = aout_DateGet( &p_sys->end_date );

    (*pp_buffer)->i_length =
        aout_DateIncrement( &p_sys->end_date, p_sys->i_frame_length )
        - (*pp_buffer)->i_pts;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * EndPacketizer: clean up the packetizer
 *****************************************************************************/
static int EndPacketizer( decoder_t *p_dec )
{
    if( p_dec->p_sys->p_sout_input != NULL )
    {
        if( p_dec->p_sys->p_sout_buffer )
        {
            sout_BufferDelete( p_dec->p_sys->p_sout_input->p_sout,
                               p_dec->p_sys->p_sout_buffer );
        }

        sout_InputDelete( p_dec->p_sys->p_sout_input );
    }

    if( p_dec->p_sys->p_chain ) block_ChainRelease( p_dec->p_sys->p_chain );

    free( p_dec->p_sys );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ADTSSyncInfo: parse MPEG 4 audio ADTS sync info
 *****************************************************************************/
static int ADTSSyncInfo( decoder_t * p_dec, const byte_t * p_buf,
                         unsigned int * pi_channels,
                         unsigned int * pi_sample_rate,
                         unsigned int * pi_frame_length,
                         unsigned int * pi_raw_blocks_in_frame )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_id, i_profile, i_sample_rate_idx, i_frame_size;

    /* Fixed header between frames */
    i_id = ( (p_buf[1] >> 3) & 0x01 ) ? 2 : 4;
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
    if( !p_dec->p_sys->sout_format.i_extra_data )
    {
        p_sys->sout_format.i_extra_data = 2;
        p_sys->sout_format.p_extra_data = malloc( 2 );
        p_sys->sout_format.p_extra_data[0] =
            (i_profile + 1) << 3 | (i_sample_rate_idx > 1);
        p_sys->sout_format.p_extra_data[1] =
            ((i_sample_rate_idx & 0x01) << 7) | (*pi_channels <<3);
    }

    return i_frame_size;
}
