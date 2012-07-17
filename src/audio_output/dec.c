/*****************************************************************************
 * dec.c : audio output API towards decoders
 *****************************************************************************
 * Copyright (C) 2002-2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_input.h>
#include <vlc_atomic.h>

#include "aout_internal.h"
#include "libvlc.h"

/**
 * Creates an audio output
 */
int aout_DecNew( audio_output_t *p_aout,
                 const audio_sample_format_t *p_format,
                 const audio_replay_gain_t *p_replay_gain,
                 const aout_request_vout_t *p_request_vout )
{
    /* Sanitize audio format */
    if( p_format->i_channels != aout_FormatNbChannels( p_format ) )
    {
        msg_Err( p_aout, "incompatible audio channels count with layout mask" );
        return -1;
    }

    if( p_format->i_rate > 192000 )
    {
        msg_Err( p_aout, "excessive audio sample frequency (%u)",
                 p_format->i_rate );
        return -1;
    }
    if( p_format->i_rate < 4000 )
    {
        msg_Err( p_aout, "too low audio sample frequency (%u)",
                 p_format->i_rate );
        return -1;
    }

    aout_owner_t *owner = aout_owner(p_aout);
#ifdef RECYCLE
    /* Calling decoder is responsible for serializing aout_DecNew() and
     * aout_DecDelete(). So no need to lock to _read_ those properties. */
    if (owner->module != NULL) /* <- output exists */
    {   /* Check if we can recycle the existing output and pipelines */
        if (AOUT_FMTS_IDENTICAL(&owner->input_format, p_format))
            return 0;

        /* TODO? If the new input format is closer to the output format than
         * the old input format was, then the output could be recycled. The
         * input pipeline however would need to be restarted. */

        /* No recycling: delete everything and restart from scratch */
        aout_Shutdown (p_aout);
    }
#endif
    int ret = 0;

    /* TODO: reduce lock scope depending on decoder's real need */
    aout_lock( p_aout );
    assert (owner->module == NULL);

    /* Create the audio output stream */
    var_Destroy( p_aout, "audio-device" );
    var_Destroy( p_aout, "audio-channels" );

    owner->input_format = *p_format;
    vlc_atomic_set (&owner->restart, 0);
    owner->volume = aout_volume_New (p_aout, p_replay_gain);
    if( aout_OutputNew( p_aout, p_format ) < 0 )
    {
        ret = -1;
        goto error;
    }
    aout_volume_SetFormat (owner->volume, owner->mixer_format.i_format);

    /* Create the audio filtering "input" pipeline */
    date_Init (&owner->sync.date, owner->mixer_format.i_rate, 1);
    date_Set (&owner->sync.date, VLC_TS_INVALID);

    assert (owner->input == NULL);
    owner->input = aout_InputNew (p_aout, p_format, &owner->mixer_format,
                                  p_request_vout);
    if (owner->input == NULL)
    {
        aout_OutputDelete (p_aout);
        aout_volume_Delete (owner->volume);
        aout_unlock (p_aout);
        return -1;
    }
error:
    aout_unlock( p_aout );
    return ret;
}

/**
 * Stops all plugins involved in the audio output.
 */
void aout_Shutdown (audio_output_t *p_aout)
{
    aout_owner_t *owner = aout_owner (p_aout);
    aout_input_t *input;

    aout_lock( p_aout );
    /* Remove the input. */
    input = owner->input;
    if (likely(input != NULL))
        aout_InputDelete (p_aout, input);
    owner->input = NULL;

    aout_OutputDelete( p_aout );
    aout_volume_Delete (owner->volume);
    var_Destroy( p_aout, "audio-device" );
    var_Destroy( p_aout, "audio-channels" );

    aout_unlock( p_aout );
    free (input);
}

/**
 * Stops the decoded audio input.
 * @note Due to output recycling, this function is esssentially a stub.
 */
void aout_DecDelete (audio_output_t *aout)
{
#ifdef RECYCLE
    (void) aout;
#else
    aout_Shutdown (aout);
#endif
}

#define AOUT_RESTART_OUTPUT 1
#define AOUT_RESTART_INPUT  2
static void aout_CheckRestart (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);

    aout_assert_locked (aout);

    int restart = vlc_atomic_swap (&owner->restart, 0);
    if (likely(restart == 0))
        return;

    assert (restart & AOUT_RESTART_INPUT);

    const aout_request_vout_t request_vout = owner->input->request_vout;

    if (likely(owner->input != NULL))
        aout_InputDelete (aout, owner->input);
    owner->input = NULL;

    /* Reinitializes the output */
    if (restart & AOUT_RESTART_OUTPUT)
    {
        aout_OutputDelete (aout);
        if (aout_OutputNew (aout, &owner->input_format))
        {
            aout_volume_Delete (owner->volume);
            return; /* we are officially screwed */
        }
        aout_volume_SetFormat (owner->volume, owner->mixer_format.i_format);
    }

    owner->input = aout_InputNew (aout, &owner->input_format,
                                  &owner->mixer_format, &request_vout);
}

/**
 * Marks the audio output for restart, to update any parameter of the output
 * plug-in (e.g. output device or channel mapping).
 */
void aout_RequestRestart (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);

    /* DO NOT remove AOUT_RESTART_INPUT. You need to change the atomic ops. */
    vlc_atomic_set (&owner->restart, AOUT_RESTART_OUTPUT|AOUT_RESTART_INPUT);
}

/**
 * This function will safely mark aout input to be restarted as soon as
 * possible to take configuration changes into account
 */
void aout_InputRequestRestart (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);

    vlc_atomic_compare_swap (&owner->restart, 0, AOUT_RESTART_INPUT);
}


/*
 * Buffer management
 */

/*****************************************************************************
 * aout_DecNewBuffer : ask for a new empty buffer
 *****************************************************************************/
block_t *aout_DecNewBuffer (audio_output_t *aout, size_t samples)
{
    /* NOTE: the caller is responsible for serializing input change */
    aout_owner_t *owner = aout_owner (aout);

    size_t length = samples * owner->input_format.i_bytes_per_frame
                            / owner->input_format.i_frame_length;
    block_t *block = block_Alloc( length );
    if( likely(block != NULL) )
    {
        block->i_nb_samples = samples;
        block->i_pts = block->i_length = 0;
    }
    return block;
}

/*****************************************************************************
 * aout_DecDeleteBuffer : destroy an undecoded buffer
 *****************************************************************************/
void aout_DecDeleteBuffer (audio_output_t *aout, block_t *block)
{
    (void) aout;
    block_Release (block);
}

/*****************************************************************************
 * aout_DecPlay : filter & mix the decoded buffer
 *****************************************************************************/
int aout_DecPlay (audio_output_t *p_aout, block_t *p_buffer, int i_input_rate)
{
    aout_owner_t *owner = aout_owner (p_aout);
    aout_input_t *input;

    assert( i_input_rate >= INPUT_RATE_DEFAULT / AOUT_MAX_INPUT_RATE &&
            i_input_rate <= INPUT_RATE_DEFAULT * AOUT_MAX_INPUT_RATE );
    assert( p_buffer->i_pts > 0 );

    p_buffer->i_length = (mtime_t)p_buffer->i_nb_samples * 1000000
                                / owner->input_format.i_rate;

    aout_lock( p_aout );
    aout_CheckRestart( p_aout );

    input = owner->input;
    if (unlikely(input == NULL)) /* can happen due to restart */
    {
        aout_unlock( p_aout );
        block_Release( p_buffer );
        return -1;
    }

    /* Input */
    p_buffer = aout_InputPlay (p_aout, input, p_buffer, i_input_rate,
                               &owner->sync.date);
    if( p_buffer != NULL )
    {
        date_Increment (&owner->sync.date, p_buffer->i_nb_samples);

        /* Mixer */
        aout_volume_Amplify (owner->volume, p_buffer);

        /* Output */
        aout_OutputPlay( p_aout, p_buffer );
    }

    aout_unlock( p_aout );
    return 0;
}

int aout_DecGetResetLost (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);
    aout_input_t *input = owner->input;
    int val;

    aout_lock (aout);
    if (likely(input != NULL))
    {
        val = input->i_buffer_lost;
        input->i_buffer_lost = 0;
    }
    else
        val = 0; /* if aout_CheckRestart() failed */
    aout_unlock (aout);

    return val;
}

void aout_DecChangePause (audio_output_t *aout, bool paused, mtime_t date)
{
    aout_owner_t *owner = aout_owner (aout);

    aout_lock (aout);
    /* XXX: Should the date be offset by the pause duration instead? */
    date_Set (&owner->sync.date, VLC_TS_INVALID);
    aout_OutputPause (aout, paused, date);
    aout_unlock (aout);
}

void aout_DecFlush (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);

    aout_lock (aout);
    date_Set (&owner->sync.date, VLC_TS_INVALID);
    aout_OutputFlush (aout, false);
    aout_unlock (aout);
}

bool aout_DecIsEmpty (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);
    mtime_t end_date, now = mdate ();
    bool empty;

    aout_lock (aout);
    end_date = date_Get (&owner->sync.date);
    empty = end_date == VLC_TS_INVALID || end_date <= now;
    if (empty)
        /* The last PTS has elapsed already. So the underlying audio output
         * buffer should be empty or almost. Thus draining should be fast
         * and will not block the caller too long. */
        aout_OutputFlush (aout, true);
    aout_unlock (aout);
    return empty;
}
