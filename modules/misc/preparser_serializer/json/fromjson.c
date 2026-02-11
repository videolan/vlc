// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * fromjson.c: functions to translate json to object.
 *****************************************************************************
 * Copyright © 2025 Videolabs, VideoLAN and VLC authors
 *
 * Authors: Gabriel Lafond Thenaille <gabriel@videolabs.io>
 *****************************************************************************/

#include "vlc_input.h"
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_es.h>
#include <vlc_input_item.h>
#include <vlc_preparser.h>
#include <vlc_preparser_ipc.h>
#include <vlc_strings.h>

#include <assert.h>
#include <float.h>
#include <limits.h>

#include "serializer.h"
#include "../../../demux/json/json.h"

/****************************************************************************
 * Helpers
 */

/* Translate a `const char *` fourcc to its `vlc_fourcc_t` version. */
static inline vlc_fourcc_t vlc_fourcc_from_char(const char *fourcc)
{
    assert(fourcc != NULL);

    char f[4] = {0};
    int i = 0;
    while (*fourcc != '\0' && i < 4) {
        if (*fourcc == '\\') {
            if (strlen(fourcc) < 4 || fourcc[1] != 'x') {
                return VLC_CODEC_UNKNOWN;
            }
            char hex[3] = {fourcc[2], fourcc[3], 0};
            if (sscanf(hex, "%2"SCNx8, &f[i++]) != 1) {
                return VLC_CODEC_UNKNOWN;
            }
            fourcc += 4;
        } else {
            f[i++] = *fourcc++;
        }
    }
    if (*fourcc != '\0') {
        return VLC_CODEC_UNKNOWN;
    }
    return VLC_FOURCC(f[0], f[1], f[2], f[3]);
}

/* Get value in `obj` with the key `name` and check if it's a number and if 
 * it's between `min` and `max`. */
static inline bool json_object_to_number(const struct json_object *obj,
                                           const char *name, double *number,
                                           bool *error, double min, double max)
{
    assert(obj != NULL);
    assert(name != NULL);
    assert(number != NULL);
    assert(error != NULL);

    if (*error) {
        return *error;
    }
    const struct json_value *v = json_get(obj, name);
    if (v == NULL || v->type != JSON_NUMBER) {
        *error = true;
        return *error;
    }
    if (v->number < min || v->number > max) {
        *error = true;
        return *error;
    }
    *number = v->number;
    return *error;
}

/* Ensure that the returned number is has not a decimal part and can be cast as
 * an integer. */
static inline bool json_object_to_integer(const struct json_object *obj,
                                          const char *name, int *number,
                                          bool *error, int MIN, int MAX)
{
    assert(obj != NULL);
    assert(name != NULL);
    assert(number != NULL);
    assert(error != NULL);

    double n = NAN;
    json_object_to_number(obj, name, &n, error, MIN, MAX);
    if (*error || isnan(n) || isinf(n) || (floor(n) != ceil(n))) {
        *error = true;
        return *error;
    }
    *number = (int)n;
    return *error;
}

#define json_object_to_enum(obj, name, integer, error, MIN, MAX) \
    json_object_to_integer(obj, name, (int *)(integer), error, MIN, MAX)

#define json_object_to_int_def(suffix, integer, MIN, MAX) \
static inline bool json_object_to_##suffix(const struct json_object *obj,\
                                           const char *name, integer *number,\
                                           bool *error)\
{\
    assert(obj != NULL);\
    assert(name != NULL);\
    assert(number != NULL);\
    assert(error != NULL);\
\
    const struct json_value *v = json_get(obj, name);\
    if (v == NULL || v->type != JSON_NUMBER) {\
        *error = true;\
        return *error;\
    }\
    if (*error || isnan(v->number) || isinf(v->number)\
               || (floor(v->number) != ceil(v->number))) {\
        *error = true;\
        return *error;\
    }\
    if (v->number < MIN || v->number > MAX) {\
        *error = true;\
        return *error;\
    }\
    *number = (integer)v->number;\
    return *error;\
}

json_object_to_int_def(int8, int8_t, INT8_MIN, INT8_MAX)
json_object_to_int_def(int16, int16_t, INT16_MIN, INT16_MAX)
json_object_to_int_def(int32, int32_t, INT32_MIN, INT32_MAX)
json_object_to_int_def(uint8, uint8_t, 0, UINT8_MAX)
json_object_to_int_def(uint16, uint16_t, 0, UINT16_MAX)
json_object_to_int_def(uint32, uint32_t, 0, UINT32_MAX)
#undef json_object_to_int_def

/** Signed interger functions */

static inline bool json_object_to_int64(const struct json_object *obj,
                                        const char *name, int64_t *number,
                                        bool *error)
{
    assert(obj != NULL);
    assert(name != NULL);
    assert(number != NULL);
    assert(error != NULL);

    if (*error) {
        return *error;
    }
    const struct json_value *v = json_get(obj, name);
    if (v == NULL) {
        *error = true;
        return *error;
    }
    int64_t num = 0;
    switch (v->type) {
        case JSON_NUMBER:
            if (isnan(v->number) || isinf(v->number)
                || (floor(v->number) != ceil(v->number))) {
                *error = true;
                return *error;
            }
            num = v->number;
            break;
        case JSON_STRING:
            {
                char *endptr = NULL;
                num = (int64_t)strtoll(v->string, &endptr, 10);
                if (*endptr != '\0' || endptr == v->string) {
                    *error = true;
                    return *error;
                }
            }
            break;
        default:
            *error = true;
            return *error;
    }
    *number = num;
    return *error;
}

static inline bool json_object_to_int_sized(const struct json_object *obj,
                                             const char *name, void *number,
                                             size_t size, bool *error)
{
    assert(obj != NULL);
    assert(name != NULL);
    assert(number != NULL);
    assert(error != NULL);

    switch (size) {
        case sizeof(int64_t):
            return json_object_to_int64(obj, name, number, error);
        case sizeof(int32_t):
            return json_object_to_int32(obj, name, number, error);
        case sizeof(int16_t):
            return json_object_to_int16(obj, name, number, error);
        case sizeof(int8_t):
            return json_object_to_int8(obj, name, number, error);
        default:
            (assert(!"unreachable"), unreachable());
    }
}

#define json_object_to_int(obj, name, number, error) \
    json_object_to_int_sized(obj, name, number, sizeof(*(number)), error)

/** Unsigned interger functions */

static inline bool json_object_to_uint64(const struct json_object *obj,
                                         const char *name, uint64_t *number,
                                         bool *error)
{
    assert(obj != NULL);
    assert(name != NULL);
    assert(number != NULL);
    assert(error != NULL);

    if (*error) {
        return *error;
    }
    const struct json_value *v = json_get(obj, name);
    if (v == NULL) {
        *error = true;
        return *error;
    }
    uint64_t num = 0;
    switch (v->type) {
        case JSON_NUMBER:
            if (isnan(v->number) || isinf(v->number)
                || (floor(v->number) != ceil(v->number))) {
                *error = true;
                return *error;
            }
            num = v->number;
            break;
        case JSON_STRING:
            {
                char *endptr = NULL;
                num = (uint64_t)strtoull(v->string, &endptr, 10);
                if (*endptr != '\0' || endptr == v->string) {
                    *error = true;
                    return *error;
                }
            }
            break;
        default:
            *error = true;
            return *error;
    }
    *number = num;
    return *error;
}

static inline bool json_object_to_uint_sized(const struct json_object *obj,
                                             const char *name, void *number,
                                             size_t size, bool *error)
{
    assert(obj != NULL);
    assert(name != NULL);
    assert(number != NULL);
    assert(error != NULL);

    switch (size) {
        case sizeof(uint64_t):
            return json_object_to_uint64(obj, name, number, error);
        case sizeof(uint32_t):
            return json_object_to_uint32(obj, name, number, error);
        case sizeof(uint16_t):
            return json_object_to_uint16(obj, name, number, error);
        case sizeof(uint8_t):
            return json_object_to_uint8(obj, name, number, error);
        default:
            (assert(!"unreachable"), unreachable());
    }
}

#define json_object_to_uint(obj, name, number, error) \
    json_object_to_uint_sized(obj, name, number, sizeof(*(number)), error)


/* Ensure that the returned number is a double. */
#define json_object_to_double(obj, name, number, error) \
    json_object_to_number(obj, name, number, error, -DBL_MAX, DBL_MAX)

/* Ensure that the returned number is a float. */
#define json_object_to_float(obj, name, number, error) \
    json_object_to_number(obj, name, (double *)(number), error, -FLT_MAX, FLT_MAX)

/* Get value in `obj` with the key `name` and check if it's a string or null.
 */
static inline bool json_object_to_string(const struct json_object *obj,
                                         const char *name, char **str,
                                         bool *error)
{
    assert(obj != NULL);
    assert(name != NULL);
    assert(str != NULL);
    assert(error != NULL);

    if (*error) {
        return *error;
    }

    const struct json_value *v = json_get(obj, name);
    if (v == NULL || (v->type != JSON_STRING && v->type != JSON_NULL)) {
        *error = true;
        return *error;
    }
    if (v->type == JSON_STRING) {
        char *dup = strdup(v->string);
        if (dup != NULL) {
            *str = dup;
            return *error;
        }
    } else {
        *str = NULL;
        return *error;
    }
    *error = true;
    return *error;
}

/* Get value in `obj` with the key `name` and check if it's a boolean. */
static inline bool json_object_to_boolean(const struct json_object *obj,
                                          const char *name, bool *boolean,
                                          bool *error)
{
    assert(obj != NULL);
    assert(name != NULL);
    assert(boolean != NULL);
    assert(error != NULL);

    if (*error) {
        return *error;
    }

    const struct json_value *v = json_get(obj, name);
    if (v == NULL || v->type != JSON_BOOLEAN) {
        *error = true;
        return *error;
    }
    *boolean = v->boolean;
    return *error;
}

/****************************************************************************
 * Macro definition
 */

struct json_array_iter {
    size_t i;
    const struct json_array *a;
};

/* Get the array with `name` as a key, if there is not array with this key set
 * `error` to true and don't start the loop, else iterate over the array and
 * set `item` to the current value each round. */
#define json_array_foreach_ref(obj, name, item, error) \
    for (struct json_array_iter \
            idx_##item = { 0, json_get_array(obj, name) }; \
            ((idx_##item.a != NULL || (*(error) = true, false)) && \
            idx_##item.i < idx_##item.a->size && \
            ((item) = &(idx_##item.a)->entries[idx_##item.i], true)); \
            idx_##item.i++)

/* Load value from a json array to a double array. If there is non number
 * element, too many or not enough element or element not between min and max
 * set `error` to true. */
#define json_array_double_load(obj, name, error, var, size, min, max) \
    do {\
        int idx = 0;\
        struct json_value *_v;\
        json_array_foreach_ref(obj, name, _v, &(err)) {\
            if (_v->type != JSON_NUMBER || idx >= (size)) {\
                (error) = true;\
                break;\
            }\
            if (_v->number >= (min) && _v->number <= (max)) {\
                (var)[idx++] = _v->number;\
            }\
        }\
        if (idx != (size)) {\
            (error) = true;\
        }\
    } while (0)

/* Load value from a json array to a integer array. If there is non number
 * element, too many or not enough element, element not between min and max
 * or non integer number set `error` to true. */
#define json_array_integer_load(obj, name, error, var, size, min, max) \
    do {\
        int idx = 0;\
        struct json_value *_v;\
        json_array_foreach_ref(obj, name, _v, &(err)) {\
            if (_v->type != JSON_NUMBER || idx >= (size)) {\
                (error) = true;\
                break;\
            }\
            double num = _v->number;\
            if (isnan(num) || isinf(num) || (floor(num) != ceil(num))) {\
                break;\
            }\
            if (num >= (min) && num <= (max)) {\
                (var)[idx++] = num;\
            }\
        }\
        if (idx != (size)) {\
            (error) = true;\
        }\
    } while (0)

/* Load value from a json array to a boolean array. If there is non boolean
 * element or too many or not enough element set `error` to true. */
#define json_array_boolean_load(obj, name, error, var, size) \
    do {\
        int idx = 0;\
        struct json_value *_v;\
        json_array_foreach_ref(obj, name, _v, &(err)) {\
            if (_v->type != JSON_BOOLEAN || idx >= (size)) {\
                (error) = true;\
                break;\
            }\
            (var)[idx++] = _v->boolean;\
        }\
        if (idx != (size)) {\
            (error) = true;\
        }\
    } while (0)

/* Call func with object found in parent with name as a key */
#define json_object_from_name(sys, parent, name, var, error, ornull, func) \
    do {\
        const struct json_value *_v = json_get(parent, name);\
        if (_v == NULL) {\
            (error) = true;\
            break;\
        }\
        if (ornull == true && _v->type == JSON_NULL) {\
            break;\
        } else if (_v->type == JSON_OBJECT) {\
            (func)(sys, &_v->object, var, &(error));\
        } else {\
            (error) = true;\
        }\
    } while (0)

/****************************************************************************
 * FromJSON functions
 */

static void fromJSON_audio_format(struct serdes_sys *sys,
                                  const struct json_object *obj,
                                  audio_format_t *audio, bool *error)
{
    assert(obj != NULL);
    assert(audio != NULL);
    assert(error != NULL);

    VLC_UNUSED(sys);

    bool err = false;

    json_object_to_uint(obj, "i_rate", &audio->i_rate, &err);
    json_object_to_uint(obj, "i_physical_channels",
                        &audio->i_physical_channels, &err);
    json_object_to_uint(obj, "i_chan_mode", &audio->i_chan_mode, &err);
    json_object_to_enum(obj, "channel_type", &audio->channel_type, &err,
                        AUDIO_CHANNEL_TYPE_BITMAP,
                        AUDIO_CHANNEL_TYPE_AMBISONICS);
    json_object_to_uint(obj, "i_bytes_per_frame", &audio->i_bytes_per_frame,
                        &err);
    json_object_to_uint(obj, "i_frame_length", &audio->i_frame_length, &err);
    json_object_to_uint(obj, "i_bitspersample", &audio->i_bitspersample, &err);
    json_object_to_uint(obj, "i_blockalign", &audio->i_blockalign, &err);
    json_object_to_uint(obj, "i_channels", &audio->i_channels, &err);

    *error |= err;
}

static void fromJSON_audio_replay_gain(struct serdes_sys *sys,
                                       const struct json_object *obj,
                                       audio_replay_gain_t *ar, bool *error)
{
    assert(obj != NULL);
    assert(ar != NULL);
    assert(error != NULL);

    VLC_UNUSED(sys);

    bool err = false;

    json_array_boolean_load(obj, "pb_peak", err, ar->pb_peak,
                            AUDIO_REPLAY_GAIN_MAX);
    json_array_double_load(obj, "pf_peak", err, ar->pf_peak,
                           AUDIO_REPLAY_GAIN_MAX, -FLT_MIN, FLT_MAX);
    json_array_boolean_load(obj, "pb_gain", err, ar->pb_gain,
                            AUDIO_REPLAY_GAIN_MAX);
    json_array_double_load(obj, "pf_gain", err, ar->pf_gain,
                           AUDIO_REPLAY_GAIN_MAX, -FLT_MIN, FLT_MAX);

    *error |= err;
}

static void fromJSON_video_palette(struct serdes_sys *sys,
                                   const struct json_object *obj,
                                   video_palette_t **palette, bool *error)
{
    assert(obj != NULL);
    assert(palette != NULL);
    assert(error != NULL);

    VLC_UNUSED(sys);

    bool err = false;

    video_palette_t *p = malloc(sizeof(*p));
    if (p == NULL) {
        *error = true;
        return;
    }

    json_object_to_int(obj, "i_entries", &p->i_entries, &err);
    struct json_value *v;
    int i = 0;
    json_array_foreach_ref(obj, "palette", v, &err) {
        if (v->type != JSON_ARRAY) {
            continue;
        }
        for (size_t j = 0; j < v->array.size; j++) {
            struct json_value *subv = &v->array.entries[j];
            if (subv->type != JSON_NUMBER || subv->number < 0 ||
                    subv->number > 255 || j >= 4) {
                err = true;
                break;
            }
            p->palette[i][j] = subv->number;
        }
        if (err || i >= VIDEO_PALETTE_COLORS_MAX) {
            err = true;
            break;
        }
        i++;
    }

    if (err) {
        free(p);
    } else {
        *palette = p;
    }
    *error |= err;
}

static void fromJSON_video_format(struct serdes_sys *sys,
                                  const struct json_object *obj,
                                  video_format_t *video, bool *error)
{
    assert(obj != NULL);
    assert(video != NULL);
    assert(error != NULL);

    bool err = false;

    char *chroma = NULL;
    json_object_to_string(obj, "i_chroma", &chroma, &err);
    video->i_chroma = chroma != NULL ? vlc_fourcc_from_char(chroma) : 0;
    free(chroma);
    json_object_to_uint(obj, "i_width", &video->i_width, &err);
    json_object_to_uint(obj, "i_height", &video->i_height, &err);
    json_object_to_uint(obj, "i_x_offset", &video->i_x_offset, &err);
    json_object_to_uint(obj, "i_y_offset", &video->i_y_offset, &err);
    json_object_to_uint(obj, "i_visible_width", &video->i_visible_width, &err);
    json_object_to_uint(obj, "i_visible_height", &video->i_visible_height, &err);
    json_object_to_uint(obj, "i_sar_num", &video->i_sar_num, &err);
    json_object_to_uint(obj, "i_sar_den", &video->i_sar_den, &err);
    json_object_to_uint(obj, "i_frame_rate", &video->i_frame_rate, &err);
    json_object_to_uint(obj, "i_frame_rate_base", &video->i_frame_rate_base, &err);
    video->p_palette = NULL;
    json_object_from_name(sys, obj, "p_palette", &video->p_palette, err, true,
                          fromJSON_video_palette);
    json_object_to_enum(obj, "orientation", &video->orientation, &err,
                        ORIENT_TOP_LEFT, ORIENT_MAX);
    json_object_to_enum(obj, "primaries", &video->primaries, &err,
                        COLOR_PRIMARIES_UNDEF, COLOR_PRIMARIES_MAX);
    json_object_to_enum(obj, "transfer", &video->transfer, &err,
                        TRANSFER_FUNC_UNDEF, TRANSFER_FUNC_MAX);
    json_object_to_enum(obj, "space", &video->space, &err, COLOR_SPACE_UNDEF,
                        COLOR_SPACE_MAX);
    json_object_to_enum(obj, "color_range", &video->color_range, &err,
                        COLOR_RANGE_UNDEF, COLOR_RANGE_MAX);
    json_object_to_enum(obj, "chroma_location", &video->chroma_location, &err,
                        CHROMA_LOCATION_UNDEF, CHROMA_LOCATION_MAX);
    json_object_to_enum(obj, "multiview_mode", &video->multiview_mode, &err,
                        MULTIVIEW_2D, MULTIVIEW_STEREO_MAX);
    json_object_to_boolean(obj, "b_multiview_right_eye_first",
                           &video->b_multiview_right_eye_first, &err);
    json_object_to_enum(obj, "projection_mode", &video->projection_mode, &err,
                        PROJECTION_MODE_RECTANGULAR,
                        PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD);

    const struct json_object *subobj = json_get_object(obj, "pose");
    err |= (subobj == NULL);
    if (!err) {
        json_object_to_float(subobj, "yaw", &video->pose.yaw, &err);
        json_object_to_float(subobj, "pitch", &video->pose.pitch, &err);
        json_object_to_float(subobj, "roll", &video->pose.roll, &err);
        json_object_to_float(subobj, "fov", &video->pose.fov, &err);
    }

    subobj = json_get_object(obj, "mastering");
    err |= (subobj == NULL);
    if (!err) {
        json_array_integer_load(subobj, "primaries", err,
                                video->mastering.primaries, 3*2,
                                0, UINT16_MAX);
        json_array_integer_load(subobj, "white_point", err,
                                video->mastering.white_point, 2,
                                0, UINT16_MAX);
        json_object_to_uint(subobj, "max_luminance",
                            &video->mastering.max_luminance, &err);
        json_object_to_uint(subobj, "min_luminance",
                            &video->mastering.min_luminance, &err);
    }

    subobj = json_get_object(obj, "lighting");
    err |= (subobj == NULL);
    if (!err) {
        json_object_to_uint(subobj, "MaxCLL", &video->lighting.MaxCLL, &err);
        json_object_to_uint(subobj, "MaxFALL", &video->lighting.MaxFALL,
                            &err);
    }

    subobj = json_get_object(obj, "dovi");
    err |= (subobj == NULL);
    if (!err) {
        json_object_to_uint(subobj, "version_major",
                            &video->dovi.version_major, &err);
        json_object_to_uint(subobj, "version_minor",
                            &video->dovi.version_minor, &err);

        int tmp = 0;
        if (!json_object_to_integer(subobj, "profile", &tmp, &err, 0,
                                    (1 << 7) - 1)) {
            video->dovi.profile = tmp;
        }
        if (!json_object_to_integer(subobj, "level", &tmp, &err, 0,
                                    (1 << 6) - 1)) {
            video->dovi.level = tmp;
        }
        if (!json_object_to_integer(subobj, "rpu_present", &tmp, &err, 0, 1)) {
            video->dovi.rpu_present = tmp;
        }
        if (!json_object_to_integer(subobj, "el_present", &tmp, &err, 0, 1)) {
            video->dovi.el_present = tmp;
        }
        if (!json_object_to_integer(subobj, "bl_present", &tmp, &err, 0, 1)) {
            video->dovi.bl_present = tmp;
        }
    }

    json_object_to_uint(obj, "i_cubemap_padding", &video->i_cubemap_padding,
                        &err);

    *error |= err;
}

static void fromJSON_subs_format(struct serdes_sys *sys,
                                 const struct json_object *obj,
                                 subs_format_t *sub, bool *error)
{
    assert(obj != NULL);
    assert(sub != NULL);
    assert(error != NULL);

    VLC_UNUSED(sys);

    bool err = false;

    json_object_to_string(obj, "psz_encoding", &sub->psz_encoding, &err);
    json_object_to_int(obj, "i_x_origin", &sub->i_x_origin, &err);
    json_object_to_int(obj, "i_y_origin", &sub->i_y_origin, &err);

    const struct json_object *subobj = json_get_object(obj, "spu");
    err |= (subobj == NULL);
    if (!err) {
        json_object_to_uint(subobj, "i_original_frame_width",
                            &sub->spu.i_original_frame_width, &err);
        json_object_to_uint(subobj, "i_original_frame_height",
                            &sub->spu.i_original_frame_height, &err);
        json_array_integer_load(subobj, "palette", err, sub->spu.palette,
                                VIDEO_PALETTE_CLUT_COUNT, 0, UINT32_MAX);
        json_object_to_boolean(subobj, "b_palette", &sub->spu.b_palette, &err);
    }

    subobj = json_get_object(obj, "dvb");
    err |= (subobj == NULL);
    if (!err) {
        json_object_to_int(subobj, "i_id", &sub->dvb.i_id, &err);
    }

    subobj = json_get_object(obj, "teletext");
    err |= (subobj == NULL);
    if (!err) {
        json_object_to_uint(subobj, "i_magazine", &sub->teletext.i_magazine,
                            &err);
        json_object_to_uint(subobj, "i_page", &sub->teletext.i_page, &err);
    }

    subobj = json_get_object(obj, "cc");
    err |= (subobj == NULL);
    if (!err) {
        json_object_to_uint(subobj, "i_channel", &sub->cc.i_channel, &err);
        json_object_to_int(subobj, "i_reorder_depth", &sub->cc.i_reorder_depth,
                           &err);
    }

    *error |= err;
}

static void fromJSON_es_format(struct serdes_sys *sys,
                               const struct json_object *obj, es_format_t *es,
                               bool *error)
{
    assert(obj != NULL);
    assert(es != NULL);
    assert(error != NULL);

    bool err = false;

    json_object_to_enum(obj, "i_cat", &es->i_cat, &err, UNKNOWN_ES, DATA_ES);

    char *fourcc = NULL;
    json_object_to_string(obj, "i_codec", &fourcc, &err);
    es->i_codec = fourcc != NULL ? vlc_fourcc_from_char(fourcc) : 0;
    free(fourcc);
    fourcc = NULL;
    json_object_to_string(obj, "i_original_fourcc", &fourcc, &err);
    es->i_original_fourcc = fourcc != NULL ? vlc_fourcc_from_char(fourcc) : 0;
    free(fourcc);

    json_object_to_integer(obj, "i_id", &es->i_id, &err, -1, INT_MAX);
    json_object_to_integer(obj, "i_group", &es->i_group, &err, -1, INT_MAX);
    json_object_to_integer(obj, "i_priority", &es->i_priority, &err, -2,
                           INT_MAX);
    json_object_to_string(obj, "psz_language", &es->psz_language, &err);
    json_object_to_string(obj, "psz_description", &es->psz_description, &err);

    unsigned int elvec_size = 0;
    json_object_to_uint(obj, "i_extra_languages", &elvec_size, &err);
    struct VLC_VECTOR(extra_languages_t) el_vec = VLC_VECTOR_INITIALIZER;
    vlc_vector_init(&el_vec);

    struct json_value *v;
    json_array_foreach_ref(obj, "p_extra_languages", v, &err) {
        if (v->type != JSON_OBJECT) {
            continue;
        }
        const struct json_object *subobj = &v->object;
        extra_languages_t el = {NULL, NULL};
        json_object_to_string(subobj, "psz_language", &el.psz_language, &err);
        json_object_to_string(subobj, "psz_description", &el.psz_description,
                              &err);
        vlc_vector_push(&el_vec, el);
    }
    if (elvec_size == el_vec.size) {
        es->p_extra_languages = el_vec.data;
        es->i_extra_languages = el_vec.size;
        vlc_vector_init(&el_vec);
    } else {
        vlc_vector_clear(&el_vec);
        err = true;
    }

    switch (es->i_cat) {
        case AUDIO_ES:
            json_object_from_name(sys, obj, "audio", &es->audio, err, false,
                                  fromJSON_audio_format);
            json_object_from_name(sys, obj, "audio_replay_gain",
                                  &es->audio_replay_gain, err, false,
                                  fromJSON_audio_replay_gain);
            break;
        case VIDEO_ES:
            json_object_from_name(sys, obj, "video", &es->video, err, false,
                                  fromJSON_video_format);
            break;
        case SPU_ES:
            json_object_from_name(sys, obj, "subs", &es->subs, err, false,
                                  fromJSON_subs_format);
            break;
        default:
            break;
    }

    json_object_to_uint(obj, "i_bitrate", &es->i_bitrate, &err);
    json_object_to_int(obj, "i_profile", &es->i_profile, &err);
    json_object_to_int(obj, "i_level", &es->i_level, &err);
    json_object_to_boolean(obj, "b_packetized", &es->b_packetized, &err);

    *error |= err;
}

static void fromJSON_input_item_es(struct serdes_sys *sys,
                                   const struct json_object *obj,
                                   struct input_item_es *item_es, bool *error)
{
    assert(obj != NULL);
    assert(item_es != NULL);
    assert(error != NULL);

    bool err = false;

    json_object_to_string(obj, "id", &item_es->id, &err);
    json_object_to_boolean(obj, "id_stable", &item_es->id_stable, &err);
    json_object_from_name(sys, obj, "es", &item_es->es, err, false,
                          fromJSON_es_format);

    *error |= err;
}

static void fromJSON_meta(struct serdes_sys *sys,
                          const struct json_object *obj, vlc_meta_t *meta,
                          bool *error)
{
    assert(obj != NULL);
    assert(meta != NULL);
    assert(error != NULL);

    VLC_UNUSED(sys);

    bool err = false;

    /* Used to get value and priority of a meta value */
#define meta_from_object(obj, meta, type, err) \
    do { \
        const struct json_object *o_##type = json_get_object(obj, #type); \
        if (o_##type == NULL) { \
            err = true; \
            break; \
        } \
        vlc_meta_priority_t p_##type = VLC_META_PRIORITY_BASIC;\
        json_object_to_enum(o_##type, "priority", &p_##type, &err,\
                            VLC_META_PRIORITY_BASIC,\
                            VLC_META_PRIORITY_INBAND);\
        char *v_##type = NULL;\
        json_object_to_string(o_##type, "value", &v_##type, &err); \
        if (err) { \
            free(v_##type); \
            err = true; \
            break; \
        } \
        vlc_meta_SetWithPriority(meta, vlc_meta_##type, v_##type, p_##type); \
        free(v_##type); \
    } while (0)

    meta_from_object(obj, meta, Title, err);
    meta_from_object(obj, meta, Artist, err);
    meta_from_object(obj, meta, AlbumArtist, err);
    meta_from_object(obj, meta, Genre, err);
    meta_from_object(obj, meta, Copyright, err);
    meta_from_object(obj, meta, Album, err);
    meta_from_object(obj, meta, TrackNumber, err);
    meta_from_object(obj, meta, Description, err);
    meta_from_object(obj, meta, Rating, err);
    meta_from_object(obj, meta, Date, err);
    meta_from_object(obj, meta, Setting, err);
    meta_from_object(obj, meta, URL, err);
    meta_from_object(obj, meta, Language, err);
    meta_from_object(obj, meta, NowPlaying, err);
    meta_from_object(obj, meta, ESNowPlaying, err);
    meta_from_object(obj, meta, Publisher, err);
    meta_from_object(obj, meta, EncodedBy, err);
    meta_from_object(obj, meta, ArtworkURL, err);
    meta_from_object(obj, meta, TrackID, err);
    meta_from_object(obj, meta, TrackTotal, err);
    meta_from_object(obj, meta, Director, err);
    meta_from_object(obj, meta, Season, err);
    meta_from_object(obj, meta, Episode, err);
    meta_from_object(obj, meta, ShowName, err);
    meta_from_object(obj, meta, Actors, err);
    meta_from_object(obj, meta, DiscNumber, err);
    meta_from_object(obj, meta, DiscTotal, err);

#undef meta_from_object

    struct json_value *v;
    json_array_foreach_ref(obj, "extra_tags", v, &err) {
        if (v->type != JSON_OBJECT) {
            continue;
        }
        char *key = NULL;
        json_object_to_string(&v->object, "key", &key, &err);
        char *value = NULL;
        json_object_to_string(&v->object, "value", &value, &err);
        vlc_meta_priority_t priority = VLC_META_PRIORITY_BASIC;
        json_object_to_enum(&v->object, "priority", &priority, &err,
                            VLC_META_PRIORITY_BASIC, VLC_META_PRIORITY_INBAND);
        if (err == false) {
            vlc_meta_SetExtraWithPriority(meta, key, value, priority);
        }
        free(key);
        free(value);
    }

    *error |= err;
}

static void fromJSON_input_item_slave(struct serdes_sys *sys,
                                      const struct json_object *obj,
                                      struct input_item_slave **slave,
                                      bool *error)
{
    assert(obj != NULL);
    assert(slave != NULL);
    assert(error != NULL);

    VLC_UNUSED(sys);

    bool err = false;

    char *uri = NULL;
    json_object_to_string(obj, "psz_uri", &uri, &err);
    enum slave_type i_type = SLAVE_TYPE_SPU;
    json_object_to_enum(obj, "i_type", &i_type, &err, SLAVE_TYPE_SPU,
                        SLAVE_TYPE_GENERIC);
    enum slave_priority i_priority = SLAVE_PRIORITY_MATCH_NONE;
    json_object_to_enum(obj, "i_priority", &i_priority, &err,
                        SLAVE_PRIORITY_MATCH_NONE, SLAVE_PRIORITY_USER);
    bool b_forced = false;
    json_object_to_boolean(obj, "b_forced", &b_forced, &err);

    if (!err) {
        *slave = input_item_slave_New(uri, i_type, i_priority);
        err |= *slave == NULL;
        if (!err) {
            (*slave)->b_forced = b_forced;
        }
    }
    free(uri);

    *error |= err;
}

static void fromJSON_input_item(struct serdes_sys *sys,
                                const struct json_object *obj,
                                input_item_t **item, bool *error)
{
    assert(obj != NULL);
    assert(item != NULL);
    assert(error != NULL);

    bool err = false;

    input_item_t *i = input_item_New(NULL, NULL);
    if (i == NULL) {
        *error = true;
        return;
    }

    json_object_to_string(obj, "psz_name", &i->psz_name, &err);
    json_object_to_string(obj, "psz_uri", &i->psz_uri, &err);
    json_object_to_int(obj, "i_duration", &i->i_duration, &err);

    /* Iterate over the json es_vec array, fill an input_item_es struct and
     * push it to the es_vec vector. */
    vlc_vector_clear(&i->es_vec);
    struct json_value *v;
    json_array_foreach_ref(obj, "es_vec", v, &err) {
        if (v->type != JSON_OBJECT) {
            continue;
        }
        struct input_item_es item_es = {0};
        fromJSON_input_item_es(sys, &v->object, &item_es, &err);
        if (!err) {
            vlc_vector_push(&i->es_vec, item_es);
        }
    }

    /* Create a new meta object and fill it with p_meta json object. */
    if (i->p_meta == NULL) {
        i->p_meta = vlc_meta_New();
    }
    err |= i->p_meta == NULL;
    if (!err) {
        json_object_from_name(sys, obj, "p_meta", i->p_meta, err, false,
                              fromJSON_meta);
    }

    json_array_foreach_ref(obj, "pp_slaves", v, &err) {
        if (v->type != JSON_OBJECT) {
            continue;
        }
        struct input_item_slave *slave = NULL;
        fromJSON_input_item_slave(sys, &v->object, &slave, &err);
        if (!err) {
            input_item_AddSlave(i, slave);
        }
    }
    int check = 0;
    json_object_to_int(obj, "i_slaves", &check, &err);
    err |= check != i->i_slaves;

    json_object_to_enum(obj, "i_type", &i->i_type, &err, ITEM_TYPE_UNKNOWN,
                        ITEM_TYPE_NUMBER - 1);
    json_object_to_boolean(obj, "b_net", &i->b_net, &err);

    *error |= err;
    if (!err) {
        *item = i;
        return;
    }
    input_item_Release(i);
}

static void
fromJSON_input_attachment(struct serdes_sys *sys,
                          const struct json_object *obj,
                          input_attachment_t **a, bool *error)
{
    assert(obj != NULL);
    assert(a != NULL);
    assert(error != NULL);

    bool err = false;

    char *name = NULL;
    json_object_to_string(obj, "psz_name", &name, &err);
    char *mime = NULL;
    json_object_to_string(obj, "psz_mime", &mime, &err);
    char *description = NULL;
    json_object_to_string(obj, "psz_description", &description, &err);
    size_t i_data = 0;
    json_object_to_uint(obj, "i_data", &i_data, &err);
    char *p_data = NULL;
    json_object_to_string(obj, "p_data", &p_data, &err);
    if (p_data != NULL) {
        err = true;
    }

    if (!err) {
        *a = vlc_input_attachment_New(name, mime, description, NULL, 0);
        if (*a == NULL) {
            err = true;
        }
    }
    free(name);
    free(mime);
    free(description);
    free(p_data);

    if (err) {
        *error |= err;
        return;
    }

    void *data = malloc(i_data);
    if (data == NULL) {
        *error = true;
        return;
    }
    ssize_t ret = 0;
    ret = serdes_buf_read(sys, data, i_data, false);
    if ((size_t)ret == i_data) {
        (*a)->i_data = i_data;
        free((*a)->p_data);
        (*a)->p_data = data;
        return;
    }
    if (ret < 0) {
        sys->error = ret;
    }
    *error = true;
    free(data);
    vlc_input_attachment_Release(*a);
}

static void
fromJSON_input_item_node(struct serdes_sys *sys,
                         const struct json_object *obj,
                         input_item_node_t **node, bool *error)
{
    assert(obj != NULL);
    assert(node != NULL);
    assert(error != NULL);

    bool err = false;
    input_item_node_t *nd = NULL;
    input_item_t *item = NULL;

    json_object_from_name(sys, obj, "p_item", &item, err, true,
                          fromJSON_input_item);
    if (err) {
        *error = true;
        return;
    }

    nd = input_item_node_Create(item);
    input_item_Release(item);
    if (nd == NULL) {
        *error = true;
        return;
    }
    int children = 0;
    json_object_to_int(obj, "i_children", &children, &err);

    const struct json_value *v = NULL;
    json_array_foreach_ref(obj, "pp_children", v, &err) {
        if (v->type != JSON_OBJECT) {
            continue;
        }
        input_item_node_t *n = NULL;
        fromJSON_input_item_node(sys, &v->object, &n, &err);
        if (err) {
            break;
        }
        input_item_node_AppendNode(nd, n);
    }
    if (children != nd->i_children) {
        err = true;
    }

    *error |= err;
    if (!err) {
        *node = nd;
        return;
    }
    input_item_node_Delete(nd);
}

static void
fromJSON_plane(struct serdes_sys *sys, const struct json_object *obj,
               plane_t *p, bool *error)
{
    assert(obj != NULL);
    assert(p != NULL);
    assert(error != NULL);

    bool err = false;

    int lines = 0;
    json_object_to_int(obj, "i_lines", &lines, &err);
    err |= lines != p->i_lines;
    int pitch = 0;
    json_object_to_int(obj, "i_pitch", &pitch, &err);
    err |= pitch != p->i_pitch;
    int pixel_pitch = 0;
    json_object_to_int(obj, "i_pixel_pitch", &pixel_pitch, &err);
    err |= pixel_pitch != p->i_pixel_pitch;
    int visible_lines = 0;
    json_object_to_int(obj, "i_visible_lines", &visible_lines, &err);
    err |= visible_lines != p->i_visible_lines;
    int visible_pitch = 0;
    json_object_to_int(obj, "i_visible_pitch", &visible_pitch, &err);
    err |= visible_pitch != p->i_visible_pitch;

    if (err) {
        *error |= err;
        return;
    }

    size_t size = lines * pitch;
    void *data = malloc(size);
    if (data == NULL) {
        *error = true;
        return;
    }
    ssize_t ret = 0;
    ret = serdes_buf_read(sys, data, size, false);
    if (ret < 0) {
        sys->error = ret;
        *error = true;
    } else if ((size_t)ret != size) {
        *error = true;
    }

    plane_t pln;
    pln.i_lines = p->i_lines;
    pln.i_pitch = p->i_pitch;
    pln.i_pixel_pitch = p->i_pixel_pitch;
    pln.i_visible_lines = p->i_visible_lines;
    pln.i_visible_pitch = p->i_visible_pitch;
    pln.p_pixels = data;
    plane_CopyPixels(p, &pln);
    free(data);
}

static void
fromJSON_picture(struct serdes_sys *sys, const struct json_object *obj,
                 picture_t **p, bool *error)
{
    assert(obj != NULL);
    assert(p != NULL);
    assert(error != NULL);

    bool err = false;

    video_format_t f;
    json_object_from_name(sys, obj, "format", &f, err, false,
                          fromJSON_video_format);
    if (err) {
        *error = true;
        return;
    }

    picture_t *pic = picture_NewFromFormat(&f);
    if (pic == NULL) {
        video_format_Clean(&f);
        *error = true;
        return;
    }

    int i = 0;
    const struct json_value *v = NULL;
    json_array_foreach_ref(obj, "p", v, &err) {
        if (v->type != JSON_OBJECT) {
            continue;
        }
        if (i >= pic->i_planes) {
            err = true;
            break;
        }

        fromJSON_plane(sys, &v->object, &pic->p[i], &err);
        if (err) {
            break;
        }
        i++;
    }
    json_object_to_int(obj, "date", &pic->date, &err);
    json_object_to_boolean(obj, "b_force", &pic->b_force, &err);
    json_object_to_boolean(obj, "b_still", &pic->b_still, &err);
    json_object_to_boolean(obj, "b_progressive", &pic->b_progressive, &err);
    json_object_to_boolean(obj, "b_top_field_first", &pic->b_top_field_first,
                           &err);
    json_object_to_boolean(obj, "b_multiview_left_eye",
                           &pic->b_multiview_left_eye, &err);
    unsigned int nb_fields = 0;
    json_object_to_uint(obj, "i_nb_fields", &nb_fields, &err);
    err |= nb_fields != pic->i_nb_fields;

    if (!err) {
        *p = pic;
        return;
    }
    *error |= err;
    picture_Release(pic);
}

static void
fromJSON_vlc_thumbnailer_output(struct serdes_sys *sys,
                                const struct json_object *obj,
                                struct vlc_thumbnailer_output *out,
                                bool *error)
{
    assert(obj != NULL);
    assert(out != NULL);
    assert(error != NULL);

    VLC_UNUSED(sys);

    bool err = false;


    json_object_to_enum(obj, "format", &out->format, &err,
                        VLC_THUMBNAILER_FORMAT_PNG,
                        VLC_THUMBNAILER_FORMAT_JPEG);
    json_object_to_int(obj, "width", &out->width, &err);
    json_object_to_int(obj, "height", &out->height, &err);
    json_object_to_boolean(obj, "crop", &out->crop, &err);
    json_object_to_uint(obj, "creat_mode", &out->creat_mode, &err);

    *error |= err;
}

static void
fromJSON_vlc_preparser_msg_req(struct serdes_sys *sys,
                               const struct json_object *obj,
                               struct vlc_preparser_msg_req *req, bool *error)
{
    assert(obj != NULL);
    assert(req != NULL);
    assert(error != NULL);

    bool err = false;

    const struct json_value *v = NULL;

    json_object_to_enum(obj, "type", &req->type, &err,
                        VLC_PREPARSER_MSG_REQ_TYPE_PARSE,
                        VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL_TO_FILES);
    if (err) {
        *error = true;
        return;
    }
    if (req->type == VLC_PREPARSER_MSG_REQ_TYPE_PARSE) {
        json_object_to_int(obj, "options", &req->options, &err);
    } else {
        if (req->type == VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL_TO_FILES) {
            json_array_foreach_ref(obj, "outputs", v, &err) {
                if (v->type != JSON_OBJECT) {
                    err = true;
                    break;
                }
                char *ptr = NULL;
                json_object_to_string(&v->object, "file_path", &ptr, &err);
                if (!err) {
                    vlc_vector_push(&req->outputs_path, ptr);
                }
            }
            json_array_foreach_ref(obj, "outputs", v, &err) {
                if (v->type != JSON_OBJECT) {
                    err = true;
                    break;
                }
                struct vlc_thumbnailer_output out;
                fromJSON_vlc_thumbnailer_output(sys, &v->object, &out, &err);
                size_t i = req->outputs.size;
                err |= i >= req->outputs_path.size;
                if (!err) {
                    out.file_path = req->outputs_path.data[i];
                    vlc_vector_push(&req->outputs, out);
                }
            }
        }
        json_object_to_enum(obj, "seek.type", &req->arg.seek.type, &err,
                            VLC_THUMBNAILER_SEEK_NONE,
                            VLC_THUMBNAILER_SEEK_POS);
        if (req->arg.seek.type == VLC_THUMBNAILER_SEEK_TIME) {
            json_object_to_int(obj, "seek.time", &req->arg.seek.time,
                                 &err);
        } else {
            json_object_to_double(obj, "seek.pos", &req->arg.seek.pos,
                                  &err);
        }
        json_object_to_enum(obj, "seek.speed", &req->arg.seek.speed,
                            &err, VLC_THUMBNAILER_SEEK_PRECISE,
                            VLC_THUMBNAILER_SEEK_FAST);
        json_object_to_boolean(obj, "hw_dec", &req->arg.hw_dec, &err);
    }
    json_object_to_string(obj, "uri", &req->uri, &err);

    *error |= err;
}

static void
fromJSON_vlc_preparser_msg_res(struct serdes_sys *sys,
                               const struct json_object *obj,
                               struct vlc_preparser_msg_res *res, bool *error)
{
    assert(obj != NULL);
    assert(res != NULL);
    assert(error != NULL);

    bool err = false;

    const struct json_value *v = NULL;

    json_object_to_enum(obj, "type", &res->type, &err,
                        VLC_PREPARSER_MSG_REQ_TYPE_PARSE,
                        VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL_TO_FILES);
    switch (res->type) {
        case VLC_PREPARSER_MSG_REQ_TYPE_PARSE:
            json_array_foreach_ref(obj, "attachments", v, &err) {
                if (v->type != JSON_OBJECT) {
                    err = true;
                    break;
                }
                input_attachment_t *a;
                fromJSON_input_attachment(sys, &v->object, &a, &err);
                if (!err) {
                    vlc_vector_push(&res->attachments, a);
                }
            }
            json_object_from_name(sys, obj, "subtree", &res->subtree, err,
                                  true, fromJSON_input_item_node);
            break;
        case VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL:
            json_object_from_name(sys, obj, "pic", &res->pic, err, true,
                                  fromJSON_picture);
            break;
        case VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL_TO_FILES:
            json_array_foreach_ref(obj, "result", v, &err) {
                if (v->type != JSON_BOOLEAN) {
                    err = true;
                    break;
                }
                vlc_vector_push(&res->result, v->boolean);
            }
            break;
        default:
            vlc_assert_unreachable();
    }
    json_object_to_int(obj, "status", &res->status, &err);

    json_object_from_name(sys, obj, "item", &res->item, err, true,
                          fromJSON_input_item);


    *error |= err;
}

bool
fromJSON_vlc_preparser_msg(struct serdes_sys *sys,
                           struct vlc_preparser_msg *msg,
                           const struct json_object *obj)
{
    assert(sys != NULL);
    assert(msg != NULL);
    assert(obj != NULL);

    bool err = false;

    int msg_type = 0;
    json_object_to_enum(obj, "type", &msg_type, &err,
                        VLC_PREPARSER_MSG_TYPE_REQ,
                        VLC_PREPARSER_MSG_TYPE_RES);

    int req_type = 0;
    json_object_to_enum(obj, "req_type", &req_type, &err,
                        VLC_PREPARSER_MSG_REQ_TYPE_PARSE,
                        VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL_TO_FILES);

    vlc_preparser_msg_Init(msg, msg_type, req_type);

    if (msg_type == VLC_PREPARSER_MSG_TYPE_REQ) {
        json_object_from_name(sys, obj, "req", &msg->req, err, false,
                              fromJSON_vlc_preparser_msg_req);
    } else {
        json_object_from_name(sys, obj, "res", &msg->res, err, false,
                              fromJSON_vlc_preparser_msg_res);
    }

    if (err) {
        vlc_preparser_msg_Clean(msg);
    }
    return err;
}

/* Undefine macros. */
#undef json_array_double_load
#undef json_array_integer_load
#undef json_array_boolean_load
#undef json_array_foreach_ref
#undef json_object_to_float
#undef json_object_to_double
#undef json_object_to_int
#undef json_object_to_uint
#undef json_object_to_enum
