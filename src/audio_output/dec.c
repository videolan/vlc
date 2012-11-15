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

    /* TODO: reduce lock scope depending on decoder's real need */
    aout_lock( p_aout );

    var_Destroy( p_aout, "stereo-mode" );

    /* Create the audio output stream */
    owner->volume = aout_volume_New (p_aout, p_replay_gain);

    vlc_atomic_set (&owner->restart, 0);
    owner->input_format = *p_format;
    owner->mixer_format = owner->input_format;

    if (aout_OutputNew (p_aout, &owner->mixer_format))
        goto error;
    aout_volume_SetFormat (owner->volume, owner->mixer_format.i_format);

    /* Create the audio filtering "input" pipeline */
    if (aout_FiltersNew (p_aout, p_format, &owner->mixer_format,
                         p_request_vout))
    {
        aout_OutputDelete (p_aout);
error:
        aout_volume_Delete (owner->volume);
        aout_unlock (p_aout);
        return -1;
    }

    owner->sync.end = VLC_TS_INVALID;
    owner->sync.resamp_type = AOUT_RESAMPLING_NONE;
    owner->sync.discontinuity = true;
    aout_unlock( p_aout );

    atomic_init (&owner->buffers_lost, 0);
    return 0;
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
static int aout_CheckReady (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);

    aout_assert_locked (aout);

    int restart = vlc_atomic_swap (&owner->restart, 0);
    if (unlikely(restart))
    {
        assert (restart & AOUT_RESTART_INPUT);

        const aout_request_vout_t request_vout = owner->request_vout;

        aout_FiltersDelete (aout);
        if (restart & AOUT_RESTART_OUTPUT)
        {   /* Reinitializes the output */
            aout_OutputDelete (aout);
            owner->mixer_format = owner->input_format;
            if (aout_OutputNew (aout, &owner->mixer_format))
                owner->mixer_format.i_format = 0;
            aout_volume_SetFormat (owner->volume,
                                   owner->mixer_format.i_format);
        }

        owner->sync.end = VLC_TS_INVALID;
        owner->sync.resamp_type = AOUT_RESAMPLING_NONE;

        if (aout_FiltersNew (aout, &owner->input_format, &owner->mixer_format,
                             &request_vout))
        {
            aout_OutputDelete (aout);
            owner->mixer_format.i_format = 0;
        }
    }
    return (owner->mixer_format.i_format) ? 0 : -1;
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

static void aout_DecSilence (audio_output_t *aout, mtime_t length, mtime_t pts)
{
    aout_owner_t *owner = aout_owner (aout);
    const audio_sample_format_t *fmt = &owner->mixer_format;
    size_t frames = (fmt->i_rate * length) / CLOCK_FREQ;
    block_t *block;

    if (AOUT_FMT_SPDIF(fmt))
        block = block_Alloc (4 * frames);
    else
        block = block_Alloc (frames * fmt->i_bytes_per_frame);
    if (unlikely(block == NULL))
        return; /* uho! */

    msg_Dbg (aout, "inserting %zu zeroes", frames);
    memset (block->p_buffer, 0, block->i_buffer);
    block->i_nb_samples = frames;
    block->i_pts = pts;
    block->i_dts = pts;
    block->i_length = length;
    aout_OutputPlay (aout, block);
}

static void aout_DecSynchronize (audio_output_t *aout, mtime_t dec_pts,
                                 int input_rate)
{
    aout_owner_t *owner = aout_owner (aout);
    mtime_t aout_pts, drift;

    /**
     * Depending on the drift between the actual and intended playback times,
     * the audio core may ignore the drift, trigger upsampling or downsampling,
     * insert silence or even discard samples.
     * Future VLC versions may instead adjust the input rate.
     *
     * The audio output plugin is responsible for estimating its actual
     * playback time, or rather the estimated time when the next sample will
     * be played. (The actual playback time is always the current time, that is
     * to say mdate(). It is not an useful statistic.)
     *
     * Most audio output plugins can estimate the delay until playback of
     * the next sample to be written to the buffer, or equally the time until
     * all samples in the buffer will have been played. Then:
     *    pts = mdate() + delay
     */
    if (aout_OutputTimeGet (aout, &aout_pts) != 0)
        return; /* nothing can be done if timing is unknown */
    drift = aout_pts - dec_pts;

    /* Late audio output.
     * This can happen due to insufficient caching, scheduling jitter
     * or bug in the decoder. Ideally, the output would seek backward. But that
     * is not portable, not supported by some hardware and often unsafe/buggy
     * where supported. The other alternative is to flush the buffers
     * completely. */
    if (drift > (owner->sync.discontinuity ? 0
                  : +3 * input_rate * AOUT_MAX_PTS_DELAY / INPUT_RATE_DEFAULT))
    {
        if (!owner->sync.discontinuity)
            msg_Warn (aout, "playback way too late (%"PRId64"): "
                      "flushing buffers", drift);
        else
            msg_Dbg (aout, "playback too late (%"PRId64"): "
                     "flushing buffers", drift);
        aout_OutputFlush (aout, false);

        aout_StopResampling (aout);
        owner->sync.end = VLC_TS_INVALID;
        owner->sync.discontinuity = true;

        /* Now the output might be too early... Recheck. */
        if (aout_OutputTimeGet (aout, &aout_pts) != 0)
            return; /* nothing can be done if timing is unknown */
        drift = aout_pts - dec_pts;
    }

    /* Early audio output.
     * This is rare except at startup when the buffers are still empty. */
    if (drift < (owner->sync.discontinuity ? 0
                : -3 * input_rate * AOUT_MAX_PTS_ADVANCE / INPUT_RATE_DEFAULT))
    {
        if (!owner->sync.discontinuity)
            msg_Warn (aout, "playback way too early (%"PRId64"): "
                      "playing silence", drift);
        aout_DecSilence (aout, -drift, dec_pts);

        aout_StopResampling (aout);
        owner->sync.discontinuity = true;
        drift = 0;
    }

    /* Resampling */
    if (drift > +AOUT_MAX_PTS_DELAY
     && owner->sync.resamp_type != AOUT_RESAMPLING_UP)
    {
        msg_Warn (aout, "playback too late (%"PRId64"): up-sampling",
                  drift);
        owner->sync.resamp_type = AOUT_RESAMPLING_UP;
        owner->sync.resamp_start_drift = +drift;
    }
    if (drift < -AOUT_MAX_PTS_ADVANCE
     && owner->sync.resamp_type != AOUT_RESAMPLING_DOWN)
    {
        msg_Warn (aout, "playback too early (%"PRId64"): down-sampling",
                  drift);
        owner->sync.resamp_type = AOUT_RESAMPLING_DOWN;
        owner->sync.resamp_start_drift = -drift;
    }

    if (owner->sync.resamp_type == AOUT_RESAMPLING_NONE)
        return; /* Everything is fine. Nothing to do. */

    if (llabs (drift) > 2 * owner->sync.resamp_start_drift)
    {   /* If the drift is ever increasing, then something is seriously wrong.
         * Cease resampling and hope for the best. */
        msg_Warn (aout, "timing screwed (drift: %"PRId64" us): "
                  "stopping resampling", drift);
        aout_StopResampling (aout);
        return;
    }

    /* Resampling has been triggered earlier. This checks if it needs to be
     * increased or decreased. Resampling rate changes must be kept slow for
     * the comfort of listeners. */
    int adj = (owner->sync.resamp_type == AOUT_RESAMPLING_UP) ? +2 : -2;

    if (2 * llabs (drift) <= owner->sync.resamp_start_drift)
        /* If the drift has been reduced from more than half its initial
         * value, then it is time to switch back the resampling direction. */
        adj *= -1;

    if (!aout_FiltersAdjustResampling (aout, adj))
    {   /* Everything is back to normal: stop resampling. */
        owner->sync.resamp_type = AOUT_RESAMPLING_NONE;
        msg_Dbg (aout, "resampling stopped (drift: %"PRId64" us)", drift);
    }
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
    if (unlikely(aout_CheckReady (aout)))
        goto drop; /* Pipeline is unrecoverably broken :-( */

    const mtime_t now = mdate (), advance = block->i_pts - now;
    if (advance < -AOUT_MAX_PTS_DELAY)
    {   /* Late buffer can be caused by bugs in the decoder, by scheduling
         * latency spikes (excessive load, SIGSTOP, etc.) or if buffering is
         * insufficient. We assume the PTS is wrong and play the buffer anyway:
         * Hopefully video has encountered a similar PTS problem as audio. */
        msg_Warn (aout, "buffer too late (%"PRId64" us): dropped", advance);
        goto drop;
    }
    if (advance > AOUT_MAX_ADVANCE_TIME)
    {   /* Early buffers can only be caused by bugs in the decoder. */
        msg_Err (aout, "buffer too early (%"PRId64" us): dropped", advance);
        goto drop;
    }
    if (block->i_flags & BLOCK_FLAG_DISCONTINUITY)
        owner->sync.discontinuity = true;

    block = aout_FiltersPlay (aout, block, input_rate);
    if (block == NULL)
        goto lost;

    /* Software volume */
    aout_volume_Amplify (owner->volume, block);

    /* Drift correction */
    aout_DecSynchronize (aout, block->i_pts, input_rate);

    /* Output */
    owner->sync.end = block->i_pts + block->i_length + 1;
    owner->sync.discontinuity = false;
    aout_OutputPlay (aout, block);
out:
    aout_unlock (aout);
    return 0;
drop:
    owner->sync.discontinuity = true;
    block_Release (block);
lost:
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
    if (owner->sync.end != VLC_TS_INVALID)
    {
        if (paused)
            owner->sync.end -= date;
        else
            owner->sync.end += date;
    }
    aout_OutputPause (aout, paused, date);
    aout_unlock (aout);
}

void aout_DecFlush (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);

    aout_lock (aout);
    owner->sync.end = VLC_TS_INVALID;
    aout_OutputFlush (aout, false);
    aout_unlock (aout);
}

bool aout_DecIsEmpty (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);
    mtime_t now = mdate ();
    bool empty = true;

    aout_lock (aout);
    if (owner->sync.end != VLC_TS_INVALID)
        empty = owner->sync.end <= now;
    if (empty)
        /* The last PTS has elapsed already. So the underlying audio output
         * buffer should be empty or almost. Thus draining should be fast
         * and will not block the caller too long. */
        aout_OutputFlush (aout, true);
    aout_unlock (aout);
    return empty;
}
