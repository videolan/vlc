/*****************************************************************************
 * lpcm.c: lpcm decoder module
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: lpcm.c,v 1.14 2003/03/18 01:22:13 sam Exp $
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

/*****************************************************************************
 * dec_thread_t : lpcm decoder thread descriptor
 *****************************************************************************/
typedef struct dec_thread_t
{
    /*
     * Input properties
     */
    decoder_fifo_t *    p_fifo;                /* stores the PES stream data */

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

#define LPCM_HEADER_LEN 6

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
    if( (p_dec = (dec_thread_t *)malloc( sizeof(dec_thread_t)) )
            == NULL) 
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return -1;
    }

    /* Initialize the thread properties */
    p_dec->p_fifo = p_fifo;

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
    pes_packet_t *     p_pes;
    data_packet_t *    p_data;
    aout_buffer_t *    p_buffer;
    void *             p_dest;
    mtime_t            i_pts;
    uint8_t            i_header;
    unsigned int       i_rate = 0, i_original_channels = 0, i_size;
    int                i;

    input_ExtractPES( p_dec->p_fifo, &p_pes );
    if ( !p_pes )
    {
        p_dec->p_fifo->b_error = 1;
        return;
    }

    /* Compute the size of the PES - i_pes_size includes the PES header. */
    p_data = p_pes->p_first;
    i_size = 0;
    while ( p_data != NULL )
    {
        i_size += p_data->p_payload_end - p_data->p_payload_start;
        p_data = p_data->p_next;
    }
    if ( i_size < LPCM_HEADER_LEN )
    {
        msg_Err(p_dec->p_fifo, "PES packet is too short");
        input_DeletePES( p_dec->p_fifo->p_packets_mgt, p_pes );
        return;
    }

    i_pts = p_pes->i_pts;
    if( i_pts != 0 && i_pts != aout_DateGet( &p_dec->end_date ) )
    {
        aout_DateSet( &p_dec->end_date, i_pts );
    }

    p_data = p_pes->p_first;
    /* It necessarily contains one byte. */
    /* Get LPCM header. */

    /* Drop the first four bytes. */
    for ( i = 0; i < 4; i++ )
    {
        if ( p_data->p_payload_end == p_data->p_payload_start )
        {
            p_data = p_data->p_next;
        }
        p_data->p_payload_start++;
    }

    i_header = p_data->p_payload_start[0];
    p_data->p_payload_start++;

    switch ( (i_header >> 4) & 0x3 )
    {
    case 0:
        i_rate = 48000;
        break;
    case 1:
        i_rate = 96000;
        break;
    case 2:
        i_rate = 44100;
        break;
    case 3:
        i_rate = 32000;
        break;
    }

    switch ( i_header & 0x7 )
    {
    case 0:
        i_original_channels = AOUT_CHAN_CENTER;
        break;
    case 1:
        i_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
        break;
    case 2:
        /* This is unsure. */
        i_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_LFE;
        break;
    case 3:
        i_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
        break;
    case 4:
        /* This is unsure. */
        i_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                               | AOUT_CHAN_LFE;
        break;
    case 5:
        i_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                               | AOUT_CHAN_CENTER | AOUT_CHAN_LFE;
        break;
    case 6:
        i_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                               | AOUT_CHAN_CENTER | AOUT_CHAN_MIDDLELEFT
                               | AOUT_CHAN_MIDDLERIGHT;
        break;
    case 7:
        i_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                               | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                               | AOUT_CHAN_CENTER | AOUT_CHAN_MIDDLELEFT
                               | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE;
        break;
    }

    /* Check frame sync and drop it. */
    if ( p_data->p_payload_end == p_data->p_payload_start )
    {
        p_data = p_data->p_next;
    }
    if ( p_data->p_payload_start[0] != 0x80 )
    {
        msg_Warn(p_dec->p_fifo, "no frame sync");
        input_DeletePES( p_dec->p_fifo->p_packets_mgt, p_pes );
        return;
    }
    p_data->p_payload_start++;

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
            input_DeletePES( p_dec->p_fifo->p_packets_mgt, p_pes );
            return;
        }
    }

    if ( !aout_DateGet( &p_dec->end_date ) )
    {
        /* We've just started the stream, wait for the first PTS. */
        input_DeletePES( p_dec->p_fifo->p_packets_mgt, p_pes );
        return;
    }

    p_buffer = aout_DecNewBuffer( p_dec->p_aout, p_dec->p_aout_input,
            (i_size - LPCM_HEADER_LEN)
                / p_dec->output_format.i_bytes_per_frame );
    
    if( p_buffer == NULL )
    {
        msg_Err( p_dec->p_fifo, "cannot get aout buffer" );
        p_dec->p_fifo->b_error = 1;
        input_DeletePES( p_dec->p_fifo->p_packets_mgt, p_pes );
        return;
    }
    p_buffer->start_date = aout_DateGet( &p_dec->end_date );
    p_buffer->end_date = aout_DateIncrement( &p_dec->end_date,
            (i_size - LPCM_HEADER_LEN)
                / p_dec->output_format.i_bytes_per_frame );

    /* Get the whole frame. */
    p_dest = p_buffer->p_buffer;
    while ( p_data != NULL )
    {
        p_dec->p_fifo->p_vlc->pf_memcpy( p_dest, p_data->p_payload_start,
                p_data->p_payload_end - p_data->p_payload_start );
        p_dest += p_data->p_payload_end - p_data->p_payload_start;
        p_data = p_data->p_next;
    }
    input_DeletePES( p_dec->p_fifo->p_packets_mgt, p_pes );

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

    free( p_dec );
}
