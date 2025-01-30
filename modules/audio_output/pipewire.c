/*****************************************************************************
 * pipewire.c: PipeWire audio output plugin for VLC
 *****************************************************************************
 * Copyright (C) 2022 Rémi Denis-Courmont
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

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <search.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <pipewire/pipewire.h>
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_tick.h>
#include "vlc_pipewire.h"

struct vlc_pw_stream {
    struct vlc_pw_context *context;
    struct pw_stream *stream;
    struct spa_hook listener;
    size_t stride;

    struct {
        vlc_frame_t *head;
        vlc_frame_t **tailp;
        size_t size;
    } queue;

    struct {
        vlc_tick_t pts;
        ptrdiff_t frames;
        unsigned int rate;
        uint64_t injected;
        vlc_tick_t next_update;
    } time;

    vlc_tick_t start;
    vlc_tick_t first_pts;
    bool starting;
    bool draining;
    bool error;

    audio_output_t *aout;
};

/**
 * Stream control callback.
 *
 * This monitors the stream for control changes and reports them as applicable.
 */
static void stream_control_info(void *data, uint32_t id,
                                const struct pw_stream_control *control)
{
    struct vlc_pw_stream *s = data;

    vlc_pw_debug(s->context, "control %"PRIu32" %s", id, control->name);

    switch (id) {
        case SPA_PROP_mute:
            aout_MuteReport(s->aout, control->values[0] != 0.f);
            break;

        case SPA_PROP_channelVolumes: {
            float vol = 0.f;

            for (size_t i = 0; i < control->n_values; i++)
                 vol = fmaxf(vol, control->values[i]);

            aout_VolumeReport(s->aout, vol);
            break;
        }
    }
}

/**
 * Stream state callback.
 *
 * This monitors the stream for state change, looking out for fatal errors.
 */
static void stream_state_changed(void *data, enum pw_stream_state old,
                                 enum pw_stream_state state, const char *err)
{
    struct vlc_pw_stream *s = data;

    if (state == PW_STREAM_STATE_ERROR)
    {
        vlc_pw_error(s->context, "stream error: %s", err);
        s->error = true;
    }
    else
        vlc_pw_debug(s->context, "stream %s",
                     pw_stream_state_as_string(state));
    if (old != state)
        vlc_pw_signal(s->context);
}

/**
 * Retrieve latest timings
 */
static int stream_update_latency(struct vlc_pw_stream *s, vlc_tick_t *now)
{
    struct pw_time ts;

#if PW_CHECK_VERSION(1, 1, 0)
    /* PW monotonic clock, same than vlc_tick_now() */
    *now = VLC_TICK_FROM_NS(pw_stream_get_nsec(s->stream));
#else
    *now = vlc_tick_now();
#endif

    if (pw_stream_get_time_n(s->stream, &ts, sizeof (ts)) < 0
     || ts.rate.denom == 0)
        return -1;

    s->time.pts = VLC_TICK_FROM_NS(ts.now);
    s->time.pts += vlc_tick_from_frac(ts.delay * ts.rate.num, ts.rate.denom);
    s->time.frames = ts.buffered + ts.queued;

    return 0;
}

/**
 * Stream processing callback.
 *
 * This fills in the next audio buffer.
 */
static void stream_process(void *data)
{
    struct vlc_pw_stream *s = data;
    vlc_tick_t now;
    int val = stream_update_latency(s, &now);
    struct pw_buffer *b = pw_stream_dequeue_buffer(s->stream);

    if (likely(b != NULL)) {
        /* One should have more indirection layers than pizza condiments */
        struct spa_buffer *buf = b->buffer;
        struct spa_data *d = &buf->datas[0];
        struct spa_chunk *chunk = d->chunk;
        unsigned char *dst = d->data;
        size_t frame_room = d->maxsize / s->stride;
        size_t room = frame_room * s->stride;
        vlc_frame_t *block;

        chunk->offset = 0;
        chunk->stride = s->stride;
        chunk->size = 0;

        /* Adjust start time */
        if (s->starting) {
            /*
             * If timing data is not available (val != 0), we can at least
             * assume that the playback delay is no less than zero.
             */
            vlc_tick_t pts = (val == 0) ? s->time.pts : now;
            vlc_tick_t gap = s->start - pts;
            vlc_tick_t span = vlc_tick_from_samples(frame_room, s->time.rate);
            size_t skip;

            if (gap >= span) { /* Buffer too early, fill with silence */
                vlc_pw_debug(s->context, "too early to start, silence");
                skip = frame_room;

            } else if (gap >= 0) {
                vlc_pw_debug(s->context, "starting %s time",
                             val ? "without" : "on");
                skip = samples_from_vlc_tick(gap, s->time.rate);
                s->starting = false;

            } else {
                vlc_pw_warn(s->context, "starting late");
                skip = 0;
                s->starting = false;
            }

            skip *= s->stride;
            assert(skip <= room);
            memset(dst, 0, skip);
            dst += skip;
            room -= skip;
        }

        if (!s->starting && s->time.pts != VLC_TICK_INVALID
         && now >= s->time.next_update && s->first_pts != VLC_TICK_INVALID) {
            vlc_tick_t elapsed = now - s->time.pts;
            /* A sample injected now would be delayed by the following amount of vlc_tick_t */
            vlc_tick_t delay = vlc_tick_from_samples(s->time.frames, s->time.rate)
                             - elapsed;
            vlc_tick_t audio_ts = vlc_tick_from_samples(s->time.injected, s->time.rate)
                                + s->first_pts;
            aout_TimingReport(s->aout, now + delay, audio_ts);
            /* Once we have enough points to initiate the clock we can delay the reports */
            if (now >= s->start + VLC_TICK_FROM_SEC(1))
                s->time.next_update = now + VLC_TICK_FROM_SEC(1);
        }

        while ((block = s->queue.head) != NULL) {
            size_t avail = block->i_buffer;
            size_t length = (avail < room) ? avail : room;

            memcpy(dst, block->p_buffer, length);
            block->p_buffer += length;
            block->i_buffer -= length;
            dst += length;
            room -= length;
            chunk->size += length;
            assert((length % s->stride) == 0);
            const size_t written = length / s->stride;
            s->time.injected += written;
            s->queue.size -= length;

            if (block->i_buffer > 0) {
                assert(room == 0);
                break;
            }

            s->queue.head = block->p_next;
            vlc_frame_Release(block);
        }

        if (s->queue.head == NULL)
            s->queue.tailp = &s->queue.head;

        b->size = chunk->size / s->stride;
        pw_stream_queue_buffer(s->stream, b);
    }

    if (s->queue.head == NULL && s->draining) {
        s->first_pts = s->start = VLC_TICK_INVALID;
        s->starting = false;
        s->draining = false;
        pw_stream_flush(s->stream, true);
    }
}

/**
 * Stream drain callback.
 *
 * This monitors the stream for completion of draining.
 */
static void stream_drained(void *data)
{
    struct vlc_pw_stream *s = data;

    vlc_pw_debug(s->context, "stream drained");
    aout_DrainedReport(s->aout);
}

static void stream_trigger_done(void *data)
{
    struct vlc_pw_stream *s = data;

    vlc_pw_debug(s->context, "stream trigger done");
}

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .state_changed = stream_state_changed,
    .control_info = stream_control_info,
    .process = stream_process,
    .drained = stream_drained,
    .trigger_done = stream_trigger_done,
};

static enum pw_stream_state vlc_pw_stream_get_state(struct vlc_pw_stream *s)
{
    /* PW Workaround: The error state can be notified via
     * stream_state_changed() but never returned by pw_stream_get_state() */
    return s->error ? PW_STREAM_STATE_ERROR
                    : pw_stream_get_state(s->stream, NULL);
}

/**
 * Queues an audio buffer for playback.
 */
static void vlc_pw_stream_play(struct vlc_pw_stream *s, vlc_frame_t *block,
                               vlc_tick_t date)
{
    assert((block->i_buffer % s->stride) == 0);
    vlc_pw_lock(s->context);
    if (vlc_pw_stream_get_state(s) == PW_STREAM_STATE_ERROR) {
        vlc_frame_Release(block);
        goto out;
    }

    if (s->start == VLC_TICK_INVALID) {
        /* Upon flush or drain, the stream is implicitly inactivated. This
         * re-activates it. In other cases, this should be a no-op. */
        pw_stream_set_active(s->stream, true);
        assert(!s->starting);
        s->starting = true;
        s->time.next_update = VLC_TICK_0;
        s->first_pts = block->i_pts;
    }
    if (s->starting)
        s->start = date
                 - vlc_tick_from_samples(s->queue.size / s->stride, s->time.rate);

    *(s->queue.tailp) = block;
    s->queue.tailp = &block->p_next;
    s->queue.size += block->i_buffer;
out:
    s->draining = false;
    vlc_pw_unlock(s->context);
}

/**
 * Pauses or resumes the playback.
 */
static void vlc_pw_stream_set_pause(struct vlc_pw_stream *s, bool paused,
                                    vlc_tick_t date)
{
    vlc_pw_lock(s->context);
    pw_stream_set_active(s->stream, !paused);
    s->time.pts = VLC_TICK_INVALID;
    if (unlikely(s->starting)) {
        assert(s->start != VLC_TICK_INVALID);
        if (paused)
            s->start -= date;
        else
            s->start += date;
    }
    vlc_pw_unlock(s->context);
}

/**
 * Flushes (discards) all pending audio buffers.
 */
static void vlc_pw_stream_flush(struct vlc_pw_stream *s)
{
    vlc_pw_lock(s->context);
    vlc_frame_ChainRelease(s->queue.head);
    s->queue.head = NULL;
    s->queue.tailp = &s->queue.head;
    s->queue.size = 0;
    s->time.pts = VLC_TICK_INVALID;
    s->time.injected = 0;
    s->first_pts = s->start = VLC_TICK_INVALID;
    s->starting = false;
    s->draining = false;
    s->error = false;
    pw_stream_flush(s->stream, false);
    vlc_pw_unlock(s->context);
}

/**
 * Starts draining.
 *
 * This flags the start of draining. stream_drained() will be called by the
 * thread loop whence draining completes.
 */
static void vlc_pw_stream_drain(struct vlc_pw_stream *s)
{
    vlc_pw_lock(s->context);
    bool empty = s->start == VLC_TICK_INVALID;
    s->first_pts = s->start = VLC_TICK_INVALID;
    if (vlc_pw_stream_get_state(s) == PW_STREAM_STATE_ERROR || empty)
        stream_drained(s); /* Don't wait on a failed stream */
    else if (s->queue.head == NULL)
        pw_stream_flush(s->stream, true); /* Drain now */
    else
        s->draining = true; /* Let ->process() drain */
    vlc_pw_unlock(s->context);
}

static void vlc_pw_stream_set_volume(struct vlc_pw_stream *s, float vol)
{
    const struct pw_stream_control *old;

    vlc_pw_lock(s->context);
    old = pw_stream_get_control(s->stream, SPA_PROP_channelVolumes);
    if (old != NULL) {
        float values[SPA_AUDIO_MAX_CHANNELS];
        float oldvol = 0.f;

        assert(old->n_values <= ARRAY_SIZE(values));
        /* Try to preserve the balance */
        for (size_t i = 0; i < old->n_values; i++)
            oldvol = fmaxf(oldvol, old->values[i]);

        float delta = vol - oldvol;

        for (size_t i = 0; i < old->n_values; i++)
            values[i] = fmaxf(0.f, old->values[i] + delta);

        pw_stream_set_control(s->stream, SPA_PROP_channelVolumes,
                              old->n_values, values, 0);
    }
    vlc_pw_unlock(s->context);
}

static void vlc_pw_stream_set_mute(struct vlc_pw_stream *s, bool mute)
{
    float value = mute ? 1.f : 0.f;

    vlc_pw_lock(s->context);
    pw_stream_set_control(s->stream, SPA_PROP_mute, 1, &value, 0);
    vlc_pw_unlock(s->context);
}

static void vlc_pw_stream_select_device(struct vlc_pw_stream *s,
                                        const char *name)
{
    struct spa_dict_item items[] = {
        SPA_DICT_ITEM_INIT(PW_KEY_OBJECT_SERIAL, name),
    };
    struct spa_dict dict = SPA_DICT_INIT(items, ARRAY_SIZE(items));

    vlc_pw_debug(s->context, "setting object serial: %s", name);
    vlc_pw_lock(s->context);
    pw_stream_update_properties(s->stream, &dict);
    vlc_pw_unlock(s->context);
}

static void vlc_pw_stream_destroy(struct vlc_pw_stream *s)
{
    vlc_pw_stream_flush(s);
    vlc_pw_lock(s->context);
    pw_stream_disconnect(s->stream);
    pw_stream_destroy(s->stream);
    vlc_pw_unlock(s->context);
    free(s);
}

struct vlc_pw_aout {
    struct vlc_pw_context *context;
    struct vlc_pw_stream *stream;
    struct spa_hook listener;
    void *nodes;
    struct {
        uint64_t target;
        float volume;
        signed char mute;
    } initial;
};

static struct vlc_pw_stream *vlc_pw_stream_create(audio_output_t *aout,
                                           audio_sample_format_t *restrict fmt)
{
    struct vlc_pw_aout *sys = aout->sys;
    struct vlc_pw_context *ctx = sys->context;
    struct spa_audio_info_raw rawfmt = {
        .rate = fmt->i_rate,
        .channels = fmt->i_channels,
    };

    enum spa_audio_iec958_codec encoding = SPA_AUDIO_IEC958_CODEC_PCM; /* PCM by default */

    /* Map possible audio output formats */
    switch (fmt->i_format) {
        case VLC_CODEC_FL64:
            rawfmt.format = SPA_AUDIO_FORMAT_F64;
            break;
        case VLC_CODEC_FL32:
            rawfmt.format = SPA_AUDIO_FORMAT_F32;
            break;
        case VLC_CODEC_S32N:
            rawfmt.format = SPA_AUDIO_FORMAT_S32;
            break;
        case VLC_CODEC_S16N:
            rawfmt.format = SPA_AUDIO_FORMAT_S16;
            break;
        case VLC_CODEC_U8:
            rawfmt.format = SPA_AUDIO_FORMAT_U8;
            break;
        case VLC_CODEC_A52:
            fmt->i_format = VLC_CODEC_SPDIFL;
            fmt->i_bytes_per_frame = 4;
            fmt->i_frame_length = 1;
            fmt->i_physical_channels = AOUT_CHANS_2_0;
            fmt->i_channels = 2;
            encoding = SPA_AUDIO_IEC958_CODEC_AC3;
            break;
        case VLC_CODEC_EAC3:
            fmt->i_format = VLC_CODEC_SPDIFL;
            fmt->i_bytes_per_frame = 4;
            fmt->i_frame_length = 1;
            fmt->i_physical_channels = AOUT_CHANS_2_0;
            fmt->i_channels = 2;
            encoding = SPA_AUDIO_IEC958_CODEC_EAC3;
            break;
        case VLC_CODEC_DTS:
            fmt->i_format = VLC_CODEC_SPDIFL;
            fmt->i_bytes_per_frame = 4;
            fmt->i_frame_length = 1;
            fmt->i_physical_channels = AOUT_CHANS_2_0;
            fmt->i_channels = 2;
            encoding = SPA_AUDIO_IEC958_CODEC_DTS;
            break;
        default:
            vlc_pw_error(sys->context, "unknown format");
            errno = ENOTSUP;
            return NULL;
    }

    /* Map audio channels */
    if (encoding == SPA_AUDIO_IEC958_CODEC_PCM)
    {
        size_t mapped = 0;

        if (fmt->i_channels > SPA_AUDIO_MAX_CHANNELS) {
            vlc_pw_error(sys->context, "too many channels");
            errno = ENOTSUP;
            return NULL;
        }

        static const unsigned char map[] = {
            SPA_AUDIO_CHANNEL_FC,
            SPA_AUDIO_CHANNEL_FL,
            SPA_AUDIO_CHANNEL_FR,
            0 /* unassigned */,
            SPA_AUDIO_CHANNEL_RC,
            SPA_AUDIO_CHANNEL_RL,
            SPA_AUDIO_CHANNEL_RR,
            0 /* unassigned */,
            SPA_AUDIO_CHANNEL_SL,
            SPA_AUDIO_CHANNEL_SR,
            0 /* unassigned */,
            0 /* unassigned */,
            SPA_AUDIO_CHANNEL_LFE,
        };

        for (size_t i = 0; i < ARRAY_SIZE(map); i++)
            if ((fmt->i_physical_channels >> i) & 1)
                rawfmt.position[mapped++] = map[i];

        while (mapped < fmt->i_channels) {
            rawfmt.position[mapped] = SPA_AUDIO_CHANNEL_START_Aux + mapped;
            mapped++;
        }
    }

    /* Assemble the stream format */
    const struct spa_pod *params[1];
    unsigned char buf[1024];
    struct spa_pod_builder builder = SPA_POD_BUILDER_INIT(buf, sizeof (buf));

    if (encoding == SPA_AUDIO_IEC958_CODEC_PCM)
    {
        params[0] = spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat,
                                               &rawfmt);
    }
    else /* pass-through */
    {
        struct spa_audio_info_iec958 iecfmt = {
            .rate = fmt->i_rate,
            .codec = encoding
        };
        params[0] = spa_format_audio_iec958_build(&builder, SPA_PARAM_EnumFormat,
                                                  &iecfmt);
    }

    /* Assemble stream properties */
    struct pw_properties *props;

    props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                              PW_KEY_MEDIA_CATEGORY, "Playback",
                              NULL);

    if (likely(props != NULL)) {
        char *role = var_InheritString(aout, "role");
        if (role != NULL) { /* Capitalise the first character */
            pw_properties_setf(props, PW_KEY_MEDIA_ROLE, "%c%s",
                               toupper((unsigned char)role[0]), role + 1);
            free(role);
        }
    }

    /* Create the stream */
    struct vlc_pw_stream *s = malloc(sizeof (*s));
    if (unlikely(s == NULL))
        return NULL;

    enum pw_stream_flags flags =
        PW_STREAM_FLAG_AUTOCONNECT |
        PW_STREAM_FLAG_MAP_BUFFERS;
        /*PW_STREAM_FLAG_EXCLUSIVE TODO*/
    enum pw_stream_state state;

    s->context = ctx;
    s->listener = (struct spa_hook){ };
    s->stride = fmt->i_bytes_per_frame;
    s->queue.head = NULL;
    s->queue.tailp = &s->queue.head;
    s->queue.size = 0;
    s->time.pts = VLC_TICK_INVALID;
    s->time.rate = fmt->i_rate;
    s->time.injected = 0;
    s->first_pts = s->start = VLC_TICK_INVALID;
    s->starting = false;
    s->draining = false;
    s->error = false;
    s->aout = aout;

    vlc_pw_lock(s->context);
    s->stream = vlc_pw_stream_new(s->context, "audio stream", props);
    if (unlikely(s->stream == NULL)) {
        vlc_pw_unlock(s->context);
        free(s);
        return NULL;
    }

    if (sys->initial.target != SPA_ID_INVALID) {
        char target[21];
        struct spa_dict_item items[] = {
            SPA_DICT_ITEM_INIT(PW_KEY_TARGET_OBJECT, target),
        };
        struct spa_dict dict = SPA_DICT_INIT(items, ARRAY_SIZE(items));

        snprintf(target, sizeof (target), "%"PRIu64, sys->initial.target);
        pw_stream_update_properties(s->stream, &dict);
    }

    pw_stream_add_listener(s->stream, &s->listener, &stream_events, s);
    pw_stream_connect(s->stream, PW_DIRECTION_OUTPUT, PW_ID_ANY, flags,
                      params, ARRAY_SIZE(params));

    if (encoding == SPA_AUDIO_IEC958_CODEC_PCM)
    {
        /* Wait for the PCM stream to be ready */
        while ((state = vlc_pw_stream_get_state(s)) == PW_STREAM_STATE_CONNECTING)
            vlc_pw_wait(s->context);
    }
    else
    {
        /* In case of passthrough, an error can be triggered just after the
         * CONNECTING -> PAUSED state, so wait for the STREAMING state or any
         * errors. */
        while ((state = vlc_pw_stream_get_state(s)) == PW_STREAM_STATE_CONNECTING
             || state == PW_STREAM_STATE_PAUSED)
            vlc_pw_wait(s->context);
    }
    vlc_pw_unlock(s->context);

    switch (state) {
        case PW_STREAM_STATE_PAUSED:
        case PW_STREAM_STATE_STREAMING:
            break;
        default:
            vlc_pw_stream_destroy(s);
            errno = ENOBUFS;
            return NULL;
    }

    /* Apply initial controls */
    sys->initial.target = SPA_ID_INVALID;

    if (sys->initial.mute >= 0) {
        vlc_pw_stream_set_mute(s, sys->initial.mute);
        sys->initial.mute = -1;
    }
    if (!isnan(sys->initial.volume)) {
        vlc_pw_stream_set_volume(s, sys->initial.volume);
        sys->initial.volume = NAN;
    }
    return s;
}

static void Play(audio_output_t *aout, vlc_frame_t *block, vlc_tick_t date)
{
    struct vlc_pw_aout *sys = aout->sys;
    vlc_pw_stream_play(sys->stream, block, date);
}

static void Pause(audio_output_t *aout, bool paused, vlc_tick_t date)
{
    struct vlc_pw_aout *sys = aout->sys;
    vlc_pw_stream_set_pause(sys->stream, paused, date);
}

static void Flush(audio_output_t *aout)
{
    struct vlc_pw_aout *sys = aout->sys;
    vlc_pw_stream_flush(sys->stream);
}

static void Drain(audio_output_t *aout)
{
    struct vlc_pw_aout *sys = aout->sys;
    vlc_pw_stream_drain(sys->stream);
}

static void Stop(audio_output_t *aout)
{
    struct vlc_pw_aout *sys = aout->sys;
    vlc_pw_stream_destroy(sys->stream);
    sys->stream = NULL;
}

static int Start(audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    struct vlc_pw_aout *sys = aout->sys;

    sys->stream = vlc_pw_stream_create(aout, fmt);
    return (sys->stream != NULL) ? VLC_SUCCESS : -errno;
}

static int VolumeSet(audio_output_t *aout, float volume)
{
    struct vlc_pw_aout *sys = aout->sys;

    if (sys->stream != NULL) {
        vlc_pw_stream_set_volume(sys->stream, volume);
        return 0;
    }

    sys->initial.volume = volume;
    aout_VolumeReport(aout, volume);
    return 0;
}

static int MuteSet(audio_output_t *aout, bool mute)
{
    struct vlc_pw_aout *sys = aout->sys;

    if (sys->stream != NULL) {
        vlc_pw_stream_set_mute(sys->stream, mute);
        return 0;
    }

    sys->initial.mute = mute;
    aout_MuteReport(aout, mute);
    return 0;
}

struct vlc_pw_node {
    uint32_t id;
    uint64_t serial;
};

static int node_by_id(const void *a, const void *b)
{
    const struct vlc_pw_node *na = a, *nb = b;

    if (na->id > nb->id)
        return +1;
    if (na->id < nb->id)
        return -1;
    return 0;
}

static int DeviceSelect(audio_output_t *aout, const char *name)
{
    struct vlc_pw_aout *sys = aout->sys;

    if (sys->stream != NULL) {
        vlc_pw_stream_select_device(sys->stream, name);
        return 0;
    }

    sys->initial.target = strtoul(name, NULL, 10);
    aout_DeviceReport(aout, name);
    return 0;
}

static void registry_node(audio_output_t *aout, uint32_t id, uint32_t perms,
                          uint32_t version, const struct spa_dict *props)
{
    struct vlc_pw_aout *sys = aout->sys;
    char serialstr[21];
    uint64_t serial;

    if (props == NULL)
        return;

    const char *class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
    if (class == NULL)
        return;
    if (strcmp(class, "Audio/Sink") && strcmp(class, "Audio/Duplex"))
        return; /* Not an audio output */
    if (!spa_atou64(spa_dict_lookup(props, PW_KEY_OBJECT_SERIAL), &serial, 0))
        return;

    const char *desc = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
    if (unlikely(desc == NULL))
        desc = "???";

    struct vlc_pw_node *node = malloc(sizeof (*node));
    if (unlikely(node == NULL))
        return;

    node->id = id;
    node->serial = serial;

    struct vlc_pw_node **pp = tsearch(node, &sys->nodes, node_by_id);
    if (unlikely(pp == NULL)) { /* Memory allocation error in the tree */
        free(node);
        return;
    }
    if (*pp != node) { /* Existing node, update it */
        free(*pp);
        *pp = node;
    }

    snprintf(serialstr, sizeof (serialstr), "%"PRIu64, serial);
    aout_HotplugReport(aout, serialstr, desc);
    (void) perms; (void) version;
}

/**
 * Global callback.
 *
 * This gets called for every initial, then every new, object from the PipeWire
 * server. We can find the usable playback devices through this.
 */
static void registry_global(void *data, uint32_t id, uint32_t perms,
                            const char *name, uint32_t version,
                            const struct spa_dict *props)
{
    audio_output_t *aout = data;

    if (strcmp(name, PW_TYPE_INTERFACE_Node) == 0)
        registry_node(aout, id, perms, version, props);
}

/**
 * Global removal callback.
 *
 * This gets called when an object disappers. We can detect when a playback
 * device is unplugged here.
 */
static void registry_global_remove(void *data, uint32_t id)
{
    audio_output_t *aout = data;
    struct vlc_pw_aout *sys = aout->sys;
    struct vlc_pw_node key = { .id = id };
    struct vlc_pw_node **pp = tfind(&key, &sys->nodes, node_by_id);

    if (pp != NULL) { /* A known device has been removed */
        struct vlc_pw_node *node = *pp;
        char serialstr[11];

        snprintf(serialstr, sizeof (serialstr), "%"PRIu64, node->serial);
        aout_HotplugReport(aout, serialstr, NULL);
        tdelete(node, &sys->nodes, node_by_id);
        free(node);
    }
}

static const struct pw_registry_events events = {
    PW_VERSION_REGISTRY_EVENTS,
    .global = registry_global,
    .global_remove =  registry_global_remove
};

static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    struct vlc_pw_aout *sys = aout->sys;

    vlc_pw_disconnect(sys->context);
    tdestroy(sys->nodes, free);
    free(sys);
}

static int Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;

    struct vlc_pw_aout *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->context = vlc_pw_connect(obj, "audio output");
    if (sys->context == NULL) {
        free(sys);
        return -errno;
    }

    sys->initial.target = SPA_ID_INVALID;
    sys->initial.volume = NAN;
    sys->initial.mute = -1;
    sys->nodes = NULL;

    aout->sys = sys;
    aout->start = Start;
    aout->stop = Stop;
    aout->time_get = NULL;
    aout->play = Play;
    aout->pause = Pause;
    aout->flush = Flush;
    aout->drain = Drain;
    aout->volume_set = VolumeSet;
    aout->mute_set = MuteSet;
    aout->device_select = DeviceSelect;

    vlc_pw_lock(sys->context);
    vlc_pw_registry_listen(sys->context, &sys->listener, &events, aout);
    vlc_pw_roundtrip_unlocked(sys->context); /* Enumerate device nodes */
    vlc_pw_unlock(sys->context);
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname("PipeWire")
    set_description(N_("PipeWire audio output"))
    set_capability("audio output", 200)
    set_subcategory(SUBCAT_AUDIO_AOUT)
    add_shortcut("pipewire", "pw")
    set_callbacks(Open, Close)
vlc_module_end()
