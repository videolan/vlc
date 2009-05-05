/*****************************************************************************
 * dec.c : audio output API towards decoders
 *****************************************************************************
 * Copyright (C) 2002-2007 the VideoLAN team
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#ifdef HAVE_ALLOCA_H
#   include <alloca.h>
#endif

#include <vlc_aout.h>
#include <vlc_input.h>

#include "aout_internal.h"

/*****************************************************************************
 * aout_DecNew : create a decoder
 *****************************************************************************/
static aout_input_t * DecNew( aout_instance_t * p_aout,
                              audio_sample_format_t *p_format,
                              const audio_replay_gain_t *p_replay_gain,
                              const aout_request_vout_t *p_request_vout )
{
    aout_input_t * p_input;

    /* Sanitize audio format */
    if( p_format->i_channels > 32 )
    {
        msg_Err( p_aout, "too many audio channels (%u)",
                 p_format->i_channels );
        return NULL;
    }
    if( p_format->i_channels <= 0 )
    {
        msg_Err( p_aout, "no audio channels" );
        return NULL;
    }
    if( p_format->i_channels != aout_FormatNbChannels( p_format ) )
    {
        msg_Err( p_aout, "incompatible audio channels count with layout mask" );
        return NULL;
    }

    if( p_format->i_rate > 192000 )
    {
        msg_Err( p_aout, "excessive audio sample frequency (%u)",
                 p_format->i_rate );
        return NULL;
    }
    if( p_format->i_rate < 4000 )
    {
        msg_Err( p_aout, "too low audio sample frequency (%u)",
                 p_format->i_rate );
        return NULL;
    }

    /* We can only be called by the decoder, so no need to lock
     * p_input->lock. */
    aout_lock_mixer( p_aout );

    if ( p_aout->i_nb_inputs >= AOUT_MAX_INPUTS )
    {
        msg_Err( p_aout, "too many inputs already (%d)", p_aout->i_nb_inputs );
        goto error;
    }

    p_input = calloc( 1, sizeof(aout_input_t));
    if( !p_input )
        goto error;

    vlc_mutex_init( &p_input->lock );

    p_input->b_changed = false;
    p_input->b_error = true;
    p_input->b_paused = false;
    p_input->i_pause_date = 0;

    aout_FormatPrepare( p_format );

    memcpy( &p_input->input, p_format,
            sizeof(audio_sample_format_t) );
    if( p_replay_gain )
        p_input->replay_gain = *p_replay_gain;

    aout_lock_input_fifos( p_aout );
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
                aout_lock_input( p_aout, p_aout->pp_inputs[i] );
                aout_InputDelete( p_aout, p_aout->pp_inputs[i] );
                aout_unlock_input( p_aout, p_aout->pp_inputs[i] );
            }
            aout_unlock_input_fifos( p_aout );
            aout_unlock_mixer( p_aout );
            return p_input;
        }

        /* Create other input streams. */
        for ( i = 0; i < p_aout->i_nb_inputs - 1; i++ )
        {
            aout_input_t *p_input = p_aout->pp_inputs[i];

            aout_lock_input( p_aout, p_input );
            aout_InputDelete( p_aout, p_input );
            aout_InputNew( p_aout, p_input, &p_input->request_vout );
            aout_unlock_input( p_aout, p_input );
        }
    }
    else
    {
        aout_MixerDelete( p_aout );
    }

    if ( aout_MixerNew( p_aout ) == -1 )
    {
        aout_OutputDelete( p_aout );
        aout_unlock_input_fifos( p_aout );
        goto error;
    }

    aout_InputNew( p_aout, p_input, p_request_vout );
    aout_unlock_input_fifos( p_aout );

    aout_unlock_mixer( p_aout );

    return p_input;

error:
    aout_unlock_mixer( p_aout );
    return NULL;
}

aout_input_t * __aout_DecNew( vlc_object_t * p_this,
                              aout_instance_t ** pp_aout,
                              audio_sample_format_t * p_format,
                              const audio_replay_gain_t *p_replay_gain,
                              const aout_request_vout_t *p_request_video )
{
    aout_instance_t *p_aout = *pp_aout;
    if ( p_aout == NULL )
    {
        msg_Dbg( p_this, "no aout present, spawning one" );
        p_aout = aout_New( p_this );

        /* Everything failed, I'm a loser, I just wanna die */
        if( p_aout == NULL )
            return NULL;

        vlc_object_attach( p_aout, p_this );
        *pp_aout = p_aout;
    }

    return DecNew( p_aout, p_format, p_replay_gain, p_request_video );
}

/*****************************************************************************
 * aout_DecDelete : delete a decoder
 *****************************************************************************/
int aout_DecDelete( aout_instance_t * p_aout, aout_input_t * p_input )
{
    int i_input;

    /* This function can only be called by the decoder itself, so no need
     * to lock p_input->lock. */
    aout_lock_mixer( p_aout );

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
        aout_unlock_mixer( p_aout );
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

    aout_unlock_mixer( p_aout );

    return 0;
}


/*
 * Buffer management
 */

/*****************************************************************************
 * aout_DecNewBuffer : ask for a new empty buffer
 *****************************************************************************/
aout_buffer_t * aout_DecNewBuffer( aout_input_t * p_input,
                                   size_t i_nb_samples )
{
    aout_buffer_t * p_buffer;
    mtime_t duration;

    aout_lock_input( NULL, p_input );

    if ( p_input->b_error )
    {
        aout_unlock_input( NULL, p_input );
        return NULL;
    }

    duration = (1000000 * (mtime_t)i_nb_samples) / p_input->input.i_rate;

    /* This necessarily allocates in the heap. */
    aout_BufferAlloc( &p_input->input_alloc, duration, NULL, p_buffer );
    if( p_buffer != NULL )
        p_buffer->i_nb_bytes = i_nb_samples * p_input->input.i_bytes_per_frame
                                  / p_input->input.i_frame_length;

    /* Suppose the decoder doesn't have more than one buffered buffer */
    p_input->b_changed = false;

    aout_unlock_input( NULL, p_input );

    if( p_buffer == NULL )
        return NULL;

    p_buffer->i_nb_samples = i_nb_samples;
    p_buffer->start_date = p_buffer->end_date = 0;
    return p_buffer;
}

/*****************************************************************************
 * aout_DecDeleteBuffer : destroy an undecoded buffer
 *****************************************************************************/
void aout_DecDeleteBuffer( aout_instance_t * p_aout, aout_input_t * p_input,
                           aout_buffer_t * p_buffer )
{
    (void)p_aout; (void)p_input;
    aout_BufferFree( p_buffer );
}

/*****************************************************************************
 * aout_DecPlay : filter & mix the decoded buffer
 *****************************************************************************/
int aout_DecPlay( aout_instance_t * p_aout, aout_input_t * p_input,
                  aout_buffer_t * p_buffer, int i_input_rate )
{
    assert( i_input_rate >= INPUT_RATE_DEFAULT / AOUT_MAX_INPUT_RATE &&
            i_input_rate <= INPUT_RATE_DEFAULT * AOUT_MAX_INPUT_RATE );

    assert( p_buffer->start_date > 0 );

    p_buffer->end_date = p_buffer->start_date
                            + (mtime_t)p_buffer->i_nb_samples * 1000000
                                / p_input->input.i_rate;

    aout_lock_input( p_aout, p_input );

    if( p_input->b_error )
    {
        aout_unlock_input( p_aout, p_input );
        aout_BufferFree( p_buffer );
        return -1;
    }

    if( p_input->b_changed )
    {
        /* Maybe the allocation size has changed. Re-allocate a buffer. */
        aout_buffer_t * p_new_buffer;
        mtime_t duration = (1000000 * (mtime_t)p_buffer->i_nb_samples)
                            / p_input->input.i_rate;

        aout_BufferAlloc( &p_input->input_alloc, duration, NULL, p_new_buffer );
        vlc_memcpy( p_new_buffer->p_buffer, p_buffer->p_buffer,
                    p_buffer->i_nb_bytes );
        p_new_buffer->i_nb_samples = p_buffer->i_nb_samples;
        p_new_buffer->i_nb_bytes = p_buffer->i_nb_bytes;
        p_new_buffer->start_date = p_buffer->start_date;
        p_new_buffer->end_date = p_buffer->end_date;
        aout_BufferFree( p_buffer );
        p_buffer = p_new_buffer;
        p_input->b_changed = false;
    }

    int i_ret = aout_InputPlay( p_aout, p_input, p_buffer, i_input_rate );

    aout_unlock_input( p_aout, p_input );

    if( i_ret == -1 )
        return -1;

    /* Run the mixer if it is able to run. */
    aout_lock_mixer( p_aout );

    aout_MixerRun( p_aout );

    aout_unlock_mixer( p_aout );

    return 0;
}

int aout_DecGetResetLost( aout_instance_t *p_aout, aout_input_t *p_input )
{
    aout_lock_input( p_aout, p_input );
    int i_value = p_input->i_buffer_lost;
    p_input->i_buffer_lost = 0;
    aout_unlock_input( p_aout, p_input );

    return i_value;
}

void aout_DecChangePause( aout_instance_t *p_aout, aout_input_t *p_input, bool b_paused, mtime_t i_date )
{
    mtime_t i_duration = 0;
    aout_lock_input( p_aout, p_input );
    assert( !p_input->b_paused || !b_paused );
    if( p_input->b_paused )
    {
        i_duration = i_date - p_input->i_pause_date;
    }
    p_input->b_paused = b_paused;
    p_input->i_pause_date = i_date;
    aout_unlock_input( p_aout, p_input );

    if( i_duration != 0 )
    {
        aout_lock_mixer( p_aout );
        for( aout_buffer_t *p = p_input->fifo.p_first; p != NULL; p = p->p_next )
        {
            p->start_date += i_duration;
            p->end_date += i_duration;
        }
        aout_unlock_mixer( p_aout );
    }
}

void aout_DecFlush( aout_instance_t *p_aout, aout_input_t *p_input )
{
    aout_lock_input_fifos( p_aout );

    aout_FifoSet( p_aout, &p_input->fifo, 0 );
    p_input->p_first_byte_to_mix = NULL;

    aout_unlock_input_fifos( p_aout );
}

