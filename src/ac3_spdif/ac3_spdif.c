/*****************************************************************************
 * ac3_spdif.c: ac3 pass-through to external decoder with enabled soundcard
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: ac3_spdif.c,v 1.12 2001/09/30 20:25:13 bozo Exp $
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
#include "threads.h"
#include "mtime.h"

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */

#include "stream_control.h"
#include "input_ext-dec.h"

#include "audio_output.h"

#include "ac3_spdif.h"
#include "ac3_iec958.h"

#define FRAME_NB 8

/****************************************************************************
 * Local Prototypes
 ****************************************************************************/
static int  InitThread       ( ac3_spdif_thread_t * );
static void RunThread        ( ac3_spdif_thread_t * );
static void ErrorThread      ( ac3_spdif_thread_t * );
static void EndThread        ( ac3_spdif_thread_t * );
static void BitstreamCallback( bit_stream_t *, boolean_t );

/****************************************************************************
 * spdif_CreateThread: initialize the spdif thread
 ****************************************************************************/
vlc_thread_t spdif_CreateThread( adec_config_t * p_config )
{
    ac3_spdif_thread_t *   p_spdif;

    intf_DbgMsg( "spdif debug: creating ac3 pass-through thread" );

    /* Allocate the memory needed to store the thread's structure */
    p_spdif = malloc( sizeof(ac3_spdif_thread_t) );

    if( p_spdif == NULL )
    {
        intf_ErrMsg ( "spdif error: not enough memory "
                      "for spdif_CreateThread() to create the new thread");
        return 0;
    }
    
    /* Temporary buffer to store ac3 frames to be transformed */
    p_spdif->p_ac3 = malloc( SPDIF_FRAME_SIZE );

    if( p_spdif->p_ac3 == NULL )
    {
        free( p_spdif->p_ac3 );
        return 0;
    }

    /*
     * Initialize the thread properties
     */
    p_spdif->p_config = p_config;
    p_spdif->p_fifo = p_config->decoder_config.p_decoder_fifo;

    p_spdif->p_aout_fifo = NULL;

    /* Spawn the ac3 to spdif thread */
    if (vlc_thread_create(&p_spdif->thread_id, "spdif", 
                (vlc_thread_func_t)RunThread, (void *)p_spdif))
    {
        intf_ErrMsg( "spdif error: can't spawn spdif thread" );
        free( p_spdif->p_ac3 );
        free( p_spdif );
        return 0;
    }

    intf_DbgMsg( "spdif debug: spdif thread (%p) created", p_spdif );

    return p_spdif->thread_id;
}

/*
 * Local functions
 */

/****************************************************************************
 * InitThread: initialize thread data and create output fifo
 ****************************************************************************/
static int InitThread( ac3_spdif_thread_t * p_spdif )
{
    boolean_t b_sync = 0;

    p_spdif->p_config->decoder_config.pf_init_bit_stream(
            &p_spdif->bit_stream,
            p_spdif->p_config->decoder_config.p_decoder_fifo,
            BitstreamCallback, (void*)p_spdif );

    /* Creating the audio output fifo */
    p_spdif->p_aout_fifo = aout_CreateFifo( AOUT_ADEC_SPDIF_FIFO, 1, 48000, 0,
                                            SPDIF_FRAME_SIZE, NULL );

    if( p_spdif->p_aout_fifo == NULL )
    {
        return -1;
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
        return -1;
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

    return 0;
}

/****************************************************************************
 * RunThread: loop that reads ac3 ES and transform it to
 * an spdif compliant stream.
 ****************************************************************************/
static void RunThread( ac3_spdif_thread_t * p_spdif )
{
    mtime_t     i_frame_time;
    boolean_t   b_sync;
    /* PTS of the current frame */
    mtime_t     i_current_pts = 0;

    /* Initializing the spdif decoder thread */
    if( InitThread( p_spdif ) )
    {
         p_spdif->p_fifo->b_error = 1;
    }

    /* Compute the theorical duration of an ac3 frame */
    i_frame_time = 1000000 * AC3_FRAME_SIZE /
                             p_spdif->ac3_info.i_sample_rate;

    while( !p_spdif->p_fifo->b_die && !p_spdif->p_fifo->b_error )
    {
        /* Handle the dates */
        if( p_spdif->i_real_pts )
        {
            if(i_current_pts + i_frame_time != p_spdif->i_real_pts)
            {
                intf_WarnMsg( 2, "spdif warning: date discontinuity (%d)",
                              p_spdif->i_real_pts - i_current_pts -
                              i_frame_time );
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
        ErrorThread( p_spdif );
    }

    /* End of the ac3 decoder thread */
    EndThread( p_spdif );

    return;
}

/*****************************************************************************
 * ErrorThread : ac3 spdif's RunThread() error loop
 *****************************************************************************/
static void ErrorThread( ac3_spdif_thread_t * p_spdif )
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
 * EndThread : ac3 spdif thread destruction
 *****************************************************************************/
static void EndThread( ac3_spdif_thread_t * p_spdif )
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
    free( p_spdif->p_config );
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
