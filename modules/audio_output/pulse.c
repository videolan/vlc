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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <vlc_aout.h>
#include <vlc_cpu.h>

#include <pulse/pulseaudio.h>

/*****************************************************************************
 * aout_sys_t: Pulseaudio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    struct pa_stream *stream; /**< PulseAudio playback stream object */
    struct pa_context *context; /**< PulseAudio connection context */
    struct pa_threaded_mainloop *mainloop; /**< PulseAudio event loop */
    size_t buffer_size;
    mtime_t start_date;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open        ( vlc_object_t * );
static void Close       ( vlc_object_t * );
static void Play        ( aout_instance_t * );

static void context_state_cb(pa_context *c, void *userdata);
static void stream_state_cb(pa_stream *s, void * userdata);
static void stream_request_cb(pa_stream *s, size_t length, void *userdata);
static void success_cb(pa_stream *s, int sucess, void *userdata);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_shortname( "PulseAudio" )
    set_description( N_("Pulseaudio audio output") )
    set_capability( "audio output", 160 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )
    add_shortcut( "pulseaudio", "pa" )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Open: open the audio device
 *****************************************************************************/
static int Open ( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;

    /* Sample format specification */
    struct pa_sample_spec ss;

    switch(p_aout->output.output.i_format)
    {
        case VLC_CODEC_F64B:
            p_aout->output.output.i_format = VLC_CODEC_F32B;
        case VLC_CODEC_F32B:
            ss.format = PA_SAMPLE_FLOAT32BE;
            break;
        case VLC_CODEC_F64L:
            p_aout->output.output.i_format = VLC_CODEC_F32L;
        case VLC_CODEC_F32L:
            ss.format = PA_SAMPLE_FLOAT32LE;
            break;
        case VLC_CODEC_FI32:
            p_aout->output.output.i_format = VLC_CODEC_FL32;
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
            p_aout->output.output.i_format = VLC_CODEC_U8;
        case VLC_CODEC_U8:
            ss.format = PA_SAMPLE_U8;
            break;
        default:
            if (HAVE_FPU)
            {
                p_aout->output.output.i_format = VLC_CODEC_FL32;
                ss.format = PA_SAMPLE_FLOAT32NE;
            }
            else
            {
                p_aout->output.output.i_format = VLC_CODEC_S16N;
                ss.format = PA_SAMPLE_S16NE;
            }
            break;
    }

    ss.rate = p_aout->output.output.i_rate;
    ss.channels = aout_FormatNbChannels(&p_aout->output.output);
    if (!pa_sample_spec_valid(&ss)) {
        msg_Err(p_aout, "unsupported sample specification");
        return VLC_EGENERIC;
    }

    /* Channel mapping */
    struct pa_channel_map map;
    map.channels = 0;

    if (p_aout->output.output.i_physical_channels & AOUT_CHAN_CENTER)
    {
        if (ss.channels == 1)
            map.map[map.channels++] = PA_CHANNEL_POSITION_MONO;
        else
            map.map[map.channels++] = PA_CHANNEL_POSITION_FRONT_CENTER;
    }
    if (p_aout->output.output.i_physical_channels & AOUT_CHAN_LEFT)
        map.map[map.channels++] = PA_CHANNEL_POSITION_FRONT_LEFT;
    if (p_aout->output.output.i_physical_channels & AOUT_CHAN_RIGHT)
        map.map[map.channels++] = PA_CHANNEL_POSITION_FRONT_RIGHT;

    if (p_aout->output.output.i_physical_channels & AOUT_CHAN_REARCENTER)
        map.map[map.channels++] = PA_CHANNEL_POSITION_REAR_CENTER;
    if (p_aout->output.output.i_physical_channels & AOUT_CHAN_REARLEFT)
        map.map[map.channels++] = PA_CHANNEL_POSITION_REAR_LEFT;
    if (p_aout->output.output.i_physical_channels & AOUT_CHAN_REARRIGHT)
        map.map[map.channels++] = PA_CHANNEL_POSITION_REAR_RIGHT;

    if (p_aout->output.output.i_physical_channels & AOUT_CHAN_MIDDLELEFT)
        map.map[map.channels++] = PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER;
    if (p_aout->output.output.i_physical_channels & AOUT_CHAN_MIDDLERIGHT)
        map.map[map.channels++] = PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER;

    if (p_aout->output.output.i_physical_channels & AOUT_CHAN_LFE)
        map.map[map.channels++] = PA_CHANNEL_POSITION_LFE;

    for (unsigned i = 0; map.channels < ss.channels; i++) {
        map.map[map.channels++] = PA_CHANNEL_POSITION_AUX0 + i;
        msg_Warn(p_aout, "mapping channel %"PRIu8" to AUX%u", map.channels, i);
    }

    if (!pa_channel_map_valid(&map)) {
        msg_Err(p_aout, "unsupported channel map");
        return VLC_EGENERIC;
    } else {
        const char *name = pa_channel_map_to_pretty_name(&map);
        msg_Dbg(p_aout, "using %s channel map", (name != NULL) ? name : "?");
    }

    const pa_stream_flags_t flags = PA_STREAM_INTERPOLATE_TIMING
                                  | PA_STREAM_AUTO_TIMING_UPDATE
                                  | PA_STREAM_ADJUST_LATENCY;

    struct pa_buffer_attr attr;
    /* Reduce overall latency to 100mS to reduce audible clicks
     * Also minreq and internal buffers are now 100ms to reduce resampling.
     * But it still should not drop samples even with USB sound cards. */
    attr.tlength = pa_bytes_per_second(&ss)/10;
    attr.maxlength = attr.tlength * 2;
    attr.prebuf = -1;
    attr.minreq = -1;
    attr.fragsize = 0; /* not used for output */

    /* Allocate structures */
    struct aout_sys_t *sys = malloc(sizeof(*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    sys->stream = NULL;
    sys->start_date = VLC_TS_INVALID;

    sys->mainloop = pa_threaded_mainloop_new();
    if (unlikely(sys->mainloop == NULL)) {
        free(sys);
        return VLC_ENOMEM;
    }

    char *ua = var_InheritString(p_aout, "user-agent");

    sys->context = pa_context_new(
                              pa_threaded_mainloop_get_api(sys->mainloop), ua);
    free(ua);
    if (unlikely(sys->context == NULL)) {
        pa_threaded_mainloop_free(sys->mainloop);
        free(sys);
        return VLC_ENOMEM;
    }

    p_aout->output.p_sys = sys;
    pa_context_set_state_callback(sys->context, context_state_cb, sys);
    if (pa_context_connect(sys->context, NULL, 0, NULL) < 0) {
connect_fail:
        msg_Err(p_aout, "cannot connect to server: %s",
                pa_strerror(pa_context_errno(sys->context)));
        goto fail;
    }

    if (pa_threaded_mainloop_start(sys->mainloop) < 0) {
        msg_Err(p_aout, "cannot start main loop");
        goto fail;
    }

    pa_threaded_mainloop_lock(sys->mainloop);
    /* Wait until the context is ready */
    while (pa_context_get_state(sys->context) != PA_CONTEXT_READY) {
        if (pa_context_get_state(sys->context) == PA_CONTEXT_FAILED)
            goto connect_fail;
        pa_threaded_mainloop_wait(sys->mainloop);
    }

    /* Create a stream */
    sys->stream = pa_stream_new(sys->context, "audio stream", &ss, &map);
    if (sys->stream == NULL) {
        msg_Err(p_aout, "cannot create stream: %s",
                pa_strerror(pa_context_errno(sys->context)));
        goto unlock_and_fail;
    }
    pa_stream_set_state_callback(sys->stream, stream_state_cb, sys);
    pa_stream_set_write_callback(sys->stream, stream_request_cb, p_aout);
    if (pa_stream_connect_playback(sys->stream, NULL, &attr,
                                   flags, NULL, NULL) < 0) {
stream_fail:
        msg_Err(p_aout, "cannot connect stream: %s",
                pa_strerror(pa_context_errno(sys->context)));
        goto unlock_and_fail;
    }

    while (pa_stream_get_state(sys->stream) != PA_STREAM_READY) {
        if (pa_stream_get_state(sys->stream) == PA_STREAM_FAILED)
            goto stream_fail;
        pa_threaded_mainloop_wait(sys->mainloop);
    }

    msg_Dbg(p_aout, "Connected to device %s (%u, %ssuspended).",
            pa_stream_get_device_name(sys->stream),
            pa_stream_get_device_index(sys->stream),
            pa_stream_is_suspended(sys->stream) ? "" : "not ");

    const struct pa_buffer_attr *pba = pa_stream_get_buffer_attr(sys->stream);

    p_aout->output.i_nb_samples = pba->minreq / pa_frame_size(&ss);
    /* Set buffersize from pulseaudio defined minrequest */
    sys->buffer_size = pba->minreq;
    msg_Dbg(p_aout, "using buffer metrics: maxlength=%u, tlength=%u, "
            "prebuf=%u, minreq=%u",
            pba->maxlength, pba->tlength, pba->prebuf, pba->minreq);
    {
        char sst[PA_SAMPLE_SPEC_SNPRINT_MAX];
        msg_Dbg(p_aout, "using sample specification: %s",
                pa_sample_spec_snprint(sst, sizeof(sst),
                                     pa_stream_get_sample_spec(sys->stream)));
    }
    {
        char cmt[PA_CHANNEL_MAP_SNPRINT_MAX];
        msg_Dbg(p_aout, "using channel map: %s",
                pa_channel_map_snprint(cmt, sizeof(cmt),
                                      pa_stream_get_channel_map(sys->stream)));
    }
    pa_threaded_mainloop_unlock(sys->mainloop);

    p_aout->output.pf_play = Play;
    aout_VolumeSoftInit(p_aout);
    return VLC_SUCCESS;

unlock_and_fail:
    pa_threaded_mainloop_unlock(sys->mainloop);
fail:
    Close(p_this);
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Play: play a sound samples buffer
 *****************************************************************************/
static void Play(aout_instance_t * aout)
{
    struct aout_sys_t *sys = aout->output.p_sys;

    if (likely(sys->start_date != VLC_TS_INVALID))
        return;
    pa_threaded_mainloop_lock(sys->mainloop);
    sys->start_date = aout_FifoFirstDate(aout, &aout->output.fifo);
    pa_threaded_mainloop_unlock(sys->mainloop);
}

/*****************************************************************************
 * Close: close the audio device
 *****************************************************************************/
static void Close (vlc_object_t *obj)
{
    aout_instance_t *aout = (aout_instance_t *)obj;
    struct aout_sys_t *sys = aout->output.p_sys;

    if (sys->stream) {
        pa_operation *o;

        pa_threaded_mainloop_lock(sys->mainloop);
        pa_stream_set_write_callback(sys->stream, NULL, NULL);

        o = pa_stream_flush(sys->stream, success_cb, sys);
        if (o != NULL) {
            while (pa_operation_get_state(o) == PA_OPERATION_RUNNING)
                pa_threaded_mainloop_wait(sys->mainloop);
            pa_operation_unref(o);
        }

        o = pa_stream_drain(sys->stream, success_cb, sys);
        if (o != NULL) {
            while (pa_operation_get_state(o) == PA_OPERATION_RUNNING)
                pa_threaded_mainloop_wait(sys->mainloop);
            pa_operation_unref(o);
        }

        pa_threaded_mainloop_unlock(sys->mainloop);
    }

    pa_threaded_mainloop_stop(sys->mainloop);
    if (sys->stream) {
        pa_stream_disconnect(sys->stream);
        pa_stream_unref(sys->stream);
    }
    pa_context_disconnect(sys->context);
    pa_context_unref(sys->context);
    pa_threaded_mainloop_free(sys->mainloop);
    free(sys);
}

static void context_state_cb(pa_context *c, void *userdata)
{
    struct aout_sys_t *sys = userdata;

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_READY:
        case PA_CONTEXT_FAILED:
            pa_threaded_mainloop_signal(sys->mainloop, 0);
        default:
            break;
    }
}

static void stream_state_cb(pa_stream *s, void *userdata)
{
    struct aout_sys_t *sys = userdata;

    switch (pa_stream_get_state(s)) {
        case PA_STREAM_READY:
        case PA_STREAM_FAILED:
            pa_threaded_mainloop_signal(sys->mainloop, 0);
        default:
            break;
    }
}

/* Memory free callback. The block_t address is in front of the data. */
static void block_free_cb(void *data)
{
    block_t **pp = data, *block;

    memcpy(&block, pp - 1, sizeof (block));
    block_Release(block);
}

static void stream_request_cb(pa_stream *s, size_t length, void *userdata)
{
    aout_instance_t *aout = userdata;
    struct aout_sys_t *sys = aout->output.p_sys;
    size_t buffer_size = sys->buffer_size;

    do {
        block_t *block = NULL;

        if (sys->start_date != VLC_TS_INVALID) {
            pa_usec_t latency;
            int negative;

            if (pa_stream_get_latency(s, &latency, &negative) < 0){
                if (pa_context_errno(sys->context) != PA_ERR_NODATA) {
                    msg_Err(aout, "cannot determine latency: %s",
                            pa_strerror(pa_context_errno(sys->context)));
                }
                latency = 0;

            }

            //msg_Dbg(p_aout, "latency=%"PRId64, latency);
            mtime_t next_date = mdate() + latency;

            if (sys->start_date < next_date + AOUT_PTS_TOLERANCE)
                block = aout_OutputNextBuffer(aout, next_date, 0);
        }

        if (block != NULL)
            /* PA won't let us pass a reference to the buffer meta data... */
            block = block_Realloc (block, sizeof (block), block->i_buffer);
        if (block != NULL) {
            memcpy(block->p_buffer, &block, sizeof (block));
            block->p_buffer += sizeof (block);
            block->i_buffer -= sizeof (block);

            length -= block->i_buffer;
            pa_stream_write(s, block->p_buffer, block->i_buffer,
                            block_free_cb, 0, PA_SEEK_RELATIVE);
        } else {
            void *data = pa_xmalloc(length);
            memset(data, 0, length);
            pa_stream_write(s, data, length, pa_xfree, 0, PA_SEEK_RELATIVE);
            length = 0;
        }
    } while (length > buffer_size);
}

static void success_cb(pa_stream *s, int sucess, void *userdata)
{
    struct aout_sys_t *sys = userdata;

    (void) s;
    (void) sucess;
    pa_threaded_mainloop_signal(sys->mainloop, 0);
}
