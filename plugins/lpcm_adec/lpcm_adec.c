/*****************************************************************************
 * lpcm_decoder_thread.c: lpcm decoder thread
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: lpcm_adec.c,v 1.15.2.3 2002/10/14 16:27:44 gbazin Exp $
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
#include <stdio.h>                                           /* "intf_msg.h" */
#include <string.h>                                    /* memcpy(), memset() */
#include <stdlib.h>                                      /* malloc(), free() */

#include <videolan/vlc.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>                                           /* getpid() */
#endif

#include "audio_output.h"

#include "stream_control.h"
#include "input_ext-dec.h"

#include "lpcm_adec.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  decoder_Probe  ( u8 * );
static int  decoder_Run    ( decoder_config_t * );
       void DecodeFrame    ( lpcmdec_thread_t * );
static int  InitThread     ( lpcmdec_thread_t * );
static void EndThread      ( lpcmdec_thread_t * );


/*****************************************************************************
 * Capabilities
 *****************************************************************************/
void _M( adec_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->functions.dec.pf_probe = decoder_Probe;
    p_function_list->functions.dec.pf_run   = decoder_Run;
}

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( _("linear PCM audio decoder") )
    ADD_CAPABILITY( DECODER, 100 )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( adec_getfunctions )( &p_module->p_functions->dec );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * decoder_Probe: probe the decoder and return score
 *****************************************************************************/
static int decoder_Probe( u8 *pi_type )
{
    return ( (*pi_type == LPCM_AUDIO_ES || *pi_type == LPCMB_AUDIO_ES) ? 0 : -1 );
}

/*****************************************************************************
 * decoder_Run: the lpcm decoder
 *****************************************************************************/
static int decoder_Run( decoder_config_t * p_config )
{
    lpcmdec_thread_t *   p_lpcmdec;

    /* Allocate the memory needed to store the thread's structure */
    if( (p_lpcmdec = (lpcmdec_thread_t *)malloc (sizeof(lpcmdec_thread_t)) )
            == NULL) 
    {
        intf_ErrMsg( "LPCM : error : cannot create lpcmdec_thread_t" );
        DecoderError( p_config->p_decoder_fifo );
        return( -1 );
    }

    /*
     * Initialize the thread properties
     */
    p_lpcmdec->p_config = p_config;
    p_lpcmdec->p_fifo = p_config->p_decoder_fifo;

    if( InitThread( p_lpcmdec ) )
    {
        DecoderError( p_config->p_decoder_fifo );
        free( p_lpcmdec );
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
 * InitThread : initialize an lpcm decoder thread
 *****************************************************************************/
static int InitThread (lpcmdec_thread_t * p_lpcmdec)
{

    /* Init the BitStream */
    InitBitstream( &p_lpcmdec->bit_stream, p_lpcmdec->p_config->p_decoder_fifo,
                   NULL, NULL);

    /* Creating the audio output fifo */
    p_lpcmdec->p_aout_fifo = aout_CreateFifo( AOUT_FIFO_PCM, 2, 48000,
                                              LPCMDEC_FRAME_SIZE/2, NULL  );
    if ( p_lpcmdec->p_aout_fifo == NULL )
    {
        return( -1 );
    }
    return( 0 );
}

/*****************************************************************************
 * DecodeFrame: decodes a frame.
 *****************************************************************************/
void DecodeFrame( lpcmdec_thread_t * p_lpcmdec )
{
    byte_t * buffer;
#ifndef WORDS_BIGENDIAN
    byte_t p_temp[LPCMDEC_FRAME_SIZE];
#endif
    int i_loop;
    byte_t byte1, byte2;

    CurrentPTS( &p_lpcmdec->bit_stream,
        &p_lpcmdec->p_aout_fifo->date[p_lpcmdec->p_aout_fifo->i_end_frame],
        NULL );
    if( !p_lpcmdec->p_aout_fifo->date[p_lpcmdec->p_aout_fifo->i_end_frame] )
    {
        p_lpcmdec->p_aout_fifo->date[p_lpcmdec->p_aout_fifo->i_end_frame] =
            LAST_MDATE;
    }

    buffer = ((byte_t *)p_lpcmdec->p_aout_fifo->buffer) + 
              (p_lpcmdec->p_aout_fifo->i_end_frame * LPCMDEC_FRAME_SIZE);

    RemoveBits32(&p_lpcmdec->bit_stream);
    byte1 = GetBits(&p_lpcmdec->bit_stream, 8) ;
    byte2 = GetBits(&p_lpcmdec->bit_stream, 8) ;
    
    /* I only have 2 test streams. As far as I understand
     * after the RemoveBits and the 2 GetBits, we should be exactly 
     * where we want : the sync word : 0x0180.
     * If not, we go and find it. */
    while( ( byte1 != 0x01 || byte2 != 0x80 ) && (!p_lpcmdec->p_fifo->b_die)
                                       && (!p_lpcmdec->p_fifo->b_error) )
    {
        byte1 = byte2;
        byte2 = GetBits(&p_lpcmdec->bit_stream, 8);
    }
    
#ifndef WORDS_BIGENDIAN
    GetChunk( &p_lpcmdec->bit_stream, p_temp, LPCMDEC_FRAME_SIZE);
    if( p_lpcmdec->p_fifo->b_die ) return;

#   ifdef HAVE_SWAB
    swab( p_temp, buffer, LPCMDEC_FRAME_SIZE );
#   else
    for( i_loop = 0; i_loop < LPCMDEC_FRAME_SIZE/2; i_loop++ )
    {
        buffer[2*i_loop]=p_temp[2*i_loop+1];
        buffer[2*i_loop+1]=p_temp[2*i_loop];
    }
#   endif

#else
    GetChunk( &p_lpcmdec->bit_stream, buffer, LPCMDEC_FRAME_SIZE);
    if( p_lpcmdec->p_fifo->b_die ) return;
#endif
    
    vlc_mutex_lock (&p_lpcmdec->p_aout_fifo->data_lock);
    p_lpcmdec->p_aout_fifo->i_end_frame = 
        (p_lpcmdec->p_aout_fifo->i_end_frame + 1) & AOUT_FIFO_SIZE;
    vlc_cond_signal (&p_lpcmdec->p_aout_fifo->data_wait);
    vlc_mutex_unlock (&p_lpcmdec->p_aout_fifo->data_lock);
}

/*****************************************************************************
 * EndThread : lpcm decoder thread destruction
 *****************************************************************************/
static void EndThread( lpcmdec_thread_t * p_lpcmdec )
{
    /* If the audio output fifo was created, we destroy it */
    if( p_lpcmdec->p_aout_fifo != NULL ) 
    {
        aout_DestroyFifo( p_lpcmdec->p_aout_fifo );

        /* Make sure the output thread leaves the NextFrame() function */
        vlc_mutex_lock( &(p_lpcmdec->p_aout_fifo->data_lock) );
        vlc_cond_signal( &(p_lpcmdec->p_aout_fifo->data_wait) );
        vlc_mutex_unlock( &(p_lpcmdec->p_aout_fifo->data_lock) );
    }

    /* Destroy descriptor */
    free( p_lpcmdec );
}
