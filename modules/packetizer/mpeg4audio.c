/*****************************************************************************
 * mpeg4audio.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: mpeg4audio.c,v 1.4 2003/03/31 03:46:11 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/decoder.h>
#include <vlc/input.h>
#include <vlc/sout.h>
#include "codecs.h"
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

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

typedef struct packetizer_thread_s
{
    /* Input properties */
    int                     b_adts;

    decoder_fifo_t          *p_fifo;
    bit_stream_t            bit_stream;

    /* Output properties */
    sout_packetizer_input_t *p_sout_input;
    sout_packet_format_t    output_format;

//    mtime_t                 i_pts_start;
    mtime_t                 i_last_pts;

    WAVEFORMATEX            *p_wf;

    /* Extracted from AAC config */
    int                     i_sample_rate;
    int                     i_frame_size;   // 1024 or 960

} packetizer_thread_t;

static int  Open    ( vlc_object_t * );
static int  Run     ( decoder_fifo_t * );

static int  InitThread           ( packetizer_thread_t * );
static void PacketizeThreadMPEG4 ( packetizer_thread_t * );
static void PacketizeThreadADTS  ( packetizer_thread_t * );
static void EndThread            ( packetizer_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    set_description( _("MPEG4 Audio packetizer") );
    set_capability( "packetizer", 50 );
    set_callbacks( Open, NULL );
vlc_module_end();


/*****************************************************************************
 * OpenDecoder: probe the packetizer and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to choose.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;

    p_fifo->pf_run = Run;

    if( p_fifo->i_fourcc == VLC_FOURCC( 'm', 'p', '4', 'a') )
    {
        return( VLC_SUCCESS );
    }
    else
    {
        return( VLC_EGENERIC );
    }
}

/*****************************************************************************
 * RunDecoder: this function is called just after the thread is created
 *****************************************************************************/
static int Run( decoder_fifo_t *p_fifo )
{
    packetizer_thread_t *p_pack;
    int b_error;

    msg_Info( p_fifo, "Running MPEG4 audio packetizer" );
    if( !( p_pack = malloc( sizeof( packetizer_thread_t ) ) ) )
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return( -1 );
    }
    memset( p_pack, 0, sizeof( packetizer_thread_t ) );

    p_pack->p_fifo = p_fifo;

    if( InitThread( p_pack ) != 0 )
    {
        DecoderError( p_fifo );
        return( -1 );
    }

    while( ( !p_pack->p_fifo->b_die )&&( !p_pack->p_fifo->b_error ) )
    {
        if( p_pack->b_adts )
        {
            PacketizeThreadADTS( p_pack );
        }
        else
        {
            PacketizeThreadMPEG4( p_pack );
        }
    }


    if( ( b_error = p_pack->p_fifo->b_error ) )
    {
        DecoderError( p_pack->p_fifo );
    }

    EndThread( p_pack );
    if( b_error )
    {
        return( -1 );
    }

    return( 0 );
}


#define FREE( p ) if( p ) free( p ); p = NULL

/*****************************************************************************
 * InitThread: initialize data before entering main loop
 *****************************************************************************/
static int i_sample_rates[] = 
{
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 
    16000, 12000, 11025, 8000,  7350,  0,     0,     0
};

static int InitThread( packetizer_thread_t *p_pack )
{
    WAVEFORMATEX    *p_wf;

    p_wf = (WAVEFORMATEX*)p_pack->p_fifo->p_waveformatex;

    if( p_wf && p_wf->cbSize > 0)
    {
        uint8_t *p_config = (uint8_t*)&p_wf[1];
        int i_wf = sizeof( WAVEFORMATEX ) + p_wf->cbSize;
        int i_index;


        p_pack->p_wf = malloc( i_wf );
        memcpy( p_pack->p_wf,
                p_wf,
                i_wf );
        p_pack->output_format.i_cat = AUDIO_ES;
        p_pack->output_format.i_fourcc = VLC_FOURCC( 'm', 'p', '4', 'a' );
        p_pack->output_format.p_format = p_pack->p_wf;
        p_pack->b_adts = 0;

        i_index = ( ( p_config[0] << 1 ) | ( p_config[1] >> 7 ) )&0x0f;
        if( i_index != 0x0f )
        {
            p_pack->i_sample_rate = i_sample_rates[i_index];
            p_pack->i_frame_size  = ( ( p_config[1] >> 2 )&0x01 ) ? 960 : 1024;
        }
        else
        {
            p_pack->i_sample_rate = ( ( p_config[1]&0x7f ) << 17 ) | ( p_config[2] << 9 )| 
                                      ( p_config[3] << 1 ) | ( p_config[4] >> 7 );
            p_pack->i_frame_size  = ( ( p_config[4] >> 2 )&0x01 ) ? 960 : 1024;
        }
        msg_Dbg( p_pack->p_fifo,
                 "aac %dHz %d samples/frame",
                 p_pack->i_sample_rate,
                 p_pack->i_frame_size );
    }
    else
    {
        /* we will try to create a AAC Config from adts */
        p_pack->output_format.i_cat = UNKNOWN_ES;
        p_pack->output_format.i_fourcc = VLC_FOURCC( 'n', 'u', 'l', 'l' );
        p_pack->b_adts = 1;

        if( InitBitstream( &p_pack->bit_stream, p_pack->p_fifo,
               NULL, NULL ) != VLC_SUCCESS )
        {
            msg_Err( p_pack->p_fifo, "cannot initialize bitstream" );
            return -1;
        }

    }

    p_pack->p_sout_input =
        sout_InputNew( p_pack->p_fifo,
                       &p_pack->output_format );

    if( !p_pack->p_sout_input )
    {
        msg_Err( p_pack->p_fifo,
                 "cannot add a new stream" );
        return( -1 );
    }

    p_pack->i_last_pts = 0;
    return( 0 );
}

/*****************************************************************************
 * PacketizeThread: packetize an unit (here copy a complete pes)
 *****************************************************************************/
static void PacketizeThreadMPEG4( packetizer_thread_t *p_pack )
{
    sout_buffer_t   *p_sout_buffer;
    pes_packet_t    *p_pes;
    ssize_t         i_size;
    mtime_t         i_pts;

    /* **** get samples count **** */
    input_ExtractPES( p_pack->p_fifo, &p_pes );
    if( !p_pes )
    {
        p_pack->p_fifo->b_error = 1;
        return;
    }
#if 0
    if( p_pack->i_pts_start < 0 && p_pes->i_pts > 0 )
    {
        p_pack->i_pts_start = p_pes->i_pts;
    }
    p_pack->i_pts = p_pes->i_pts - p_pack->i_pts_start;
#endif

    i_pts = p_pes->i_pts;

    if( i_pts <= 0 && p_pack->i_last_pts <= 0 )
    {
        msg_Dbg( p_pack->p_fifo, "need a starting pts" );
        input_DeletePES( p_pack->p_fifo->p_packets_mgt, p_pes );
        return;
    }
    i_size = p_pes->i_pes_size;

    if( i_size > 0 )
    {
        data_packet_t   *p_data;
        ssize_t          i_buffer;

        p_sout_buffer = 
            sout_BufferNew( p_pack->p_sout_input->p_sout, i_size );
        if( !p_sout_buffer )
        {
            p_pack->p_fifo->b_error = 1;
            input_DeletePES( p_pack->p_fifo->p_packets_mgt, p_pes );
            return;
        }
        /* TODO: memcpy of the pes packet */
        for( i_buffer = 0, p_data = p_pes->p_first;
             p_data != NULL && i_buffer < i_size;
             p_data = p_data->p_next)
        {
            size_t          i_copy;

            i_copy = __MIN( p_data->p_payload_end - p_data->p_payload_start,
                            i_size - i_buffer );
            if( i_copy > 0 )
            {
                p_pack->p_fifo->p_vlc->pf_memcpy( p_sout_buffer->p_buffer + i_buffer,
                                                  p_data->p_payload_start,
                                                  i_copy );
            }
            i_buffer += i_copy;
        }

        if( i_pts <= 0 )
        {
            i_pts = p_pack->i_last_pts +
                        (mtime_t)1000000 *
                        (mtime_t)p_pack->i_frame_size /
                        (mtime_t)p_pack->i_sample_rate;
        }
        p_pack->i_last_pts = i_pts;

        p_sout_buffer->i_length = (mtime_t)1000000 *
                                  (mtime_t)p_pack->i_frame_size /
                                  (mtime_t)p_pack->i_sample_rate;
        p_sout_buffer->i_bitrate = 0;
        p_sout_buffer->i_dts = i_pts;
        p_sout_buffer->i_pts = i_pts;

        sout_InputSendBuffer( p_pack->p_sout_input,
                               p_sout_buffer );
    }

    input_DeletePES( p_pack->p_fifo->p_packets_mgt, p_pes );
}


static void PacketizeThreadADTS( packetizer_thread_t *p_pack )
{
    msg_Err( p_pack->p_fifo, "adts stream unsupported" );
    p_pack->p_fifo->b_error = 1;
}

/*****************************************************************************
 * EndThread : packetizer thread destruction
 *****************************************************************************/
static void EndThread ( packetizer_thread_t *p_pack)
{
    if( p_pack->p_sout_input )
    {
        sout_InputDelete( p_pack->p_sout_input );
    }
    if( p_pack->p_wf )
    {
        free( p_pack->p_wf );
    }
}

