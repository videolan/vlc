/*****************************************************************************
 * a52.c: ATSC A/52 aka AC-3 decoder plugin for vlc.
 *   This plugin makes use of liba52 to decode A/52 audio
 *   (http://liba52.sf.net/).
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: a52.c,v 1.1 2002/08/11 01:27:01 massiot Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/decoder.h>

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */
#ifdef HAVE_STDINT_H
#   include <stdint.h>                                         /* int16_t .. */
#elif HAVE_INTTYPES_H
#   include <inttypes.h>                                       /* int16_t .. */
#endif

#ifdef USE_A52DEC_TREE                                 /* liba52 header file */
#   include "include/a52.h"
#else
#   include "a52dec/a52.h"
#endif

#define A52_FRAME_SIZE 1536 

/*****************************************************************************
 * a52_thread_t : a52 decoder thread descriptor
 *****************************************************************************/
typedef struct a52_thread_s
{
    /*
     * liba52 properties
     */
    a52_state_t *       p_a52_state;
    vlc_bool_t          b_dynrng;

    /* The bit stream structure handles the PES stream at the bit level */
    bit_stream_t        bit_stream;

    /*
     * Input properties
     */
    decoder_fifo_t *    p_fifo;                /* stores the PES stream data */
    data_packet_t *     p_data;

    /*
     * Output properties
     */
    aout_instance_t *   p_aout; /* opaque */
    aout_input_t *      p_aout_input; /* opaque */
    audio_sample_format_t output_format;
    mtime_t             last_date;
} a52_thread_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder    ( vlc_object_t * );
static int  RunDecoder     ( decoder_fifo_t * );
static int  DecodeFrame    ( a52_thread_t *, const byte_t * );
static int  InitThread     ( a52_thread_t *, decoder_fifo_t * );
static void EndThread      ( a52_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DYNRNG_TEXT N_("A/52 dynamic range compression")
#define DYNRNG_LONGTEXT N_( \
    "Dynamic range compression makes the loud sounds softer, and the soft " \
    "sounds louder, so you can more easily listen to the stream in a noisy " \
    "environment without disturbing anyone. If you disable the dynamic range "\
    "compression the playback will be more adapted to a movie theater or a " \
    "listening room.")

vlc_module_begin();
    add_category_hint( N_("Miscellaneous"), NULL );
    add_bool( "a52-dynrng", 1, NULL, DYNRNG_TEXT, DYNRNG_LONGTEXT );
    set_description( _("a52 ATSC A/52 aka AC-3 audio decoder module") );
    set_capability( "decoder", 60 );
    set_callbacks( OpenDecoder, NULL );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to choose.
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;
    
    if( p_fifo->i_fourcc != VLC_FOURCC('a','5','2',' ') )
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
    a52_thread_t *p_dec;

    /* Allocate the memory needed to store the thread's structure */
    p_dec = (a52_thread_t *)malloc( sizeof(a52_thread_t) );
    if( p_dec == NULL )
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return -1;
    }

    if( InitThread( p_dec, p_fifo ) )
    {
        msg_Err( p_dec->p_fifo, "could not initialize thread" );
        DecoderError( p_fifo );
        free( p_dec );
        return -1;
    }

    /* liba52 decoder thread's main loop */
    while( !p_dec->p_fifo->b_die && !p_dec->p_fifo->b_error )
    {
        int i_frame_size, i_flags, i_rate, i_bit_rate;
        mtime_t pts;
        /* Temporary buffer to store the raw frame to be decoded */
        byte_t p_frame_buffer[3840];

        /* Look for sync word - should be 0x0b77 */
        RealignBits( &p_dec->bit_stream );
        while( (ShowBits( &p_dec->bit_stream, 16 ) ) != 0x0b77 && 
               (!p_dec->p_fifo->b_die) && (!p_dec->p_fifo->b_error))
        {
            RemoveBits( &p_dec->bit_stream, 8 );
        }

        /* Get A/52 frame header */
        GetChunk( &p_dec->bit_stream, p_frame_buffer, 7 );
        if( p_dec->p_fifo->b_die ) break;

        /* Check if frame is valid and get frame info */
        i_frame_size = a52_syncinfo( p_frame_buffer, &i_flags, &i_rate,
                                     &i_bit_rate );

        if( !i_frame_size )
        {
            msg_Warn( p_dec->p_fifo, "a52_syncinfo failed" );
            continue;
        }

        if( (p_dec->p_aout_input != NULL) &&
            ( (p_dec->output_format.i_rate != i_rate)
               /* || (p_dec->output_format.i_channels != i_channels) */ ) )
        {
            /* Parameters changed - this should not happen. */
            aout_InputDelete( p_dec->p_aout, p_dec->p_aout_input );
            p_dec->p_aout_input = NULL;
        }

        /* Creating the audio input if not created yet. */
        if( p_dec->p_aout_input == NULL )
        {
            p_dec->output_format.i_rate = i_rate;
            /* p_dec->output_format.i_channels = i_channels; */
            p_dec->p_aout_input = aout_InputNew( p_dec->p_fifo,
                                                 &p_dec->p_aout,
                                                 &p_dec->output_format );

            if ( p_dec->p_aout_input == NULL )
            {
                p_dec->p_fifo->b_error = 1;
                break;
            }
        }

        /* Set the Presentation Time Stamp */
        CurrentPTS( &p_dec->bit_stream, &pts, NULL );
        if ( pts != 0 )
        {
            p_dec->last_date = pts;
        }

        /* Get the complete frame */
        GetChunk( &p_dec->bit_stream, p_frame_buffer + 7,
                  i_frame_size - 7 );
        if( p_dec->p_fifo->b_die ) break;

        if( DecodeFrame( p_dec, p_frame_buffer ) )
        {
            p_dec->p_fifo->b_error = 1;
            break;
        }
    }

    /* If b_error is set, the decoder thread enters the error loop */
    if( p_dec->p_fifo->b_error )
    {
        DecoderError( p_dec->p_fifo );
    }

    /* End of the a52 decoder thread */
    EndThread( p_dec );

    return 0;
}

/*****************************************************************************
 * InitThread: initialize data before entering main loop
 *****************************************************************************/
static int InitThread( a52_thread_t * p_dec, decoder_fifo_t * p_fifo )
{
    /* Initialize the thread properties */
    p_dec->p_aout = NULL;
    p_dec->p_aout_input = NULL;
    p_dec->p_fifo = p_fifo;
    p_dec->output_format.i_format = AOUT_FMT_FLOAT32;
    p_dec->output_format.i_channels = 2; /* FIXME ! */
    p_dec->last_date = 0;

    /* Initialize liba52 */
    p_dec->p_a52_state = a52_init( 0 );
    if( p_dec->p_a52_state == NULL )
    {
        msg_Err( p_dec->p_fifo, "unable to initialize liba52" );
        return -1;
    }

    p_dec->b_dynrng = config_GetInt( p_dec->p_fifo, "a52-dynrng" );

    /* Init the Bitstream */
    InitBitstream( &p_dec->bit_stream, p_dec->p_fifo,
                   NULL, NULL );

    return 0;
}

/*****************************************************************************
 * Interleave: helper function to interleave channels
 *****************************************************************************/
static void Interleave( float * p_out, const float * p_in, int i_channels )
{
    int i, j;

    for ( j = 0; j < i_channels; j++ )
    {
        for ( i = 0; i < 256; i++ )
        {
            p_out[i * i_channels + j] = p_in[j * 256 + i];
        }
    }
}

/*****************************************************************************
 * DecodeFrame: decode an ATSC A/52 frame.
 *****************************************************************************/
static int DecodeFrame( a52_thread_t * p_dec, const byte_t * p_frame_buffer )
{
    sample_t        i_sample_level = 1;
    aout_buffer_t * p_buffer;
    int             i, i_flags;
    int             i_bytes_per_block = 256 * p_dec->output_format.i_channels
                      * sizeof(float);

    if( !p_dec->last_date )
    {
        /* We've just started the stream, wait for the first PTS. */
        return 0;
    }

    p_buffer = aout_BufferNew( p_dec->p_aout, p_dec->p_aout_input,
                               A52_FRAME_SIZE );
    if ( p_buffer == NULL ) return -1;
    p_buffer->start_date = p_dec->last_date;
    p_dec->last_date += (mtime_t)(A52_FRAME_SIZE * 1000000)
                          / p_dec->output_format.i_rate;
    p_buffer->end_date = p_dec->last_date;

    /* FIXME */
    i_flags = A52_STEREO | A52_ADJUST_LEVEL;

    /* Do the actual decoding now */
    a52_frame( p_dec->p_a52_state, p_frame_buffer,
               &i_flags, &i_sample_level, 0 );

    if( !p_dec->b_dynrng )
    {
        a52_dynrng( p_dec->p_a52_state, NULL, NULL );
    }

    for ( i = 0; i < 6; i++ )
    {
        sample_t * p_samples;

        if( a52_block( p_dec->p_a52_state ) )
        {
            msg_Warn( p_dec->p_fifo, "a52_block failed for block %i", i );
        }

        p_samples = a52_samples( p_dec->p_a52_state );

        /* Interleave the *$£%ù samples */
        Interleave( (float *)(p_buffer->p_buffer + i * i_bytes_per_block),
                    p_samples, p_dec->output_format.i_channels );
    }

    aout_BufferPlay( p_dec->p_aout, p_dec->p_aout_input, p_buffer );

    return 0;
}

/*****************************************************************************
 * EndThread : liba52 decoder thread destruction
 *****************************************************************************/
static void EndThread( a52_thread_t * p_dec )
{
    if ( p_dec->p_aout_input != NULL )
    {
        aout_InputDelete( p_dec->p_aout, p_dec->p_aout_input );
    }

    a52_free( p_dec->p_a52_state );
    free( p_dec );
}

