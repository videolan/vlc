/*****************************************************************************
 * dec.c : audio output API towards decoders
 *****************************************************************************
 * Copyright (C) 2002-2019 VLC authors, VideoLAN and Videolabs SAS
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

#include <math.h>

#include <vlc_common.h>
#include <vlc_aout.h>

#include "aout_internal.h"
#include "clock/clock.h"
#include "libvlc.h"

static void aout_Drain(audio_output_t *aout)
{
    if (aout->drain)
        aout->drain(aout);
    else
    {
        vlc_tick_t delay;
        if (aout->time_get(aout, &delay) == 0)
            vlc_tick_sleep(delay);
    }
}

/**
 * Creates an audio output
 */
int aout_DecNew(audio_output_t *p_aout, const audio_sample_format_t *p_format,
                int profile, vlc_clock_t *clock,
                const audio_replay_gain_t *p_replay_gain)
{
    assert(p_aout);
    assert(p_format);
    assert(clock);
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

    /* Create the audio output stream */
    if (!owner->bitexact)
        owner->volume = aout_volume_New (p_aout, p_replay_gain);

    atomic_store_explicit(&owner->restart, 0, memory_order_relaxed);
    owner->input_profile = profile;
    owner->filter_format = owner->mixer_format = owner->input_format = *p_format;

    owner->sync.clock = clock;

    owner->filters = NULL;
    owner->filters_cfg = AOUT_FILTERS_CFG_INIT;
    if (aout_OutputNew (p_aout))
        goto error;
    aout_volume_SetFormat (owner->volume, owner->mixer_format.i_format);

    if (!owner->bitexact)
    {
        /* Create the audio filtering "input" pipeline */
        owner->filters = aout_FiltersNewWithClock(VLC_OBJECT(p_aout), clock,
                                                  &owner->filter_format,
                                                  &owner->mixer_format,
                                                  &owner->filters_cfg);
        if (owner->filters == NULL)
        {
            aout_OutputDelete (p_aout);
error:
            aout_volume_Delete (owner->volume);
            owner->volume = NULL;
            return -1;
        }
    }

    owner->sync.rate = 1.f;
    owner->sync.resamp_type = AOUT_RESAMPLING_NONE;
    owner->sync.discontinuity = true;
    owner->original_pts = VLC_TICK_INVALID;
    owner->sync.delay = owner->sync.request_delay = 0;

    atomic_init (&owner->buffers_lost, 0);
    atomic_init (&owner->buffers_played, 0);
    atomic_store_explicit(&owner->vp.update, true, memory_order_relaxed);
    return 0;
}

/**
 * Stops all plugins involved in the audio output.
 */
void aout_DecDelete (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);

    if (owner->mixer_format.i_format)
    {
        aout_DecFlush(aout);
        if (owner->filters)
            aout_FiltersDelete (aout, owner->filters);
        aout_OutputDelete (aout);
    }
    aout_volume_Delete (owner->volume);
    owner->volume = NULL;
}

static int aout_CheckReady (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);
    int status = AOUT_DEC_SUCCESS;

    int restart = atomic_exchange_explicit(&owner->restart, 0,
                                           memory_order_acquire);
    if (unlikely(restart))
    {
        if (owner->filters)
        {
            aout_FiltersDelete (aout, owner->filters);
            owner->filters = NULL;
        }

        if (restart & AOUT_RESTART_OUTPUT)
        {   /* Reinitializes the output */
            msg_Dbg (aout, "restarting output...");
            if (owner->mixer_format.i_format)
                aout_OutputDelete (aout);
            owner->filter_format = owner->mixer_format = owner->input_format;
            owner->filters_cfg = AOUT_FILTERS_CFG_INIT;
            if (aout_OutputNew (aout))
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
        owner->sync.resamp_type = AOUT_RESAMPLING_NONE;

        if (owner->mixer_format.i_format && !owner->bitexact)
        {
            owner->filters = aout_FiltersNewWithClock(VLC_OBJECT(aout),
                                                      owner->sync.clock,
                                                      &owner->filter_format,
                                                      &owner->mixer_format,
                                                      &owner->filters_cfg);
            if (owner->filters == NULL)
            {
                aout_OutputDelete (aout);
                owner->mixer_format.i_format = 0;
            }
            aout_FiltersSetClockDelay(owner->filters, owner->sync.delay);
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
    atomic_fetch_or_explicit(&owner->restart, mode, memory_order_release);
    msg_Dbg (aout, "restart requested (%u)", mode);
}

/*
 * Buffer management
 */

static void aout_StopResampling (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);
    assert(owner->filters);

    owner->sync.resamp_type = AOUT_RESAMPLING_NONE;
    aout_FiltersAdjustResampling (owner->filters, 0);
}

static void aout_DecSynchronize(audio_output_t *aout, vlc_tick_t system_now,
                                vlc_tick_t dec_pts);
static void aout_DecSilence (audio_output_t *aout, vlc_tick_t length, vlc_tick_t pts)
{
    aout_owner_t *owner = aout_owner (aout);
    const audio_sample_format_t *fmt = &owner->mixer_format;
    size_t frames = samples_from_vlc_tick(length, fmt->i_rate);

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

    const vlc_tick_t system_now = vlc_tick_now();
    const vlc_tick_t system_pts =
       vlc_clock_ConvertToSystem(owner->sync.clock, system_now, pts,
                                 owner->sync.rate);
    aout->play(aout, block, system_pts);
}

static void aout_DecSynchronize(audio_output_t *aout, vlc_tick_t system_now,
                                vlc_tick_t dec_pts)
{
    /**
     * Depending on the drift between the actual and intended playback times,
     * the audio core may ignore the drift, trigger upsampling or downsampling,
     * insert silence or even discard samples.
     * Future VLC versions may instead adjust the input rate.
     *
     * The audio output plugin is responsible for estimating its actual
     * playback time, or rather the estimated time when the next sample will
     * be played. (The actual playback time is always the current time, that is
     * to say vlc_tick_now(). It is not an useful statistic.)
     *
     * Most audio output plugins can estimate the delay until playback of
     * the next sample to be written to the buffer, or equally the time until
     * all samples in the buffer will have been played. Then:
     *    pts = vlc_tick_now() + delay
     */
    aout_owner_t *owner = aout_owner (aout);
    vlc_tick_t delay;

    if (aout->time_get(aout, &delay) != 0)
        return; /* nothing can be done if timing is unknown */

    if (owner->sync.discontinuity)
    {
        /* Chicken-egg situation for most aout modules that can't be started
         * deferred (all except PulseAudio). These modules will start to play
         * data immediately and ignore the given play_date (that take the clock
         * jitter into account). We don't want to let aout_RequestRetiming()
         * handle the first silence (from the "Early audio output" case) since
         * this function will first update the clock without taking the jitter
         * into account. Therefore, we manually insert silence that correspond
         * to the clock jitter value before updating the clock.
         */
        vlc_tick_t play_date =
            vlc_clock_ConvertToSystem(owner->sync.clock, system_now + delay,
                                      dec_pts, owner->sync.rate);
        vlc_tick_t jitter = play_date - system_now;
        if (jitter > 0)
        {
            aout_DecSilence (aout, jitter, dec_pts - delay);
            if (aout->time_get(aout, &delay) != 0)
                return;
        }
    }

    aout_RequestRetiming(aout, system_now + delay, dec_pts);
}

void aout_RequestRetiming(audio_output_t *aout, vlc_tick_t system_ts,
                          vlc_tick_t audio_ts)
{
    aout_owner_t *owner = aout_owner (aout);
    float rate = owner->sync.rate;
    vlc_tick_t drift =
        -vlc_clock_Update(owner->sync.clock, system_ts, audio_ts, rate);

    if (unlikely(drift == INT64_MAX) || owner->bitexact)
        return; /* cf. INT64_MAX comment in aout_DecPlay() */

    /* Late audio output.
     * This can happen due to insufficient caching, scheduling jitter
     * or bug in the decoder. Ideally, the output would seek backward. But that
     * is not portable, not supported by some hardware and often unsafe/buggy
     * where supported. The other alternative is to flush the buffers
     * completely. */
    if (drift > (owner->sync.discontinuity ? 0
                : lroundf(+3 * AOUT_MAX_PTS_DELAY / rate)))
    {
        if (!owner->sync.discontinuity)
            msg_Warn (aout, "playback way too late (%"PRId64"): "
                      "flushing buffers", drift);
        else
            msg_Dbg (aout, "playback too late (%"PRId64"): "
                     "flushing buffers", drift);
        aout_DecFlush(aout);
        aout_StopResampling (aout);

        return; /* nothing can be done if timing is unknown */
    }

    /* Early audio output.
     * This is rare except at startup when the buffers are still empty. */
    if (drift < (owner->sync.discontinuity ? 0
                : lroundf(-3 * AOUT_MAX_PTS_ADVANCE / rate)))
    {
        if (!owner->sync.discontinuity)
            msg_Warn (aout, "playback way too early (%"PRId64"): "
                      "playing silence", drift);
        aout_DecSilence (aout, -drift, audio_ts);

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
int aout_DecPlay(audio_output_t *aout, block_t *block)
{
    aout_owner_t *owner = aout_owner (aout);

    assert (block->i_pts != VLC_TICK_INVALID);

    block->i_length = vlc_tick_from_samples( block->i_nb_samples,
                                   owner->input_format.i_rate );

    int ret = aout_CheckReady (aout);
    if (unlikely(ret == AOUT_DEC_FAILED))
        goto drop; /* Pipeline is unrecoverably broken :-( */

    if (block->i_flags & BLOCK_FLAG_DISCONTINUITY)
    {
        owner->sync.discontinuity = true;
        owner->original_pts = VLC_TICK_INVALID;
    }

    if (owner->original_pts == VLC_TICK_INVALID)
    {
        /* Use the original PTS for synchronization and as a play date of the
         * aout module. This PTS need to be saved here in order to use the PTS
         * of the first block that has been filtered. Indeed, aout filters may
         * need more than one block to output a new one. */
        owner->original_pts = block->i_pts;
    }

    if (owner->filters)
    {
        if (atomic_load_explicit(&owner->vp.update, memory_order_relaxed))
        {
            vlc_mutex_lock (&owner->vp.lock);
            aout_FiltersChangeViewpoint (owner->filters, &owner->vp.value);
            atomic_store_explicit(&owner->vp.update, false, memory_order_relaxed);
            vlc_mutex_unlock (&owner->vp.lock);
        }

        block = aout_FiltersPlay(owner->filters, block, owner->sync.rate);
        if (block == NULL)
            return ret;
    }

    const vlc_tick_t original_pts = owner->original_pts;
    owner->original_pts = VLC_TICK_INVALID;

    /* Software volume */
    aout_volume_Amplify (owner->volume, block);

    /* Update delay */
    if (owner->sync.request_delay != owner->sync.delay)
    {
        owner->sync.delay = owner->sync.request_delay;
        vlc_tick_t delta = vlc_clock_SetDelay(owner->sync.clock, owner->sync.delay);
        if (owner->filters)
            aout_FiltersSetClockDelay(owner->filters, owner->sync.delay);
        if (delta > 0)
            aout_DecSilence (aout, delta, block->i_pts);
    }

    /* Drift correction */
    vlc_tick_t system_now = vlc_tick_now();
    aout_DecSynchronize(aout, system_now, original_pts);

    vlc_tick_t play_date =
        vlc_clock_ConvertToSystem(owner->sync.clock, system_now, original_pts,
                                  owner->sync.rate);
    if (unlikely(play_date == INT64_MAX))
    {
        /* The clock is paused but not the output, play the audio anyway since
         * we can't delay audio playback from here. */
        play_date = system_now;

    }
    /* Output */
    owner->sync.discontinuity = false;
    aout->play(aout, block, play_date);

    atomic_fetch_add_explicit(&owner->buffers_played, 1, memory_order_relaxed);
    return ret;
drop:
    owner->sync.discontinuity = true;
    owner->original_pts = VLC_TICK_INVALID;
    block_Release (block);
    atomic_fetch_add_explicit(&owner->buffers_lost, 1, memory_order_relaxed);
    return ret;
}

void aout_DecGetResetStats(audio_output_t *aout, unsigned *restrict lost,
                           unsigned *restrict played)
{
    aout_owner_t *owner = aout_owner (aout);

    *lost = atomic_exchange_explicit(&owner->buffers_lost, 0,
                                     memory_order_relaxed);
    *played = atomic_exchange_explicit(&owner->buffers_played, 0,
                                       memory_order_relaxed);
}

void aout_DecChangePause (audio_output_t *aout, bool paused, vlc_tick_t date)
{
    aout_owner_t *owner = aout_owner (aout);

    if (owner->mixer_format.i_format)
    {
        if (aout->pause != NULL)
            aout->pause(aout, paused, date);
        else if (paused)
            aout->flush(aout);
    }
}

void aout_DecChangeRate(audio_output_t *aout, float rate)
{
    aout_owner_t *owner = aout_owner(aout);

    owner->sync.rate = rate;
}

void aout_DecChangeDelay(audio_output_t *aout, vlc_tick_t delay)
{
    aout_owner_t *owner = aout_owner(aout);

    owner->sync.request_delay = delay;
}

void aout_DecFlush(audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);

    if (owner->mixer_format.i_format)
    {
        if (owner->filters)
            aout_FiltersFlush (owner->filters);

        aout->flush(aout);
        vlc_clock_Reset(owner->sync.clock);
        if (owner->filters)
            aout_FiltersResetClock(owner->filters);

        if (owner->sync.delay > 0)
        {
            /* Also reset the delay in case of a positive delay. This will
             * trigger a silence playback before the next play. Consequently,
             * the first play date won't be (delay + dejitter) but only
             * dejitter. This will allow the aout to update the master clock
             * sooner.
             */
            vlc_clock_SetDelay(owner->sync.clock, 0);
            if (owner->filters)
                aout_FiltersSetClockDelay(owner->filters, 0);
            owner->sync.request_delay = owner->sync.delay;
            owner->sync.delay = 0;
        }
    }
    owner->sync.discontinuity = true;
    owner->original_pts = VLC_TICK_INVALID;
}

void aout_DecDrain(audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);

    if (!owner->mixer_format.i_format)
        return;

    if (owner->filters)
    {
        block_t *block = aout_FiltersDrain (owner->filters);
        if (block)
            aout->play(aout, block, vlc_tick_now());
    }

    aout_Drain(aout);

    vlc_clock_Reset(owner->sync.clock);
    if (owner->filters)
        aout_FiltersResetClock(owner->filters);

    owner->sync.discontinuity = true;
    owner->original_pts = VLC_TICK_INVALID;
}
