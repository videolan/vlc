/*****************************************************************************
 * lpcm_decoder_thread.c: lpcm decoder thread
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: lpcm_decoder_thread.c,v 1.18 2001/11/06 18:13:21 massiot Exp $
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
#include "defs.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>                                              /* getpid() */
#endif

#include <stdio.h>                                           /* "intf_msg.h" */
#include <string.h>                                    /* memcpy(), memset() */
#include <stdlib.h>                                      /* malloc(), free() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */

#include "stream_control.h"
#include "input_ext-dec.h"

#include "audio_output.h"

#include "lpcm_decoder_thread.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int      InitThread              (lpcmdec_thread_t * p_adec);
static void     RunThread               (lpcmdec_thread_t * p_adec);
static void     ErrorThread             (lpcmdec_thread_t * p_adec);
static void     EndThread               (lpcmdec_thread_t * p_adec);

/*****************************************************************************
 * lpcmdec_CreateThread: creates an lpcm decoder thread
 *****************************************************************************/
vlc_thread_t lpcmdec_CreateThread( adec_config_t * p_config )
{
    lpcmdec_thread_t *   p_lpcmdec;
    intf_DbgMsg( "LPCM: creating lpcm decoder thread" );

    /* Allocate the memory needed to store the thread's structure */
    if( (p_lpcmdec = (lpcmdec_thread_t *)malloc (sizeof(lpcmdec_thread_t)) )
            == NULL) 
    {
        intf_ErrMsg( "LPCM : error : cannot create lpcmdec_thread_t" );
        return 0;
    }

    /*
     * Initialize the thread properties
     */
    p_lpcmdec->p_config = p_config;
    p_lpcmdec->p_fifo = p_config->decoder_config.p_decoder_fifo;

    /*
     * Initialize the output properties
     */
    p_lpcmdec->p_aout_fifo = NULL;

    /* Spawn the lpcm decoder thread */
    if( vlc_thread_create( &p_lpcmdec->thread_id, "lpcm decoder", 
                           (vlc_thread_func_t)RunThread, (void *)p_lpcmdec ) )
    {
        intf_ErrMsg( "LPCM : error : cannot spawn thread" );
        free (p_lpcmdec);
        return 0;
    }

    intf_DbgMsg( "LPCM Debug: lpcm decoder thread (%p) created\n", p_lpcmdec );
    return p_lpcmdec->thread_id;
}

/* Following functions are local */

/*****************************************************************************
 * InitThread : initialize an lpcm decoder thread
 *****************************************************************************/
static int InitThread (lpcmdec_thread_t * p_lpcmdec)
{

    intf_DbgMsg ( "lpcm Debug: initializing lpcm decoder thread %p", 
                   p_lpcmdec );

    /* Init the BitStream */
    p_lpcmdec->p_config->decoder_config.pf_init_bit_stream(
            &p_lpcmdec->bit_stream,
            p_lpcmdec->p_config->decoder_config.p_decoder_fifo,
            NULL, NULL);

    /* Creating the audio output fifo */
    p_lpcmdec->p_aout_fifo = aout_CreateFifo( AOUT_ADEC_STEREO_FIFO, 2, 48000,
                                            0, LPCMDEC_FRAME_SIZE/2, NULL  );
    if ( p_lpcmdec->p_aout_fifo == NULL )
    {
        return -1;
    }

    intf_DbgMsg( "LPCM Debug: lpcm decoder thread %p initialized\n", 
                 p_lpcmdec );
    return( 0 );
}

/*****************************************************************************
 * RunThread : lpcm decoder thread
 *****************************************************************************/
static void RunThread (lpcmdec_thread_t * p_lpcmdec)
{
    intf_DbgMsg( "LPCM Debug: running lpcm decoder thread (%p) (pid== %i)", 
                 p_lpcmdec, getpid() );

    /* Initializing the lpcm decoder thread */
    if (InitThread (p_lpcmdec)) 
    {
        p_lpcmdec->p_fifo->b_error = 1;
    }

    /* lpcm decoder thread's main loop */
   
    while ((!p_lpcmdec->p_fifo->b_die) && (!p_lpcmdec->p_fifo->b_error))
    {
        byte_t * buffer,p_temp[LPCMDEC_FRAME_SIZE];
        int i_loop;
        byte_t byte1, byte2;
    
        if( DECODER_FIFO_START(*p_lpcmdec->p_fifo)->i_pts )
        {
            p_lpcmdec->p_aout_fifo->date[p_lpcmdec->p_aout_fifo->l_end_frame] =
                DECODER_FIFO_START(*p_lpcmdec->p_fifo)->i_pts;
            DECODER_FIFO_START(*p_lpcmdec->p_fifo)->i_pts = 0;
        }
        else
        { 
            p_lpcmdec->p_aout_fifo->date[p_lpcmdec->p_aout_fifo->l_end_frame] =
                LAST_MDATE;
        }

        buffer = ((byte_t *)p_lpcmdec->p_aout_fifo->buffer) + 
                  (p_lpcmdec->p_aout_fifo->l_end_frame * LPCMDEC_FRAME_SIZE);
    
        RemoveBits32(&p_lpcmdec->bit_stream);
        byte1 = GetBits(&p_lpcmdec->bit_stream, 8) ;
        byte2 = GetBits(&p_lpcmdec->bit_stream, 8) ;
        
        /* I only have 2 test streams. As far as I understand
         * after the RemoveBits and the 2 GetBits, we should be exactly 
         * where we whant : the sync word : 0x0180.
         * If not, we got and find it. */
        while( ( byte1 != 0x01 || byte2 != 0x80 ) && (!p_lpcmdec->p_fifo->b_die)
                                           && (!p_lpcmdec->p_fifo->b_error) )
        {
            byte1 = byte2;
            byte2 = GetBits(&p_lpcmdec->bit_stream, 8);
        }
        
        GetChunk( &p_lpcmdec->bit_stream, p_temp, LPCMDEC_FRAME_SIZE);
        
        for( i_loop = 0; i_loop < LPCMDEC_FRAME_SIZE/2; i_loop++ )
        {
            buffer[2*i_loop]=p_temp[2*i_loop+1];
            buffer[2*i_loop+1]=p_temp[2*i_loop];
        }
        
        vlc_mutex_lock (&p_lpcmdec->p_aout_fifo->data_lock);
        p_lpcmdec->p_aout_fifo->l_end_frame = 
            (p_lpcmdec->p_aout_fifo->l_end_frame + 1) & AOUT_FIFO_SIZE;
        vlc_cond_signal (&p_lpcmdec->p_aout_fifo->data_wait);
        vlc_mutex_unlock (&p_lpcmdec->p_aout_fifo->data_lock);
        
        intf_DbgMsg( "LPCM Debug: %x", *buffer );

    }

    /* If b_error is set, the lpcm decoder thread enters the error loop */
    if (p_lpcmdec->p_fifo->b_error)
    {
        ErrorThread (p_lpcmdec);
    }

    /* End of the lpcm decoder thread */
    EndThread (p_lpcmdec);
}

/*****************************************************************************
 * ErrorThread : lpcm decoder's RunThread() error loop
 *****************************************************************************/
static void ErrorThread( lpcmdec_thread_t * p_lpcmdec )
{
    /* We take the lock, because we are going to read/write the start/end
     * indexes of the decoder fifo */
    vlc_mutex_lock( &p_lpcmdec->p_fifo->data_lock );

    /* Wait until a `die' order is sent */
    while( !p_lpcmdec->p_fifo->b_die ) 
    {
        /* Trash all received PES packets */
        while( !DECODER_FIFO_ISEMPTY(*p_lpcmdec->p_fifo) ) 
        {
            p_lpcmdec->p_fifo->pf_delete_pes( p_lpcmdec->p_fifo->p_packets_mgt,
                    DECODER_FIFO_START(*p_lpcmdec->p_fifo ));
            DECODER_FIFO_INCSTART( *p_lpcmdec->p_fifo );
        }

        /* Waiting for the input thread to put new PES packets in the fifo */
        vlc_cond_wait ( &p_lpcmdec->p_fifo->data_wait, 
                        &p_lpcmdec->p_fifo->data_lock );
    }

    /* We can release the lock before leaving */
    vlc_mutex_unlock( &p_lpcmdec->p_fifo->data_lock );
}

/*****************************************************************************
 * EndThread : lpcm decoder thread destruction
 *****************************************************************************/
static void EndThread( lpcmdec_thread_t * p_lpcmdec )
{
    intf_DbgMsg( "LPCM Debug: destroying lpcm decoder thread %p", p_lpcmdec );

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

    intf_DbgMsg( "LPCM Debug: lpcm decoder thread %p destroyed", p_lpcmdec );
}
