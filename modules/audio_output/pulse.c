/*****************************************************************************
 * pulse.c : Pulseaudio output plugin for vlc
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * Copyright (C) 2009-2011 Rémi Denis-Courmont
 *
 * Authors: Martin Hamrle <hamrle @ post . cz>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_aout_intf.h>
#include <vlc_cpu.h>

#include <pulse/pulseaudio.h>

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

/* TODO:
 * - single static mainloop
 * - pause input on policy event
 * - resample to compensate for long term drift
 * - select music or video stream property correctly (?)
 * - set further appropriate stream properties
 * - update output devices list dynamically
 */

/* NOTE:
 * Be careful what you do when the PulseAudio mainloop is held, which is to say
 * within PulseAudio callbacks, or after pa_threaded_mainloop_lock().
 * In particular, a VLC variable callback cannot be triggered nor deleted with
 * the PulseAudio mainloop lock held, if the callback acquires the lock. */

struct aout_sys_t
{
    pa_stream *stream; /**< PulseAudio playback stream object */
    pa_context *context; /**< PulseAudio connection context */
    pa_threaded_mainloop *mainloop; /**< PulseAudio event loop */
    pa_volume_t base_volume; /**< 0dB reference volume */
    pa_cvolume cvolume; /**< actual sink input volume */
};

/*** Context helpers ***/
static void context_state_cb(pa_context *c, void *userdata)
{
    pa_threaded_mainloop *mainloop = userdata;

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_READY:
        case PA_CONTEXT_FAILED:
        case PA_CONTEXT_TERMINATED:
            pa_threaded_mainloop_signal(mainloop, 0);
        default:
            break;
    }
}

static bool context_wait(pa_threaded_mainloop *mainloop, pa_context *context)
{
    pa_context_state_t state;

    while ((state = pa_context_get_state(context)) != PA_CONTEXT_READY) {
        if (state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED)
            return -1;
        pa_threaded_mainloop_wait(mainloop);
    }
    return 0;
}

static void error(aout_instance_t *aout, const char *msg, pa_context *context)
{
    msg_Err(aout, "%s: %s", msg, pa_strerror(pa_context_errno(context)));
}

/*** Sink ***/
static void sink_list_cb(pa_context *c, const pa_sink_info *i, int eol,
                         void *userdata)
{
    aout_instance_t *aout = userdata;
    vlc_value_t val, text;

    if (eol)
        return;
    (void) c;

    msg_Dbg(aout, "listing sink %s (%"PRIu32"): %s", i->name, i->index,
            i->description);
    val.i_int = i->index;
    text.psz_string = (char *)i->description;
    var_Change(aout, "audio-device", VLC_VAR_ADDCHOICE, &val, &text);
}

static void sink_info_cb(pa_context *c, const pa_sink_info *i, int eol,
                         void *userdata)
{
    aout_instance_t *aout = userdata;
    aout_sys_t *sys = aout->output.p_sys;

    if (eol)
        return;
    (void) c;

    /* PulseAudio flat volume NORM / 100% / 0dB corresponds to no software
     * amplification and maximum hardware amplification.
     * VLC maps DEFAULT / 100% to no gain at all (software/hardware).
     * Thus we need to use the sink base_volume as a multiplier,
     * if and only if flat volume is active for our current sink. */
    if (i->flags & PA_SINK_FLAT_VOLUME)
        sys->base_volume = i->base_volume;
    else
        sys->base_volume = PA_VOLUME_NORM;
    msg_Dbg(aout, "base volume: %f", pa_sw_volume_to_linear(sys->base_volume));
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

static void stream_moved_cb(pa_stream *s, void *userdata)
{
    aout_instance_t *aout = userdata;
    aout_sys_t *sys = aout->output.p_sys;
    pa_operation *op;
    uint32_t idx = pa_stream_get_device_index(s);

    msg_Dbg(aout, "connected to sink %"PRIu32": %s", idx,
                  pa_stream_get_device_name(s));
    op = pa_context_get_sink_info_by_index(sys->context, idx,
                                           sink_info_cb, aout);
    if (likely(op != NULL))
        pa_operation_unref(op);

    /* Update the variable if someone else moved our stream */
    var_Change(aout, "audio-device", VLC_VAR_SETVALUE,
               &(vlc_value_t){ .i_int = idx }, NULL);
}

static void stream_overflow_cb(pa_stream *s, void *userdata)
{
    aout_instance_t *aout = userdata;

    msg_Err(aout, "overflow");
    (void) s;
}

static void stream_started_cb(pa_stream *s, void *userdata)
{
    aout_instance_t *aout = userdata;

    msg_Dbg(aout, "started");
    (void) s;
}

static void stream_suspended_cb(pa_stream *s, void *userdata)
{
    aout_instance_t *aout = userdata;

    msg_Dbg(aout, "suspended");
    (void) s;
}

static void stream_underflow_cb(pa_stream *s, void *userdata)
{
    aout_instance_t *aout = userdata;
    pa_operation *op;

    msg_Warn(aout, "underflow");
    op = pa_stream_cork(s, 1, NULL, NULL);
    if (op != NULL)
        pa_operation_unref(op);
}

static int stream_wait(pa_threaded_mainloop *mainloop, pa_stream *stream)
{
    pa_stream_state_t state;

    while ((state = pa_stream_get_state(stream)) != PA_STREAM_READY) {
        if (state == PA_STREAM_FAILED || state == PA_STREAM_TERMINATED)
            return -1;
        pa_threaded_mainloop_wait(mainloop);
    }
    return 0;
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
 * Queue one audio frame to the playabck stream
 */
static void Play(aout_instance_t *aout)
{
    aout_sys_t *sys = aout->output.p_sys;
    pa_stream *s = sys->stream;

    /* This function is called exactly once per block in the output FIFO. */
    block_t *block = aout_FifoPop(&aout->output.fifo);
    assert (block != NULL);

    const void *ptr = data_convert(&block);
    if (unlikely(ptr == NULL))
        return;

    size_t len = block->i_buffer;
    mtime_t pts = block->i_pts + block->i_length;

    /* Note: The core already holds the output FIFO lock at this point.
     * Therefore we must not under any circumstances (try to) acquire the
     * output FIFO lock while the PulseAudio threaded main loop lock is held
     * (including from PulseAudio stream callbacks). Otherwise lock inversion
     * will take place, and sooner or later a deadlock. */
    pa_threaded_mainloop_lock(sys->mainloop);

    if (pa_stream_is_corked(s) > 0) {
        /* Start or resume the stream. Zeroes are prepended to sync.
         * This does not really work because PulseAudio latency measurement is
         * garbage at start. */
        pa_operation *op;
        pa_usec_t latency;
        int negative;

        if (pa_stream_get_latency(s, &latency, &negative) == 0)
            msg_Dbg(aout, "starting with %c%"PRIu64" us latency",
                    negative ? '-' : '+', latency);
        else
            latency = negative = 0;

        mtime_t advance = block->i_pts - mdate();
        if (negative)
            advance += latency;
        else
            advance -= latency;

        if (advance > 0) {
            size_t nb = (advance * aout->output.output.i_rate) / CLOCK_FREQ;
            size_t size = aout->output.output.i_bytes_per_frame;
            float *zeroes = calloc (nb, size);

            msg_Dbg(aout, "prepending %zu zeroes", nb);
            if (likely(zeroes != NULL))
                if (pa_stream_write(s, zeroes, nb * size, free, 0,
                                    PA_SEEK_RELATIVE) < 0)
                    free(zeroes);
        }

        op = pa_stream_cork(s, 0, NULL, NULL);
        if (op != NULL)
            pa_operation_unref(op);
        op = pa_stream_trigger(s, NULL, NULL);
        if (op != NULL)
            pa_operation_unref(op);
        msg_Dbg(aout, "uncorking");
    }

#if 0 /* Fault injector to test underrun recovery */
    static unsigned u = 0;
    if ((++u % 500) == 0) {
        msg_Err(aout, "fault injection");
        msleep(CLOCK_FREQ*2);
    }
#endif

    if (pa_stream_write(s, ptr, len, data_free, 0, PA_SEEK_RELATIVE) < 0) {
        error(aout, "cannot write", sys->context);
        block_Release(block);
    }

    pa_threaded_mainloop_unlock(sys->mainloop);
}

/**
 * Cork or uncork the playback stream
 */
static void Pause(aout_instance_t *aout, bool b_paused, mtime_t i_date)
{
    aout_sys_t *sys = aout->output.p_sys;
    pa_stream *s = sys->stream;

    if (!b_paused)
        return; /* nothing to do - yet */

    pa_threaded_mainloop_lock(sys->mainloop);

    pa_operation *op = pa_stream_cork(s, 1, NULL, NULL);
    if (op != NULL)
        pa_operation_unref(op);

    pa_threaded_mainloop_unlock(sys->mainloop);
    (void) i_date;
}

static int VolumeSet(aout_instance_t *aout, audio_volume_t vol, bool mute)
{
    aout_sys_t *sys = aout->output.p_sys;
    pa_threaded_mainloop *mainloop = sys->mainloop;
    pa_operation *op;

    uint32_t idx = pa_stream_get_index(sys->stream);
    pa_volume_t volume = pa_sw_volume_from_linear(vol / (float)AOUT_VOLUME_DEFAULT);
    pa_cvolume cvolume;

    /* TODO: do not ruin the channel balance (if set outside VLC) */
    /* TODO: notify UI about volume changes by other PulseAudio clients */
    pa_cvolume_set(&sys->cvolume, sys->cvolume.channels, volume);
    pa_sw_cvolume_multiply_scalar(&cvolume, &sys->cvolume, sys->base_volume);
    assert(pa_cvolume_valid(&cvolume));

    pa_threaded_mainloop_lock(mainloop);
    op = pa_context_set_sink_input_volume(sys->context, idx, &cvolume, NULL, NULL);
    if (likely(op != NULL))
        pa_operation_unref(op);
    op = pa_context_set_sink_input_mute(sys->context, idx, mute, NULL, NULL);
    if (likely(op != NULL))
        pa_operation_unref(op);
    pa_threaded_mainloop_unlock(mainloop);

    return 0;
}

static int StreamMove(vlc_object_t *obj, const char *varname, vlc_value_t old,
                      vlc_value_t val, void *userdata)
{
    aout_instance_t *aout = (aout_instance_t *)obj;
    aout_sys_t *sys = aout->output.p_sys;
    pa_stream *s = userdata;
    pa_operation *op;
    uint32_t idx = pa_stream_get_index(s);
    uint32_t sink_idx = val.i_int;

    (void) varname; (void) old;

    pa_threaded_mainloop_lock(sys->mainloop);
    op = pa_context_move_sink_input_by_index(sys->context, idx, sink_idx,
                                             NULL, NULL);
    if (likely(op != NULL)) {
        pa_operation_unref(op);
        msg_Dbg(aout, "moving to sink %"PRIu32, sink_idx);
    } else
        error(aout, "cannot move sink", sys->context);
    pa_threaded_mainloop_unlock(sys->mainloop);

    return (op != NULL) ? VLC_SUCCESS : VLC_EGENERIC;
}


/**
 * Create a PulseAudio playback stream, a.k.a. a sink input.
 */
static int Open(vlc_object_t *obj)
{
    aout_instance_t *aout = (aout_instance_t *)obj;
    pa_operation *op;

    /* Sample format specification */
    struct pa_sample_spec ss;

    switch(aout->output.output.i_format)
    {
        case VLC_CODEC_F64B:
            aout->output.output.i_format = VLC_CODEC_F32B;
        case VLC_CODEC_F32B:
            ss.format = PA_SAMPLE_FLOAT32BE;
            break;
        case VLC_CODEC_F64L:
            aout->output.output.i_format = VLC_CODEC_F32L;
        case VLC_CODEC_F32L:
            ss.format = PA_SAMPLE_FLOAT32LE;
            break;
        case VLC_CODEC_FI32:
            aout->output.output.i_format = VLC_CODEC_FL32;
            ss.format = PA_SAMPLE_FLOAT32NE;
            break;
        case VLC_CODEC_S32B:
            ss.format = PA_SAMPLE_S32BE;
            break;
        case VLC_CODEC_S32L:
            ss.format = PA_SAMPLE_S32LE;
            break;
        case VLC_CODEC_S24B:
            ss.format = PA_SAMPLE_S24BE;
            break;
        case VLC_CODEC_S24L:
            ss.format = PA_SAMPLE_S24LE;
            break;
        case VLC_CODEC_S16B:
            ss.format = PA_SAMPLE_S16BE;
            break;
        case VLC_CODEC_S16L:
            ss.format = PA_SAMPLE_S16LE;
            break;
        case VLC_CODEC_S8:
            aout->output.output.i_format = VLC_CODEC_U8;
        case VLC_CODEC_U8:
            ss.format = PA_SAMPLE_U8;
            break;
        default:
            if (HAVE_FPU)
            {
                aout->output.output.i_format = VLC_CODEC_FL32;
                ss.format = PA_SAMPLE_FLOAT32NE;
            }
            else
            {
                aout->output.output.i_format = VLC_CODEC_S16N;
                ss.format = PA_SAMPLE_S16NE;
            }
            break;
    }

    ss.rate = aout->output.output.i_rate;
    ss.channels = aout_FormatNbChannels(&aout->output.output);
    if (!pa_sample_spec_valid(&ss)) {
        msg_Err(aout, "unsupported sample specification");
        return VLC_EGENERIC;
    }

    /* Channel mapping (order defined in vlc_aout.h) */
    struct pa_channel_map map;
    map.channels = 0;

    if (aout->output.output.i_physical_channels & AOUT_CHAN_LEFT)
        map.map[map.channels++] = PA_CHANNEL_POSITION_FRONT_LEFT;
    if (aout->output.output.i_physical_channels & AOUT_CHAN_RIGHT)
        map.map[map.channels++] = PA_CHANNEL_POSITION_FRONT_RIGHT;
    if (aout->output.output.i_physical_channels & AOUT_CHAN_MIDDLELEFT)
        map.map[map.channels++] = PA_CHANNEL_POSITION_SIDE_LEFT;
    if (aout->output.output.i_physical_channels & AOUT_CHAN_MIDDLERIGHT)
        map.map[map.channels++] = PA_CHANNEL_POSITION_SIDE_RIGHT;
    if (aout->output.output.i_physical_channels & AOUT_CHAN_REARLEFT)
        map.map[map.channels++] = PA_CHANNEL_POSITION_REAR_LEFT;
    if (aout->output.output.i_physical_channels & AOUT_CHAN_REARRIGHT)
        map.map[map.channels++] = PA_CHANNEL_POSITION_REAR_RIGHT;
    if (aout->output.output.i_physical_channels & AOUT_CHAN_REARCENTER)
        map.map[map.channels++] = PA_CHANNEL_POSITION_REAR_CENTER;
    if (aout->output.output.i_physical_channels & AOUT_CHAN_CENTER)
    {
        if (ss.channels == 1)
            map.map[map.channels++] = PA_CHANNEL_POSITION_MONO;
        else
            map.map[map.channels++] = PA_CHANNEL_POSITION_FRONT_CENTER;
    }
    if (aout->output.output.i_physical_channels & AOUT_CHAN_LFE)
        map.map[map.channels++] = PA_CHANNEL_POSITION_LFE;

    for (unsigned i = 0; map.channels < ss.channels; i++) {
        map.map[map.channels++] = PA_CHANNEL_POSITION_AUX0 + i;
        msg_Warn(aout, "mapping channel %"PRIu8" to AUX%u", map.channels, i);
    }

    if (!pa_channel_map_valid(&map)) {
        msg_Err(aout, "unsupported channel map");
        return VLC_EGENERIC;
    } else {
        const char *name = pa_channel_map_to_pretty_name(&map);
        msg_Dbg(aout, "using %s channel map", (name != NULL) ? name : "?");
    }

    /* Stream parameters */
    const pa_stream_flags_t flags = PA_STREAM_START_CORKED
                                  //| PA_STREAM_INTERPOLATE_TIMING
                                  | PA_STREAM_AUTO_TIMING_UPDATE;

    const uint32_t byterate = pa_bytes_per_second(&ss);
    struct pa_buffer_attr attr;
    attr.maxlength = -1;
    attr.tlength = byterate * AOUT_MAX_PREPARE_TIME / CLOCK_FREQ;
    attr.prebuf = 0; /* trigger manually */
    attr.minreq = -1;
    attr.fragsize = 0; /* not used for output */

    /* Allocate structures */
    aout_sys_t *sys = malloc(sizeof(*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    aout->output.p_sys = sys;
    sys->context = NULL;
    sys->stream = NULL;

    /* Channel volume */
    sys->base_volume = PA_VOLUME_NORM;
    pa_cvolume_set(&sys->cvolume, ss.channels, PA_VOLUME_NORM);

    /* Allocate threaded main loop */
    pa_threaded_mainloop *mainloop = pa_threaded_mainloop_new();
    if (unlikely(mainloop == NULL)) {
        free(sys);
        return VLC_ENOMEM;
    }
    sys->mainloop = mainloop;

    if (pa_threaded_mainloop_start(mainloop) < 0) {
        pa_threaded_mainloop_free(mainloop);
        free(sys);
        return VLC_ENOMEM;
    }
    pa_threaded_mainloop_lock(mainloop);

    /* Connect to PulseAudio server */
    char *user_agent = var_InheritString(aout, "user-agent");
    pa_context *ctx = pa_context_new(pa_threaded_mainloop_get_api(mainloop),
                                     user_agent);
    free(user_agent);
    if (unlikely(ctx == NULL))
        goto fail;
    sys->context = ctx;

    pa_context_set_state_callback(ctx, context_state_cb, mainloop);
    if (pa_context_connect(ctx, NULL, 0, NULL) < 0
     || context_wait(mainloop, ctx)) {
        error(aout, "cannot connect to server", ctx);
        goto fail;
    }

    /* Create a playback stream */
    pa_stream *s = pa_stream_new(ctx, "audio stream", &ss, &map);
    if (s == NULL) {
        error(aout, "cannot create stream", ctx);
        goto fail;
    }
    sys->stream = s;
    pa_stream_set_state_callback(s, stream_state_cb, mainloop);
    pa_stream_set_moved_callback(s, stream_moved_cb, aout);
    pa_stream_set_overflow_callback(s, stream_overflow_cb, aout);
    pa_stream_set_started_callback(s, stream_started_cb, aout);
    pa_stream_set_suspended_callback(s, stream_suspended_cb, aout);
    pa_stream_set_underflow_callback(s, stream_underflow_cb, aout);

    if (pa_stream_connect_playback(s, NULL, &attr, flags, NULL, NULL) < 0
     || stream_wait(mainloop, s)) {
        error(aout, "cannot connect stream", ctx);
        goto fail;
    }

    const struct pa_buffer_attr *pba = pa_stream_get_buffer_attr(s);
    msg_Dbg(aout, "using buffer metrics: maxlength=%u, tlength=%u, "
            "prebuf=%u, minreq=%u",
            pba->maxlength, pba->tlength, pba->prebuf, pba->minreq);

    aout->output.i_nb_samples = pba->minreq / pa_frame_size(&ss);

    var_Create(aout, "audio-device", VLC_VAR_INTEGER|VLC_VAR_HASCHOICE);
    var_Change(aout, "audio-device", VLC_VAR_SETTEXT,
               &(vlc_value_t){ .psz_string = (char *)_("Audio device") },
               NULL);
    var_AddCallback (aout, "audio-device", StreamMove, s);
    op = pa_context_get_sink_info_list(ctx, sink_list_cb, aout);
    /* We may need to wait for completion... once LibVLC supports this */
    if (op != NULL)
        pa_operation_unref(op);
    stream_moved_cb(s, aout);
    pa_threaded_mainloop_unlock(mainloop);

    aout->output.pf_play = Play;
    aout->output.pf_pause = Pause;
    aout->output.pf_volume_set = VolumeSet;
    return VLC_SUCCESS;

fail:
    pa_threaded_mainloop_unlock(mainloop);
    Close(obj);
    return VLC_EGENERIC;
}

/**
 * Removes a PulseAudio playback stream
 */
static void Close (vlc_object_t *obj)
{
    aout_instance_t *aout = (aout_instance_t *)obj;
    aout_sys_t *sys = aout->output.p_sys;
    pa_threaded_mainloop *mainloop = sys->mainloop;
    pa_context *ctx = sys->context;
    pa_stream *s = sys->stream;

    if (s != NULL) {
        /* The callback takes mainloop lock, so it CANNOT be held here! */
        var_DelCallback (aout, "audio-device", StreamMove, s);
        var_Destroy (aout, "audio-device");
    }

    pa_threaded_mainloop_lock(mainloop);
    if (s != NULL) {
        pa_operation *op;

        op = pa_stream_flush(s, NULL, NULL);
        if (op != NULL)
            pa_operation_unref(op);
        op = pa_stream_drain(s, NULL, NULL);
        if (op != NULL)
            pa_operation_unref(op);
        pa_stream_disconnect(s);
        pa_stream_unref(s);
    }
    if (ctx != NULL)
        pa_context_unref(ctx);
    pa_threaded_mainloop_unlock(mainloop);
    pa_threaded_mainloop_free(mainloop);
    free(sys);
}
