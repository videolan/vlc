/*****************************************************************************
 * lpcm.c: lpcm decoder module
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: lpcm.c,v 1.8 2002/12/28 02:02:18 massiot Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Henri Fallon <henri@videolan.org>
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
#include <string.h>                                    /* memcpy(), memset() */

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/decoder.h>
#include <input_ext-dec.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>                                           /* getpid() */
#endif

/* DVD PES size (2048) - 40 bytes (headers) */
#define LPCM_FRAME_LENGTH 2008

/*****************************************************************************
 * dec_thread_t : lpcm decoder thread descriptor
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
    int                 sync_ptr;         /* sync ptr from lpcm magic header */

    /*
     * Output properties
     */
    aout_instance_t        *p_aout;
    aout_input_t           *p_aout_input;
    audio_sample_format_t   output_format;
    audio_date_t            end_date;
} dec_thread_t;

/*
 * LPCM header :
 * - PES header
 * - private stream ID (16 bits) == 0xA0 -> not in the bitstream
 * - frame number (8 bits)
 * - unknown (16 bits) == 0x0003 ?
 * - unknown (4 bits)
 * - current frame (4 bits)
 * - unknown (2 bits)
 * - frequency (2 bits) 0 == 48 kHz, 1 == 32 kHz, 2 == ?, 3 == ?
 * - unknown (1 bit)
 * - number of channels - 1 (3 bits) 1 == 2 channels
 * - start code (8 bits) == 0x80
 */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder    ( vlc_object_t * );
static int  RunDecoder     ( decoder_fifo_t * );

static void DecodeFrame    ( dec_thread_t * );
static void EndThread      ( dec_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("linear PCM audio parser") );
    set_capability( "decoder", 100 );
    set_callbacks( OpenDecoder, NULL );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;

    if( p_fifo->i_fourcc != VLC_FOURCC('l','p','c','m')
         && p_fifo->i_fourcc != VLC_FOURCC('l','p','c','b') )
    {   
        return VLC_EGENERIC;
    }
    
    p_fifo->pf_run = RunDecoder;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * RunDecoder: the lpcm decoder
 *****************************************************************************/
static int RunDecoder( decoder_fifo_t * p_fifo )
{
    dec_thread_t *   p_dec;

    /* Allocate the memory needed to store the thread's structure */
    if( (p_dec = (dec_thread_t *)malloc (sizeof(dec_thread_t)) )
            == NULL) 
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return -1;
    }

    /* Initialize the thread properties */
    p_dec->p_fifo = p_fifo;

    /* Init the bitstream */
    if( InitBitstream( &p_dec->bit_stream, p_dec->p_fifo,
                       NULL, NULL ) != VLC_SUCCESS )
    {
        msg_Err( p_fifo, "cannot initialize bitstream" );
        DecoderError( p_fifo );
        EndThread( p_dec );
        return -1;
    }
   
    p_dec->output_format.i_format = VLC_FOURCC('s','1','6','b');
    p_dec->p_aout = NULL;
    p_dec->p_aout_input = NULL;

    /* LPCM decoder thread's main loop */
    while ( (!p_dec->p_fifo->b_die) && (!p_dec->p_fifo->b_error) )
    {
        DecodeFrame(p_dec);
    }

    /* If b_error is set, the lpcm decoder thread enters the error loop */
    if ( p_dec->p_fifo->b_error )
    {
        DecoderError( p_dec->p_fifo );
    }

    /* End of the lpcm decoder thread */
    EndThread( p_dec );

    return 0;
}

/*****************************************************************************
 * DecodeFrame: decodes a frame.
 *****************************************************************************/
static void DecodeFrame( dec_thread_t * p_dec )
{
    aout_buffer_t *    p_buffer;
    mtime_t            i_pts;
    uint8_t            i_header;
    unsigned int       i_rate, i_original_channels;

    /* Look for sync word - should be 0xXX80 */
    RealignBits( &p_dec->bit_stream );
    while ( (ShowBits( &p_dec->bit_stream, 16 ) & 0xc8ff) != 0x0080 && 
             (!p_dec->p_fifo->b_die) && (!p_dec->p_fifo->b_error) )
    {
        RemoveBits( &p_dec->bit_stream, 8 );
    }
    if ( p_dec->p_fifo->b_die || p_dec->p_fifo->b_error ) return;

    NextPTS( &p_dec->bit_stream, &i_pts, NULL );
    if( i_pts != 0 && i_pts != aout_DateGet( &p_dec->end_date ) )
    {
        aout_DateSet( &p_dec->end_date, i_pts );
    }
    
    /* Get LPCM header. */
    i_header = GetBits( &p_dec->bit_stream, 16 ) >> 8;

    switch ( i_header >> 4 )
    {
    case 0:
        i_rate = 48000;
        break;
    case 1:
        i_rate = 32000;
        break;
    default:
        msg_Err( p_dec->p_fifo, "unsupported LPCM rate (0x%x)", i_header );
        p_dec->p_fifo->b_error = 1;
        return;
    }

    switch ( i_header & 0x7 )
    {
    case 0:
        i_original_channels = AOUT_CHAN_CENTER;
        break;
    case 1:
        i_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
        break;
    case 3:
        i_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
        break;
    case 5:
        i_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                               | AOUT_CHAN_CENTER | AOUT_CHAN_LFE;
        break;
    case 2:
    case 4:
    case 6:
    case 7:
    default:
        msg_Err( p_dec->p_fifo, "unsupported LPCM channels (0x%x)",
                 i_header );
        p_dec->p_fifo->b_error = 1;
        return;
    }

    if( (p_dec->p_aout_input != NULL) &&
        ( (p_dec->output_format.i_rate != i_rate)
            || (p_dec->output_format.i_original_channels
                  != i_original_channels) ) )
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
        aout_DateInit( &p_dec->end_date, i_rate );
        p_dec->p_aout_input = aout_DecNew( p_dec->p_fifo,
                                           &p_dec->p_aout,
                                           &p_dec->output_format );

        if ( p_dec->p_aout_input == NULL )
        {
            p_dec->p_fifo->b_error = 1;
            return;
        }
    }

    if ( !aout_DateGet( &p_dec->end_date ) )
    {
        byte_t p_junk[LPCM_FRAME_LENGTH];

        /* We've just started the stream, wait for the first PTS. */
        GetChunk( &p_dec->bit_stream, p_junk, LPCM_FRAME_LENGTH );
        return;
    }

    p_buffer = aout_DecNewBuffer( p_dec->p_aout, p_dec->p_aout_input,
            LPCM_FRAME_LENGTH / p_dec->output_format.i_bytes_per_frame );
    
    if( p_buffer == NULL )
    {
        msg_Err( p_dec->p_fifo, "cannot get aout buffer" );
        p_dec->p_fifo->b_error = 1;
        return;
    }
    p_buffer->start_date = aout_DateGet( &p_dec->end_date );
    p_buffer->end_date = aout_DateIncrement( &p_dec->end_date,
            LPCM_FRAME_LENGTH / p_dec->output_format.i_bytes_per_frame );

    /* Get the whole frame. */
    GetChunk( &p_dec->bit_stream, p_buffer->p_buffer, 
              LPCM_FRAME_LENGTH);
    if( p_dec->p_fifo->b_die )
    {
        aout_DecDeleteBuffer( p_dec->p_aout, p_dec->p_aout_input,
                              p_buffer );
        return;
    }

    /* Send the buffer to the aout core. */
    aout_DecPlay( p_dec->p_aout, p_dec->p_aout_input, p_buffer );
}

/*****************************************************************************
 * EndThread : lpcm decoder thread destruction
 *****************************************************************************/
static void EndThread( dec_thread_t * p_dec )
{
    if( p_dec->p_aout_input != NULL )
    {
        aout_DecDelete( p_dec->p_aout, p_dec->p_aout_input );
    }

    CloseBitstream( &p_dec->bit_stream );
    free( p_dec );
}
