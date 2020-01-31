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

#include "gl_common.h"

#ifdef HAVE_LIBPLACEBO
#include "../placebo_utils.h"


#if PL_API_VER >= 10
#define add_desat_params() \
    add_float("desat-strength", pl_color_map_default_params.desaturation_strength, \
              DESAT_STRENGTH_TEXT, DESAT_STRENGTH_LONGTEXT, false) \
    add_float("desat-exponent", pl_color_map_default_params.desaturation_exponent, \
              DESAT_EXPONENT_TEXT, DESAT_EXPONENT_LONGTEXT, false) \
    add_float("desat-base", pl_color_map_default_params.desaturation_base, \
              DESAT_BASE_TEXT, DESAT_BASE_LONGTEXT, false) \
    add_obsolete_string("tone-mapping-desat")
#else
#define add_desat_params() \
    add_float("tone-mapping-desat", pl_color_map_default_params.tone_mapping_desaturate, \
              TONEMAP_DESAT_TEXT, TONEMAP_DESAT_LONGTEXT, false)
#endif

#define add_glopts_placebo() \
    set_section(N_("Colorspace conversion"), NULL) \
    add_integer("rendering-intent", pl_color_map_default_params.intent, \
                RENDER_INTENT_TEXT, RENDER_INTENT_LONGTEXT, false) \
            change_integer_list(intent_values, intent_text) \
    add_integer("target-prim", PL_COLOR_PRIM_UNKNOWN, PRIM_TEXT, PRIM_LONGTEXT, false) \
            change_integer_list(prim_values, prim_text) \
    add_integer("target-trc", PL_COLOR_TRC_UNKNOWN, TRC_TEXT, TRC_LONGTEXT, false) \
            change_integer_list(trc_values, trc_text) \
    set_section(N_("Tone mapping"), NULL) \
    add_integer("tone-mapping", PL_TONE_MAPPING_HABLE, \
                TONEMAPPING_TEXT, TONEMAPPING_LONGTEXT, false) \
            change_integer_list(tone_values, tone_text) \
    add_desat_params() \
    add_float("tone-mapping-param", pl_color_map_default_params.tone_mapping_param, \
              TONEMAP_PARAM_TEXT, TONEMAP_PARAM_LONGTEXT, true) \
    add_bool("tone-mapping-warn", false, GAMUT_WARN_TEXT, GAMUT_WARN_LONGTEXT, false) \
    set_section(N_("Dithering"), NULL) \
    add_integer("dither-algo", -1, DITHER_TEXT, DITHER_LONGTEXT, false) \
            change_integer_list(dither_values, dither_text) \
    add_integer_with_range("dither-depth", 0, 0, 16, \
            DITHER_DEPTH_TEXT, DITHER_DEPTH_LONGTEXT, false)
#else
#define add_glopts_placebo()
#endif

#define GLINTEROP_TEXT N_("Open GL/GLES hardware interop")
#define GLINTEROP_LONGTEXT N_( \
    "Force a \"glinterop\" module.")

#define add_glopts() \
    add_module("glinterop", "glinterop", NULL, GLINTEROP_TEXT, GLINTEROP_LONGTEXT) \
    add_glopts_placebo ()

typedef struct vout_display_opengl_t vout_display_opengl_t;

vout_display_opengl_t *vout_display_opengl_New(video_format_t *fmt,
                                               const vlc_fourcc_t **subpicture_chromas,
                                               vlc_gl_t *gl,
                                               const vlc_viewpoint_t *viewpoint,
                                               vlc_video_context *context);
void vout_display_opengl_Delete(vout_display_opengl_t *vgl);

int vout_display_opengl_SetViewpoint(vout_display_opengl_t *vgl, const vlc_viewpoint_t*);

void vout_display_opengl_SetWindowAspectRatio(vout_display_opengl_t *vgl,
                                              float f_sar);

void vout_display_opengl_Viewport(vout_display_opengl_t *vgl, int x, int y,
                                  unsigned width, unsigned height);

int vout_display_opengl_Prepare(vout_display_opengl_t *vgl,
                                picture_t *picture, subpicture_t *subpicture);
int vout_display_opengl_Display(vout_display_opengl_t *vgl);

#endif
