/*****************************************************************************
 * avcommon.h: common code for libav*
 *****************************************************************************
 * Copyright (C) 2012 VLC authors and VideoLAN
 *
 * Authors: Rafaël Carré <funman@videolanorg>
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

#ifndef AVCOMMON_H
#define AVCOMMON_H 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_avcodec.h>
#include <vlc_configuration.h>
#include <vlc_variables.h>
#include <vlc_es.h>

#include <limits.h>

#include "avcommon_compat.h"

# include <libavutil/avutil.h>
# include <libavutil/dict.h>
# include <libavutil/cpu.h>
# include <libavutil/log.h>

#define VLC_TIME_BASE_Q     (AVRational){1, CLOCK_FREQ}

#define FROM_AVSCALE(x, scale) \
    av_rescale_q((x), (scale), VLC_TIME_BASE_Q)
#define TO_AVSCALE(x, scale)\
    av_rescale_q((x), VLC_TIME_BASE_Q, (scale))

#if (CLOCK_FREQ == AV_TIME_BASE)
#define FROM_AV_TS_NZ(x)     (x)
#define TO_AV_TS_NZ(x)       (x)
#else
#define FROM_AV_TS_NZ(x)  FROM_AVSCALE((x), AV_TIME_BASE_Q)
#define TO_AV_TS_NZ(x)    TO_AVSCALE((x), AV_TIME_BASE_Q)
#endif

#define FROM_AV_TS(x)     (FROM_AV_TS_NZ(x) + VLC_TICK_0)
#define TO_AV_TS(x)       (TO_AV_TS_NZ((x) - VLC_TICK_0))

#define AV_OPTIONS_TEXT     N_("Advanced options")
#define AV_OPTIONS_LONGTEXT N_("Advanced options, in the form {opt=val,opt2=val2}.")

#define AV_RESET_TS_TEXT     N_("Reset timestamps")
#define AV_RESET_TS_LONGTEXT N_("The muxed content will start near a 0 timestamp.")

static inline void vlc_av_get_options(const char *psz_opts, AVDictionary** pp_dict)
{
    config_chain_t *cfg = NULL;
    config_ChainParseOptions(&cfg, psz_opts);
    for (config_chain_t *item = cfg; item != NULL; item = item->p_next) {
        av_dict_set(pp_dict, item->psz_name, item->psz_value, 0);
    }
    if (cfg)
        config_ChainDestroy(cfg);
}

static inline void vlc_init_avutil(vlc_object_t *obj)
{
    int level = AV_LOG_QUIET;

    int64_t verbose = var_InheritInteger(obj, "verbose");
    if (verbose >= 0) switch(verbose + VLC_MSG_ERR) {
    case VLC_MSG_ERR:
        level = AV_LOG_ERROR;
        break;
    case VLC_MSG_WARN:
        level = AV_LOG_WARNING;
        break;
    case VLC_MSG_INFO:
        level = AV_LOG_INFO;
        break;
    case VLC_MSG_DBG:
        level = AV_LOG_VERBOSE;
        break;
    default:
        level = AV_LOG_DEBUG;
        break;
    }

    av_log_set_level(level);

    msg_Dbg(obj, "CPU flags: 0x%08x", av_get_cpu_flags());
}

#ifdef HAVE_LIBAVFORMAT_AVFORMAT_H
# include <libavformat/avformat.h>
# include <libavformat/version.h>
static inline void vlc_init_avformat(vlc_object_t *obj)
{
    vlc_avcodec_lock();

    vlc_init_avutil(obj);

    avformat_network_init();

#if (LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100))
    av_register_all();
#endif

    vlc_avcodec_unlock();
}
#endif

# include <libavcodec/avcodec.h>
# include <libavcodec/version.h>
static inline void vlc_init_avcodec(vlc_object_t *obj)
{
    vlc_avcodec_lock();

    vlc_init_avutil(obj);

#if (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 10, 100))
    avcodec_register_all();
#endif

    vlc_avcodec_unlock();
}

#ifndef AV_ERROR_MAX_STRING_SIZE
 #define AV_ERROR_MAX_STRING_SIZE 64
#endif

static inline vlc_rational_t FromAVRational(const AVRational rat)
{
    return (vlc_rational_t){.num = rat.num, .den = rat.den};
}

static inline void set_video_color_settings( const video_format_t *p_fmt, AVCodecContext *p_context )
{
    switch( p_fmt->color_range )
    {
    case COLOR_RANGE_FULL:
        p_context->color_range = AVCOL_RANGE_JPEG;
        break;
    case COLOR_RANGE_LIMITED:
        p_context->color_range = AVCOL_RANGE_MPEG;
    case COLOR_RANGE_UNDEF: /* do nothing */
        break;
    default:
        p_context->color_range = AVCOL_RANGE_UNSPECIFIED;
        break;
    }

    switch( p_fmt->space )
    {
        case COLOR_SPACE_BT709:
            p_context->colorspace = AVCOL_SPC_BT709;
            break;
        case COLOR_SPACE_BT601:
            p_context->colorspace = AVCOL_SPC_BT470BG;
            break;
        case COLOR_SPACE_BT2020:
            p_context->colorspace = AVCOL_SPC_BT2020_CL;
            break;
        default:
            p_context->colorspace = AVCOL_SPC_UNSPECIFIED;
            break;
    }

    switch( p_fmt->transfer )
    {
        case TRANSFER_FUNC_LINEAR:
            p_context->color_trc = AVCOL_TRC_LINEAR;
            break;
        case TRANSFER_FUNC_SRGB:
            p_context->color_trc = AVCOL_TRC_GAMMA22;
            break;
        case TRANSFER_FUNC_BT470_BG:
            p_context->color_trc = AVCOL_TRC_GAMMA28;
            break;
        case TRANSFER_FUNC_BT470_M:
            p_context->color_trc = AVCOL_TRC_GAMMA22;
            break;
        case TRANSFER_FUNC_BT709:
            p_context->color_trc = AVCOL_TRC_BT709;
            break;
        case TRANSFER_FUNC_SMPTE_ST2084:
            p_context->color_trc = AVCOL_TRC_SMPTEST2084;
            break;
        case TRANSFER_FUNC_SMPTE_240:
            p_context->color_trc = AVCOL_TRC_SMPTE240M;
            break;
        default:
            p_context->color_trc = AVCOL_TRC_UNSPECIFIED;
            break;
    }
    switch( p_fmt->primaries )
    {
        case COLOR_PRIMARIES_BT601_525:
            p_context->color_primaries = AVCOL_PRI_SMPTE170M;
            break;
        case COLOR_PRIMARIES_BT601_625:
            p_context->color_primaries = AVCOL_PRI_BT470BG;
            break;
        case COLOR_PRIMARIES_BT709:
            p_context->color_primaries = AVCOL_PRI_BT709;
            break;
        case COLOR_PRIMARIES_BT2020:
            p_context->color_primaries = AVCOL_PRI_BT2020;
            break;
        case COLOR_PRIMARIES_FCC1953:
            p_context->color_primaries = AVCOL_PRI_BT470M;
            break;
        default:
            p_context->color_primaries = AVCOL_PRI_UNSPECIFIED;
            break;
    }
    switch( p_fmt->chroma_location )
    {
        case CHROMA_LOCATION_LEFT:
            p_context->chroma_sample_location = AVCHROMA_LOC_LEFT;
            break;
        case CHROMA_LOCATION_CENTER:
            p_context->chroma_sample_location = AVCHROMA_LOC_CENTER;
            break;
        case CHROMA_LOCATION_TOP_LEFT:
            p_context->chroma_sample_location = AVCHROMA_LOC_TOPLEFT;
            break;
        case CHROMA_LOCATION_TOP_CENTER:
            p_context->chroma_sample_location = AVCHROMA_LOC_TOP;
            break;
        case CHROMA_LOCATION_BOTTOM_LEFT:
            p_context->chroma_sample_location = AVCHROMA_LOC_BOTTOMLEFT;
            break;
        case CHROMA_LOCATION_BOTTOM_CENTER:
            p_context->chroma_sample_location = AVCHROMA_LOC_BOTTOM;
            break;
        default:
            p_context->chroma_sample_location = AVCHROMA_LOC_UNSPECIFIED;
            break;
    }
}

static inline void get_video_color_settings( const AVCodecContext *ctx,
                                             video_format_t *fmt )
{
    switch ( ctx->color_range )
    {
    case AVCOL_RANGE_JPEG:
        fmt->color_range = COLOR_RANGE_FULL;
        break;
    case AVCOL_RANGE_MPEG:
        fmt->color_range = COLOR_RANGE_LIMITED;
        break;
    default: /* do nothing */
        break;
    }

    switch( ctx->colorspace )
    {
        case AVCOL_SPC_BT709:
            fmt->space = COLOR_SPACE_BT709;
            break;
        case AVCOL_SPC_SMPTE170M:
        case AVCOL_SPC_BT470BG:
            fmt->space = COLOR_SPACE_BT601;
            break;
        case AVCOL_SPC_BT2020_NCL:
        case AVCOL_SPC_BT2020_CL:
            fmt->space = COLOR_SPACE_BT2020;
            break;
        default:
            break;
    }

    switch( ctx->color_trc )
    {
        case AVCOL_TRC_LINEAR:
            fmt->transfer = TRANSFER_FUNC_LINEAR;
            break;
        case AVCOL_TRC_GAMMA22:
            fmt->transfer = TRANSFER_FUNC_SRGB;
            break;
        case AVCOL_TRC_BT709:
            fmt->transfer = TRANSFER_FUNC_BT709;
            break;
        case AVCOL_TRC_SMPTE170M:
        case AVCOL_TRC_BT2020_10:
        case AVCOL_TRC_BT2020_12:
            fmt->transfer = TRANSFER_FUNC_BT2020;
            break;
#if LIBAVUTIL_VERSION_CHECK( 55, 31, 100)
        case AVCOL_TRC_ARIB_STD_B67:
            fmt->transfer = TRANSFER_FUNC_ARIB_B67;
            break;
#endif
#if LIBAVUTIL_VERSION_CHECK( 55, 37, 100)
        case AVCOL_TRC_SMPTE2084:
            fmt->transfer = TRANSFER_FUNC_SMPTE_ST2084;
            break;
        case AVCOL_TRC_SMPTE240M:
            fmt->transfer = TRANSFER_FUNC_SMPTE_240;
            break;
        case AVCOL_TRC_GAMMA28:
            fmt->transfer = TRANSFER_FUNC_BT470_BG;
            break;
#endif
        default:
            break;
    }

    switch( ctx->color_primaries )
    {
        case AVCOL_PRI_BT709:
            fmt->primaries = COLOR_PRIMARIES_BT709;
            break;
        case AVCOL_PRI_BT470BG:
            fmt->primaries = COLOR_PRIMARIES_BT601_625;
            break;
        case AVCOL_PRI_SMPTE170M:
        case AVCOL_PRI_SMPTE240M:
            fmt->primaries = COLOR_PRIMARIES_BT601_525;
            break;
        case AVCOL_PRI_BT2020:
            fmt->primaries = COLOR_PRIMARIES_BT2020;
            break;
        default:
            break;
    }

    switch( ctx->chroma_sample_location )
    {
        case AVCHROMA_LOC_LEFT:
            fmt->chroma_location = CHROMA_LOCATION_LEFT;
            break;
        case AVCHROMA_LOC_CENTER:
            fmt->chroma_location = CHROMA_LOCATION_CENTER;
            break;
        case AVCHROMA_LOC_TOPLEFT:
            fmt->chroma_location = CHROMA_LOCATION_TOP_LEFT;
            break;
        default:
            break;
    }
}

#endif
