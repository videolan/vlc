/*****************************************************************************
 * input.c : internal management of input streams for the audio output
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: input.c,v 1.7 2002/08/21 22:41:59 massiot Exp $
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

#include "audio_output.h"
#include "aout_internal.h"

/*****************************************************************************
 * aout_InputNew : allocate a new input and rework the filter pipeline
 *****************************************************************************/
static aout_input_t * InputNew( aout_instance_t * p_aout,
                                audio_sample_format_t * p_format )
{
    aout_input_t * p_input = malloc(sizeof(aout_input_t));

    if ( p_input == NULL ) return NULL;

    vlc_mutex_lock( &p_aout->mixer_lock );

    if ( p_aout->i_nb_inputs == 0 )
    {
        /* Recreate the output using the new format. */
        if ( aout_OutputNew( p_aout, p_format ) < 0 )
        {
            free( p_input );
            return NULL;
        }
    }
    else
    {
        aout_MixerDelete( p_aout );
    }

    memcpy( &p_input->input, p_format,
            sizeof(audio_sample_format_t) );
    aout_FormatPrepare( &p_input->input );

    /* Prepare FIFO. */
    aout_FifoInit( p_aout, &p_input->fifo, p_aout->mixer.mixer.i_rate );
    p_input->p_first_byte_to_mix = NULL;

    /* Create filters. */
    if ( aout_FiltersCreatePipeline( p_aout, p_input->pp_filters,
                                     &p_input->i_nb_filters, &p_input->input,
                                     &p_aout->mixer.mixer ) < 0 )
    {
        msg_Err( p_aout, "couldn't set an input pipeline" );

        aout_FifoDestroy( p_aout, &p_input->fifo );

        free( p_input );

        if ( !p_aout->i_nb_inputs )
        {
            aout_OutputDelete( p_aout );
        }
        return NULL;
    }

    p_aout->pp_inputs[p_aout->i_nb_inputs] = p_input;
    p_aout->i_nb_inputs++;

    if ( aout_MixerNew( p_aout ) < 0 )
    {
        p_aout->i_nb_inputs--;
        aout_FiltersDestroyPipeline( p_aout, p_input->pp_filters,
                                     p_input->i_nb_filters );
        aout_FifoDestroy( p_aout, &p_input->fifo );

        free( p_input );

        if ( !p_aout->i_nb_inputs )
        {
            aout_OutputDelete( p_aout );
        }
        else
        {
            aout_MixerNew( p_aout );
        }
        vlc_mutex_unlock( &p_aout->mixer_lock );

        return NULL;
    }

    vlc_mutex_unlock( &p_aout->mixer_lock );

    /* Prepare hints for the buffer allocator. */
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

    msg_Dbg( p_aout, "input 0x%x created", p_input );
    return p_input;
}

aout_input_t * __aout_InputNew( vlc_object_t * p_this,
                                aout_instance_t ** pp_aout,
                                audio_sample_format_t * p_format )
{
    /* Create an audio output if there is none. */
    *pp_aout = vlc_object_find( p_this, VLC_OBJECT_AOUT, FIND_ANYWHERE );

    if( *pp_aout == NULL )
    {
        msg_Dbg( p_this, "no aout present, spawning one" );

        *pp_aout = aout_NewInstance( p_this );
        /* Everything failed, I'm a loser, I just wanna die */
        if( *pp_aout == NULL )
        {
            return NULL;
        }
    }
    else
    {
        vlc_object_release( *pp_aout );
    }

    return InputNew( *pp_aout, p_format );
}

/*****************************************************************************
 * aout_InputDelete : delete an input
 *****************************************************************************/
void aout_InputDelete( aout_instance_t * p_aout, aout_input_t * p_input )
{
    int i_input;

    vlc_mutex_lock( &p_aout->mixer_lock );

    for ( i_input = 0; i_input < p_aout->i_nb_inputs; i_input++ )
    {
        if ( p_aout->pp_inputs[i_input] == p_input )
        {
            break;
        }
    }

    if ( i_input == p_aout->i_nb_inputs )
    {
        msg_Err( p_aout, "cannot find an input to delete" );
        return;
    }

    /* Remove the input from the list. */
    memmove( &p_aout->pp_inputs[i_input], &p_aout->pp_inputs[i_input + 1],
             (AOUT_MAX_INPUTS - i_input - 1) * sizeof(aout_input_t *) );
    p_aout->i_nb_inputs--;

    vlc_mutex_unlock( &p_aout->mixer_lock );

    aout_FiltersDestroyPipeline( p_aout, p_input->pp_filters,
                                 p_input->i_nb_filters );
    aout_FifoDestroy( p_aout, &p_input->fifo );

    free( p_input );

    if ( !p_aout->i_nb_inputs )
    {
        aout_OutputDelete( p_aout );
        aout_MixerDelete( p_aout );
    }

    msg_Dbg( p_aout, "input 0x%x destroyed", p_input );
}

/*****************************************************************************
 * aout_InputPlay : play a buffer
 *****************************************************************************/
void aout_InputPlay( aout_instance_t * p_aout, aout_input_t * p_input,
                     aout_buffer_t * p_buffer )
{
    mtime_t start_date, duration;

    vlc_mutex_lock( &p_aout->input_lock );
    while( p_aout->b_change_requested )
    {
        vlc_cond_wait( &p_aout->input_signal, &p_aout->input_lock );
    }
    p_aout->i_inputs_active++;
    vlc_mutex_unlock( &p_aout->input_lock );

    /* We don't care if someone changes the start date behind our back after
     * this. We'll deal with that when pushing the buffer, and compensate
     * with the next incoming buffer. */
    start_date = aout_FifoNextStart( p_aout, &p_input->fifo );

    if ( start_date != 0 && start_date < mdate() )
    {
        /* The decoder is _very_ late. This can only happen if the user
         * pauses the stream (or if the decoder is buggy, which cannot
         * happen :). */
        msg_Warn( p_aout, "Computed PTS is out of range (%lld), clearing out",
                  start_date );
        vlc_mutex_lock( &p_aout->mixer_lock );
        aout_FifoSet( p_aout, &p_input->fifo, 0 );
        vlc_mutex_unlock( &p_aout->mixer_lock );
        start_date = 0;
    } 

    if ( p_buffer->start_date < mdate() - AOUT_MIN_PREPARE_TIME )
    {
        /* The decoder gives us f*cked up PTS. It's its business, but we
         * can't present it anyway, so drop the buffer. */
        msg_Warn( p_aout, "PTS is out of range (%lld), dropping buffer",
                  mdate() - p_buffer->start_date );
        aout_BufferFree( p_buffer );

        vlc_mutex_lock( &p_aout->input_lock );
        p_aout->i_inputs_active--;
        vlc_cond_broadcast( &p_aout->input_signal );
        vlc_mutex_unlock( &p_aout->input_lock );
        return;
    }

    if ( start_date == 0 ) start_date = p_buffer->start_date;

    if ( start_date < p_buffer->start_date - AOUT_PTS_TOLERANCE
          || start_date > p_buffer->start_date + AOUT_PTS_TOLERANCE )
    {
        /* Can happen in several circumstances :
         * 1. A problem at the input (clock drift)
         * 2. A small pause triggered by the user
         * 3. Some delay in the output stage, causing a loss of lip
         *    synchronization
         * Solution : resample the buffer to avoid a scratch.
         */
        audio_sample_format_t new_output;
        int i_ratio, i_nb_filters;
        mtime_t old_duration;
        aout_filter_t * pp_filters[AOUT_MAX_FILTERS];
        aout_buffer_t * p_new_buffer;
        aout_alloc_t dummy_alloc;
        mtime_t drift = p_buffer->start_date - start_date;

        msg_Warn( p_aout, "buffer is %lld %s, resampling",
                         drift > 0 ? drift : -drift,
                         drift > 0 ? "in advance" : "late" );
        memcpy( &new_output, &p_aout->mixer.mixer,
                sizeof(audio_sample_format_t) );
        old_duration = p_buffer->end_date - p_buffer->start_date;
        duration = p_buffer->end_date - start_date;
        i_ratio = duration * 100 / old_duration;
        /* If the ratio is too != 100, the sound quality will be awful. */
        if ( i_ratio < 66 /* % */ )
        {
            duration = old_duration * 66 / 100;
        }
        if ( i_ratio > 150 /* % */ )
        {
            duration = old_duration * 150 / 100;
        }
        new_output.i_rate = new_output.i_rate * duration / old_duration;

        if ( aout_FiltersCreatePipeline( p_aout, pp_filters,
                                         &i_nb_filters, &p_input->input,
                                         &new_output ) < 0 )
        {
            msg_Err( p_aout, "couldn't set an input pipeline for resampling" );
            vlc_mutex_lock( &p_aout->mixer_lock );
            aout_FifoSet( p_aout, &p_input->fifo, 0 );
            vlc_mutex_unlock( &p_aout->mixer_lock );
            aout_BufferFree( p_buffer );

            vlc_mutex_lock( &p_aout->input_lock );
            p_aout->i_inputs_active--;
            vlc_cond_broadcast( &p_aout->input_signal );
            vlc_mutex_unlock( &p_aout->input_lock );
            return;
        }

        dummy_alloc.i_alloc_type = AOUT_ALLOC_HEAP;
        dummy_alloc.i_bytes_per_sec = -1;
        aout_FiltersHintBuffers( p_aout, pp_filters, i_nb_filters,
                                 &dummy_alloc );
        dummy_alloc.i_bytes_per_sec = __MAX(
                                    dummy_alloc.i_bytes_per_sec,
                                    p_input->input.i_bytes_per_frame
                                     * p_input->input.i_rate
                                     / p_input->input.i_frame_length );
        dummy_alloc.i_alloc_type = AOUT_ALLOC_HEAP;

        aout_BufferAlloc( &dummy_alloc, old_duration, NULL, p_new_buffer );
        memcpy( p_new_buffer->p_buffer, p_buffer->p_buffer,
                p_buffer->i_nb_bytes );
        p_new_buffer->i_nb_samples = p_buffer->i_nb_samples;
        p_new_buffer->i_nb_bytes = p_buffer->i_nb_bytes;
        aout_BufferFree( p_buffer );
        p_buffer = p_new_buffer;

        aout_FiltersPlay( p_aout, pp_filters, i_nb_filters,
                          &p_buffer );

        aout_FiltersDestroyPipeline( p_aout, pp_filters,
                                     i_nb_filters );
    }
    else
    {
        /* No resampling needed (except maybe the one imposed by the
         * output). */
        duration = p_buffer->end_date - p_buffer->start_date;
        aout_FiltersPlay( p_aout, p_input->pp_filters, p_input->i_nb_filters,
                          &p_buffer );
    }

    vlc_mutex_lock( &p_aout->mixer_lock );
    /* Adding the start date will be managed by aout_FifoPush(). */
    p_buffer->start_date = start_date;
    p_buffer->end_date = start_date + duration;
    aout_FifoPush( p_aout, &p_input->fifo, p_buffer );
    vlc_mutex_unlock( &p_aout->mixer_lock );

    vlc_mutex_lock( &p_aout->input_lock );
    p_aout->i_inputs_active--;
    vlc_cond_broadcast( &p_aout->input_signal );
    vlc_mutex_unlock( &p_aout->input_lock );
}
