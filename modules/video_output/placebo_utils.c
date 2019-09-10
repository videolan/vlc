/*****************************************************************************
 * placebo.c: Definition of various libplacebo helpers
 *****************************************************************************
 * Copyright (C) 2018 Niklas Haas
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
# include <config.h>
#endif

#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>

#include <vlc_common.h>
#include "placebo_utils.h"

static void Log(void *priv, enum pl_log_level level, const char *msg)
{
    struct vlc_object_t *obj = priv;

    switch (level) {
    case PL_LOG_FATAL: // fall through
    case PL_LOG_ERR:   msg_Err(obj,  "%s", msg); break;
    case PL_LOG_WARN:  msg_Warn(obj, "%s", msg); break;
    case PL_LOG_INFO:  msg_Info(obj, "%s", msg); break;
    case PL_LOG_DEBUG: msg_Dbg(obj,  "%s", msg); break;
    default: break;
    }
}

struct pl_context *vlc_placebo_Create(vlc_object_t *obj)
{
    return pl_context_create(PL_API_VER, &(struct pl_context_params) {
        .log_level = PL_LOG_DEBUG,
        .log_cb    = Log,
        .log_priv  = obj,
    });
}

struct plane_desc {
    int components;
    size_t pixel_size;
    int comp_bits[4];
    int comp_map[4];
    int w_denom;
    int h_denom;
};

struct fmt_desc {
    enum pl_fmt_type type;
    struct plane_desc planes[4];
    int num_planes;
    int color_bits; // relevant bits, or 0 if the same as the texture depth
};

#define SIZE(n, bits, pad) (((n) * (bits) + (pad) + 7) / 8)
#define COMPS(...) {__VA_ARGS__}

#define PLANE(n, bits, map, wd, hd, pad)            \
  { .components = n,                                \
    .pixel_size = SIZE(n, bits, pad),               \
    .comp_bits = {bits, bits, bits, bits},          \
    .comp_map = map,                                \
    .w_denom = wd,                                  \
    .h_denom = hd,                                  \
  }

#define _PLANAR(n, bits, wd, hd)          \
  .type = PL_FMT_UNORM,                   \
  .num_planes = n,                        \
  .planes = {                             \
      PLANE(1, bits, {0},  1,  1, 0),     \
      PLANE(1, bits, {1}, wd, hd, 0),     \
      PLANE(1, bits, {2}, wd, hd, 0),     \
      PLANE(1, bits, {3},  1,  1, 0),     \
  }

#define _SEMIPLANAR(n, bits, wd, hd)           \
  .type = PL_FMT_UNORM,                        \
  .num_planes = n,                             \
  .planes = {                                  \
      PLANE(1, bits, {0},          1,  1, 0),  \
      PLANE(2, bits, COMPS(1, 2), wd, hd, 0),  \
      PLANE(1, bits, {3},          1,  1, 0),  \
  }

#define PACKED(n, bits, pad)                          \
  .type = PL_FMT_UNORM,                               \
  .num_planes = 1,                                    \
  .planes = {                                         \
      PLANE(n, bits, COMPS(0, 1, 2, 3), 1, 1, pad),   \
  }

#define SINGLE(t, bits)                   \
  .type = PL_FMT_##t,                     \
  .num_planes = 1,                        \
  .planes = {                             \
      PLANE(1, bits, {0}, 1, 1, 0),       \
  }

#define PLANAR(...) _PLANAR(__VA_ARGS__)
#define SEMIPLANAR(...) _SEMIPLANAR(__VA_ARGS__)

#define _410 4, 2
#define _411 4, 1
#define _420 2, 2
#define _422 2, 1
#define _440 1, 2
#define _444 1, 1

// NOTE: This list contains some special formats that don't follow the normal
// rules, but which are included regardless. The corrections for these
// exceptions happen below, in the function vlc_placebo_PlaneFormat!
static const struct { vlc_fourcc_t fcc; struct fmt_desc desc; } formats[] = {
    { VLC_CODEC_YV9,            {PLANAR(3,  8, _410)} },
    { VLC_CODEC_I410,           {PLANAR(3,  8, _410)} },
    { VLC_CODEC_I411,           {PLANAR(3,  8, _411)} },
    { VLC_CODEC_I440,           {PLANAR(3,  8, _440)} },
    { VLC_CODEC_J440,           {PLANAR(3,  8, _440)} },
    { VLC_CODEC_GREY,           {PLANAR(1,  8, _444)} },

    { VLC_CODEC_I420,           {PLANAR(3,  8, _420)} },
    { VLC_CODEC_J420,           {PLANAR(3,  8, _420)} },
#ifdef WORDS_BIGENDIAN
    { VLC_CODEC_I420_9B,        {PLANAR(3, 16, _420), .color_bits = 9} },
    { VLC_CODEC_I420_10B,       {PLANAR(3, 16, _420), .color_bits = 10} },
    { VLC_CODEC_I420_12B,       {PLANAR(3, 16, _420), .color_bits = 12} },
    { VLC_CODEC_I420_16B,       {PLANAR(3, 16, _420), .color_bits = 16} },
#else
    { VLC_CODEC_I420_9L,        {PLANAR(3, 16, _420), .color_bits = 9} },
    { VLC_CODEC_I420_10L,       {PLANAR(3, 16, _420), .color_bits = 10} },
    { VLC_CODEC_I420_12L,       {PLANAR(3, 16, _420), .color_bits = 12} },
    { VLC_CODEC_I420_16L,       {PLANAR(3, 16, _420), .color_bits = 16} },
#endif

    { VLC_CODEC_I422,           {PLANAR(3,  8, _422)} },
    { VLC_CODEC_J422,           {PLANAR(3,  8, _422)} },
#ifdef WORDS_BIGENDIAN
    { VLC_CODEC_I422_9B,        {PLANAR(3, 16, _422), .color_bits = 9} },
    { VLC_CODEC_I422_10B,       {PLANAR(3, 16, _422), .color_bits = 10} },
    { VLC_CODEC_I422_12B,       {PLANAR(3, 16, _422), .color_bits = 12} },
#else
    { VLC_CODEC_I422_9L,        {PLANAR(3, 16, _422), .color_bits = 9} },
    { VLC_CODEC_I422_10L,       {PLANAR(3, 16, _422), .color_bits = 10} },
    { VLC_CODEC_I422_12L,       {PLANAR(3, 16, _422), .color_bits = 12} },
#endif

    { VLC_CODEC_I444,           {PLANAR(3,  8, _444)} },
    { VLC_CODEC_J444,           {PLANAR(3,  8, _444)} },
#ifdef WORDS_BIGENDIAN
    { VLC_CODEC_I444_9B,        {PLANAR(3, 16, _444), .color_bits = 9} },
    { VLC_CODEC_I444_10B,       {PLANAR(3, 16, _444), .color_bits = 10} },
    { VLC_CODEC_I444_12B,       {PLANAR(3, 16, _444), .color_bits = 12} },
    { VLC_CODEC_I444_16B,       {PLANAR(3, 16, _444), .color_bits = 16} },
#else
    { VLC_CODEC_I444_9L,        {PLANAR(3, 16, _444), .color_bits = 9} },
    { VLC_CODEC_I444_10L,       {PLANAR(3, 16, _444), .color_bits = 10} },
    { VLC_CODEC_I444_12L,       {PLANAR(3, 16, _444), .color_bits = 12} },
    { VLC_CODEC_I444_16L,       {PLANAR(3, 16, _444), .color_bits = 16} },
#endif

    { VLC_CODEC_YUVA,           {PLANAR(4,  8, _444)} },
    { VLC_CODEC_YUV422A,        {PLANAR(4,  8, _422)} },
#ifdef WORDS_BIGENDIAN
    { VLC_CODEC_YUVA_444_10B,   {PLANAR(4, 16, _444), .color_bits = 10} },
#else
    { VLC_CODEC_YUVA_444_10L,   {PLANAR(4, 16, _444), .color_bits = 10} },
#endif

    { VLC_CODEC_NV12,           {SEMIPLANAR(2,  8, _420)} },
    { VLC_CODEC_NV21,           {SEMIPLANAR(2,  8, _420)} },
    { VLC_CODEC_P010,           {SEMIPLANAR(2, 16, _420)} },
    { VLC_CODEC_P016,           {SEMIPLANAR(2, 16, _420)} },
    { VLC_CODEC_NV16,           {SEMIPLANAR(2,  8, _422)} },
    { VLC_CODEC_NV61,           {SEMIPLANAR(2,  8, _422)} },
    { VLC_CODEC_NV24,           {SEMIPLANAR(2,  8, _444)} },
    { VLC_CODEC_NV42,           {SEMIPLANAR(2,  8, _444)} },

    { VLC_CODEC_RGB8,           {PACKED(3, 2, 2)} },
    { VLC_CODEC_RGB12,          {PACKED(3, 4, 4)} },
    { VLC_CODEC_RGB15,          {PACKED(3, 5, 1)} },
    { VLC_CODEC_RGB16,          {PACKED(3, 5, 1)} },
    { VLC_CODEC_RGB24,          {PACKED(3, 8, 0)} },
    { VLC_CODEC_RGB32,          {PACKED(3, 8, 8)} },
    { VLC_CODEC_RGBA,           {PACKED(4, 8, 0)} },
    { VLC_CODEC_BGRA,           {PACKED(4, 8, 0)} },

    { VLC_CODEC_GBR_PLANAR,     {PLANAR(3,  8, _444)} },
#ifdef WORDS_BIGENDIAN
    { VLC_CODEC_GBR_PLANAR_9B,  {PLANAR(3, 16, _444), .color_bits = 9} },
    { VLC_CODEC_GBR_PLANAR_10B, {PLANAR(3, 16, _444), .color_bits = 10} },
    { VLC_CODEC_GBR_PLANAR_16B, {PLANAR(3, 16, _444), .color_bits = 16} },
#else
    { VLC_CODEC_GBR_PLANAR_9L,  {PLANAR(3, 16, _444), .color_bits = 9} },
    { VLC_CODEC_GBR_PLANAR_10L, {PLANAR(3, 16, _444), .color_bits = 10} },
    { VLC_CODEC_GBR_PLANAR_16L, {PLANAR(3, 16, _444), .color_bits = 16} },
#endif

    { VLC_CODEC_U8,             {SINGLE(UNORM,  8)} },
    { VLC_CODEC_S8,             {SINGLE(SNORM,  8)} },
    { VLC_CODEC_U16N,           {SINGLE(UNORM, 16)} },
    { VLC_CODEC_S16N,           {SINGLE(SNORM, 16)} },
    { VLC_CODEC_U24N,           {SINGLE(UNORM, 24)} },
    { VLC_CODEC_S24N,           {SINGLE(SNORM, 24)} },
    { VLC_CODEC_U32N,           {SINGLE(UNORM, 32)} },
    { VLC_CODEC_S32N,           {SINGLE(SNORM, 32)} },
    { VLC_CODEC_FL32,           {SINGLE(FLOAT, 32)} },
    { VLC_CODEC_FL64,           {SINGLE(FLOAT, 64)} },

    { 0 },
};

static const struct fmt_desc *FindDesc(vlc_fourcc_t fcc)
{
    for (int i = 0; formats[i].fcc; i++) {
        if (formats[i].fcc == fcc) {
            return &formats[i].desc;
        }
    }

    return NULL;
}

// This fills everything except width/height, which are left as 1
static void FillDesc(vlc_fourcc_t fcc, const struct fmt_desc *desc,
                     struct pl_plane_data data[4])
{
    assert(desc->num_planes <= 4);
    for (int i = 0; i < desc->num_planes; i++) {
        const struct plane_desc *p = &desc->planes[i];

        data[i] = (struct pl_plane_data) {
            .type   = desc->type,
            .width  = 1,
            .height = 1,
            .pixel_stride = p->pixel_size,
        };

        for (int c = 0; c < p->components; c++) {
            data[i].component_size[c] = p->comp_bits[c];
            data[i].component_map[c] = p->comp_map[c];
        }
    }

    // Exceptions to the rule
    switch (fcc) {
    case VLC_CODEC_YV9:
    case VLC_CODEC_YV12:
        // Planar Y:V:U
        data[1].component_map[0] = 2;
        data[2].component_map[0] = 1;
        break;

    case VLC_CODEC_RGB32:
        // XRGB instead of RGBX
        data[0].component_map[0] = -1;
        data[1].component_map[0] = 0;
        data[2].component_map[0] = 1;
        data[3].component_map[0] = 2;
        break;

    case VLC_CODEC_BGRA:
        // Packed BGR
        data[0].component_map[0] = 2;
        data[0].component_map[1] = 1;
        data[0].component_map[2] = 0;
        break;

    case VLC_CODEC_GBR_PLANAR:
    case VLC_CODEC_GBR_PLANAR_9L:
    case VLC_CODEC_GBR_PLANAR_10L:
    case VLC_CODEC_GBR_PLANAR_16L:
    case VLC_CODEC_GBR_PLANAR_9B:
    case VLC_CODEC_GBR_PLANAR_10B:
    case VLC_CODEC_GBR_PLANAR_16B:
        // Planar GBR
        data[0].component_map[0] = 1;
        data[1].component_map[0] = 2;
        data[2].component_map[0] = 0;
        break;

    case VLC_CODEC_RGB16:
        // 5:6:5 instead of 5:5:5
        data[0].component_size[1] += 1;
        break;

    case VLC_CODEC_RGB8:
        // 3:3:2 instead of 2:2:2
        data[0].component_size[0] += 1;
        data[0].component_size[1] += 1;
        break;

    default: break;
    }
}

int vlc_placebo_PlaneFormat(const video_format_t *fmt, struct pl_plane_data data[4])
{
    const struct fmt_desc *desc = FindDesc(fmt->i_chroma);
    if (!desc)
        return 0;

    FillDesc(fmt->i_chroma, desc, data);
    for (int i = 0; i < desc->num_planes; i++) {
        const struct plane_desc *p = &desc->planes[i];
        data[i].width  = (fmt->i_visible_width  + p->w_denom - 1) / p->w_denom;
        data[i].height = (fmt->i_visible_height + p->h_denom - 1) / p->h_denom;
    }

    return desc->num_planes;
}

int vlc_placebo_PlaneData(const picture_t *pic, struct pl_plane_data data[4],
                          const struct pl_buf *buf)
{
    int planes = vlc_placebo_PlaneFormat(&pic->format, data);
    if (!planes)
        return 0;

    assert(planes == pic->i_planes);
    for (int i = 0; i < planes; i++) {
        assert(data[i].height == pic->p[i].i_visible_lines);
        data[i].row_stride = pic->p[i].i_pitch;
        if (buf) {
            assert(buf->data);
            assert(pic->p[i].p_pixels <= buf->data + buf->params.size);
            data[i].buf = buf;
            data[i].buf_offset = (uintptr_t) pic->p[i].p_pixels - (ptrdiff_t) buf->data;
        } else {
            data[i].pixels = pic->p[i].p_pixels;
        }
    }

    return planes;
}

bool vlc_placebo_FormatSupported(const struct pl_gpu *gpu, vlc_fourcc_t fcc)
{
    const struct fmt_desc *desc = FindDesc(fcc);
    if (!desc)
        return false;

    struct pl_plane_data data[4];
    FillDesc(fcc, desc, data);
    for (int i = 0; i < desc->num_planes; i++) {
        if (!pl_plane_find_fmt(gpu, NULL, &data[i]))
            return false;
    }

    return true;
}

struct pl_color_space vlc_placebo_ColorSpace(const video_format_t *fmt)
{
    static const enum pl_color_primaries primaries[COLOR_PRIMARIES_MAX+1] = {
        [COLOR_PRIMARIES_UNDEF]     = PL_COLOR_PRIM_UNKNOWN,
        [COLOR_PRIMARIES_BT601_525] = PL_COLOR_PRIM_BT_601_525,
        [COLOR_PRIMARIES_BT601_625] = PL_COLOR_PRIM_BT_601_625,
        [COLOR_PRIMARIES_BT709]     = PL_COLOR_PRIM_BT_709,
        [COLOR_PRIMARIES_BT2020]    = PL_COLOR_PRIM_BT_2020,
        [COLOR_PRIMARIES_DCI_P3]    = PL_COLOR_PRIM_DCI_P3,
        [COLOR_PRIMARIES_BT470_M]   = PL_COLOR_PRIM_BT_470M,
    };

    static const enum pl_color_transfer transfers[TRANSFER_FUNC_MAX+1] = {
        [TRANSFER_FUNC_UNDEF]        = PL_COLOR_TRC_UNKNOWN,
        [TRANSFER_FUNC_LINEAR]       = PL_COLOR_TRC_LINEAR,
        [TRANSFER_FUNC_SRGB]         = PL_COLOR_TRC_SRGB,
        [TRANSFER_FUNC_SMPTE_ST2084] = PL_COLOR_TRC_PQ,
        [TRANSFER_FUNC_HLG]          = PL_COLOR_TRC_HLG,
        // these are all designed to be displayed on BT.1886 displays, so this
        // is the correct way to handle them in libplacebo
        [TRANSFER_FUNC_BT470_BG]    = PL_COLOR_TRC_BT_1886,
        [TRANSFER_FUNC_BT470_M]     = PL_COLOR_TRC_BT_1886,
        [TRANSFER_FUNC_BT709]       = PL_COLOR_TRC_BT_1886,
        [TRANSFER_FUNC_SMPTE_240]   = PL_COLOR_TRC_BT_1886,
    };

    // Derive the signal peak/avg from the color light level metadata
    float sig_peak = fmt->lighting.MaxCLL / PL_COLOR_REF_WHITE;
    float sig_avg = fmt->lighting.MaxFALL / PL_COLOR_REF_WHITE;

    // As a fallback value for the signal peak, we can also use the mastering
    // metadata's luminance information
    if (!sig_peak)
        sig_peak = fmt->mastering.max_luminance / (10000.0 * PL_COLOR_REF_WHITE);

    // Sanitize the sig_peak/sig_avg, because of buggy or low quality tagging
    // that's sadly common in lots of typical sources
    sig_peak = (sig_peak > 1.0 && sig_peak <= 100.0) ? sig_peak : 0.0;
    sig_avg  = (sig_avg >= 0.0 && sig_avg <= 1.0) ? sig_avg : 0.0;

    return (struct pl_color_space) {
        .primaries = primaries[fmt->primaries],
        .transfer  = transfers[fmt->transfer],
        .light     = PL_COLOR_LIGHT_UNKNOWN,
        .sig_peak  = sig_peak,
        .sig_avg   = sig_avg,
    };
}

struct pl_color_repr vlc_placebo_ColorRepr(const video_format_t *fmt)
{
    static const enum pl_color_system yuv_systems[COLOR_SPACE_MAX+1] = {
        [COLOR_SPACE_UNDEF]     = PL_COLOR_SYSTEM_BT_709, // _UNKNOWN is RGB
        [COLOR_SPACE_BT601]     = PL_COLOR_SYSTEM_BT_601,
        [COLOR_SPACE_BT709]     = PL_COLOR_SYSTEM_BT_709,
        [COLOR_SPACE_BT2020]    = PL_COLOR_SYSTEM_BT_2020_NC,
    };

    // fmt->space describes the YCbCr type only, it does not distinguish
    // between YUV, XYZ, RGB and the likes!
    enum pl_color_system sys;
    if (likely(vlc_fourcc_IsYUV(fmt->i_chroma))) {
        sys = yuv_systems[fmt->space];
    } else if (unlikely(fmt->i_chroma == VLC_CODEC_XYZ12)) {
        sys = PL_COLOR_SYSTEM_XYZ;
    } else {
        sys = PL_COLOR_SYSTEM_RGB;
    }

    const struct fmt_desc *desc = FindDesc(fmt->i_chroma);
    int sample_depth = desc->planes[0].comp_bits[0]; // just use first component

    return (struct pl_color_repr) {
        .sys        = sys,
        .alpha      = PL_ALPHA_PREMULTIPLIED,
        .levels     = unlikely(fmt->color_range == COLOR_RANGE_FULL)
                        ? PL_COLOR_LEVELS_PC
                        : PL_COLOR_LEVELS_TV,
        .bits = {
            .sample_depth   = sample_depth,
            .color_depth    = desc->color_bits ? desc->color_bits : sample_depth,
            .bit_shift      = 0,
        },
    };
}

enum pl_chroma_location vlc_placebo_ChromaLoc(const video_format_t *fmt)
{
    static const enum pl_chroma_location locs[CHROMA_LOCATION_MAX+1] = {
        [CHROMA_LOCATION_UNDEF]         = PL_CHROMA_UNKNOWN,
        [CHROMA_LOCATION_LEFT]          = PL_CHROMA_LEFT,
        [CHROMA_LOCATION_CENTER]        = PL_CHROMA_CENTER,
        [CHROMA_LOCATION_TOP_LEFT]      = PL_CHROMA_TOP_LEFT,
        [CHROMA_LOCATION_TOP_CENTER]    = PL_CHROMA_TOP_CENTER,
        [CHROMA_LOCATION_BOTTOM_LEFT]   = PL_CHROMA_BOTTOM_LEFT,
        [CHROMA_LOCATION_BOTTOM_CENTER] = PL_CHROMA_BOTTOM_CENTER,
    };

    return locs[fmt->chroma_location];
}
