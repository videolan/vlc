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
#ifdef HAVE_LIBAVFORMAT_AVFORMAT_H
#   include <libavformat/avformat.h>
#elif defined(HAVE_FFMPEG_AVFORMAT_H)
#   include <ffmpeg/avformat.h>
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
