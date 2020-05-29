/*****************************************************************************
 * mock.c : mock demux module for vlc
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
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

#include <ctype.h>
#include <limits.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_picture.h>
#include <vlc_demux.h>
#include <vlc_input.h>
#include <vlc_vector.h>


struct mock_track
{
    es_format_t fmt;
    es_out_id_t *id;
};
typedef struct VLC_VECTOR(struct mock_track *) mock_track_vector;

static ssize_t
var_InheritSsize(vlc_object_t *obj, const char *name)
{
    int64_t value = var_InheritInteger(obj, name);
    return value >= 0 ? value : -1;
}

static unsigned
var_InheritUnsigned(vlc_object_t *obj, const char *name)
{
    int64_t value = var_InheritInteger(obj, name);
    return value >= 0 && value < UINT_MAX ? value : UINT_MAX;
}

static vlc_fourcc_t
var_InheritFourcc(vlc_object_t *obj, const char *name)
{
    char *var_value = var_InheritString(obj, name);
    if (!var_value)
        return 0;

    size_t var_len = strlen(var_value);
    if (var_len > 4)
    {
        free(var_value);
        return 0;
    }

    /* Pad with spaces if the string len is less than 4 */
    char value[] = "    ";
    strcpy(value, var_value);
    if (var_len != 4)
        value[var_len] = ' ';
    free(var_value);

    vlc_fourcc_t fourcc;
    memcpy(&fourcc, value, 4);
    return fourcc;
}

#define OPTIONS_AUDIO(Y) \
    Y(audio, channels, unsigned, add_integer, Unsigned, 2) \
    Y(audio, format, vlc_fourcc_t, add_string, Fourcc, "u8") \
    Y(audio, rate, unsigned, add_integer, Unsigned, 44100) \
    Y(audio, packetized, bool, add_bool, Bool, true) \
    Y(audio, sample_length, vlc_tick_t, add_integer, Integer, VLC_TICK_FROM_MS(40) )

#define OPTIONS_VIDEO(Y) \
    Y(video, chroma, vlc_fourcc_t, add_string, Fourcc, "I420") \
    Y(video, width, unsigned, add_integer, Unsigned, 640) \
    Y(video, height, unsigned, add_integer, Unsigned, 480) \
    Y(video, frame_rate, unsigned, add_integer, Unsigned, 25) \
    Y(video, frame_rate_base, unsigned, add_integer, Unsigned, 1) \
    Y(video, packetized, bool, add_bool, Bool, true)

#define OPTIONS_SUB(Y) \
    Y(sub, packetized, bool, add_bool, Bool, true)

/* var_name, type, module_header_type, getter, default_value */
#define OPTIONS_GLOBAL(X) \
    X(length, vlc_tick_t, add_integer, Integer, VLC_TICK_FROM_MS(5000)) \
    X(audio_track_count, ssize_t, add_integer, Ssize, 0) \
    X(video_track_count, ssize_t, add_integer, Ssize, 0) \
    X(sub_track_count, ssize_t, add_integer, Ssize, 0) \
    X(input_sample_length, vlc_tick_t, add_integer, Integer, VLC_TICK_FROM_MS(40) ) \
    X(title_count, ssize_t, add_integer, Ssize, 0 ) \
    X(chapter_count, ssize_t, add_integer, Ssize, 0) \
    X(null_names, bool, add_bool, Bool, false) \
    X(program_count, ssize_t, add_integer, Ssize, 0) \
    X(can_seek, bool, add_bool, Bool, true) \
    X(can_pause, bool, add_bool, Bool, true) \
    X(can_control_pace, bool, add_bool, Bool, true) \
    X(can_control_rate, bool, add_bool, Bool, true) \
    X(can_record, bool, add_bool, Bool, true) \
    X(error, bool, add_bool, Bool, false) \
    X(pts_delay, unsigned, add_integer, Unsigned, MS_FROM_VLC_TICK(DEFAULT_PTS_DELAY)) \
    X(add_video_track_at, vlc_tick_t, add_integer, Integer, VLC_TICK_INVALID ) \
    X(add_audio_track_at, vlc_tick_t, add_integer, Integer, VLC_TICK_INVALID ) \
    X(add_spu_track_at, vlc_tick_t, add_integer, Integer, VLC_TICK_INVALID ) \
    X(config, char *, add_string, String, NULL )

#define DECLARE_OPTION(var_name, type, module_header_type, getter, default_value)\
    type var_name;
#define DECLARE_SUBOPTION(a,b,c,d,e,f) DECLARE_OPTION(b,c,d,e,f)

#define READ(var_name, member_name, getter) \
    sys->member_name = var_Inherit##getter(obj, "mock-"#var_name);
#define READ_OPTION(var_name, type, module_header_type, getter, default_value) \
    READ(var_name, var_name, getter)
#define READ_SUBOPTION(group_name, var_name, type, module_header_type, getter, default_value) \
    READ(group_name##_##var_name, group_name.var_name, getter)

#define DECLARE_MODULE_OPTIONS(var_name, type, module_header_type, getter, default_value) \
    module_header_type("mock-"#var_name, default_value, NULL, NULL, true) \
    change_volatile() \
    change_safe()
#define DECLARE_MODULE_SUBOPTIONS(a,b,c,d,e,f) \
    DECLARE_MODULE_OPTIONS(a##_##b,c,d,e,f)

struct mock_video_options
{
    OPTIONS_VIDEO(DECLARE_SUBOPTION)
};

struct mock_audio_options
{
    OPTIONS_AUDIO(DECLARE_SUBOPTION)
};

struct mock_sub_options
{
    OPTIONS_SUB(DECLARE_SUBOPTION)
};

struct demux_sys
{
    mock_track_vector tracks;
    vlc_tick_t pts;
    vlc_tick_t audio_pts;
    vlc_tick_t video_pts;

    int current_title;
    vlc_tick_t chapter_gap;

    unsigned int updates;
    OPTIONS_GLOBAL(DECLARE_OPTION)
    struct mock_video_options video;
    struct mock_audio_options audio;
    struct mock_sub_options sub;
};
#undef X

static input_title_t *
CreateTitle(demux_t *demux, size_t idx)
{
    struct demux_sys *sys = demux->p_sys;

    input_title_t *t = vlc_input_title_New();
    if (!t)
        return NULL;

    t->i_length = sys->length;
    if (!sys->null_names
     && asprintf(&t->psz_name, "Mock Title %zu", idx) == -1)
    {
        t->psz_name = NULL;
        vlc_input_title_Delete(t);
        return NULL;
    }
    t->seekpoint = vlc_alloc(sys->chapter_count, sizeof(*t->seekpoint));
    if (!t->seekpoint)
    {
        vlc_input_title_Delete(t);
        return NULL;
    }

    for (ssize_t i = 0; i < sys->chapter_count; ++i)
    {
        t->seekpoint[i] = vlc_seekpoint_New();
        if (!t->seekpoint[i])
        {
            vlc_input_title_Delete(t);
            return NULL;
        }
        t->i_seekpoint++;
        if (!sys->null_names
         && asprintf(&t->seekpoint[i]->psz_name, "Mock Chapter %zu-%zu", idx, i)
            == -1)
        {
            vlc_input_title_Delete(t);
            return NULL;
        }
        t->seekpoint[i]->i_time_offset = i * sys->chapter_gap;
    }
    return t;
}

static int
Control(demux_t *demux, int query, va_list args)
{
    struct demux_sys *sys = demux->p_sys;

    switch (query)
    {
        case DEMUX_CAN_SEEK:
            *va_arg(args, bool *) = sys->can_seek;
            return VLC_SUCCESS;
        case DEMUX_CAN_PAUSE:
            *va_arg(args, bool *) = sys->can_pause;
            return VLC_SUCCESS;
        case DEMUX_CAN_CONTROL_PACE:
            *va_arg(args, bool *) = sys->can_control_pace;
            return VLC_SUCCESS;
        case DEMUX_GET_PTS_DELAY:
            *va_arg(args, vlc_tick_t *) = VLC_TICK_FROM_MS(sys->pts_delay);
            return VLC_SUCCESS;
        case DEMUX_GET_META:
            return VLC_EGENERIC;
        case DEMUX_GET_SIGNAL:
            return VLC_EGENERIC;
        case DEMUX_SET_PAUSE_STATE:
            return sys->can_pause ? VLC_SUCCESS : VLC_EGENERIC;
        case DEMUX_SET_TITLE:
            if (sys->title_count > 0)
            {
                int new_title = va_arg(args, int);
                if (new_title >= sys->title_count)
                    return VLC_EGENERIC;
                sys->current_title = new_title;
                sys->pts = sys->audio_pts = sys->video_pts = VLC_TICK_0;
                sys->updates |= INPUT_UPDATE_TITLE;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        case DEMUX_SET_SEEKPOINT:
            if (sys->chapter_gap != VLC_TICK_INVALID)
            {
                const int seekpoint_idx = va_arg(args, int);
                if (seekpoint_idx < sys->chapter_count)
                {
                    sys->pts = sys->audio_pts = sys->video_pts =
                        (seekpoint_idx * sys->chapter_gap) + VLC_TICK_0;
                    return VLC_SUCCESS;
                }
            }
            return VLC_EGENERIC;
        case DEMUX_TEST_AND_CLEAR_FLAGS:
        {
            unsigned *restrict flags = va_arg(args, unsigned *);
            *flags &= sys->updates;
            sys->updates &= ~*flags;
            return VLC_SUCCESS;
        }
        case DEMUX_GET_TITLE:
            if (sys->title_count > 0)
            {
                *va_arg(args, int *) = sys->current_title;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        case DEMUX_GET_SEEKPOINT:
            if (sys->chapter_gap != VLC_TICK_INVALID)
            {
                *va_arg(args, int *) = sys->pts / sys->chapter_gap;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        case DEMUX_GET_POSITION:
            *va_arg(args, double *) = sys->pts / (double) sys->length;
            return VLC_SUCCESS;
        case DEMUX_SET_POSITION:
            if (!sys->can_seek)
                return VLC_EGENERIC;
            sys->pts = sys->video_pts = sys->audio_pts = va_arg(args, double) * sys->length;
            return VLC_SUCCESS;
        case DEMUX_GET_LENGTH:
            *va_arg(args, vlc_tick_t *) = sys->length;
            return VLC_SUCCESS;
        case DEMUX_GET_TIME:
            *va_arg(args, vlc_tick_t *) = sys->pts;
            return VLC_SUCCESS;
        case DEMUX_SET_TIME:
            if (!sys->can_seek)
                return VLC_EGENERIC;
            sys->pts = sys->video_pts = sys->audio_pts = va_arg(args, vlc_tick_t);
            return VLC_SUCCESS;
        case DEMUX_GET_TITLE_INFO:
            if (sys->title_count > 0)
            {
                input_title_t ***titles = va_arg(args, input_title_t ***);
                *titles = vlc_alloc(sys->title_count, sizeof(*titles));
                if (!*titles)
                    return VLC_ENOMEM;
                for (ssize_t i = 0; i < sys->title_count; ++i)
                {
                    (*titles)[i] = CreateTitle(demux, i);
                    if (!(*titles)[i])
                    {
                        while (i--)
                            vlc_input_title_Delete((*titles)[i - 1]);
                        free(*titles);
                        *titles = NULL;
                        return VLC_ENOMEM;
                    }
                }
                *va_arg(args, int *) = sys->title_count;
                *va_arg(args, int *) = 0;
                *va_arg(args, int *) = 0;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        case DEMUX_SET_GROUP_DEFAULT:
            return VLC_EGENERIC;
        case DEMUX_SET_GROUP_ALL:
            return VLC_EGENERIC;
        case DEMUX_SET_GROUP_LIST:
            return VLC_EGENERIC;
        case DEMUX_SET_ES:
            return VLC_EGENERIC;
        case DEMUX_SET_ES_LIST:
            return VLC_EGENERIC;
        case DEMUX_SET_NEXT_DEMUX_TIME:
            return VLC_EGENERIC;
        case DEMUX_GET_FPS:
            return VLC_EGENERIC;
        case DEMUX_HAS_UNSUPPORTED_META:
            return VLC_EGENERIC;
        case DEMUX_GET_ATTACHMENTS:
            return VLC_EGENERIC;
        case DEMUX_CAN_RECORD:
            *va_arg(args, bool *) = sys->can_record;
            return VLC_SUCCESS;
        case DEMUX_SET_RECORD_STATE:
            return sys->can_record ? VLC_SUCCESS : VLC_EGENERIC;
        case DEMUX_CAN_CONTROL_RATE:
            *va_arg(args, bool *) = sys->can_control_rate;
            return VLC_SUCCESS;
        case DEMUX_SET_RATE:
            return sys->can_control_rate ? VLC_SUCCESS : VLC_EGENERIC;
        case DEMUX_IS_PLAYLIST:
            return VLC_EGENERIC;
        case DEMUX_NAV_ACTIVATE:
            return VLC_EGENERIC;
        case DEMUX_NAV_UP:
            return VLC_EGENERIC;
        case DEMUX_NAV_DOWN:
            return VLC_EGENERIC;
        case DEMUX_NAV_LEFT:
            return VLC_EGENERIC;
        case DEMUX_NAV_RIGHT:
            return VLC_EGENERIC;
        case DEMUX_NAV_POPUP:
            return VLC_EGENERIC;
        case DEMUX_NAV_MENU:
            return VLC_EGENERIC;
        default:
            return VLC_EGENERIC;
    }
}

static block_t *
CreateAudioBlock(demux_t *demux, struct mock_track *track, vlc_tick_t length)
{
    const int64_t samples =
        samples_from_vlc_tick(length, track->fmt.audio.i_rate);
    const int64_t bytes = samples / track->fmt.audio.i_frame_length
                        * track->fmt.audio.i_bytes_per_frame;
    block_t *b = block_Alloc(bytes);
    if (!b)
        return NULL;
    memset(b->p_buffer, 0, b->i_buffer);
    return b;
    (void) demux;
}

struct video_block
{
    block_t b;
    picture_t *pic;
};

static void
video_block_free_cb(block_t *b)
{
    struct video_block *video = container_of(b, struct video_block, b);
    picture_Release(video->pic);
    free(video);
}

static block_t *
CreateVideoBlock(demux_t *demux, struct mock_track *track)
{
    struct demux_sys *sys = demux->p_sys;
    picture_t *pic = picture_NewFromFormat(&track->fmt.video);
    if (!pic)
        return NULL;

    struct video_block *video = malloc(sizeof(*video));
    if (!video)
    {
        picture_Release(pic);
        return NULL;
    }
    video->pic = pic;

    static const struct vlc_block_callbacks cbs =
    {
        .free = video_block_free_cb
    };

    size_t block_len = 0;
    for (int i = 0; i < pic->i_planes; ++i)
        block_len += pic->p[i].i_lines * pic->p[i].i_pitch;
    memset(pic->p[0].p_pixels, (sys->video_pts / VLC_TICK_FROM_MS(10)) % 255,
           block_len);
    return block_Init(&video->b, &cbs, pic->p[0].p_pixels, block_len);
    (void) demux;
}

static block_t *
CreateSubBlock(demux_t *demux, struct mock_track *track)
{
    struct demux_sys *sys = demux->p_sys;
    char *text;
    if (asprintf(&text, "subtitle @ %"PRId64, sys->video_pts) == -1)
        return NULL;
    size_t len = strlen(text) + 1;

    block_t *b = block_Alloc(len);
    if (!b)
    {
        free(text);
        return NULL;
    }

    memcpy(b->p_buffer, text, len);
    b->i_buffer = len;

    free(text);
    return b;
    (void) track;
}

static int
AppendMockTrack(demux_t *demux, const es_format_t *fmt, int id, int group,
                bool packetized)
{
    struct demux_sys *sys = demux->p_sys;
    struct mock_track *mock_track = malloc(sizeof(*mock_track));
    if (!mock_track)
        return VLC_EGENERIC;
    mock_track->fmt = *fmt;
    mock_track->fmt.i_id = id;
    mock_track->fmt.i_group = group;
    mock_track->fmt.b_packetized = packetized;
    mock_track->id = es_out_Add(demux->out, & mock_track->fmt);
    if (!mock_track->id)
    {
        free(mock_track);
        return VLC_ENOMEM;
    }
    bool success = vlc_vector_push(&sys->tracks, mock_track);
    assert(success); (void) success; /* checked by reserve() */
    return VLC_SUCCESS;
}

static int
InitVideoTracks(demux_t *demux, int group, size_t count)
{
    struct demux_sys *sys = demux->p_sys;

    if (count == 0)
        return VLC_SUCCESS;

    const vlc_chroma_description_t *desc =
        vlc_fourcc_GetChromaDescription(sys->video.chroma);
    if (!desc || desc->plane_count == 0)
        sys->video.chroma = 0;

    const bool frame_rate_ok =
        sys->video.frame_rate != 0 && sys->video.frame_rate != UINT_MAX &&
        sys->video.frame_rate_base != 0 && sys->video.frame_rate_base != UINT_MAX;
    const bool chroma_ok = sys->video.chroma != 0;
    const bool size_ok = sys->video.width != UINT_MAX &&
                         sys->video.height != UINT_MAX;

    if (sys->video.frame_rate == 0 || sys->video.frame_rate_base == 0
     || sys->video.chroma == 0)
    if (!frame_rate_ok || !chroma_ok || !size_ok)
    {
        if (!frame_rate_ok)
            msg_Err(demux, "Invalid video frame rate");
        if (!chroma_ok)
            msg_Err(demux, "Invalid video chroma");
        if (!size_ok)
            msg_Err(demux, "Invalid video size");
        return VLC_EGENERIC;
    }

    for (size_t i = 0; i < count; ++i)
    {
        es_format_t fmt;
        es_format_Init(&fmt, VIDEO_ES, sys->video.chroma);
        fmt.video.i_chroma = fmt.i_codec;
        fmt.video.i_width = fmt.video.i_visible_width = sys->video.width;
        fmt.video.i_height = fmt.video.i_visible_height = sys->video.height;
        fmt.video.i_frame_rate = sys->video.frame_rate;
        fmt.video.i_frame_rate_base = sys->video.frame_rate_base;

        if (AppendMockTrack(demux, &fmt, i, group, sys->video.packetized))
            return VLC_ENOMEM;
    }
    return VLC_SUCCESS;
}

static int
InitAudioTracks(demux_t *demux, int group, size_t count)
{
    struct demux_sys *sys = demux->p_sys;

    if (count == 0)
        return VLC_SUCCESS;

    const bool rate_ok = sys->audio.rate > 0 && sys->audio.rate != UINT_MAX;
    const bool format_ok = aout_BitsPerSample(sys->audio.format) != 0;
    const bool channels_ok = sys->audio.channels > 0 &&
                             sys->audio.channels <= AOUT_CHAN_MAX;

    if (!rate_ok || !format_ok || !channels_ok)
    {
        if (!rate_ok)
            msg_Err(demux, "Invalid audio rate");
        if (!format_ok)
            msg_Err(demux, "Invalid audio format");
        if (!channels_ok)
            msg_Err(demux, "Invalid audio channels");
        return VLC_EGENERIC;
    }

    uint16_t physical_channels = 0;
    switch (sys->audio.channels)
    {
        case 1: physical_channels = AOUT_CHAN_CENTER; break;
        case 2: physical_channels = AOUT_CHANS_2_0; break;
        case 3: physical_channels = AOUT_CHANS_2_1; break;
        case 4: physical_channels = AOUT_CHANS_4_0; break;
        case 5: physical_channels = AOUT_CHANS_4_1; break;
        case 6: physical_channels = AOUT_CHANS_6_0; break;
        case 7: physical_channels = AOUT_CHANS_7_0; break;
        case 8: physical_channels = AOUT_CHANS_7_1; break;
        case 9: physical_channels = AOUT_CHANS_8_1; break;
        default: vlc_assert_unreachable();
    }

    for (size_t i = 0; i < count; ++i)
    {
        es_format_t fmt;
        es_format_Init(&fmt, AUDIO_ES, sys->audio.format);
        fmt.audio.i_format = fmt.i_codec;
        fmt.audio.i_rate = sys->audio.rate;
        fmt.audio.i_physical_channels = physical_channels;
        aout_FormatPrepare(&fmt.audio);

        if (AppendMockTrack(demux, &fmt, i, group, sys->audio.packetized))
            return VLC_ENOMEM;
    }

    return VLC_SUCCESS;
}

static int
InitSubTracks(demux_t *demux, int group, size_t count)
{
    struct demux_sys *sys = demux->p_sys;

    if (count == 0)
        return VLC_SUCCESS;

    for (size_t i = 0; i < count; ++i)
    {
        es_format_t fmt;
        es_format_Init(&fmt, SPU_ES, VLC_CODEC_SUBT);

        if (AppendMockTrack(demux, &fmt, i, group, sys->sub.packetized))
            return VLC_ENOMEM;
    }

    return VLC_SUCCESS;
}

static int
DemuxAudio(demux_t *demux, vlc_tick_t step_length, vlc_tick_t end_pts)
{
    struct demux_sys *sys = demux->p_sys;

    while (sys->audio_pts < end_pts)
    {
        struct mock_track *track;
        vlc_vector_foreach(track, &sys->tracks)
        {
            block_t *block;
            switch (track->fmt.i_cat)
            {
                case AUDIO_ES:
                    block = CreateAudioBlock(demux, track, step_length);
                    break;
                default:
                    continue;
            }
            if (!block)
                return VLC_EGENERIC;

            block->i_length = step_length;
            block->i_pts = block->i_dts = sys->audio_pts;

            int ret = es_out_Send(demux->out, track->id, block);
            if (ret != VLC_SUCCESS)
                return ret;
        }
        sys->audio_pts += step_length;
    }
    return VLC_SUCCESS;
}

static int
DemuxVideo(demux_t *demux, vlc_tick_t step_length, vlc_tick_t end_pts)
{
    struct demux_sys *sys = demux->p_sys;

    while (sys->video_pts < end_pts)
    {
        struct mock_track *track;
        vlc_vector_foreach(track, &sys->tracks)
        {
            block_t *block;
            switch (track->fmt.i_cat)
            {
                case VIDEO_ES:
                    block = CreateVideoBlock(demux, track);
                    break;
                case SPU_ES:
                    block = CreateSubBlock(demux, track);
                    break;
                default:
                    continue;
            }
            if (!block)
                return VLC_EGENERIC;

            block->i_length = step_length;
            block->i_pts = block->i_dts = sys->video_pts;

            int ret = es_out_Send(demux->out, track->id, block);
            if (ret != VLC_SUCCESS)
                return ret;
        }
        sys->video_pts += step_length;
    }
    return VLC_SUCCESS;
}

static int
Demux(demux_t *demux)
{
    struct demux_sys *sys = demux->p_sys;

    if (sys->error)
        return VLC_DEMUXER_EGENERIC;

    if (sys->audio_track_count > 0
     && (sys->video_track_count > 0 || sys->sub_track_count > 0))
        sys->pts = __MIN(sys->audio_pts, sys->video_pts);
    else if (sys->audio_track_count > 0)
        sys->pts = sys->audio_pts;
    else if (sys->video_track_count > 0 || sys->sub_track_count > 0)
        sys->pts = sys->video_pts;

    if (sys->pts > sys->length)
        sys->pts = sys->length;
    es_out_SetPCR(demux->out, sys->pts);

    const vlc_tick_t video_step_length =
        (sys->video_track_count > 0 || sys->sub_track_count > 0) ?
         VLC_TICK_FROM_SEC(1) * sys->video.frame_rate_base
                              / sys->video.frame_rate : 0;

    const vlc_tick_t audio_step_length =
        sys->audio_track_count > 0 ? sys->audio.sample_length : 0;

    const vlc_tick_t step_length = __MAX(audio_step_length, video_step_length);

    int ret = VLC_SUCCESS;
    bool audio_eof = true, video_eof = true, input_eof = true;
    if (sys->audio_track_count > 0)
    {
        ret = DemuxAudio(demux, audio_step_length,
                         __MIN(step_length + sys->audio_pts, sys->length));
        if (sys->audio_pts + audio_step_length < sys->length)
            audio_eof = false;
    }

    if (ret == VLC_SUCCESS
     && (sys->video_track_count > 0 || sys->sub_track_count > 0))
    {
        ret = DemuxVideo(demux, video_step_length,
                         __MIN(step_length + sys->video_pts, sys->length));
        if (sys->video_pts + video_step_length < sys->length)
            video_eof = false;
    }

    /* No audio/video/sub: simulate that we read some inputs */
    if (step_length == 0)
    {
        sys->pts += sys->input_sample_length;
        if (sys->pts + sys->input_sample_length < sys->length)
            input_eof = false;
    }

    if (ret != VLC_SUCCESS)
        return VLC_DEMUXER_EGENERIC;

    /* Add late tracks if any */
    if (sys->add_video_track_at != VLC_TICK_INVALID &&
        sys->add_video_track_at <= sys->pts)
    {
        sys->add_video_track_at = VLC_TICK_INVALID;

        ret = InitVideoTracks(demux, 0, 1);
        if (ret != VLC_SUCCESS)
            return VLC_DEMUXER_EGENERIC;
        sys->video_track_count++;
        sys->video_pts = sys->pts;
        video_eof = false;
    }

    if (sys->add_audio_track_at != VLC_TICK_INVALID &&
        sys->add_audio_track_at <= sys->pts)
    {
        sys->add_audio_track_at = VLC_TICK_INVALID;

        ret = InitAudioTracks(demux, 0, 1);
        if (ret != VLC_SUCCESS)
            return VLC_DEMUXER_EGENERIC;
        sys->audio_track_count++;
        sys->audio_pts = sys->pts;
        audio_eof = false;
    }

    if (sys->add_spu_track_at != VLC_TICK_INVALID &&
        sys->add_spu_track_at <= sys->pts)
    {
        sys->add_spu_track_at = VLC_TICK_INVALID;

        ret = InitSubTracks(demux, 0, 1);
        if (ret != VLC_SUCCESS)
            return VLC_DEMUXER_EGENERIC;
        sys->sub_track_count++;
        sys->video_pts = sys->pts;
        video_eof = false;
    }

    return audio_eof && video_eof && input_eof ? VLC_DEMUXER_EOF
                                               : VLC_DEMUXER_SUCCESS;
}

static void
Close(vlc_object_t *obj)
{
    demux_t *demux = (demux_t*)obj;
    struct demux_sys *sys = demux->p_sys;

    struct mock_track *track;
    vlc_vector_foreach(track, &sys->tracks)
    {
        es_out_Del(demux->out, track->id);
        free(track);
    }
    vlc_vector_clear(&sys->tracks);
}

static int
Open(vlc_object_t *obj)
{
    demux_t *demux = (demux_t*)obj;

    if (demux->out == NULL)
        return VLC_EGENERIC;
    struct demux_sys *sys = vlc_obj_malloc(obj, sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    demux->p_sys = sys;

    if (var_LocationParse(obj, demux->psz_location, "mock-") != VLC_SUCCESS)
        return VLC_ENOMEM;

    OPTIONS_GLOBAL(READ_OPTION)
    OPTIONS_AUDIO(READ_SUBOPTION)
    OPTIONS_VIDEO(READ_SUBOPTION)
    OPTIONS_SUB(READ_SUBOPTION)

    if (sys->chapter_count > 0 && sys->title_count == 0)
        sys->title_count++;

    const bool length_ok = sys->length >= 0;
    const bool tracks_ok = sys->video_track_count >= 0 &&
                           sys->audio_track_count >= 0 &&
                           sys->sub_track_count >= 0;
    const bool titles_ok = sys->title_count >= 0;
    const bool chapters_ok = sys->chapter_count >= 0;
    const bool programs_ok = sys->program_count >= 0;

    if (!length_ok || !tracks_ok || !titles_ok || !chapters_ok || !programs_ok)
    {
        if (!length_ok)
            msg_Err(demux, "Invalid length");
        if (!tracks_ok)
            msg_Err(demux, "Invalid track count");
        if (!titles_ok)
            msg_Err(demux, "Invalid title count");
        if (!chapters_ok)
            msg_Err(demux, "Invalid chapter count");
        if (!programs_ok)
            msg_Err(demux, "Invalid program count");
        return VLC_EGENERIC;
    }

    if (sys->program_count == 0)
        sys->program_count = 1;
    size_t track_count = (sys->video_track_count + sys->audio_track_count +
                          sys->sub_track_count) * sys->program_count;
    vlc_vector_init(&sys->tracks);
    if (track_count > 0)
    {
        bool success = vlc_vector_reserve(&sys->tracks, track_count);
        if (!success)
            return VLC_ENOMEM;
    }

    int ret;
    for (ssize_t i = 0; i < sys->program_count; ++i)
    {
        ret = InitVideoTracks(demux, i, sys->video_track_count);
        if (ret != VLC_SUCCESS)
            goto error;

        ret = InitAudioTracks(demux, i, sys->audio_track_count);
        if (ret != VLC_SUCCESS)
            goto error;

        ret = InitSubTracks(demux, i, sys->sub_track_count);
        if (ret != VLC_SUCCESS)
            goto error;
    }


    sys->pts = sys->audio_pts = sys->video_pts = VLC_TICK_0;
    sys->current_title = 0;
    sys->chapter_gap = sys->chapter_count > 0 ?
                       (sys->length / sys->chapter_count) : VLC_TICK_INVALID;
    sys->updates = 0;

    demux->pf_control = Control;
    demux->pf_demux = Demux;

    return VLC_SUCCESS;
error:
    Close(obj);
    demux->p_sys = NULL;
    return ret;
}

vlc_module_begin()
    set_description("mock access demux")
    set_capability("access", 0)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_ACCESS)
    set_callbacks(Open, Close)
    OPTIONS_GLOBAL(DECLARE_MODULE_OPTIONS)
    OPTIONS_AUDIO(DECLARE_MODULE_SUBOPTIONS)
    OPTIONS_VIDEO(DECLARE_MODULE_SUBOPTIONS)
    OPTIONS_SUB(DECLARE_MODULE_SUBOPTIONS)
    add_shortcut("mock")
vlc_module_end()

#undef X
