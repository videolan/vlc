/*****************************************************************************
 * a52.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: a52.c,v 1.5 2003/05/03 02:09:41 fenrir Exp $
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
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/decoder.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */
#include "codecs.h"                         /* WAVEFORMATEX BITMAPINFOHEADER */
/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct packetizer_s
{
    /* Input properties */
    decoder_fifo_t          *p_fifo;
    bit_stream_t            bit_stream;

    /* Output properties */
    sout_packetizer_input_t *p_sout_input;
    sout_format_t           output_format;

    uint64_t                i_samplescount;

    mtime_t                 i_last_pts;
} packetizer_t;

static int  Open    ( vlc_object_t * );
static int  Run     ( decoder_fifo_t * );

static int  InitThread     ( packetizer_t * );
static void PacketizeThread   ( packetizer_t * );
static void EndThread      ( packetizer_t * );

static int SyncInfo( const byte_t *, int *, int *, int * );

#define FREE( p ) if( p ) free( p ); p = NULL

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    set_description( _("A/52 audio packetizer") );
    set_capability( "packetizer", 10 );
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

    if( p_fifo->i_fourcc != VLC_FOURCC( 'a', '5', '2', ' ') &&
        p_fifo->i_fourcc != VLC_FOURCC( 'a', '5', '2', 'b') )
    {
        return VLC_EGENERIC;
    }

    p_fifo->pf_run = Run;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * RunDecoder: this function is called just after the thread is created
 *****************************************************************************/
static int Run( decoder_fifo_t *p_fifo )
{
    packetizer_t *p_pack;
    int b_error;

    msg_Info( p_fifo, "Running A/52 packetizer" );
    if( !( p_pack = malloc( sizeof( packetizer_t ) ) ) )
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return( -1 );
    }
    memset( p_pack, 0, sizeof( packetizer_t ) );

    p_pack->p_fifo = p_fifo;

    if( InitThread( p_pack ) != 0 )
    {
        DecoderError( p_fifo );
        return( -1 );
    }

    while( ( !p_pack->p_fifo->b_die )&&( !p_pack->p_fifo->b_error ) )
    {
        PacketizeThread( p_pack );
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



/*****************************************************************************
 * InitThread: initialize data before entering main loop
 *****************************************************************************/

static int InitThread( packetizer_t *p_pack )
{

    p_pack->output_format.i_cat = AUDIO_ES;
    p_pack->output_format.i_fourcc = VLC_FOURCC( 'a', '5', '2', ' ' );

    p_pack->p_sout_input   = NULL;

    p_pack->i_samplescount = 0;
    p_pack->i_last_pts     = 0;

    if( InitBitstream( &p_pack->bit_stream, p_pack->p_fifo,
                       NULL, NULL ) != VLC_SUCCESS )
    {
        msg_Err( p_pack->p_fifo, "cannot initialize bitstream" );
        return -1;
    }

    return( 0 );
}

/*****************************************************************************
 * PacketizeThread: packetize an unit (here copy a complete pes)
 *****************************************************************************/
static void PacketizeThread( packetizer_t *p_pack )
{
    sout_buffer_t   *p_sout_buffer;


    uint8_t p_header[7];
    int     i_channels, i_samplerate, i_bitrate;
    int     i_framelength;
    mtime_t i_pts;

    /* search a valid start code */
    for( ;; )
    {
        int i_skip = 0;

        RealignBits( &p_pack->bit_stream );
        while( ShowBits( &p_pack->bit_stream, 16 ) != 0x0b77 &&
               !p_pack->p_fifo->b_die && !p_pack->p_fifo->b_error )
        {
            RemoveBits( &p_pack->bit_stream, 8 );
            i_skip++;
        }
        if( i_skip )
        {
            msg_Warn( p_pack->p_fifo, "trashing %d bytes", i_skip );
        }
        if( p_pack->p_fifo->b_die || p_pack->p_fifo->b_error )
        {
            return;
        }

        /* Set the Presentation Time Stamp */
        NextPTS( &p_pack->bit_stream, &i_pts, NULL );

        GetChunk( &p_pack->bit_stream, p_header, 7 );
        if( p_pack->p_fifo->b_die ) return;

        /* Check if frame is valid and get frame info */
        i_framelength = SyncInfo( p_header, &i_channels, &i_samplerate,
                                  &i_bitrate );

        if( !i_framelength )
        {
            msg_Warn( p_pack->p_fifo, "invalid header found" );
            continue;
        }
        else
        {
//            msg_Dbg( p_pack->p_fifo, "frame length %d b", i_framelength );
            break;
        }
    }

    if( !p_pack->p_sout_input )
    {
        p_pack->output_format.i_sample_rate = i_samplerate;
        p_pack->output_format.i_channels    = i_channels;
        p_pack->output_format.i_block_align = 0;
        p_pack->output_format.i_bitrate     = 0;
        p_pack->output_format.i_extra_data  = 0;
        p_pack->output_format.p_extra_data  = NULL;

        p_pack->p_sout_input =
            sout_InputNew( p_pack->p_fifo,
                           &p_pack->output_format );

        if( !p_pack->p_sout_input )
        {
            msg_Err( p_pack->p_fifo,
                     "cannot add a new stream" );
            p_pack->p_fifo->b_error = 1;
            return;
        }
        msg_Info( p_pack->p_fifo,
                 "A/52 channels:%d samplerate:%d bitrate:%d",
                 i_channels, i_samplerate, i_bitrate );
    }

    if( i_pts <= 0 && p_pack->i_last_pts <= 0 )
    {
        msg_Dbg( p_pack->p_fifo, "need a starting pts" );
        return;
    }

    /* fix pts */
    if( i_pts <= 0 )
    {
        i_pts = p_pack->i_last_pts +
                    (uint64_t)1000000 *
                    (uint64_t)A52_FRAME_NB /
                    (uint64_t)i_samplerate;
    }
    p_pack->i_last_pts = i_pts;

    p_sout_buffer =
        sout_BufferNew( p_pack->p_sout_input->p_sout, i_framelength );
    if( !p_sout_buffer )
    {
        p_pack->p_fifo->b_error = 1;
        return;
    }
    memcpy( p_sout_buffer->p_buffer, p_header, 7 );
    p_sout_buffer->i_bitrate = i_bitrate;

    p_sout_buffer->i_pts =
        p_sout_buffer->i_dts = i_pts;

    p_sout_buffer->i_length =
            (uint64_t)1000000 *
            (uint64_t)A52_FRAME_NB /
            (uint64_t)i_samplerate;

    p_pack->i_samplescount += A52_FRAME_NB;

    /* we are already aligned */
    GetChunk( &p_pack->bit_stream,
              p_sout_buffer->p_buffer + 7,
              i_framelength - 7 );

    sout_InputSendBuffer( p_pack->p_sout_input,
                          p_sout_buffer );
}


/*****************************************************************************
 * EndThread : packetizer thread destruction
 *****************************************************************************/
static void EndThread ( packetizer_t *p_pack)
{
    if( p_pack->p_sout_input )
    {
        sout_InputDelete( p_pack->p_sout_input );
    }
    free( p_pack );
}

/*****************************************************************************
 * SyncInfo: parse A52 sync info
 *****************************************************************************
 * This code is borrowed from liba52 by Aaron Holtzman & Michel Lespinasse,
 * since we don't want to oblige S/PDIF people to use liba52 just to get
 * their SyncInfo...
 * fenrir: I've change it to report channels count
 *****************************************************************************/
static int SyncInfo( const byte_t * p_buf, int * pi_channels,
                     int * pi_sample_rate, int * pi_bit_rate )
{
    static const uint8_t halfrate[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3 };
    static const int rate[] = { 32,  40,  48,  56,  64,  80,  96, 112,
                                128, 160, 192, 224, 256, 320, 384, 448,
                                512, 576, 640 };
    static const uint8_t lfeon[8] = { 0x10, 0x10, 0x04, 0x04,
                                      0x04, 0x01, 0x04, 0x01 };
    static const int acmod_to_channels[8] = { 2, 1, 2, 3, 3, 4, 4, 5 };
    int frmsizecod;
    int bitrate;
    int half;
    int acmod;

    if ((p_buf[0] != 0x0b) || (p_buf[1] != 0x77))        /* syncword */
        return 0;

    if (p_buf[5] >= 0x60)                /* bsid >= 12 */
        return 0;
    half = halfrate[p_buf[5] >> 3];

    /* acmod, dsurmod and lfeon */
    acmod = p_buf[6] >> 5;
    if ( (p_buf[6] & 0xf8) == 0x50 )
    {
        /* Dolby surround = stereo + Dolby */
/*        *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                        | AOUT_CHAN_DOLBYSTEREO;*/
        *pi_channels = 2; /* FIXME ???  */
    }
    else
    {
        *pi_channels = acmod_to_channels[acmod&0x07];
    }

    if ( p_buf[6] & lfeon[acmod] ) *pi_channels += 1;//|= AOUT_CHAN_LFEA;

    frmsizecod = p_buf[4] & 63;
    if (frmsizecod >= 38)
        return 0;
    bitrate = rate [frmsizecod >> 1];
    *pi_bit_rate = (bitrate * 1000) >> half;

    switch (p_buf[4] & 0xc0) {
    case 0:
        *pi_sample_rate = 48000 >> half;
        return 4 * bitrate;
    case 0x40:
        *pi_sample_rate = 44100 >> half;
        return 2 * (320 * bitrate / 147 + (frmsizecod & 1));
    case 0x80:
        *pi_sample_rate = 32000 >> half;
        return 6 * bitrate;
    default:
        return 0;
    }
}


