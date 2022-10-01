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
#include <vlc_tracer.h>

#include "aout_internal.h"
#include "clock/clock.h"
#include "libvlc.h"

#define MAX_TIMING_POINT 16
struct timing_point
{
    vlc_tick_t system_ts;
    vlc_tick_t audio_ts;
};

struct vlc_aout_stream
{
    aout_instance_t *instance;
    aout_volume_t *volume;
    aout_filters_t *filters;
    aout_filters_cfg_t filters_cfg;

    atomic_bool drained;
    _Atomic vlc_tick_t drain_deadline;

    struct
    {
        struct vlc_clock_t *clock;
        float rate; /**< Play-out speed rate */
        vlc_tick_t resamp_start_drift; /**< Resampler drift absolute value */
        int resamp_type; /**< Resampler mode (FIXME: redundant / resampling) */
        bool discontinuity;
        vlc_tick_t request_delay;
        vlc_tick_t delay;
    } sync;
    vlc_tick_t original_pts;

    struct
    {
        vlc_mutex_t lock; /* Guard data, write and read */
        struct timing_point data[MAX_TIMING_POINT];
        size_t write, read; /* Circular buffer */

        struct timing_point last;
        void (*notify_latency_cb)(void *data);
        void *notify_latency_data;
    } timing_points;

    const char *str_id;

    /* Original input format and profile, won't change for the lifetime of a
     * stream (between vlc_aout_stream_New() and vlc_aout_stream_Delete()). */
    int                   input_profile;
    audio_sample_format_t input_format;

    /* Format used to configure the conversion filters. It is based on the
     * input_format but its fourcc can be different when the module is handling
     * codec passthrough. Indeed, in case of DTSHD->DTS or EAC3->AC3 fallback,
     * the filter need to know which codec is handled by the output. */
    audio_sample_format_t filter_format;

    /* Output format used and modified by the module. */
    audio_sample_format_t mixer_format;

    atomic_uchar restart;

    atomic_uint buffers_lost;
    atomic_uint buffers_played;
    _Atomic vlc_tick_t latency;
};

static inline aout_owner_t *aout_stream_owner(vlc_aout_stream *stream)
{
    return &stream->instance->owner;
}

static inline audio_output_t *aout_stream_aout(vlc_aout_stream *stream)
{
    return &stream->instance->output;
}

static inline struct vlc_tracer *aout_stream_tracer(vlc_aout_stream *stream)
{
    return stream->str_id == NULL ? NULL :
        vlc_object_get_tracer(VLC_OBJECT(aout_stream_aout(stream)));
}

static int stream_TimeGet(vlc_aout_stream *stream, vlc_tick_t *delay)
{
    audio_output_t *aout = aout_stream_aout(stream);

    if (aout->time_get == NULL)
    {
        /* Interpolate the last updated point. */
        const struct timing_point *p = &stream->timing_points.last;
        if (p->audio_ts == VLC_TICK_INVALID)
            return -1;

        *delay = (p->audio_ts + vlc_tick_now() - p->system_ts) * stream->sync.rate;
        return 0;
    }
    return aout->time_get(aout, delay);
}

static void stream_Discontinuity(vlc_aout_stream *stream)
{
    stream->sync.discontinuity = true;
    stream->original_pts = VLC_TICK_INVALID;

    vlc_mutex_lock(&stream->timing_points.lock);
    stream->timing_points.read = stream->timing_points.write = 0;
    vlc_mutex_unlock(&stream->timing_points.lock);

    stream->timing_points.last.audio_ts =
    stream->timing_points.last.system_ts = VLC_TICK_INVALID;
}

static void stream_Reset(vlc_aout_stream *stream)
{
    aout_owner_t *owner = aout_stream_owner(stream);

    if (stream->mixer_format.i_format)
    {
        vlc_audio_meter_Flush(&owner->meter);

        if (stream->filters)
            aout_FiltersFlush (stream->filters);

        vlc_clock_Reset(stream->sync.clock);
        if (stream->filters)
            aout_FiltersResetClock(stream->filters);

        if (stream->sync.delay > 0)
        {
            /* Also reset the delay in case of a positive delay. This will
             * trigger a silence playback before the next play. Consequently,
             * the first play date won't be (delay + dejitter) but only
             * dejitter. This will allow the aout to update the master clock
             * sooner.
             */
            vlc_clock_SetDelay(stream->sync.clock, 0);
            if (stream->filters)
                aout_FiltersSetClockDelay(stream->filters, 0);
            stream->sync.request_delay = stream->sync.delay;
            stream->sync.delay = 0;
        }
    }

    atomic_store_explicit(&stream->drained, false, memory_order_relaxed);
    atomic_store_explicit(&stream->drain_deadline, VLC_TICK_INVALID,
                          memory_order_relaxed);

    stream_Discontinuity(stream);
}

/**
 * Creates an audio output
 */
vlc_aout_stream * vlc_aout_stream_New(audio_output_t *p_aout,
                                      const struct vlc_aout_stream_cfg *cfg)
{
    assert(p_aout);
    assert(cfg->fmt);
    assert(cfg->clock);
    const audio_sample_format_t *p_format = cfg->fmt;

    if( p_format->i_bitspersample > 0 )
    {
        /* Sanitize audio format, input need to have a valid physical channels
         * layout or a valid number of channels. */
        int i_map_channels = aout_FormatNbChannels( p_format );
        if( ( i_map_channels == 0 && p_format->i_channels == 0 )
           || i_map_channels > AOUT_CHAN_MAX || p_format->i_channels > INPUT_CHAN_MAX )
        {
            msg_Err( p_aout, "invalid audio channels count" );
            return NULL;
        }
    }

    if( p_format->i_rate > 384000 )
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

    aout_owner_t *owner = aout_owner(p_aout);

    vlc_aout_stream *stream = malloc(sizeof(*stream));
    if (stream == NULL)
        return NULL;
    stream->instance = aout_instance(p_aout);

    stream->volume = NULL;
    if (!owner->bitexact)
        stream->volume = aout_volume_New (p_aout, cfg->replay_gain);

    atomic_init(&stream->restart, 0);
    stream->input_profile = cfg->profile;
    stream->filter_format = stream->mixer_format = stream->input_format = *p_format;

    stream->sync.clock = cfg->clock;
    stream->str_id = cfg->str_id;

    vlc_mutex_init(&stream->timing_points.lock);
    stream->timing_points.notify_latency_cb = cfg->notify_latency_cb;
    stream->timing_points.notify_latency_data = cfg->notify_latency_data;

    stream->sync.rate = 1.f;
    stream->sync.resamp_type = AOUT_RESAMPLING_NONE;
    stream->sync.delay = stream->sync.request_delay = 0;
    stream_Discontinuity(stream);

    atomic_init (&stream->buffers_lost, 0);
    atomic_init (&stream->buffers_played, 0);
    atomic_init (&stream->latency, 0);
    atomic_store_explicit(&owner->vp.update, true, memory_order_relaxed);

    atomic_init(&stream->drained, false);
    atomic_init(&stream->drain_deadline, VLC_TICK_INVALID);

    stream->filters = NULL;
    stream->filters_cfg = AOUT_FILTERS_CFG_INIT;
    if (aout_OutputNew(p_aout, stream, &stream->mixer_format, stream->input_profile,
                       &stream->filter_format, &stream->filters_cfg))
        goto error;
    aout_volume_SetFormat (stream->volume, stream->mixer_format.i_format);

    vlc_audio_meter_Reset(&owner->meter, &stream->mixer_format);

    if (!owner->bitexact)
    {
        /* Create the audio filtering "input" pipeline */
        stream->filters = aout_FiltersNewWithClock(VLC_OBJECT(p_aout), cfg->clock,
                                                   &stream->filter_format,
                                                   &stream->mixer_format,
                                                   &stream->filters_cfg);
        if (stream->filters == NULL)
        {
            aout_OutputDelete (p_aout);
            vlc_audio_meter_Reset(&owner->meter, NULL);

error:
            aout_volume_Delete (stream->volume);
            stream->volume = NULL;
            free(stream);
            return NULL;
        }
    }

    return stream;
}

/**
 * Stops all plugins involved in the audio output.
 */
void vlc_aout_stream_Delete (vlc_aout_stream *stream)
{
    audio_output_t *aout = aout_stream_aout(stream);
    aout_owner_t *owner = aout_stream_owner(stream);

    if (stream->mixer_format.i_format)
    {
        stream_Reset(stream);
        vlc_audio_meter_Reset(&owner->meter, NULL);
        if (stream->filters)
            aout_FiltersDelete (aout, stream->filters);
        aout_OutputDelete (aout);
    }
    aout_volume_Delete (stream->volume);
    free(stream);
}

static int stream_CheckReady (vlc_aout_stream *stream)
{
    aout_owner_t *owner = aout_stream_owner(stream);
    audio_output_t *aout = aout_stream_aout(stream);
    int status = AOUT_DEC_SUCCESS;

    int restart = atomic_exchange_explicit(&stream->restart, 0,
                                           memory_order_acquire);
    if (unlikely(restart))
    {
        if (stream->filters)
        {
            aout_FiltersDelete (aout, stream->filters);
            stream->filters = NULL;
        }

        struct vlc_tracer *tracer = aout_stream_tracer(stream);

        if (restart & AOUT_RESTART_OUTPUT)
        {   /* Reinitializes the output */
            if (tracer != NULL)
                vlc_tracer_TraceEvent(tracer, "RENDER", stream->str_id, "restart");

            msg_Dbg (aout, "restarting output...");
            if (stream->mixer_format.i_format)
                aout_OutputDelete (aout);
            stream->filter_format = stream->mixer_format = stream->input_format;
            stream->filters_cfg = AOUT_FILTERS_CFG_INIT;
            if (aout_OutputNew(aout, stream, &stream->mixer_format, stream->input_profile,
                               &stream->filter_format, &stream->filters_cfg))
                stream->mixer_format.i_format = 0;
            aout_volume_SetFormat (stream->volume,
                                   stream->mixer_format.i_format);

            /* Notify the decoder that the aout changed in order to try a new
             * suitable codec (like an HDMI audio format). However, keep the
             * same codec if the aout was restarted because of a stereo-mode
             * change from the user. */
            if (restart == AOUT_RESTART_OUTPUT)
                status = AOUT_DEC_CHANGED;
        }
        else if (tracer != NULL)
            vlc_tracer_TraceEvent(tracer, "RENDER", stream->str_id, "filters_restart");

        msg_Dbg (aout, "restarting filters...");
        stream->sync.resamp_type = AOUT_RESAMPLING_NONE;

        if (stream->mixer_format.i_format && !owner->bitexact)
        {
            stream->filters = aout_FiltersNewWithClock(VLC_OBJECT(aout),
                                                       stream->sync.clock,
                                                       &stream->filter_format,
                                                       &stream->mixer_format,
                                                       &stream->filters_cfg);
            if (stream->filters == NULL)
            {
                aout_OutputDelete (aout);
                stream->mixer_format.i_format = 0;
            }
            aout_FiltersSetClockDelay(stream->filters, stream->sync.delay);
        }

        vlc_audio_meter_Reset(&owner->meter,
                              stream->mixer_format.i_format ? &stream->mixer_format : NULL);

        /* TODO: This would be a good time to call clean up any video output
         * left over by an audio visualization:
        input_resource_TerminatVout(MAGIC HERE); */
    }
    return (stream->mixer_format.i_format) ? status : AOUT_DEC_FAILED;
}

/**
 * Marks the audio output for restart, to update any parameter of the output
 * plug-in (e.g. output device or channel mapping).
 */
void vlc_aout_stream_RequestRestart(vlc_aout_stream *stream, unsigned mode)
{
    audio_output_t *aout = aout_stream_aout(stream);
    atomic_fetch_or_explicit(&stream->restart, mode, memory_order_release);
    msg_Dbg (aout, "restart requested (%u)", mode);
}

/*
 * Buffer management
 */

static void stream_StopResampling(vlc_aout_stream *stream)
{
    assert(stream->filters);

    stream->sync.resamp_type = AOUT_RESAMPLING_NONE;
    aout_FiltersAdjustResampling (stream->filters, 0);
}

static void stream_Silence (vlc_aout_stream *stream, vlc_tick_t length, vlc_tick_t pts)
{
    audio_output_t *aout = aout_stream_aout(stream);
    const audio_sample_format_t *fmt = &stream->mixer_format;
    size_t frames = samples_from_vlc_tick(length, fmt->i_rate);

    block_t *block = block_Alloc (frames * fmt->i_bytes_per_frame
                                  / fmt->i_frame_length);
    if (unlikely(block == NULL))
        return; /* uho! */

    msg_Dbg (aout, "inserting %zu zeroes / %"PRId64"ms", frames, MS_FROM_VLC_TICK(length));
    memset (block->p_buffer, 0, block->i_buffer);
    block->i_nb_samples = frames;
    block->i_pts = pts;
    block->i_dts = pts;
    block->i_length = length;

    const vlc_tick_t system_now = vlc_tick_now();
    const vlc_tick_t system_pts =
       vlc_clock_ConvertToSystem(stream->sync.clock, system_now, pts,
                                 stream->sync.rate);
    aout->play(aout, block, system_pts);
}

static void stream_HandleDrift(vlc_aout_stream *stream, vlc_tick_t drift,
                               vlc_tick_t audio_ts)
{
    aout_owner_t *owner = aout_stream_owner(stream);
    audio_output_t *aout = aout_stream_aout(stream);

    float rate = stream->sync.rate;

    if (unlikely(drift == VLC_TICK_MAX) || owner->bitexact)
        return; /* cf. VLC_TICK_MAX comment in vlc_aout_stream_Play() */

    struct vlc_tracer *tracer = aout_stream_tracer(stream);

    /* Following calculations expect an opposite drift. Indeed,
     * vlc_clock_Update() returns a positive relative time, corresponding to
     * the time when audio_ts is expected to be played (in the future when not
     * late). */
    drift = -drift;

    /* Late audio output.
     * This can happen due to insufficient caching, scheduling jitter
     * or bug in the decoder. Ideally, the output would seek backward. But that
     * is not portable, not supported by some hardware and often unsafe/buggy
     * where supported. The other alternative is to flush the buffers
     * completely. */
    if (drift > (stream->sync.discontinuity ? 0
                : lroundf(+3 * AOUT_MAX_PTS_DELAY / rate)))
    {
        if (tracer != NULL)
            vlc_tracer_TraceEvent(tracer, "RENDER", stream->str_id, "late_flush");

        if (!stream->sync.discontinuity)
            msg_Warn (aout, "playback way too late (%"PRId64"): "
                      "flushing buffers", drift);
        else
            msg_Dbg (aout, "playback too late (%"PRId64"): "
                     "flushing buffers", drift);
        vlc_aout_stream_Flush(stream);
        stream_StopResampling(stream);

        return; /* nothing can be done if timing is unknown */
    }

    /* Early audio output.
     * This is rare except at startup when the buffers are still empty. */
    if (drift < (stream->sync.discontinuity ? 0
                : lroundf(-3 * AOUT_MAX_PTS_ADVANCE / rate)))
    {
        if (!stream->sync.discontinuity)
        {
            if (tracer != NULL)
                vlc_tracer_TraceEvent(tracer, "RENDER", stream->str_id, "early_silence");

            msg_Warn (aout, "playback way too early (%"PRId64"): "
                      "playing silence", drift);
        }
        stream_Silence(stream, -drift, audio_ts);

        stream_StopResampling(stream);
        stream->sync.discontinuity = true;
        drift = 0;
    }

    if (!aout_FiltersCanResample(stream->filters))
        return;

    /* Resampling */
    if (drift > +AOUT_MAX_PTS_DELAY
     && stream->sync.resamp_type != AOUT_RESAMPLING_UP)
    {
        if (tracer != NULL)
            vlc_tracer_TraceEvent(tracer, "RENDER", stream->str_id, "late_upsampling");
        msg_Warn (aout, "playback too late (%"PRId64"): up-sampling",
                  drift);
        stream->sync.resamp_type = AOUT_RESAMPLING_UP;
        stream->sync.resamp_start_drift = +drift;
    }
    if (drift < -AOUT_MAX_PTS_ADVANCE
     && stream->sync.resamp_type != AOUT_RESAMPLING_DOWN)
    {
        if (tracer != NULL)
            vlc_tracer_TraceEvent(tracer, "RENDER", stream->str_id, "early_downsampling");
        msg_Warn (aout, "playback too early (%"PRId64"): down-sampling",
                  drift);
        stream->sync.resamp_type = AOUT_RESAMPLING_DOWN;
        stream->sync.resamp_start_drift = -drift;
    }

    if (stream->sync.resamp_type == AOUT_RESAMPLING_NONE)
        return; /* Everything is fine. Nothing to do. */

    if (llabs (drift) > 2 * stream->sync.resamp_start_drift)
    {   /* If the drift is ever increasing, then something is seriously wrong.
         * Cease resampling and hope for the best. */
        if (tracer != NULL)
            vlc_tracer_TraceEvent(tracer, "RENDER", stream->str_id, "timing_screwed");

        msg_Warn (aout, "timing screwed (drift: %"PRId64" us): "
                  "stopping resampling", drift);
        stream_StopResampling(stream);
        return;
    }

    /* Resampling has been triggered earlier. This checks if it needs to be
     * increased or decreased. Resampling rate changes must be kept slow for
     * the comfort of listeners. */
    int adj = (stream->sync.resamp_type == AOUT_RESAMPLING_UP) ? +2 : -2;

    if (2 * llabs (drift) <= stream->sync.resamp_start_drift)
        /* If the drift has been reduced from more than half its initial
         * value, then it is time to switch back the resampling direction. */
        adj *= -1;

    if (!aout_FiltersAdjustResampling (stream->filters, adj))
    {   /* Everything is back to normal: stop resampling. */
        stream->sync.resamp_type = AOUT_RESAMPLING_NONE;

        if (tracer != NULL)
            vlc_tracer_TraceEvent(tracer, "RENDER", stream->str_id, "stop_resampling");
        msg_Dbg (aout, "resampling stopped (drift: %"PRId64" us)", drift);
    }
}

static void stream_Synchronize(vlc_aout_stream *stream, vlc_tick_t system_now,
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
    vlc_tick_t delay;

    if (stream_TimeGet(stream, &delay) != 0)
        return; /* nothing can be done if timing is unknown */

    if (stream->sync.discontinuity)
    {
        /* Chicken-egg situation for most aout modules that can't be started
         * deferred (all except PulseAudio). These modules will start to play
         * data immediately and ignore the given play_date (that take the clock
         * jitter into account). We don't want to let stream_HandleDrift()
         * handle the first silence (from the "Early audio output" case) since
         * this function will first update the clock without taking the jitter
         * into account. Therefore, we manually insert silence that correspond
         * to the clock jitter value before updating the clock.
         */
        vlc_tick_t play_date =
            vlc_clock_ConvertToSystem(stream->sync.clock, system_now + delay,
                                      dec_pts, stream->sync.rate);
        vlc_tick_t jitter = play_date - system_now;
        if (jitter > 0)
        {
            stream_Silence(stream, jitter, dec_pts - delay);
            if (stream_TimeGet(stream, &delay) != 0)
                return;
        }
    }

    vlc_tick_t drift = vlc_clock_Update(stream->sync.clock, system_now + delay,
                                        dec_pts, stream->sync.rate);

    stream_HandleDrift(stream, drift, dec_pts);
}


void vlc_aout_stream_NotifyLatency(vlc_aout_stream *stream, vlc_tick_t latency)
{
    atomic_store_explicit(&stream->latency, latency, memory_order_relaxed);
}

void vlc_aout_stream_NotifyTiming(vlc_aout_stream *stream, vlc_tick_t system_ts,
                                  vlc_tick_t audio_ts)
{
    /* This function might be called from high priority audio threads (so, no
     * mutexes, allocation, IO, debug, wait...). That is why we use a circular
     * buffer of points. The vlc_aout_stream user will read these points and
     * update the clock from vlc_aout_stream_Play() and
     * vlc_aout_stream_UpdateLatency(). */

    /* VLC mutexes use atomic and the reader will only do very fast
     * operations (copy of the timing_point data). */
    vlc_mutex_lock(&stream->timing_points.lock);
    size_t write = (stream->timing_points.write++) % MAX_TIMING_POINT;

    struct timing_point *p = &stream->timing_points.data[write];
    p->system_ts = system_ts;
    p->audio_ts = audio_ts;

    if (stream->timing_points.write >= MAX_TIMING_POINT)
        stream->timing_points.read++;
    vlc_mutex_unlock(&stream->timing_points.lock);

    if (stream->timing_points.notify_latency_cb != NULL)
        stream->timing_points.notify_latency_cb(
                stream->timing_points.notify_latency_data);
}

static vlc_tick_t
stream_ReadTimingPoints(vlc_aout_stream *stream)
{
    struct timing_point points[MAX_TIMING_POINT];
    size_t count = 0;

    vlc_mutex_lock(&stream->timing_points.lock);
    assert(stream->timing_points.write - stream->timing_points.read < MAX_TIMING_POINT);

    size_t initial_read = stream->timing_points.read;
    for (size_t read = stream->timing_points.read;
         read < stream->timing_points.write; ++read, ++count)
        points[count] = stream->timing_points.data[read % MAX_TIMING_POINT];

    stream->timing_points.write = stream->timing_points.read = 0;
    vlc_mutex_unlock(&stream->timing_points.lock);

    if (count == 0)
        return 0;

    stream->timing_points.last = points[count - 1];

    if (initial_read > 0)
    {
        /* This is not critical to miss timing points, the more important is
         * to get the last ones. Log it anyway since it can help identifying
         * buggy audio outputs spamming timing points. */

        audio_output_t *aout = aout_stream_aout(stream);
        msg_Dbg(aout, "Missed %zu timing points", initial_read);
    }
    vlc_tick_t drift = 0; /* Use only the last updated drift */
    float rate = stream->sync.rate; /* TODO: match rate with audio_ts */

    for (size_t i = 0; i < count; ++i)
        drift = vlc_clock_Update(stream->sync.clock, points[i].system_ts,
                                 points[i].audio_ts, rate);

    return drift;
}

void vlc_aout_stream_UpdateLatency(vlc_aout_stream *stream)
{
    stream_ReadTimingPoints(stream);
}

/*****************************************************************************
 * vlc_aout_stream_Play : filter & mix the decoded buffer
 *****************************************************************************/
int vlc_aout_stream_Play(vlc_aout_stream *stream, block_t *block)
{
    aout_owner_t *owner = aout_stream_owner(stream);
    audio_output_t *aout = aout_stream_aout(stream);

    assert (block->i_pts != VLC_TICK_INVALID);

    block->i_length = vlc_tick_from_samples( block->i_nb_samples,
                                   stream->input_format.i_rate );

    int ret = stream_CheckReady (stream);
    if (unlikely(ret == AOUT_DEC_FAILED))
        goto drop; /* Pipeline is unrecoverably broken :-( */

    if (block->i_flags & BLOCK_FLAG_DISCONTINUITY)
        stream_Discontinuity(stream);

    if (stream->original_pts == VLC_TICK_INVALID)
    {
        /* Use the original PTS for synchronization and as a play date of the
         * aout module. This PTS need to be saved here in order to use the PTS
         * of the first block that has been filtered. Indeed, aout filters may
         * need more than one block to output a new one. */
        stream->original_pts = block->i_pts;
    }

    if (stream->filters)
    {
        if (atomic_load_explicit(&owner->vp.update, memory_order_relaxed))
        {
            vlc_mutex_lock (&owner->vp.lock);
            aout_FiltersChangeViewpoint (stream->filters, &owner->vp.value);
            atomic_store_explicit(&owner->vp.update, false, memory_order_relaxed);
            vlc_mutex_unlock (&owner->vp.lock);
        }

        block = aout_FiltersPlay(stream->filters, block, stream->sync.rate);
        if (block == NULL)
            return ret;
    }

    const vlc_tick_t original_pts = stream->original_pts;
    stream->original_pts = VLC_TICK_INVALID;

    /* Software volume */
    aout_volume_Amplify(stream->volume, block);

    /* Update delay */
    if (stream->sync.request_delay != stream->sync.delay)
    {
        stream->sync.delay = stream->sync.request_delay;
        vlc_tick_t delta = vlc_clock_SetDelay(stream->sync.clock, stream->sync.delay);
        if (stream->filters)
            aout_FiltersSetClockDelay(stream->filters, stream->sync.delay);
        if (delta > 0)
            stream_Silence(stream, delta, block->i_pts);
    }

    /* Drift correction */
    vlc_tick_t system_now = vlc_tick_now();
    if (aout->time_get != NULL)
        stream_Synchronize(stream, system_now, original_pts);
    else
    {
        vlc_tick_t drift = stream_ReadTimingPoints(stream);
        stream_HandleDrift(stream, drift, original_pts);
    }

    vlc_tick_t play_date =
        vlc_clock_ConvertToSystem(stream->sync.clock, system_now, original_pts,
                                  stream->sync.rate);
    if (unlikely(play_date == VLC_TICK_MAX))
    {
        /* The clock is paused but not the output, play the audio anyway since
         * we can't delay audio playback from here. */
        play_date = system_now;

    }

    vlc_audio_meter_Process(&owner->meter, block, play_date);

    /* Output */
    stream->sync.discontinuity = false;
    aout->play(aout, block, play_date);

    atomic_fetch_add_explicit(&stream->buffers_played, 1, memory_order_relaxed);
    return ret;
drop:
    stream_Discontinuity(stream);
    block_Release (block);
    atomic_fetch_add_explicit(&stream->buffers_lost, 1, memory_order_relaxed);
    return ret;
}

void vlc_aout_stream_GetResetStats(vlc_aout_stream *stream, unsigned *restrict lost,
                           unsigned *restrict played, vlc_tick_t *restrict latency)
{
    *lost = atomic_exchange_explicit(&stream->buffers_lost, 0,
                                     memory_order_relaxed);
    *played = atomic_exchange_explicit(&stream->buffers_played, 0,
                                       memory_order_relaxed);
    *latency = atomic_load_explicit(&stream->latency, memory_order_relaxed);
}

void vlc_aout_stream_ChangePause(vlc_aout_stream *stream, bool paused, vlc_tick_t date)
{
    audio_output_t *aout = aout_stream_aout(stream);

    if (stream->mixer_format.i_format)
    {
        struct vlc_tracer *tracer = aout_stream_tracer(stream);
        if (tracer != NULL)
            vlc_tracer_TraceEvent(tracer, "RENDER", stream->str_id,
                                  paused ? "paused" : "resumed");

        if (aout->pause != NULL)
            aout->pause(aout, paused, date);
        else if (paused)
            aout->flush(aout);
    }
}

void vlc_aout_stream_ChangeRate(vlc_aout_stream *stream, float rate)
{
    stream->sync.rate = rate;
}

void vlc_aout_stream_ChangeDelay(vlc_aout_stream *stream, vlc_tick_t delay)
{
    stream->sync.request_delay = delay;
}

void vlc_aout_stream_Flush(vlc_aout_stream *stream)
{
    audio_output_t *aout = aout_stream_aout(stream);

    struct vlc_tracer *tracer = aout_stream_tracer(stream);
    if (tracer != NULL)
        vlc_tracer_TraceEvent(tracer, "RENDER", stream->str_id, "flushed");

    if (stream->mixer_format.i_format)
        aout->flush(aout);
    stream_Reset(stream);
}

void vlc_aout_stream_NotifyGain(vlc_aout_stream *stream, float gain)
{
    aout_volume_SetVolume(stream->volume, gain);
}

void vlc_aout_stream_NotifyDrained(vlc_aout_stream *stream)
{
    atomic_store_explicit(&stream->drained, true, memory_order_relaxed);
}

bool vlc_aout_stream_IsDrained(vlc_aout_stream *stream)
{
    audio_output_t *aout = aout_stream_aout(stream);

    if (aout->drain == NULL)
    {
        vlc_tick_t drain_deadline =
            atomic_load_explicit(&stream->drain_deadline, memory_order_relaxed);
        return drain_deadline != VLC_TICK_INVALID
            && vlc_tick_now() >= drain_deadline;
    }
    else
        return atomic_load_explicit(&stream->drained, memory_order_relaxed);
}

void vlc_aout_stream_Drain(vlc_aout_stream *stream)
{
    audio_output_t *aout = aout_stream_aout(stream);

    if (!stream->mixer_format.i_format)
        return;

    struct vlc_tracer *tracer = aout_stream_tracer(stream);

    if (tracer != NULL)
        vlc_tracer_TraceEvent(tracer, "RENDER", stream->str_id, "drain");

    if (stream->filters)
    {
        block_t *block = aout_FiltersDrain (stream->filters);
        if (block)
            aout->play(aout, block, vlc_tick_now());
    }

    if (aout->drain)
    {
        assert(!atomic_load_explicit(&stream->drained, memory_order_relaxed));
        aout->drain(aout);
    }
    else
    {
        assert(atomic_load_explicit(&stream->drain_deadline,
                                    memory_order_relaxed) == VLC_TICK_INVALID);

        vlc_tick_t drain_deadline = vlc_tick_now();

        vlc_tick_t delay;
        if (stream_TimeGet(stream, &delay) == 0)
            drain_deadline += delay;
        /* else the deadline is now, and vlc_aout_stream_IsDrained() will
         * return true on the first call. */

        atomic_store_explicit(&stream->drain_deadline, drain_deadline,
                              memory_order_relaxed);
    }
}
