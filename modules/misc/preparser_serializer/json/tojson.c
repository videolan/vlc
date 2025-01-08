// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * tojson.c: write json from vlc struct
 *****************************************************************************
 * Copyright © 2025 Videolabs, VideoLAN and VLC authors
 *
 * Authors: Gabriel Lafond Thenaille <gabriel@videolabs.io>
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_charset.h>
#include <vlc_es.h>
#include <vlc_input_item.h>
#include <vlc_strings.h>
#include <vlc_preparser_ipc.h>
#include <vlc_vector.h>

#include <float.h>
#include <stdckdint.h>

#include "serializer.h"

/****************************************************************************
 * Stringify functions
 */

/* Write in the memstream `character` value in utf notation. */
static inline void json_stringify_utf8char(struct serdes_sys *sys,
                                           uint32_t character)
{
    assert(sys != NULL);

    /* If the character is in the Basic Multilingual Plane (U+0000 through
     * U+FFFF), then it may be represented as a six-character sequence:
     * \uxxxx */
    if (character < 0x10000)
    {
        if (serdes_buf_printf(sys, "\\u%04x", character) < 0) {
            return;
        }
    }
    /* To escape an extended character that is not in the Basic Multilingual
     * Plane, the character is represented as a 12-character sequence, encoding
     * the UTF-16 surrogate pair. */

    else if (0x10000 <= character && character <= 0x10FFFF) {
        unsigned int code;
        uint16_t units[2];

        code = (character - 0x10000);
        units[0] = 0xD800 | (code >> 10);
        units[1] = 0xDC00 | (code & 0x3FF);

        if (serdes_buf_printf(sys, "\\u%04x\\u%04x", units[0], units[1]) < 0) {
            return;
        }
    }
}

/* Write in the memstream a string. If is not encoded in utf8 write 'null'
 * instead.
 * `str` mustn't be null.*/
static inline void json_stringify_utf8string(struct serdes_sys *sys,
                                             const char *str)
{
    assert(sys != NULL);
    assert(str != NULL);

    if (!IsUTF8(str)) {
        if (serdes_buf_puts(sys, "null") < 0) {
            return;
        }
        return;
    }

    if (serdes_buf_putc(sys, '\"') < 0) {
        return;
    }

    unsigned char byte;
    while (*str != '\0') {
        switch (*str) {
            case '\b':
                if (serdes_buf_write(sys, "\\b", 2) < 0) {
                    return;
                }
                break;
            case '\f':
                if (serdes_buf_write(sys, "\\f", 2) < 0) {
                    return;
                }
                break;
            case '\n':
                if (serdes_buf_write(sys, "\\n", 2) < 0) {
                    return;
                }
                break;
            case '\r':
                if (serdes_buf_write(sys, "\\r", 2) < 0) {
                    return;
                }
                break;
            case '\t':
                if (serdes_buf_write(sys, "\\t", 2) < 0) {
                    return;
                }
                break;
            case '\\':
                if (serdes_buf_write(sys, "\\\\", 2) < 0) {
                    return;
                }
                break;
            case '\"':
                if (serdes_buf_write(sys, "\\\"", 2) < 0) {
                    return;
                }
                break;
            default:
                byte = *str;
                if (byte <= 0x1F || byte == 0x7F) {
                    if (serdes_buf_printf(sys, "\\u%04x", byte) < 0) {
                        return;
                    }
                }
                else if (byte < 0x80) {
                    if (serdes_buf_putc(sys, byte) < 0) {
                        return;
                    }
                }
                else {
                    uint32_t bytes;
                    ssize_t len = vlc_towc(str, &bytes);
                    assert(len > 0);
                    json_stringify_utf8char(sys, bytes);
                    str += len - 1;
                }
        }
        str++;
    }

    if (serdes_buf_putc(sys, '\"') < 0) {
        return;
    }
}

static inline void json_stringify_double(struct serdes_sys *sys,
                                         double number)
{
    assert(sys != NULL);

    if (serdes_buf_printf(sys, "%.*g", DBL_DECIMAL_DIG, number) < 0) {
        return;
    }
}

static inline void json_stringify_int64(struct serdes_sys *sys,
                                        int64_t number)
{
    assert(sys != NULL);

    if (number > (1LL << 53) - 1 || number < -((1LL << 53) - 1)) {
        if (serdes_buf_printf(sys, "\"%" PRId64 "\"", number) < 0) {
            return;
        }
    } else {
        if (serdes_buf_printf(sys, "%" PRId64, number) < 0) {
            return;
        }
    }
}

static inline void json_stringify_uint64(struct serdes_sys *sys,
                                        uint64_t number)
{
    assert(sys != NULL);

    if (number > (1LL << 53) - 1) {
        if (serdes_buf_printf(sys, "\"%" PRIu64 "\"", number) < 0) {
            return;
        }
    } else {
        if (serdes_buf_printf(sys, "%" PRIu64, number) < 0) {
            return;
        }
    }
}

#define json_stringify_number(sys, number) \
    _Generic(number,\
            int64_t: json_stringify_int64(sys, number),\
            uint64_t: json_stringify_uint64(sys, number),\
            default: json_stringify_double(sys, number))

static inline void json_stringify_string(struct serdes_sys *sys,
                                         const char *str)
{
    assert(sys != NULL);

    if (str != NULL) {
        json_stringify_utf8string(sys, str);
    } else {
        if (serdes_buf_puts(sys, "null") < 0) {
            return;
        }
    }
}

static inline void json_stringify_boolean(struct serdes_sys *sys,
                                          bool boolean)
{
    assert(sys != NULL);

    if (serdes_buf_puts(sys, boolean ? "true" : "false") < 0) {
        return;
    }
}

/* The last argument is for using the function inside marco as the others. */
static inline void json_stringify_null(struct serdes_sys *sys, void *unused)
{
    assert(sys != NULL);

    VLC_UNUSED(unused);
    if (serdes_buf_puts(sys, "null") < 0) {
        return;
    }
}

static inline void json_stringify_fourcc(struct serdes_sys *sys,
                                         vlc_fourcc_t fourcc)
{
    char str[25];
    char *ptr = str;
    char f[5];
    vlc_fourcc_to_char(fourcc, f);
    for (size_t i = 0; i < 4; i++) {
        if (f[i] < 0x20 || f[i] >= 0x7f) {
            int ret = sprintf(ptr, "\\\\x%02" PRIx8, f[i]);
            ptr += ret;
        } else {
            *ptr++ = f[i];
        }
    }
    *ptr = '\0';
    if (serdes_buf_printf(sys, "\"%s\"", str) < 0) {
        return;
    }
}

/****************************************************************************
 * Macro declaration
 */

#define json_stringify_first(type, sys, name, var) \
    do { \
        if (serdes_buf_putc(sys, '{') < 0) { \
            return;\
        }\
     json_stringify_utf8string(sys, name); \
        if (serdes_buf_puts(sys, ": ") < 0) { \
            return;\
        }\
        json_stringify_##type(sys, var); \
        if (serdes_buf_puts(sys, ", ") < 0) { \
            return;\
        }\
    } while (0)

#define json_stringify(type, sys, name, var) \
    do { \
        json_stringify_utf8string(sys, name); \
        if (serdes_buf_puts(sys, ": ") < 0) { \
            return;\
        }\
        json_stringify_##type(sys, var); \
        if (serdes_buf_puts(sys, ", ") < 0) { \
            return;\
        }\
    } while (0)

#define json_stringify_last(type, sys, name, var) \
    do { \
        json_stringify_utf8string(sys, name); \
        if (serdes_buf_puts(sys, ": ") < 0) { \
            return;\
        }\
        json_stringify_##type(sys, var); \
        if (serdes_buf_putc(sys, '}') < 0) { \
            return;\
        }\
    } while (0)

#define json_stringify_array(func, size, sys, name, array) \
    do { \
        if ((name) != NULL) {\
            json_stringify_utf8string(sys, name); \
            if (serdes_buf_puts(sys, ": ") < 0) { \
                return;\
            }\
        }\
        if (serdes_buf_putc(sys, '[') < 0) { \
            return;\
        }\
        if ((size) > 0) { \
            (func)(sys, &(array)[0], NULL); \
            for (size_t _i = 1; _i < (size); _i++) { \
                if (serdes_buf_puts(sys, ", ") < 0) {\
                    return;\
                }\
                (func)(sys, &(array)[_i], NULL); \
            }\
        } \
        if (serdes_buf_putc(sys, ']') < 0) { \
            return;\
        }\
    } while (0)

#define json_stringify_array_value(type, size, sys, name, array) \
    do { \
        if ((name) != NULL) {\
            json_stringify_utf8string(sys, name); \
            if (serdes_buf_puts(sys, ": ") < 0) { \
                return;\
            }\
        }\
        if (serdes_buf_putc(sys, '[') < 0) { \
            return;\
        }\
        if ((size) > 0) { \
            json_stringify_##type(sys, (array)[0]); \
            for (size_t _i = 1; _i < (size); _i++) { \
                if (serdes_buf_puts(sys, ", ") < 0) {\
                    return;\
                }\
                json_stringify_##type(sys, (array)[_i]); \
            }\
        } \
        if (serdes_buf_putc(sys, ']') < 0) { \
            return;\
        }\
    } while (0)

/****************************************************************************
 * tojson functions
 */

static void toJSON_extra_languages(struct serdes_sys *sys,
                                   const extra_languages_t *el,
                                   const char *name)
{
    assert(sys != NULL);

    if (name != NULL) {
        json_stringify_utf8string(sys, name);
        if (serdes_buf_puts(sys, ": ") < 0) {
            return;
        }
    }
    if (el == NULL) {
        if (serdes_buf_puts(sys, "null") < 0) {
            return;
        }
        return;
    }
    json_stringify_first(string, sys, "psz_language", el->psz_language);
    json_stringify_last(string, sys, "psz_description", el->psz_description);
}

static void toJSON_audio_format(struct serdes_sys *sys,
                                const audio_format_t *a, const char *name)
{
    assert(sys != NULL);

    if (name != NULL) {
        json_stringify_utf8string(sys, name);
        if (serdes_buf_puts(sys, ": ") < 0) {
            return;
        }
    }
    if (a == NULL) {
        if (serdes_buf_puts(sys, "null") < 0) {
            return;
        }
        return;
    }
    json_stringify_first(number, sys, "i_format", a->i_format);
    json_stringify(number, sys, "i_rate", a->i_rate);
    json_stringify(number, sys, "i_physical_channels", a->i_physical_channels);
    json_stringify(number, sys, "i_chan_mode", a->i_chan_mode);
    json_stringify(number, sys, "channel_type", a->channel_type);
    json_stringify(number, sys, "i_bytes_per_frame", a->i_bytes_per_frame);
    json_stringify(number, sys, "i_frame_length", a->i_frame_length);
    json_stringify(number, sys, "i_bitspersample", a->i_bitspersample);
    json_stringify(number, sys, "i_blockalign", a->i_blockalign);
    json_stringify_last(number, sys, "i_channels", a->i_channels);
}

static void toJSON_audio_replay_gain(struct serdes_sys *sys,
                                     const audio_replay_gain_t *ag,
                                     const char *name)
{
    assert(sys != NULL);

    if (name != NULL) {
        json_stringify_utf8string(sys, name);
        if (serdes_buf_puts(sys, ": ") < 0) {
            return;
        }
    }
    if (ag == NULL) {
        if (serdes_buf_puts(sys, "null") < 0) {
            return;
        }
        return;
    }
    if (serdes_buf_putc(sys, '{') < 0) {
        return;
    }
    json_stringify_array_value(boolean, AUDIO_REPLAY_GAIN_MAX, sys, "pb_peak",
                               ag->pb_peak);
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }
    json_stringify_array_value(number, AUDIO_REPLAY_GAIN_MAX, sys, "pf_peak",
                               ag->pf_peak);
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }
    json_stringify_array_value(boolean, AUDIO_REPLAY_GAIN_MAX, sys, "pb_gain",
                               ag->pb_gain);
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }
    json_stringify_array_value(number, AUDIO_REPLAY_GAIN_MAX, sys, "pf_gain",
                               ag->pf_gain);
    if (serdes_buf_putc(sys, '}') < 0) {
        return;
    }
}

static void toJSON_video_palette(struct serdes_sys *sys,
                                 const video_palette_t *palette,
                                 const char *name)
{
    assert(sys != NULL);

    if (name != NULL) {
        json_stringify_utf8string(sys, name);
        if (serdes_buf_puts(sys, ": ") < 0) {
            return;
        }
    }
    if (palette == NULL) {
        if (serdes_buf_puts(sys, "null") < 0) {
            return;
        }
        return;
    }

    json_stringify_first(number, sys, "i_entries", palette->i_entries);
    json_stringify_utf8string(sys, "palette");
    if (serdes_buf_puts(sys, ": [") < 0) {
        return;
    }
    int i = 0;
    if (i < VIDEO_PALETTE_COLORS_MAX) {
        json_stringify_array_value(number, 4, sys, NULL, palette->palette[i]);
        for (i = 1; i < VIDEO_PALETTE_COLORS_MAX; i++) {
            if (serdes_buf_puts(sys, ", ") < 0) {
                return;
            }
            json_stringify_array_value(number, 4, sys, NULL,
                                       palette->palette[i]);
        }
    }
    if (serdes_buf_puts(sys, "]}") < 0) {
        return;
    }
}

static void toJSON_video_format(struct serdes_sys *sys,
                                const video_format_t *video,
                                const char *name)
{
    assert(sys != NULL);

    if (name != NULL) {
        json_stringify_utf8string(sys, name);
        if (serdes_buf_puts(sys, ": ") < 0) {
            return;
        }
    }
    if (video == NULL) {
        if (serdes_buf_puts(sys, "null") < 0) {
            return;
        }
        return;
    }
    json_stringify_first(fourcc, sys, "i_chroma", video->i_chroma);
    json_stringify(number, sys, "i_width", video->i_width);
    json_stringify(number, sys, "i_height", video->i_height);
    json_stringify(number, sys, "i_x_offset", video->i_x_offset);
    json_stringify(number, sys, "i_y_offset", video->i_y_offset);
    json_stringify(number, sys, "i_visible_width", video->i_visible_width);
    json_stringify(number, sys, "i_visible_height", video->i_visible_height);
    json_stringify(number, sys, "i_sar_num", video->i_sar_num);
    json_stringify(number, sys, "i_sar_den", video->i_sar_den);
    json_stringify(number, sys, "i_frame_rate", video->i_frame_rate);
    json_stringify(number, sys, "i_frame_rate_base", video->i_frame_rate_base);
    toJSON_video_palette(sys, video->p_palette, "p_palette");
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }
    json_stringify(number, sys, "orientation", video->orientation);
    json_stringify(number, sys, "primaries", video->primaries);
    json_stringify(number, sys, "transfer", video->transfer);
    json_stringify(number, sys, "space", video->space);
    json_stringify(number, sys, "color_range", video->color_range);
    json_stringify(number, sys, "chroma_location", video->chroma_location);
    json_stringify(number, sys, "multiview_mode", video->multiview_mode);
    json_stringify(boolean, sys, "b_multiview_right_eye_first",
                   video->b_multiview_right_eye_first);
    json_stringify(number, sys, "projection_mode", video->projection_mode);

    /* vlc_viewpoint_t pose */
    json_stringify_utf8string(sys, "pose");
    if (serdes_buf_puts(sys, ": ") < 0) {
        return;
    }
    json_stringify_first(number, sys, "yaw", video->pose.yaw);
    json_stringify(number, sys, "pitch", video->pose.pitch);
    json_stringify(number, sys, "roll", video->pose.roll);
    json_stringify_last(number, sys, "fov", video->pose.fov);
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }

    /* video->mastering */
    json_stringify_utf8string(sys, "mastering");
    if (serdes_buf_puts(sys, ": {") < 0) {
        return;
    }
    /* size: G,B,R / x,y */
    json_stringify_array_value(number, 3 * 2, sys, "primaries",
                               video->mastering.primaries);
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }
    /* size: x,y */
    json_stringify_array_value(number, 2, sys, "white_point",
                               video->mastering.white_point);
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }
    json_stringify(number, sys, "max_luminance",
                   video->mastering.max_luminance);
    json_stringify_last(number, sys, "min_luminance",
                        video->mastering.min_luminance);
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }

    /* video->lighting */
    json_stringify_utf8string(sys, "lighting");
    if (serdes_buf_puts(sys, ": ") < 0) {
        return;
    }
    json_stringify_first(number, sys, "MaxCLL", video->lighting.MaxCLL);
    json_stringify_last(number, sys, "MaxFALL", video->lighting.MaxFALL);
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }

    /* video->dovi */
    json_stringify_utf8string(sys, "dovi");
    if (serdes_buf_puts(sys, ": ") < 0) {
        return;
    }
    json_stringify_first(number, sys, "version_major",
                         video->dovi.version_major);
    json_stringify(number, sys, "version_minor", video->dovi.version_minor);
    json_stringify(number, sys, "profile", video->dovi.profile);
    json_stringify(number, sys, "level", video->dovi.level);
    json_stringify(number, sys, "rpu_present", video->dovi.rpu_present);
    json_stringify(number, sys, "el_present", video->dovi.el_present);
    json_stringify_last(number, sys, "bl_present", video->dovi.bl_present);
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }

    json_stringify_last(number, sys, "i_cubemap_padding",
                        video->i_cubemap_padding);
}

static void toJSON_subs_format(struct serdes_sys *sys,
                               const subs_format_t *subs, const char *name)
{
    assert(sys != NULL);

    if (name != NULL) {
        json_stringify_utf8string(sys, name);
        if (serdes_buf_puts(sys, ": ") < 0) {
            return;
        }
    }
    if (subs == NULL) {
        if (serdes_buf_puts(sys, "null") < 0) {
            return;
        }
        return;
    }
    json_stringify_first(string, sys, "psz_encoding", subs->psz_encoding);
    json_stringify(number, sys, "i_x_origin", subs->i_x_origin);
    json_stringify(number, sys, "i_y_origin", subs->i_y_origin);

    /* subs->spu */
    json_stringify_utf8string(sys, "spu");
    if (serdes_buf_puts(sys, ": ") < 0) {
        return;
    }
    json_stringify_first(number, sys, "i_original_frame_width",
                         subs->spu.i_original_frame_width);
    json_stringify(number, sys, "i_original_frame_height",
                   subs->spu.i_original_frame_height);
    json_stringify_array_value(number, VIDEO_PALETTE_CLUT_COUNT, sys,
                               "palette", subs->spu.palette);
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }
    json_stringify_last(boolean, sys, "b_palette", subs->spu.b_palette);
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }

    /* subs->dvb */
    json_stringify_utf8string(sys, "dvb");
    if (serdes_buf_puts(sys, ": {") < 0) {
        return;
    }
    json_stringify_last(number, sys, "i_id", subs->dvb.i_id);
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }

    /* subs->teletext */
    json_stringify_utf8string(sys, "teletext");
    if (serdes_buf_puts(sys, ": ") < 0) {
        return;
    }
    json_stringify_first(number, sys, "i_magazine", subs->teletext.i_magazine);
    json_stringify_last(number, sys, "i_page", subs->teletext.i_page);
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }

    /* subs->cc */
    json_stringify_utf8string(sys, "cc");
    if (serdes_buf_puts(sys, ": ") < 0) {
        return;
    }
    json_stringify_first(number, sys, "i_channel", subs->cc.i_channel);
    json_stringify_last(number, sys, "i_reorder_depth",
                        subs->cc.i_reorder_depth);

    if (serdes_buf_puts(sys, "}") < 0) {
        return;
    }
}

static void toJSON_es_format(struct serdes_sys *sys, const es_format_t *es,
                             const char *name)
{
    assert(sys != NULL);

    if (name != NULL) {
        json_stringify_utf8string(sys, name);
        if (serdes_buf_puts(sys, ": ") < 0) {
            return;
        }
    }
    if (es == NULL) {
        if (serdes_buf_puts(sys, "null") < 0) {
            return;
        }
        return;
    }
    json_stringify_first(number, sys, "i_cat", es->i_cat);
    json_stringify(fourcc, sys, "i_codec", es->i_codec);
    json_stringify(fourcc, sys, "i_original_fourcc", es->i_original_fourcc);
    json_stringify(number, sys, "i_id", es->i_id);
    json_stringify(number, sys, "i_group", es->i_group);
    json_stringify(number, sys, "i_priority", es->i_priority);
    json_stringify(string, sys, "psz_language", es->psz_language);
    json_stringify(string, sys, "psz_description", es->psz_description);
    json_stringify(number, sys, "i_extra_languages", es->i_extra_languages);
    json_stringify_array(toJSON_extra_languages, es->i_extra_languages, sys,
                         "p_extra_languages", es->p_extra_languages);
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }

    switch (es->i_cat) {
        case AUDIO_ES:
            toJSON_audio_format(sys, &es->audio, "audio");
            if (serdes_buf_puts(sys, ", ") < 0) {
                return;
            }
            toJSON_audio_replay_gain(sys, &es->audio_replay_gain,
                                     "audio_replay_gain");
            break;
        case VIDEO_ES:
            toJSON_video_format(sys, &es->video, "video");
            break;
        case SPU_ES:
            toJSON_subs_format(sys, &es->subs, "subs");
            break;
        default:
            break;
    }
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }

    json_stringify(number, sys, "i_bitrate", es->i_bitrate);
    json_stringify(number, sys, "i_profile", es->i_profile);
    json_stringify(number, sys, "i_level", es->i_level);
    json_stringify_last(boolean, sys, "b_packetized", es->b_packetized);
    //json_stringify_last(number, sys, "i_extra", es->i_extra);
    /* p_extra not sent */
}

static void toJSON_meta(struct serdes_sys *sys, const vlc_meta_t *meta,
                        const char *name)
{
#define stringify_meta(sys, meta, type) \
    do { \
        json_stringify_utf8string(sys, #type); \
        if (serdes_buf_puts(sys, ": ") < 0) {\
            return;\
        } \
        vlc_meta_priority_t p_##type = VLC_META_PRIORITY_BASIC; \
        const char *v_##type = vlc_meta_GetWithPriority(meta, vlc_meta_##type,\
                                                        &p_##type); \
        assert(p_##type >= VLC_META_PRIORITY_BASIC \
                && p_##type <= VLC_META_PRIORITY_INBAND); \
        json_stringify_first(string, sys, "value", v_##type); \
        json_stringify_last(number, sys, "priority", p_##type); \
        if (serdes_buf_puts(sys, ", ") < 0) {\
            return;\
        } \
    } while (0)

    assert(sys != NULL);

    if (name != NULL) {
        json_stringify_utf8string(sys, name);
        if (serdes_buf_puts(sys, ": ") < 0) {
            return;
        }
    }
    if (meta == NULL) {
        if (serdes_buf_puts(sys, "null") < 0) {
            return;
        }
        return;
    }
    if (serdes_buf_putc(sys, '{') < 0) {
        return;
    }
    stringify_meta(sys, meta, Title);
    stringify_meta(sys, meta, Artist);
    stringify_meta(sys, meta, AlbumArtist);
    stringify_meta(sys, meta, Genre);
    stringify_meta(sys, meta, Copyright);
    stringify_meta(sys, meta, Album);
    stringify_meta(sys, meta, TrackNumber);
    stringify_meta(sys, meta, Description);
    stringify_meta(sys, meta, Rating);
    stringify_meta(sys, meta, Date);
    stringify_meta(sys, meta, Setting);
    stringify_meta(sys, meta, URL);
    stringify_meta(sys, meta, Language);
    stringify_meta(sys, meta, NowPlaying);
    stringify_meta(sys, meta, ESNowPlaying);
    stringify_meta(sys, meta, Publisher);
    stringify_meta(sys, meta, EncodedBy);
    stringify_meta(sys, meta, ArtworkURL);
    stringify_meta(sys, meta, TrackID);
    stringify_meta(sys, meta, TrackTotal);
    stringify_meta(sys, meta, Director);
    stringify_meta(sys, meta, Season);
    stringify_meta(sys, meta, Episode);
    stringify_meta(sys, meta, ShowName);
    stringify_meta(sys, meta, Actors);
    stringify_meta(sys, meta, DiscNumber);
    stringify_meta(sys, meta, DiscTotal);

#undef stringify_meta

    json_stringify_utf8string(sys, "extra_tags");
    if (serdes_buf_puts(sys, ": ") < 0) {
        return;
    }
    if (serdes_buf_putc(sys, '[') < 0) {
        return;
    }
    char **keys = vlc_meta_CopyExtraNames(meta);
    if (keys != NULL) {
        for (size_t i = 0; i < vlc_meta_GetExtraCount(meta); i++) {
            if (i != 0) {
                if (serdes_buf_puts(sys, ", ") < 0) {
                    return;
                }
            }
            vlc_meta_priority_t priority = VLC_META_PRIORITY_BASIC;
            const char *value = vlc_meta_GetExtraWithPriority(meta, keys[i],
                                                              &priority);
            assert(priority >= VLC_META_PRIORITY_BASIC
                    && priority <= VLC_META_PRIORITY_INBAND);
            json_stringify_first(string, sys, "key", keys[i]);
            json_stringify(string, sys, "value", value);
            json_stringify_last(number, sys, "priority", priority);
            free(keys[i]);
        }
        free(keys);
    }
    if (serdes_buf_putc(sys, ']') < 0) {
        return;
    }
    if (serdes_buf_putc(sys, '}') < 0) {
        return;
    }
}

static void toJSON_input_item_es(struct serdes_sys *sys,
                                 const struct input_item_es *item_es,
                                 const char *name)
{
    assert(sys != NULL);

    if (name != NULL) {
        json_stringify_utf8string(sys, name);
        if (serdes_buf_puts(sys, ": ") < 0) {
            return;
        }
    }
    if (item_es == NULL) {
        if (serdes_buf_puts(sys, "null") < 0) {
            return;
        }
        return;
    }
    if (serdes_buf_putc(sys, '{') < 0) {
        return;
    }
    toJSON_es_format(sys, &item_es->es, "es");
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }
    json_stringify(string, sys, "id", item_es->id);
    json_stringify_last(boolean, sys, "id_stable", item_es->id_stable);
}

static void toJSON_input_item_slave(struct serdes_sys *sys,
                                    struct input_item_slave * const * slave,
                                    const char *name)
{
    assert(sys != NULL);
    assert(slave != NULL);

    if (name != NULL) {
        json_stringify_utf8string(sys, name);
        if (serdes_buf_puts(sys, ": ") < 0) {
            return;
        }
    }
    if (*slave == NULL) {
        if (serdes_buf_puts(sys, "null") < 0) {
            return;
        }
        return;
    }
    json_stringify_first(number, sys, "i_type", (*slave)->i_type);
    json_stringify(number, sys, "i_priority", (*slave)->i_priority);
    json_stringify(boolean, sys, "b_forced", (*slave)->b_forced);
    json_stringify_last(string, sys, "psz_uri", (*slave)->psz_uri);
}

static inline void toJSON_input_item(struct serdes_sys *sys,
                                     const input_item_t *item,
                                     const char *name)
{
    assert(sys != NULL);

    if (name != NULL) {
        json_stringify_utf8string(sys, name);
        if (serdes_buf_puts(sys, ": ") < 0) {
            return;
        }
    }
    if (item == NULL) {
        if (serdes_buf_puts(sys, "null") < 0) {
            return;
        }
        return;
    }
    json_stringify_first(string, sys, "psz_name", item->psz_name);
    json_stringify(string, sys, "psz_uri", item->psz_uri);
    json_stringify(number, sys, "i_duration", item->i_duration);
    json_stringify_array(toJSON_input_item_es, item->es_vec.size, sys,
                         "es_vec", item->es_vec.data);
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }
    toJSON_meta(sys, item->p_meta, "p_meta");
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }
    json_stringify(number, sys, "i_slaves", item->i_slaves);
    json_stringify_array(toJSON_input_item_slave, (size_t)item->i_slaves, sys,
                         "pp_slaves", item->pp_slaves);
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }
    json_stringify(number, sys, "i_type", item->i_type);
    json_stringify_last(boolean, sys, "b_net", item->b_net);
}

static inline void toJSON_input_item_node(struct serdes_sys *sys,
                                          input_item_node_t * const *n,
                                          const char *name)
{
    assert(sys != NULL);
    assert(n != NULL);
    const input_item_node_t *node = *n;

    if (name != NULL) {
        json_stringify_utf8string(sys, name);
        if (serdes_buf_puts(sys, ": ") < 0) {
            return;
        }
    }
    if (node == NULL) {
        if (serdes_buf_puts(sys, "null") < 0) {
            return;
        }
        return;
    }
    if (serdes_buf_putc(sys, '{') < 0) {
        return;
    }
    toJSON_input_item(sys, node->p_item, "p_item");
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }
    json_stringify(number, sys, "i_children", node->i_children);
    json_stringify_array(toJSON_input_item_node, (size_t)node->i_children, sys,
                         "pp_children", node->pp_children);
    if (serdes_buf_putc(sys, '}') < 0) {
        return;
    }
}

static void toJSON_input_attachment(struct serdes_sys *sys,
                                    input_attachment_t * const *a,
                                    const char *name)
{
    assert(sys != NULL);
    assert(a != NULL);

    if (name != NULL) {
        json_stringify_utf8string(sys, name);
        if (serdes_buf_puts(sys, ": ") < 0) {
            return;
        }
    }
    if (*a == NULL) {
        if (serdes_buf_puts(sys, "null") < 0) {
            return;
        }
        return;
    }
    json_stringify_first(string, sys, "psz_name", (*a)->psz_name);
    json_stringify(string, sys, "psz_mime", (*a)->psz_mime);
    json_stringify(string, sys, "psz_description", (*a)->psz_description);
    json_stringify(number, sys, "i_data", (*a)->i_data);
    if (sys->bin_data) {
        json_stringify_last(null, sys, "p_data", NULL);
        vlc_vector_push_all(&sys->attach_data , (*a)->p_data, (*a)->i_data);
    } else {
        char *b64 = vlc_b64_encode_binary((*a)->p_data, (*a)->i_data);
        json_stringify_last(string, sys, "p_data", b64);
        free(b64);
    }
}

static void toJSON_plane(struct serdes_sys *sys, const plane_t *plane,
                         const char *name)
{
    assert(sys != NULL);

    if (name != NULL) {
        json_stringify_utf8string(sys, name);
        if (serdes_buf_puts(sys, ": ") < 0) {
            return;
        }
    }
    if (plane == NULL) {
        if (serdes_buf_puts(sys, "null") < 0) {
            return;
        }
        return;
    }
    if (sys->bin_data) {
        json_stringify_first(null, sys, "p_pixels", NULL);
        vlc_vector_push_all(&sys->attach_data, plane->p_pixels,
                            plane->i_pitch * plane->i_lines);
    } else {
        char *b64 = vlc_b64_encode_binary(plane->p_pixels,
                                          plane->i_pitch * plane->i_lines);
        json_stringify_first(string, sys, "p_pixels", b64);
        free(b64);
    }
    json_stringify(number, sys, "i_lines", plane->i_lines);
    json_stringify(number, sys, "i_pitch", plane->i_pitch);
    json_stringify(number, sys, "i_pixel_pitch", plane->i_pixel_pitch);
    json_stringify(number, sys, "i_visible_lines", plane->i_visible_lines);
    json_stringify_last(number, sys, "i_visible_pitch", plane->i_visible_pitch);
}

static void toJSON_picture(struct serdes_sys *sys, const picture_t *pic,
                           const char *name)
{
    assert(sys != NULL);

    if (name != NULL) {
        json_stringify_utf8string(sys, name);
        if (serdes_buf_puts(sys, ": ") < 0) {
            return;
        }
    }
    if (pic == NULL) {
        if (serdes_buf_puts(sys, "null") < 0) {
            return;
        }
        return;
    }
    if (serdes_buf_putc(sys, '{') < 0) {
        return;
    }
    toJSON_video_format(sys, &pic->format, "format");
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }
    json_stringify_array(toJSON_plane, (size_t)pic->i_planes, sys, "p", pic->p);
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }
    json_stringify(number, sys, "i_planes", pic->i_planes);
    json_stringify(number, sys, "date", pic->date);
    json_stringify(boolean, sys, "b_force", pic->b_force);
    json_stringify(boolean, sys, "b_still", pic->b_still);
    json_stringify(boolean, sys, "b_progressive", pic->b_progressive);
    json_stringify(boolean, sys, "b_top_field_first", pic->b_top_field_first);
    json_stringify(boolean, sys, "b_multiview_left_eye",
                   pic->b_multiview_left_eye);
    json_stringify_last(number, sys, "i_nb_fields", pic->i_nb_fields);
}

static void
toJSON_vlc_thumbnailer_output(struct serdes_sys *sys,
                              const struct vlc_thumbnailer_output *output,
                              const char *name)
{
    assert(sys != NULL);

    if (name != NULL) {
        json_stringify_utf8string(sys, name);
        if (serdes_buf_puts(sys, ": ") < 0) {
            return;
        }
    }
    if (output == NULL) {
        if (serdes_buf_puts(sys, "null") < 0) {
            return;
        }
        return;
    }
    json_stringify_first(number, sys, "format", output->format);
    json_stringify(number, sys, "width", output->width);
    json_stringify(number, sys, "height", output->height);
    json_stringify(boolean, sys, "crop", output->crop);
    json_stringify(string, sys, "file_path", output->file_path);
    json_stringify_last(number, sys, "creat_mode", output->creat_mode);
}

static void
toJSON_vlc_preparser_msg_req(struct serdes_sys *sys,
                             const struct vlc_preparser_msg_req *req,
                             const char *name)
{
    assert(sys != NULL);

    if (name != NULL) {
        json_stringify_utf8string(sys, name);
        if (serdes_buf_puts(sys, ": ") < 0) {
            return;
        }
    }
    if (req == NULL) {
        if (serdes_buf_puts(sys, "null") < 0) {
            return;
        }
        return;
    }

    json_stringify_first(number, sys, "type", req->type);

    assert(req->type == VLC_PREPARSER_MSG_REQ_TYPE_PARSE ||
            req->type == VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL ||
            req->type == VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL_TO_FILES);

    if (req->type == VLC_PREPARSER_MSG_REQ_TYPE_PARSE) {
        json_stringify(number, sys, "options", req->options);
    } else {
        if (req->type == VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL_TO_FILES) {
            json_stringify_array(toJSON_vlc_thumbnailer_output,
                                 req->outputs.size, sys, "outputs",
                                 req->outputs.data);
            if (serdes_buf_puts(sys, ", ") < 0) {
                return;
            }
        }
        json_stringify(number, sys, "seek.type", req->arg.seek.type);
        if (req->arg.seek.type == VLC_THUMBNAILER_SEEK_TIME) {
            json_stringify(number, sys, "seek.time", req->arg.seek.time);
        } else if (req->arg.seek.type == VLC_THUMBNAILER_SEEK_POS) {
            json_stringify(number, sys, "seek.pos", req->arg.seek.pos);
        } else if (req->arg.seek.type != VLC_THUMBNAILER_SEEK_NONE) {
            vlc_assert_unreachable();
        }
        json_stringify(number, sys, "seek.speed", req->arg.seek.speed);
        json_stringify(boolean, sys, "hw_dec", req->arg.hw_dec);
    }

    json_stringify_last(string, sys, "uri", req->uri);
}

/*
 * TODO: use more json_stringify_array if possible and add to toJSON_* functions
 * name and possible null.
 */
static void
toJSON_vlc_preparser_msg_res(struct serdes_sys *sys,
                             const struct vlc_preparser_msg_res *res,
                             const char *name)
{
    assert(sys != NULL);

    if (name != NULL) {
        json_stringify_utf8string(sys, name);
        if (serdes_buf_puts(sys, ": ") < 0) {
            return;
        }
    }
    if (res == NULL) {
        if (serdes_buf_puts(sys, "null") < 0) {
            return;
        }
        return;
    }

    json_stringify_first(number, sys, "type", res->type);
    switch (res->type) {
        case VLC_PREPARSER_MSG_REQ_TYPE_PARSE:
            json_stringify_array(toJSON_input_attachment, res->attachments.size,
                    sys, "attachments", res->attachments.data);
            if (serdes_buf_puts(sys, ", ") < 0) {
                return;
            }
            toJSON_input_item_node(sys, &res->subtree, "subtree");
            break;
        case VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL:
            toJSON_picture(sys, res->pic, "pic");
            break;
        case VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL_TO_FILES:
            json_stringify_array_value(boolean, res->result.size, sys, "result",
                    res->result.data);
            break;
        default:
            vlc_assert_unreachable();
    }
    if (serdes_buf_puts(sys, ", ") < 0) {
        return;
    }
    json_stringify(number, sys, "status", res->status);
    toJSON_input_item(sys, res->item, "item");
    if (serdes_buf_puts(sys, "}") < 0) {
        return;
    }
}

void
toJSON_vlc_preparser_msg(struct serdes_sys *sys,
                         const struct vlc_preparser_msg *msg)
{
    assert(sys != NULL);
    assert(msg != NULL);

    json_stringify_first(number, sys, "type", msg->type);
    json_stringify(number, sys, "req_type", msg->req_type);

    if (msg->type == VLC_PREPARSER_MSG_TYPE_REQ) {
        toJSON_vlc_preparser_msg_req(sys, &msg->req, "req");
    } else if (msg->type == VLC_PREPARSER_MSG_TYPE_RES) {
        toJSON_vlc_preparser_msg_res(sys, &msg->res, "res");
    } else {
        vlc_assert_unreachable();
    }
    if (serdes_buf_puts(sys, "}") < 0) {
        return;
    }
}

#undef json_stringify_first
#undef json_stringify
#undef json_stringify_last
#undef json_stringify_array
#undef json_stringify_array_value
