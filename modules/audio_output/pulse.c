/*****************************************************************************
 * pulse.c : Pulseaudio output plugin for vlc
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
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

#include <pulse/pulseaudio.h>

#include <assert.h>

/*****************************************************************************
 * aout_sys_t: Pulseaudio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    /** PulseAudio playback stream object */
    struct pa_stream *stream;

    /** PulseAudio connection context */
    struct pa_context *context;

    /** Main event loop object */
    struct pa_threaded_mainloop *mainloop;

    int started;
    size_t buffer_size;
    mtime_t start_date;
};

#define    PULSE_CLIENT_NAME N_("VLC media player")

#if 0
#define PULSE_DEBUG( ...) \
    msg_Dbg( p_aout, __VA_ARGS__ )
#else
#define PULSE_DEBUG( ...) \
    (void) 0
#endif


#define CHECK_DEAD_GOTO(label) do { \
if (!p_sys->context || pa_context_get_state(p_sys->context) != PA_CONTEXT_READY || \
    !p_sys->stream || pa_stream_get_state(p_sys->stream) != PA_STREAM_READY) { \
        msg_Err(p_aout, "Connection died: %s", p_sys->context ? pa_strerror(pa_context_errno(p_sys->context)) : "NULL"); \
        goto label; \
    }  \
} while(0);
/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open        ( vlc_object_t * );
static void Close       ( vlc_object_t * );
static void Play        ( aout_instance_t * );

static void context_state_cb(pa_context *c, void *userdata);
static void stream_state_cb(pa_stream *s, void * userdata);
static void stream_request_cb(pa_stream *s, size_t length, void *userdata);
static void stream_latency_update_cb(pa_stream *s, void *userdata);
static void success_cb(pa_stream *s, int sucess, void *userdata);
static void uninit(aout_instance_t *p_aout);
/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_shortname( "Pulse Audio" );
    set_description( N_("Pulseaudio audio output") );
    set_capability( "audio output", 40 );
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_AOUT );
    add_shortcut( "pulseaudio" );
    add_shortcut( "pa" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: open the audio device
 *****************************************************************************/
static int Open ( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys;
    struct pa_sample_spec ss;
    const struct pa_buffer_attr *buffer_attr;
    struct pa_buffer_attr a;
    struct pa_channel_map map;

    /* Allocate structures */
    p_aout->output.p_sys = p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;
    memset( p_sys, 0, sizeof( aout_sys_t ) );

    PULSE_DEBUG( "Pulse start initialization");

    ss.rate = p_aout->output.output.i_rate;
    ss.channels = 2;

    ss.format = PA_SAMPLE_S16LE;
    p_aout->output.output.i_physical_channels =
            AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    p_aout->output.output.i_format = AOUT_FMT_S16_NE;

    if (!pa_sample_spec_valid(&ss)) {
        msg_Err(p_aout,"Invalid sample spec");
        goto fail;
    }
    
    a.maxlength = pa_bytes_per_second(&ss)/4/pa_frame_size(&ss);
    a.tlength = a.maxlength*9/10;
    a.prebuf = a.tlength/2;
    a.minreq = a.tlength/10;

    a.maxlength *= pa_frame_size(&ss);
    a.tlength *= pa_frame_size(&ss);
    a.prebuf *= pa_frame_size(&ss);
    a.minreq *= pa_frame_size(&ss);

    p_sys->buffer_size = a.minreq;

    pa_channel_map_init_stereo(&map);


    if (!(p_sys->mainloop = pa_threaded_mainloop_new())) {
        msg_Err(p_aout, "Failed to allocate main loop");
        goto fail;
    }

    if (!(p_sys->context = pa_context_new(pa_threaded_mainloop_get_api(p_sys->mainloop), _( PULSE_CLIENT_NAME )))) {
        msg_Err(p_aout, "Failed to allocate context");
        goto fail;
    }

    pa_context_set_state_callback(p_sys->context, context_state_cb, p_aout);

    PULSE_DEBUG( "Pulse before context connect");

    if (pa_context_connect(p_sys->context, NULL, 0, NULL) < 0) {
        msg_Err(p_aout, "Failed to connect to server: %s", pa_strerror(pa_context_errno(p_sys->context)));
        goto fail;
    }

    PULSE_DEBUG( "Pulse after context connect");

    pa_threaded_mainloop_lock(p_sys->mainloop);
    
    if (pa_threaded_mainloop_start(p_sys->mainloop) < 0) {
        msg_Err(p_aout, "Failed to start main loop");
        goto unlock_and_fail;
    }

    msg_Dbg(p_aout, "Pulse mainloop started");

    /* Wait until the context is ready */
    pa_threaded_mainloop_wait(p_sys->mainloop);

    if (pa_context_get_state(p_sys->context) != PA_CONTEXT_READY) {
        msg_Err(p_aout, "Failed to connect to server: %s", pa_strerror(pa_context_errno(p_sys->context)));
        goto unlock_and_fail;
    }

    if (!(p_sys->stream = pa_stream_new(p_sys->context, "audio stream", &ss, &map))) {
        msg_Err(p_aout, "Failed to create stream: %s", pa_strerror(pa_context_errno(p_sys->context)));
        goto unlock_and_fail;
    }

    PULSE_DEBUG( "Pulse after new stream");

    pa_stream_set_state_callback(p_sys->stream, stream_state_cb, p_aout);
    pa_stream_set_write_callback(p_sys->stream, stream_request_cb, p_aout);
    pa_stream_set_latency_update_callback(p_sys->stream, stream_latency_update_cb, p_aout);

    if (pa_stream_connect_playback(p_sys->stream, NULL, &a, PA_STREAM_INTERPOLATE_TIMING|PA_STREAM_AUTO_TIMING_UPDATE, NULL, NULL) < 0) {
        msg_Err(p_aout, "Failed to connect stream: %s", pa_strerror(pa_context_errno(p_sys->context)));
        goto unlock_and_fail;
    }

     PULSE_DEBUG("Pulse stream connect");

    /* Wait until the stream is ready */
    pa_threaded_mainloop_wait(p_sys->mainloop);

    msg_Dbg(p_aout,"Pulse stream connected");

    if (pa_stream_get_state(p_sys->stream) != PA_STREAM_READY) {
        msg_Err(p_aout, "Failed to connect to server: %s", pa_strerror(pa_context_errno(p_sys->context)));
        goto unlock_and_fail;
    }


    PULSE_DEBUG("Pulse after stream get status");

    pa_threaded_mainloop_unlock(p_sys->mainloop);

    buffer_attr = pa_stream_get_buffer_attr(p_sys->stream);
    p_aout->output.i_nb_samples = buffer_attr->minreq / pa_frame_size(&ss);
    p_aout->output.pf_play = Play;
    aout_VolumeSoftInit(p_aout);
    msg_Dbg(p_aout, "Pulse initialized successfully");
    {
        char cmt[PA_CHANNEL_MAP_SNPRINT_MAX], sst[PA_SAMPLE_SPEC_SNPRINT_MAX];

        msg_Dbg(p_aout, "Buffer metrics: maxlength=%u, tlength=%u, prebuf=%u, minreq=%u", buffer_attr->maxlength, buffer_attr->tlength, buffer_attr->prebuf, buffer_attr->minreq);
        msg_Dbg(p_aout, "Using sample spec '%s', channel map '%s'.",
                pa_sample_spec_snprint(sst, sizeof(sst), pa_stream_get_sample_spec(p_sys->stream)),
                pa_channel_map_snprint(cmt, sizeof(cmt), pa_stream_get_channel_map(p_sys->stream)));

            msg_Dbg(p_aout, "Connected to device %s (%u, %ssuspended).",
                        pa_stream_get_device_name(p_sys->stream),
                        pa_stream_get_device_index(p_sys->stream),
                        pa_stream_is_suspended(p_sys->stream) ? "" : "not ");
    }

    return VLC_SUCCESS;

unlock_and_fail:
    msg_Dbg(p_aout, "Pulse initialization unlock and fail");

    if (p_sys->mainloop)
        pa_threaded_mainloop_unlock(p_sys->mainloop);
fail:
    msg_Err(p_aout, "Pulse initialization failed");
    uninit(p_aout);
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Play: play a sound samples buffer
 *****************************************************************************/
static void Play( aout_instance_t * p_aout )
{
    struct aout_sys_t * p_sys = (struct aout_sys_t *) p_aout->output.p_sys;

    pa_operation *o;

    if(!p_sys->started){
        msg_Dbg(p_aout, "Pulse stream started");
        p_sys->start_date =
            aout_FifoFirstDate( p_aout, &p_aout->output.fifo );
        p_sys->started = 1;

        pa_threaded_mainloop_lock(p_sys->mainloop);
        if((o = pa_stream_flush(p_sys->stream, success_cb, p_aout))){
            pa_operation_unref(o);
        }
        pa_threaded_mainloop_unlock(p_sys->mainloop);

        pa_threaded_mainloop_signal(p_sys->mainloop, 0);
    }
}

/*****************************************************************************
 * Close: close the audio device
 *****************************************************************************/
static void Close ( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    msg_Dbg(p_aout, "Pulse Close");

    if(p_sys->stream){
        pa_operation *o;
        pa_threaded_mainloop_lock(p_sys->mainloop);
        pa_stream_set_write_callback(p_sys->stream, NULL, NULL);

        if((o = pa_stream_drain(p_sys->stream, success_cb, p_aout))){
            while (pa_operation_get_state(o) != PA_OPERATION_DONE) {
                CHECK_DEAD_GOTO(fail);
                pa_threaded_mainloop_wait(p_sys->mainloop);
            }

        fail:

            pa_operation_unref(o);
        }

        pa_threaded_mainloop_unlock(p_sys->mainloop);
    }
    uninit(p_aout);
}

static void uninit(aout_instance_t *p_aout){
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    if (p_sys->mainloop)
        pa_threaded_mainloop_stop(p_sys->mainloop);

    if (p_sys->stream) {
        pa_stream_disconnect(p_sys->stream);
        pa_stream_unref(p_sys->stream);
        p_sys->stream = NULL;
    }

    if (p_sys->context) {
        pa_context_disconnect(p_sys->context);
        pa_context_unref(p_sys->context);
        p_sys->context = NULL;
    }

    if (p_sys->mainloop) {
        pa_threaded_mainloop_free(p_sys->mainloop);
        p_sys->mainloop = NULL;
    }

    free(p_sys);
    p_aout->output.p_sys = NULL;
}

static void context_state_cb(pa_context *c, void *userdata) {
    aout_instance_t *p_aout = (aout_instance_t *)userdata;
    struct aout_sys_t * p_sys = (struct aout_sys_t *) p_aout->output.p_sys;

    assert(c);

    PULSE_DEBUG( "Pulse context state changed");

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_READY:
        case PA_CONTEXT_TERMINATED:
        case PA_CONTEXT_FAILED:
        PULSE_DEBUG( "Pulse context state changed signal");
            pa_threaded_mainloop_signal(p_sys->mainloop, 0);
            break;

        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
        PULSE_DEBUG( "Pulse context state changed no signal");
            break;
    }
}

static void stream_state_cb(pa_stream *s, void * userdata) {
    aout_instance_t *p_aout = (aout_instance_t *)userdata;
    struct aout_sys_t * p_sys = (struct aout_sys_t *) p_aout->output.p_sys;

    assert(s);

    PULSE_DEBUG( "Pulse stream state changed");

    switch (pa_stream_get_state(s)) {

        case PA_STREAM_READY:
        case PA_STREAM_FAILED:
        case PA_STREAM_TERMINATED:
            pa_threaded_mainloop_signal(p_sys->mainloop, 0);
            break;

        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
            break;
    }
}

static void stream_request_cb(pa_stream *s, size_t length, void *userdata) {
    aout_instance_t *p_aout = (aout_instance_t *)userdata;
    struct aout_sys_t * p_sys = (struct aout_sys_t *) p_aout->output.p_sys;
    mtime_t next_date;

    assert(s);
    assert(p_sys);

    size_t buffer_size = p_sys->buffer_size;

    PULSE_DEBUG( "Pulse stream request %d", length);

    do{
        aout_buffer_t *   p_buffer = NULL;
        if(p_sys->started){
            pa_usec_t latency;
            int negative;
            if(pa_stream_get_latency(p_sys->stream, &latency, &negative)<0){
                if (pa_context_errno(p_sys->context) != PA_ERR_NODATA) {
                    msg_Err(p_aout, "pa_stream_get_latency() failed: %s", pa_strerror(pa_context_errno(p_sys->context)));
                }
                latency = 0;

            }
            PULSE_DEBUG( "Pulse stream request latency=%"PRId64"", latency);
            next_date = mdate() + latency;


            if(p_sys->start_date < next_date + AOUT_PTS_TOLERANCE ){
    /*
                  vlc_mutex_lock( &p_aout->output_fifo_lock );
                p_buffer = aout_FifoPop( p_aout, &p_aout->output.fifo );
                vlc_mutex_unlock( &p_aout->output_fifo_lock );
    */
                p_buffer = aout_OutputNextBuffer( p_aout, next_date, 0);
            }
        }

        if ( p_buffer != NULL )
        {
            PULSE_DEBUG( "Pulse stream request write buffer %d", p_buffer->i_nb_bytes);
            pa_stream_write(p_sys->stream, p_buffer->p_buffer, p_buffer->i_nb_bytes, NULL, 0, PA_SEEK_RELATIVE);
            length -= p_buffer->i_nb_bytes;
            aout_BufferFree( p_buffer );
        }
        else
        {
            PULSE_DEBUG( "Pulse stream request write zeroes");
            void *data = pa_xmalloc(buffer_size);
            bzero(data, buffer_size);
            pa_stream_write(p_sys->stream, data, buffer_size, pa_xfree, 0, PA_SEEK_RELATIVE);
            length -= buffer_size;
        }
    }while(length > buffer_size);

    pa_threaded_mainloop_signal(p_sys->mainloop, 0);
}

static void stream_latency_update_cb(pa_stream *s, void *userdata) {
    aout_instance_t *p_aout = (aout_instance_t *)userdata;
    struct aout_sys_t * p_sys = (struct aout_sys_t *) p_aout->output.p_sys;

    assert(s);

    PULSE_DEBUG( "Pulse stream latency update");

    pa_threaded_mainloop_signal(p_sys->mainloop, 0);
}

static void success_cb(pa_stream *s, int sucess, void *userdata)
{
    aout_instance_t *p_aout = (aout_instance_t *)userdata;
    struct aout_sys_t * p_sys = (struct aout_sys_t *) p_aout->output.p_sys;

    VLC_UNUSED(sucess);

    assert(s);

    pa_threaded_mainloop_signal(p_sys->mainloop, 0);
}

#undef PULSE_DEBUG
