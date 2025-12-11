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
#include <vlc_configuration.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_picture.h>
#include <vlc_demux.h>
#include <vlc_input.h>
#include <vlc_vector.h>

enum
{
    PALETTE_RED = 0,
    PALETTE_GREEN,
    PALETTE_BLUE,
    PALETTE_BLACK,
};

// packed RGBA in memory order
const uint8_t rgbpal[4][4] = {[PALETTE_RED] =   { 0xFF, 0x00, 0x00, 0xFF },
                              [PALETTE_GREEN] = { 0x00, 0xFF, 0x00, 0xFF },
                              [PALETTE_BLUE] =  { 0x00, 0x00, 0xFF, 0xFF },
                              [PALETTE_BLACK] = { 0x00, 0x00, 0x00, 0xFF }};

// packed YUVA in memory order
const uint8_t yuvpal[4][4] = {[PALETTE_RED] =   { 0x4C, 0x54, 0xFF, 0xFF },
                              [PALETTE_GREEN] = { 0x95, 0x2B, 0x15, 0xFF },
                              [PALETTE_BLUE] =  { 0x1D, 0xFF, 0x6B, 0xFF },
                              [PALETTE_BLACK] = { 0x00, 0x80, 0x80, 0xFF }};

#define GLYPH_COLS 6
#define GLYPH_ROWS 10

static const uint8_t glyph10_bitmap[3][GLYPH_ROWS] =
    {
        [PALETTE_BLUE] = {
            /*Unicode: U+0042 (B) , Width: 6 */
        0xfc,  //%%%%%%
        0xfc,  //%%%%%%
        0xcc,  //%%..%%
        0xcc,  //%%..%%
        0xf0,  //%%%%..
        0xf0,  //%%%%..
        0xcc,  //%%..%%
        0xcc,  //%%..%%
        0xfc,  //%%%%%%
        0xfc,  //%%%%%%
        }, [PALETTE_GREEN] = {
            /*Unicode: U+0047 (G) , Width: 6 */
        0xfc,  //%%%%%%
        0xfc,  //%%%%%%
        0xc0,  //%%....
        0xc0,  //%%....
        0xcc,  //%%..%%
        0xcc,  //%%..%%
        0xcc,  //%%..%%
        0xcc,  //%%..%%
        0xfc,  //%%%%%%
        0xfc,  //%%%%%%
        }, [PALETTE_RED] = {
            /*Unicode: U+0052 (R) , Width: 6 */
        0xfc,  //%%%%%%
        0xfc,  //%%%%%%
        0xcc,  //%%..%%
        0xcc,  //%%..%%
        0xf0,  //%%%%..
        0xf0,  //%%%%..
        0xcc,  //%%..%%
        0xcc,  //%%..%%
        0xcc,  //%%..%%
        0xcc,  //%%..%%
        }
};

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

static vlc_fourcc_t
var_Read_vlc_fourcc_t(const char *psz)
{
    char fourcc[5] = { 0 };
    if (psz)
    {
        strncpy(fourcc, psz, 4);
        return VLC_FOURCC(fourcc[0], fourcc[1],fourcc[2],fourcc[3]);
    }
    return 0;
}

static bool
var_Read_bool(const char *psz)
{
    if (!psz)
        return false;
    char *endptr;
    long long int value = strtoll(psz, &endptr, 0);
    if (endptr == psz) /* Not an integer */
        return strcasecmp(psz, "true") == 0
            || strcasecmp(psz, "yes") == 0;
    return !!value;
}

static int64_t
var_Read_integer(const char *psz)
{
    return psz ? strtoll(psz, NULL, 0) : 0;
}
#define var_Read_vlc_tick_t var_Read_integer

static unsigned
var_Read_unsigned(const char *psz)
{
    int64_t value = var_Read_integer(psz);
    return value >= 0 && value < UINT_MAX ? value : UINT_MAX;
}

static float
var_Read_float(const char *psz)
{
    return atof(psz);
}

#define FREE_CB(x) free(x)
#define NO_FREE(x) (void) x

#define OPTIONS_AUDIO(Y) \
    Y(audio, packetized, bool, add_bool, Bool, true, NO_FREE) \
    Y(audio, add_track_at, vlc_tick_t, add_integer, Integer, VLC_TICK_INVALID, NO_FREE) \
    Y(audio, channels, unsigned, add_integer, Unsigned, 2, NO_FREE) \
    Y(audio, format, vlc_fourcc_t, add_string, Fourcc, "f32l", NO_FREE) \
    Y(audio, rate, unsigned, add_integer, Unsigned, 48000, NO_FREE) \
    Y(audio, sample_length, vlc_tick_t, add_integer, Integer, VLC_TICK_FROM_MS(40), NO_FREE) \
    Y(audio, sinewave, bool, add_bool, Bool, true, NO_FREE) \
    Y(audio, sinewave_frequency, unsigned, add_integer, Integer, 500, NO_FREE) \
    Y(audio, sinewave_amplitude, float, add_float, Float, 0.2, NO_FREE)

#define OPTIONS_VIDEO(Y) \
    Y(video, packetized, bool, add_bool, Bool, true, NO_FREE) \
    Y(video, add_track_at, vlc_tick_t, add_integer, Integer, VLC_TICK_INVALID, NO_FREE) \
    Y(video, chroma, vlc_fourcc_t, add_string, Fourcc, "I420", NO_FREE) \
    Y(video, width, unsigned, add_integer, Unsigned, 640, NO_FREE) \
    Y(video, height, unsigned, add_integer, Unsigned, 480, NO_FREE) \
    Y(video, frame_rate, unsigned, add_integer, Unsigned, 25, NO_FREE) \
    Y(video, frame_rate_base, unsigned, add_integer, Unsigned, 1, NO_FREE) \
    Y(video, colorbar, bool, add_bool, Bool, false, NO_FREE) \
    Y(video, orientation, unsigned, add_integer, Unsigned, ORIENT_NORMAL, NO_FREE) \
    Y(video, image_count, unsigned, add_integer, Unsigned, 0, NO_FREE)

#define OPTIONS_SUB(Y) \
    Y(sub, packetized, bool, add_bool, Bool, true, NO_FREE) \
    Y(sub, add_track_at, vlc_tick_t, add_integer, Integer, VLC_TICK_INVALID, NO_FREE) \
    Y(sub, format, vlc_fourcc_t, add_string, Fourcc, "subt", NO_FREE) \
    Y(sub, page, unsigned, add_integer, Integer, 0, NO_FREE)

/* var_name, type, module_header_type, getter, default_value */
#define OPTIONS_GLOBAL(X) \
    X(node_count, ssize_t, add_integer, Ssize, 0, NO_FREE) \
    X(length, vlc_tick_t, add_integer, Integer, VLC_TICK_FROM_MS(5000), NO_FREE) \
    X(report_length, bool, add_bool, Bool, true, NO_FREE) \
    X(audio_track_count, ssize_t, add_integer, Ssize, 0, NO_FREE) \
    X(video_track_count, ssize_t, add_integer, Ssize, 0, NO_FREE) \
    X(sub_track_count, ssize_t, add_integer, Ssize, 0, NO_FREE) \
    X(input_sample_length, vlc_tick_t, add_integer, Integer, VLC_TICK_FROM_MS(40), NO_FREE) \
    X(title_count, ssize_t, add_integer, Ssize, 0, NO_FREE) \
    X(chapter_count, ssize_t, add_integer, Ssize, 0, NO_FREE) \
    X(null_names, bool, add_bool, Bool, false, NO_FREE) \
    X(program_count, ssize_t, add_integer, Ssize, 0, NO_FREE) \
    X(attachment_count, ssize_t, add_integer, Ssize, 0, NO_FREE) \
    X(can_seek, bool, add_bool, Bool, true, NO_FREE) \
    X(can_pause, bool, add_bool, Bool, true, NO_FREE) \
    X(can_control_pace, bool, add_bool, Bool, true, NO_FREE) \
    X(can_control_rate, bool, add_bool, Bool, true, NO_FREE) \
    X(can_record, bool, add_bool, Bool, true, NO_FREE) \
    X(error, bool, add_bool, Bool, false, NO_FREE) \
    X(pts_delay, vlc_tick_t, add_integer, Unsigned, DEFAULT_PTS_DELAY, NO_FREE) \
    X(pts_offset, vlc_tick_t, add_integer, Unsigned, 0, NO_FREE) \
    X(time_offset, vlc_tick_t, add_integer, Ssize, 0, NO_FREE) \
    X(discontinuities, char *, add_string, String, NULL, FREE_CB) \
    X(config, char *, add_string, String, NULL, FREE_CB)

#define DECLARE_OPTION(var_name, type, module_header_type, getter, default_value, free_cb) \
    type var_name;
#define DECLARE_SUBOPTION(a,b,c,d,e,f,g) DECLARE_OPTION(b,c,d,e,f,g)

#define READ(var_name, member_name, getter) \
    sys->member_name = var_Inherit##getter(obj, "mock-"#var_name);
#define READ_OPTION(var_name, type, module_header_type, getter, default_value, free_cb) \
    READ(var_name, var_name, getter)
#define READ_SUBOPTION(group_name, var_name, type, module_header_type, getter, default_value, free_cb) \
    READ(group_name##_##var_name, group_name.var_name, getter)

#define DECLARE_MODULE_OPTIONS(var_name, type, module_header_type, getter, default_value, free_cb) \
    module_header_type("mock-"#var_name, default_value, #var_name, NULL) \
    change_volatile() \
    change_safe()
#define DECLARE_MODULE_SUBOPTIONS(a,b,c,d,e,f,g) \
    DECLARE_MODULE_OPTIONS(a##_##b,c,d,e,f,g)

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

struct mock_track
{
    es_format_t fmt;
    es_out_id_t *id;
    union
    {
        struct mock_video_options video;
        struct mock_audio_options audio;
        struct mock_sub_options sub;
    };
};
typedef struct VLC_VECTOR(struct mock_track *) mock_track_vector;
/* Ensure common members have same location */
static_assert(offsetof(struct mock_video_options, add_track_at) ==
              offsetof(struct mock_audio_options, add_track_at), "inconsistent offset");
static_assert(offsetof(struct mock_video_options, add_track_at) ==
              offsetof(struct mock_sub_options, add_track_at), "inconsistent offset");

struct pcr_point
{
    vlc_tick_t oldpcr;
    vlc_tick_t newpcr;
};
typedef struct VLC_VECTOR(struct pcr_point) pcr_point_vector;

struct demux_sys
{
    mock_track_vector tracks;

    vlc_tick_t clock;
    vlc_tick_t audio_pts;
    date_t video_date;

    int current_title;
    vlc_tick_t chapter_gap;
    int current_chapter;

    uint8_t bar_colors[PICTURE_PLANE_MAX][PICTURE_PLANE_MAX];
    bool b_colors;

    unsigned int updates;
    OPTIONS_GLOBAL(DECLARE_OPTION)
    struct mock_video_options video;
    struct mock_audio_options audio;
    struct mock_sub_options sub;

    char *art_url;

    pcr_point_vector pcr_points;
    size_t next_pcr_index;

    bool eof_requested;
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
            t->seekpoint[i]->psz_name = NULL;
            vlc_input_title_Delete(t);
            return NULL;
        }
        t->seekpoint[i]->i_time_offset = i * sys->chapter_gap;
    }
    return t;
}

static input_attachment_t *
CreateAttachment(demux_t *demux, const char *prefix_name, size_t index)
{
    input_attachment_t *attach = NULL;
    picture_t *pic = NULL;
    block_t *block = NULL;

    char *name;
    int ret = asprintf(&name, "%s %zu", prefix_name, index);
    if (ret < 0)
        return NULL;

    pic = picture_New(VLC_CODEC_RGB24, 100, 100, 1, 1);
    if (pic == NULL)
        goto end;

    memset(pic->p[0].p_pixels, 0x80, pic->p[0].i_lines * pic->p[0].i_pitch);

    ret = picture_Export(VLC_OBJECT(demux), &block, NULL, pic, VLC_CODEC_BMP,
                         0, 0, false);
    if (ret != VLC_SUCCESS)
        goto end;

    attach = vlc_input_attachment_New(name, "image/bmp", "Mock Attach Desc",
                                      block->p_buffer, block->i_buffer);

end:
    if (block != NULL)
        block_Release(block);
    if (pic != NULL)
        picture_Release(pic);
    free(name);
    return attach;
}

static int
GetAttachments(demux_t *demux, input_attachment_t ***attach_array_p,
               int *attach_count_p)
{
    struct demux_sys *sys = demux->p_sys;
    assert(sys->attachment_count > 0);
    size_t attachment_count = sys->attachment_count;

    input_attachment_t **attach_array =
        vlc_alloc(sys->attachment_count, sizeof(*attach_array));
    if (attach_array == NULL)
        return VLC_ENOMEM;

    for (size_t i = 0; i < attachment_count; i++)
    {
        attach_array[i] = CreateAttachment(demux, "Mock Attach", i);

        if (attach_array[i] == NULL)
        {
            if (i == 0)
            {
                free(attach_array);
                return VLC_ENOMEM;
            }
            *attach_array_p = attach_array;
            *attach_count_p = i;
            return VLC_SUCCESS;
        }

        if (sys->art_url == NULL
         && asprintf(&sys->art_url, "attachment://%s",
                     attach_array[i]->psz_name) == -1)
            sys->art_url = NULL;
    }

    *attach_array_p = attach_array;
    *attach_count_p = sys->attachment_count;

    return VLC_SUCCESS;
}

static vlc_meta_t *
CreateMeta(demux_t *demux)
{
    struct demux_sys *sys = demux->p_sys;

    vlc_meta_t *meta = vlc_meta_New();
    if (meta == NULL)
        return NULL;

    vlc_meta_SetArtist(meta, "VideoLAN");
    vlc_meta_SetGenre(meta, "Best Media Player");

    if (sys->art_url != NULL)
        vlc_meta_SetArtURL(meta, sys->art_url);

    return meta;
}

static void
SetAudioPts(demux_t *demux, vlc_tick_t pts)
{
    struct demux_sys *sys = demux->p_sys;

    if (sys->audio_track_count == 0)
        return;
    assert(pts >= VLC_TICK_0);

    /* Round up (ceiling) on duration (hence the VLC_TICK_0 removal) */
    pts -= VLC_TICK_0;
    pts = ((pts + sys->audio.sample_length - 1) / sys->audio.sample_length)
          * sys->audio.sample_length;
    pts += VLC_TICK_0;

    sys->audio_pts = pts;
}

static void
SetVideoPts(demux_t *demux, vlc_tick_t pts)
{
    struct demux_sys *sys = demux->p_sys;

    if (sys->video_track_count == 0 && sys->sub_track_count == 0)
        return;
    assert(pts >= VLC_TICK_0);

    vlc_tick_t frame_duration =
        vlc_tick_rate_duration(sys->video.frame_rate /
                               (float)sys->video.frame_rate_base);
    /* Round up (ceiling) via date_Increment() */
    pts -= VLC_TICK_0;
    unsigned nb_frames = (pts + frame_duration - 1) / frame_duration;

    date_Set(&sys->video_date, VLC_TICK_0);
    date_Increment(&sys->video_date, nb_frames);
}

static void
UpdateClock(demux_t *demux)
{
    struct demux_sys *sys = demux->p_sys;
    if (sys->audio_track_count > 0
     && (sys->video_track_count > 0 || sys->sub_track_count > 0))
        sys->clock = __MIN(sys->audio_pts, date_Get(&sys->video_date));
    else if (sys->audio_track_count > 0)
        sys->clock = sys->audio_pts;
    else if (sys->video_track_count > 0 || sys->sub_track_count > 0)
        sys->clock = date_Get(&sys->video_date);

    if (sys->clock > VLC_TICK_0 + sys->pts_offset + sys->length)
        sys->clock = VLC_TICK_0 + sys->pts_offset + sys->length;
}

static void
SetTime(demux_t *demux, vlc_tick_t time)
{
    assert(time >= VLC_TICK_0);
    SetVideoPts(demux, time);
    SetAudioPts(demux, time);
    UpdateClock(demux);
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
            *va_arg(args, vlc_tick_t *) = sys->pts_delay;
            return VLC_SUCCESS;
        case DEMUX_GET_META:
        {
            vlc_meta_t *meta_out = va_arg(args, vlc_meta_t *);
            vlc_meta_t *meta = CreateMeta(demux);
            if (meta == NULL)
                return VLC_ENOMEM;
            vlc_meta_Merge(meta_out, meta);
            vlc_meta_Delete(meta);
            return VLC_SUCCESS;
        }
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
                SetTime(demux, VLC_TICK_0);
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
                    SetTime(demux, seekpoint_idx * sys->chapter_gap + VLC_TICK_0);
                    sys->current_chapter = seekpoint_idx;
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
                *va_arg(args, int *) = sys->current_chapter;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        case DEMUX_GET_POSITION:
            *va_arg(args, double *) = (sys->clock - VLC_TICK_0 - sys->pts_offset) / (double) sys->length;
            return VLC_SUCCESS;
        case DEMUX_SET_POSITION:
        {
            if (!sys->can_seek)
                return VLC_EGENERIC;
            double pos = va_arg(args, double);
            SetTime(demux, pos * sys->length + VLC_TICK_0);
            return VLC_SUCCESS;
        }
        case DEMUX_GET_LENGTH:
            if (!sys->report_length)
                return VLC_EGENERIC;
            *va_arg(args, vlc_tick_t *) = sys->length;
            return VLC_SUCCESS;
        case DEMUX_GET_NORMAL_TIME:
            *va_arg(args, vlc_tick_t *) = VLC_TICK_0 + sys->pts_offset;
            return VLC_SUCCESS;
        case DEMUX_GET_TIME:
            *va_arg(args, vlc_tick_t *) = sys->time_offset + sys->clock - VLC_TICK_0 - sys->pts_offset;
            return VLC_SUCCESS;
        case DEMUX_SET_TIME:
        {
            if (!sys->can_seek)
                return VLC_EGENERIC;
            vlc_tick_t time = va_arg(args, vlc_tick_t);
            SetTime(demux, time);
            return VLC_SUCCESS;
        }
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
        {
            input_attachment_t ***attach_array_p = va_arg(args, input_attachment_t***);
            int *attach_count_p = va_arg(args, int *);
            if (sys->attachment_count <= 0)
                return VLC_EGENERIC;

            return GetAttachments(demux, attach_array_p, attach_count_p);
        }
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

static void
GenerateAudioSineWave(demux_t *demux, struct mock_track *track, block_t *block)
{
    struct demux_sys *sys = demux->p_sys;
    float *out = (float *) block->p_buffer;
    double delta = 1 / (double) track->fmt.audio.i_rate;
    double audio_pts_sec = sys->audio_pts / (double) CLOCK_FREQ;

    assert(track->fmt.audio.i_format == VLC_CODEC_FL32);

    for (unsigned si = 0; si < block->i_nb_samples; ++si)
    {
        double value = track->audio.sinewave_amplitude
                     * sin(2 * M_PI * track->audio.sinewave_frequency * audio_pts_sec);
        audio_pts_sec += delta;

        for (unsigned ci = 0; ci < track->fmt.audio.i_channels; ++ci)
            *out++ = value;
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
    b->i_nb_samples = samples;

    if (track->audio.sinewave)
        GenerateAudioSineWave(demux, track, b);
    else
        memset(b->p_buffer, 0, b->i_buffer);

    return b;
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
CreateVideoBlock(demux_t *demux, struct mock_track *track, vlc_tick_t pts)
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

    unsigned range = pic->format.p_palette ? 3 : 255;
    unsigned delay = 2550 / range;

    size_t block_len = 0;
    for (int i = 0; i < pic->i_planes; ++i)
        block_len += pic->p[i].i_lines * pic->p[i].i_pitch;

    uint8_t pixel = (pts / VLC_TICK_FROM_MS(delay)) % range;
    if (sys->b_colors)
    {
        int bars = __MAX(3, pic->p[0].i_pixel_pitch);
        unsigned lines_per_color = pic->p[0].i_visible_lines / bars;
        for (int bar = 0; bar < bars; bar++)
        {
            for (unsigned y=bar*lines_per_color; y < (bar+1)*lines_per_color; y++)
            {
                for (int x=0; x < pic->p[0].i_visible_pitch; x += pic->p[0].i_pixel_pitch)
                {
                    memcpy(&pic->p[0].p_pixels[x + y*pic->p[0].i_pitch], &sys->bar_colors[bar], pic->p[0].i_pixel_pitch);
                }
            }
        }
    }
    else
        memset(pic->p[0].p_pixels, pixel, block_len);

    if(pic->format.p_palette && pixel < PALETTE_BLACK)
    {
        unsigned incx = pic->p[0].i_pitch / GLYPH_COLS / 2;
        unsigned incy = pic->p[0].i_lines / GLYPH_ROWS / 2;
        uint8_t *p = pic->p[0].p_pixels;
        for(unsigned y=0; y<GLYPH_ROWS; y++)
        {
            for(unsigned yrepeat=0; yrepeat<incy; yrepeat++)
            {
                uint8_t *q = p;
                for(unsigned x=0; x<GLYPH_COLS; x++)
                {
                    uint8_t mask = 0x80 >> x;
                    if( glyph10_bitmap[pixel][y] & mask )
                        memset(q, PALETTE_BLACK, incx);
                    else
                        memset(q, pixel, incx);
                    q += incx;
                }
                p += pic->p[0].i_visible_pitch;
            }
        }
    }

    return block_Init(&video->b, &cbs, pic->p[0].p_pixels, block_len);
    (void) demux;
}

static block_t *
CreateSubBlock(demux_t *demux, struct mock_track *track, vlc_tick_t pts)
{
    (void) demux;
    char *text;
    if (asprintf(&text, "subtitle @ %"PRId64, pts) == -1)
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
    (void) track;
    return b;
}

static int
CheckAndCreateTracksEs(demux_t *demux, vlc_tick_t pts, bool *created)
{
    struct demux_sys *sys = demux->p_sys;
    *created = false;

    struct mock_track *track;
    vlc_vector_foreach(track, &sys->tracks)
    {
        if (track->id ||
           (track->video.add_track_at != VLC_TICK_INVALID &&
            (pts - VLC_TICK_0 - sys->pts_offset) < track->video.add_track_at))
            continue;
        track->id = es_out_Add(demux->out, & track->fmt);
        if (!track->id)
            return VLC_EGENERIC;
        *created = true;
    }

    return VLC_SUCCESS;
}

static void
DeleteTrack(demux_t *demux, struct mock_track *track)
{
    if (track->id)
        es_out_Del(demux->out, track->id);
    es_format_Clean(&track->fmt);
    free(track);
}

static struct mock_track *
CreateTrack(demux_t *demux, int i_cat, int id, int group)
{
    struct demux_sys *sys = demux->p_sys;
    struct mock_track *track =  calloc(1, sizeof(*track));
    if (!track)
        return NULL;

    es_format_Init(&track->fmt, i_cat, 0);
    switch (i_cat)
    {
        case AUDIO_ES:
            track->audio = sys->audio;
            break;
        case VIDEO_ES:
            track->video = sys->video;
            break;
        case SPU_ES:
            track->sub = sys->sub;
            break;
        default:
            vlc_assert_unreachable();
            return NULL;
    }

    track->fmt.i_group = group;
    track->fmt.i_id = id;

    return track;
}

static int
ConfigureVideoTrack(demux_t *demux,
                    const struct mock_video_options *options,
                    es_format_t *fmt)
{
    vlc_fourcc_t chroma = options->chroma;
    const vlc_chroma_description_t *desc =
        vlc_fourcc_GetChromaDescription(chroma);
    if (!desc || desc->plane_count == 0)
        chroma = 0;

    const bool frame_rate_ok =
        options->frame_rate != 0 && options->frame_rate != UINT_MAX &&
        options->frame_rate_base != 0 && options->frame_rate_base != UINT_MAX;
    const bool chroma_ok = chroma != 0;
    const bool size_ok = options->width != UINT_MAX &&
                         options->height != UINT_MAX;

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

    if (options->orientation > ORIENT_RIGHT_BOTTOM)
    {
        msg_Err(demux, "Invalid orientation value %u", options->orientation);
        return VLC_EGENERIC;
    }

    if(chroma == VLC_CODEC_RGBP || chroma == VLC_CODEC_YUVP)
    {
        fmt->video.p_palette = malloc(sizeof(video_palette_t));
        if(!fmt->video.p_palette)
            return VLC_EGENERIC;
        fmt->video.p_palette->i_entries = 4;
        memcpy(fmt->video.p_palette->palette,
               chroma == VLC_CODEC_RGBP ? rgbpal : yuvpal,
               sizeof(rgbpal));
    }

    fmt->i_codec = chroma;
    fmt->video.i_chroma = chroma;
    fmt->video.i_width = fmt->video.i_visible_width = options->width;
    fmt->video.i_height = fmt->video.i_visible_height = options->height;
    fmt->video.i_frame_rate = options->frame_rate;
    fmt->video.i_frame_rate_base = options->frame_rate_base;
    fmt->video.orientation = options->orientation;

    fmt->b_packetized = options->packetized;

    if (options->colorbar && !vlc_fourcc_IsYUV(chroma) && desc->plane_count == 1)
    {
        struct demux_sys *sys = demux->p_sys;
        sys->b_colors = true;

        unsigned bars = __MAX(3, desc->pixel_size);
        for (unsigned bar = 0; bar < bars; bar++)
        {
            memset(&sys->bar_colors[bar], 0, sizeof(*sys->bar_colors));
            if (desc->pixel_bits == 15)
            {
                // ONLY Little-Endian FOR NOW to match AVI

                // only first 2 bytes of bar_colors are used
                if (bar == 0)
                    SetWLE(&sys->bar_colors[bar], 0x1F << 10);
                else if (bar == 1)
                    SetWLE(&sys->bar_colors[bar], 0x1F << 5);
                else if (bar == 2)
                    SetWLE(&sys->bar_colors[bar], 0x1F << 0);
            }
            else if (desc->pixel_bits == 16)
            {
                // ONLY Little-Endian FOR NOW to match AVI

                // only first 2 bytes of bar_colors are used
                if (bar == 0)
                    SetWLE(&sys->bar_colors[bar], 0x1F << 11);
                else if (bar == 1)
                    SetWLE(&sys->bar_colors[bar], 0x3F << 5);
                else if (bar == 2)
                    SetWLE(&sys->bar_colors[bar], 0x1F << 0);
            }
            else if (desc->pixel_bits == 32 || desc->pixel_bits == 24)
                // write 0xFF on the offset of the bar
                sys->bar_colors[bar][bar] = 0xFF;
            else
                sys->b_colors = false; // unsupported RGB type
        }
    }

    return VLC_SUCCESS;
}

static int
ConfigureAudioTrack(demux_t *demux,
                    const struct mock_audio_options *options,
                    es_format_t *fmt)
{
    const bool rate_ok = options->rate > 0 && options->rate != UINT_MAX;
    const bool format_ok = aout_BitsPerSample(options->format) != 0;
    const bool channels_ok = options->channels > 0 &&
                             options->channels <= AOUT_CHAN_MAX;

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

    if (options->sinewave && options->format != VLC_CODEC_FL32)
    {
        /* We could support every formats if we plug an audio converter, but
         * this may be overkill for a mock module */
        msg_Err(demux, "audio sinewave works only with fl32 format");
        return VLC_EGENERIC;
    }


    uint16_t physical_channels = 0;
    switch (options->channels)
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

    fmt->i_codec = options->format;
    fmt->audio.i_format = options->format;
    fmt->audio.i_rate = options->rate;
    fmt->audio.i_physical_channels = physical_channels;
    aout_FormatPrepare(&fmt->audio);

    fmt->b_packetized = options->packetized;

    return VLC_SUCCESS;
}

static int
ConfigureSubTrack(demux_t *demux,
                  const struct mock_sub_options *options,
                  es_format_t *fmt)
{
    VLC_UNUSED(demux);

    fmt->i_codec = options->format;
    fmt->subs.teletext.i_magazine = options->page / 100 % 10;
    fmt->subs.teletext.i_page = (((options->page / 10) % 10) << 4) +
                                (options->page % 10);

    fmt->b_packetized = options->packetized;

    return VLC_SUCCESS;
}

static struct mock_track *
GetMockTrackByID(struct demux_sys *sys,
                 enum es_format_category_e cat, unsigned id)
{
    unsigned current=0;
    struct mock_track *track;
    vlc_vector_foreach(track, &sys->tracks)
    {
        if (track->fmt.i_cat != cat)
            continue;
        if (id == current++)
            return track;
    }
    return NULL;
}

static int
OverrideTrackOptions(const config_chain_t *config_chain,
                     struct mock_track *track)
{
#define OVERRIDE_SUBOPTION(group_name, var_name, type, module_header_type, getter, default_value, free_cb) \
    if (!strcmp(""#var_name, config_chain->psz_name)) \
    { \
        free_cb(track->group_name.var_name); \
        track->group_name.var_name = var_Read_ ## type(config_chain->psz_value); \
        break; \
    }

    for (; config_chain ; config_chain = config_chain->p_next)
    {
        switch (track->fmt.i_cat)
        {
            case VIDEO_ES:
                OPTIONS_VIDEO(OVERRIDE_SUBOPTION)
                        break;
            case AUDIO_ES:
                OPTIONS_AUDIO(OVERRIDE_SUBOPTION)
                        break;
            case SPU_ES:
                OPTIONS_SUB(OVERRIDE_SUBOPTION)
                        break;
            default:
                vlc_assert_unreachable();
                break;
        }
    }
    return VLC_SUCCESS;
}

static int
UpdateTrackConfiguration(demux_t *demux,
                         const char *config_name,
                         const config_chain_t *config_chain)
{
    struct demux_sys *sys = demux->p_sys;

    const struct
    {
        const char *name;
        const int cat;
    } chain_names[3] = {
        { "audio[%u]", AUDIO_ES },
        { "video[%u]", VIDEO_ES },
        { "sub[%u]",   SPU_ES   },
    };

    for (int i=0; i<3; i++)
    {
        unsigned trackid;
        struct mock_track *track;
        if (sscanf(config_name, chain_names[i].name, &trackid) == 1)
        {
            if (!(track = GetMockTrackByID(sys, chain_names[i].cat, trackid)))
                return VLC_EGENERIC;
            OverrideTrackOptions(config_chain, track);
            return VLC_SUCCESS;
        }
    }
    msg_Warn(demux, "ignoring %s", config_name);
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
            if (!track->id)
                continue;
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
DemuxVideo(demux_t *demux, vlc_tick_t end_pts)
{
    struct demux_sys *sys = demux->p_sys;

    vlc_tick_t current_pts;
    while ((current_pts = date_Get(&sys->video_date)) < end_pts)
    {
        vlc_tick_t next_pts = date_Increment(&sys->video_date, 1);
        vlc_tick_t frame_duration = next_pts - current_pts;

        struct mock_track *track;
        vlc_vector_foreach(track, &sys->tracks)
        {
            if (!track->id)
                continue;
            block_t *block;
            switch (track->fmt.i_cat)
            {
                case VIDEO_ES:

                    if (track->video.image_count >= 1)
                    {
                        track->video.image_count--;
                        if (track->video.image_count == 0)
                            sys->eof_requested = true;
                    }
                    block = CreateVideoBlock(demux, track, current_pts);
                    break;
                case SPU_ES:
                    block = CreateSubBlock(demux, track, current_pts);
                    break;
                default:
                    continue;
            }
            if (!block)
                return VLC_EGENERIC;

            block->i_length = frame_duration;
            block->i_pts = block->i_dts = current_pts;

            int ret = es_out_Send(demux->out, track->id, block);
            if (ret != VLC_SUCCESS)
                return ret;
        }
    }
    return VLC_SUCCESS;
}

static int
Demux(demux_t *demux)
{
    struct demux_sys *sys = demux->p_sys;
    int ret = VLC_SUCCESS;

    if (sys->error)
        return VLC_DEMUXER_EGENERIC;

    /* Add late tracks if any */
    bool created;
    ret = CheckAndCreateTracksEs(demux, sys->clock, &created);
    if (ret != VLC_SUCCESS)
        return VLC_DEMUXER_EGENERIC;

    if (sys->pcr_points.size > sys->next_pcr_index)
    {
        const struct pcr_point *pt = &sys->pcr_points.data[sys->next_pcr_index];
        if (sys->clock >= pt->oldpcr)
        {
            sys->audio_pts = sys->clock = pt->newpcr;
            if (sys->video_track_count > 0 || sys->sub_track_count > 0)
                date_Set(&sys->video_date, pt->newpcr);

            sys->next_pcr_index++;
        }
    }

    vlc_tick_t prev_pts = sys->clock;
    UpdateClock(demux);

    if (sys->chapter_gap > 0)
    {
        int chapter_index = (sys->clock - VLC_TICK_0 - sys->pts_offset) / sys->chapter_gap;
        if (chapter_index != sys->current_chapter)
        {
            sys->updates |= INPUT_UPDATE_SEEKPOINT;
            sys->current_chapter = chapter_index;
        }
    }

    if (!sys->can_control_pace)
    {
        /* Simulate a live input */
        vlc_tick_t delay = sys->clock - prev_pts;
        delay = delay - delay / 1000 /* Sleep a little less */;
        vlc_tick_sleep(delay);
    }

    es_out_SetPCR(demux->out, sys->clock);

    const vlc_tick_t video_step_length =
        (sys->video_track_count == 0 && sys->sub_track_count == 0) ? 0 :
         vlc_tick_rate_duration(sys->video.frame_rate /
                                (float) sys->video.frame_rate_base);

    const vlc_tick_t audio_step_length =
        sys->audio_track_count > 0 ? sys->audio.sample_length : 0;

    const vlc_tick_t step_length = __MAX(audio_step_length, video_step_length);

    bool eof = !created;
    if (sys->audio_track_count > 0)
    {
        ret = DemuxAudio(demux, audio_step_length,
                         __MIN(step_length + sys->clock - VLC_TICK_0, sys->length));
        if (sys->audio_pts - VLC_TICK_0 + audio_step_length <= sys->length)
            eof = false;
    }

    if (ret == VLC_SUCCESS
     && (sys->video_track_count > 0 || sys->sub_track_count > 0))
    {
        ret = DemuxVideo(demux,
                         __MIN(step_length + sys->clock - VLC_TICK_0 - sys->pts_offset, sys->length));
        if (date_Get(&sys->video_date) - VLC_TICK_0 - sys->pts_offset + video_step_length <= sys->length)
            eof = false;
    }

    /* No audio/video/sub: simulate that we read some inputs */
    if (step_length == 0)
    {
        sys->clock += sys->input_sample_length;
        if (sys->clock - VLC_TICK_0 - sys->pts_offset + sys->input_sample_length <= sys->length)
            eof = false;
    }

    if (ret != VLC_SUCCESS)
        return VLC_DEMUXER_EGENERIC;

    if (sys->eof_requested)
        return VLC_DEMUXER_EOF;

    return eof ? VLC_DEMUXER_EOF : VLC_DEMUXER_SUCCESS;
}

static char *
StripNodeParams(const char *orignal_url)
{
    char *url = strdup(orignal_url);
    if (url == NULL)
        return NULL;

    /* Strip "node_count=xxx" */
    char *substr_start = strcasestr(url, "node_count=");
    assert(substr_start != NULL);

    char *substr_end = strchr(substr_start, ';');
    if (substr_end != NULL)
        substr_end++;
    else
        substr_end = strchr(substr_start, '\0');

    assert(substr_end != NULL);
    memmove(substr_start, substr_end, strlen(substr_end) + 1);

    /* Strip "node[xxx]={xxx}" */
    while ((substr_start = strcasestr(url, "node[")) != NULL)
    {
        substr_end = strchr(substr_start, '{');
        if (substr_end != NULL)
        {
            substr_end = strchr(substr_start, '}');
            substr_end++;
            if (*substr_end == ';')
                substr_end++;
        }

        if (substr_end == NULL)
        {
            free(url);
            return NULL;
        }

        memmove(substr_start, substr_end, strlen(substr_end) + 1);
    }

    return url;
}

/*
 * Create sub items from the demux URL
 *
 * Example:
 * "mock://node[2]{video_track_count=1};node_count=4;length=1000000;node[1]{length=2000000}"
 * Will create the following sub items:
 * - input_item_New(mock://length=1000000, submock[0]);
 * - input_item_New(mock://length=2000000, submock[1]);
 * - input_item_New(mock://video_track_count=1, submock[2]);
 * - input_item_New(mock://length=1000000, submock[3]);
 *
 * If specified with node[]{}, a sub item will use the params between "{}".
 * If not specified, all subitems will use the same params from the url (here:
 * "length=1000000").
 */
static int
Readdir(stream_t *demux, input_item_node_t *node)
{
    struct demux_sys *sys = demux->p_sys;

    char *default_url = StripNodeParams(demux->psz_url);
    if (default_url == NULL)
        return VLC_EGENERIC;

    int ret = VLC_ENOMEM;
    for (ssize_t i = 0; i < sys->node_count; ++i)
    {
        const char *url;
        char *name, *option_name, *url_buf = NULL;
        int len;

        len = asprintf(&option_name, "node[%zd]{", i);
        if (len < 0)
            goto error;
        char *option = strstr(demux->psz_url, option_name);
        free(option_name);

        if (option != NULL)
        {
            option = strchr(option, '{');
            assert(option != NULL);
            option++;
            char *option_end = strchr(option, '}');
            if (option_end == NULL)
            {
                ret = VLC_EINVAL;
                goto error;
            }

            ptrdiff_t option_size = option_end - option;
            if (option_size > INT_MAX)
            {
                ret = VLC_EINVAL;
                goto error;
            }

            len = asprintf(&url_buf, "mock://%.*s", (int) option_size, option);
            if (len < 0)
                goto error;
            url = url_buf;
        }
        else
            url = default_url;

        len = asprintf(&name, "submock[%zd]", i);
        if (len < 0)
        {
            free(url_buf);
            goto error;
        }

        input_item_t *item = input_item_New(url, name);
        free(name);
        free(url_buf);

        if (item == NULL)
            goto error;

        input_item_node_AppendItem(node, item);
        input_item_Release(item);
    }

    ret = VLC_SUCCESS;
error:
    free(default_url);
    return ret;
}

static int
ReaddirControl(demux_t *demux, int query, va_list args)
{
    (void) demux;
    switch (query)
    {
        case DEMUX_GET_META:
        case DEMUX_GET_TYPE:
            return VLC_EGENERIC;
        case DEMUX_HAS_UNSUPPORTED_META:
        {
            *(va_arg(args, bool *)) = false;
            return VLC_SUCCESS;
        }
    }
    return VLC_EGENERIC;
}

static void
Close(vlc_object_t *obj)
{
    demux_t *demux = (demux_t*)obj;
    struct demux_sys *sys = demux->p_sys;

#define FREE_OPTIONS(var_name, type, module_header_type, getter, default_value, free_cb) \
    free_cb(sys->var_name);
#define FREE_SUBOPTIONS(group_name, var_name, type, module_header_type, getter, default_value, free_cb) \
    free_cb(track->group_name.var_name);

    OPTIONS_GLOBAL(FREE_OPTIONS);

    struct mock_track *track;
    vlc_vector_foreach(track, &sys->tracks)
    {
        OPTIONS_AUDIO(FREE_SUBOPTIONS);
        OPTIONS_VIDEO(FREE_SUBOPTIONS);
        OPTIONS_SUB(FREE_SUBOPTIONS);

        DeleteTrack(demux, track);
    }
    vlc_vector_clear(&sys->tracks);
    vlc_vector_clear(&sys->pcr_points);

    free(sys->art_url);
}

static int
ParseDiscontinuities(demux_t *demux)
{
    /* the 'discontinuities' option is in the following format:
     * "(oldpcr_1,newpcr_1)(oldpcr_2,newpcr_2)...(oldpcr_n,newpcr_n)"
     *
     * Example: "(1000000,5000000)(7000000,1000000)"
     * After 1s, there will be a discontinuity to 5s
     * 2s after the previous discontinuity, there will be an other one to 1s
     * */
    struct demux_sys *sys = demux->p_sys;
    assert(sys->discontinuities != NULL);

    size_t discontinuities_len = strlen(sys->discontinuities);
    size_t pcr_count = 0;

    for (size_t i = 0; i < discontinuities_len; ++i)
        if (sys->discontinuities[i] == ',')
            pcr_count++;

    if (pcr_count == 0)
    {
        msg_Err(demux, "ParseDiscontinuities: 0 points parsed");
        return VLC_EINVAL;
    }

    if (!vlc_vector_push_hole(&sys->pcr_points, pcr_count))
        return VLC_ENOMEM;

    char *savetpr;
    size_t index = 0;
    for (const char *str = strtok_r(sys->discontinuities, "(", &savetpr);
         str != NULL; str = strtok_r(NULL, "(", &savetpr))
    {
        char *endptr;
        long long oldpcrval = strtoll(str, &endptr, 10);
        if (oldpcrval == LLONG_MIN || oldpcrval == LLONG_MAX || endptr == str
         || *endptr != ',')
        {
            vlc_vector_clear(&sys->pcr_points);
            msg_Err(demux, "ParseDiscontinuities: invalid first value: '%s' "
                    "at index %zu", str, index);
            return VLC_EINVAL;
        }

        str = endptr + 1;
        long long newpcrval = strtoll(str, &endptr, 10);
        if (newpcrval == LLONG_MIN || newpcrval == LLONG_MAX || endptr == str
         || *endptr != ')')
        {
            vlc_vector_clear(&sys->pcr_points);
            msg_Err(demux, "ParseDiscontinuities: invalid second value: '%s' "
                    "at index %zu", str, index);
            return VLC_EINVAL;
        }

        assert(index < pcr_count);
        struct pcr_point *point = &sys->pcr_points.data[index++];
        point->oldpcr = VLC_TICK_0 + oldpcrval;
        point->newpcr = VLC_TICK_0 + newpcrval;
    }

    return VLC_SUCCESS;
}

static int
Open(vlc_object_t *obj)
{
    demux_t *demux = (demux_t*)obj;
    int ret = VLC_EGENERIC;

    if (demux->out == NULL)
        return VLC_EGENERIC;
    struct demux_sys *sys = vlc_obj_malloc(obj, sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    demux->p_sys = sys;
    sys->eof_requested = false;
    vlc_vector_init(&sys->tracks);
    vlc_vector_init(&sys->pcr_points);
    sys->next_pcr_index = 0;

    if (var_LocationParse(obj, demux->psz_location, "mock-") != VLC_SUCCESS)
        return VLC_ENOMEM;

    OPTIONS_GLOBAL(READ_OPTION)
    OPTIONS_AUDIO(READ_SUBOPTION)
    OPTIONS_VIDEO(READ_SUBOPTION)
    OPTIONS_SUB(READ_SUBOPTION)
    sys->art_url = NULL;

    if (sys->node_count > 0)
    {
        demux->pf_control = ReaddirControl;
        demux->pf_readdir = Readdir;
        return VLC_SUCCESS;
    }

    sys->b_colors = false;

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

    if (track_count > 0)
    {
        bool success = vlc_vector_reserve(&sys->tracks, track_count);
        if (!success)
            return VLC_ENOMEM;
    }

    struct
    {
        int cat;
        ssize_t count;
    } trackscountmap[3] = {
        { AUDIO_ES, sys->audio_track_count },
        { VIDEO_ES, sys->video_track_count },
        { SPU_ES,   sys->sub_track_count   },
    };

    for (ssize_t i = 0; i < sys->program_count; ++i)
    {
        for (size_t j=0; j<3; ++j)
        {
            for (ssize_t k=0; k<trackscountmap[j].count; ++k)
            {
                struct mock_track *track = CreateTrack(demux,
                                                       trackscountmap[j].cat,
                                                       (i << 10) + k, i);
                if (!track || !vlc_vector_push(&sys->tracks, track))
                {
                    if (track)
                        DeleteTrack(demux, track);
                    goto error;
                }
            }
        }
    }
    assert(track_count == sys->tracks.size);

    /* Convert config to config chain separators (collides with parselocation) */
    if (sys->config)
    {
        for (int i=0; sys->config[i]; i++)
            if (sys->config[i] == '+')
                sys->config[i] = ':';
    }

    if (sys->discontinuities != NULL)
    {
        ret = ParseDiscontinuities(demux);
        if (ret != VLC_SUCCESS)
            goto error;
    }

    /* Read per track config chain */
    for (char *psz_in = sys->config; psz_in;)
    {
        config_chain_t *chain = NULL;
        char *name;
        char *psz_next = config_ChainCreate(&name, &chain, psz_in);
        if (name)
        {
            UpdateTrackConfiguration(demux, name, chain);
            free(name);
        }
        config_ChainDestroy(chain);
        if (sys->config != psz_in)
            free(psz_in);
        psz_in = psz_next;
    };

    struct mock_track *track;
    vlc_vector_foreach(track, &sys->tracks)
    {
        switch(track->fmt.i_cat)
        {
            case AUDIO_ES:
                ret = ConfigureAudioTrack(demux, &track->audio, &track->fmt);
                break;
            case VIDEO_ES:
                ret = ConfigureVideoTrack(demux, &track->video, &track->fmt);
                break;
            case SPU_ES:
                ret = ConfigureSubTrack(demux, &track->sub, &track->fmt);
                break;
            default:
                vlc_assert_unreachable();
                ret = VLC_EGENERIC;
                break;
        }
        if (ret != VLC_SUCCESS)
            goto error;
    }

    bool created;
    if (CheckAndCreateTracksEs(demux, VLC_TICK_0, &created) != VLC_SUCCESS)
        goto error;

    sys->clock = VLC_TICK_0 + sys->pts_offset;
    SetAudioPts(demux, sys->clock);

    /* Initialize video_date for proper frame duration calculation */
    if (sys->video_track_count > 0 || sys->sub_track_count > 0)
    {
        date_Init(&sys->video_date, sys->video.frame_rate,
                  sys->video.frame_rate_base);
        SetVideoPts(demux, VLC_TICK_0 + sys->pts_offset);
    }

    sys->current_title = 0;
    sys->chapter_gap = sys->chapter_count > 0 ?
                       (sys->length / sys->chapter_count) : VLC_TICK_INVALID;
    sys->current_chapter = 0;
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
    set_subcategory(SUBCAT_INPUT_ACCESS)
    set_callbacks(Open, Close)
    OPTIONS_GLOBAL(DECLARE_MODULE_OPTIONS)
    OPTIONS_AUDIO(DECLARE_MODULE_SUBOPTIONS)
    OPTIONS_VIDEO(DECLARE_MODULE_SUBOPTIONS)
    OPTIONS_SUB(DECLARE_MODULE_SUBOPTIONS)
    add_shortcut("mock")
vlc_module_end()

#undef X
