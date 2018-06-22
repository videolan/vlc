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
    if( p_format->i_bitspersample > 0 )
    {
        /* Sanitize audio format, input need to have a valid physical channels
         * layout or a valid number of channels. */
        int i_map_channels = aout_FormatNbChannels( p_format );
        if( ( i_map_channels == 0 && p_format->i_channels == 0 )
           || i_map_channels > AOUT_CHAN_MAX || p_format->i_channels > INPUT_CHAN_MAX )
        {
            msg_Err( p_aout, "invalid audio channels count" );
            return -1;
        }
    }

    if( p_format->i_rate > 384000 )
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
    aout_OutputLock (p_aout);

    /* Create the audio output stream */
    owner->volume = aout_volume_New (p_aout, p_replay_gain);

    atomic_store (&owner->restart, 0);
    owner->input_format = *p_format;
    owner->mixer_format = owner->input_format;
    owner->request_vout = *p_request_vout;

    var_Change (p_aout, "stereo-mode", VLC_VAR_SETVALUE,
                &(vlc_value_t) { .i_int = owner->initial_stereo_mode }, NULL);

    owner->filters_cfg = AOUT_FILTERS_CFG_INIT;
    if (aout_OutputNew (p_aout, &owner->mixer_format, &owner->filters_cfg))
        goto error;
    aout_volume_SetFormat (owner->volume, owner->mixer_format.i_format);

    /* Create the audio filtering "input" pipeline */
    owner->filters = aout_FiltersNew (p_aout, p_format, &owner->mixer_format,
                                      &owner->request_vout,
                                      &owner->filters_cfg);
    if (owner->filters == NULL)
    {
        aout_OutputDelete (p_aout);
error:
        aout_volume_Delete (owner->volume);
        owner->volume = NULL;
        aout_OutputUnlock (p_aout);
        return -1;
    }


    owner->sync.end = VLC_TS_INVALID;
    owner->sync.resamp_type = AOUT_RESAMPLING_NONE;
    owner->sync.discontinuity = true;
    aout_OutputUnlock (p_aout);

    atomic_init (&owner->buffers_lost, 0);
    atomic_init (&owner->buffers_played, 0);
    atomic_store (&owner->vp.update, true);
    return 0;
}

/**
 * Stops all plugins involved in the audio output.
 */
void aout_DecDelete (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);

    aout_OutputLock (aout);
    if (owner->mixer_format.i_format)
    {
        aout_FiltersDelete (aout, owner->filters);
        aout_OutputDelete (aout);
    }
    aout_volume_Delete (owner->volume);
    owner->volume = NULL;
    aout_OutputUnlock (aout);
}

static int aout_CheckReady (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);

    int status = AOUT_DEC_SUCCESS;
    int restart = atomic_exchange (&owner->restart, 0);
    if (unlikely(restart))
    {
        if (owner->mixer_format.i_format)
            aout_FiltersDelete (aout, owner->filters);

        if (restart & AOUT_RESTART_OUTPUT)
        {   /* Reinitializes the output */
            msg_Dbg (aout, "restarting output...");
            if (owner->mixer_format.i_format)
                aout_OutputDelete (aout);
            owner->mixer_format = owner->input_format;
            owner->filters_cfg = AOUT_FILTERS_CFG_INIT;
            if (aout_OutputNew (aout, &owner->mixer_format, &owner->filters_cfg))
                owner->mixer_format.i_format = 0;
            aout_volume_SetFormat (owner->volume,
                                   owner->mixer_format.i_format);

            /* Notify the decoder that the aout changed in order to try a new
             * suitable codec (like an HDMI audio format). However, keep the
             * same codec if the aout was restarted because of a stereo-mode
             * change from the user. */
            if (restart == AOUT_RESTART_OUTPUT)
                status = AOUT_DEC_CHANGED;
        }

        msg_Dbg (aout, "restarting filters...");
        owner->sync.end = VLC_TS_INVALID;
        owner->sync.resamp_type = AOUT_RESAMPLING_NONE;

        if (owner->mixer_format.i_format)
        {
            owner->filters = aout_FiltersNew (aout, &owner->input_format,
                                              &owner->mixer_format,
                                              &owner->request_vout,
                                              &owner->filters_cfg);
            if (owner->filters == NULL)
            {
                aout_OutputDelete (aout);
                owner->mixer_format.i_format = 0;
            }
        }
        /* TODO: This would be a good time to call clean up any video output
         * left over by an audio visualization:
        input_resource_TerminatVout(MAGIC HERE); */
    }
    return (owner->mixer_format.i_format) ? status : AOUT_DEC_FAILED;
}

/**
 * Marks the audio output for restart, to update any parameter of the output
 * plug-in (e.g. output device or channel mapping).
 */
void aout_RequestRestart (audio_output_t *aout, unsigned mode)
{
    aout_owner_t *owner = aout_owner (aout);
    atomic_fetch_or (&owner->restart, mode);
    msg_Dbg (aout, "restart requested (%u)", mode);
}

/*
 * Buffer management
 */

static void aout_StopResampling (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);

    owner->sync.resamp_type = AOUT_RESAMPLING_NONE;
    aout_FiltersAdjustResampling (owner->filters, 0);
}

static void aout_DecSilence (audio_output_t *aout, vlc_tick_t length, vlc_tick_t pts)
{
    aout_owner_t *owner = aout_owner (aout);
    const audio_sample_format_t *fmt = &owner->mixer_format;
    size_t frames = (fmt->i_rate * length) / CLOCK_FREQ;

    block_t *block = block_Alloc (frames * fmt->i_bytes_per_frame
                                  / fmt->i_frame_length);
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

static void aout_DecSynchronize (audio_output_t *aout, vlc_tick_t dec_pts,
                                 int input_rate)
{
    aout_owner_t *owner = aout_owner (aout);
    vlc_tick_t drift;

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
    if (aout_OutputTimeGet (aout, &drift) != 0)
        return; /* nothing can be done if timing is unknown */
    drift += mdate () - dec_pts;

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
        if (aout_OutputTimeGet (aout, &drift) != 0)
            return; /* nothing can be done if timing is unknown */
        drift += mdate () - dec_pts;
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

    if (!aout_FiltersCanResample(owner->filters))
        return;

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

    if (!aout_FiltersAdjustResampling (owner->filters, adj))
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

    aout_OutputLock (aout);
    int ret = aout_CheckReady (aout);
    if (unlikely(ret == AOUT_DEC_FAILED))
        goto drop; /* Pipeline is unrecoverably broken :-( */

    const vlc_tick_t now = mdate (), advance = block->i_pts - now;
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

    if (atomic_exchange(&owner->vp.update, false))
    {
        vlc_mutex_lock (&owner->vp.lock);
        aout_FiltersChangeViewpoint (owner->filters, &owner->vp.value);
        vlc_mutex_unlock (&owner->vp.lock);
    }

    block = aout_FiltersPlay (owner->filters, block, input_rate);
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
    atomic_fetch_add(&owner->buffers_played, 1);
out:
    aout_OutputUnlock (aout);
    return ret;
drop:
    owner->sync.discontinuity = true;
    block_Release (block);
lost:
    atomic_fetch_add(&owner->buffers_lost, 1);
    goto out;
}

void aout_DecGetResetStats(audio_output_t *aout, unsigned *restrict lost,
                           unsigned *restrict played)
{
    aout_owner_t *owner = aout_owner (aout);

    *lost = atomic_exchange(&owner->buffers_lost, 0);
    *played = atomic_exchange(&owner->buffers_played, 0);
}

void aout_DecChangePause (audio_output_t *aout, bool paused, vlc_tick_t date)
{
    aout_owner_t *owner = aout_owner (aout);

    aout_OutputLock (aout);
    if (owner->sync.end != VLC_TS_INVALID)
    {
        if (paused)
            owner->sync.end -= date;
        else
            owner->sync.end += date;
    }
    if (owner->mixer_format.i_format)
        aout_OutputPause (aout, paused, date);
    aout_OutputUnlock (aout);
}

void aout_DecFlush (audio_output_t *aout, bool wait)
{
    aout_owner_t *owner = aout_owner (aout);

    aout_OutputLock (aout);
    owner->sync.end = VLC_TS_INVALID;
    if (owner->mixer_format.i_format)
    {
        if (wait)
        {
            block_t *block = aout_FiltersDrain (owner->filters);
            if (block)
                aout_OutputPlay (aout, block);
        }
        else
            aout_FiltersFlush (owner->filters);
        aout_OutputFlush (aout, wait);
    }
    aout_OutputUnlock (aout);
}

void aout_ChangeViewpoint(audio_output_t *aout,
                          const vlc_viewpoint_t *p_viewpoint)
{
    aout_owner_t *owner = aout_owner (aout);

    vlc_mutex_lock (&owner->vp.lock);
    owner->vp.value = *p_viewpoint;
    atomic_store(&owner->vp.update, true);
    vlc_mutex_unlock (&owner->vp.lock);
}
