/*****************************************************************************
 * ac3_decoder_thread.c: ac3 decoder thread
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/

/*
 * TODO :
 *
 * - vérifier l'état de la fifo de sortie avant d'y stocker les samples
 *   décodés ;
 * - vlc_cond_signal() / vlc_cond_wait()
 *
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <unistd.h>                                              /* getpid() */

#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                            /* "input.h" */

#include "common.h"
#include "config.h"
#include "mtime.h"
#include "threads.h"
#include "debug.h"                                      /* "input_netlist.h" */

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */

#include "input.h"                                           /* pes_packet_t */
#include "input_netlist.h"                         /* input_NetlistFreePES() */
#include "decoder_fifo.h"         /* DECODER_FIFO_(ISEMPTY|START|INCSTART)() */

#include "audio_output.h"

#include "ac3_decoder.h"
#include "ac3_decoder_thread.h"

#define AC3DEC_FRAME_SIZE (2*1536) 
typedef s16 ac3dec_frame_t[ AC3DEC_FRAME_SIZE ];

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int      InitThread              ( ac3dec_thread_t * p_adec );
static void     RunThread               ( ac3dec_thread_t * p_adec );
static void     ErrorThread             ( ac3dec_thread_t * p_adec );
static void     EndThread               ( ac3dec_thread_t * p_adec );

/*****************************************************************************
 * ac3dec_CreateThread: creates an ac3 decoder thread
 *****************************************************************************/
ac3dec_thread_t * ac3dec_CreateThread( input_thread_t * p_input )
{
    ac3dec_thread_t *   p_ac3dec;

    intf_DbgMsg("ac3dec debug: creating ac3 decoder thread\n");

    /* Allocate the memory needed to store the thread's structure */
    if ( (p_ac3dec = (ac3dec_thread_t *)malloc( sizeof(ac3dec_thread_t) )) == NULL )
    {
        intf_ErrMsg("ac3dec error: not enough memory for ac3dec_CreateThread() to create the new thread\n");
        return( NULL );
    }

    /*
     * Initialize the thread properties
     */
    p_ac3dec->b_die = 0;
    p_ac3dec->b_error = 0;

    /*
     * Initialize the input properties
     */
    /* Initialize the decoder fifo's data lock and conditional variable and set
     * its buffer as empty */
    vlc_mutex_init( &p_ac3dec->fifo.data_lock );
    vlc_cond_init( &p_ac3dec->fifo.data_wait );
    p_ac3dec->fifo.i_start = 0;
    p_ac3dec->fifo.i_end = 0;

    /* Initialize the ac3 decoder structures */
    ac3_init (&p_ac3dec->ac3_decoder);

    /* Initialize the bit stream structure */
    p_ac3dec->p_input = p_input;

    /*
     * Initialize the output properties
     */
    p_ac3dec->p_aout = p_input->p_aout;
    p_ac3dec->p_aout_fifo = NULL;

    /* Spawn the ac3 decoder thread */
    if ( vlc_thread_create(&p_ac3dec->thread_id, "ac3 decoder", (vlc_thread_func_t)RunThread, (void *)p_ac3dec) )
    {
        intf_ErrMsg( "ac3dec error: can't spawn ac3 decoder thread\n" );
        free( p_ac3dec );
        return( NULL );
    }

    intf_DbgMsg( "ac3dec debug: ac3 decoder thread (%p) created\n", p_ac3dec );
    return( p_ac3dec );
}

/*****************************************************************************
 * ac3dec_DestroyThread: destroys an ac3 decoder thread
 *****************************************************************************/
void ac3dec_DestroyThread( ac3dec_thread_t * p_ac3dec )
{
    intf_DbgMsg( "ac3dec debug: requesting termination of ac3 decoder thread %p\n", p_ac3dec );

    /* Ask thread to kill itself */
    p_ac3dec->b_die = 1;

    /* Make sure the decoder thread leaves the GetByte() function */
    vlc_mutex_lock( &(p_ac3dec->fifo.data_lock) );
    vlc_cond_signal( &(p_ac3dec->fifo.data_wait) );
    vlc_mutex_unlock( &(p_ac3dec->fifo.data_lock) );

    /* Waiting for the decoder thread to exit */
    /* Remove this as soon as the "status" flag is implemented */
    vlc_thread_join( p_ac3dec->thread_id );
}

/* Following functions are local */

/*****************************************************************************
 * InitThread : initialize an ac3 decoder thread
 *****************************************************************************/
static int InitThread( ac3dec_thread_t * p_ac3dec )
{
    aout_fifo_t         aout_fifo;
    ac3_byte_stream_t * byte_stream;

    intf_DbgMsg( "ac3dec debug: initializing ac3 decoder thread %p\n", p_ac3dec );

    /* Our first job is to initialize the bit stream structure with the
     * beginning of the input stream */
    vlc_mutex_lock( &p_ac3dec->fifo.data_lock );
    while ( DECODER_FIFO_ISEMPTY(p_ac3dec->fifo) )
    {
        if ( p_ac3dec->b_die )
        {
            vlc_mutex_unlock( &p_ac3dec->fifo.data_lock );
            return( -1 );
        }
        vlc_cond_wait( &p_ac3dec->fifo.data_wait, &p_ac3dec->fifo.data_lock );
    }
    p_ac3dec->p_ts = DECODER_FIFO_START( p_ac3dec->fifo )->p_first_ts;
    byte_stream = ac3_byte_stream (&p_ac3dec->ac3_decoder);
    byte_stream->p_byte =
	p_ac3dec->p_ts->buffer + p_ac3dec->p_ts->i_payload_start;
    byte_stream->p_end =
	p_ac3dec->p_ts->buffer + p_ac3dec->p_ts->i_payload_end;
    byte_stream->info = p_ac3dec;
    vlc_mutex_unlock( &p_ac3dec->fifo.data_lock );

    aout_fifo.i_type = AOUT_ADEC_STEREO_FIFO;
    aout_fifo.i_channels = 2;
    aout_fifo.b_stereo = 1;

    aout_fifo.l_frame_size = AC3DEC_FRAME_SIZE;

    /* Creating the audio output fifo */
    if ( (p_ac3dec->p_aout_fifo = aout_CreateFifo(p_ac3dec->p_aout, &aout_fifo)) == NULL )
    {
        return( -1 );
    }

    intf_DbgMsg( "ac3dec debug: ac3 decoder thread %p initialized\n", p_ac3dec );
    return( 0 );
}

/*****************************************************************************
 * RunThread : ac3 decoder thread
 *****************************************************************************/
static void RunThread( ac3dec_thread_t * p_ac3dec )
{
    int sync;

    intf_DbgMsg( "ac3dec debug: running ac3 decoder thread (%p) (pid == %i)\n", p_ac3dec, getpid() );

    msleep( INPUT_PTS_DELAY );

    /* Initializing the ac3 decoder thread */
    if ( InitThread(p_ac3dec) ) /* XXX?? */
    {
        p_ac3dec->b_error = 1;
    }

    sync = 0;
    p_ac3dec->sync_ptr = 0;

    /* ac3 decoder thread's main loop */
    /* FIXME : do we have enough room to store the decoded frames ?? */
    while ( (!p_ac3dec->b_die) && (!p_ac3dec->b_error) )
    {
	s16 * buffer;
	ac3_sync_info_t sync_info;

	if (!sync) { /* have to find a synchro point */
	    int ptr;
	    ac3_byte_stream_t * p_byte_stream;

	    printf ("sync\n");

	    p_byte_stream = ac3_byte_stream (&p_ac3dec->ac3_decoder);

	    /* first read till next ac3 magic header */
	    do {
		ac3_byte_stream_next (p_byte_stream);
	    } while ((!p_ac3dec->sync_ptr) &&
		     (!p_ac3dec->b_die) &&
		     (!p_ac3dec->b_error));
	    /* skip the specified number of bytes */

	    ptr = p_ac3dec->sync_ptr;
	    while (--ptr && (!p_ac3dec->b_die) && (!p_ac3dec->b_error)) {
		if (p_byte_stream->p_byte >= p_byte_stream->p_end) {
		    ac3_byte_stream_next (p_byte_stream);		    
		}
		p_byte_stream->p_byte++;
	    }

	    /* we are in sync now */

	    sync = 1;
	    p_ac3dec->sync_ptr = 0;
	}

        if ( DECODER_FIFO_START(p_ac3dec->fifo)->b_has_pts )
        {
                p_ac3dec->p_aout_fifo->date[p_ac3dec->p_aout_fifo->l_end_frame] = DECODER_FIFO_START(p_ac3dec->fifo)->i_pts;
                DECODER_FIFO_START(p_ac3dec->fifo)->b_has_pts = 0;
        }
        else
        {
                p_ac3dec->p_aout_fifo->date[p_ac3dec->p_aout_fifo->l_end_frame] = LAST_MDATE;
        }

        if (ac3_sync_frame (&p_ac3dec->ac3_decoder, &sync_info)) {
	    sync = 0;
	    goto bad_frame;
	}

	p_ac3dec->p_aout_fifo->l_rate = sync_info.sample_rate;

	buffer = ((ac3dec_frame_t *)p_ac3dec->p_aout_fifo->buffer)[ p_ac3dec->p_aout_fifo->l_end_frame ];

	if (ac3_decode_frame (&p_ac3dec->ac3_decoder, buffer)) {
	    sync = 0;
	    goto bad_frame;
	}

	vlc_mutex_lock( &p_ac3dec->p_aout_fifo->data_lock );
	p_ac3dec->p_aout_fifo->l_end_frame = (p_ac3dec->p_aout_fifo->l_end_frame + 1) & AOUT_FIFO_SIZE;
	vlc_cond_signal( &p_ac3dec->p_aout_fifo->data_wait );
	vlc_mutex_unlock( &p_ac3dec->p_aout_fifo->data_lock );

bad_frame:
    }

    /* If b_error is set, the ac3 decoder thread enters the error loop */
    if ( p_ac3dec->b_error )
    {
        ErrorThread( p_ac3dec );
    }

    /* End of the ac3 decoder thread */
    EndThread( p_ac3dec );
}

/*****************************************************************************
 * ErrorThread : ac3 decoder's RunThread() error loop
 *****************************************************************************/
static void ErrorThread( ac3dec_thread_t * p_ac3dec )
{
    /* We take the lock, because we are going to read/write the start/end
     * indexes of the decoder fifo */
    vlc_mutex_lock( &p_ac3dec->fifo.data_lock );

    /* Wait until a `die' order is sent */
    while( !p_ac3dec->b_die )
    {
        /* Trash all received PES packets */
        while( !DECODER_FIFO_ISEMPTY(p_ac3dec->fifo) )
        {
            input_NetlistFreePES( p_ac3dec->p_input, DECODER_FIFO_START(p_ac3dec->fifo) );
            DECODER_FIFO_INCSTART( p_ac3dec->fifo );
        }

        /* Waiting for the input thread to put new PES packets in the fifo */
        vlc_cond_wait( &p_ac3dec->fifo.data_wait, &p_ac3dec->fifo.data_lock );
    }

    /* We can release the lock before leaving */
    vlc_mutex_unlock( &p_ac3dec->fifo.data_lock );
}

/*****************************************************************************
 * EndThread : ac3 decoder thread destruction
 *****************************************************************************/
static void EndThread( ac3dec_thread_t * p_ac3dec )
{
    intf_DbgMsg( "ac3dec debug: destroying ac3 decoder thread %p\n", p_ac3dec );

    /* If the audio output fifo was created, we destroy it */
    if ( p_ac3dec->p_aout_fifo != NULL )
    {
        aout_DestroyFifo( p_ac3dec->p_aout_fifo );

        /* Make sure the output thread leaves the NextFrame() function */
        vlc_mutex_lock( &(p_ac3dec->p_aout_fifo->data_lock) );
        vlc_cond_signal( &(p_ac3dec->p_aout_fifo->data_wait) );
        vlc_mutex_unlock( &(p_ac3dec->p_aout_fifo->data_lock) );
    }

    /* Destroy descriptor */
    free( p_ac3dec );

    intf_DbgMsg( "ac3dec debug: ac3 decoder thread %p destroyed\n", p_ac3dec );
}

void ac3_byte_stream_next (ac3_byte_stream_t * p_byte_stream)
{
    ac3dec_thread_t * p_ac3dec = p_byte_stream->info;

    /* We are looking for the next TS packet that contains real data,
     * and not just a PES header */
    do {
	/* We were reading the last TS packet of this PES packet... It's
	 * time to jump to the next PES packet */
	if (p_ac3dec->p_ts->p_next_ts == NULL) {
	    int ptr;

	    /* We are going to read/write the start and end indexes of the 
	     * decoder fifo and to use the fifo's conditional variable, 
	     * that's why we need to take the lock before */ 
	    vlc_mutex_lock (&p_ac3dec->fifo.data_lock);
	    
	    /* Is the input thread dying ? */
	    if (p_ac3dec->p_input->b_die) {
		vlc_mutex_unlock (&(p_ac3dec->fifo.data_lock));
		return;
	    }

	    /* We should increase the start index of the decoder fifo, but
             * if we do this now, the input thread could overwrite the
             * pointer to the current PES packet, and we weren't able to
             * give it back to the netlist. That's why we free the PES
             * packet first. */
	    input_NetlistFreePES (p_ac3dec->p_input, DECODER_FIFO_START(p_ac3dec->fifo) );

	    DECODER_FIFO_INCSTART (p_ac3dec->fifo);

	    while (DECODER_FIFO_ISEMPTY(p_ac3dec->fifo)) {
		vlc_cond_wait (&p_ac3dec->fifo.data_wait, &p_ac3dec->fifo.data_lock );

		if (p_ac3dec->p_input->b_die) {
		    vlc_mutex_unlock (&(p_ac3dec->fifo.data_lock));
		    return;
		}
	    }

	    /* The next byte could be found in the next PES packet */
	    p_ac3dec->p_ts = DECODER_FIFO_START (p_ac3dec->fifo)->p_first_ts;

	    /* parse ac3 magic header */
	    ptr = p_ac3dec->p_ts->buffer [p_ac3dec->p_ts->i_payload_start+2];
	    ptr <<= 8;
	    ptr |= p_ac3dec->p_ts->buffer [p_ac3dec->p_ts->i_payload_start+3];
	    p_ac3dec->sync_ptr = ptr;
	    p_ac3dec->p_ts->i_payload_start += 4;

	    /* We can release the fifo's data lock */
	    vlc_mutex_unlock (&p_ac3dec->fifo.data_lock);
	}

	/* Perhaps the next TS packet of the current PES packet contains 
	 * real data (ie its payload's size is greater than 0) */
	else {
	    p_ac3dec->p_ts = p_ac3dec->p_ts->p_next_ts;
	}
    } while (p_ac3dec->p_ts->i_payload_start == p_ac3dec->p_ts->i_payload_end);
    p_byte_stream->p_byte =
	p_ac3dec->p_ts->buffer + p_ac3dec->p_ts->i_payload_start; 
    p_byte_stream->p_end =
	p_ac3dec->p_ts->buffer + p_ac3dec->p_ts->i_payload_end; 
}
