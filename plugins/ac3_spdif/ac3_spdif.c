/*****************************************************************************
 * ac3_spdif.c: ac3 pass-through to external decoder with enabled soundcard
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: ac3_spdif.c,v 1.4 2001/11/28 15:08:05 massiot Exp $
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Juha Yrjola <jyrjola@cc.hut.fi>
 *          German Gomez Garcia <german@piraos.com>
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

#define MODULE_NAME ac3_spdif
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>                                              /* memcpy() */
#include <fcntl.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "config.h"
#include "common.h"
#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */
#include "threads.h"
#include "mtime.h"

#include "audio_output.h"

#include "modules.h"
#include "modules_export.h"

#include "stream_control.h"
#include "input_ext-dec.h"

#include "ac3_spdif.h"
#include "ac3_iec958.h"

#define FRAME_NB 8

/****************************************************************************
 * Local Prototypes
 ****************************************************************************/
static int  ac3_spdif_Probe       ( probedata_t * );
static int  ac3_spdif_Run         ( decoder_config_t * );
static int  ac3_spdif_Init        ( ac3_spdif_thread_t * );
static void ac3_spdif_ErrorThread ( ac3_spdif_thread_t * );
static void ac3_spdif_EndThread   ( ac3_spdif_thread_t * );
static void BitstreamCallback( bit_stream_t *, boolean_t );

/*****************************************************************************
 * Capabilities
 *****************************************************************************/
void _M( adec_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = ac3_spdif_Probe;
    p_function_list->functions.dec.pf_RunThread = ac3_spdif_Run;
}

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for ac3 spdif decoder module" )
    ADD_COMMENT( "Nothing to configure" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    p_module->i_capabilities = MODULE_CAPABILITY_DEC;
    p_module->psz_longname = "Ac3 SPDIF decoder for AC3 pass-through";
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( adec_getfunctions )( &p_module->p_functions->dec );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * ac3_spdif_Probe: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able 
 * to chose.
 *****************************************************************************/
static int ac3_spdif_Probe( probedata_t *p_data )
{
    if( main_GetIntVariable( AOUT_SPDIF_VAR, 0 ) && 
        p_data->i_type == AC3_AUDIO_ES )
        return( 100 );
    else
        return( 0 );
}


/****************************************************************************
 * ac3_spdif_Run: the whole thing
 ****************************************************************************
 * This function is called just after the thread is launched.
 ****************************************************************************/
static int ac3_spdif_Run( decoder_config_t * p_config )
{
    ac3_spdif_thread_t *   p_spdif;
    mtime_t     i_frame_time;
    boolean_t   b_sync;
    /* PTS of the current frame */
    mtime_t     i_current_pts = 0;

    intf_DbgMsg( "spdif debug: ac3_spdif thread created, initializing." );

    /* Allocate the memory needed to store the thread's structure */
    p_spdif = malloc( sizeof(ac3_spdif_thread_t) );

    if( p_spdif == NULL )
    {
        intf_ErrMsg ( "spdif error: not enough memory "
                      "for spdif_CreateThread() to create the new thread");
        return( -1 );
    }
  
    p_spdif->p_config = p_config; 
    
    if (ac3_spdif_Init( p_spdif ) )
    {
        intf_ErrMsg( "spdif error: could not initialize thread" );
        return( -1 );
    }

    /* Compute the theorical duration of an ac3 frame */
    i_frame_time = 1000000 * AC3_FRAME_SIZE /
                             p_spdif->ac3_info.i_sample_rate;
    
    intf_DbgMsg( "spdif debug: ac3_spdif thread (%p) initialized", p_spdif );

    while( !p_spdif->p_fifo->b_die && !p_spdif->p_fifo->b_error )
    {
        /* Handle the dates */
        if( p_spdif->i_real_pts )
        {
            mtime_t     i_delta = p_spdif->i_real_pts - i_current_pts -
                                  i_frame_time;
            if( i_delta > i_frame_time || i_delta < -i_frame_time )
            {
                intf_WarnMsg( 3, "spdif warning: date discontinuity (%d)",
                              i_delta );
            }
            i_current_pts = p_spdif->i_real_pts;
            p_spdif->i_real_pts = 0;
        }
        else
        {
            i_current_pts += i_frame_time;
        }

        /* if we're late here the output won't have to play the frame */
        if( i_current_pts > mdate() )
        {
            p_spdif->p_aout_fifo->date[p_spdif->p_aout_fifo->l_end_frame] =
                i_current_pts;
    
            /* Write in the first free packet of aout fifo */
            p_spdif->p_iec = ((u8*)(p_spdif->p_aout_fifo->buffer) + 
                (p_spdif->p_aout_fifo->l_end_frame * SPDIF_FRAME_SIZE ));
    
            /* Build burst to be sent to hardware decoder */
            ac3_iec958_build_burst( p_spdif );
    
            vlc_mutex_lock (&p_spdif->p_aout_fifo->data_lock);
            p_spdif->p_aout_fifo->l_end_frame = 
                    (p_spdif->p_aout_fifo->l_end_frame + 1 ) & AOUT_FIFO_SIZE;
            vlc_mutex_unlock (&p_spdif->p_aout_fifo->data_lock);
        }

        /* Find syncword again in case of stream discontinuity */
        /* Here we have p_spdif->i_pts == 0
         * Therefore a non-zero value after a call to GetBits() means the PES
         * has changed. */
        b_sync = 0;
        while( !b_sync )
        {
            while( GetBits( &p_spdif->bit_stream, 8 ) != 0x0b );
            p_spdif->i_real_pts = p_spdif->i_pts;
            p_spdif->i_pts = 0;
            b_sync = ( ShowBits( &p_spdif->bit_stream, 8 ) == 0x77 );
        }
        RemoveBits( &p_spdif->bit_stream, 8 );

        /* Read data from bitstream */
        GetChunk( &p_spdif->bit_stream, p_spdif->p_ac3 + 2,
                  p_spdif->ac3_info.i_frame_size - 2 );
    }

    /* If b_error is set, the ac3 spdif thread enters the error loop */
    if( p_spdif->p_fifo->b_error )
    {
        ac3_spdif_ErrorThread( p_spdif );
    }

    /* End of the ac3 decoder thread */
    ac3_spdif_EndThread( p_spdif );
    
    return( 0 );
}

/****************************************************************************
 * ac3_spdif_Init: initialize thread data and create output fifo
 ****************************************************************************/
static int ac3_spdif_Init( ac3_spdif_thread_t * p_spdif )
{
    boolean_t b_sync = 0;

    /* Temporary buffer to store ac3 frames to be transformed */
    p_spdif->p_ac3 = malloc( SPDIF_FRAME_SIZE );

    if( p_spdif->p_ac3 == NULL )
    {
        free( p_spdif->p_ac3 );
        return( -1 );
    }

    /*
     * Initialize the thread properties
     */
    p_spdif->p_fifo = p_spdif->p_config->p_decoder_fifo;

    p_spdif->p_config->pf_init_bit_stream(
            &p_spdif->bit_stream,
            p_spdif->p_config->p_decoder_fifo,
            BitstreamCallback, (void*)p_spdif );

    /* Creating the audio output fifo */
    p_spdif->p_aout_fifo = aout_CreateFifo( AOUT_ADEC_SPDIF_FIFO, 1, 48000, 0,
                                            SPDIF_FRAME_SIZE, NULL );

    if( p_spdif->p_aout_fifo == NULL )
    {
        return( -1 );
    }

    intf_WarnMsg( 3, "spdif: aout fifo #%d created",
                     p_spdif->p_aout_fifo->i_fifo );

    /* Sync word */
    p_spdif->p_ac3[0] = 0x0b;
    p_spdif->p_ac3[1] = 0x77;

    /* Find syncword */
    while( !b_sync )
    {
        while( GetBits( &p_spdif->bit_stream, 8 ) != 0x0b );
        p_spdif->i_real_pts = p_spdif->i_pts;
        p_spdif->i_pts = 0;
        b_sync = ( ShowBits( &p_spdif->bit_stream, 8 ) == 0x77 );
    }
    RemoveBits( &p_spdif->bit_stream, 8 );

    /* Check stream properties */
    if( ac3_iec958_parse_syncinfo( p_spdif ) < 0 )
    {
        intf_ErrMsg( "spdif error: stream not valid");

        aout_DestroyFifo( p_spdif->p_aout_fifo );
        return( -1 );
    }

    /* Check that we can handle the rate 
     * FIXME: we should check that we have the same rate for all fifos 
     * but all rates should be supported by the decoder (32, 44.1, 48) */
    if( p_spdif->ac3_info.i_sample_rate != 48000 )
    {
        intf_ErrMsg( "spdif error: Only 48000 Hz streams supported");

        aout_DestroyFifo( p_spdif->p_aout_fifo );
        return -1;
    }
    p_spdif->p_aout_fifo->l_rate = p_spdif->ac3_info.i_sample_rate;

    GetChunk( &p_spdif->bit_stream, p_spdif->p_ac3 + sizeof(sync_frame_t),
        p_spdif->ac3_info.i_frame_size - sizeof(sync_frame_t) );

    return( 0 );
}


/*****************************************************************************
 * ac3_spdif_ErrorThread : ac3 spdif's RunThread() error loop
 *****************************************************************************/
static void ac3_spdif_ErrorThread( ac3_spdif_thread_t * p_spdif )
{
    /* We take the lock, because we are going to read/write the start/end
     * indexes of the decoder fifo */
    vlc_mutex_lock (&p_spdif->p_fifo->data_lock);

    /* Wait until a `die' order is sent */
    while( !p_spdif->p_fifo->b_die )
    {
        /* Trash all received PES packets */
        while( !DECODER_FIFO_ISEMPTY( *p_spdif->p_fifo ) )
        {
            p_spdif->p_fifo->pf_delete_pes(p_spdif->p_fifo->p_packets_mgt,
                    DECODER_FIFO_START( *p_spdif->p_fifo ) );
            DECODER_FIFO_INCSTART( *p_spdif->p_fifo );
        }

        /* Waiting for the input thread to put new PES packets in the fifo */
        vlc_cond_wait( &p_spdif->p_fifo->data_wait,
                       &p_spdif->p_fifo->data_lock );
    }

    /* We can release the lock before leaving */
    vlc_mutex_unlock( &p_spdif->p_fifo->data_lock );
}

/*****************************************************************************
 * ac3_spdif_EndThread : ac3 spdif thread destruction
 *****************************************************************************/
static void ac3_spdif_EndThread( ac3_spdif_thread_t * p_spdif )
{
    intf_DbgMsg( "spdif debug: destroying thread %p", p_spdif );

    /* If the audio output fifo was created, we destroy it */
    if( p_spdif->p_aout_fifo != NULL )
    {
        aout_DestroyFifo( p_spdif->p_aout_fifo );

        /* Make sure the output thread leaves the NextFrame() function */
        vlc_mutex_lock( &(p_spdif->p_aout_fifo->data_lock ) );
        vlc_cond_signal( &(p_spdif->p_aout_fifo->data_wait ) );
        vlc_mutex_unlock( &(p_spdif->p_aout_fifo->data_lock ) );
        
    }

    /* Destroy descriptor */
    free( p_spdif->p_ac3 );
    free( p_spdif );
    
    intf_DbgMsg ("spdif debug: thread %p destroyed", p_spdif );
}

/*****************************************************************************
 * BitstreamCallback: Import parameters from the new data/PES packet
 *****************************************************************************
 * This function is called by input's NextDataPacket.
 *****************************************************************************/
static void BitstreamCallback ( bit_stream_t * p_bit_stream,
                                        boolean_t b_new_pes)
{
    ac3_spdif_thread_t *    p_spdif;

    if( b_new_pes )
    {
        p_spdif = (ac3_spdif_thread_t *)p_bit_stream->p_callback_arg;

        p_bit_stream->p_byte += 3;

        p_spdif->i_pts =
            DECODER_FIFO_START( *p_bit_stream->p_decoder_fifo )->i_pts;
        DECODER_FIFO_START( *p_bit_stream->p_decoder_fifo )->i_pts = 0;
    }
}
