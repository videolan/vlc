/*****************************************************************************
 * @file pipewire.c
 * @brief PipeWire input plugin for vlc
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Authors: Ayush Dey <deyayush6@gmail.com>
 *          Thomas Guillem <tguillem@videolan.org>
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
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <spa/param/video/format-utils.h>
#include <pipewire/pipewire.h>
#include "audio_output/vlc_pipewire.h"

#define HELP_TEXT N_( \
    "Pass pipewire:// to open the default PipeWire source, " \
    "or pipewire://SOURCE to open a specific source named SOURCE.")

struct vlc_pw_stream
{
    struct vlc_pw_context *context;
    struct pw_stream *stream;
    struct spa_hook listener;
    demux_t *demux;
    struct spa_audio_info audio_format; /**< Audio information about the stream */
    struct spa_video_info video_format; /**< Video information about the stream */
    bool es_out_added; /**< Whether es_format_t structure created */
};

struct demux_sys_t
{
    struct vlc_pw_context *context;
    struct vlc_pw_stream *stream;
    struct spa_hook listener;
    es_out_id_t *es;
    unsigned framesize; /**< Byte size of a sample */
    vlc_tick_t caching; /**< Caching value */
    vlc_tick_t interval;
    bool discontinuity; /**< The next frame will not follow the last one */
    enum es_format_category_e media_type;
    uint8_t chans_table[AOUT_CHAN_MAX]; /**< Channels order table */
    uint8_t chans_to_reorder; /**< Number of channels to reorder */
    vlc_fourcc_t format; /**< Sample format */
};

/** PipeWire audio sample (PCM) format to VLC codec */
static vlc_fourcc_t spa_audio_format_to_fourcc(enum spa_audio_format spa_format)
{
    switch (spa_format)
    {
        case SPA_AUDIO_FORMAT_U8:
            return VLC_CODEC_U8;
        case SPA_AUDIO_FORMAT_ALAW:
            return VLC_CODEC_ALAW;
        case SPA_AUDIO_FORMAT_ULAW:
            return VLC_CODEC_MULAW;
        case SPA_AUDIO_FORMAT_S16_LE:
            return VLC_CODEC_S16L;
        case SPA_AUDIO_FORMAT_S16_BE:
            return VLC_CODEC_S16B;
        case SPA_AUDIO_FORMAT_F32_LE:
            return VLC_CODEC_F32L;
        case SPA_AUDIO_FORMAT_F32_BE:
            return VLC_CODEC_F32B;
        case SPA_AUDIO_FORMAT_S32_LE:
            return VLC_CODEC_S32L;
        case SPA_AUDIO_FORMAT_S32_BE:
            return VLC_CODEC_S32B;
        case SPA_AUDIO_FORMAT_S24_LE:
            return VLC_CODEC_S24L;
        case SPA_AUDIO_FORMAT_S24_BE:
            return VLC_CODEC_S24B;
        case SPA_AUDIO_FORMAT_S24_32_LE:
            return VLC_CODEC_S24L32;
        case SPA_AUDIO_FORMAT_S24_32_BE:
            return VLC_CODEC_S24B32;
        default:
            return 0;
    }
}

/** PipeWire video sample (PCM) format to VLC codec */
static vlc_fourcc_t spa_video_format_to_fourcc(enum spa_video_format spa_format)
{
    switch (spa_format)
    {
        case SPA_VIDEO_FORMAT_YUY2:
            return VLC_CODEC_YUYV;
        case SPA_VIDEO_FORMAT_RGB:
            return VLC_CODEC_RGB24;
        case SPA_VIDEO_FORMAT_RGBA:
            return VLC_CODEC_RGBA;
        case SPA_VIDEO_FORMAT_RGBx:
            return VLC_CODEC_RGBX;
        case SPA_VIDEO_FORMAT_BGRx:
            return VLC_CODEC_BGRX;
        default:
            return 0;
    }
}

/** PipeWire to VLC channel mapping */
static const uint16_t vlc_chans[] = {
    [SPA_AUDIO_CHANNEL_MONO] = AOUT_CHAN_CENTER,
    [SPA_AUDIO_CHANNEL_FL]   = AOUT_CHAN_LEFT,
    [SPA_AUDIO_CHANNEL_FR]   = AOUT_CHAN_RIGHT,
    [SPA_AUDIO_CHANNEL_RL]   = AOUT_CHAN_REARLEFT,
    [SPA_AUDIO_CHANNEL_RR]   = AOUT_CHAN_REARRIGHT,
    [SPA_AUDIO_CHANNEL_FC]   = AOUT_CHAN_CENTER,
    [SPA_AUDIO_CHANNEL_LFE]  = AOUT_CHAN_LFE,
    [SPA_AUDIO_CHANNEL_SL]   = AOUT_CHAN_MIDDLELEFT,
    [SPA_AUDIO_CHANNEL_SR]   = AOUT_CHAN_MIDDLERIGHT,
    [SPA_AUDIO_CHANNEL_RC]   = AOUT_CHAN_REARCENTER,
};

/**
 * Initializes the `es_format_t fmt` object (audio properties)
 */
static int initialize_audio_format(struct vlc_pw_stream *s, es_format_t *fmt, vlc_fourcc_t format)
{
    demux_t *demux = s->demux;
    struct demux_sys_t *sys = demux->p_sys;
    es_format_Init(fmt, AUDIO_ES, format);
    uint32_t channels = s->audio_format.info.raw.channels;

    if (channels == 0)
    {
        vlc_pw_error(sys->context, "source should have at least one channel");
        return -1;
    }

    uint32_t chans_in[AOUT_CHAN_MAX];
    for (uint32_t i = 0; i < channels; i++)
    {
        uint16_t vlc_chan = 0;
        uint32_t pos = s->audio_format.info.raw.position[i];
        if (pos < ARRAY_SIZE(vlc_chans))
            vlc_chan = vlc_chans[pos];

        if (vlc_chan == 0)
        {
            vlc_pw_error(sys->context, "%s channel %u position %u", "unsupported", i, pos);
            return -1;
        }
        chans_in[i] = vlc_chan;
        fmt->audio.i_physical_channels |= vlc_chan;
    }

    sys->chans_to_reorder = aout_CheckChannelReorder(chans_in, NULL, fmt->audio.i_physical_channels,
                                                     sys->chans_table);
    sys->format = format;
    fmt->audio.i_format = format;
    aout_FormatPrepare(&fmt->audio);
    fmt->audio.i_rate = s->audio_format.info.raw.rate;
    fmt->audio.i_blockalign = fmt->audio.i_bitspersample * fmt->audio.i_channels / 8;
    fmt->i_bitrate = fmt->audio.i_bitspersample * fmt->audio.i_channels * fmt->audio.i_rate;
    sys->framesize = fmt->audio.i_blockalign;
    return 0;
}

/**
 * Initializes the `es_format_t fmt` object (video properties)
 */
static void initialize_video_format(struct vlc_pw_stream *s, es_format_t *fmt, vlc_fourcc_t format)
{
    demux_t *demux = s->demux;
    struct demux_sys_t *sys = demux->p_sys;
    es_format_Init(fmt, VIDEO_ES, format);
    fmt->video.i_frame_rate = s->video_format.info.raw.framerate.num;
    fmt->video.i_frame_rate_base = s->video_format.info.raw.framerate.denom;
    video_format_Setup(&fmt->video, format, s->video_format.info.raw.size.width,
                        s->video_format.info.raw.size.height, s->video_format.info.raw.size.width,
                        s->video_format.info.raw.size.height,
                        s->video_format.info.raw.pixel_aspect_ratio.num,
                        s->video_format.info.raw.pixel_aspect_ratio.denom);
    sys->interval = vlc_tick_from_samples(fmt->video.i_frame_rate,
                                            fmt->video.i_frame_rate_base);
}

/**
 * Parameter change callback.
 *
 * This callback is invoked upon stream initialization and
 * the ES is initialized before the stream starts capturing data.
 */
static void on_stream_param_changed(void *data, uint32_t id, const struct spa_pod *param)
{
    struct vlc_pw_stream *s = data;

    if(s->es_out_added) /* ES already initialized and added */
        return;

    demux_t *demux = s->demux;
    struct demux_sys_t *sys = demux->p_sys;
    es_format_t fmt;
    if (param == NULL || id != SPA_PARAM_Format)
        return;

    if (sys->media_type == AUDIO_ES)
    {
        if (spa_format_parse(param, &s->audio_format.media_type, &s->audio_format.media_subtype) < 0)
            return;

        if (s->audio_format.media_type != SPA_MEDIA_TYPE_audio ||
            s->audio_format.media_subtype != SPA_MEDIA_SUBTYPE_raw)
            return;

        if (spa_format_audio_raw_parse(param, &s->audio_format.info.raw) < 0)
            return;

        vlc_fourcc_t format = spa_audio_format_to_fourcc(s->audio_format.info.raw.format);
        if (format == 0)
        {
            vlc_pw_error(sys->context, "unsupported PipeWire sample format %u",
                        (unsigned)s->audio_format.info.raw.format);
            return;
        }

        if (initialize_audio_format(s, &fmt, format))
            return;
    }
    else
    {
        if (spa_format_parse(param, &s->video_format.media_type, &s->video_format.media_subtype) < 0)
            return;

        if (s->video_format.media_type != SPA_MEDIA_TYPE_video ||
            s->video_format.media_subtype != SPA_MEDIA_SUBTYPE_raw)
            return;

        if (spa_format_video_raw_parse(param, &s->video_format.info.raw) < 0)
            return;

        vlc_fourcc_t format = spa_video_format_to_fourcc(s->video_format.info.raw.format);
        if (format == 0)
        {
            vlc_pw_error(sys->context, "unsupported PipeWire sample format %u",
                        (unsigned)s->video_format.info.raw.format);
            return;
        }

        initialize_video_format(s, &fmt, format);
    }

    sys->es = es_out_Add (demux->out, &fmt);
    s->es_out_added = true;
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
        vlc_pw_error(s->context, "stream error: %s", err);
    else
        vlc_pw_debug(s->context, "stream %s",
                                pw_stream_state_as_string(state));
    if (old != state)
        vlc_pw_signal(s->context);
}

/**
 * Retrieve latest timings
 */
static vlc_tick_t stream_update_latency(struct vlc_pw_stream *s, vlc_tick_t *now)
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
        return VLC_TICK_INVALID;

    return (VLC_TICK_FROM_NS(ts.now) + vlc_tick_from_frac(ts.delay * ts.rate.num, ts.rate.denom));
}

/**
 * Stream processing callback.
 *
 * This consumes data from the server buffer.
 */
static void on_process(void *data)
{
    struct vlc_pw_stream *s = data;
    demux_t *demux = s->demux;
    struct demux_sys_t *sys = demux->p_sys;
    vlc_tick_t now;
    vlc_tick_t val = stream_update_latency(s, &now);
    struct pw_buffer *b = pw_stream_dequeue_buffer(s->stream);

    if (unlikely(b == NULL))
        return;

    struct spa_buffer *buf = b->buffer;
    struct spa_data *d = &buf->datas[0];
    struct spa_chunk *chunk = d->chunk;
    unsigned char *dst = d->data;
    size_t length = chunk->size;

    /*
     * If timing data is not available (val == VLC_TICK_INVALID), we can at least
     * assume that the capture delay is no less than zero.
     */
    vlc_tick_t pts = (val != VLC_TICK_INVALID) ? val : now;
    es_out_SetPCR(demux->out, pts);
    if (unlikely(sys->es == NULL))
        goto end;

    vlc_frame_t *frame = vlc_frame_Alloc(length);
    if (likely(frame != NULL))
    {
        memcpy(frame->p_buffer, dst + chunk->offset, length);
        if (sys->media_type == AUDIO_ES)
        {
            frame->i_nb_samples = length / sys->framesize;
            if (sys->chans_to_reorder != 0)
                aout_ChannelReorder(frame->p_buffer, length,
                                    sys->chans_to_reorder, sys->chans_table,
                                    sys->format);
        }
        frame->i_dts = frame->i_pts = pts;
        if (sys->discontinuity)
        {
            frame->i_flags |= VLC_FRAME_FLAG_DISCONTINUITY;
            sys->discontinuity = false;
        }

        es_out_Send(demux->out, sys->es, frame);
    }
    else
        sys->discontinuity = true;

    end:
        pw_stream_queue_buffer(s->stream, b);
}

/**
 * Disconnects a stream from source and destroys it.
 */
static void vlc_pw_stream_destroy(struct vlc_pw_stream *s)
{
    vlc_pw_lock(s->context);
    pw_stream_flush(s->stream, false);
    pw_stream_disconnect(s->stream);
    pw_stream_destroy(s->stream);
    vlc_pw_unlock(s->context);
    free(s);
}

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .state_changed = stream_state_changed,
    .param_changed = on_stream_param_changed,
    .process = on_process,
};

static int Control(demux_t *demux, int query, va_list ap)
{
    struct demux_sys_t *sys = demux->p_sys;
    struct vlc_pw_stream *s = sys->stream;
    
    switch (query)
    {
        case DEMUX_GET_TIME:
        {
            struct pw_time ts;
            
            if (pw_stream_get_time_n(s->stream, &ts, sizeof(ts)) < 0)
                return VLC_EGENERIC;
            *(va_arg(ap, vlc_tick_t *)) = VLC_TICK_FROM_NS(ts.now);
            break;
        }

        case DEMUX_GET_PTS_DELAY:
        {
            vlc_tick_t *pd = va_arg(ap, vlc_tick_t *);

            *pd = sys->caching;
            /* in case of VIDEO_ES, cap at one frame, more than enough */
            if (sys->media_type == VIDEO_ES && *pd > sys->interval)
                *pd = sys->interval;
            break;
        }

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

/**
 * Global callback
 * This callback is used to find whether the media_type is AUDIO_ES or VIDEO_ES.
 */
static void registry_global(void *data, uint32_t id, uint32_t perms,
                            const char *type, uint32_t version,
                            const struct spa_dict *props)
{
    demux_t *demux = data;
    struct demux_sys_t *sys = demux->p_sys;

    if (strcmp(type, PW_TYPE_INTERFACE_Node) == 0)
    {
        const char *name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
        if (unlikely(name == NULL))
            return;

        const char *class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
        if (class == NULL)
            return;
    
        if (strstr(class, "Source") == NULL)
            return;

        if (strcmp(name, demux->psz_location) == 0)
        {
            if (strstr(class, "Video") != NULL)
                sys->media_type = VIDEO_ES;
        }
    }
}

static const struct pw_registry_events global_events = {
    PW_VERSION_REGISTRY_EVENTS,
    .global = registry_global
};

static int Open(vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;

    if (demux->out == NULL)
        return VLC_EGENERIC;

    struct demux_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->context = vlc_pw_connect(obj, "access");
    if (sys->context == NULL)
    {
        free(sys);
        return VLC_EGENERIC;
    }

    sys->stream = NULL;
    sys->es = NULL;
    sys->discontinuity = false;
    sys->caching = VLC_TICK_FROM_MS( var_InheritInteger(obj, "live-caching") );
    sys->listener = (struct spa_hook){ };
    sys->media_type = AUDIO_ES;
    demux->p_sys = sys;

    /* The `sys->media_type` is set to either `AUDIO_ES` or `VIDEO_ES` during this call,
       depending on the media type of the capture node. */
    vlc_pw_lock(sys->context);
    vlc_pw_registry_listen(sys->context, &sys->listener, &global_events, demux);
    vlc_pw_roundtrip_unlocked(sys->context);
    vlc_pw_unlock(sys->context);

    /* Stream parameters */
    struct spa_audio_info_raw rawaudiofmt = {
        .rate = 48000,
        .channels = 2,
        .format = SPA_AUDIO_FORMAT_S16,
        .position = { SPA_AUDIO_CHANNEL_FL, SPA_AUDIO_CHANNEL_FR }
    };

    /* Assemble the stream format and properties */
    const struct spa_pod *params[1];
    unsigned char buf[1024];
    struct spa_pod_builder builder = SPA_POD_BUILDER_INIT(buf, sizeof (buf));
    struct pw_properties *props;
    const char *stream_name;

    if (sys->media_type == AUDIO_ES)
    {
        params[0] = spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat,
                                                &rawaudiofmt);

        props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                                PW_KEY_MEDIA_CATEGORY, "Capture",
                                PW_KEY_MEDIA_ROLE, "Raw",
                                NULL);
        stream_name = "audio stream";
    }
    else
    {
        params[0] = spa_pod_builder_add_object(&builder,
                SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
                SPA_FORMAT_mediaType,       SPA_POD_Id(SPA_MEDIA_TYPE_video),
                SPA_FORMAT_mediaSubtype,    SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
                SPA_FORMAT_VIDEO_format,    SPA_POD_CHOICE_ENUM_Id(6,
                                                SPA_VIDEO_FORMAT_RGB,
                                                SPA_VIDEO_FORMAT_RGB,
                                                SPA_VIDEO_FORMAT_RGBA,
                                                SPA_VIDEO_FORMAT_RGBx,
                                                SPA_VIDEO_FORMAT_BGRx,
                                                SPA_VIDEO_FORMAT_YUY2),
                SPA_FORMAT_VIDEO_size,      SPA_POD_CHOICE_RANGE_Rectangle(
                                                &SPA_RECTANGLE(320, 240),
                                                &SPA_RECTANGLE(1, 1),
                                                &SPA_RECTANGLE(4096, 4096)),
                SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(
                                                &SPA_FRACTION(25, 1),
                                                &SPA_FRACTION(0, 1),
                                                &SPA_FRACTION(1000, 1)));

        props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Video",
                                PW_KEY_MEDIA_CATEGORY, "Capture",
                                PW_KEY_MEDIA_ROLE, "Camera",
                                NULL);
        stream_name = "video stream";
    }

    pw_properties_set(props, PW_KEY_TARGET_OBJECT, demux->psz_location);

    /* Create the stream */
    struct vlc_pw_stream *s = malloc(sizeof (*s));
    if (unlikely(s == NULL))
    {
        pw_properties_free(props);
        vlc_pw_disconnect(sys->context);
        free(sys);
        return VLC_EGENERIC;
    }

    enum pw_stream_flags flags =
        PW_STREAM_FLAG_AUTOCONNECT |
        PW_STREAM_FLAG_MAP_BUFFERS;

    enum pw_stream_state state;

    s->context = sys->context;
    s->listener = (struct spa_hook){ };
    s->es_out_added = false;
    s->demux = demux;

    vlc_pw_lock(s->context);
    s->stream = vlc_pw_stream_new(s->context, stream_name, props);
    if (unlikely(s->stream == NULL))
    {
        vlc_pw_unlock(s->context);
        free(s);
        vlc_pw_disconnect(sys->context);
        free(sys);
        return VLC_EGENERIC;
    }

    sys->stream = s;
    pw_stream_add_listener(s->stream, &s->listener, &stream_events, s);
    pw_stream_connect(s->stream, PW_DIRECTION_INPUT, PW_ID_ANY, flags,
                      params, ARRAY_SIZE(params));

    /* Wait for the stream to be ready */
    while ((state = pw_stream_get_state(s->stream,
                                        NULL)) == PW_STREAM_STATE_CONNECTING)
        vlc_pw_wait(s->context);

    vlc_pw_unlock(s->context);

    switch (state)
    {
        case PW_STREAM_STATE_PAUSED:
        case PW_STREAM_STATE_STREAMING:
            break;
        default:
            vlc_pw_stream_destroy(s);
            vlc_pw_disconnect(sys->context);
            free(sys);
            return VLC_EGENERIC;
    }

    demux->pf_demux = NULL;
    demux->pf_control = Control;
    return VLC_SUCCESS;
}

static void Close (vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    struct demux_sys_t *sys = demux->p_sys;
    vlc_pw_stream_destroy(sys->stream);
    vlc_pw_disconnect(sys->context);
    free(sys);
}

vlc_module_begin ()
    set_shortname (N_("PipeWire"))
    set_description (N_("PipeWire input"))
    set_capability ("access", 0)
    set_subcategory (SUBCAT_INPUT_ACCESS)
    set_help (HELP_TEXT)

    add_shortcut ("pipewire", "pw")
    set_callbacks (Open, Close)
vlc_module_end ()
