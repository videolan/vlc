/*****************************************************************************
 * a52.c: ATSC A/52 aka AC-3 decoder plugin for vlc.
 *   This plugin makes use of liba52 to decode A/52 audio
 *   (http://liba52.sf.net/).
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: a52.c,v 1.8 2002/04/25 21:52:42 sam Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
#include <stdint.h>                                            /* int16_t .. */

#include <videolan/vlc.h>

#include "audio_output.h"

#include "stream_control.h"
#include "input_ext-dec.h"

#include <a52dec/a52.h>                                /* liba52 header file */
#include "a52.h"

#define AC3DEC_FRAME_SIZE 1536 

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  decoder_Probe  ( u8 * );
static int  decoder_Run    ( decoder_config_t * );
static int  DecodeFrame    ( a52_adec_thread_t * );
static int  InitThread     ( a52_adec_thread_t * );
static void EndThread      ( a52_adec_thread_t * );

static void               BitstreamCallback ( bit_stream_t *, boolean_t );
static void               float2s16_2       ( float *, int16_t * );
static __inline__ int16_t convert   ( int32_t );

/*****************************************************************************
 * Capabilities
 *****************************************************************************/
void _M( adec_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->functions.dec.pf_probe = decoder_Probe;
    p_function_list->functions.dec.pf_run   = decoder_Run;
}

/*****************************************************************************
 * Build configuration structure.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( _("a52 ATSC A/52 aka AC-3 audio decoder module") )
    ADD_CAPABILITY( DECODER, 40 )
    ADD_SHORTCUT( "a52" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( adec_getfunctions )( &p_module->p_functions->dec );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * decoder_Probe: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to choose.
 *****************************************************************************/
static int decoder_Probe( u8 *pi_type )
{
    return ( *pi_type == AC3_AUDIO_ES ? 0 : -1 );
}

/*****************************************************************************
 * decoder_Run: this function is called just after the thread is created
 *****************************************************************************/
static int decoder_Run ( decoder_config_t *p_config )
{
    a52_adec_thread_t *p_a52_adec;

    /* Allocate the memory needed to store the thread's structure */
    p_a52_adec =
        (a52_adec_thread_t *)malloc( sizeof(a52_adec_thread_t) );
    if (p_a52_adec == NULL)
    {
        intf_ErrMsg ( "a52 error: not enough memory "
                      "for decoder_Run() to allocate p_a52_adec" );
        DecoderError( p_config->p_decoder_fifo );
        return( -1 );
    }

    /* FIXME */
    p_a52_adec->i_channels = 2;

    /*
     * Initialize the thread properties
     */
    p_a52_adec->p_aout_fifo = NULL;
    p_a52_adec->p_config = p_config;
    p_a52_adec->p_fifo = p_a52_adec->p_config->p_decoder_fifo;
    if( InitThread( p_a52_adec ) )
    {
        intf_ErrMsg( "a52 error: could not initialize thread" );
        DecoderError( p_config->p_decoder_fifo );
        free( p_a52_adec );
        return( -1 );
    }

    /* liba52 decoder thread's main loop */
    while( !p_a52_adec->p_fifo->b_die && !p_a52_adec->p_fifo->b_error )
    {

        /* look for sync word - should be 0x0b77 */
        RealignBits(&p_a52_adec->bit_stream);
        while( (ShowBits( &p_a52_adec->bit_stream, 16 ) ) != 0x0b77 && 
               (!p_a52_adec->p_fifo->b_die) && (!p_a52_adec->p_fifo->b_error))
        {
            RemoveBits( &p_a52_adec->bit_stream, 8 );
        }

        /* get a52 frame header */
        GetChunk( &p_a52_adec->bit_stream, p_a52_adec->p_frame_buffer, 7 );

        /* check if frame is valid and get frame info */
        p_a52_adec->frame_size = a52_syncinfo( p_a52_adec->p_frame_buffer,
                                               &p_a52_adec->flags,
                                               &p_a52_adec->sample_rate,
                                               &p_a52_adec->bit_rate );

        if( !p_a52_adec->frame_size )
        {
            intf_WarnMsg( 3, "a52: a52_syncinfo failed" );
            continue;
        }

        if( DecodeFrame( p_a52_adec ) )
        {
            DecoderError( p_config->p_decoder_fifo );
            free( p_a52_adec );
            return( -1 );
        }

    }

    /* If b_error is set, the decoder thread enters the error loop */
    if( p_a52_adec->p_fifo->b_error )
    {
        DecoderError( p_a52_adec->p_fifo );
    }

    /* End of the liba52 decoder thread */
    EndThread( p_a52_adec );

    return( 0 );
}

/*****************************************************************************
 * InitThread: initialize data before entering main loop
 *****************************************************************************/
static int InitThread( a52_adec_thread_t * p_a52_adec )
{
    intf_WarnMsg( 3, "a52: InitThread" );

    /* Initialize liba52 */
    p_a52_adec->p_a52_state = a52_init( 0 );
    if( p_a52_adec->p_a52_state == NULL )
    {
        intf_ErrMsg ( "a52 error: InitThread() unable to initialize "
                      "liba52" );
        return -1;
    }

    /* Init the BitStream */
    InitBitstream( &p_a52_adec->bit_stream,
                   p_a52_adec->p_fifo,
                   BitstreamCallback, NULL );

    return( 0 );
}

/*****************************************************************************
 * DecodeFrame: decodes an ATSC A/52 frame.
 *****************************************************************************/
static int DecodeFrame( a52_adec_thread_t * p_a52_adec )
{
    sample_t sample_level = 1;
    byte_t   *p_buffer;
    int i;

    if( ( p_a52_adec->p_aout_fifo != NULL ) &&
        ( p_a52_adec->p_aout_fifo->i_rate != p_a52_adec->sample_rate ) )
    {
        /* Make sure the output thread leaves the NextFrame() function */
        vlc_mutex_lock (&(p_a52_adec->p_aout_fifo->data_lock));
        aout_DestroyFifo (p_a52_adec->p_aout_fifo);
        vlc_cond_signal (&(p_a52_adec->p_aout_fifo->data_wait));
        vlc_mutex_unlock (&(p_a52_adec->p_aout_fifo->data_lock));

        p_a52_adec->p_aout_fifo = NULL;
    }

    /* Creating the audio output fifo if not created yet */
    if (p_a52_adec->p_aout_fifo == NULL )
    {
        p_a52_adec->p_aout_fifo = aout_CreateFifo( AOUT_FIFO_PCM, 
                                    p_a52_adec->i_channels,
                                    p_a52_adec->sample_rate,
                                    AC3DEC_FRAME_SIZE * p_a52_adec->i_channels,
                                    NULL );

        if ( p_a52_adec->p_aout_fifo == NULL )
        { 
            return( -1 );
        }
    }

    /* Set the Presentation Time Stamp */
    CurrentPTS( &p_a52_adec->bit_stream,
                &p_a52_adec->p_aout_fifo->date[
                    p_a52_adec->p_aout_fifo->i_end_frame],
                NULL );

    if( !p_a52_adec->p_aout_fifo->date[
            p_a52_adec->p_aout_fifo->i_end_frame] )
    {
        p_a52_adec->p_aout_fifo->date[
            p_a52_adec->p_aout_fifo->i_end_frame] = LAST_MDATE;
    }



    p_buffer = ((byte_t *)p_a52_adec->p_aout_fifo->buffer) +
        ( p_a52_adec->p_aout_fifo->i_end_frame * AC3DEC_FRAME_SIZE *
          p_a52_adec->i_channels * sizeof(s16) );

    /* FIXME */
    p_a52_adec->flags = A52_STEREO | A52_ADJUST_LEVEL;

    /* Get the complete frame */
    GetChunk( &p_a52_adec->bit_stream, p_a52_adec->p_frame_buffer + 7,
              p_a52_adec->frame_size - 7 );

    /* do the actual decoding now */
    a52_frame( p_a52_adec->p_a52_state, p_a52_adec->p_frame_buffer,
               &p_a52_adec->flags, &sample_level, 384 );

    for( i = 0; i < 6; i++ )
    {
        if( a52_block( p_a52_adec->p_a52_state ) )
            intf_WarnMsg( 5, "a52: a52_block failed for block %i", i );

        float2s16_2( a52_samples( p_a52_adec->p_a52_state ),
                     ((int16_t *)p_buffer) + i * 256 * p_a52_adec->i_channels );
    }


    vlc_mutex_lock( &p_a52_adec->p_aout_fifo->data_lock );
    p_a52_adec->p_aout_fifo->i_end_frame = 
      (p_a52_adec->p_aout_fifo->i_end_frame + 1) & AOUT_FIFO_SIZE;
    vlc_cond_signal (&p_a52_adec->p_aout_fifo->data_wait);
    vlc_mutex_unlock (&p_a52_adec->p_aout_fifo->data_lock);

    return 0;
}

/*****************************************************************************
 * EndThread : liba52 decoder thread destruction
 *****************************************************************************/
static void EndThread (a52_adec_thread_t *p_a52_adec)
{
    intf_WarnMsg ( 3, "a52: EndThread" );

    /* If the audio output fifo was created, we destroy it */
    if (p_a52_adec->p_aout_fifo != NULL)
    {
        aout_DestroyFifo (p_a52_adec->p_aout_fifo);

        /* Make sure the output thread leaves the NextFrame() function */
        vlc_mutex_lock (&(p_a52_adec->p_aout_fifo->data_lock));
        vlc_cond_signal (&(p_a52_adec->p_aout_fifo->data_wait));
        vlc_mutex_unlock (&(p_a52_adec->p_aout_fifo->data_lock));
    }

    a52_free( p_a52_adec->p_a52_state );
    free( p_a52_adec );

}

/*****************************************************************************
 * float2s16_2 : converts floats to ints using a trick based on the IEEE
 *               floating-point format
 *****************************************************************************/
static __inline__ int16_t convert (int32_t i)
{
    if (i > 0x43c07fff)
        return 32767;
    else if (i < 0x43bf8000)
        return -32768;
    else
        return i - 0x43c00000;
}

static void float2s16_2 (float * _f, int16_t * s16)
{
    int i;
    int32_t * f = (int32_t *) _f;

    for (i = 0; i < 256; i++) {
      s16[2*i] = convert (f[i]);
        s16[2*i+1] = convert (f[i+256]);
    }
}

/*****************************************************************************
 * BitstreamCallback: Import parameters from the new data/PES packet
 *****************************************************************************
 * This function is called by input's NextDataPacket.
 *****************************************************************************/
static void BitstreamCallback ( bit_stream_t * p_bit_stream,
                                boolean_t b_new_pes )
{
    if( b_new_pes )
    {
        /* Drop special AC3 header */
        p_bit_stream->p_byte += 3;
    }
}
