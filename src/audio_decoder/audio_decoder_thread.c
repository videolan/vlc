/*****************************************************************************
 * audio_decoder.c: MPEG1 Layer I-II audio decoder thread
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
#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                            /* "input.h" */

#include "threads.h"
#include "common.h"
#include "config.h"
#include "mtime.h"
#include "plugins.h"
#include "debug.h"                                      /* "input_netlist.h" */

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */

#include "input.h"                                           /* pes_packet_t */
#include "input_netlist.h"                         /* input_NetlistFreePES() */
#include "decoder_fifo.h"         /* DECODER_FIFO_(ISEMPTY|START|INCSTART)() */

#include "audio_output.h"               /* aout_fifo_t (for audio_decoder.h) */

#include "audio_constants.h"
#include "audio_decoder.h"
#include "audio_decoder_thread.h"
#include "audio_math.h"                                    /* DCT32(), PCM() */

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
adec_thread_t * adec_CreateThread (input_thread_t * p_input)
{
    adec_thread_t *     p_adec;

    intf_DbgMsg ("adec debug: creating audio decoder thread\n");

    /* Allocate the memory needed to store the thread's structure */
    if ((p_adec = (adec_thread_t *)malloc (sizeof(adec_thread_t))) == NULL) {
        intf_ErrMsg ("adec error: not enough memory for adec_CreateThread() to create the new thread\n");
        return NULL;
    }

    /*
     * Initialize the thread properties
     */
    p_adec->b_die = 0;
    p_adec->b_error = 0;

    /*
     * Initialize the input properties
     */
    /* Initialize the decoder fifo's data lock and conditional variable and set
     * its buffer as empty */
    vlc_mutex_init (&p_adec->fifo.data_lock);
    vlc_cond_init (&p_adec->fifo.data_wait);
    p_adec->fifo.i_start = 0;
    p_adec->fifo.i_end = 0;

    /* Initialize the bit stream structure */
    p_adec->p_input = p_input;

    /*
     * Initialize the decoder properties
     */
    adec_init (&p_adec->audio_decoder);

    /*
     * Initialize the output properties
     */
    p_adec->p_aout = p_input->p_aout;
    p_adec->p_aout_fifo = NULL;

    /* Spawn the audio decoder thread */
    if (vlc_thread_create(&p_adec->thread_id, "audio decoder", (vlc_thread_func_t)RunThread, (void *)p_adec)) {
        intf_ErrMsg ("adec error: can't spawn audio decoder thread\n");
        free (p_adec);
        return NULL;
    }

    intf_DbgMsg ("adec debug: audio decoder thread (%p) created\n", p_adec);
    return p_adec;
}

/*****************************************************************************
 * adec_DestroyThread: destroys an audio decoder thread
 *****************************************************************************
 * This function asks an audio decoder thread to terminate. This function has
 * not to wait until the decoder thread has really died, because the killer (ie
 * this function's caller) is the input thread, that's why we are sure that no
 * other thread will try to access to this thread's descriptor after its
 * destruction.
 *****************************************************************************/
void adec_DestroyThread (adec_thread_t * p_adec)
{
    intf_DbgMsg ("adec debug: requesting termination of audio decoder thread %p\n", p_adec);

    /* Ask thread to kill itself */
    p_adec->b_die = 1;

    /* Make sure the decoder thread leaves the GetByte() function */
    vlc_mutex_lock (&(p_adec->fifo.data_lock));
    vlc_cond_signal (&(p_adec->fifo.data_wait));
    vlc_mutex_unlock (&(p_adec->fifo.data_lock));

    /* Waiting for the decoder thread to exit */
    /* Remove this as soon as the "status" flag is implemented */
    vlc_thread_join (p_adec->thread_id);
}

/*****************************************************************************
 * InitThread : initialize an audio decoder thread
 *****************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization. It returns 0 on success.
 *****************************************************************************/
static int InitThread (adec_thread_t * p_adec)
{
    aout_fifo_t          aout_fifo;
    adec_byte_stream_t * byte_stream;

    intf_DbgMsg ("adec debug: initializing audio decoder thread %p\n", p_adec);

    /* Our first job is to initialize the bit stream structure with the
     * beginning of the input stream */
    vlc_mutex_lock (&p_adec->fifo.data_lock);
    while (DECODER_FIFO_ISEMPTY(p_adec->fifo)) {
        if (p_adec->b_die) {
            vlc_mutex_unlock (&p_adec->fifo.data_lock);
            return -1;
        }
        vlc_cond_wait (&p_adec->fifo.data_wait, &p_adec->fifo.data_lock);
    }
    p_adec->p_ts = DECODER_FIFO_START (p_adec->fifo)->p_first_ts;
    byte_stream = adec_byte_stream (&p_adec->audio_decoder);
    byte_stream->p_byte =
	p_adec->p_ts->buffer + p_adec->p_ts->i_payload_start;
    byte_stream->p_end =
	p_adec->p_ts->buffer + p_adec->p_ts->i_payload_end;
    byte_stream->info = p_adec;
    vlc_mutex_unlock (&p_adec->fifo.data_lock);

    aout_fifo.i_type = AOUT_ADEC_STEREO_FIFO;
    aout_fifo.i_channels = 2;
    aout_fifo.b_stereo = 1;
    aout_fifo.l_frame_size = ADEC_FRAME_SIZE;

    /* Creating the audio output fifo */
    if ((p_adec->p_aout_fifo = aout_CreateFifo(p_adec->p_aout, &aout_fifo)) == NULL) {
        return -1;
    }

    intf_DbgMsg ("adec debug: audio decoder thread %p initialized\n", p_adec);
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

    intf_DbgMsg ("adec debug: running audio decoder thread (%p) (pid == %i)\n", p_adec, getpid());

    msleep (INPUT_PTS_DELAY);

    /* Initializing the audio decoder thread */
    if (InitThread (p_adec)) {
        p_adec->b_error = 1;
    }

    sync = 0;

    /* Audio decoder thread's main loop */
    while ((!p_adec->b_die) && (!p_adec->b_error)) {
	s16 * buffer;
	adec_sync_info_t sync_info;

	if (!sync) {	/* have to find a synchro point */
	    adec_byte_stream_t * p_byte_stream;

	    printf ("sync\n");

	    p_adec->align = 0;

	    p_byte_stream = adec_byte_stream (&p_adec->audio_decoder);
	    do {
		adec_byte_stream_next (p_byte_stream);
	    } while ((!p_adec->align) &&
		     (!p_adec->b_die) && 
		     (!p_adec->b_error));

	    sync = 1;
	}

	if (DECODER_FIFO_START(p_adec->fifo)->b_has_pts) {
	    p_adec->p_aout_fifo->date[p_adec->p_aout_fifo->l_end_frame] = DECODER_FIFO_START(p_adec->fifo)->i_pts;
	    DECODER_FIFO_START(p_adec->fifo)->b_has_pts = 0;
	} else {
	    p_adec->p_aout_fifo->date[p_adec->p_aout_fifo->l_end_frame] = LAST_MDATE;
	}

	if (adec_sync_frame (&p_adec->audio_decoder, &sync_info)) {
	    sync = 0;
	    goto bad_frame;
	}

	p_adec->p_aout_fifo->l_rate = sync_info.sample_rate;

	buffer = ((s16 *)p_adec->p_aout_fifo->buffer) + (p_adec->p_aout_fifo->l_end_frame * ADEC_FRAME_SIZE);

	if (adec_decode_frame (&p_adec->audio_decoder, buffer)) {
	    sync = 0;
	    goto bad_frame;
	}

	vlc_mutex_lock (&p_adec->p_aout_fifo->data_lock);

	p_adec->p_aout_fifo->l_end_frame = (p_adec->p_aout_fifo->l_end_frame + 1) & AOUT_FIFO_SIZE;
	vlc_cond_signal (&p_adec->p_aout_fifo->data_wait);
	vlc_mutex_unlock (&p_adec->p_aout_fifo->data_lock);

    bad_frame:
    }

    /* If b_error is set, the audio decoder thread enters the error loop */
    if (p_adec->b_error) {
        ErrorThread (p_adec);
    }

    /* End of the audio decoder thread */
    EndThread (p_adec);
}

/*****************************************************************************
 * ErrorThread : audio decoder's RunThread() error loop
 *****************************************************************************
 * This function is called when an error occured during thread main's loop. The
 * thread can still receive feed, but must be ready to terminate as soon as
 * possible.
 *****************************************************************************/
static void ErrorThread (adec_thread_t *p_adec)
{
    /* We take the lock, because we are going to read/write the start/end
     * indexes of the decoder fifo */
    vlc_mutex_lock (&p_adec->fifo.data_lock);

    /* Wait until a `die' order is sent */
    while (!p_adec->b_die) {
        /* Trash all received PES packets */
        while (!DECODER_FIFO_ISEMPTY(p_adec->fifo)) {
            input_NetlistFreePES (p_adec->p_input, DECODER_FIFO_START(p_adec->fifo));
            DECODER_FIFO_INCSTART (p_adec->fifo);
        }

        /* Waiting for the input thread to put new PES packets in the fifo */
        vlc_cond_wait (&p_adec->fifo.data_wait, &p_adec->fifo.data_lock);
    }

    /* We can release the lock before leaving */
    vlc_mutex_unlock (&p_adec->fifo.data_lock);
}

/*****************************************************************************
 * EndThread : audio decoder thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessfull
 * initialization.
 *****************************************************************************/
static void EndThread (adec_thread_t *p_adec)
{
    intf_DbgMsg ("adec debug: destroying audio decoder thread %p\n", p_adec);

    /* If the audio output fifo was created, we destroy it */
    if (p_adec->p_aout_fifo != NULL) {
        aout_DestroyFifo (p_adec->p_aout_fifo);

        /* Make sure the output thread leaves the NextFrame() function */
        vlc_mutex_lock (&(p_adec->p_aout_fifo->data_lock));
        vlc_cond_signal (&(p_adec->p_aout_fifo->data_wait));
        vlc_mutex_unlock (&(p_adec->p_aout_fifo->data_lock));
    }
    /* Destroy descriptor */
    free (p_adec);

    intf_DbgMsg ("adec debug: audio decoder thread %p destroyed\n", p_adec);
}

void adec_byte_stream_next (adec_byte_stream_t * p_byte_stream)
{
    adec_thread_t * p_adec = p_byte_stream->info;

    /* We are looking for the next TS packet that contains real data,
     * and not just a PES header */
    do {
        /* We were reading the last TS packet of this PES packet... It's
         * time to jump to the next PES packet */
        if (p_adec->p_ts->p_next_ts == NULL) {
            /* We are going to read/write the start and end indexes of the
             * decoder fifo and to use the fifo's conditional variable,
             * that's why we need to take the lock before */
            vlc_mutex_lock (&p_adec->fifo.data_lock);

            /* Is the input thread dying ? */
            if (p_adec->p_input->b_die) {
                vlc_mutex_unlock (&(p_adec->fifo.data_lock));
                return;
            }

            /* We should increase the start index of the decoder fifo, but
             * if we do this now, the input thread could overwrite the
             * pointer to the current PES packet, and we weren't able to
             * give it back to the netlist. That's why we free the PES
             * packet first. */
            input_NetlistFreePES (p_adec->p_input, DECODER_FIFO_START(p_adec->fifo));
            DECODER_FIFO_INCSTART (p_adec->fifo);

            while (DECODER_FIFO_ISEMPTY(p_adec->fifo)) {
                vlc_cond_wait (&p_adec->fifo.data_wait, &p_adec->fifo.data_lock);
                if (p_adec->p_input->b_die) {
                    vlc_mutex_unlock (&(p_adec->fifo.data_lock));
                    return;
                }
            }

            /* The next byte could be found in the next PES packet */
            p_adec->p_ts = DECODER_FIFO_START (p_adec->fifo)->p_first_ts;
	    if (DECODER_FIFO_START (p_adec->fifo)->b_data_alignment)
		p_adec->align = 1;

            /* We can release the fifo's data lock */
            vlc_mutex_unlock (&p_adec->fifo.data_lock);
        }
        /* Perhaps the next TS packet of the current PES packet contains
         * real data (ie its payload's size is greater than 0) */
        else {
            p_adec->p_ts = p_adec->p_ts->p_next_ts;
        }
    } while (p_adec->p_ts->i_payload_start == p_adec->p_ts->i_payload_end);

    /* We've found a TS packet which contains interesting data... */
    p_byte_stream->p_byte =
	p_adec->p_ts->buffer + p_adec->p_ts->i_payload_start;
    p_byte_stream->p_end =
	p_adec->p_ts->buffer + p_adec->p_ts->i_payload_end;
}
