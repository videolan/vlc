/*****************************************************************************
 * placebo_utils.h: Definition of various libplacebo helpers
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

#ifndef VLC_PLACEBO_UTILS_H
#define VLC_PLACEBO_UTILS_H 1

#include <vlc_common.h>
#include <vlc_picture.h>

#include <libplacebo/colorspace.h>
#include <libplacebo/shaders/colorspace.h>

// Create a libplacebo context, hooked up to the log system; or NULL on OOM
VLC_API struct pl_context *vlc_placebo_Create(vlc_object_t *);

// Turn a video_format_t into the equivalent libplacebo values
VLC_API struct pl_color_space vlc_placebo_ColorSpace(const video_format_t *);

// Shared options strings/structs for libplacebo options

#define RENDER_INTENT_TEXT "Rendering intent for color conversion"
#define RENDER_INTENT_LONGTEXT "The mapping type used to convert between color spaces."

static const int intent_values[] = {
    PL_INTENT_PERCEPTUAL,
    PL_INTENT_RELATIVE_COLORIMETRIC,
    PL_INTENT_SATURATION,
    PL_INTENT_ABSOLUTE_COLORIMETRIC,
};

static const char * const intent_text[] = {
    "Perceptual",
    "Relative colorimetric",
    "Absolute colorimetric",
    "Saturation",
};

#define PRIM_TEXT "Display primaries"
#define PRIM_LONGTEXT "The primaries associated with the output display"

static const int prim_values[] = {
    PL_COLOR_PRIM_UNKNOWN,
    PL_COLOR_PRIM_BT_601_525,
    PL_COLOR_PRIM_BT_601_625,
    PL_COLOR_PRIM_BT_709,
    PL_COLOR_PRIM_BT_470M,
    PL_COLOR_PRIM_BT_2020,
    PL_COLOR_PRIM_APPLE,
    PL_COLOR_PRIM_ADOBE,
    PL_COLOR_PRIM_PRO_PHOTO,
    PL_COLOR_PRIM_CIE_1931,
    PL_COLOR_PRIM_DCI_P3,
    PL_COLOR_PRIM_V_GAMUT,
    PL_COLOR_PRIM_S_GAMUT,
};

static const char * const prim_text[] = {
    "Automatic / Unknown primaries",
    "ITU-R Rec. BT.601 (525-line = NTSC, SMPTE-C)",
    "ITU-R Rec. BT.601 (625-line = PAL, SECAM)",
    "ITU-R Rec. BT.709 (HD), also sRGB",
    "ITU-R Rec. BT.470 M",
    "ITU-R Rec. BT.2020 (UltraHD)",
    "Apple RGB",
    "Adobe RGB (1998)",
    "ProPhoto RGB (ROMM)",
    "CIE 1931 RGB primaries",
    "DCI-P3 (Digital Cinema)",
    "Panasonic V-Gamut (VARICAM)",
    "Sony S-Gamut",
};

#define TRC_TEXT "Display gamma / transfer function"
#define TRC_LONGTEXT "The gamma/transfer function associated with the output display"

static const int trc_values[] = {
    PL_COLOR_TRC_UNKNOWN,
    PL_COLOR_TRC_BT_1886,
    PL_COLOR_TRC_SRGB,
    PL_COLOR_TRC_LINEAR,
    PL_COLOR_TRC_GAMMA18,
    PL_COLOR_TRC_GAMMA22,
    PL_COLOR_TRC_GAMMA28,
    PL_COLOR_TRC_PRO_PHOTO,
    PL_COLOR_TRC_PQ,
    PL_COLOR_TRC_HLG,
    PL_COLOR_TRC_V_LOG,
    PL_COLOR_TRC_S_LOG1,
    PL_COLOR_TRC_S_LOG2,
};

static const char * const trc_text[] = {
    "Automatic / Unknown gamma",
    "ITU-R Rec. BT.1886 (CRT emulation + OOTF)",
    "IEC 61966-2-4 sRGB (CRT emulation)",
    "Linear light content",
    "Pure power gamma 1.8",
    "Pure power gamma 2.2",
    "Pure power gamma 2.8",
    "ProPhoto RGB (ROMM)",
    "ITU-R BT.2100 PQ (perceptual quantizer), aka SMPTE ST2084",
    "ITU-R BT.2100 HLG (hybrid log-gamma), aka ARIB STD-B67",
    "Panasonic V-Log (VARICAM)",
    "Sony S-Log1",
    "Sony S-Log2",
};


#define TONEMAPPING_TEXT "Tone-mapping algorithm"
#define TONEMAPPING_LONGTEXT "Algorithm to use when converting from wide gamut to standard gamut, or from HDR to SDR."

static const int tone_values[] = {
    PL_TONE_MAPPING_HABLE,
    PL_TONE_MAPPING_MOBIUS,
    PL_TONE_MAPPING_REINHARD,
    PL_TONE_MAPPING_GAMMA,
    PL_TONE_MAPPING_LINEAR,
    PL_TONE_MAPPING_CLIP,
};

static const char * const tone_text[] = {
    "Hable (filmic mapping, recommended)",
    "Mobius (linear + knee)",
    "Reinhard (simple non-linear)",
    "Gamma-Power law",
    "Linear stretch (peak to peak)",
    "Hard clip out-of-gamut",
};

#define TONEMAP_PARAM_TEXT "Tone-mapping parameter"
#define TONEMAP_PARAM_LONGTEXT "This parameter can be used to tune the tone-mapping curve. Specifics depend on the curve used. If left as 0, the curve's preferred default is used."

#define TONEMAP_DESAT_TEXT "Tone-mapping desaturation coefficient"
#define TONEMAP_DESAT_LONGTEXT "How strongly to desaturate bright spectral colors towards white. 0.0 disables this behavior."

#define GAMUT_WARN_TEXT "Highlight clipped pixels"
#define GAMUT_WARN_LONGTEXT "Debugging tool to indicate which pixels were clipped as part of the tone mapping process."

#define DITHER_TEXT "Dithering algorithm"
#define DITHER_LONGTEXT "The algorithm to use when dithering to a lower bit depth."

static const int dither_values[] = {
    -1, // no dithering
    PL_DITHER_BLUE_NOISE,
    PL_DITHER_ORDERED_FIXED,
    PL_DITHER_ORDERED_LUT,
    PL_DITHER_WHITE_NOISE,
};

static const char * const dither_text[] = {
    "Disabled",
    "Blue noise (high quality)",
    "Bayer matrix (ordered dither), 16x16 fixed size (fast)",
    "Bayer matrix (ordered dither), any size",
    "White noise (fast but low quality)",
};

#define DITHER_DEPTH_TEXT "Dither depth override (0 = auto)"
#define DITHER_DEPTH_LONGTEXT "Overrides the detected framebuffer depth. Useful to dither to lower bit depths than otherwise required."

#endif // VLC_PLACEBO_UTILS_H
