/*****************************************************************************
 * audio_decoder.c: MPEG audio decoder thread
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: audio_decoder.c,v 1.46 2001/01/11 17:44:48 sam Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Michel Lespinasse <walken@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
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

/*
 * TODO :
 *
 * - optimiser les NeedBits() et les GetBits() du code là où c'est possible ;
 * - vlc_cond_signal() / vlc_cond_wait() ;
 *
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <unistd.h>                                              /* getpid() */

#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                                      /* malloc(), free() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "plugins.h"

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */
 
#include "stream_control.h"
#include "input_ext-dec.h"

#include "audio_output.h"               /* aout_fifo_t (for audio_decoder.h) */

#include "adec_generic.h"
#include "audio_decoder.h"
#include "adec_math.h"                                     /* DCT32(), PCM() */

#define ADEC_FRAME_SIZE (2*1152)

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int      InitThread             (adec_thread_t * p_adec);
static void     RunThread              (adec_thread_t * p_adec);
static void     ErrorThread            (adec_thread_t * p_adec);
static void     EndThread              (adec_thread_t * p_adec);

/*****************************************************************************
 * adec_CreateThread: creates an audio decoder thread
 *****************************************************************************
 * This function creates a new audio decoder thread, and returns a pointer to
 * its description. On error, it returns NULL.
 *****************************************************************************/
vlc_thread_t adec_CreateThread ( adec_config_t * p_config )
{
    adec_thread_t *     p_adec;

    intf_DbgMsg ( "adec debug: creating audio decoder thread" );

    /* Allocate the memory needed to store the thread's structure */
    if ( (p_adec = (adec_thread_t *)malloc (sizeof(adec_thread_t))) == NULL ) 
    {
        intf_ErrMsg ( "adec error: not enough memory for"
                      " adec_CreateThread() to create the new thread" );
        return 0;
    }

    /*
     * Initialize the thread properties
     */
    p_adec->p_config = p_config;
    p_adec->p_fifo = p_config->decoder_config.p_decoder_fifo;

    /*
     * Initialize the decoder properties
     */
    adec_Init ( p_adec );

    /*
     * Initialize the output properties
     */
    p_adec->p_aout = p_config->p_aout;
    p_adec->p_aout_fifo = NULL;

    /* Spawn the audio decoder thread */
    if ( vlc_thread_create(&p_adec->thread_id, "audio decoder",
         (vlc_thread_func_t)RunThread, (void *)p_adec) ) 
    {
        intf_ErrMsg ("adec error: can't spawn audio decoder thread");
        free (p_adec);
        return 0;
    }

    intf_DbgMsg ("adec debug: audio decoder thread (%p) created", p_adec);
    return p_adec->thread_id;
}

/* following functions are local */

/*****************************************************************************
 * InitThread : initialize an audio decoder thread
 *****************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization. It returns 0 on success.
 *****************************************************************************/
static int InitThread (adec_thread_t * p_adec)
{
    aout_fifo_t          aout_fifo;

    intf_DbgMsg ("adec debug: initializing audio decoder thread %p", p_adec);

    p_adec->p_config->decoder_config.pf_init_bit_stream( &p_adec->bit_stream,
        p_adec->p_config->decoder_config.p_decoder_fifo );

    aout_fifo.i_type = AOUT_ADEC_STEREO_FIFO;
    aout_fifo.i_channels = 2;
    aout_fifo.b_stereo = 1;
    aout_fifo.l_frame_size = ADEC_FRAME_SIZE;

    /* Creating the audio output fifo */
    if ( (p_adec->p_aout_fifo =
                aout_CreateFifo(p_adec->p_aout, &aout_fifo)) == NULL ) 
    {
        return -1;
    }

    intf_DbgMsg ( "adec debug: audio decoder thread %p initialized", p_adec );
    return 0;
}

/*****************************************************************************
 * RunThread : audio decoder thread
 *****************************************************************************
 * Audio decoder thread. This function does only returns when the thread is
 * terminated.
 *****************************************************************************/
static void RunThread (adec_thread_t * p_adec)
{
    int sync;

    intf_DbgMsg ( "adec debug: running audio decoder thread (%p) (pid == %i)",
                  p_adec, getpid() );

    /* You really suck */
    //msleep ( INPUT_PTS_DELAY );

    /* Initializing the audio decoder thread */
    p_adec->p_fifo->b_error = InitThread (p_adec);

    sync = 0;

    /* Audio decoder thread's main loop */
    while( (!p_adec->p_fifo->b_die) && (!p_adec->p_fifo->b_error) )
    {
        s16 * buffer;
        adec_sync_info_t sync_info;

        if( DECODER_FIFO_START( *p_adec->p_fifo)->i_pts )
        {
            p_adec->p_aout_fifo->date[p_adec->p_aout_fifo->l_end_frame] =
                DECODER_FIFO_START( *p_adec->p_fifo )->i_pts;
            DECODER_FIFO_START(*p_adec->p_fifo)->i_pts = 0;
        }
        else
        {
            p_adec->p_aout_fifo->date[p_adec->p_aout_fifo->l_end_frame] =
                LAST_MDATE;
        }

        if( ! adec_SyncFrame (p_adec, &sync_info) )
        {
            sync = 1;

            p_adec->p_aout_fifo->l_rate = sync_info.sample_rate;

            buffer = ((s16 *)p_adec->p_aout_fifo->buffer)
                        + (p_adec->p_aout_fifo->l_end_frame * ADEC_FRAME_SIZE);

            if( adec_DecodeFrame (p_adec, buffer) )
            {
                /* Ouch, failed decoding... We'll have to resync */
                sync = 0;
            }
            else
            {
                vlc_mutex_lock (&p_adec->p_aout_fifo->data_lock);
    
                p_adec->p_aout_fifo->l_end_frame =
                    (p_adec->p_aout_fifo->l_end_frame + 1) & AOUT_FIFO_SIZE;
                vlc_cond_signal (&p_adec->p_aout_fifo->data_wait);
                vlc_mutex_unlock (&p_adec->p_aout_fifo->data_lock);
            }
        }
    }

    /* If b_error is set, the audio decoder thread enters the error loop */
    if( p_adec->p_fifo->b_error ) 
    {
        ErrorThread( p_adec );
    }

    /* End of the audio decoder thread */
    EndThread( p_adec );
}

/*****************************************************************************
 * ErrorThread : audio decoder's RunThread() error loop
 *****************************************************************************
 * This function is called when an error occured during thread main's loop. The
 * thread can still receive feed, but must be ready to terminate as soon as
 * possible.
 *****************************************************************************/
static void ErrorThread ( adec_thread_t *p_adec )
{
    /* We take the lock, because we are going to read/write the start/end
     * indexes of the decoder fifo */
    vlc_mutex_lock ( &p_adec->p_fifo->data_lock );

    /* Wait until a `die' order is sent */
    while ( !p_adec->p_fifo->b_die ) 
    {
        /* Trash all received PES packets */
        while ( !DECODER_FIFO_ISEMPTY(*p_adec->p_fifo) ) 
        {
            p_adec->p_fifo->pf_delete_pes ( p_adec->p_fifo->p_packets_mgt,
                                   DECODER_FIFO_START(*p_adec->p_fifo) );
            DECODER_FIFO_INCSTART ( *p_adec->p_fifo );
        }

        /* Waiting for the input thread to put new PES packets in the fifo */
        vlc_cond_wait ( &p_adec->p_fifo->data_wait, &p_adec->p_fifo->data_lock );
    }

    /* We can release the lock before leaving */
    vlc_mutex_unlock ( &p_adec->p_fifo->data_lock );
}

/*****************************************************************************
 * EndThread : audio decoder thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization.
 *****************************************************************************/
static void EndThread ( adec_thread_t *p_adec )
{
    intf_DbgMsg ( "adec debug: destroying audio decoder thread %p", p_adec );

    /* If the audio output fifo was created, we destroy it */
    if ( p_adec->p_aout_fifo != NULL ) 
    {
        aout_DestroyFifo ( p_adec->p_aout_fifo );

        /* Make sure the output thread leaves the NextFrame() function */
        vlc_mutex_lock (&(p_adec->p_aout_fifo->data_lock));
        vlc_cond_signal (&(p_adec->p_aout_fifo->data_wait));
        vlc_mutex_unlock (&(p_adec->p_aout_fifo->data_lock));
    }
    /* Destroy descriptor */
    free( p_adec->p_config );
    free( p_adec );

    intf_DbgMsg ("adec debug: audio decoder thread %p destroyed", p_adec);
}

