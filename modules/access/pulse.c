/**
 * \file pulse.c
 * \brief PulseAudio input plugin for vlc
 */
/*****************************************************************************
 * Copyright (C) 2011 Rémi Denis-Courmont
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
#include <vlc_aout.h>
#include <vlc_demux.h>
#include <vlc_plugin.h>
#include <pulse/pulseaudio.h>
#include "../audio_output/vlcpulse.h"

#define HELP_TEXT N_( \
    "Pass pulse:// to open the default PulseAudio source, " \
    "or pulse://SOURCE to open a specific source named SOURCE.")

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin ()
    set_shortname (N_("PulseAudio"))
    set_description (N_("PulseAudio input"))
    set_capability ("access_demux", 0)
    set_category (CAT_INPUT)
    set_subcategory (SUBCAT_INPUT_ACCESS)
    set_help (HELP_TEXT)

    add_shortcut ("pulse", "pulseaudio", "pa")
    set_callbacks (Open, Close)
vlc_module_end ()

struct demux_sys_t
{
    pa_stream *stream; /**< PulseAudio playback stream object */
    pa_context *context; /**< PulseAudio connection context */
    pa_threaded_mainloop *mainloop; /**< PulseAudio thread */

    es_out_id_t *es;
    bool discontinuity; /**< The next block will not follow the last one */
    unsigned framesize; /**< Byte size of a sample */
    mtime_t caching; /**< Caching value */
};

/* Stream helpers */
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

static void stream_success_cb (pa_stream *s, int success, void *userdata)
{
    pa_threaded_mainloop *mainloop = userdata;

    pa_threaded_mainloop_signal(mainloop, 0);
    (void) s;
    (void) success;
}

static void stream_buffer_attr_cb(pa_stream *s, void *userdata)
{
    demux_t *demux = userdata;
    const struct pa_buffer_attr *pba = pa_stream_get_buffer_attr(s);

    msg_Dbg(demux, "using buffer metrics: maxlength=%"PRIu32", "
            "fragsize=%"PRIu32, pba->maxlength, pba->fragsize);
}

static void stream_moved_cb(pa_stream *s, void *userdata)
{
    demux_t *demux = userdata;
    uint32_t idx = pa_stream_get_device_index(s);

    msg_Dbg(demux, "connected to source %"PRIu32": %s", idx,
                  pa_stream_get_device_name(s));
    stream_buffer_attr_cb(s, userdata);
}

static void stream_overflow_cb(pa_stream *s, void *userdata)
{
    demux_t *demux = userdata;

    msg_Err(demux, "overflow");
    (void) s;
}

static void stream_started_cb(pa_stream *s, void *userdata)
{
    demux_t *demux = userdata;

    msg_Dbg(demux, "started");
    (void) s;
}

static void stream_suspended_cb(pa_stream *s, void *userdata)
{
    demux_t *demux = userdata;

    msg_Dbg(demux, "suspended");
    (void) s;
}

static void stream_underflow_cb(pa_stream *s, void *userdata)
{
    demux_t *demux = userdata;

    msg_Dbg(demux, "underflow");
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

static void stream_read_cb(pa_stream *s, size_t length, void *userdata)
{
    demux_t *demux = userdata;
    demux_sys_t *sys = demux->p_sys;
    const void *ptr;
    unsigned samples = length / sys->framesize;

    if (pa_stream_peek(s, &ptr, &length) < 0) {
        vlc_pa_error(demux, "cannot peek stream", sys->context);
        return;
    }

    mtime_t pts = mdate();
    pa_usec_t latency;
    int negative;

    if (pa_stream_get_latency(s, &latency, &negative) < 0) {
        vlc_pa_error(demux, "cannot determine latency", sys->context);
        return;
    }
    if (negative)
        pts += latency;
    else
        pts -= latency;

    es_out_Control(demux->out, ES_OUT_SET_PCR, pts);
    if (unlikely(sys->es == NULL))
        goto race;

    block_t *block = block_Alloc(length);
    if (likely(block != NULL)) {
        memcpy(block->p_buffer, ptr, length);
        block->i_nb_samples = samples;
        block->i_dts = block->i_pts = pts;
        if (sys->discontinuity) {
            block->i_flags |= BLOCK_FLAG_DISCONTINUITY;
            sys->discontinuity = false;
        }

        es_out_Send(demux->out, sys->es, block);
    } else
        sys->discontinuity = true;
race:
    pa_stream_drop(s);
}

static int Control(demux_t *demux, int query, va_list ap)
{
    demux_sys_t *sys = demux->p_sys;

    switch (query)
    {
        case DEMUX_GET_TIME:
        {
            pa_usec_t us;

            if (pa_stream_get_time(sys->stream, &us) < 0)
                return VLC_EGENERIC;
            *(va_arg(ap, int64_t *)) = us;
            break;
        }

        //case DEMUX_SET_NEXT_DEMUX_TIME: TODO
        //case DEMUX_GET_META TODO

        case DEMUX_GET_PTS_DELAY:
            *(va_arg(ap, int64_t *)) = sys->caching;
            break;

        case DEMUX_HAS_UNSUPPORTED_META:
        case DEMUX_CAN_RECORD:
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_CAN_CONTROL_RATE:
        case DEMUX_CAN_SEEK:
            *(va_arg(ap, bool *)) = false;
            break;

        default:
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/** PulseAudio sample (PCM) format to VLC codec */
static const vlc_fourcc_t fourccs[] = {
    [PA_SAMPLE_U8] =        VLC_CODEC_U8,
    [PA_SAMPLE_ALAW] =      VLC_CODEC_ALAW,
    [PA_SAMPLE_ULAW] =      VLC_CODEC_MULAW,
    [PA_SAMPLE_S16LE] =     VLC_CODEC_S16L,
    [PA_SAMPLE_S16BE] =     VLC_CODEC_S16B,
    [PA_SAMPLE_FLOAT32LE] = VLC_CODEC_F32L,
    [PA_SAMPLE_FLOAT32BE] = VLC_CODEC_F32B,
    [PA_SAMPLE_S32LE] =     VLC_CODEC_S32L,
    [PA_SAMPLE_S32BE] =     VLC_CODEC_S32B,
    [PA_SAMPLE_S24LE] =     VLC_CODEC_S24L,
    [PA_SAMPLE_S24BE] =     VLC_CODEC_S24B,
    [PA_SAMPLE_S24_32LE] =  VLC_CODEC_S24L32,
    [PA_SAMPLE_S24_32BE] =  VLC_CODEC_S24B32,
};

static int Open(vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;

    demux_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->context = vlc_pa_connect(obj, &sys->mainloop);
    if (sys->context == NULL) {
        free(sys);
        return VLC_EGENERIC;
    }

    sys->stream = NULL;
    sys->es = NULL;
    sys->discontinuity = false;
    sys->caching = INT64_C(1000) * var_InheritInteger(obj, "live-caching");
    demux->p_sys = sys;

    /* Stream parameters */
    struct pa_sample_spec ss;
    ss.format = PA_SAMPLE_S16NE;
    ss.rate = 48000;
    ss.channels = 2;
    assert(pa_sample_spec_valid(&ss));

    struct pa_channel_map map;
    map.channels = 2;
    map.map[0] = PA_CHANNEL_POSITION_FRONT_LEFT;
    map.map[1] = PA_CHANNEL_POSITION_FRONT_RIGHT;
    assert(pa_channel_map_valid(&map));

    const pa_stream_flags_t flags = PA_STREAM_INTERPOLATE_TIMING
                                  | PA_STREAM_AUTO_TIMING_UPDATE
                                  | PA_STREAM_ADJUST_LATENCY
                                  | PA_STREAM_FIX_FORMAT
                                  | PA_STREAM_FIX_RATE
                                  /*| PA_STREAM_FIX_CHANNELS*/;

    const char *dev = NULL;
    if (demux->psz_location != NULL && demux->psz_location[0] != '\0')
        dev = demux->psz_location;

    struct pa_buffer_attr attr = {
        .maxlength = -1,
        .fragsize = pa_usec_to_bytes(sys->caching, &ss) / 2,
    };

    es_format_t fmt;

    /* Create record stream */
    pa_stream *s;
    pa_operation *op;

    pa_threaded_mainloop_lock(sys->mainloop);
    s = pa_stream_new(sys->context, "audio stream", &ss, &map);
    if (s == NULL)
        goto error;

    sys->stream = s;
    pa_stream_set_state_callback(s, stream_state_cb, sys->mainloop);
    pa_stream_set_read_callback(s, stream_read_cb, demux);
    pa_stream_set_buffer_attr_callback(s, stream_buffer_attr_cb, demux);
    pa_stream_set_moved_callback(s, stream_moved_cb, demux);
    pa_stream_set_overflow_callback(s, stream_overflow_cb, demux);
    pa_stream_set_started_callback(s, stream_started_cb, demux);
    pa_stream_set_suspended_callback(s, stream_suspended_cb, demux);
    pa_stream_set_underflow_callback(s, stream_underflow_cb, demux);

    if (pa_stream_connect_record(s, dev, &attr, flags) < 0
     || stream_wait(s, sys->mainloop)) {
        vlc_pa_error(obj, "cannot connect record stream", sys->context);
        goto error;
    }

    /* The ES should be initialized before stream_read_cb(), but how? */
    const struct pa_sample_spec *pss = pa_stream_get_sample_spec(s);
    if ((unsigned)pss->format >= sizeof (fourccs) / sizeof (fourccs[0])) {
        msg_Err(obj, "unknown PulseAudio sample format %u",
                (unsigned)pss->format);
        goto error;
    }

    vlc_fourcc_t format = fourccs[pss->format];
    if (format == 0) { /* FIXME: should renegotiate something else */
        msg_Err(obj, "unsupported PulseAudio sample format %u",
                (unsigned)pss->format);
        goto error;
    }

    es_format_Init(&fmt, AUDIO_ES, format);
    fmt.audio.i_physical_channels = fmt.audio.i_original_channels =
        AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    fmt.audio.i_channels = ss.channels;
    fmt.audio.i_rate = pss->rate;
    fmt.audio.i_bitspersample = aout_BitsPerSample(format);
    fmt.audio.i_blockalign = fmt.audio.i_bitspersample * ss.channels / 8;
    fmt.i_bitrate = fmt.audio.i_bitspersample * ss.channels * pss->rate;
    sys->framesize = fmt.audio.i_blockalign;
    sys->es = es_out_Add (demux->out, &fmt);

    /* Update the buffer attributes according to actual format */
    attr.fragsize = pa_usec_to_bytes(sys->caching, pss) / 2;
    op = pa_stream_set_buffer_attr(s, &attr, stream_success_cb, sys->mainloop);
    if (likely(op != NULL)) {
        while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
            pa_threaded_mainloop_wait(sys->mainloop);
        pa_operation_unref(op);
    }
    stream_buffer_attr_cb(s, demux);
    pa_threaded_mainloop_unlock(sys->mainloop);

    demux->pf_demux = NULL;
    demux->pf_control = Control;
    return VLC_SUCCESS;

error:
    pa_threaded_mainloop_unlock(sys->mainloop);
    Close(obj);
    return VLC_EGENERIC;
}

static void Close (vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *sys = demux->p_sys;
    pa_stream *s = sys->stream;

    if (likely(s != NULL)) {
        pa_threaded_mainloop_lock(sys->mainloop);
        pa_stream_disconnect(s);
        pa_stream_set_state_callback(s, NULL, NULL);
        pa_stream_set_read_callback(s, NULL, NULL);
        pa_stream_set_buffer_attr_callback(s, NULL, NULL);
        pa_stream_set_moved_callback(s, NULL, NULL);
        pa_stream_set_overflow_callback(s, NULL, NULL);
        pa_stream_set_started_callback(s, NULL, NULL);
        pa_stream_set_suspended_callback(s, NULL, NULL);
        pa_stream_set_underflow_callback(s, NULL, NULL);
        pa_stream_unref(s);
        pa_threaded_mainloop_unlock(sys->mainloop);
    }

    vlc_pa_disconnect(obj, sys->context, sys->mainloop);
    free(sys);
}
