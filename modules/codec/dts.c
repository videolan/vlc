/*****************************************************************************
 * dts.c: DTS basic parser
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: dts.c,v 1.1 2003/03/09 20:07:47 jlj Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
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
 * dec_thread_t : decoder thread descriptor
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

static int  SyncInfo       ( const byte_t *, unsigned int *,
                             unsigned int *, unsigned int *,
                             unsigned int * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("DTS parser") );
    set_capability( "decoder", 100 );
    set_callbacks( OpenDecoder, NULL );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;

    if( p_fifo->i_fourcc != VLC_FOURCC('d','t','s',' ')
         && p_fifo->i_fourcc != VLC_FOURCC('d','t','s','b') )
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
    p_dec->output_format.i_format = VLC_FOURCC('d','t','s',' ');

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
    while( !p_dec->p_fifo->b_die && !p_dec->p_fifo->b_error )
    {
        int i;
        mtime_t pts;
        byte_t p_header[10];

        unsigned int i_rate;
        unsigned int i_bit_rate;
        unsigned int i_frame_size;
        unsigned int i_frame_length;
        unsigned int i_original_channels;

        aout_buffer_t * p_buffer = NULL;

        for( i = 0; i < 3; i++ )
        {
            RealignBits( &p_dec->bit_stream );
            while( (ShowBits( &p_dec->bit_stream, 32 ) ) != 0x7ffe8001 &&
                   (!p_dec->p_fifo->b_die) && (!p_dec->p_fifo->b_error) )
            {
                RemoveBits( &p_dec->bit_stream, 8 );
            }
            if( p_dec->p_fifo->b_die || p_dec->p_fifo->b_error ) break;

            if( i == 0 )
            {
                /* Set the Presentation Time Stamp */
                NextPTS( &p_dec->bit_stream, &pts, NULL );
                if( pts != 0 && pts != aout_DateGet( &end_date ) )
                {
                    aout_DateSet( &end_date, pts );
                }
            }

            /* Get DTS frame header */
            GetChunk( &p_dec->bit_stream, p_header, 10 );
            if( p_dec->p_fifo->b_die || p_dec->p_fifo->b_error ) break;

            i_frame_size = SyncInfo( p_header, &i_original_channels, &i_rate,
                                               &i_bit_rate, &i_frame_length );
            if( !i_frame_size )
            {
                msg_Warn( p_dec->p_fifo, "dts_syncinfo failed" );
                i--; continue;
            }

            if( i == 0 )
            {
                if( (p_dec->p_aout_input != NULL) &&
                    ( (p_dec->output_format.i_rate != i_rate)
                        || (p_dec->output_format.i_original_channels
                            != i_original_channels)
                        || (p_dec->output_format.i_bytes_per_frame 
                            != i_frame_size * 3) ) )
                {
                    /* Parameters changed - this should not happen. */
                    aout_DecDelete( p_dec->p_aout, p_dec->p_aout_input );
                    p_dec->p_aout_input = NULL;
                }

                /* Creating the audio input if not created yet. */
                if( p_dec->p_aout_input == NULL )
                {
                    p_dec->output_format.i_rate = i_rate;
                    p_dec->output_format.i_original_channels 
                                = i_original_channels;
                    p_dec->output_format.i_physical_channels
                                = i_original_channels & AOUT_CHAN_PHYSMASK;
                    p_dec->output_format.i_bytes_per_frame = i_frame_size * 3;
                    p_dec->output_format.i_frame_length = i_frame_length * 3;
                    aout_DateInit( &end_date, i_rate );
                    p_dec->p_aout_input = aout_DecNew( p_dec->p_fifo,
                                                       &p_dec->p_aout,
                                                       &p_dec->output_format );

                    if( p_dec->p_aout_input == NULL )
                    {
                        p_dec->p_fifo->b_error = 1;
                        break;
                    }
                }
            }

            if( !aout_DateGet( &end_date ) )
            {
                byte_t p_junk[ i_frame_size ];

                /* We've just started the stream, wait for the first PTS. */
                GetChunk( &p_dec->bit_stream, p_junk, i_frame_size - 10 );
                i--; continue;
            }

            if( i == 0 )
            {
                p_buffer = aout_DecNewBuffer( p_dec->p_aout, 
                                              p_dec->p_aout_input,
                                              i_frame_length * 3 );
                if( p_buffer == NULL )
                {
                    p_dec->p_fifo->b_error = 1;
                    break;
                }
                p_buffer->start_date = aout_DateGet( &end_date );
                p_buffer->end_date = aout_DateIncrement( &end_date,
                                                         i_frame_length * 3 );
            }

            /* Get the whole frame. */
            memcpy( p_buffer->p_buffer + (i * i_frame_size), p_header, 10 );
            GetChunk( &p_dec->bit_stream, 
                      p_buffer->p_buffer + (i * i_frame_size) + 10,
                      i_frame_size - 10 );
            if( p_dec->p_fifo->b_die ) break;
        }

        if( p_dec->p_fifo->b_die || p_dec->p_fifo->b_error )
        {
            if( p_buffer != NULL )
            {
                aout_DecDeleteBuffer( p_dec->p_aout, p_dec->p_aout_input,
                                      p_buffer );
            }

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
 * EndThread : thread destruction
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
 * SyncInfo: parse DTS sync info
 *****************************************************************************/
static int SyncInfo( const byte_t * p_buf, unsigned int * pi_channels,
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

    if( ((uint32_t*)p_buf)[0] != 0x7ffe8001 )
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
            *pi_channels = AOUT_CHAN_CENTER;
            break;
        case 0x1:
            /* Dual-mono = stereo + dual-mono */
            *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                           AOUT_CHAN_DUALMONO;
            break;
        case 0x2:
        case 0x3:
        case 0x4:
            /* Stereo */
            *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
            break;
        case 0x5:
            /* 3F */
            *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER;
            break;
        case 0x6:
            /* 2F/LFE */
            *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_LFE;
            break;
        case 0x7:
            /* 3F/LFE */
            *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                           AOUT_CHAN_CENTER | AOUT_CHAN_LFE;
            break;
        case 0x8:
            /* 2F2R */
            *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                           AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
            break;
        case 0x9:
            /* 3F2R */
            *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                           AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT |
                           AOUT_CHAN_REARRIGHT;
            break;
        case 0xA:
        case 0xB:
            /* 2F2M2R */
            *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                           AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT |
                           AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
            break;
        case 0xC:
            /* 3F2M2R */
            *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                           AOUT_CHAN_CENTER | AOUT_CHAN_MIDDLELEFT |
                           AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_REARLEFT |
                           AOUT_CHAN_REARRIGHT;
            break;
        case 0xD:
        case 0xE:
            /* 3F2M2R/LFE */
            *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                           AOUT_CHAN_CENTER | AOUT_CHAN_MIDDLELEFT |
                           AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_REARLEFT |
                           AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE;
            break;

        default:
            return( 0 );
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
