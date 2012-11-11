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
    int ret = 0;

    /* TODO: reduce lock scope depending on decoder's real need */
    aout_lock( p_aout );

    var_Destroy( p_aout, "stereo-mode" );

    /* Create the audio output stream */
    owner->input_format = *p_format;
    vlc_atomic_set (&owner->restart, 0);
    owner->volume = aout_volume_New (p_aout, p_replay_gain);
    if( aout_OutputNew( p_aout, p_format ) < 0 )
        goto error;
    aout_volume_SetFormat (owner->volume, owner->mixer_format.i_format);

    /* Create the audio filtering "input" pipeline */
    if (aout_FiltersNew (p_aout, p_format, &owner->mixer_format,
                         p_request_vout))
    {
        aout_OutputDelete (p_aout);
error:
        aout_volume_Delete (owner->volume);
        ret = -1;
        goto error;
    }

    date_Init (&owner->sync.date, owner->mixer_format.i_rate, 1);
    date_Set (&owner->sync.date, VLC_TS_INVALID);
    owner->sync.resamp_type = AOUT_RESAMPLING_NONE;
    aout_unlock( p_aout );

    atomic_init (&owner->buffers_lost, 0);

    return ret;
}

/**
 * Stops all plugins involved in the audio output.
 */
void aout_DecDelete (audio_output_t *p_aout)
{
    aout_owner_t *owner = aout_owner (p_aout);

    aout_lock( p_aout );
    aout_FiltersDelete (p_aout);
    aout_OutputDelete( p_aout );
    aout_volume_Delete (owner->volume);

    var_Destroy( p_aout, "stereo-mode" );

    aout_unlock( p_aout );
}

#define AOUT_RESTART_OUTPUT 1
#define AOUT_RESTART_INPUT  2
static int aout_CheckRestart (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);

    aout_assert_locked (aout);

    int restart = vlc_atomic_swap (&owner->restart, 0);
    if (likely(restart == 0))
        return 0;

    assert (restart & AOUT_RESTART_INPUT);

    const aout_request_vout_t request_vout = owner->request_vout;

    aout_FiltersDelete (aout);

    /* Reinitializes the output */
    if (restart & AOUT_RESTART_OUTPUT)
    {
        aout_OutputDelete (aout);
        if (aout_OutputNew (aout, &owner->input_format))
            abort (); /* FIXME we are officially screwed */
        aout_volume_SetFormat (owner->volume, owner->mixer_format.i_format);
    }

    owner->sync.resamp_type = AOUT_RESAMPLING_NONE;

    if (aout_FiltersNew (aout, &owner->input_format, &owner->mixer_format,
                         &request_vout))
    {
        abort (); /* FIXME */
    }
    return 0;
}

/**
 * Marks the audio output for restart, to update any parameter of the output
 * plug-in (e.g. output device or channel mapping).
 */
static void aout_RequestRestart (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);

    /* DO NOT remove AOUT_RESTART_INPUT. You need to change the atomic ops. */
    vlc_atomic_set (&owner->restart, AOUT_RESTART_OUTPUT|AOUT_RESTART_INPUT);
}

int aout_ChannelsRestart (vlc_object_t *obj, const char *varname,
                          vlc_value_t oldval, vlc_value_t newval, void *data)
{
    audio_output_t *aout = (audio_output_t *)obj;
    (void)oldval; (void)newval; (void)data;

    if (!strcmp (varname, "audio-device"))
    {
        /* This is supposed to be a significant change and supposes
         * rebuilding the channel choices. */
        var_Destroy (aout, "stereo-mode");
    }
    aout_RequestRestart (aout);
    return 0;
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

static void aout_StopResampling (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);

    owner->sync.resamp_type = AOUT_RESAMPLING_NONE;
    aout_FiltersAdjustResampling (aout, 0);
}

/*****************************************************************************
 * aout_DecPlay : filter & mix the decoded buffer
 *****************************************************************************/
int aout_DecPlay (audio_output_t *aout, block_t *block, int input_rate)
{
    aout_owner_t *owner = aout_owner (aout);

    assert (input_rate >= INPUT_RATE_DEFAULT / AOUT_MAX_INPUT_RATE);
    assert (input_rate <= INPUT_RATE_DEFAULT * AOUT_MAX_INPUT_RATE);
    assert (block->i_pts >= VLC_TS_0);

    block->i_length = CLOCK_FREQ * block->i_nb_samples
                                 / owner->input_format.i_rate;

    aout_lock (aout);
    if (unlikely(aout_CheckRestart (aout)))
        goto drop; /* Pipeline is unrecoverably broken :-( */

    /* We don't care if someone changes the start date behind our back after
     * this. We'll deal with that when pushing the buffer, and compensate
     * with the next incoming buffer. */
    mtime_t start_date = date_Get (&owner->sync.date);
    const mtime_t now = mdate ();

    if (start_date != VLC_TS_INVALID && start_date < now)
    {   /* The decoder is _very_ late. This can only happen if the user
         * pauses the stream (or if the decoder is buggy, which cannot
         * happen :). */
        msg_Warn (aout, "computed PTS is out of range (%"PRId64"), "
                  "clearing out", now - start_date);
        aout_OutputFlush (aout, false);
        if (owner->sync.resamp_type != AOUT_RESAMPLING_NONE)
            msg_Warn (aout, "timing screwed, stopping resampling");
        aout_StopResampling (aout);
        block->i_flags |= BLOCK_FLAG_DISCONTINUITY;
        start_date = VLC_TS_INVALID;
    }

    if (block->i_pts < now + AOUT_MIN_PREPARE_TIME)
    {   /* The decoder gives us f*cked up PTS. It's its business, but we
         * can't present it anyway, so drop the buffer. */
        msg_Warn (aout, "PTS is out of range (%"PRId64"), dropping buffer",
                  now - block->i_pts);
        aout_StopResampling (aout);
        goto drop;
    }

    /* If the audio drift is too big then it's not worth trying to resample
     * the audio. */
    if (start_date == VLC_TS_INVALID)
    {
        start_date = block->i_pts;
        date_Set (&owner->sync.date, start_date);
    }

    mtime_t drift = start_date - block->i_pts;
    if (drift < -input_rate * 3 * AOUT_MAX_PTS_ADVANCE / INPUT_RATE_DEFAULT)
    {
        msg_Warn (aout, "buffer way too early (%"PRId64"), clearing queue",
                  drift);
        aout_OutputFlush (aout, false);
        if (owner->sync.resamp_type != AOUT_RESAMPLING_NONE)
            msg_Warn (aout, "timing screwed, stopping resampling");
        aout_StopResampling (aout);
        block->i_flags |= BLOCK_FLAG_DISCONTINUITY;
        start_date = block->i_pts;
        date_Set (&owner->sync.date, start_date);
        drift = 0;
    }
    else
    if (drift > +input_rate * 3 * AOUT_MAX_PTS_DELAY / INPUT_RATE_DEFAULT)
    {
        msg_Warn (aout, "buffer way too late (%"PRId64"), dropping buffer",
                  drift);
        goto drop;
    }

    block = aout_FiltersPlay (aout, block, input_rate);
    if (block == NULL)
    {
        atomic_fetch_add(&owner->buffers_lost, 1);
        goto out;
    }

    /* Adjust the resampler if needed.
     * We first need to calculate the output rate of this resampler. */
    if ((owner->sync.resamp_type == AOUT_RESAMPLING_NONE)
     && (drift < -AOUT_MAX_PTS_ADVANCE || drift > +AOUT_MAX_PTS_DELAY))
    {   /* Can happen in several circumstances :
         * 1. A problem at the input (clock drift)
         * 2. A small pause triggered by the user
         * 3. Some delay in the output stage, causing a loss of lip
         *    synchronization
         * Solution : resample the buffer to avoid a scratch.
         */
        owner->sync.resamp_start_drift = (int)-drift;
        owner->sync.resamp_type = (drift < 0) ? AOUT_RESAMPLING_DOWN
                                             : AOUT_RESAMPLING_UP;
        msg_Warn (aout, (drift < 0)
                  ? "buffer too early (%"PRId64"), down-sampling"
                  : "buffer too late  (%"PRId64"), up-sampling", drift);
    }
    if (owner->sync.resamp_type != AOUT_RESAMPLING_NONE)
    {   /* Resampling has been triggered previously (because of dates
         * mismatch). We want the resampling to happen progressively so
         * it isn't too audible to the listener. */
        const int adjust = (owner->sync.resamp_type == AOUT_RESAMPLING_UP)
            ? +2 : -2;
        /* Check if everything is back to normal, then stop resampling. */
        if (!aout_FiltersAdjustResampling (aout, adjust))
        {
            owner->sync.resamp_type = AOUT_RESAMPLING_NONE;
            msg_Warn (aout, "resampling stopped (drift: %"PRIi64")",
                      block->i_pts - start_date);
        }
        else if (abs ((int)(block->i_pts - start_date))
                                    < abs (owner->sync.resamp_start_drift) / 2)
        {   /* If we reduced the drift from half, then it is time to switch
             * back the resampling direction. */
            if (owner->sync.resamp_type == AOUT_RESAMPLING_UP)
                owner->sync.resamp_type = AOUT_RESAMPLING_DOWN;
            else
                owner->sync.resamp_type = AOUT_RESAMPLING_UP;
            owner->sync.resamp_start_drift = 0;
        }
        else if (owner->sync.resamp_start_drift
              && (abs ((int)(block->i_pts - start_date))
                               > abs (owner->sync.resamp_start_drift) * 3 / 2))
        {   /* If the drift is increasing and not decreasing, than something
             * is bad. We'd better stop the resampling right now. */
            msg_Warn (aout, "timing screwed, stopping resampling");
            aout_StopResampling (aout);
            block->i_flags |= BLOCK_FLAG_DISCONTINUITY;
        }
    }

    block->i_pts = start_date;
    date_Increment (&owner->sync.date, block->i_nb_samples);

    /* Software volume */
    aout_volume_Amplify (owner->volume, block);

    /* Output */
    aout_OutputPlay (aout, block);
out:
    aout_unlock (aout);
    return 0;
drop:
    block_Release (block);
    atomic_fetch_add(&owner->buffers_lost, 1);
    goto out;
}

int aout_DecGetResetLost (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);
    return atomic_exchange(&owner->buffers_lost, 0);
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
