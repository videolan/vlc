/*****************************************************************************
 * mixer.c : audio output mixing operations
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: mixer.c,v 1.6 2002/08/19 21:31:11 massiot Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
#include <stdlib.h>                            /* calloc(), malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>

#ifdef HAVE_ALLOCA_H
#   include <alloca.h> 
#endif

#include "audio_output.h"
#include "aout_internal.h"

/*****************************************************************************
 * aout_MixerNew: prepare a mixer plug-in
 *****************************************************************************/
int aout_MixerNew( aout_instance_t * p_aout )
{
    p_aout->mixer.p_module = module_Need( p_aout, "audio mixer", NULL );
    if ( p_aout->mixer.p_module == NULL )
    {
        msg_Err( p_aout, "no suitable aout mixer" );
        return -1;
    }
    return 0;
}

/*****************************************************************************
 * aout_MixerDelete: delete the mixer
 *****************************************************************************/
void aout_MixerDelete( aout_instance_t * p_aout )
{
    module_Unneed( p_aout, p_aout->mixer.p_module );
}

/*****************************************************************************
 * aout_MixerRun: entry point for the mixer & post-filters processing
 *****************************************************************************/
void aout_MixerRun( aout_instance_t * p_aout )
{
    int             i;
    aout_buffer_t * p_output_buffer;
    mtime_t start_date, end_date;

    /* Retrieve the date of the next buffer. We don't use aout_FifoNextStart
     * because we need to keep the lock on the FIFO, to prevent the aout
     * thread from triggering resampling while we are running. */
    vlc_mutex_lock( &p_aout->output.fifo.lock );
    start_date = p_aout->output.fifo.end_date;
    if ( start_date != 0 && start_date < mdate() )
    {
        /* The output is _very_ late. This can only happen if the user
         * pauses the stream (or if the decoder is buggy, which cannot
         * happen :). */
        msg_Warn( p_aout, "Output PTS is out of range (%lld), clearing out",
                  start_date );
        start_date = p_aout->output.fifo.end_date = 0;
    } 
    end_date = start_date + (mtime_t)p_aout->output.i_nb_samples * 1000000
                             / p_aout->output.output.i_rate;

    /* See if we have enough data to prepare a new buffer for the audio
     * output. */
    for ( i = 0; i < p_aout->i_nb_inputs; i++ )
    {
        aout_input_t * p_input = p_aout->pp_inputs[i];
        aout_fifo_t * p_fifo = &p_input->fifo;
        aout_buffer_t * p_buffer;
        mtime_t prev_date;
        boolean_t b_drop_buffers;

        vlc_mutex_lock( &p_fifo->lock );

        p_buffer = p_fifo->p_first;
        if ( p_buffer == NULL )
        {
            vlc_mutex_unlock( &p_fifo->lock );
            break;
        }

        if ( !start_date )
        {
            start_date = p_buffer->start_date;
            end_date += p_buffer->start_date;
            p_input->p_first_byte_to_mix = p_buffer->p_buffer;
        }
        else
        {
            /* Check for the continuity of start_date */
            while ( p_buffer != NULL && p_buffer->end_date < start_date )
            {
                aout_buffer_t * p_next = p_buffer->p_next;
                msg_Err( p_aout, "the mixer got a packet in the past (%lld)",
                         start_date - p_buffer->end_date );
                aout_BufferFree( p_buffer );
                p_fifo->p_first = p_buffer = p_next;
                p_input->p_first_byte_to_mix = NULL;
            }
            if ( p_buffer == NULL )
            {
                p_fifo->pp_last = &p_fifo->p_first;
                vlc_mutex_unlock( &p_fifo->lock );
                break;
            }

            if ( !AOUT_FMT_NON_LINEAR( &p_aout->mixer.mixer ) )
            {
                /* Additionally check that p_first_byte_to_mix is well
                 * located. */
                unsigned long i_nb_bytes = (start_date - p_buffer->start_date)
                                * p_aout->mixer.mixer.i_bytes_per_frame
                                * p_aout->mixer.mixer.i_rate
                                / p_aout->mixer.mixer.i_frame_length
                                / 1000000;
                ptrdiff_t mixer_nb_bytes;

                if ( p_input->p_first_byte_to_mix == NULL )
                {
                    p_input->p_first_byte_to_mix = p_buffer->p_buffer;
                }
                mixer_nb_bytes = p_input->p_first_byte_to_mix
                                  - p_buffer->p_buffer;

                if ( i_nb_bytes + p_aout->mixer.mixer.i_bytes_per_frame
                      < mixer_nb_bytes ||
                     i_nb_bytes - p_aout->mixer.mixer.i_bytes_per_frame
                      > mixer_nb_bytes )
                {
                    msg_Warn( p_aout,
                              "mixer start isn't output start (%ld)",
                              i_nb_bytes - mixer_nb_bytes );

                    /* Round to the nearest multiple */
                    i_nb_bytes /= p_aout->mixer.mixer.i_bytes_per_frame;
                    i_nb_bytes *= p_aout->mixer.mixer.i_bytes_per_frame;
                    p_input->p_first_byte_to_mix = p_buffer->p_buffer
                                                    + i_nb_bytes;
                }
            }
        }

        /* Check that we have enough samples. */
        for ( ; ; )
        {
            p_buffer = p_fifo->p_first;
            if ( p_buffer == NULL ) break;
            if ( p_buffer->end_date >= end_date ) break;

            /* Check that all buffers are contiguous. */
            prev_date = p_fifo->p_first->end_date;
            p_buffer = p_buffer->p_next;
            b_drop_buffers = 0;
            for ( ; p_buffer != NULL; p_buffer = p_buffer->p_next )
            {
                if ( prev_date != p_buffer->start_date )
                {
                    msg_Warn( p_aout,
                              "buffer discontinuity, dropping packets (%lld)",
                              p_buffer->start_date - prev_date );
                    b_drop_buffers = 1;
                    break;
                }
                if ( p_buffer->end_date >= end_date ) break;
                prev_date = p_buffer->end_date;
            }
            if ( b_drop_buffers )
            {
                aout_buffer_t * p_deleted = p_fifo->p_first;
                while ( p_deleted != NULL && p_deleted != p_buffer )
                {
                    aout_buffer_t * p_next = p_deleted->p_next;
                    aout_BufferFree( p_deleted );
                    p_deleted = p_next;
                }
                p_fifo->p_first = p_deleted; /* == p_buffer */
            }
            else break;
        }
        vlc_mutex_unlock( &p_fifo->lock );
        if ( p_buffer == NULL ) break;
    }

    if ( i < p_aout->i_nb_inputs )
    {
        /* Interrupted before the end... We can't run. */
        vlc_mutex_unlock( &p_aout->output.fifo.lock );
        return;
    }

    p_aout->b_mixer_active = 1;
    vlc_mutex_unlock( &p_aout->output.fifo.lock );

    /* Run the mixer. */
    aout_BufferAlloc( &p_aout->mixer.output_alloc,
                      ((u64)p_aout->output.i_nb_samples * 1000000)
                        / p_aout->output.output.i_rate,
                      /* This is a bit kludgy, but is actually only used
                       * for the S/PDIF dummy mixer : */
                      p_aout->pp_inputs[0]->fifo.p_first,
                      p_output_buffer );
    if ( p_output_buffer == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return;
    }
    p_output_buffer->i_nb_samples = p_aout->output.i_nb_samples;
    p_output_buffer->i_nb_bytes = p_aout->output.i_nb_samples
                              * p_aout->output.output.i_bytes_per_frame
                              / p_aout->output.output.i_frame_length;
    p_output_buffer->start_date = start_date;
    p_output_buffer->end_date = end_date;

    p_aout->mixer.pf_do_work( p_aout, p_output_buffer );

    aout_OutputPlay( p_aout, p_output_buffer );

    vlc_mutex_lock( &p_aout->output.fifo.lock );
    p_aout->b_mixer_active = 0;
    vlc_cond_signal( &p_aout->mixer_signal );
    vlc_mutex_unlock( &p_aout->output.fifo.lock );
}

