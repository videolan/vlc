/*****************************************************************************
 * decoder.c: MPEG audio decoder thread
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: decoder.c,v 1.3 2002/08/26 23:00:22 massiot Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Michel Lespinasse <walken@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
 *          Cyril Deguet <asmax@via.ecp.fr>
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
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include <vlc/aout.h>

#include "generic.h"
#include "decoder.h"

#define ADEC_FRAME_NB 1152

/*****************************************************************************
 * Local Prototypes
 *****************************************************************************/
static int  OpenDecoder    ( vlc_object_t * );
static int  RunDecoder     ( decoder_fifo_t * );

static void EndThread      ( adec_thread_t * );
static void DecodeThread   ( adec_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("MPEG I/II layer 1/2 audio decoder") );
    set_capability( "decoder", 50 );
    add_requirement( FPU );
    add_shortcut( "builtin" );
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

    if( p_fifo->i_fourcc != VLC_FOURCC('m','p','g','a') )
    {   
        return VLC_EGENERIC;
    }
    
    p_fifo->pf_run = RunDecoder;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * RunDecoder: initialize, go inside main loop, destroy
 *****************************************************************************/
static int RunDecoder( decoder_fifo_t *p_fifo )
{
    adec_thread_t   * p_dec;
    
    /* Allocate the memory needed to store the thread's structure */
    if ( (p_dec = (adec_thread_t *)malloc (sizeof(adec_thread_t))) == NULL ) 
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return 0;
    }
    
    /*
     * Initialize the thread properties
     */
    p_dec->p_fifo = p_fifo;

    /* 
     * Initilize the banks
     */
    p_dec->bank_0.actual = p_dec->bank_0.v1;
    p_dec->bank_0.pos = 0;
    p_dec->bank_1.actual = p_dec->bank_1.v1;
    p_dec->bank_1.pos = 0;
    
    /*
     * Initialize bit stream 
     */
    InitBitstream( &p_dec->bit_stream, p_dec->p_fifo, NULL, NULL );

    /* We do not create the audio output fifo now, but
       it will be created when the first frame is received */
    p_dec->p_aout = NULL;
    p_dec->p_aout_input = NULL;

    /* Audio decoder thread's main loop */
    while( (!p_dec->p_fifo->b_die) && (!p_dec->p_fifo->b_error) )
    {
        DecodeThread( p_dec );
    }
    
    /* If b_error is set, the audio decoder thread enters the error loop */
    if( p_dec->p_fifo->b_error ) 
    {
        DecoderError( p_dec->p_fifo );
    }

    /* End of the audio decoder thread */
    EndThread( p_dec );

    return( 0 );
}

/*
 * Following functions are local to this module
 */

/*****************************************************************************
 * DecodeThread: decodes a mpeg frame
 *****************************************************************************/
static void DecodeThread( adec_thread_t * p_dec )
{
    mtime_t pts;
    aout_buffer_t * p_aout_buffer;

    adec_sync_info_t sync_info;
    
    /* Look for sync word - should be 0x0b77 */
    RealignBits( &p_dec->bit_stream );
    while( (ShowBits( &p_dec->bit_stream, 16 ) & 0xfff0) != 0xfff0 && 
           (!p_dec->p_fifo->b_die) && (!p_dec->p_fifo->b_error))
    {
        RemoveBits( &p_dec->bit_stream, 8 );
    }

    /* Set the Presentation Time Stamp */
    NextPTS( &p_dec->bit_stream, &pts, NULL );
    if ( pts != 0 && pts != aout_DateGet( &p_dec->end_date ) )
    {
        aout_DateSet( &p_dec->end_date, pts );
    }

    if( !adec_SyncFrame( p_dec, &sync_info ) )
    {
        /* Create the output fifo if it doesn't exist yet */
        if( ( p_dec->p_aout_input == NULL )||
            ( p_dec->output_format.i_channels != ( sync_info.b_stereo ? 2 : 1 ) )||
            ( p_dec->output_format.i_rate != sync_info.sample_rate ) )
        {
            if( p_dec->p_aout_input )
            {
                /* Delete old output */
                msg_Warn( p_dec->p_fifo, "opening a new aout" );
                aout_InputDelete( p_dec->p_aout, p_dec->p_aout_input );
            }

            /* Set output configuration */
            p_dec->output_format.i_format   = AOUT_FMT_FLOAT32;
            p_dec->output_format.i_channels = ( sync_info.b_stereo ? 2 : 1 );
            p_dec->output_format.i_rate     = sync_info.sample_rate;
            aout_DateInit( &p_dec->end_date, sync_info.sample_rate );
            p_dec->p_aout_input = aout_InputNew( p_dec->p_fifo,
                                                  &p_dec->p_aout,
                                                  &p_dec->output_format );
        }

        if( p_dec->p_aout_input == NULL )
        {
            msg_Err( p_dec->p_fifo, "failed to create aout fifo" );
            p_dec->p_fifo->b_error = 1;
            return;
        }

        if( !aout_DateGet( &p_dec->end_date ) )
        {
            /* We've just started the stream, wait for the first PTS. */
            return;
        }

        p_aout_buffer = aout_BufferNew( p_dec->p_aout,
                                        p_dec->p_aout_input,
                                        ADEC_FRAME_NB );
        if( !p_aout_buffer )
        {
            msg_Err( p_dec->p_fifo, "cannot get aout buffer" );
            p_dec->p_fifo->b_error = 1;
            return;
        }
        p_aout_buffer->start_date = aout_DateGet( &p_dec->end_date );
        p_aout_buffer->end_date = aout_DateIncrement( &p_dec->end_date,
                                                      ADEC_FRAME_NB );

        if( adec_DecodeFrame (p_dec, (float*)p_aout_buffer->p_buffer ) )
        {
            /* Ouch, failed decoding... We'll have to resync */
            aout_BufferDelete( p_dec->p_aout, p_dec->p_aout_input, p_aout_buffer );
        }
        else
        {
            aout_BufferPlay( p_dec->p_aout, p_dec->p_aout_input, p_aout_buffer );
        }
    }
}

/*****************************************************************************
 * EndThread : audio decoder thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization.
 *****************************************************************************/
static void EndThread ( adec_thread_t *p_dec )
{
    /* If the audio output fifo was created, we destroy it */
    if( p_dec->p_aout_input )
    {
        aout_InputDelete( p_dec->p_aout, p_dec->p_aout_input );
        
    }

    /* Destroy descriptor */
    free( p_dec );
}

