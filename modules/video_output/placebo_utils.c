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
