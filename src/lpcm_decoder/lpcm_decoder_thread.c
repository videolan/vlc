/*****************************************************************************
 * lpcm_decoder_thread.c: lpcm decoder thread
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: lpcm_decoder_thread.c,v 1.12 2001/04/02 23:30:41 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

#include <unistd.h>                                              /* getpid() */

#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                                      /* malloc(), free() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */

#include "stream_control.h"
#include "input_ext-dec.h"

#include "audio_output.h"

#include "lpcm_decoder.h"
#include "lpcm_decoder_thread.h"

#define LPCMDEC_FRAME_SIZE (2*1536)                        /* May be useless */

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
vlc_thread_t lpcmdec_CreateThread (adec_config_t * p_config)
{
    lpcmdec_thread_t *   p_lpcmdec;
    intf_DbgMsg ( "lpcm: creating lpcm decoder thread" );

    /* Allocate the memory needed to store the thread's structure */
    if ((p_lpcmdec = (lpcmdec_thread_t *)malloc (sizeof(lpcmdec_thread_t))) == NULL) {
        intf_ErrMsg ( "lpcm error: cannot create lpcmdec_thread_t" );
        return 0;
    }

    /*
     * Initialize the thread properties
     */
    p_lpcmdec->p_config = p_config;
    p_lpcmdec->p_fifo = p_config->decoder_config.p_decoder_fifo;


    /* Initialize the lpcm decoder structures */
    lpcm_init (&p_lpcmdec->lpcm_decoder);

    /*
     * Initialize the output properties
     */
    p_lpcmdec->p_aout = p_config->p_aout;
    p_lpcmdec->p_aout_fifo = NULL;

    /* Spawn the lpcm decoder thread */
    if (vlc_thread_create(&p_lpcmdec->thread_id, "lpcm decoder", (vlc_thread_func_t)RunThread, (void *)p_lpcmdec)) {
        intf_ErrMsg  ( "lpcm error: cannot spawn thread" );
        free (p_lpcmdec);
        return 0;
    }

    intf_DbgMsg ( "LPCM Debug: lpcm decoder thread (%p) created", p_lpcmdec );
    return p_lpcmdec->thread_id;
}

/* Following functions are local */

/*****************************************************************************
 * InitThread : initialize an lpcm decoder thread
 *****************************************************************************/
static int InitThread (lpcmdec_thread_t * p_lpcmdec)
{
    aout_fifo_t         aout_fifo;
    lpcm_byte_stream_t * byte_stream;

    intf_DbgMsg ( "LPCM Debug: initializing lpcm decoder thread %p", p_lpcmdec );

    /* Our first job is to initialize the bit stream structure with the
     * beginning of the input stream */
    vlc_mutex_lock (&p_lpcmdec->p_fifo->data_lock);
    while (DECODER_FIFO_ISEMPTY(*p_lpcmdec->p_fifo)) {
        if (p_lpcmdec->p_fifo->b_die) {
            vlc_mutex_unlock (&p_lpcmdec->p_fifo->data_lock);
            return -1;
        }
        vlc_cond_wait (&p_lpcmdec->p_fifo->data_wait, &p_lpcmdec->p_fifo->data_lock);
    }
    p_lpcmdec->p_data = DECODER_FIFO_START (*p_lpcmdec->p_fifo)->p_first;
    byte_stream = lpcm_byte_stream (&p_lpcmdec->lpcm_decoder);
    byte_stream->p_byte = p_lpcmdec->p_data->p_payload_start;
    byte_stream->p_end = p_lpcmdec->p_data->p_payload_end;
    byte_stream->info = p_lpcmdec;
    vlc_mutex_unlock (&p_lpcmdec->p_fifo->data_lock);

    aout_fifo.i_type = AOUT_ADEC_STEREO_FIFO;
    aout_fifo.i_channels = 2;
    aout_fifo.b_stereo = 1;

    aout_fifo.l_frame_size = LPCMDEC_FRAME_SIZE;

    /* Creating the audio output fifo */
    if ((p_lpcmdec->p_aout_fifo = aout_CreateFifo(p_lpcmdec->p_aout, &aout_fifo)) == NULL) {
        return -1;
    }

    intf_DbgMsg ( "LPCM Debug: lpcm decoder thread %p initialized", p_lpcmdec );
    return 0;
}

/*****************************************************************************
 * RunThread : lpcm decoder thread
 *****************************************************************************/
static void RunThread (lpcmdec_thread_t * p_lpcmdec)
{
    int sync;

    intf_DbgMsg( "LPCM Debug: running lpcm decoder thread (%p) (pid== %i)", p_lpcmdec, getpid() );

    /* Fucking holy piece of shit ! */
    //msleep (INPUT_PTS_DELAY);

    /* Initializing the lpcm decoder thread */
    if (InitThread (p_lpcmdec)) 
    {
        p_lpcmdec->p_fifo->b_error = 1;
    }

    sync = 0;
    p_lpcmdec->sync_ptr = 0;

    /* lpcm decoder thread's main loop */
    /* FIXME : do we have enough room to store the decoded frames ?? */
   
    while ((!p_lpcmdec->p_fifo->b_die) && (!p_lpcmdec->p_fifo->b_error))
    {
        s16 * buffer;
        lpcm_sync_info_t sync_info;

        if (!sync)
        {
            /* have to find a synchro point */
        }
    
        if (DECODER_FIFO_START(*p_lpcmdec->p_fifo)->i_pts)
        {
            p_lpcmdec->p_aout_fifo->date[p_lpcmdec->p_aout_fifo->l_end_frame] = DECODER_FIFO_START(*p_lpcmdec->p_fifo)->i_pts;
            DECODER_FIFO_START(*p_lpcmdec->p_fifo)->i_pts = 0;
        }
        else
        {
            p_lpcmdec->p_aout_fifo->date[p_lpcmdec->p_aout_fifo->l_end_frame] = LAST_MDATE;
        }

        p_lpcmdec->p_aout_fifo->l_rate = sync_info.sample_rate;

        buffer = ((s16 *)p_lpcmdec->p_aout_fifo->buffer) + (p_lpcmdec->p_aout_fifo->l_end_frame * LPCMDEC_FRAME_SIZE);

        if (lpcm_decode_frame (&p_lpcmdec->lpcm_decoder, buffer))
        {
            sync = 0;
            goto bad_frame;
        }

        vlc_mutex_lock (&p_lpcmdec->p_aout_fifo->data_lock);
        p_lpcmdec->p_aout_fifo->l_end_frame = (p_lpcmdec->p_aout_fifo->l_end_frame + 1) & AOUT_FIFO_SIZE;
        vlc_cond_signal (&p_lpcmdec->p_aout_fifo->data_wait);
        vlc_mutex_unlock (&p_lpcmdec->p_aout_fifo->data_lock);

        intf_DbgMsg( "LPCM Debug: %x", *buffer );
        bad_frame:
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
static void ErrorThread (lpcmdec_thread_t * p_lpcmdec)
{
    /* We take the lock, because we are going to read/write the start/end
     * indexes of the decoder fifo */
    vlc_mutex_lock (&p_lpcmdec->p_fifo->data_lock);

    /* Wait until a `die' order is sent */
    while (!p_lpcmdec->p_fifo->b_die) {
        /* Trash all received PES packets */
        while (!DECODER_FIFO_ISEMPTY(*p_lpcmdec->p_fifo)) {
            p_lpcmdec->p_fifo->pf_delete_pes(p_lpcmdec->p_fifo->p_packets_mgt,
                    DECODER_FIFO_START(*p_lpcmdec->p_fifo));
            DECODER_FIFO_INCSTART (*p_lpcmdec->p_fifo);
        }

        /* Waiting for the input thread to put new PES packets in the fifo */
        vlc_cond_wait (&p_lpcmdec->p_fifo->data_wait, &p_lpcmdec->p_fifo->data_lock);
    }

    /* We can release the lock before leaving */
    vlc_mutex_unlock (&p_lpcmdec->p_fifo->data_lock);
}

/*****************************************************************************
 * EndThread : lpcm decoder thread destruction
 *****************************************************************************/
static void EndThread (lpcmdec_thread_t * p_lpcmdec)
{
    intf_DbgMsg( "LPCM Debug: destroying lpcm decoder thread %p", p_lpcmdec );

    /* If the audio output fifo was created, we destroy it */
    if (p_lpcmdec->p_aout_fifo != NULL) {
        aout_DestroyFifo (p_lpcmdec->p_aout_fifo);

        /* Make sure the output thread leaves the NextFrame() function */
        vlc_mutex_lock (&(p_lpcmdec->p_aout_fifo->data_lock));
        vlc_cond_signal (&(p_lpcmdec->p_aout_fifo->data_wait));
        vlc_mutex_unlock (&(p_lpcmdec->p_aout_fifo->data_lock));
    }

    /* Destroy descriptor */
    free (p_lpcmdec);

    intf_DbgMsg( "LPCM Debug: lpcm decoder thread %p destroyed", p_lpcmdec );
}

void lpcm_byte_stream_next (lpcm_byte_stream_t * p_byte_stream)
{
//    lpcmdec_thread_t * p_lpcmdec = p_byte_stream->info;

    /* We are looking for the next TS packet that contains real data,
     * and not just a PES header */
}
