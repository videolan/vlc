/*****************************************************************************
 * spu_decoder.c : spu decoder thread
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
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

/* repompé sur video_decoder.c
 * FIXME: passer en terminate/destroy avec les signaux supplémentaires ?? */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                          /* for input.h */
#include <unistd.h>                                              /* getpid() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "intf_msg.h"
#include "debug.h"                                                 /* ASSERT */

#include "input.h"
#include "input_netlist.h"
#include "decoder_fifo.h"

#include "spu_decoder.h"

/*
 * Local prototypes
 */
static int      InitThread          ( spudec_thread_t *p_spudec );
static void     RunThread           ( spudec_thread_t *p_spudec );
static void     ErrorThread         ( spudec_thread_t *p_spudec );
static void     EndThread           ( spudec_thread_t *p_spudec );

/*****************************************************************************
 * spudec_CreateThread: create a spu decoder thread
 *****************************************************************************/
spudec_thread_t * spudec_CreateThread( input_thread_t * p_input )
{
    spudec_thread_t *     p_spudec;

    intf_DbgMsg("spudec debug: creating spu decoder thread\n");

    /* Allocate the memory needed to store the thread's structure */
    if ( (p_spudec = (spudec_thread_t *)malloc( sizeof(spudec_thread_t) )) == NULL )
    {
        intf_ErrMsg("spudec error: not enough memory for spudec_CreateThread() to create the new thread\n");
        return( NULL );
    }

    /*
     * Initialize the thread properties
     */
    p_spudec->b_die = 0;
    p_spudec->b_error = 0;

    /*
     * Initialize the input properties
     */
    /* Initialize the decoder fifo's data lock and conditional variable and set
     * its buffer as empty */
    vlc_mutex_init( &p_spudec->fifo.data_lock );
    vlc_cond_init( &p_spudec->fifo.data_wait );
    p_spudec->fifo.i_start = 0;
    p_spudec->fifo.i_end = 0;
    /* Initialize the bit stream structure */
    p_spudec->bit_stream.p_input = p_input;
    p_spudec->bit_stream.p_decoder_fifo = &p_spudec->fifo;
    p_spudec->bit_stream.fifo.buffer = 0;
    p_spudec->bit_stream.fifo.i_available = 0;

    /* Spawn the spu decoder thread */
    if ( vlc_thread_create(&p_spudec->thread_id, "spu decoder",
         (vlc_thread_func_t)RunThread, (void *)p_spudec) )
    {
        intf_ErrMsg("spudec error: can't spawn spu decoder thread\n");
        free( p_spudec );
        return( NULL );
    }

    intf_DbgMsg("spudec debug: spu decoder thread (%p) created\n", p_spudec);
    return( p_spudec );
}

/*****************************************************************************
 * spudec_DestroyThread: destroy a spu decoder thread
 *****************************************************************************
 * Destroy and terminate thread. This function will return 0 if the thread could
 * be destroyed, and non 0 else. The last case probably means that the thread
 * was still active, and another try may succeed.
 *****************************************************************************/
void spudec_DestroyThread( spudec_thread_t *p_spudec )
{
    intf_DbgMsg("spudec debug: requesting termination of spu decoder thread %p\n", p_spudec);

    /* Ask thread to kill itself */
    p_spudec->b_die = 1;

    /* Warn the decoder that we're quitting */
    vlc_mutex_lock( &p_spudec->fifo.data_lock );
    vlc_cond_signal( &p_spudec->fifo.data_wait );
    vlc_mutex_unlock( &p_spudec->fifo.data_lock );

    /* Waiting for the decoder thread to exit */
    /* Remove this as soon as the "status" flag is implemented */
    vlc_thread_join( p_spudec->thread_id );
}

/* following functions are local */

/*****************************************************************************
 * InitThread: initialize spu decoder thread
 *****************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization. It returns 0 on success. Note that the thread's flag are not
 * modified inside this function.
 *****************************************************************************/
static int InitThread( spudec_thread_t *p_spudec )
{
    intf_DbgMsg("spudec debug: initializing spu decoder thread %p\n", p_spudec);

    /* Our first job is to initialize the bit stream structure with the
     * beginning of the input stream */
    vlc_mutex_lock( &p_spudec->fifo.data_lock );
    while ( DECODER_FIFO_ISEMPTY(p_spudec->fifo) )
    {
        if ( p_spudec->b_die )
        {
            vlc_mutex_unlock( &p_spudec->fifo.data_lock );
            return( 1 );
        }
        vlc_cond_wait( &p_spudec->fifo.data_wait, &p_spudec->fifo.data_lock );
    }

    p_spudec->bit_stream.p_ts = DECODER_FIFO_START( p_spudec->fifo )->p_first_ts;
    p_spudec->bit_stream.p_byte = p_spudec->bit_stream.p_ts->buffer + p_spudec->bit_stream.p_ts->i_payload_start;
    p_spudec->bit_stream.p_end = p_spudec->bit_stream.p_ts->buffer + p_spudec->bit_stream.p_ts->i_payload_end;
    vlc_mutex_unlock( &p_spudec->fifo.data_lock );

    /* Mark thread as running and return */
    intf_DbgMsg( "spudec debug: InitThread(%p) succeeded\n", p_spudec );
    return( 0 );
}

/*****************************************************************************
 * RunThread: spu decoder thread
 *****************************************************************************
 * spu decoder thread. This function does only return when the thread is
 * terminated.
 *****************************************************************************/
static void RunThread( spudec_thread_t *p_spudec )
{
    intf_DbgMsg("spudec debug: running spu decoder thread (%p) (pid == %i)\n",
        p_spudec, getpid());

    /*
     * Initialize thread and free configuration
     */
    p_spudec->b_error = InitThread( p_spudec );

    p_spudec->b_run = 1;

    /*
     * Main loop - it is not executed if an error occured during
     * initialization
     */
    vlc_mutex_lock( &p_spudec->fifo.data_lock );
    while( (!p_spudec->b_die) && (!p_spudec->b_error) )
    {
        /* Trash all received PES packets */
        while( !DECODER_FIFO_ISEMPTY(p_spudec->fifo) )
        {
            input_NetlistFreePES( p_spudec->bit_stream.p_input, DECODER_FIFO_START(p_spudec->fifo) );
            DECODER_FIFO_INCSTART( p_spudec->fifo );
        }
        /* Waiting for the input thread to put new PES packets in the fifo */
        vlc_cond_wait( &p_spudec->fifo.data_wait, &p_spudec->fifo.data_lock );
    }
    vlc_mutex_unlock( &p_spudec->fifo.data_lock );

    /*
     * Error loop
     */
    if( p_spudec->b_error )
    {
        ErrorThread( p_spudec );
    }

    p_spudec->b_run = 0;

    /* End of thread */
    EndThread( p_spudec );
}

/*****************************************************************************
 * ErrorThread: RunThread() error loop
 *****************************************************************************
 * This function is called when an error occured during thread main's loop. The
 * thread can still receive feed, but must be ready to terminate as soon as
 * possible.
 *****************************************************************************/
static void ErrorThread( spudec_thread_t *p_spudec )
{
    /* We take the lock, because we are going to read/write the start/end
     * indexes of the decoder fifo */
    vlc_mutex_lock( &p_spudec->fifo.data_lock );

    /* Wait until a `die' order is sent */
    while( !p_spudec->b_die )
    {
        /* Trash all received PES packets */
        while( !DECODER_FIFO_ISEMPTY(p_spudec->fifo) )
        {
            input_NetlistFreePES( p_spudec->bit_stream.p_input, DECODER_FIFO_START(p_spudec->fifo) );
            DECODER_FIFO_INCSTART( p_spudec->fifo );
        }

        /* Waiting for the input thread to put new PES packets in the fifo */
        vlc_cond_wait( &p_spudec->fifo.data_wait, &p_spudec->fifo.data_lock );
    }

    /* We can release the lock before leaving */
    vlc_mutex_unlock( &p_spudec->fifo.data_lock );
}

/*****************************************************************************
 * EndThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessfull
 * initialization.
 *****************************************************************************/
static void EndThread( spudec_thread_t *p_spudec )
{
    intf_DbgMsg( "spudec debug: destroying spu decoder thread %p\n", p_spudec );
    free( p_spudec );
    intf_DbgMsg( "spudec debug: spu decoder thread %p destroyed\n", p_spudec);
}
