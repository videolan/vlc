/*****************************************************************************
 * avinit.h: common code for libav* initialization
 *****************************************************************************
 * Copyright (C) 2012 the VideoLAN team
 * $Id$
 *
 * Authors: Rafaël Carré <funman@videolanorg>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_avcodec.h>
#include <vlc_configuration.h>
#include <vlc_variables.h>

unsigned GetVlcDspMask( void );

#ifdef HAVE_LIBAVFORMAT_AVFORMAT_H
# include <libavformat/avformat.h>
static inline void vlc_init_avformat(void)
{
    vlc_avcodec_lock();

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
# if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT( 51, 7, 0 )
#  include <libavutil/dict.h>

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
# endif
#endif
