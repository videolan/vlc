/*****************************************************************************
 * avformat.c: demuxer and muxer using libavformat library
 *****************************************************************************
 * Copyright (C) 1999-2008 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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

#ifndef MERGE_FFMPEG
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#include "avformat.h"
#include "../../codec/avcodec/avcommon.h"

vlc_module_begin ()
#endif /* MERGE_FFMPEG */
    add_shortcut( "ffmpeg", "avformat" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_description( N_("Avformat demuxer" ) )
    set_shortname( N_("Avformat") )
    set_capability( "demux", 2 )
    set_callbacks( avformat_OpenDemux, avformat_CloseDemux )
    set_section( N_("Demuxer"), NULL )
    add_string( "avformat-format", NULL, FORMAT_TEXT, FORMAT_LONGTEXT, true )
    add_obsolete_string("ffmpeg-format") /* removed since 2.1.0 */
    add_string( "avformat-options", NULL, AV_OPTIONS_TEXT, AV_OPTIONS_LONGTEXT, true )

#ifdef ENABLE_SOUT
    /* mux submodule */
    add_submodule ()
    add_shortcut( "ffmpeg", "avformat" )
    set_description( N_("Avformat muxer" ) )
    set_capability( "sout mux", 2 )
    set_section( N_("Muxer"), NULL )
    add_string( "sout-avformat-mux", NULL, MUX_TEXT, MUX_LONGTEXT, true )
    add_obsolete_string("ffmpeg-mux") /* removed since 2.1.0 */
    add_string( "sout-avformat-options", NULL, AV_OPTIONS_TEXT, AV_OPTIONS_LONGTEXT, true )
    add_bool( "sout-avformat-reset-ts", false, AV_RESET_TS_TEXT, AV_RESET_TS_LONGTEXT, true )
    set_callbacks( avformat_OpenMux, avformat_CloseMux )
#endif
#ifndef MERGE_FFMPEG
vlc_module_end ()
#endif
