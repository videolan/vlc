/*****************************************************************************
 * pulse.c : Pulseaudio output plugin for vlc
 *****************************************************************************
 * Copyright (C) 2008 VLC authors and VideoLAN
 * Copyright (C) 2009-2011 Rémi Denis-Courmont
 *
 * Authors: Martin Hamrle <hamrle @ post . cz>
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
#include <math.h>
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_cpu.h>

#include <pulse/pulseaudio.h>
#include "audio_output/vlcpulse.h"

static int  Open        ( vlc_object_t * );
static void Close       ( vlc_object_t * );

vlc_module_begin ()
    set_shortname( "PulseAudio" )
    set_description( N_("Pulseaudio audio output") )
    set_capability( "audio output", 160 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )
    add_shortcut( "pulseaudio", "pa" )
    set_callbacks( Open, Close )
vlc_module_end ()

/* NOTE:
 * Be careful what you do when the PulseAudio mainloop is held, which is to say
 * within PulseAudio callbacks, or after pa_threaded_mainloop_lock().
 * In particular, a VLC variable callback cannot be triggered nor deleted with
 * the PulseAudio mainloop lock held, if the callback acquires the lock. */

struct sink
{
    struct sink *next;
    uint32_t index;
    char name[1];
};

struct aout_sys_t
{
    pa_stream *stream; /**< PulseAudio playback stream object */
    pa_context *context; /**< PulseAudio connection context */
    pa_threaded_mainloop *mainloop; /**< PulseAudio thread */
    pa_time_event *trigger; /**< Deferred stream trigger */
    pa_cvolume cvolume; /**< actual sink input volume */
    mtime_t first_pts; /**< Play time of buffer start */

    pa_volume_t volume_force; /**< Forced volume (stream must be NULL) */
    pa_stream_flags_t flags_force; /**< Forced flags (stream must be NULL) */
    char *sink_force; /**< Forced sink name (stream must be NULL) */

    struct sink *sinks; /**< Locally-cached list of sinks */
};

static void VolumeReport(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;
    pa_volume_t volume = pa_cvolume_max(&sys->cvolume);

    aout_VolumeReport(aout, (float)volume / PA_VOLUME_NORM);
}

/*** Sink ***/
static void sink_add_cb(pa_context *ctx, const pa_sink_info *i, int eol,
                        void *userdata)
{
    audio_output_t *aout = userdata;
    aout_sys_t *sys = aout->sys;

    if (eol)
    {
        pa_threaded_mainloop_signal(sys->mainloop, 0);
        return;
    }
    (void) ctx;

    msg_Dbg(aout, "adding sink %"PRIu32": %s (%s)", i->index, i->name,
            i->description);
    aout_HotplugReport(aout, i->name, i->description);

    size_t namelen = strlen(i->name);
    struct sink *sink = malloc(sizeof (*sink) + namelen);
    if (unlikely(sink == NULL))
        return;

    sink->next = sys->sinks;
    sink->index = i->index;
    memcpy(sink->name, i->name, namelen + 1);
    sys->sinks = sink;
}

static void sink_mod_cb(pa_context *ctx, const pa_sink_info *i, int eol,
                        void *userdata)
{
    audio_output_t *aout = userdata;

    if (eol)
        return;
    (void) ctx;

    msg_Dbg(aout, "changing sink %"PRIu32": %s (%s)", i->index, i->name,
            i->description);
    aout_HotplugReport(aout, i->name, i->description);
}

static void sink_del(uint32_t index, audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;
    struct sink **pp = &sys->sinks, *sink;

    msg_Dbg(aout, "removing sink %"PRIu32, index);

    while ((sink = *pp) != NULL)
        if (sink->index == index)
        {
            *pp = sink->next;
            aout_HotplugReport(aout, sink->name, NULL);
            free(sink);
        }
        else
            pp = &sink->next;
}

static void sink_event(pa_context *ctx, unsigned type, uint32_t idx,
                       audio_output_t *aout)
{
    pa_operation *op = NULL;

    switch (type)
    {
        case PA_SUBSCRIPTION_EVENT_NEW:
            op = pa_context_get_sink_info_by_index(ctx, idx, sink_add_cb,
                                                   aout);
            break;
        case PA_SUBSCRIPTION_EVENT_CHANGE:
            op = pa_context_get_sink_info_by_index(ctx, idx, sink_mod_cb,
                                                   aout);
            break;
        case PA_SUBSCRIPTION_EVENT_REMOVE:
            sink_del(idx, aout);
            break;
    }
    if (op != NULL)
        pa_operation_unref(op);
}


/*** Latency management and lip synchronization ***/
static void stream_start_now(pa_stream *s, audio_output_t *aout)
{
    pa_operation *op;

    assert (aout->sys->trigger == NULL);

    op = pa_stream_cork(s, 0, NULL, NULL);
    if (op != NULL)
        pa_operation_unref(op);
    op = pa_stream_trigger(s, NULL, NULL);
    if (likely(op != NULL))
        pa_operation_unref(op);
}

static void stream_stop(pa_stream *s, audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;
    pa_operation *op;

    if (sys->trigger != NULL) {
        vlc_pa_rttime_free(sys->mainloop, sys->trigger);
        sys->trigger = NULL;
    }

    op = pa_stream_cork(s, 1, NULL, NULL);
    if (op != NULL)
        pa_operation_unref(op);
}

static void stream_trigger_cb(pa_mainloop_api *api, pa_time_event *e,
                              const struct timeval *tv, void *userdata)
{
    audio_output_t *aout = userdata;
    aout_sys_t *sys = aout->sys;

    assert (sys->trigger == e);

    msg_Dbg(aout, "starting deferred");
    vlc_pa_rttime_free(sys->mainloop, sys->trigger);
    sys->trigger = NULL;
    stream_start_now(sys->stream, aout);
    (void) api; (void) e; (void) tv;
}

/**
 * Starts or resumes the playback stream.
 * Tries start playing back audio samples at the most accurate time
 * in order to minimize desync and resampling during early playback.
 * @note PulseAudio lock required.
 */
static void stream_start(pa_stream *s, audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;
    mtime_t delta;

    assert (sys->first_pts != VLC_TS_INVALID);

    if (sys->trigger != NULL) {
        vlc_pa_rttime_free(sys->mainloop, sys->trigger);
        sys->trigger = NULL;
    }

    delta = vlc_pa_get_latency(aout, sys->context, s);
    if (unlikely(delta == VLC_TS_INVALID)) {
        msg_Dbg(aout, "cannot synchronize start");
        delta = 0; /* screwed */
    }

    delta = (sys->first_pts - mdate()) - delta;
    if (delta > 0) {
        msg_Dbg(aout, "deferring start (%"PRId64" us)", delta);
        delta += pa_rtclock_now();
        sys->trigger = pa_context_rttime_new(sys->context, delta,
                                             stream_trigger_cb, aout);
    } else {
        msg_Warn(aout, "starting late (%"PRId64" us)", delta);
        stream_start_now(s, aout);
    }
}

static void stream_latency_cb(pa_stream *s, void *userdata)
{
    audio_output_t *aout = userdata;
    aout_sys_t *sys = aout->sys;

    /* This callback is _never_ called while paused. */
    if (sys->first_pts == VLC_TS_INVALID)
        return; /* nothing to do if buffers are (still) empty */
    if (pa_stream_is_corked(s) > 0)
        stream_start(s, aout);
}


/*** Stream helpers ***/
static void stream_state_cb(pa_stream *s, void *userdata)
{
    pa_threaded_mainloop *mainloop = userdata;

    switch (pa_stream_get_state(s)) {
        case PA_STREAM_READY:
        case PA_STREAM_FAILED:
        case PA_STREAM_TERMINATED:
            pa_threaded_mainloop_signal(mainloop, 0);
        default:
            break;
    }
}

static void stream_buffer_attr_cb(pa_stream *s, void *userdata)
{
    audio_output_t *aout = userdata;
    const pa_buffer_attr *pba = pa_stream_get_buffer_attr(s);

    msg_Dbg(aout, "changed buffer metrics: maxlength=%u, tlength=%u, "
            "prebuf=%u, minreq=%u",
            pba->maxlength, pba->tlength, pba->prebuf, pba->minreq);
}

static void stream_event_cb(pa_stream *s, const char *name, pa_proplist *pl,
                            void *userdata)
{
    audio_output_t *aout = userdata;

    if (!strcmp(name, PA_STREAM_EVENT_REQUEST_CORK))
        aout_PolicyReport(aout, true);
    else
    if (!strcmp(name, PA_STREAM_EVENT_REQUEST_UNCORK))
        aout_PolicyReport(aout, false);
    else
    /* FIXME: expose aout_Restart() directly */
    if (!strcmp(name, PA_STREAM_EVENT_FORMAT_LOST)) {
        msg_Dbg (aout, "format lost");
        aout_RestartRequest (aout, AOUT_RESTART_OUTPUT);
    } else
        msg_Warn (aout, "unhandled stream event \"%s\"", name);
    (void) s;
    (void) pl;
}

static void stream_moved_cb(pa_stream *s, void *userdata)
{
    audio_output_t *aout = userdata;
    const char *name = pa_stream_get_device_name(s);

    msg_Dbg(aout, "connected to sink %s", name);
    aout_DeviceReport(aout, name);
}

static void stream_overflow_cb(pa_stream *s, void *userdata)
{
    audio_output_t *aout = userdata;
    aout_sys_t *sys = aout->sys;
    pa_operation *op;

    msg_Err(aout, "overflow, flushing");
    op = pa_stream_flush(s, NULL, NULL);
    if (unlikely(op == NULL))
        return;
    pa_operation_unref(op);
    sys->first_pts = VLC_TS_INVALID;
}

static void stream_started_cb(pa_stream *s, void *userdata)
{
    audio_output_t *aout = userdata;

    msg_Dbg(aout, "started");
    (void) s;
}

static void stream_suspended_cb(pa_stream *s, void *userdata)
{
    audio_output_t *aout = userdata;

    msg_Dbg(aout, "suspended");
    (void) s;
}

static void stream_underflow_cb(pa_stream *s, void *userdata)
{
    audio_output_t *aout = userdata;

    msg_Dbg(aout, "underflow");
    (void) s;
}

static int stream_wait(pa_stream *stream, pa_threaded_mainloop *mainloop)
{
    pa_stream_state_t state;

    while ((state = pa_stream_get_state(stream)) != PA_STREAM_READY) {
        if (state == PA_STREAM_FAILED || state == PA_STREAM_TERMINATED)
            return -1;
        pa_threaded_mainloop_wait(mainloop);
    }
    return 0;
}


/*** Sink input ***/
static void sink_input_info_cb(pa_context *ctx, const pa_sink_input_info *i,
                               int eol, void *userdata)
{
    audio_output_t *aout = userdata;
    aout_sys_t *sys = aout->sys;

    if (eol)
        return;
    (void) ctx;

    sys->cvolume = i->volume; /* cache volume for balance preservation */
    VolumeReport(aout);
    aout_MuteReport(aout, i->mute);
}

static void sink_input_event(pa_context *ctx,
                             pa_subscription_event_type_t type,
                             uint32_t idx, audio_output_t *aout)
{
    pa_operation *op;

    /* Gee... PA will not provide the infos directly in the event. */
    switch (type)
    {
        case PA_SUBSCRIPTION_EVENT_REMOVE:
            msg_Err(aout, "sink input killed!");
            break;

        default:
            op = pa_context_get_sink_input_info(ctx, idx, sink_input_info_cb,
                                                aout);
            if (likely(op != NULL))
                pa_operation_unref(op);
            break;
    }
}


/*** Context ***/
static void context_cb(pa_context *ctx, pa_subscription_event_type_t type,
                       uint32_t idx, void *userdata)
{
    audio_output_t *aout = userdata;
    aout_sys_t *sys = aout->sys;
    unsigned facility = type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;

    type &= PA_SUBSCRIPTION_EVENT_TYPE_MASK;
    switch (facility)
    {
        case PA_SUBSCRIPTION_EVENT_SINK:
            sink_event(ctx, type, idx, userdata);
            break;

        case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
            /* only interested in our sink input */
            if (sys->stream != NULL && idx == pa_stream_get_index(sys->stream))
                sink_input_event(ctx, type, idx, userdata);
            break;

        default: /* unsubscribed facility?! */
            vlc_assert_unreachable();
    }
}


/*** VLC audio output callbacks ***/

static int TimeGet(audio_output_t *aout, mtime_t *restrict delay)
{
    aout_sys_t *sys = aout->sys;
    pa_stream *s = sys->stream;
    int ret = -1;

    pa_threaded_mainloop_lock(sys->mainloop);
    if (pa_stream_is_corked(s) <= 0)
    {   /* latency is relevant only if not corked */
        mtime_t delta = vlc_pa_get_latency(aout, sys->context, s);
        if (delta != VLC_TS_INVALID)
        {
            *delay = delta;
            ret = 0;
        }
    }
    pa_threaded_mainloop_unlock(sys->mainloop);
    return ret;
}

/* Memory free callback. The block_t address is in front of the data. */
static void data_free(void *data)
{
    block_t **pp = data, *block;

    memcpy(&block, pp - 1, sizeof (block));
    block_Release(block);
}

static void *data_convert(block_t **pp)
{
    block_t *block = *pp;
    /* In most cases, there is enough head room, and this is really cheap: */
    block = block_Realloc(block, sizeof (block), block->i_buffer);
    *pp = block;
    if (unlikely(block == NULL))
        return NULL;

    memcpy(block->p_buffer, &block, sizeof (block));
    block->p_buffer += sizeof (block);
    block->i_buffer -= sizeof (block);
    return block->p_buffer;
}

/**
 * Queue one audio frame to the playback stream
 */
static void Play(audio_output_t *aout, block_t *block)
{
    aout_sys_t *sys = aout->sys;
    pa_stream *s = sys->stream;

    const void *ptr = data_convert(&block);
    if (unlikely(ptr == NULL))
        return;

    size_t len = block->i_buffer;

    /* Note: The core already holds the output FIFO lock at this point.
     * Therefore we must not under any circumstances (try to) acquire the
     * output FIFO lock while the PulseAudio threaded main loop lock is held
     * (including from PulseAudio stream callbacks). Otherwise lock inversion
     * will take place, and sooner or later a deadlock. */
    pa_threaded_mainloop_lock(sys->mainloop);

    if (sys->first_pts == VLC_TS_INVALID)
        sys->first_pts = block->i_pts;

    if (pa_stream_is_corked(s) > 0)
        stream_start(s, aout);

#if 0 /* Fault injector to test underrun recovery */
    static volatile unsigned u = 0;
    if ((++u % 1000) == 0) {
        msg_Err(aout, "fault injection");
        pa_operation_unref(pa_stream_flush(s, NULL, NULL));
    }
#endif

    if (pa_stream_write(s, ptr, len, data_free, 0, PA_SEEK_RELATIVE) < 0) {
        vlc_pa_error(aout, "cannot write", sys->context);
        block_Release(block);
    }

    pa_threaded_mainloop_unlock(sys->mainloop);
}

/**
 * Cork or uncork the playback stream
 */
static void Pause(audio_output_t *aout, bool paused, mtime_t date)
{
    aout_sys_t *sys = aout->sys;
    pa_stream *s = sys->stream;

    pa_threaded_mainloop_lock(sys->mainloop);

    if (paused) {
        pa_stream_set_latency_update_callback(s, NULL, NULL);
        stream_stop(s, aout);
    } else {
        pa_stream_set_latency_update_callback(s, stream_latency_cb, aout);
        if (likely(sys->first_pts != VLC_TS_INVALID))
            stream_start_now(s, aout);
    }

    pa_threaded_mainloop_unlock(sys->mainloop);
    (void) date;
}

/**
 * Flush or drain the playback stream
 */
static void Flush(audio_output_t *aout, bool wait)
{
    aout_sys_t *sys = aout->sys;
    pa_stream *s = sys->stream;
    pa_operation *op;

    pa_threaded_mainloop_lock(sys->mainloop);

    if (wait)
    {
        op = pa_stream_drain(s, NULL, NULL);

        /* XXX: Loosy drain emulation.
         * See #18141: drain callback is never received */
        mtime_t delay;
        if (TimeGet(aout, &delay) == 0 && delay <= INT64_C(5000000))
            msleep(delay);
    }
    else
        op = pa_stream_flush(s, NULL, NULL);
    if (op != NULL)
        pa_operation_unref(op);
    sys->first_pts = VLC_TS_INVALID;
    stream_stop(s, aout);

    pa_threaded_mainloop_unlock(sys->mainloop);
}

static int VolumeSet(audio_output_t *aout, float vol)
{
    aout_sys_t *sys = aout->sys;
    pa_stream *s = sys->stream;
    pa_operation *op;
    pa_volume_t volume;

    /* VLC provides the software volume so convert directly to PulseAudio
     * software volume, pa_volume_t. This is not a linear amplification factor
     * so do not use PulseAudio linear amplification! */
    vol *= PA_VOLUME_NORM;
    if (unlikely(vol >= (float)PA_VOLUME_MAX))
        volume = PA_VOLUME_MAX;
    else
        volume = lroundf(vol);

    if (s == NULL)
    {
        sys->volume_force = volume;
        aout_VolumeReport(aout, (float)volume / (float)PA_VOLUME_NORM);
        return 0;
    }

    pa_threaded_mainloop_lock(sys->mainloop);

    if (!pa_cvolume_valid(&sys->cvolume))
    {
        const pa_sample_spec *ss = pa_stream_get_sample_spec(s);

        msg_Warn(aout, "balance clobbered by volume change");
        pa_cvolume_set(&sys->cvolume, ss->channels, PA_VOLUME_NORM);
    }

    /* Preserve the balance (VLC does not support it). */
    pa_cvolume cvolume = sys->cvolume;
    pa_cvolume_scale(&cvolume, PA_VOLUME_NORM);
    pa_sw_cvolume_multiply_scalar(&cvolume, &cvolume, volume);
    assert(pa_cvolume_valid(&cvolume));

    op = pa_context_set_sink_input_volume(sys->context, pa_stream_get_index(s),
                                          &cvolume, NULL, NULL);
    if (likely(op != NULL))
        pa_operation_unref(op);
    pa_threaded_mainloop_unlock(sys->mainloop);
    return likely(op != NULL) ? 0 : -1;
}

static int MuteSet(audio_output_t *aout, bool mute)
{
    aout_sys_t *sys = aout->sys;

    if (sys->stream == NULL)
    {
        sys->flags_force &= ~(PA_STREAM_START_MUTED|PA_STREAM_START_UNMUTED);
        sys->flags_force |=
            mute ? PA_STREAM_START_MUTED : PA_STREAM_START_UNMUTED;
        aout_MuteReport(aout, mute);
        return 0;
    }

    pa_operation *op;
    uint32_t idx = pa_stream_get_index(sys->stream);
    pa_threaded_mainloop_lock(sys->mainloop);
    op = pa_context_set_sink_input_mute(sys->context, idx, mute, NULL, NULL);
    if (likely(op != NULL))
        pa_operation_unref(op);
    pa_threaded_mainloop_unlock(sys->mainloop);

    return 0;
}

static int StreamMove(audio_output_t *aout, const char *name)
{
    aout_sys_t *sys = aout->sys;

    if (sys->stream == NULL)
    {
        msg_Dbg(aout, "will connect to sink %s", name);
        free(sys->sink_force);
        sys->sink_force = strdup(name);
        aout_DeviceReport(aout, name);
        return 0;
    }

    pa_operation *op;
    uint32_t idx = pa_stream_get_index(sys->stream);

    pa_threaded_mainloop_lock(sys->mainloop);
    op = pa_context_move_sink_input_by_name(sys->context, idx, name,
                                            NULL, NULL);
    if (likely(op != NULL)) {
        pa_operation_unref(op);
        msg_Dbg(aout, "moving to sink %s", name);
    } else
        vlc_pa_error(aout, "cannot move sink input", sys->context);
    pa_threaded_mainloop_unlock(sys->mainloop);

    return (op != NULL) ? 0 : -1;
}

static void Stop(audio_output_t *);

static int strcmp_void(const void *a, const void *b)
{
    const char *const *entry = b;
    return strcmp(a, *entry);
}

static const char *str_map(const char *key, const char *const table[][2],
                           size_t n)
{
     const char **r = bsearch(key, table, n, sizeof (*table), strcmp_void);
     return (r != NULL) ? r[1] : NULL;
}

/**
 * Create a PulseAudio playback stream, a.k.a. a sink input.
 */
static int Start(audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    aout_sys_t *sys = aout->sys;

    /* Sample format specification */
    struct pa_sample_spec ss = { .format = PA_SAMPLE_INVALID };
    pa_encoding_t encoding = PA_ENCODING_PCM;

    switch (fmt->i_format)
    {
        case VLC_CODEC_FL64:
            fmt->i_format = VLC_CODEC_FL32;
            /* fall through */
        case VLC_CODEC_FL32:
            ss.format = PA_SAMPLE_FLOAT32NE;
            break;
        case VLC_CODEC_S32N:
            ss.format = PA_SAMPLE_S32NE;
            break;
        case VLC_CODEC_S16N:
            ss.format = PA_SAMPLE_S16NE;
            break;
        case VLC_CODEC_U8:
            ss.format = PA_SAMPLE_U8;
            break;
        case VLC_CODEC_A52:
            fmt->i_format = VLC_CODEC_SPDIFL;
            fmt->i_bytes_per_frame = 4;
            fmt->i_frame_length = 1;
            fmt->i_physical_channels = AOUT_CHANS_2_0;
            fmt->i_channels = 2;
            encoding = PA_ENCODING_AC3_IEC61937;
            ss.format = PA_SAMPLE_S16NE;
            break;
        case VLC_CODEC_EAC3:
            fmt->i_format = VLC_CODEC_SPDIFL;
            fmt->i_bytes_per_frame = 4;
            fmt->i_frame_length = 1;
            fmt->i_physical_channels = AOUT_CHANS_2_0;
            fmt->i_channels = 2;
            encoding = PA_ENCODING_EAC3_IEC61937;
            ss.format = PA_SAMPLE_S16NE;
            break;
        /* case VLC_CODEC_MPGA:
            fmt->i_format = VLC_CODEC_SPDIFL FIXME;
            encoding = PA_ENCODING_MPEG_IEC61937;
            break;*/
        case VLC_CODEC_DTS:
            fmt->i_format = VLC_CODEC_SPDIFL;
            fmt->i_bytes_per_frame = 4;
            fmt->i_frame_length = 1;
            fmt->i_physical_channels = AOUT_CHANS_2_0;
            fmt->i_channels = 2;
            encoding = PA_ENCODING_DTS_IEC61937;
            ss.format = PA_SAMPLE_S16NE;
            break;
        default:
            if (!AOUT_FMT_LINEAR(fmt) || aout_FormatNbChannels(fmt) == 0)
                return VLC_EGENERIC;

            if (HAVE_FPU)
            {
                fmt->i_format = VLC_CODEC_FL32;
                ss.format = PA_SAMPLE_FLOAT32NE;
            }
            else
            {
                fmt->i_format = VLC_CODEC_S16N;
                ss.format = PA_SAMPLE_S16NE;
            }
            break;
    }

    ss.rate = fmt->i_rate;
    ss.channels = fmt->i_channels;
    if (!pa_sample_spec_valid(&ss)) {
        msg_Err(aout, "unsupported sample specification");
        return VLC_EGENERIC;
    }

    /* Stream parameters */
    pa_stream_flags_t flags = sys->flags_force
                            | PA_STREAM_START_CORKED
                            | PA_STREAM_INTERPOLATE_TIMING
                            | PA_STREAM_NOT_MONOTONIC
                            | PA_STREAM_AUTO_TIMING_UPDATE
                            | PA_STREAM_FIX_RATE;

    struct pa_buffer_attr attr;
    attr.maxlength = -1;
    /* PulseAudio goes berserk if the target length (tlength) is not
     * significantly longer than 2 periods (minreq), or when the period length
     * is unspecified and the target length is short. */
    attr.tlength = pa_usec_to_bytes(3 * AOUT_MIN_PREPARE_TIME, &ss);
    attr.prebuf = 0; /* trigger manually */
    attr.minreq = pa_usec_to_bytes(AOUT_MIN_PREPARE_TIME, &ss);
    attr.fragsize = 0; /* not used for output */

    pa_cvolume *cvolume = NULL, cvolumebuf;
    if (PA_VOLUME_IS_VALID(sys->volume_force))
    {
        cvolume = &cvolumebuf;
        pa_cvolume_set(cvolume, ss.channels, sys->volume_force);
    }

    sys->trigger = NULL;
    pa_cvolume_init(&sys->cvolume);
    sys->first_pts = VLC_TS_INVALID;

    pa_format_info *formatv = pa_format_info_new();
    formatv->encoding = encoding;
    pa_format_info_set_rate(formatv, ss.rate);
    if (ss.format != PA_SAMPLE_INVALID)
        pa_format_info_set_sample_format(formatv, ss.format);

    if (fmt->channel_type == AUDIO_CHANNEL_TYPE_AMBISONICS)
    {
        fmt->channel_type = AUDIO_CHANNEL_TYPE_BITMAP;

        /* Setup low latency in order to quickly react to ambisonics
         * filters viewpoint changes. */
        flags |= PA_STREAM_ADJUST_LATENCY;
        attr.tlength = pa_usec_to_bytes(3 * AOUT_MIN_PREPARE_TIME, &ss);
    }

    if (encoding != PA_ENCODING_PCM)
    {
        pa_format_info_set_channels(formatv, ss.channels);

        /* FIX flags are only permitted for PCM, and there is no way to pass
         * different flags for different formats... */
        flags &= ~(PA_STREAM_FIX_FORMAT
                 | PA_STREAM_FIX_RATE
                 | PA_STREAM_FIX_CHANNELS);
    }
    else
    {
        /* Channel mapping (order defined in vlc_aout.h) */
        struct pa_channel_map map;
        map.channels = 0;

        if (fmt->i_physical_channels & AOUT_CHAN_LEFT)
            map.map[map.channels++] = PA_CHANNEL_POSITION_FRONT_LEFT;
        if (fmt->i_physical_channels & AOUT_CHAN_RIGHT)
            map.map[map.channels++] = PA_CHANNEL_POSITION_FRONT_RIGHT;
        if (fmt->i_physical_channels & AOUT_CHAN_MIDDLELEFT)
            map.map[map.channels++] = PA_CHANNEL_POSITION_SIDE_LEFT;
        if (fmt->i_physical_channels & AOUT_CHAN_MIDDLERIGHT)
            map.map[map.channels++] = PA_CHANNEL_POSITION_SIDE_RIGHT;
        if (fmt->i_physical_channels & AOUT_CHAN_REARLEFT)
            map.map[map.channels++] = PA_CHANNEL_POSITION_REAR_LEFT;
        if (fmt->i_physical_channels & AOUT_CHAN_REARRIGHT)
            map.map[map.channels++] = PA_CHANNEL_POSITION_REAR_RIGHT;
        if (fmt->i_physical_channels & AOUT_CHAN_REARCENTER)
            map.map[map.channels++] = PA_CHANNEL_POSITION_REAR_CENTER;
        if (fmt->i_physical_channels & AOUT_CHAN_CENTER)
        {
            if (ss.channels == 1)
                map.map[map.channels++] = PA_CHANNEL_POSITION_MONO;
            else
                map.map[map.channels++] = PA_CHANNEL_POSITION_FRONT_CENTER;
        }
        if (fmt->i_physical_channels & AOUT_CHAN_LFE)
            map.map[map.channels++] = PA_CHANNEL_POSITION_LFE;

        static_assert(AOUT_CHAN_MAX == 9, "Missing channels");

        for (unsigned i = 0; map.channels < ss.channels; i++) {
            map.map[map.channels++] = PA_CHANNEL_POSITION_AUX0 + i;
            msg_Warn(aout, "mapping channel %"PRIu8" to AUX%u", map.channels, i);
        }

        if (!pa_channel_map_valid(&map)) {
            msg_Err(aout, "unsupported channel map");
            return VLC_EGENERIC;
        } else {
            const char *name = pa_channel_map_to_name(&map);
            msg_Dbg(aout, "using %s channel map", (name != NULL) ? name : "?");
        }

        pa_format_info_set_channels(formatv, ss.channels);
        pa_format_info_set_channel_map(formatv, &map);
    }

    /* Create a playback stream */
    pa_proplist *props = pa_proplist_new();
    if (likely(props != NULL))
    {
        /* TODO: set other stream properties */
        char *str = var_InheritString(aout, "role");
        if (str != NULL)
        {
            static const char *const role_map[][2] = {
                { "accessibility", "a11y"       },
                { "animation",     "animation"  },
                { "communication", "phone"      },
                { "game",          "game"       },
                { "music",         "music"      },
                { "notification",  "event"      },
                { "production",    "production" },
                { "test",          "test"       },
                { "video",         "video"      },
            };
            const char *role = str_map(str, role_map, ARRAY_SIZE(role_map));
            if (role != NULL)
                pa_proplist_sets(props, PA_PROP_MEDIA_ROLE, role);
            free(str);
       }
    }

    pa_threaded_mainloop_lock(sys->mainloop);
    pa_stream *s = pa_stream_new_extended(sys->context, "audio stream",
                                          &formatv, 1, props);

    if (likely(props != NULL))
        pa_proplist_free(props);
    pa_format_info_free(formatv);

    if (s == NULL) {
        pa_threaded_mainloop_unlock(sys->mainloop);
        vlc_pa_error(aout, "stream creation failure", sys->context);
        return VLC_EGENERIC;
    }
    assert(sys->stream == NULL);
    sys->stream = s;
    pa_stream_set_state_callback(s, stream_state_cb, sys->mainloop);
    pa_stream_set_buffer_attr_callback(s, stream_buffer_attr_cb, aout);
    pa_stream_set_event_callback(s, stream_event_cb, aout);
    pa_stream_set_latency_update_callback(s, stream_latency_cb, aout);
    pa_stream_set_moved_callback(s, stream_moved_cb, aout);
    pa_stream_set_overflow_callback(s, stream_overflow_cb, aout);
    pa_stream_set_started_callback(s, stream_started_cb, aout);
    pa_stream_set_suspended_callback(s, stream_suspended_cb, aout);
    pa_stream_set_underflow_callback(s, stream_underflow_cb, aout);

    if (pa_stream_connect_playback(s, sys->sink_force, &attr, flags,
                                   cvolume, NULL) < 0
     || stream_wait(s, sys->mainloop)) {
        if (encoding != PA_ENCODING_PCM)
            vlc_pa_error(aout, "digital pass-through stream connection failure",
                         sys->context);
        else
            vlc_pa_error(aout, "stream connection failure", sys->context);
        goto fail;
    }
    sys->volume_force = PA_VOLUME_INVALID;
    sys->flags_force = PA_STREAM_NOFLAGS;
    free(sys->sink_force);
    sys->sink_force = NULL;

    if (encoding == PA_ENCODING_PCM)
    {
        const struct pa_sample_spec *spec = pa_stream_get_sample_spec(s);
        fmt->i_rate = spec->rate;
    }

    stream_buffer_attr_cb(s, aout);
    stream_moved_cb(s, aout);
    pa_threaded_mainloop_unlock(sys->mainloop);

    return VLC_SUCCESS;

fail:
    pa_threaded_mainloop_unlock(sys->mainloop);
    Stop(aout);
    return VLC_EGENERIC;
}

/**
 * Removes a PulseAudio playback stream
 */
static void Stop(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;
    pa_stream *s = sys->stream;

    pa_threaded_mainloop_lock(sys->mainloop);
    if (unlikely(sys->trigger != NULL))
        vlc_pa_rttime_free(sys->mainloop, sys->trigger);
    pa_stream_disconnect(s);

    /* Clear all callbacks */
    pa_stream_set_state_callback(s, NULL, NULL);
    pa_stream_set_buffer_attr_callback(s, NULL, NULL);
    pa_stream_set_event_callback(s, NULL, NULL);
    pa_stream_set_latency_update_callback(s, NULL, NULL);
    pa_stream_set_moved_callback(s, NULL, NULL);
    pa_stream_set_overflow_callback(s, NULL, NULL);
    pa_stream_set_started_callback(s, NULL, NULL);
    pa_stream_set_suspended_callback(s, NULL, NULL);
    pa_stream_set_underflow_callback(s, NULL, NULL);

    pa_stream_unref(s);
    sys->stream = NULL;
    pa_threaded_mainloop_unlock(sys->mainloop);
}

static int Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = malloc(sizeof (*sys));
    pa_operation *op;

    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    /* Allocate structures */
    pa_context *ctx = vlc_pa_connect(obj, &sys->mainloop);
    if (ctx == NULL)
    {
        free(sys);
        return VLC_EGENERIC;
    }
    sys->stream = NULL;
    sys->context = ctx;
    sys->volume_force = PA_VOLUME_INVALID;
    sys->flags_force = PA_STREAM_NOFLAGS;
    sys->sink_force = NULL;
    sys->sinks = NULL;

    aout->sys = sys;
    aout->start = Start;
    aout->stop = Stop;
    aout->time_get = TimeGet;
    aout->play = Play;
    aout->pause = Pause;
    aout->flush = Flush;
    aout->volume_set = VolumeSet;
    aout->mute_set = MuteSet;
    aout->device_select = StreamMove;

    pa_threaded_mainloop_lock(sys->mainloop);
    /* Sinks (output devices) list */
    op = pa_context_get_sink_info_list(sys->context, sink_add_cb, aout);
    if (likely(op != NULL))
    {
        while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
            pa_threaded_mainloop_wait(sys->mainloop);
        pa_operation_unref(op);
    }

    /* Context events */
    const pa_subscription_mask_t mask = PA_SUBSCRIPTION_MASK_SINK
                                      | PA_SUBSCRIPTION_MASK_SINK_INPUT;
    pa_context_set_subscribe_callback(sys->context, context_cb, aout);
    op = pa_context_subscribe(sys->context, mask, NULL, NULL);
    if (likely(op != NULL))
       pa_operation_unref(op);
    pa_threaded_mainloop_unlock(sys->mainloop);

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;
    pa_context *ctx = sys->context;

    pa_threaded_mainloop_lock(sys->mainloop);
    pa_context_set_subscribe_callback(sys->context, NULL, NULL);
    pa_threaded_mainloop_unlock(sys->mainloop);
    vlc_pa_disconnect(obj, ctx, sys->mainloop);

    for (struct sink *sink = sys->sinks, *next; sink != NULL; sink = next)
    {
        next = sink->next;
        free(sink);
    }
    free(sys->sink_force);
    free(sys);
}
