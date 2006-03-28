/*****************************************************************************
 * mixer.c : audio output mixing operations
 *****************************************************************************
 * Copyright (C) 2002-2004 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
 *****************************************************************************
 * Please note that you must hold the mixer lock.
 *****************************************************************************/
int aout_MixerNew( aout_instance_t * p_aout )
{
    p_aout->mixer.p_module = module_Need( p_aout, "audio mixer", NULL, 0 );
    if ( p_aout->mixer.p_module == NULL )
    {
        msg_Err( p_aout, "no suitable audio mixer" );
        return -1;
    }
    p_aout->mixer.b_error = 0;
    return 0;
}

/*****************************************************************************
 * aout_MixerDelete: delete the mixer
 *****************************************************************************
 * Please note that you must hold the mixer lock.
 *****************************************************************************/
void aout_MixerDelete( aout_instance_t * p_aout )
{
    if ( p_aout->mixer.b_error ) return;
    module_Unneed( p_aout, p_aout->mixer.p_module );
    p_aout->mixer.b_error = 1;
}

/*****************************************************************************
 * MixBuffer: try to prepare one output buffer
 *****************************************************************************
 * Please note that you must hold the mixer lock.
 *****************************************************************************/
static int MixBuffer( aout_instance_t * p_aout )
{
    int             i, i_first_input = 0;
    aout_buffer_t * p_output_buffer;
    mtime_t start_date, end_date;
    audio_date_t exact_start_date;

    if ( p_aout->mixer.b_error )
    {
        /* Free all incoming buffers. */
        vlc_mutex_lock( &p_aout->input_fifos_lock );
        for ( i = 0; i < p_aout->i_nb_inputs; i++ )
        {
            aout_input_t * p_input = p_aout->pp_inputs[i];
            aout_buffer_t * p_buffer = p_input->fifo.p_first;
            if ( p_input->b_error ) continue;
            while ( p_buffer != NULL )
            {
                aout_buffer_t * p_next = p_buffer->p_next;
                aout_BufferFree( p_buffer );
                p_buffer = p_next;
            }
        }
        vlc_mutex_unlock( &p_aout->input_fifos_lock );
        return -1;
    }


    vlc_mutex_lock( &p_aout->output_fifo_lock );
    vlc_mutex_lock( &p_aout->input_fifos_lock );

    /* Retrieve the date of the next buffer. */
    memcpy( &exact_start_date, &p_aout->output.fifo.end_date,
            sizeof(audio_date_t) );
    start_date = aout_DateGet( &exact_start_date );

    if ( start_date != 0 && start_date < mdate() )
    {
        /* The output is _very_ late. This can only happen if the user
         * pauses the stream (or if the decoder is buggy, which cannot
         * happen :). */
        msg_Warn( p_aout, "output PTS is out of range ("I64Fd"), clearing out",
                  mdate() - start_date );
        aout_FifoSet( p_aout, &p_aout->output.fifo, 0 );
        aout_DateSet( &exact_start_date, 0 );
        start_date = 0;
    }

    vlc_mutex_unlock( &p_aout->output_fifo_lock );

    /* See if we have enough data to prepare a new buffer for the audio
     * output. First : start date. */
    if ( !start_date )
    {
        /* Find the latest start date available. */
        for ( i = 0; i < p_aout->i_nb_inputs; i++ )
        {
            aout_input_t * p_input = p_aout->pp_inputs[i];
            aout_fifo_t * p_fifo = &p_input->fifo;
            aout_buffer_t * p_buffer;

            if ( p_input->b_error ) continue;

            p_buffer = p_fifo->p_first;
            while ( p_buffer != NULL && p_buffer->start_date < mdate() )
            {
                msg_Warn( p_aout, "input PTS is out of range ("I64Fd"), "
                          "trashing", mdate() - p_buffer->start_date );
                p_buffer = aout_FifoPop( p_aout, p_fifo );
                aout_BufferFree( p_buffer );
                if( p_input->p_input_thread )
                {
//                    stats_UpdateInteger( p_input->p_input_thread,
//                                         "lost_abuffers", 1 );
                }
                p_buffer = p_fifo->p_first;
                p_input->p_first_byte_to_mix = NULL;
            }

            if ( p_buffer == NULL )
            {
                break;
            }

            if ( !start_date || start_date < p_buffer->start_date )
            {
                aout_DateSet( &exact_start_date, p_buffer->start_date );
                start_date = p_buffer->start_date;
            }
        }

        if ( i < p_aout->i_nb_inputs )
        {
            /* Interrupted before the end... We can't run. */
            vlc_mutex_unlock( &p_aout->input_fifos_lock );
            return -1;
        }
    }
    aout_DateIncrement( &exact_start_date, p_aout->output.i_nb_samples );
    end_date = aout_DateGet( &exact_start_date );

    /* Check that start_date and end_date are available for all input
     * streams. */
    for ( i = 0; i < p_aout->i_nb_inputs; i++ )
    {
        aout_input_t * p_input = p_aout->pp_inputs[i];
        aout_fifo_t * p_fifo = &p_input->fifo;
        aout_buffer_t * p_buffer;
        mtime_t prev_date;
        vlc_bool_t b_drop_buffers;

        if ( p_input->b_error )
        {
            if ( i_first_input == i ) i_first_input++;
            continue;
        }

        p_buffer = p_fifo->p_first;
        if ( p_buffer == NULL )
        {
            break;
        }

        /* Check for the continuity of start_date */
        while ( p_buffer != NULL && p_buffer->end_date < start_date - 1 )
        {
            /* We authorize a +-1 because rounding errors get compensated
             * regularly. */
            aout_buffer_t * p_next = p_buffer->p_next;
            msg_Warn( p_aout, "the mixer got a packet in the past ("I64Fd")",
                      start_date - p_buffer->end_date );
            aout_BufferFree( p_buffer );
            if( p_input->p_input_thread )
            {
//                stats_UpdateInteger( p_input->p_input_thread,
//                                     "lost_abuffers", 1 );
            }
            p_fifo->p_first = p_buffer = p_next;
            p_input->p_first_byte_to_mix = NULL;
        }
        if ( p_buffer == NULL )
        {
            p_fifo->pp_last = &p_fifo->p_first;
            break;
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
                              "buffer hole, dropping packets ("I64Fd")",
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
        if ( p_buffer == NULL ) break;

        p_buffer = p_fifo->p_first;
        if ( !AOUT_FMT_NON_LINEAR( &p_aout->mixer.mixer ) )
        {
            /* Additionally check that p_first_byte_to_mix is well
             * located. */
            mtime_t i_nb_bytes = (start_date - p_buffer->start_date)
                            * p_aout->mixer.mixer.i_bytes_per_frame
                            * p_aout->mixer.mixer.i_rate
                            / p_aout->mixer.mixer.i_frame_length
                            / 1000000;
            ptrdiff_t mixer_nb_bytes;

            if ( p_input->p_first_byte_to_mix == NULL )
            {
                p_input->p_first_byte_to_mix = p_buffer->p_buffer;
            }
            mixer_nb_bytes = p_input->p_first_byte_to_mix - p_buffer->p_buffer;

            if ( !((i_nb_bytes + p_aout->mixer.mixer.i_bytes_per_frame
                     > mixer_nb_bytes) &&
                   (i_nb_bytes < p_aout->mixer.mixer.i_bytes_per_frame
                     + mixer_nb_bytes)) )
            {
                msg_Warn( p_aout, "mixer start isn't output start ("I64Fd")",
                          i_nb_bytes - mixer_nb_bytes );

                /* Round to the nearest multiple */
                i_nb_bytes /= p_aout->mixer.mixer.i_bytes_per_frame;
                i_nb_bytes *= p_aout->mixer.mixer.i_bytes_per_frame;
                if( i_nb_bytes < 0 )
                {
                    /* Is it really the best way to do it ? */
                    aout_FifoSet( p_aout, &p_aout->output.fifo, 0 );
                    aout_DateSet( &exact_start_date, 0 );
                    break;
                }

                p_input->p_first_byte_to_mix = p_buffer->p_buffer + i_nb_bytes;
            }
        }
    }

    if ( i < p_aout->i_nb_inputs || i_first_input == p_aout->i_nb_inputs )
    {
        /* Interrupted before the end... We can't run. */
        vlc_mutex_unlock( &p_aout->input_fifos_lock );
        return -1;
    }

    /* Run the mixer. */
    aout_BufferAlloc( &p_aout->mixer.output_alloc,
                      ((uint64_t)p_aout->output.i_nb_samples * 1000000)
                        / p_aout->output.output.i_rate,
                      /* This is a bit kludgy, but is actually only used
                       * for the S/PDIF dummy mixer : */
                      p_aout->pp_inputs[i_first_input]->fifo.p_first,
                      p_output_buffer );
    if ( p_output_buffer == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        vlc_mutex_unlock( &p_aout->input_fifos_lock );
        return -1;
    }
    /* This is again a bit kludgy - for the S/PDIF mixer. */
    if ( p_aout->mixer.output_alloc.i_alloc_type != AOUT_ALLOC_NONE )
    {
        p_output_buffer->i_nb_samples = p_aout->output.i_nb_samples;
        p_output_buffer->i_nb_bytes = p_aout->output.i_nb_samples
                              * p_aout->mixer.mixer.i_bytes_per_frame
                              / p_aout->mixer.mixer.i_frame_length;
    }
    p_output_buffer->start_date = start_date;
    p_output_buffer->end_date = end_date;

    p_aout->mixer.pf_do_work( p_aout, p_output_buffer );

    vlc_mutex_unlock( &p_aout->input_fifos_lock );

    aout_OutputPlay( p_aout, p_output_buffer );

    return 0;
}

/*****************************************************************************
 * aout_MixerRun: entry point for the mixer & post-filters processing
 *****************************************************************************
 * Please note that you must hold the mixer lock.
 *****************************************************************************/
void aout_MixerRun( aout_instance_t * p_aout )
{
    while( MixBuffer( p_aout ) != -1 );
}

/*****************************************************************************
 * aout_MixerMultiplierSet: set p_aout->mixer.f_multiplier
 *****************************************************************************
 * Please note that we assume that you own the mixer lock when entering this
 * function. This function returns -1 on error.
 *****************************************************************************/
int aout_MixerMultiplierSet( aout_instance_t * p_aout, float f_multiplier )
{
    float f_old = p_aout->mixer.f_multiplier;
    vlc_bool_t b_new_mixer = 0;

    if ( !p_aout->mixer.b_error )
    {
        aout_MixerDelete( p_aout );
        b_new_mixer = 1;
    }

    p_aout->mixer.f_multiplier = f_multiplier;

    if ( b_new_mixer && aout_MixerNew( p_aout ) )
    {
        p_aout->mixer.f_multiplier = f_old;
        aout_MixerNew( p_aout );
        return -1;
    }

    return 0;
}

/*****************************************************************************
 * aout_MixerMultiplierGet: get p_aout->mixer.f_multiplier
 *****************************************************************************
 * Please note that we assume that you own the mixer lock when entering this
 * function. This function returns -1 on error.
 *****************************************************************************/
int aout_MixerMultiplierGet( aout_instance_t * p_aout, float * pf_multiplier )
{
    *pf_multiplier = p_aout->mixer.f_multiplier;
    return 0;
}

