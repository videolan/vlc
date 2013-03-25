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

/* LIBAVUTIL_VERSION_CHECK checks for the right version of libav and FFmpeg
 * a is the major version
 * b and c the minor and micro versions of libav
 * d and e the minor and micro versions of FFmpeg */
#define LIBAVUTIL_VERSION_CHECK( a, b, c, d, e ) \
    ( (LIBAVUTIL_VERSION_MICRO <  100 && LIBAVUTIL_VERSION_INT >= AV_VERSION_INT( a, b, c ) ) || \
      (LIBAVUTIL_VERSION_MICRO >= 100 && LIBAVUTIL_VERSION_INT >= AV_VERSION_INT( a, d, e ) ) )


unsigned GetVlcDspMask( void );

#ifdef HAVE_LIBAVFORMAT_AVFORMAT_H
# include <libavformat/avformat.h>
static inline void vlc_init_avformat(void)
{
    vlc_avcodec_lock();

#if LIBAVUTIL_VERSION_CHECK(51, 25, 0, 42, 100)
    av_set_cpu_flags_mask( INT_MAX & ~GetVlcDspMask() );
#endif

    av_register_all();

    vlc_avcodec_unlock();
}
#endif

#ifdef HAVE_LIBAVCODEC_AVCODEC_H
# include <libavcodec/avcodec.h>
static inline void vlc_init_avcodec(void)
{
    vlc_avcodec_lock();

#if LIBAVCODEC_VERSION_MAJOR < 54
    avcodec_init();
#endif
    avcodec_register_all();

    vlc_avcodec_unlock();
}
#endif

#ifdef HAVE_LIBAVUTIL_AVUTIL_H
# include <libavutil/avutil.h>
# include <libavutil/dict.h>

#define AV_OPTIONS_TEXT     "Advanced options."
#define AV_OPTIONS_LONGTEXT "Advanced options, in the form {opt=val,opt2=val2} ."

static inline AVDictionary *vlc_av_get_options(const char *psz_opts)
{
    AVDictionary *options = NULL;
    config_chain_t *cfg = NULL;
    config_ChainParseOptions(&cfg, psz_opts);
    while (cfg) {
        config_chain_t *next = cfg->p_next;
        av_dict_set(&options, cfg->psz_name, cfg->psz_value,
            AV_DICT_DONT_STRDUP_KEY | AV_DICT_DONT_STRDUP_VAL);
        free(cfg);
        cfg = next;
    }
    return options;
}
#endif

#endif
