/*****************************************************************************
 * lpcm.c: lpcm decoder module
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: lpcm.c,v 1.7 2002/11/14 22:38:47 massiot Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Henri Fallon <henri@videolan.org>
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

#define LPCM_FRAME_NB 502

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

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder    ( vlc_object_t * );
static int  RunDecoder     ( decoder_fifo_t * );

       void DecodeFrame    ( dec_thread_t * );
// static int  InitThread     ( dec_thread_t * );
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
   
    /* FIXME : I suppose the number of channel and sampling rate 
     * are somewhere in the headers */
    p_dec->output_format.i_format = VLC_FOURCC('s','1','6','b');
    p_dec->output_format.i_physical_channels
           = p_dec->output_format.i_original_channels
           = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    p_dec->output_format.i_rate = 48000;
    
    aout_DateInit( &p_dec->end_date, 48000 );
    p_dec->p_aout = NULL;
    p_dec->p_aout_input = aout_DecNew( p_dec->p_fifo,
                                       &p_dec->p_aout,
                                       &p_dec->output_format );

    if ( p_dec->p_aout_input == NULL )
    {
        msg_Err( p_dec->p_fifo, "failed to create aout fifo" );
        p_dec->p_fifo->b_error = 1;
        EndThread( p_dec );
        return( -1 );
    }

    /* lpcm decoder thread's main loop */
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
void DecodeFrame( dec_thread_t * p_dec )
{
    aout_buffer_t *    p_aout_buffer;
    mtime_t     i_pts;

    NextPTS( &p_dec->bit_stream, &i_pts, NULL );
    
    if( i_pts != 0 && i_pts != aout_DateGet( &p_dec->end_date ) )
    {
        aout_DateSet( &p_dec->end_date, i_pts );
    }
    
    p_aout_buffer = aout_DecNewBuffer( p_dec->p_aout,
                                       p_dec->p_aout_input,
                                       LPCM_FRAME_NB );
    
    if( !p_aout_buffer )
    {
        msg_Err( p_dec->p_fifo, "cannot get aout buffer" );
        p_dec->p_fifo->b_error = 1;
        return;
    }
    
    p_aout_buffer->start_date = aout_DateGet( &p_dec->end_date );
    p_aout_buffer->end_date = aout_DateIncrement( &p_dec->end_date,
                                                   LPCM_FRAME_NB );

    /* Look for sync word - should be 0x0180 */
    RealignBits( &p_dec->bit_stream );
    while ( (GetBits( &p_dec->bit_stream, 16 ) ) != 0x0180 && 
             (!p_dec->p_fifo->b_die) && (!p_dec->p_fifo->b_error));

    GetChunk( &p_dec->bit_stream, p_aout_buffer->p_buffer, 
              LPCM_FRAME_NB * 4);

    if( p_dec->p_fifo->b_die )
    {
        aout_DecDeleteBuffer( p_dec->p_aout, p_dec->p_aout_input,
                              p_aout_buffer );
        return;
    }

    aout_DecPlay( p_dec->p_aout, p_dec->p_aout_input, 
                  p_aout_buffer );
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
