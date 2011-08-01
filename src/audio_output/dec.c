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

#undef aout_DecNew
/**
 * Creates an audio output
 */
aout_input_t *aout_DecNew( audio_output_t *p_aout,
                           audio_sample_format_t *p_format,
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
    p_input->i_pause_date = VLC_TS_INVALID;

    aout_FormatPrepare( p_format );

    memcpy( &p_input->input, p_format,
            sizeof(audio_sample_format_t) );
    if( p_replay_gain )
        p_input->replay_gain = *p_replay_gain;

    /* We can only be called by the decoder, so no need to lock
     * p_input->lock. */
    aout_lock( p_aout );
    assert( p_aout->p_input == NULL );
    p_aout->p_input = p_input;

    var_Destroy( p_aout, "audio-device" );
    var_Destroy( p_aout, "audio-channels" );

    /* Recreate the output using the new format. */
    if( aout_OutputNew( p_aout, p_format ) < 0 )
#warning Input without output and mixer = bad idea.
        goto out;

    assert( p_aout->mixer == NULL );
    p_aout->mixer = aout_MixerNew( p_aout, &p_aout->mixer_format );
    if( p_aout->mixer == NULL )
    {
        aout_OutputDelete( p_aout );
#warning Memory leak.
        p_input = NULL;
        goto out;
    }

    aout_InputNew( p_aout, p_input, p_request_vout );
out:
    aout_unlock( p_aout );
    return p_input;
}

/*****************************************************************************
 * aout_DecDelete : delete a decoder
 *****************************************************************************/
void aout_DecDelete( audio_output_t * p_aout, aout_input_t * p_input )
{
    aout_lock( p_aout );
    /* Remove the input. */
    assert( p_input == p_aout->p_input ); /* buggy decoder? */
    p_aout->p_input = NULL;
    aout_InputDelete( p_aout, p_input );

    aout_OutputDelete( p_aout );
    aout_MixerDelete( p_aout->mixer );
    p_aout->mixer = NULL;
    var_Destroy( p_aout, "audio-device" );
    var_Destroy( p_aout, "audio-channels" );

    aout_unlock( p_aout );
    free( p_input );
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

    aout_InputCheckAndRestart( p_aout, p_input );
    aout_InputPlay( p_aout, p_input, p_buffer, i_input_rate );

    const float amp = p_aout->mixer_multiplier * p_input->multiplier;
    while( (p_buffer = aout_OutputSlice( p_aout, &p_input->fifo ) ) != NULL )
    {
        aout_MixerRun( p_aout->mixer, p_buffer, amp );
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
    aout_lock( p_aout );
    assert( p_aout->p_input == p_input );

    aout_OutputPause( p_aout, b_paused, i_date );

    if( b_paused )
    {
        p_input->i_pause_date = i_date;
    }
    else
    {
        assert( p_input->i_pause_date != VLC_TS_INVALID );

        mtime_t i_duration = i_date - p_input->i_pause_date;
        p_input->i_pause_date = VLC_TS_INVALID;
        aout_FifoMoveDates( &p_input->fifo, i_duration );
    }
    aout_unlock( p_aout );
}

void aout_DecFlush( audio_output_t *p_aout, aout_input_t *p_input )
{
    aout_lock( p_aout );
    aout_FifoReset( &p_input->fifo );
    aout_OutputFlush( p_aout, false );
    aout_unlock( p_aout );
}

bool aout_DecIsEmpty( audio_output_t * p_aout, aout_input_t * p_input )
{
    mtime_t end_date;

    aout_lock( p_aout );
    end_date = aout_FifoNextStart( &p_input->fifo );
    aout_unlock( p_aout );
    return end_date == VLC_TS_INVALID || end_date <= mdate();
}
