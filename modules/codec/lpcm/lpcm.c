/*****************************************************************************
 * lpcm.c: lpcm decoder module
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: lpcm.c,v 1.4 2002/09/18 01:28:05 henri Exp $
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

#include "lpcm.h"
/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder    ( vlc_object_t * );
static int  RunDecoder     ( decoder_fifo_t * );

       void DecodeFrame    ( lpcmdec_thread_t * );
// static int  InitThread     ( lpcmdec_thread_t * );
static void EndThread      ( lpcmdec_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("linear PCM audio decoder") );
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
    lpcmdec_thread_t *   p_lpcmdec;

    /* Allocate the memory needed to store the thread's structure */
    if( (p_lpcmdec = (lpcmdec_thread_t *)malloc (sizeof(lpcmdec_thread_t)) )
            == NULL) 
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return( -1 );
    }

    /*
     * Initialize the thread properties
     */
    p_lpcmdec->p_fifo = p_fifo;

    /* Init the BitStream */
    InitBitstream( &p_lpcmdec->bit_stream, p_lpcmdec->p_fifo,
                   NULL, NULL );
   
    /* FIXME : I suppose the number of channel ans sampling rate 
     * are someway in the headers */
    p_lpcmdec->output_format.i_format = AOUT_FMT_S16_BE;
    p_lpcmdec->output_format.i_channels = 2;
    p_lpcmdec->output_format.i_rate = 48000;
    
    aout_DateInit( &p_lpcmdec->end_date, 48000 );
    p_lpcmdec->p_aout_input = aout_InputNew( p_lpcmdec->p_fifo,
                                         &p_lpcmdec->p_aout,
                                         &p_lpcmdec->output_format );

    if( p_lpcmdec->p_aout_input == NULL )
    {
        msg_Err( p_lpcmdec->p_fifo, "failed to create aout fifo" );
        p_lpcmdec->p_fifo->b_error = 1;
        return( -1 );
    }

    /* lpcm decoder thread's main loop */
    while ((!p_lpcmdec->p_fifo->b_die) && (!p_lpcmdec->p_fifo->b_error))
    {
        DecodeFrame(p_lpcmdec);
    }

    /* If b_error is set, the lpcm decoder thread enters the error loop */
    if (p_lpcmdec->p_fifo->b_error)
    {
        DecoderError( p_lpcmdec->p_fifo );
    }

    /* End of the lpcm decoder thread */
    EndThread (p_lpcmdec);

    return( 0 );
}

/*****************************************************************************
 * DecodeFrame: decodes a frame.
 *****************************************************************************/
void DecodeFrame( lpcmdec_thread_t * p_lpcmdec )
{
    byte_t     buffer[LPCMDEC_FRAME_SIZE];
    
    aout_buffer_t *    p_aout_buffer;
    mtime_t     i_pts;
    vlc_bool_t  b_sync;
    int         i_loop;

    NextPTS( &p_lpcmdec->bit_stream, &i_pts, NULL );
    
    if( i_pts != 0 && i_pts != aout_DateGet( &p_lpcmdec->end_date ) )
    {
        aout_DateSet( &p_lpcmdec->end_date, i_pts );
    }
    
    p_aout_buffer = aout_BufferNew( p_lpcmdec->p_aout,
                                  p_lpcmdec->p_aout_input,
                                  LPCMDEC_FRAME_SIZE/4 );
    
    if( !p_aout_buffer )
    {
        msg_Err( p_lpcmdec->p_fifo, "cannot get aout buffer" );
        p_lpcmdec->p_fifo->b_error = 1;
        return;
    }
    
    p_aout_buffer->start_date = aout_DateGet( &p_lpcmdec->end_date );
   
    p_aout_buffer->end_date = aout_DateIncrement( &p_lpcmdec->end_date,
                                                   LPCMDEC_FRAME_SIZE/4 );

    b_sync = 0;
    while( ( !p_lpcmdec->p_fifo->b_die ) &&
           ( !p_lpcmdec->p_fifo->b_error ) &&
           ( !b_sync ) )
    {
        while( ( !p_lpcmdec->p_fifo->b_die ) &&
               ( !p_lpcmdec->p_fifo->b_error ) &&
               ( GetBits( &p_lpcmdec->bit_stream, 8 ) != 0x01 ) );
        b_sync = ( ShowBits( &p_lpcmdec->bit_stream, 8 ) == 0x80 );
    }
    
    RemoveBits( &p_lpcmdec->bit_stream, 8 );
    
    GetChunk( &p_lpcmdec->bit_stream, p_aout_buffer->p_buffer, 
              LPCMDEC_FRAME_SIZE);

    if( p_lpcmdec->p_fifo->b_die || p_lpcmdec->p_fifo->b_error ) 
        return;

    aout_BufferPlay( p_lpcmdec->p_aout, p_lpcmdec->p_aout_input, 
                     p_aout_buffer );

}

/*****************************************************************************
 * EndThread : lpcm decoder thread destruction
 *****************************************************************************/
static void EndThread( lpcmdec_thread_t * p_lpcmdec )
{
    /* If the audio output fifo was created, we destroy it */
    if( p_lpcmdec->p_aout_input )
    {
        aout_InputDelete( p_lpcmdec->p_aout, p_lpcmdec->p_aout_input );
        
    }

    /* Destroy descriptor */
    free( p_lpcmdec );
}
