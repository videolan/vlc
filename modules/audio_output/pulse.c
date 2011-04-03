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

struct aout_sys_t
{
    pa_stream *stream; /**< PulseAudio playback stream object */
    pa_context *context; /**< PulseAudio connection context */
    pa_threaded_mainloop *mainloop; /**< PulseAudio event loop */
    //uint32_t byterate; /**< bytes per second */
};

/* Context helpers */
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

static void stream_moved_cb(pa_stream *s, void *userdata)
{
    vlc_object_t *obj = userdata;

    msg_Dbg(obj, "connected to device %s (%u)",
            pa_stream_get_device_name(s),
            pa_stream_get_device_index(s));
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

/*****************************************************************************
 * Play: play a sound samples buffer
 *****************************************************************************/
static void Play(aout_instance_t *aout)
{
    aout_sys_t *sys = aout->output.p_sys;
    pa_stream *s = sys->stream;

    /* Note: The core already holds the output FIFO lock at this point.
     * Therefore we must not under any circumstances (try to) acquire the
     * output FIFO lock while the PulseAudio threaded main loop lock is held
     * (including from PulseAudio stream callbacks). Otherwise lock inversion
     * will take place, and sooner or later a deadlock. */
    pa_threaded_mainloop_lock(sys->mainloop);

    if (pa_stream_is_corked(sys->stream) > 0) {
        pa_operation *op = pa_stream_cork(s, 0, NULL, NULL);
        if (op != NULL)
            pa_operation_unref(op);
        msg_Dbg(aout, "uncorking");
    }

#if 0
    /* This function should be called by the LibVLC core a header of time,
     * but not more than AOUT_MAX_PREPARE. The PulseAudio latency should be
     * shorter than that (though it might not be the case with some evil piece
     * of audio output hardware). So we need to prepend the buffer with zeroes
     * to keep audio and video in sync. */
    pa_usec_t latency;
    int negative;
    if (pa_stream_get_latency(s, &latency, &negative) < 0) {
        /* Especially at start of stream, latency may not be known (yet). */
        if (pa_context_errno(sys->context) != PA_ERR_NODATA)
            error(aout, "cannot determine latency", sys->context);
        latency = 0;
    }

    mtime_t gap = aout_FifoFirstDate(aout, &aout->output.fifo) - mdate()
                - latency;
    if (gap > AOUT_PTS_TOLERANCE)
        msg_Dbg(aout, "buffer too early (%"PRId64" us)", gap);
    else if (latency != 0 && gap < -AOUT_PTS_TOLERANCE)
        msg_Err(aout, "buffer too late (%"PRId64" us)", -gap);
#endif
#if 0 /* Fault injector to test underrun recovery */
    static unsigned u = 0;
    if ((++u % 500) == 0) {
        msg_Err(aout, "fault injection");
        msleep(CLOCK_FREQ*2);
    }
#endif

    /* This function is called exactly once per block in the output FIFO, so
     * this for-loop is not necessary.
     * If this function is changed to not always dequeue blocks, be sure to
     * limit the queue size to a reasonable limit to avoid huge leaks. */
    for (;;) {
        block_t *block = aout_FifoPop(aout, &aout->output.fifo);
        if (block == NULL)
            break;

        const void *ptr = data_convert(&block);
        if (unlikely(ptr == NULL))
            break;

        size_t len = block->i_buffer;
        //mtime_t pts = block->i_pts, duration = block->i_length;

        if (pa_stream_write(s, ptr, len, data_free, 0, PA_SEEK_RELATIVE) < 0)
        {
            block_Release(block);
            msg_Err(aout, "cannot write: %s",
                    pa_strerror(pa_context_errno(sys->context)));
        }
    }

    pa_threaded_mainloop_unlock(sys->mainloop);
}


/*****************************************************************************
 * Open: open the audio device
 *****************************************************************************/
static int Open(vlc_object_t *obj)
{
    aout_instance_t *aout = (aout_instance_t *)obj;

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
        map.map[map.channels++] = PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER;
    if (aout->output.output.i_physical_channels & AOUT_CHAN_MIDDLERIGHT)
        map.map[map.channels++] = PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER;
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

    const pa_stream_flags_t flags = PA_STREAM_INTERPOLATE_TIMING
                                  | PA_STREAM_AUTO_TIMING_UPDATE
                                  | PA_STREAM_ADJUST_LATENCY
                                  | PA_STREAM_START_CORKED;

    const uint32_t byterate = pa_bytes_per_second(&ss);
    struct pa_buffer_attr attr;
    /* no point in larger buffers on PA side than VLC */
    attr.maxlength = -1;
    attr.tlength = byterate * AOUT_MAX_ADVANCE_TIME / CLOCK_FREQ;
    attr.prebuf = byterate * AOUT_MIN_PREPARE_TIME / CLOCK_FREQ;
    attr.minreq = -1;
    attr.fragsize = 0; /* not used for output */

    /* Allocate structures */
    aout_sys_t *sys = malloc(sizeof(*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    aout->output.p_sys = sys;
    sys->context = NULL;
    sys->stream = NULL;
    //sys->byterate = byterate;

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

    if (pa_stream_connect_playback(s, NULL, &attr, flags, NULL, NULL) < 0
     || stream_wait(mainloop, s)) {
        error(aout, "cannot connect stream", ctx);
        goto fail;
    }
    stream_moved_cb(s, aout);

    const struct pa_buffer_attr *pba = pa_stream_get_buffer_attr(s);
    msg_Dbg(aout, "using buffer metrics: maxlength=%u, tlength=%u, "
            "prebuf=%u, minreq=%u",
            pba->maxlength, pba->tlength, pba->prebuf, pba->minreq);

    aout->output.i_nb_samples = pba->minreq / pa_frame_size(&ss);
    pa_threaded_mainloop_unlock(mainloop);

    aout->output.pf_play = Play;
    aout_VolumeSoftInit(aout);
    return VLC_SUCCESS;

fail:
    pa_threaded_mainloop_unlock(mainloop);
    Close(obj);
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: close the audio device
 *****************************************************************************/
static void Close (vlc_object_t *obj)
{
    aout_instance_t *aout = (aout_instance_t *)obj;
    aout_sys_t *sys = aout->output.p_sys;
    pa_threaded_mainloop *mainloop = sys->mainloop;
    pa_context *ctx = sys->context;
    pa_stream *s = sys->stream;

    pa_threaded_mainloop_lock(mainloop);
    if (s != NULL) {
        pa_operation *op;

        pa_stream_set_write_callback(s, NULL, NULL);
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
