/*****************************************************************************
 * aaudio.c: Android AAudio audio stream module
 *****************************************************************************
 * Copyright © 2018-2022 VLC authors, VideoLAN and VideoLabs
 *
 * Authors: Romain Vimont <rom1v@videolabs.io>
 *          Thomas Guillem <thomas@gllm.fr>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <assert.h>
#include <dlfcn.h>

#include "device.h"

#include <aaudio/AAudio.h>

#define TIMING_REPORT_DELAY_TICKS VLC_TICK_FROM_MS(1000)

struct sys
{
    AAudioStream *as;

    jobject dp;
    float volume;
    bool muted;

    audio_sample_format_t fmt;
    int64_t frames_flush_pos;
    bool error;

    /* Spinlock that protect all the following variables */
    vlc_mutex_t lock;

    /* Frame FIFO */
    vlc_frame_t *frame_chain;
    vlc_frame_t **frame_last;

    /* Size of the frame FIFO */
    size_t frames_total_bytes;
    /* Bytes of silence written until it started */
    size_t start_silence_bytes;
    /* Bytes of silence written after it started */
    size_t underrun_bytes;
    /* Date when the data callback should start to process audio */
    vlc_tick_t first_play_date;
    /* True is the data callback started to process audio from the frame FIFO */
    bool started;
    bool draining;
    /* Bytes written since the last timing report */
    size_t timing_report_last_written_bytes;
    /* Number of bytes to write before sending a timing report */
    size_t timing_report_delay_bytes;
};

/* dlopen/dlsym symbols */
static struct {
    void *handle;
    aaudio_result_t       (*AAudio_createStreamBuilder)(AAudioStreamBuilder **);
    const char           *(*AAudio_convertResultToText)(aaudio_result_t);
    void                  (*AAudioStreamBuilder_setFormat)(AAudioStreamBuilder *, aaudio_format_t);
    void                  (*AAudioStreamBuilder_setChannelCount)(AAudioStreamBuilder *, int32_t);
    void                  (*AAudioStreamBuilder_setDataCallback)(AAudioStreamBuilder *, AAudioStream_dataCallback, void *);
    void                  (*AAudioStreamBuilder_setErrorCallback)(AAudioStreamBuilder *, AAudioStream_errorCallback,  void *);
    void                  (*AAudioStreamBuilder_setPerformanceMode)(AAudioStreamBuilder *, aaudio_performance_mode_t);
    void                  (*AAudioStreamBuilder_setSessionId)(AAudioStreamBuilder *, aaudio_session_id_t);
    void                  (*AAudioStreamBuilder_setUsage)(AAudioStreamBuilder *, aaudio_usage_t);
    aaudio_result_t       (*AAudioStreamBuilder_openStream)(AAudioStreamBuilder *, AAudioStream **);
    void                  (*AAudioStreamBuilder_delete)(AAudioStreamBuilder *);
    aaudio_result_t       (*AAudioStream_requestStart)(AAudioStream *);
    aaudio_result_t       (*AAudioStream_requestStop)(AAudioStream *);
    aaudio_result_t       (*AAudioStream_requestPause)(AAudioStream *);
    aaudio_result_t       (*AAudioStream_requestFlush)(AAudioStream *);
    int32_t               (*AAudioStream_getSampleRate)(AAudioStream *);
    aaudio_result_t       (*AAudioStream_getTimestamp)(AAudioStream *, clockid_t,
                                                       int64_t *framePosition, int64_t *timeNanoseconds);
    aaudio_result_t       (*AAudioStream_write)(AAudioStream *, void *, int32_t numFrames, int64_t timeoutNanoseconds);
    aaudio_result_t       (*AAudioStream_close)(AAudioStream *);
    aaudio_stream_state_t (*AAudioStream_getState)(AAudioStream *);
    aaudio_result_t       (*AAudioStream_waitForStateChange)(AAudioStream *, aaudio_stream_state_t current_state, aaudio_stream_state_t *nextState, int64_t timeoutNanoseconds);
    aaudio_session_id_t   (*AAudioStream_getSessionId)(AAudioStream *);
} vt;

static int
LoadSymbols(aout_stream_t *stream)
{
    /* Force failure on Android < 8.1, where multiple restarts may cause
     * segfault (unless we leak the AAudioStream on Close). The problem is
     * fixed in AOSP by:
     * <https://android.googlesource.com/platform/frameworks/av/+/8a8a9e5d91c8cc110b9916982f4c5242efca33e3%5E%21/>
     *
     * Require AAudioStreamBuilder_setSessionId() that is available in Android
     * 9 to enforce that the previous issue is fixed and to allow us attaching
     * an AudioEffect to modify the gain.
     */

    static vlc_mutex_t lock = VLC_STATIC_MUTEX;
    static int init_state = -1;

    vlc_mutex_lock(&lock);

    if (init_state != -1)
        goto end;

    vt.handle = dlopen("libaaudio.so", RTLD_NOW);
    if (vt.handle == NULL)
    {
        msg_Err(stream, "Failed to load libaaudio");
        init_state = 0;
        goto end;
    }

#define AAUDIO_DLSYM(name) \
    do { \
        void *sym = dlsym(vt.handle, #name); \
        if (unlikely(!sym)) { \
            msg_Err(stream, "Failed to load symbol "#name); \
            init_state = 0; \
            goto end; \
        } \
        *(void **) &vt.name = sym; \
    } while(0)

    AAUDIO_DLSYM(AAudio_createStreamBuilder);
    AAUDIO_DLSYM(AAudio_convertResultToText);
    AAUDIO_DLSYM(AAudioStreamBuilder_setChannelCount);
    AAUDIO_DLSYM(AAudioStreamBuilder_setFormat);
    AAUDIO_DLSYM(AAudioStreamBuilder_setDataCallback);
    AAUDIO_DLSYM(AAudioStreamBuilder_setErrorCallback);
    AAUDIO_DLSYM(AAudioStreamBuilder_setPerformanceMode);
    AAUDIO_DLSYM(AAudioStreamBuilder_setSessionId);
    AAUDIO_DLSYM(AAudioStreamBuilder_setUsage);
    AAUDIO_DLSYM(AAudioStreamBuilder_openStream);
    AAUDIO_DLSYM(AAudioStreamBuilder_delete);
    AAUDIO_DLSYM(AAudioStream_requestStart);
    AAUDIO_DLSYM(AAudioStream_requestStop);
    AAUDIO_DLSYM(AAudioStream_requestPause);
    AAUDIO_DLSYM(AAudioStream_requestFlush);
    AAUDIO_DLSYM(AAudioStream_getSampleRate);
    AAUDIO_DLSYM(AAudioStream_getTimestamp);
    AAUDIO_DLSYM(AAudioStream_write);
    AAUDIO_DLSYM(AAudioStream_close);
    AAUDIO_DLSYM(AAudioStream_getState);
    AAUDIO_DLSYM(AAudioStream_waitForStateChange);
    AAUDIO_DLSYM(AAudioStream_getSessionId);
#undef AAUDIO_DLSYM

    init_state = 1;
end:
    vlc_mutex_unlock(&lock);
    return init_state == 1 ? VLC_SUCCESS : VLC_EGENERIC;
}

struct role
{
    char vlc[16];
    aaudio_usage_t aaudio;
};

static const struct role roles[] =
{
    { "accessibility", AAUDIO_USAGE_ASSISTANCE_ACCESSIBILITY },
    { "animation",     AAUDIO_USAGE_ASSISTANCE_SONIFICATION  },
    { "communication", AAUDIO_USAGE_VOICE_COMMUNICATION      },
    { "game",          AAUDIO_USAGE_GAME                     },
    { "music",         AAUDIO_USAGE_MEDIA                    },
    { "notification",  AAUDIO_USAGE_NOTIFICATION_EVENT       },
    { "production",    AAUDIO_USAGE_MEDIA                    },
    { "test",          AAUDIO_USAGE_MEDIA                    },
    { "video",         AAUDIO_USAGE_MEDIA                    },
};

static int
role_cmp(const void *vlc_role, const void *entry)
{
    const struct role *role = entry;
    return strcmp(vlc_role, role->vlc);
}

static aaudio_usage_t
GetUsageFromVLCRole(const char *vlc_role)
{
    struct role *role = bsearch(vlc_role, roles, ARRAY_SIZE(roles),
                                sizeof(*roles), role_cmp);

    return role == NULL ? AAUDIO_USAGE_MEDIA : role->aaudio;
}

static inline void
LogAAudioError(aout_stream_t *stream, const char *msg, aaudio_result_t result)
{
    msg_Err(stream, "%s: %s", msg, vt.AAudio_convertResultToText(result));
}

static int
WaitState(aout_stream_t *stream, aaudio_result_t wait_state)
{
#define WAIT_TIMEOUT VLC_TICK_FROM_SEC(2)
    struct sys *sys = stream->sys;

    if (sys->error)
        return VLC_EGENERIC;

    aaudio_stream_state_t next_state = vt.AAudioStream_getState(sys->as);
    aaudio_stream_state_t current_state = next_state;

    while (next_state != wait_state)
    {
        aaudio_result_t result =
            vt.AAudioStream_waitForStateChange(sys->as, current_state,
                                               &next_state, WAIT_TIMEOUT);
        if (result != AAUDIO_OK)
        {
            sys->error = true;
            return VLC_EGENERIC;
        }
        current_state = next_state;
    }
    return VLC_SUCCESS;
}

static aaudio_stream_state_t
GetState(aout_stream_t *stream)
{
    struct sys *sys = stream->sys;

    return sys->error ? AAUDIO_STREAM_STATE_UNINITIALIZED
                      : vt.AAudioStream_getState(sys->as);
}

#define Request(x) do { \
    struct sys *sys = stream->sys; \
    aaudio_result_t result = vt.AAudioStream_request##x(sys->as); \
    if (result == AAUDIO_OK) \
        return VLC_SUCCESS; \
    LogAAudioError(stream, "Failed to start AAudio stream", result); \
    sys->error = true; \
    return VLC_EGENERIC; \
} while (0)

static inline int
RequestStart(aout_stream_t *stream)
{
    Request(Start);
}

static inline int
RequestStop(aout_stream_t *stream)
{
    Request(Stop);
}

static inline int
RequestPause(aout_stream_t *stream)
{
    Request(Pause);
}

static inline int
RequestFlush(aout_stream_t *stream)
{
    Request(Flush);
}

static inline uint64_t
BytesToFrames(struct sys *sys, size_t bytes)
{
    return bytes * sys->fmt.i_frame_length / sys->fmt.i_bytes_per_frame;
}

static inline vlc_tick_t
FramesToTicks(struct sys *sys, int64_t frames)
{
    return vlc_tick_from_samples(frames, sys->fmt.i_rate);
}


static inline vlc_tick_t
BytesToTicks(struct sys *sys, size_t bytes)
{
    return FramesToTicks(sys, BytesToFrames(sys, bytes));
}

static inline size_t
FramesToBytes(struct sys *sys, uint64_t frames)
{
    return frames * sys->fmt.i_bytes_per_frame / sys->fmt.i_frame_length;
}

static inline int64_t
TicksToFrames(struct sys *sys, vlc_tick_t ticks)
{
    return samples_from_vlc_tick(ticks, sys->fmt.i_rate);
}

static inline size_t
TicksToBytes(struct sys *sys, vlc_tick_t ticks)
{
    return FramesToBytes(sys, TicksToFrames(sys, ticks));
}

static int
GetFrameTimestampLocked(aout_stream_t *stream, int64_t *pos_frames,
                        vlc_tick_t *frame_time_us)
{
    struct sys *sys = stream->sys;

    int64_t time_ns;
    aaudio_result_t result =
            vt.AAudioStream_getTimestamp(sys->as, CLOCK_MONOTONIC,
                                         pos_frames, &time_ns);
    if (result != AAUDIO_OK)
    {
        LogAAudioError(stream, "AAudioStream_getTimestamp failed", result);
        return VLC_EGENERIC;
    }
    if (*pos_frames <= 0)
        return VLC_EGENERIC;

    *frame_time_us = VLC_TICK_FROM_NS(time_ns);
    return VLC_SUCCESS;
}

static aaudio_data_callback_result_t
DataCallback(AAudioStream *as, void *user, void *data_, int32_t num_frames)
{
    aout_stream_t *stream = user;
    struct sys *sys = stream->sys;
    assert(as == sys->as); (void) as;

    size_t bytes = FramesToBytes(sys, num_frames);
    uint8_t *data = data_;

    vlc_mutex_lock(&sys->lock);
    aaudio_stream_state_t state = GetState(stream);

    if (!sys->started)
    {
        size_t tocopy;

        if (sys->first_play_date == VLC_TICK_INVALID)
            tocopy = bytes;
        else
        {
            vlc_tick_t now = vlc_tick_now();

            /* Write silence to reach the first play date */
            vlc_tick_t silence_ticks = sys->first_play_date - now;
            if (silence_ticks > 0)
            {
                tocopy = TicksToBytes(sys, silence_ticks);
                if (tocopy > bytes)
                    tocopy = bytes;
            }
            else
                tocopy = 0;
        }

        if (tocopy > 0)
        {
            memset(data, 0, tocopy);

            data += tocopy;
            bytes -= tocopy;

            sys->start_silence_bytes += tocopy;
        }
    }

    while (bytes > 0)
    {
        vlc_frame_t *f = sys->frame_chain;
        if (f == NULL)
        {
            aaudio_data_callback_result_t res;

            if (!sys->draining)
            {
                sys->underrun_bytes += bytes;
                res = AAUDIO_CALLBACK_RESULT_CONTINUE;
            }
            else
                res = AAUDIO_CALLBACK_RESULT_STOP;

            vlc_mutex_unlock(&sys->lock);

            memset(data, 0, bytes);

            if (res == AAUDIO_CALLBACK_RESULT_STOP)
                aout_stream_DrainedReport(stream);
            return res;
        }

        size_t tocopy = f->i_buffer > bytes ? bytes : f->i_buffer;

        sys->started = true;

        sys->frames_total_bytes -= tocopy;
        sys->timing_report_last_written_bytes += tocopy;

        int64_t pos_frames;
        vlc_tick_t system_ts;
        if (state == AAUDIO_STREAM_STATE_STARTED
         && sys->timing_report_last_written_bytes >= sys->timing_report_delay_bytes
         && GetFrameTimestampLocked(stream, &pos_frames, &system_ts) == VLC_SUCCESS)
        {
            sys->timing_report_last_written_bytes = 0;

            /* From now on, fetch the timestamp every 1 seconds */
            sys->timing_report_delay_bytes =
                TicksToBytes(sys, TIMING_REPORT_DELAY_TICKS);

            pos_frames -= sys->frames_flush_pos;
            if (unlikely(pos_frames < 0))
                pos_frames = 0;
            vlc_tick_t pos_ticks = FramesToTicks(sys, pos_frames);

            /* Add the start silence to the system time and don't subtract
             * it from pos_ticks to avoid (unlikely) negatives ts */
            system_ts += BytesToTicks(sys, sys->start_silence_bytes);
            aout_stream_TimingReport(stream, system_ts, pos_ticks);
        }

        memcpy(data, f->p_buffer, tocopy);

        data += tocopy;
        bytes -= tocopy;
        f->i_buffer -= tocopy;
        f->p_buffer += tocopy;

        if (f->i_buffer == 0)
        {
            sys->frame_chain = f->p_next;
            if (sys->frame_chain == NULL)
                sys->frame_last = &sys->frame_chain;

            vlc_frame_Release(f);
        }
    }

    vlc_mutex_unlock(&sys->lock);

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void
ErrorCallback(AAudioStream *as, void *user, aaudio_result_t error)
{
    aout_stream_t *stream = user;
    (void) as;

    LogAAudioError(stream, "AAudio stream error or disconnected:", error);
    aout_stream_RestartRequest(stream, AOUT_RESTART_OUTPUT);
}

static void
CloseAAudioStream(aout_stream_t *stream)
{
    struct sys *sys = stream->sys;

    RequestStop(stream);

    if (WaitState(stream, AAUDIO_STREAM_STATE_STOPPED) == VLC_SUCCESS)
        vt.AAudioStream_close(sys->as);
    else
        msg_Warn(stream, "Error waiting for stopped state");

    sys->as = NULL;
}

static int
OpenAAudioStream(aout_stream_t *stream, audio_sample_format_t *fmt)
{
    struct sys *sys = stream->sys;
    aaudio_result_t result;

    AAudioStreamBuilder *builder;
    result = vt.AAudio_createStreamBuilder(&builder);
    if (result != AAUDIO_OK)
    {
        LogAAudioError(stream, "Failed to create AAudio stream builder", result);
        return VLC_EGENERIC;
    }

    aaudio_format_t format;
    if (fmt->i_format == VLC_CODEC_S16N)
        format = AAUDIO_FORMAT_PCM_I16;
    else
    {
        if (fmt->i_format != VLC_CODEC_FL32)
        {
            /* override to request conversion */
            fmt->i_format = VLC_CODEC_FL32;
            fmt->i_bytes_per_frame = 4 * fmt->i_channels;
        }
        format = AAUDIO_FORMAT_PCM_FLOAT;
    }

    vt.AAudioStreamBuilder_setFormat(builder, format);
    vt.AAudioStreamBuilder_setChannelCount(builder, fmt->i_channels);

    /* Setup the session-id */
    int32_t session_id = var_InheritInteger(stream, "audiotrack-session-id");
    if (session_id == 0)
        session_id = AAUDIO_SESSION_ID_ALLOCATE;
    vt.AAudioStreamBuilder_setSessionId(builder, session_id);

    /* Setup the role */
    char *vlc_role = var_InheritString(stream, "role");
    if (vlc_role != NULL)
    {
        aaudio_usage_t usage = GetUsageFromVLCRole(vlc_role);
        vt.AAudioStreamBuilder_setUsage(builder, usage);
        free(vlc_role);
    } /* else if not set, default is MEDIA usage */

    bool low_latency = false;
    if (fmt->channel_type == AUDIO_CHANNEL_TYPE_AMBISONICS)
    {
        fmt->channel_type = AUDIO_CHANNEL_TYPE_BITMAP;
        low_latency = true;
    }

    vt.AAudioStreamBuilder_setDataCallback(builder, DataCallback, stream);
    vt.AAudioStreamBuilder_setErrorCallback(builder, ErrorCallback, stream);

    if (low_latency)
        vt.AAudioStreamBuilder_setPerformanceMode(builder,
            AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);

    AAudioStream *as;
    result = vt.AAudioStreamBuilder_openStream(builder, &as);
    if (result != AAUDIO_OK)
    {
        LogAAudioError(stream, "Failed to open AAudio stream", result);
        vt.AAudioStreamBuilder_delete(builder);
        return VLC_EGENERIC;
    }
    vt.AAudioStreamBuilder_delete(builder);

    /* Use the native sample rate of the device */
    fmt->i_rate = vt.AAudioStream_getSampleRate(as);
    assert(fmt->i_rate > 0);

    sys->as = as;
    sys->fmt = *fmt;

    return VLC_SUCCESS;
}

static void
Play(aout_stream_t *stream, vlc_frame_t *frame, vlc_tick_t date)
{
    struct sys *sys = stream->sys;
    assert(sys->as);

    aaudio_stream_state_t state = GetState(stream);
    if (state == AAUDIO_STREAM_STATE_OPEN
     || state == AAUDIO_STREAM_STATE_FLUSHED)
    {
        if (RequestStart(stream) != VLC_SUCCESS)
            goto bailout;
    }
    else if (state == AAUDIO_STREAM_STATE_UNINITIALIZED)
        goto bailout;
    else
    {
        assert(state == AAUDIO_STREAM_STATE_STARTING
            || state == AAUDIO_STREAM_STATE_STARTED);
    }

    vlc_mutex_lock(&sys->lock);

    if (!sys->started)
    {
        vlc_tick_t now = vlc_tick_now();
        sys->first_play_date = date - BytesToTicks(sys, sys->frames_total_bytes);

        if (sys->first_play_date > now)
            msg_Dbg(stream, "deferring start (%"PRId64" us)",
                    sys->first_play_date - now);
        else
            msg_Dbg(stream, "starting late (%"PRId64" us)",
                    sys->first_play_date - now);
    }


    vlc_frame_ChainLastAppend(&sys->frame_last, frame);
    sys->frames_total_bytes += frame->i_buffer;

    size_t underrun_bytes = sys->underrun_bytes;
    sys->underrun_bytes = 0;
    vlc_mutex_unlock(&sys->lock);

    if (underrun_bytes > 0)
        msg_Warn(stream, "underflow of %" PRId64 " us",
                 BytesToTicks(sys, underrun_bytes));

    return;
bailout:
    vlc_frame_Release(frame);
}

static void
Pause(aout_stream_t *stream, bool pause, vlc_tick_t pause_date)
{
    struct sys *sys = stream->sys;
    (void) pause_date;

    aaudio_stream_state_t state = GetState(stream);
    if (state == AAUDIO_STREAM_STATE_FLUSHED)
        return; /* already paused, will be resumed from Play() */

    if (pause)
    {
        if (state == AAUDIO_STREAM_STATE_STARTING
         || state == AAUDIO_STREAM_STATE_STARTED)
            RequestPause(stream);

        state = GetState(stream);
        if (state == AAUDIO_STREAM_STATE_PAUSING
         && WaitState(stream, AAUDIO_STREAM_STATE_PAUSED) != VLC_SUCCESS)

        sys->started = false;
    }
    else
    {
        if (state == AAUDIO_STREAM_STATE_PAUSING
         || state == AAUDIO_STREAM_STATE_PAUSED)
            RequestStart(stream);
    }
}

static void
Flush(aout_stream_t *stream)
{
    struct sys *sys = stream->sys;
    aaudio_stream_state_t state = GetState(stream);

    if (state == AAUDIO_STREAM_STATE_UNINITIALIZED
     || state == AAUDIO_STREAM_STATE_FLUSHED
     || state == AAUDIO_STREAM_STATE_OPEN)
        return;

    /* Flush must be requested while PAUSED */

    if (state != AAUDIO_STREAM_STATE_PAUSING
     && state != AAUDIO_STREAM_STATE_PAUSED
     && RequestPause(stream) != VLC_SUCCESS)
        return;

    state = GetState(stream);
    if (state == AAUDIO_STREAM_STATE_PAUSING
     && WaitState(stream, AAUDIO_STREAM_STATE_PAUSED) != VLC_SUCCESS)
        return;

    /* The doc states that "Pausing a stream will freeze the data flow but not
     * flush any buffers" but this does not seem to be the case for all
     * arch/devices/versions. So, flush everything with the lock held in the
     * unlikely case that the data callback is called between PAUSED and
     * FLUSHING */
    vlc_mutex_lock(&sys->lock);

    vlc_frame_ChainRelease(sys->frame_chain);
    sys->frame_chain = NULL;
    sys->frame_last = &sys->frame_chain;
    sys->frames_total_bytes = 0;

    sys->started = false;
    sys->draining = false;
    sys->first_play_date = VLC_TICK_INVALID;
    sys->start_silence_bytes = 0;
    sys->timing_report_last_written_bytes = 0;
    sys->timing_report_delay_bytes = 0;
    sys->underrun_bytes = 0;

    vlc_tick_t unused;
    if (GetFrameTimestampLocked(stream, &sys->frames_flush_pos, &unused) != VLC_SUCCESS)
    {
        sys->frames_flush_pos = 0;
        msg_Warn(stream, "Flush: can't get paused position");
    }

    vlc_mutex_unlock(&sys->lock);

    if (RequestFlush(stream) != VLC_SUCCESS)
        return;

    if (WaitState(stream, AAUDIO_STREAM_STATE_FLUSHED) != VLC_SUCCESS)
        return;
}

static void
Drain(aout_stream_t *stream)
{
    struct sys *sys = stream->sys;
    aaudio_stream_state_t state = GetState(stream);

    vlc_mutex_lock(&sys->lock);
    sys->draining = true;
    vlc_mutex_unlock(&sys->lock);

    /* In case of differed start, the stream may not have been started yet */
    if (unlikely(state != AAUDIO_STREAM_STATE_STARTED))
    {
        if (state != AAUDIO_STREAM_STATE_STARTING
         && RequestStart(stream) != VLC_SUCCESS)
            return;

        if (WaitState(stream, AAUDIO_STREAM_STATE_STARTED) != VLC_SUCCESS)
            return;
    }
}

static void
VolumeSet(aout_stream_t *stream, float vol)
{
    struct sys *sys = stream->sys;

    if (vol > 1.0f)
        vol = vol * vol * vol;

    sys->volume = vol;

    if (sys->muted || sys->dp == NULL)
        return;

    int ret = DynamicsProcessing_SetVolume(stream, sys->dp, vol);
    if (ret != VLC_SUCCESS)
        msg_Warn(stream, "failed to set the volume via DynamicsProcessing");
}

static void
MuteSet(aout_stream_t *stream, bool mute)
{
    struct sys *sys = stream->sys;

    sys->muted = mute;

    if (sys->dp == NULL)
        return;

    int ret = DynamicsProcessing_SetVolume(stream, sys->dp,
                                           mute ? 0.0f : sys->volume);
    if (ret != VLC_SUCCESS)
        msg_Warn(stream, "failed to mute via DynamicsProcessing");
}

static void
Stop(aout_stream_t *stream)
{
    struct sys *sys = stream->sys;

    CloseAAudioStream(stream);

    if (sys->dp != NULL)
        DynamicsProcessing_Delete(stream, sys->dp);

    vlc_frame_ChainRelease(sys->frame_chain);

    free(sys);
}

static int
Start(aout_stream_t *stream, audio_sample_format_t *fmt,
      enum android_audio_device_type adev)
{
    (void) adev;

    if (!AOUT_FMT_LINEAR(fmt))
        return VLC_EGENERIC;

    if (LoadSymbols(stream) != VLC_SUCCESS)
        return VLC_EGENERIC;

    struct sys *sys = stream->sys = malloc(sizeof(*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->volume = 1.0f;
    sys->muted = false;
    sys->frames_flush_pos = 0;
    sys->error = false;

    vlc_mutex_init(&sys->lock);
    sys->frame_chain = NULL;
    sys->frame_last = &sys->frame_chain;
    sys->frames_total_bytes = 0;
    sys->start_silence_bytes = 0;
    sys->underrun_bytes = 0;
    sys->started = false;
    sys->draining = false;
    sys->first_play_date = VLC_TICK_INVALID;
    sys->timing_report_last_written_bytes = 0;
    sys->timing_report_delay_bytes = 0;

    int ret = OpenAAudioStream(stream, fmt);
    if (ret != VLC_SUCCESS)
    {
        free(sys);
        return ret;
    }

    int32_t session_id = vt.AAudioStream_getSessionId(sys->as);

    if (session_id != AAUDIO_SESSION_ID_NONE)
        sys->dp = DynamicsProcessing_New(stream, session_id);
    else
        sys->dp = NULL;

    if (sys->dp == NULL)
        msg_Warn(stream, "failed to attach DynamicsProcessing to the stream)");

    stream->stop = Stop;
    stream->play = Play;
    stream->pause = Pause;
    stream->flush = Flush;
    stream->drain = Drain;
    stream->volume_set = VolumeSet;
    stream->mute_set = MuteSet;

    msg_Dbg(stream, "using AAudio API");
    return VLC_SUCCESS;
}

vlc_module_begin ()
    set_shortname("AAudio")
    set_description("Android AAudio output")
    set_subcategory(SUBCAT_AUDIO_AOUT)
    set_capability("aout android stream", 190)
    set_callback(Start)
vlc_module_end ()
