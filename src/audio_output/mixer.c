/*****************************************************************************
 * mixer.c : audio output mixing operations
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: mixer.c,v 1.12 2002/09/16 20:46:38 massiot Exp $
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
 *****************************************************************************
 * Please note that you must hold the mixer lock.
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
 *****************************************************************************
 * Please note that you must hold the mixer lock.
 *****************************************************************************/
void aout_MixerDelete( aout_instance_t * p_aout )
{
    module_Unneed( p_aout, p_aout->mixer.p_module );
}

/*****************************************************************************
 * MixBuffer: try to prepare one output buffer
 *****************************************************************************/
static int MixBuffer( aout_instance_t * p_aout )
{
    int             i;
    aout_buffer_t * p_output_buffer;
    mtime_t start_date, end_date;
    audio_date_t exact_start_date;

    vlc_mutex_lock( &p_aout->mixer_lock );
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
        msg_Warn( p_aout, "Output PTS is out of range (%lld), clearing out",
                  start_date );
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

            p_buffer = p_fifo->p_first;
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
            vlc_mutex_unlock( &p_aout->mixer_lock );
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

        p_buffer = p_fifo->p_first;
        if ( p_buffer == NULL )
        {
            break;
        }

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

            if ( !((i_nb_bytes + p_aout->mixer.mixer.i_bytes_per_frame
                     > mixer_nb_bytes) &&
                   (i_nb_bytes < p_aout->mixer.mixer.i_bytes_per_frame
                     + mixer_nb_bytes)) )
            {
                msg_Warn( p_aout,
                          "mixer start isn't output start (%d)",
                          i_nb_bytes - mixer_nb_bytes );

                /* Round to the nearest multiple */
                i_nb_bytes /= p_aout->mixer.mixer.i_bytes_per_frame;
                i_nb_bytes *= p_aout->mixer.mixer.i_bytes_per_frame;
                p_input->p_first_byte_to_mix = p_buffer->p_buffer
                                                + i_nb_bytes;
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
                              "buffer hole, dropping packets (%lld)",
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
    }

    if ( i < p_aout->i_nb_inputs )
    {
        /* Interrupted before the end... We can't run. */
        vlc_mutex_unlock( &p_aout->input_fifos_lock );
        vlc_mutex_unlock( &p_aout->mixer_lock );
        return -1;
    }

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
        vlc_mutex_unlock( &p_aout->input_fifos_lock );
        vlc_mutex_unlock( &p_aout->mixer_lock );
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

    vlc_mutex_unlock( &p_aout->mixer_lock );

    return 0;
}

/*****************************************************************************
 * aout_MixerRun: entry point for the mixer & post-filters processing
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

    if ( p_aout->mixer.mixer.i_format != AOUT_FMT_FLOAT32 )
    {
        msg_Warn( p_aout, "MixerMultiplierSet called for non-float32 mixer" );
        return -1;
    }

    aout_MixerDelete( p_aout );

    p_aout->mixer.f_multiplier = f_multiplier;

    if ( aout_MixerNew( p_aout ) )
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
    if ( p_aout->mixer.mixer.i_format != AOUT_FMT_FLOAT32 )
    {
        msg_Warn( p_aout, "MixerMultiplierGet called for non-float32 mixer" );
        return -1;
    }

    *pf_multiplier = p_aout->mixer.f_multiplier;
    return 0;
}

