/*****************************************************************************
 * avcommon.h: common code for libav*
 *****************************************************************************
 * Copyright (C) 2012 VLC authors and VideoLAN
 * $Id$
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

#include <limits.h>

#include "avcommon_compat.h"

#ifdef HAVE_LIBAVUTIL_AVUTIL_H
# include <libavutil/avutil.h>
# include <libavutil/dict.h>
# include <libavutil/cpu.h>
# include <libavutil/log.h>

#define AV_OPTIONS_TEXT     "Advanced options"
#define AV_OPTIONS_LONGTEXT "Advanced options, in the form {opt=val,opt2=val2}."

static inline AVDictionary *vlc_av_get_options(const char *psz_opts)
{
    AVDictionary *options = NULL;
    config_chain_t *cfg = NULL;
    config_ChainParseOptions(&cfg, psz_opts);
    while (cfg) {
        config_chain_t *next = cfg->p_next;
        av_dict_set(&options, cfg->psz_name, cfg->psz_value, 0);
        free(cfg->psz_name);
        free(cfg->psz_value);
        free(cfg);
        cfg = next;
    }
    return options;
}

static inline void vlc_init_avutil(vlc_object_t *obj)
{
    int level = AV_LOG_QUIET;

    if (!var_InheritBool(obj, "quiet")) {
        int64_t verbose = var_InheritInteger(obj, "verbose");
        if (verbose >= 0) switch(verbose + VLC_MSG_ERR) {
        case VLC_MSG_ERR:
            level = AV_LOG_ERROR;
            break;
        case VLC_MSG_WARN:
            level = AV_LOG_WARNING;
            break;
        case VLC_MSG_DBG:
            level = AV_LOG_VERBOSE;
        default:
            break;
        }
    }

    av_log_set_level(level);

    msg_Dbg(obj, "CPU flags: 0x%08x", av_get_cpu_flags());
}
#endif

#ifdef HAVE_LIBAVFORMAT_AVFORMAT_H
# include <libavformat/avformat.h>
static inline void vlc_init_avformat(vlc_object_t *obj)
{
    vlc_avcodec_lock();

    vlc_init_avutil(obj);

    avformat_network_init();

    av_register_all();

    vlc_avcodec_unlock();
}
#endif

#ifdef HAVE_LIBAVCODEC_AVCODEC_H
# include <libavcodec/avcodec.h>
static inline void vlc_init_avcodec(vlc_object_t *obj)
{
    vlc_avcodec_lock();

    vlc_init_avutil(obj);

    avcodec_register_all();

    vlc_avcodec_unlock();
}
#endif

#endif
