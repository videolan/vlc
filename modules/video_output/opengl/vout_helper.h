/*****************************************************************************
 * vout_helper.h: OpenGL vout_display helpers
 *****************************************************************************
 * Copyright (C) 2004-2016 VLC authors and VideoLAN
 * Copyright (C) 2009 Laurent Aimar
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *          Rémi Denis-Courmont
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Ilkka Ollakka <ileoo@videolan.org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          Rafaël Carré <funman@videolanorg>
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

#ifndef VLC_OPENGL_VOUT_HELPER_H
#define VLC_OPENGL_VOUT_HELPER_H

#include "converter.h"

#ifdef HAVE_LIBPLACEBO
#include <libplacebo/shaders/colorspace.h>

#define RENDER_INTENT_TEXT "Rendering intent for color conversion"
#define RENDER_INTENT_LONGTEXT "The algorithm used to convert between color spaces"

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
    "Unknown primaries",
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
    "Unknown gamma",
    "ITU-R Rec. BT.1886 (CRT emulation + OOTF)",
    "IEC 61966-2-4 sRGB (CRT emulation)",
    "Linear light content",
    "Pure power gamma 1.8",
    "Pure power gamma 2.2",
    "Pure power gamma 2.8",
    "ProPhoto RGB (ROMM)",
    "ITU-R BT.2100 PQ (perceptual quantizer), aka SMPTE ST2048",
    "ITU-R BT.2100 HLG (hybrid log-gamma), aka ARIB STD-B67",
    "Panasonic V-Log (VARICAM)",
    "Sony S-Log1",
    "Sony S-Log2",
};

#define TONEMAPPING_TEXT "Tone-mapping algorithm"
#define TONEMAPPING_LONGTEXT "Algorithm to use when converting from wide gamut to standard gamut, or from HDR to SDR"

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
#define TONEMAP_PARAM_LONGTEXT "This parameter can be used to tune the tone-mapping curve. Specifics depend on the curve used."

#define TONEMAP_DESAT_TEXT "Tone-mapping desaturation coefficient"
#define TONEMAP_DESAT_LONGTEXT "How strongly to desaturate overbright colors towards white. 0.0 disables this behavior."

#define TONEMAP_WARN_TEXT "Highlight clipped pixels"
#define TONEMAP_WARN_LONGTEXT "Debugging tool to indicate which pixels were clipped as part of the tone mapping process."

#define DITHER_TEXT "Dithering algorithm"
#define DITHER_LONGTEXT "The algorithm to use when dithering to a lower bit depth (degrades performance on some platforms)."

static const int dither_values[] = {
    -1, // no dithering
    PL_DITHER_BLUE_NOISE,
    PL_DITHER_WHITE_NOISE,
    PL_DITHER_ORDERED_LUT,
};

static const char * const dither_text[] = {
    "Disabled",
    "Blue noise",
    "White noise",
    "Bayer matrix (ordered dither)",
};

#define DEPTH_TEXT "Dither depth override (0 = framebuffer depth)"
#define DEPTH_LONGTEXT "Overrides the detected framebuffer depth. Useful to dither to lower bit depths than otherwise required."

#define add_glopts_placebo() \
    set_section("Colorspace conversion", NULL) \
    add_integer("rendering-intent", pl_color_map_default_params.intent, \
                RENDER_INTENT_TEXT, RENDER_INTENT_LONGTEXT, false) \
            change_integer_list(intent_values, intent_text) \
    add_integer("target-prim", PL_COLOR_PRIM_UNKNOWN, PRIM_TEXT, PRIM_LONGTEXT, false) \
            change_integer_list(prim_values, prim_text) \
    add_integer("target-trc", PL_COLOR_TRC_UNKNOWN, TRC_TEXT, TRC_LONGTEXT, false) \
            change_integer_list(trc_values, trc_text) \
    set_section("Tone mapping", NULL) \
    add_integer("tone-mapping", PL_TONE_MAPPING_HABLE, \
                TONEMAPPING_TEXT, TONEMAPPING_LONGTEXT, false) \
            change_integer_list(tone_values, tone_text) \
    add_float("tone-mapping-param", pl_color_map_default_params.tone_mapping_param, \
              TONEMAP_PARAM_TEXT, TONEMAP_PARAM_LONGTEXT, true) \
    add_float("tone-mapping-desat", pl_color_map_default_params.tone_mapping_desaturate, \
              TONEMAP_DESAT_TEXT, TONEMAP_DESAT_LONGTEXT, false) \
    add_bool("tone-mapping-warn", false, TONEMAP_WARN_TEXT, TONEMAP_WARN_LONGTEXT, false) \
    set_section("Dithering", NULL) \
    add_integer("dither-algo", -1, DITHER_TEXT, DITHER_LONGTEXT, false) \
            change_integer_list(dither_values, dither_text) \
    add_integer_with_range("dither-depth", 0, 0, 16, DEPTH_TEXT, DEPTH_LONGTEXT, false)
#else
#define add_glopts_placebo()
#endif

#define GLCONV_TEXT "Open GL/GLES hardware converter"
#define GLCONV_LONGTEXT "Force a \"glconv\" module."

#define add_glopts() \
    add_module ("glconv", "glconv", NULL, GLCONV_TEXT, GLCONV_LONGTEXT, true) \
    add_glopts_placebo ()

static const vlc_fourcc_t gl_subpicture_chromas[] = {
    VLC_CODEC_RGBA,
    0
};

typedef struct vout_display_opengl_t vout_display_opengl_t;

vout_display_opengl_t *vout_display_opengl_New(video_format_t *fmt,
                                               const vlc_fourcc_t **subpicture_chromas,
                                               vlc_gl_t *gl,
                                               const vlc_viewpoint_t *viewpoint);
void vout_display_opengl_Delete(vout_display_opengl_t *vgl);

picture_pool_t *vout_display_opengl_GetPool(vout_display_opengl_t *vgl, unsigned);

int vout_display_opengl_SetViewpoint(vout_display_opengl_t *vgl, const vlc_viewpoint_t*);

void vout_display_opengl_SetWindowAspectRatio(vout_display_opengl_t *vgl,
                                              float f_sar);

void vout_display_opengl_Viewport(vout_display_opengl_t *vgl, int x, int y,
                                  unsigned width, unsigned height);

int vout_display_opengl_Prepare(vout_display_opengl_t *vgl,
                                picture_t *picture, subpicture_t *subpicture);
int vout_display_opengl_Display(vout_display_opengl_t *vgl,
                                const video_format_t *source);

#endif
