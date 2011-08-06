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

#include <assert.h>

#include <vlc_common.h>

#include <vlc_aout.h>
#include <vlc_input.h>

#include "aout_internal.h"
#include "libvlc.h"

#undef aout_DecNew
/**
 * Creates an audio output
 */
aout_input_t *aout_DecNew( audio_output_t *p_aout,
                           const audio_sample_format_t *p_format,
                           const audio_replay_gain_t *p_replay_gain,
                           const aout_request_vout_t *p_request_vout )
{
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

    aout_input_t *p_input = calloc( 1, sizeof(aout_input_t));
    if( !p_input )
        return NULL;

    p_input->b_error = true;

    memcpy( &p_input->input, p_format,
            sizeof(audio_sample_format_t) );
    if( p_replay_gain )
        p_input->replay_gain = *p_replay_gain;

    /* We can only be called by the decoder, so no need to lock
     * p_input->lock. */
    aout_owner_t *owner = aout_owner(p_aout);
    aout_lock( p_aout );
    assert (owner->input == NULL);

    var_Destroy( p_aout, "audio-device" );
    var_Destroy( p_aout, "audio-channels" );

    /* Recreate the output using the new format. */
    if( aout_OutputNew( p_aout, p_format ) < 0 )
        goto error;

    assert (owner->volume.mixer == NULL);
    owner->volume.mixer = aout_MixerNew (p_aout, owner->mixer_format.i_format);
    if (owner->volume.mixer == NULL)
    {
        aout_OutputDelete( p_aout );
        goto error;
    }

    owner->input = p_input;
    aout_InputNew( p_aout, p_input, p_request_vout );
    aout_unlock( p_aout );
    return p_input;
error:
    aout_unlock( p_aout );
    free( p_input );
    return NULL;
}

/*****************************************************************************
 * aout_DecDelete : delete a decoder
 *****************************************************************************/
void aout_DecDelete( audio_output_t * p_aout, aout_input_t * p_input )
{
    aout_owner_t *owner = aout_owner (p_aout);
    struct audio_mixer *mixer;

    aout_lock( p_aout );
    /* Remove the input. */
    assert (owner->input == p_input); /* buggy decoder? */
    owner->input = NULL;
    aout_InputDelete( p_aout, p_input );

    aout_OutputDelete( p_aout );
    mixer = owner->volume.mixer;
    owner->volume.mixer = NULL;
    var_Destroy( p_aout, "audio-device" );
    var_Destroy( p_aout, "audio-channels" );

    aout_unlock( p_aout );

    aout_MixerDelete (mixer);
    free( p_input );
}

static void aout_CheckRestart (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);
    aout_input_t *input = owner->input;

    aout_assert_locked (aout);

    if (likely(!owner->need_restart))
        return;
    owner->need_restart = false;

    /* Reinitializes the output */
    aout_InputDelete (aout, owner->input);
    aout_MixerDelete (owner->volume.mixer);
    owner->volume.mixer = NULL;
    aout_OutputDelete (aout);

    if (aout_OutputNew (aout, &input->input))
    {
error:
        input->b_error = true;
        return; /* we are officially screwed */
    }

    owner->volume.mixer = aout_MixerNew (aout, owner->mixer_format.i_format);
    if (owner->volume.mixer == NULL)
    {
        aout_OutputDelete (aout);
        goto error;
    }

    if (aout_InputNew (aout, input, &input->request_vout))
        assert (input->b_error);
    else
        assert (!input->b_error);
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
    size_t length = i_nb_samples * p_input->input.i_bytes_per_frame
                                 / p_input->input.i_frame_length;
    block_t *block = block_Alloc( length );
    if( likely(block != NULL) )
    {
        block->i_nb_samples = i_nb_samples;
        block->i_pts = block->i_length = 0;
    }
    return block;
}

/*****************************************************************************
 * aout_DecDeleteBuffer : destroy an undecoded buffer
 *****************************************************************************/
void aout_DecDeleteBuffer( audio_output_t * p_aout, aout_input_t * p_input,
                           aout_buffer_t * p_buffer )
{
    (void)p_aout; (void)p_input;
    aout_BufferFree( p_buffer );
}

/*****************************************************************************
 * aout_DecPlay : filter & mix the decoded buffer
 *****************************************************************************/
int aout_DecPlay( audio_output_t * p_aout, aout_input_t * p_input,
                  aout_buffer_t * p_buffer, int i_input_rate )
{
    aout_owner_t *owner = aout_owner (p_aout);
    assert( i_input_rate >= INPUT_RATE_DEFAULT / AOUT_MAX_INPUT_RATE &&
            i_input_rate <= INPUT_RATE_DEFAULT * AOUT_MAX_INPUT_RATE );
    assert( p_buffer->i_pts > 0 );

    p_buffer->i_length = (mtime_t)p_buffer->i_nb_samples * 1000000
                                / p_input->input.i_rate;

    aout_lock( p_aout );
    if( p_input->b_error )
    {
        aout_unlock( p_aout );
        aout_BufferFree( p_buffer );
        return -1;
    }

    aout_CheckRestart( p_aout );
    aout_InputCheckAndRestart( p_aout, p_input );

    /* Input */
    p_buffer = aout_InputPlay( p_aout, p_input, p_buffer, i_input_rate );

    if( p_buffer != NULL )
    {
        /* Mixer */
        float amp = owner->volume.multiplier * p_input->multiplier;
        aout_MixerRun (owner->volume.mixer, p_buffer, amp);

        /* Output */
        aout_OutputPlay( p_aout, p_buffer );
    }

    aout_unlock( p_aout );
    return 0;
}

int aout_DecGetResetLost( audio_output_t *p_aout, aout_input_t *p_input )
{
    int val;

    aout_lock( p_aout );
    val = p_input->i_buffer_lost;
    p_input->i_buffer_lost = 0;
    aout_unlock( p_aout );

    return val;
}

void aout_DecChangePause( audio_output_t *p_aout, aout_input_t *p_input, bool b_paused, mtime_t i_date )
{
    aout_owner_t *owner = aout_owner (p_aout);

    aout_lock( p_aout );
    assert (owner->input == p_input);

    /* XXX: Should the input date be offset by the pause duration instead? */
    date_Set (&p_input->date, VLC_TS_INVALID);
    aout_OutputPause( p_aout, b_paused, i_date );
    aout_unlock( p_aout );
}

void aout_DecFlush( audio_output_t *p_aout, aout_input_t *p_input )
{
    aout_lock( p_aout );
    date_Set (&p_input->date, VLC_TS_INVALID);
    aout_OutputFlush( p_aout, false );
    aout_unlock( p_aout );
}

bool aout_DecIsEmpty( audio_output_t * p_aout, aout_input_t * p_input )
{
    mtime_t end_date;

    aout_lock( p_aout );
    /* FIXME: tell output to drain */
    end_date = date_Get (&p_input->date);
    aout_unlock( p_aout );
    return end_date == VLC_TS_INVALID || end_date <= mdate();
}
