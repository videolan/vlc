/*****************************************************************************
 * a52.c: A/52 basic parser
 *****************************************************************************
 * Copyright (C) 2001-2002 VideoLAN
 * $Id: a52.c,v 1.20 2002/12/28 02:02:18 massiot Exp $
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Michel Lespinasse <walken@zoy.org>
 *          Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
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
#include <fcntl.h>

#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include <vlc/aout.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

/*****************************************************************************
 * dec_thread_t : A52 pass-through thread descriptor
 *****************************************************************************/
typedef struct dec_thread_t
{
    /*
     * Thread properties
     */
    vlc_thread_t        thread_id;                /* id for thread functions */

    /*
     * Input properties
     */
    decoder_fifo_t *    p_fifo;                /* stores the PES stream data */
    bit_stream_t        bit_stream;

    /*
     * Output properties
     */
    aout_instance_t *   p_aout; /* opaque */
    aout_input_t *      p_aout_input; /* opaque */
    audio_sample_format_t output_format;
} dec_thread_t;

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenDecoder    ( vlc_object_t * );
static int  RunDecoder     ( decoder_fifo_t * );

static void EndThread      ( dec_thread_t * );

static int  SyncInfo       ( const byte_t *, int *, int *, int * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("A/52 parser") );
    set_capability( "decoder", 100 );
    set_callbacks( OpenDecoder, NULL );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;

    if( p_fifo->i_fourcc != VLC_FOURCC('a','5','2',' ')
         && p_fifo->i_fourcc != VLC_FOURCC('a','5','2','b') )
    {
        return VLC_EGENERIC;
    }

    p_fifo->pf_run = RunDecoder;
    return VLC_SUCCESS;
}

/****************************************************************************
 * RunDecoder: the whole thing
 ****************************************************************************
 * This function is called just after the thread is launched.
 ****************************************************************************/
static int RunDecoder( decoder_fifo_t *p_fifo )
{
    dec_thread_t * p_dec;
    audio_date_t end_date;

    /* Allocate the memory needed to store the thread's structure */
    p_dec = malloc( sizeof(dec_thread_t) );
    if( p_dec == NULL )
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return -1;
    }

    /* Initialize the thread properties */
    p_dec->p_aout = NULL;
    p_dec->p_aout_input = NULL;
    p_dec->p_fifo = p_fifo;
    p_dec->output_format.i_format = VLC_FOURCC('a','5','2',' ');

    aout_DateSet( &end_date, 0 );

    /* Init the bitstream */
    if( InitBitstream( &p_dec->bit_stream, p_dec->p_fifo,
                       NULL, NULL ) != VLC_SUCCESS )
    {
        msg_Err( p_fifo, "cannot initialize bitstream" );
        DecoderError( p_fifo );
        free( p_dec );
        return -1;
    }

    /* Decoder thread's main loop */
    while ( !p_dec->p_fifo->b_die && !p_dec->p_fifo->b_error )
    {
        int i_bit_rate;
        unsigned int i_rate, i_original_channels, i_frame_size;
        mtime_t pts;
        byte_t p_header[7];
        aout_buffer_t * p_buffer;

        /* Look for sync word - should be 0x0b77 */
        RealignBits( &p_dec->bit_stream );
        while ( (ShowBits( &p_dec->bit_stream, 16 ) ) != 0x0b77 &&
                (!p_dec->p_fifo->b_die) && (!p_dec->p_fifo->b_error) )
        {
            RemoveBits( &p_dec->bit_stream, 8 );
        }
        if ( p_dec->p_fifo->b_die || p_dec->p_fifo->b_error ) break;

        /* Set the Presentation Time Stamp */
        NextPTS( &p_dec->bit_stream, &pts, NULL );
        if ( pts != 0 && pts != aout_DateGet( &end_date ) )
        {
            aout_DateSet( &end_date, pts );
        }

        /* Get A/52 frame header */
        GetChunk( &p_dec->bit_stream, p_header, 7 );
        if ( p_dec->p_fifo->b_die || p_dec->p_fifo->b_error ) break;

        /* Check if frame is valid and get frame info */
        i_frame_size = SyncInfo( p_header, &i_original_channels, &i_rate,
                                 &i_bit_rate );

        if( !i_frame_size )
        {
            msg_Warn( p_dec->p_fifo, "a52_syncinfo failed" );
            continue;
        }

        if( (p_dec->p_aout_input != NULL) &&
            ( (p_dec->output_format.i_rate != i_rate)
                || (p_dec->output_format.i_original_channels
                      != i_original_channels)
                || (p_dec->output_format.i_bytes_per_frame != i_frame_size) ) )
        {
            /* Parameters changed - this should not happen. */
            aout_DecDelete( p_dec->p_aout, p_dec->p_aout_input );
            p_dec->p_aout_input = NULL;
        }

        /* Creating the audio input if not created yet. */
        if( p_dec->p_aout_input == NULL )
        {
            p_dec->output_format.i_rate = i_rate;
            p_dec->output_format.i_original_channels = i_original_channels;
            p_dec->output_format.i_physical_channels
                       = i_original_channels & AOUT_CHAN_PHYSMASK;
            p_dec->output_format.i_bytes_per_frame = i_frame_size;
            p_dec->output_format.i_frame_length = A52_FRAME_NB;
            aout_DateInit( &end_date, i_rate );
            p_dec->p_aout_input = aout_DecNew( p_dec->p_fifo,
                                               &p_dec->p_aout,
                                               &p_dec->output_format );

            if ( p_dec->p_aout_input == NULL )
            {
                p_dec->p_fifo->b_error = 1;
                break;
            }
        }

        if ( !aout_DateGet( &end_date ) )
        {
            byte_t p_junk[3840];

            /* We've just started the stream, wait for the first PTS. */
            GetChunk( &p_dec->bit_stream, p_junk, i_frame_size - 7 );
            continue;
        }

        p_buffer = aout_DecNewBuffer( p_dec->p_aout, p_dec->p_aout_input,
                                      A52_FRAME_NB );
        if ( p_buffer == NULL )
        {
            p_dec->p_fifo->b_error = 1;
            break;
        }
        p_buffer->start_date = aout_DateGet( &end_date );
        p_buffer->end_date = aout_DateIncrement( &end_date,
                                                 A52_FRAME_NB );

        /* Get the whole frame. */
        memcpy( p_buffer->p_buffer, p_header, 7 );
        GetChunk( &p_dec->bit_stream, p_buffer->p_buffer + 7,
                  i_frame_size - 7 );
        if( p_dec->p_fifo->b_die )
        {
            aout_DecDeleteBuffer( p_dec->p_aout, p_dec->p_aout_input,
                                  p_buffer );
            break;
        }

        /* Send the buffer to the aout core. */
        aout_DecPlay( p_dec->p_aout, p_dec->p_aout_input, p_buffer );
    }

    if( p_dec->p_fifo->b_error )
    {
        DecoderError( p_dec->p_fifo );
    }

    EndThread( p_dec );

    return 0;
}

/*****************************************************************************
 * EndThread : spdif thread destruction
 *****************************************************************************/
static void EndThread( dec_thread_t * p_dec )
{
    if ( p_dec->p_aout_input != NULL )
    {
        aout_DecDelete( p_dec->p_aout, p_dec->p_aout_input );
    }

    CloseBitstream( &p_dec->bit_stream );
    free( p_dec );
}

/*****************************************************************************
 * SyncInfo: parse A52 sync info
 *****************************************************************************
 * This code is borrowed from liba52 by Aaron Holtzman & Michel Lespinasse,
 * since we don't want to oblige S/PDIF people to use liba52 just to get
 * their SyncInfo...
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
        *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                        | AOUT_CHAN_DOLBYSTEREO;
    }
    else switch ( acmod )
    {
    case 0x0:
        /* Dual-mono = stereo + dual-mono */
        *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                        | AOUT_CHAN_DUALMONO;
        break;
    case 0x1:
        /* Mono */
        *pi_channels = AOUT_CHAN_CENTER;
        break;
    case 0x2:
        /* Stereo */
        *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
        break;
    case 0x3:
        /* 3F */
        *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER;
        break;
    case 0x4:
        /* 2F1R */
        *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARCENTER;
        break;
    case 0x5:
        /* 3F1R */
        *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
                        | AOUT_CHAN_REARCENTER;
        break;
    case 0x6:
        /* 2F2R */
        *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                        | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
        break;
    case 0x7:
        /* 3F2R */
        *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
                        | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
        break;
    default:
        return 0;
    }

    if ( p_buf[6] & lfeon[acmod] ) *pi_channels |= AOUT_CHAN_LFE;

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

