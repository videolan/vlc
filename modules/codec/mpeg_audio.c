/*****************************************************************************
 * mpeg_audio.c: parse MPEG audio sync info and packetize the stream
 *****************************************************************************
 * Copyright (C) 2001-2003 VideoLAN
 * $Id: mpeg_audio.c,v 1.10 2003/02/16 08:56:24 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

#define MAX_FRAME_SIZE 10000
/* This isn't the place to put mad-specific stuff. However, it makes the
 * mad plug-in's life much easier if we put 8 extra bytes at the end of the
 * buffer, because that way it doesn't have to copy the aout_buffer_t to a
 * bigger buffer. This has no implication on other plug-ins, and we only
 * lose 8 bytes per frame. --Meuuh */
#define MAD_BUFFER_GUARD 8


/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  Open           ( vlc_object_t * );
static int  RunDecoder     ( decoder_fifo_t * );

static void EndThread      ( dec_thread_t * );

static int SyncInfo( uint32_t i_header, unsigned int * pi_channels,
                     unsigned int * pi_sample_rate, unsigned int * pi_bit_rate,
                     unsigned int * pi_frame_length,
                     unsigned int * pi_current_frame_length,
                     unsigned int * pi_layer );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("MPEG audio layer I/II/III parser") );
    set_capability( "decoder", 100 );
    set_callbacks( Open, NULL );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;

    if( p_fifo->i_fourcc != VLC_FOURCC( 'm', 'p', 'g', 'a') )
    {
        return VLC_EGENERIC;
    }

    p_fifo->pf_run = RunDecoder;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * RunDecoder: this function is called just after the thread is created
 *****************************************************************************/
static int RunDecoder( decoder_fifo_t *p_fifo )
{
    dec_thread_t * p_dec;
    audio_date_t end_date;
    unsigned int i_layer = 0;
    byte_t p_sync[MAD_BUFFER_GUARD];
    mtime_t pts;
    int i_free_frame_size = 0;

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

    /* Init sync buffer. */
    NextPTS( &p_dec->bit_stream, &pts, NULL );
    GetChunk( &p_dec->bit_stream, p_sync, MAD_BUFFER_GUARD );

    /* Decoder thread's main loop */
    while ( !p_dec->p_fifo->b_die && !p_dec->p_fifo->b_error )
    {
        int i_current_frame_size;
        unsigned int i_rate, i_original_channels, i_frame_size, i_frame_length;
        unsigned int i_new_layer, i_bit_rate;
        uint32_t i_header;
        aout_buffer_t * p_buffer;
        int i;

        /* Look for sync word - should be 0xffe */
        if ( (p_sync[0] != 0xff) || ((p_sync[1] & 0xe0) != 0xe0) )
        {
            msg_Warn( p_dec->p_fifo, "no sync - skipping" );
            /* Look inside the sync buffer. */
            for ( i = 1; i < MAD_BUFFER_GUARD - 1; i++ )
            {
                if ( (p_sync[i] == 0xff) && ((p_sync[i + 1] & 0xe0) != 0xe0) )
                    break;
            }
            if ( i < MAD_BUFFER_GUARD - 1 )
            {
                /* Found it ! */
                memmove( p_sync, &p_sync[i], MAD_BUFFER_GUARD - i );
                GetChunk( &p_dec->bit_stream, &p_sync[MAD_BUFFER_GUARD - i],
                          i );
            }
            else
            {
                if ( p_sync[MAD_BUFFER_GUARD - 1] == 0xff
                      && ShowBits( &p_dec->bit_stream, 3 ) == 0x3 )
                {
                    /* Found it ! */
                    p_sync[0] = p_sync[MAD_BUFFER_GUARD - 1];
                    GetChunk( &p_dec->bit_stream,
                              &p_sync[1], MAD_BUFFER_GUARD - 1 );
                }
                else
                {
                    /* Scan the stream. */
                    while ( ShowBits( &p_dec->bit_stream, 11 ) != 0x07ff &&
                            (!p_dec->p_fifo->b_die) &&
                            (!p_dec->p_fifo->b_error) )
                    {
                        RemoveBits( &p_dec->bit_stream, 8 );
                    }
                    if ( p_dec->p_fifo->b_die || p_dec->p_fifo->b_error )
                        break;
                    NextPTS( &p_dec->bit_stream, &pts, NULL );
                    GetChunk( &p_dec->bit_stream,p_sync, MAD_BUFFER_GUARD );
                }
            }
        }
        if ( p_dec->p_fifo->b_die || p_dec->p_fifo->b_error ) break;

        /* Set the Presentation Time Stamp */
        if ( pts != 0 && pts != aout_DateGet( &end_date ) )
        {
            aout_DateSet( &end_date, pts );
        }

        /* Get frame header */
        i_header = (p_sync[0] << 24) | (p_sync[1] << 16) | (p_sync[2] << 8)
                     | p_sync[3];

        /* Check if frame is valid and get frame info */
        i_current_frame_size = SyncInfo( i_header,
                                         &i_original_channels, &i_rate,
                                         &i_bit_rate, &i_frame_length,
                                         &i_frame_size, &i_new_layer );
        if ( !i_current_frame_size )
            i_current_frame_size = i_free_frame_size;

        if ( i_current_frame_size == -1 )
        {
            msg_Warn( p_dec->p_fifo, "syncinfo failed" );
            /* This is probably an emulated startcode, drop the first byte
             * to force looking for the next startcode. */
            memmove( p_sync, &p_sync[1], MAD_BUFFER_GUARD - 1 );
            p_sync[MAD_BUFFER_GUARD - 1] = GetBits( &p_dec->bit_stream, 8 );
            continue;
        }
        if ( (unsigned int)i_current_frame_size > i_frame_size )
        {
            msg_Warn( p_dec->p_fifo, "frame too big %d > %d",
                      i_current_frame_size, i_frame_size );
            memmove( p_sync, &p_sync[1], MAD_BUFFER_GUARD - 1 );
            p_sync[MAD_BUFFER_GUARD - 1] = GetBits( &p_dec->bit_stream, 8 );
            continue;
        }

        if( (p_dec->p_aout_input != NULL) &&
            ( (p_dec->output_format.i_rate != i_rate)
                || (p_dec->output_format.i_original_channels
                      != i_original_channels)
                || (p_dec->output_format.i_bytes_per_frame
                      != i_frame_size + MAD_BUFFER_GUARD)
                || (p_dec->output_format.i_frame_length != i_frame_length)
                || (i_layer != i_new_layer) ) )
        {
            /* Parameters changed - this should not happen. */
            aout_DecDelete( p_dec->p_aout, p_dec->p_aout_input );
            p_dec->p_aout_input = NULL;
        }

        /* Creating the audio input if not created yet. */
        if( p_dec->p_aout_input == NULL )
        {
            i_layer = i_new_layer;
            if ( i_layer == 3 )
            {
                p_dec->output_format.i_format = VLC_FOURCC('m','p','g','3');
            }
            else
            {
                p_dec->output_format.i_format = VLC_FOURCC('m','p','g','a');
            }
            p_dec->output_format.i_rate = i_rate;
            p_dec->output_format.i_original_channels = i_original_channels;
            p_dec->output_format.i_physical_channels
                       = i_original_channels & AOUT_CHAN_PHYSMASK;
            p_dec->output_format.i_bytes_per_frame = i_frame_size
                                                        + MAD_BUFFER_GUARD;
            p_dec->output_format.i_frame_length = i_frame_length;
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
            byte_t p_junk[MAX_FRAME_SIZE];
            int    i_skip = i_current_frame_size - MAD_BUFFER_GUARD;

            /* We've just started the stream, wait for the first PTS. */
            while( i_skip > 0 )
            {
                int i_read;

                i_read = __MIN( i_current_frame_size  - MAD_BUFFER_GUARD, MAX_FRAME_SIZE );
                GetChunk( &p_dec->bit_stream, p_junk, i_read );

                i_skip -= i_read;
            }
            NextPTS( &p_dec->bit_stream, &pts, NULL );
            GetChunk( &p_dec->bit_stream, p_sync, MAD_BUFFER_GUARD );
            continue;
        }

        p_buffer = aout_DecNewBuffer( p_dec->p_aout, p_dec->p_aout_input,
                                      i_frame_length );
        if ( p_buffer == NULL )
        {
            p_dec->p_fifo->b_error = 1;
            break;
        }
        p_buffer->start_date = aout_DateGet( &end_date );
        p_buffer->end_date = aout_DateIncrement( &end_date,
                                                 i_frame_length );

        /* Get the whole frame. */
        memcpy( p_buffer->p_buffer, p_sync, MAD_BUFFER_GUARD );
        if ( i_current_frame_size )
        {
            GetChunk( &p_dec->bit_stream, p_buffer->p_buffer + MAD_BUFFER_GUARD,
                      i_current_frame_size - MAD_BUFFER_GUARD );
        }
        else
        {
            /* Free bit-rate stream. Peek at next frame header. */
            i = MAD_BUFFER_GUARD;
            for ( ; ; )
            {
                int i_next_real_frame_size;
                unsigned int i_next_channels, i_next_rate, i_next_bit_rate;
                unsigned int i_next_frame_length, i_next_frame_size;
                unsigned int i_next_layer;
                while ( ShowBits( &p_dec->bit_stream, 11 ) != 0x07ff &&
                        (!p_dec->p_fifo->b_die) &&
                        (!p_dec->p_fifo->b_error) &&
                        i < (int)i_frame_size + MAD_BUFFER_GUARD )
                {
                    ((uint8_t *)p_buffer->p_buffer)[i++] =
                                GetBits( &p_dec->bit_stream, 8 );
                }
                if ( p_dec->p_fifo->b_die || p_dec->p_fifo->b_error
                      || i == (int)i_frame_size + MAD_BUFFER_GUARD )
                    break;
                i_header = ShowBits( &p_dec->bit_stream, 8 );
                i_next_real_frame_size = SyncInfo( i_header,
                                         &i_next_channels, &i_next_rate,
                                         &i_next_bit_rate, &i_next_frame_length,
                                         &i_next_frame_size, &i_next_layer );
                if ( i_next_real_frame_size != 0 ||
                     i_next_channels != i_original_channels ||
                     i_next_rate != i_rate ||
                     i_next_bit_rate != i_bit_rate ||
                     i_next_frame_length != i_frame_length ||
                     i_next_frame_size != i_frame_size ||
                     i_next_layer != i_new_layer )
                {
                    /* This is an emulated start code, try again. */
                    continue;
                }
                i_free_frame_size = i;
                break;
            }
            if ( p_dec->p_fifo->b_die || p_dec->p_fifo->b_error )
                break;
            if ( i == (int)i_frame_size + MAD_BUFFER_GUARD )
            {
                /* Couldn't find the next start-code. This is sooo
                 * embarassing. */
                aout_DecDeleteBuffer( p_dec->p_aout, p_dec->p_aout_input,
                                      p_buffer );
                NextPTS( &p_dec->bit_stream, &pts, NULL );
                GetChunk( &p_dec->bit_stream, p_sync, MAD_BUFFER_GUARD );
                continue;
            }
        }
        if( p_dec->p_fifo->b_die )
        {
            aout_DecDeleteBuffer( p_dec->p_aout, p_dec->p_aout_input,
                                  p_buffer );
            break;
        }
        /* Get beginning of next frame. */
        NextPTS( &p_dec->bit_stream, &pts, NULL );
        GetChunk( &p_dec->bit_stream, p_buffer->p_buffer + i_current_frame_size,
                  MAD_BUFFER_GUARD );
        memcpy( p_sync, p_buffer->p_buffer + i_current_frame_size,
                MAD_BUFFER_GUARD );

        p_buffer->i_nb_bytes = i_current_frame_size + MAD_BUFFER_GUARD;

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
 * SyncInfo: parse MPEG audio sync info
 *****************************************************************************/
static int SyncInfo( uint32_t i_header, unsigned int * pi_channels,
                     unsigned int * pi_sample_rate, unsigned int * pi_bit_rate,
                     unsigned int * pi_frame_length,
                     unsigned int * pi_frame_size, unsigned int * pi_layer )
{
    static const int pppi_mpegaudio_bitrate[2][3][16] =
    {
        {
            /* v1 l1 */
            { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384,
              416, 448, 0},
            /* v1 l2 */
            { 0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256,
              320, 384, 0},
            /* v1 l3 */
            { 0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224,
              256, 320, 0}
        },

        {
            /* v2 l1 */
            { 0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192,
              224, 256, 0},
            /* v2 l2 */
            { 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128,
              144, 160, 0},
            /* v2 l3 */
            { 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128,
              144, 160, 0}
        }
    };

    static const int ppi_mpegaudio_samplerate[2][4] = /* version 1 then 2 */
    {
        { 44100, 48000, 32000, 0 },
        { 22050, 24000, 16000, 0 }
    };

    int i_version, i_mode, i_emphasis;
    vlc_bool_t b_padding, b_mpeg_2_5;
    int i_current_frame_size = 0;
    int i_bitrate_index, i_samplerate_index;
    int i_max_bit_rate;

    b_mpeg_2_5  = 1 - ((i_header & 0x100000) >> 20);
    i_version   = 1 - ((i_header & 0x80000) >> 19);
    *pi_layer   = 4 - ((i_header & 0x60000) >> 17);
    /* CRC */
    i_bitrate_index = (i_header & 0xf000) >> 12;
    i_samplerate_index = (i_header & 0xc00) >> 10;
    b_padding   = (i_header & 0x200) >> 9;
    /* Extension */
    i_mode      = (i_header & 0xc0) >> 6;
    /* Modeext, copyright & original */
    i_emphasis  = i_header & 0x3;

    if( *pi_layer != 4 &&
        i_bitrate_index < 0x0f &&
        i_samplerate_index != 0x03 &&
        i_emphasis != 0x02 )
    {
        switch ( i_mode )
        {
        case 0: /* stereo */
        case 1: /* joint stereo */
            *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
            break;
        case 2: /* dual-mono */
            *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                            | AOUT_CHAN_DUALMONO;
            break;
        case 3: /* mono */
            *pi_channels = AOUT_CHAN_CENTER;
            break;
        }
        *pi_bit_rate = pppi_mpegaudio_bitrate[i_version][*pi_layer-1][i_bitrate_index];
        i_max_bit_rate = pppi_mpegaudio_bitrate[i_version][*pi_layer-1][14];
        *pi_sample_rate = ppi_mpegaudio_samplerate[i_version][i_samplerate_index];

        if ( b_mpeg_2_5 )
        {
            *pi_sample_rate >>= 1;
        }

        switch( *pi_layer )
        {
        case 1:
            i_current_frame_size = ( 12000 *
                                        *pi_bit_rate / *pi_sample_rate
                                        + b_padding ) * 4;
            *pi_frame_size = ( 12000 *
                                  i_max_bit_rate / *pi_sample_rate + 1 ) * 4;
            *pi_frame_length = 384;
            break;

        case 2:
            i_current_frame_size = 144000 *
                                      *pi_bit_rate / *pi_sample_rate
                                      + b_padding;
            *pi_frame_size = 144000 * i_max_bit_rate / *pi_sample_rate + 1;
            *pi_frame_length = 1152;
            break;

        case 3:
            i_current_frame_size = 144000 *
                                      *pi_bit_rate / *pi_sample_rate
                                      + b_padding;
            *pi_frame_size = 144000 * i_max_bit_rate / *pi_sample_rate + 1;
            *pi_frame_length = i_version ? 576 : 1152;
            break;

        default:
            break;
        }
    }
    else
    {
        return -1;
    }

    return i_current_frame_size;
}

