/*****************************************************************************
 * avformat.c: demuxer and muxer using libavformat library
 *****************************************************************************
 * Copyright (C) 1999-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>

/* ffmpeg header */
#define HAVE_MMX 1
#ifdef HAVE_LIBAVCODEC_AVCODEC_H
#   include <libavcodec/avcodec.h>
#elif defined(HAVE_FFMPEG_AVCODEC_H)
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

#if LIBAVCODEC_BUILD < 5000
#   error You must have a libavcodec >= 5000 (get CVS)
#endif

#include "avformat.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    add_shortcut( "ffmpeg" );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_SCODEC );
    set_description( N_("FFmpeg demuxer" ) );
    set_capability( "demux", 2 );
    set_callbacks( OpenDemux, CloseDemux );

#ifdef ENABLE_SOUT
    /* mux submodule */
    add_submodule();
    set_description( N_("FFmpeg muxer" ) );
    set_capability( "sout mux", 2 );
    add_string( "ffmpeg-mux", NULL, NULL, MUX_TEXT,
                MUX_LONGTEXT, true );
    set_callbacks( OpenMux, CloseMux );
#endif
vlc_module_end();

/*****************************************************************************
 *
 *****************************************************************************/
void LibavcodecCallback( void *p_opaque, int i_level,
                             const char *psz_format, va_list va )
{
    int i_vlc_level;
    AVCodecContext *p_avctx = (AVCodecContext *)p_opaque;
    AVClass *p_avc;
    vlc_object_t *p_this;
    char *psz_new_format;
    const char *psz_item_name;

    p_avc = p_avctx ? p_avctx->av_class : 0;

#define cln p_avc->class_name
    /* Make sure we can get p_this back */
    if( !p_avctx || !p_avc || !cln ||
        cln[0]!='A' || cln[1]!='V' || cln[2]!='C' || cln[3]!='o' ||
        cln[4]!='d' || cln[5]!='e' || cln[6]!='c' )
    {
        if( i_level == AV_LOG_ERROR ) vfprintf( stderr, psz_format, va );
        return;
    }
#undef cln

    p_this = (vlc_object_t *)p_avctx->opaque;

    switch( i_level )
    {
    case AV_LOG_QUIET:
        i_vlc_level = VLC_MSG_ERR;
        break;
    case AV_LOG_ERROR:
        i_vlc_level = VLC_MSG_WARN;
        break;
    case AV_LOG_INFO:
        i_vlc_level = VLC_MSG_DBG;
        break;
    case AV_LOG_DEBUG:
        /* Print debug messages if they were requested */
        if( p_avctx->debug ) vfprintf( stderr, psz_format, va );
        return;
    default:
        return;
    }

    psz_item_name = p_avc->item_name(p_opaque);
    psz_new_format = malloc( strlen(psz_format) + strlen(psz_item_name)
                              + 18 + 5 );
    snprintf( psz_new_format, strlen(psz_format) + strlen(psz_item_name)
              + 18 + 5, "%s (%s@%p)", psz_format, p_avc->item_name(p_opaque), p_opaque );
    msg_GenericVa( p_this, i_vlc_level,
                    MODULE_STRING, psz_new_format, va );
    free( psz_new_format );
}

void InitLibavcodec( vlc_object_t *p_object )
{
    static int b_ffmpeginit = 0;
    vlc_mutex_t *lock = var_AcquireMutex( "avcodec" );

    /* *** init ffmpeg library (libavcodec) *** */
    if( !b_ffmpeginit )
    {
        avcodec_init();
        avcodec_register_all();
        av_log_set_callback( LibavcodecCallback );
        b_ffmpeginit = 1;

        msg_Dbg( p_object, "libavcodec initialized (interface %d )",
                 LIBAVCODEC_VERSION_INT );
    }
    else
    {
        msg_Dbg( p_object, "libavcodec already initialized" );
    }

    vlc_mutex_unlock( lock );
}
