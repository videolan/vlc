/*****************************************************************************
 * input.c : internal management of input streams for the audio output
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: input.c,v 1.20 2002/11/11 22:27:00 gbazin Exp $
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
 * aout_InputNew : allocate a new input and rework the filter pipeline
 *****************************************************************************/
int aout_InputNew( aout_instance_t * p_aout, aout_input_t * p_input )
{
    audio_sample_format_t intermediate_format;

    aout_FormatPrint( p_aout, "input", &p_input->input );

    /* Prepare FIFO. */
    aout_FifoInit( p_aout, &p_input->fifo, p_aout->mixer.mixer.i_rate );
    p_input->p_first_byte_to_mix = NULL;

    /* Create filters. */
    memcpy( &intermediate_format, &p_aout->mixer.mixer,
            sizeof(audio_sample_format_t) );
    intermediate_format.i_rate = p_input->input.i_rate;
    if ( aout_FiltersCreatePipeline( p_aout, p_input->pp_filters,
                                     &p_input->i_nb_filters, &p_input->input,
                                     &intermediate_format ) < 0 )
    {
        msg_Err( p_aout, "couldn't set an input pipeline" );

        aout_FifoDestroy( p_aout, &p_input->fifo );
        p_input->b_error = 1;

        return -1;
    }

    /* Prepare hints for the buffer allocator. */
    p_input->input_alloc.i_alloc_type = AOUT_ALLOC_HEAP;
    p_input->input_alloc.i_bytes_per_sec = -1;

    if ( AOUT_FMT_NON_LINEAR( &p_aout->mixer.mixer ) )
    {
        p_input->i_nb_resamplers = 0;
    }
    else
    {
        /* Create resamplers. */
        intermediate_format.i_rate = (p_input->input.i_rate
                                 * (100 + AOUT_MAX_RESAMPLING)) / 100;
        if ( intermediate_format.i_rate == p_aout->mixer.mixer.i_rate )
        {
            /* Just in case... */
            intermediate_format.i_rate++;
        }
        if ( aout_FiltersCreatePipeline( p_aout, p_input->pp_resamplers,
                                         &p_input->i_nb_resamplers,
                                         &intermediate_format,
                                         &p_aout->mixer.mixer ) < 0 )
        {
            msg_Err( p_aout, "couldn't set a resampler pipeline" );

            aout_FiltersDestroyPipeline( p_aout, p_input->pp_filters,
                                         p_input->i_nb_filters );
            aout_FifoDestroy( p_aout, &p_input->fifo );
            p_input->b_error = 1;

            return -1;
        }

        aout_FiltersHintBuffers( p_aout, p_input->pp_resamplers,
                                 p_input->i_nb_resamplers,
                                 &p_input->input_alloc );

        /* Setup the initial rate of the resampler */
        p_input->pp_resamplers[0]->input.i_rate = p_input->input.i_rate;
        p_input->i_resampling_type = AOUT_RESAMPLING_NONE;

    }

    p_input->input_alloc.i_alloc_type = AOUT_ALLOC_HEAP;
    p_input->input_alloc.i_bytes_per_sec = -1;
    aout_FiltersHintBuffers( p_aout, p_input->pp_filters,
                             p_input->i_nb_filters,
                             &p_input->input_alloc );

    /* i_bytes_per_sec is still == -1 if no filters */
    p_input->input_alloc.i_bytes_per_sec = __MAX(
                                    p_input->input_alloc.i_bytes_per_sec,
                                    p_input->input.i_bytes_per_frame
                                     * p_input->input.i_rate
                                     / p_input->input.i_frame_length );
    /* Allocate in the heap, it is more convenient for the decoder. */
    p_input->input_alloc.i_alloc_type = AOUT_ALLOC_HEAP;

    p_input->b_error = 0;

    return 0;
}

/*****************************************************************************
 * aout_InputDelete : delete an input
 *****************************************************************************
 * This function must be entered with the mixer lock.
 *****************************************************************************/
int aout_InputDelete( aout_instance_t * p_aout, aout_input_t * p_input )
{
    if ( p_input->b_error ) return 0;

    aout_FiltersDestroyPipeline( p_aout, p_input->pp_filters,
                                 p_input->i_nb_filters );
    aout_FiltersDestroyPipeline( p_aout, p_input->pp_resamplers,
                                 p_input->i_nb_resamplers );
    aout_FifoDestroy( p_aout, &p_input->fifo );

    return 0;
}

/*****************************************************************************
 * aout_InputPlay : play a buffer
 *****************************************************************************
 * This function must be entered with the input lock.
 *****************************************************************************/
int aout_InputPlay( aout_instance_t * p_aout, aout_input_t * p_input,
                    aout_buffer_t * p_buffer )
{
    mtime_t start_date;

    /* We don't care if someone changes the start date behind our back after
     * this. We'll deal with that when pushing the buffer, and compensate
     * with the next incoming buffer. */
    vlc_mutex_lock( &p_aout->input_fifos_lock );
    start_date = aout_FifoNextStart( p_aout, &p_input->fifo );
    vlc_mutex_unlock( &p_aout->input_fifos_lock );

    if ( start_date != 0 && start_date < mdate() )
    {
        /* The decoder is _very_ late. This can only happen if the user
         * pauses the stream (or if the decoder is buggy, which cannot
         * happen :). */
        msg_Warn( p_aout, "computed PTS is out of range ("I64Fd"), "
                  "clearing out", mdate() - start_date );
        vlc_mutex_lock( &p_aout->input_fifos_lock );
        aout_FifoSet( p_aout, &p_input->fifo, 0 );
        vlc_mutex_unlock( &p_aout->input_fifos_lock );
        if ( p_input->i_resampling_type != AOUT_RESAMPLING_NONE )
            msg_Warn( p_aout, "timing screwed, stopping resampling" );
        p_input->i_resampling_type = AOUT_RESAMPLING_NONE;
        p_input->pp_resamplers[0]->input.i_rate = p_input->input.i_rate;
        p_input->pp_resamplers[0]->b_reinit = VLC_TRUE;
        start_date = 0;
    }

    if ( p_buffer->start_date < mdate() + AOUT_MIN_PREPARE_TIME )
    {
        /* The decoder gives us f*cked up PTS. It's its business, but we
         * can't present it anyway, so drop the buffer. */
        msg_Warn( p_aout, "PTS is out of range ("I64Fd"), dropping buffer",
                  mdate() - p_buffer->start_date );
        aout_BufferFree( p_buffer );

        return 0;
    }

    if ( start_date == 0 ) start_date = p_buffer->start_date;

    /* Run pre-filters. */
    aout_FiltersPlay( p_aout, p_input->pp_filters, p_input->i_nb_filters,
                      &p_buffer );

    /* Run the resampler if needed.
     * We first need to calculate the output rate of this resampler. */
    if ( ( p_input->i_resampling_type == AOUT_RESAMPLING_NONE ) &&
         ( start_date < p_buffer->start_date - AOUT_PTS_TOLERANCE
           || start_date > p_buffer->start_date + AOUT_PTS_TOLERANCE ) )
    {
        /* Can happen in several circumstances :
         * 1. A problem at the input (clock drift)
         * 2. A small pause triggered by the user
         * 3. Some delay in the output stage, causing a loss of lip
         *    synchronization
         * Solution : resample the buffer to avoid a scratch.
         */
        mtime_t drift = p_buffer->start_date - start_date;

        p_input->i_resamp_start_date = mdate();
        p_input->i_resamp_start_drift = drift;

        if ( drift > 0 )
            p_input->i_resampling_type = AOUT_RESAMPLING_DOWN;
        else
            p_input->i_resampling_type = AOUT_RESAMPLING_UP;

        msg_Warn( p_aout, "buffer is "I64Fd" %s, triggering %ssampling",
                          drift > 0 ? drift : -drift,
                          drift > 0 ? "in advance" : "late",
                          drift > 0 ? "down" : "up");
    }

    if ( p_input->i_resampling_type != AOUT_RESAMPLING_NONE )
    {
        /* Resampling has been triggered previously (because of dates
         * mismatch). We want the resampling to happen progressively so
         * it isn't too audible to the listener. */

        if( p_input->i_resampling_type == AOUT_RESAMPLING_UP )
        {
            p_input->pp_resamplers[0]->input.i_rate += 10; /* Hz */
        }
        else
        {
            p_input->pp_resamplers[0]->input.i_rate -= 10; /* Hz */
        }

        /* Check if everything is back to normal, in which case we can stop the
         * resampling */
        if( p_input->pp_resamplers[0]->input.i_rate ==
              p_input->input.i_rate )
        {
            p_input->i_resampling_type = AOUT_RESAMPLING_NONE;
            msg_Warn( p_aout, "resampling stopped after "I64Fi" usec",
                      mdate() - p_input->i_resamp_start_date );
        }
        else if( abs( p_buffer->start_date - start_date ) <
                 abs( p_input->i_resamp_start_drift ) / 2 )
        {
            /* if we reduced the drift from half, then it is time to switch
             * back the resampling direction. */
            if( p_input->i_resampling_type == AOUT_RESAMPLING_UP )
                p_input->i_resampling_type = AOUT_RESAMPLING_DOWN;
            else
                p_input->i_resampling_type = AOUT_RESAMPLING_UP;
            p_input->i_resamp_start_drift = 0;
        }
        else if( p_input->i_resamp_start_drift &&
                 ( abs( p_buffer->start_date - start_date ) >
                   abs( p_input->i_resamp_start_drift ) * 3 / 2 ) )
        {
            /* If the drift is increasing and not decreasing, than something
             * is bad. We'd better stop the resampling right now. */
            msg_Warn( p_aout, "timing screwed, stopping resampling" );
            p_input->i_resampling_type = AOUT_RESAMPLING_NONE;
            p_input->pp_resamplers[0]->input.i_rate = p_input->input.i_rate;
        }

    }

    /* Adding the start date will be managed by aout_FifoPush(). */
    p_buffer->start_date = start_date;
    p_buffer->end_date = start_date +
        (p_buffer->end_date - p_buffer->start_date);

    /* Actually run the resampler now. */
    if ( p_aout->mixer.mixer.i_rate !=
         p_input->pp_resamplers[0]->input.i_rate )
    {
        aout_FiltersPlay( p_aout, p_input->pp_resamplers,
                          p_input->i_nb_resamplers,
                          &p_buffer );
    }

    vlc_mutex_lock( &p_aout->input_fifos_lock );
    aout_FifoPush( p_aout, &p_input->fifo, p_buffer );
    vlc_mutex_unlock( &p_aout->input_fifos_lock );

    return 0;
}
