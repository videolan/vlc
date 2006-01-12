/*****************************************************************************
 * dec.c : audio output API towards decoders
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
#include <vlc/input.h>                 /* for input_thread_t and i_pts_delay */

/*
 * Creation/Deletion
 */

/*****************************************************************************
 * aout_DecNew : create a decoder
 *****************************************************************************/
static aout_input_t * DecNew( vlc_object_t * p_this, aout_instance_t * p_aout,
                              audio_sample_format_t * p_format )
{
    aout_input_t * p_input;
    input_thread_t * p_input_thread;
    vlc_value_t val;

    /* We can only be called by the decoder, so no need to lock
     * p_input->lock. */
    vlc_mutex_lock( &p_aout->mixer_lock );

    if ( p_aout->i_nb_inputs >= AOUT_MAX_INPUTS )
    {
        msg_Err( p_aout, "too many inputs already (%d)", p_aout->i_nb_inputs );
        return NULL;
    }

    p_input = malloc(sizeof(aout_input_t));
    if ( p_input == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return NULL;
    }

    vlc_mutex_init( p_aout, &p_input->lock );

    p_input->b_changed = 0;
    p_input->b_error = 1;
    aout_FormatPrepare( p_format );
    memcpy( &p_input->input, p_format,
            sizeof(audio_sample_format_t) );

    p_aout->pp_inputs[p_aout->i_nb_inputs] = p_input;
    p_aout->i_nb_inputs++;

    if ( p_aout->mixer.b_error )
    {
        int i;

        var_Destroy( p_aout, "audio-device" );
        var_Destroy( p_aout, "audio-channels" );

        /* Recreate the output using the new format. */
        if ( aout_OutputNew( p_aout, p_format ) < 0 )
        {
            for ( i = 0; i < p_aout->i_nb_inputs - 1; i++ )
            {
                vlc_mutex_lock( &p_aout->pp_inputs[i]->lock );
                aout_InputDelete( p_aout, p_aout->pp_inputs[i] );
                vlc_mutex_unlock( &p_aout->pp_inputs[i]->lock );
            }
            vlc_mutex_unlock( &p_aout->mixer_lock );
            return p_input;
        }

        /* Create other input streams. */
        for ( i = 0; i < p_aout->i_nb_inputs - 1; i++ )
        {
            vlc_mutex_lock( &p_aout->pp_inputs[i]->lock );
            aout_InputDelete( p_aout, p_aout->pp_inputs[i] );
            aout_InputNew( p_aout, p_aout->pp_inputs[i] );
            vlc_mutex_unlock( &p_aout->pp_inputs[i]->lock );
        }
    }
    else
    {
        aout_MixerDelete( p_aout );
    }

    if ( aout_MixerNew( p_aout ) == -1 )
    {
        aout_OutputDelete( p_aout );
        vlc_mutex_unlock( &p_aout->mixer_lock );
        return NULL;
    }

    aout_InputNew( p_aout, p_input );

    vlc_mutex_unlock( &p_aout->mixer_lock );

    var_Create( p_this, "audio-desync", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_this, "audio-desync", &val );
    p_input->i_desync = val.i_int * 1000;

    p_input_thread = (input_thread_t *)vlc_object_find( p_this,
                                           VLC_OBJECT_INPUT, FIND_PARENT );
    if( p_input_thread )
    {
        p_input->i_pts_delay = p_input_thread->i_pts_delay;
        p_input->i_pts_delay += p_input->i_desync;
        vlc_object_release( p_input_thread );
    }
    else
    {
        p_input->i_pts_delay = DEFAULT_PTS_DELAY;
        p_input->i_pts_delay += p_input->i_desync;
    }

    return p_input;
}

aout_input_t * __aout_DecNew( vlc_object_t * p_this,
                              aout_instance_t ** pp_aout,
                              audio_sample_format_t * p_format )
{
    if ( *pp_aout == NULL )
    {
        /* Create an audio output if there is none. */
        *pp_aout = vlc_object_find( p_this, VLC_OBJECT_AOUT, FIND_ANYWHERE );

        if( *pp_aout == NULL )
        {
            msg_Dbg( p_this, "no aout present, spawning one" );

            *pp_aout = aout_New( p_this );
            /* Everything failed, I'm a loser, I just wanna die */
            if( *pp_aout == NULL )
            {
                return NULL;
            }
            vlc_object_attach( *pp_aout, p_this->p_vlc );
        }
        else
        {
            vlc_object_release( *pp_aout );
        }
    }

    return DecNew( p_this, *pp_aout, p_format );
}

/*****************************************************************************
 * aout_DecDelete : delete a decoder
 *****************************************************************************/
int aout_DecDelete( aout_instance_t * p_aout, aout_input_t * p_input )
{
    int i_input;

    /* This function can only be called by the decoder itself, so no need
     * to lock p_input->lock. */
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
        return -1;
    }

    /* Remove the input from the list. */
    memmove( &p_aout->pp_inputs[i_input], &p_aout->pp_inputs[i_input + 1],
             (AOUT_MAX_INPUTS - i_input - 1) * sizeof(aout_input_t *) );
    p_aout->i_nb_inputs--;

    aout_InputDelete( p_aout, p_input );

    vlc_mutex_destroy( &p_input->lock );
    free( p_input );

    if ( !p_aout->i_nb_inputs )
    {
        aout_OutputDelete( p_aout );
        aout_MixerDelete( p_aout );
        if ( var_Type( p_aout, "audio-device" ) != 0 )
        {
            var_Destroy( p_aout, "audio-device" );
        }
        if ( var_Type( p_aout, "audio-channels" ) != 0 )
        {
            var_Destroy( p_aout, "audio-channels" );
        }
    }

    vlc_mutex_unlock( &p_aout->mixer_lock );

    return 0;
}


/*
 * Buffer management
 */

/*****************************************************************************
 * aout_DecNewBuffer : ask for a new empty buffer
 *****************************************************************************/
aout_buffer_t * aout_DecNewBuffer( aout_instance_t * p_aout,
                                   aout_input_t * p_input,
                                   size_t i_nb_samples )
{
    aout_buffer_t * p_buffer;
    mtime_t duration;

    vlc_mutex_lock( &p_input->lock );

    if ( p_input->b_error )
    {
        vlc_mutex_unlock( &p_input->lock );
        return NULL;
    }

    duration = (1000000 * (mtime_t)i_nb_samples) / p_input->input.i_rate;

    /* This necessarily allocates in the heap. */
    aout_BufferAlloc( &p_input->input_alloc, duration, NULL, p_buffer );
    p_buffer->i_nb_samples = i_nb_samples;
    p_buffer->i_nb_bytes = i_nb_samples * p_input->input.i_bytes_per_frame
                              / p_input->input.i_frame_length;

    /* Suppose the decoder doesn't have more than one buffered buffer */
    p_input->b_changed = 0;

    vlc_mutex_unlock( &p_input->lock );

    if ( p_buffer == NULL )
    {
        msg_Err( p_aout, "NULL buffer !" );
    }
    else
    {
        p_buffer->start_date = p_buffer->end_date = 0;
    }

    return p_buffer;
}

/*****************************************************************************
 * aout_DecDeleteBuffer : destroy an undecoded buffer
 *****************************************************************************/
void aout_DecDeleteBuffer( aout_instance_t * p_aout, aout_input_t * p_input,
                           aout_buffer_t * p_buffer )
{
    aout_BufferFree( p_buffer );
}

/*****************************************************************************
 * aout_DecPlay : filter & mix the decoded buffer
 *****************************************************************************/
int aout_DecPlay( aout_instance_t * p_aout, aout_input_t * p_input,
                  aout_buffer_t * p_buffer )
{
    if ( p_buffer->start_date == 0 )
    {
        msg_Warn( p_aout, "non-dated buffer received" );
        aout_BufferFree( p_buffer );
        return -1;
    }

    /* Apply the desynchronisation requested by the user */
    p_buffer->start_date += p_input->i_desync;
    p_buffer->end_date += p_input->i_desync;

    if ( p_buffer->start_date > mdate() + p_input->i_pts_delay +
         AOUT_MAX_ADVANCE_TIME )
    {
        msg_Warn( p_aout, "received buffer in the future ("I64Fd")",
                  p_buffer->start_date - mdate());
        aout_BufferFree( p_buffer );
        return -1;
    }

    p_buffer->end_date = p_buffer->start_date
                            + (mtime_t)(p_buffer->i_nb_samples * 1000000)
                                / p_input->input.i_rate;

    vlc_mutex_lock( &p_input->lock );

    if ( p_input->b_error )
    {
        vlc_mutex_unlock( &p_input->lock );
        aout_BufferFree( p_buffer );
        return -1;
    }

    if ( p_input->b_changed )
    {
        /* Maybe the allocation size has changed. Re-allocate a buffer. */
        aout_buffer_t * p_new_buffer;
        mtime_t duration = (1000000 * (mtime_t)p_buffer->i_nb_samples)
                            / p_input->input.i_rate;

        aout_BufferAlloc( &p_input->input_alloc, duration, NULL, p_new_buffer );
        p_aout->p_vlc->pf_memcpy( p_new_buffer->p_buffer, p_buffer->p_buffer,
                                  p_buffer->i_nb_bytes );
        p_new_buffer->i_nb_samples = p_buffer->i_nb_samples;
        p_new_buffer->i_nb_bytes = p_buffer->i_nb_bytes;
        p_new_buffer->start_date = p_buffer->start_date;
        p_new_buffer->end_date = p_buffer->end_date;
        aout_BufferFree( p_buffer );
        p_buffer = p_new_buffer;
        p_input->b_changed = 0;
    }

    /* If the buffer is too early, wait a while. */
    mwait( p_buffer->start_date - AOUT_MAX_PREPARE_TIME );

    if ( aout_InputPlay( p_aout, p_input, p_buffer ) == -1 )
    {
        vlc_mutex_unlock( &p_input->lock );
        return -1;
    }

    vlc_mutex_unlock( &p_input->lock );

    /* Run the mixer if it is able to run. */
    vlc_mutex_lock( &p_aout->mixer_lock );
    aout_MixerRun( p_aout );
    vlc_mutex_unlock( &p_aout->mixer_lock );

    return 0;
}
