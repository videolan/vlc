/*****************************************************************************
 * var.c: object variables for input thread
 *****************************************************************************
 * Copyright (C) 2004-2007 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <assert.h>

#include "input_internal.h"

/*****************************************************************************
 * input_ConfigVarInit:
 *  Create all config object variables
 *****************************************************************************/
void input_ConfigVarInit ( input_thread_t *p_input )
{
    /* Create Object Variables for private use only */

    if( !input_priv(p_input)->b_preparsing )
    {
        var_Create( p_input, "video", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
        var_Create( p_input, "audio", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
        var_Create( p_input, "spu", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

        var_Create( p_input, "video-track", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
        var_Create( p_input, "audio-track", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
        var_Create( p_input, "sub-track", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );

        var_Create( p_input, "audio-language",
                    VLC_VAR_STRING|VLC_VAR_DOINHERIT );
        var_Create( p_input, "sub-language",
                    VLC_VAR_STRING|VLC_VAR_DOINHERIT );
        var_Create( p_input, "menu-language",
                    VLC_VAR_STRING|VLC_VAR_DOINHERIT );

        var_Create( p_input, "video-track-id",
                    VLC_VAR_STRING|VLC_VAR_DOINHERIT );
        var_Create( p_input, "audio-track-id",
                    VLC_VAR_STRING|VLC_VAR_DOINHERIT );
        var_Create( p_input, "sub-track-id",
                    VLC_VAR_STRING|VLC_VAR_DOINHERIT );

        var_Create( p_input, "sub-file", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        var_Create( p_input, "sub-autodetect-file", VLC_VAR_BOOL |
                    VLC_VAR_DOINHERIT );
        var_Create( p_input, "sub-autodetect-path", VLC_VAR_STRING |
                    VLC_VAR_DOINHERIT );
        var_Create( p_input, "sub-autodetect-fuzzy", VLC_VAR_INTEGER |
                    VLC_VAR_DOINHERIT );

        var_Create( p_input, "sout", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        var_Create( p_input, "sout-all",   VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
        var_Create( p_input, "sout-audio", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
        var_Create( p_input, "sout-video", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
        var_Create( p_input, "sout-spu", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
        var_Create( p_input, "sout-keep",  VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

        var_Create( p_input, "input-repeat",
                    VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
        var_Create( p_input, "start-time", VLC_VAR_FLOAT|VLC_VAR_DOINHERIT );
        var_Create( p_input, "stop-time", VLC_VAR_FLOAT|VLC_VAR_DOINHERIT );
        var_Create( p_input, "run-time", VLC_VAR_FLOAT|VLC_VAR_DOINHERIT );
        var_Create( p_input, "input-fast-seek", VLC_VAR_BOOL|VLC_VAR_DOINHERIT );

        var_Create( p_input, "input-slave",
                    VLC_VAR_STRING | VLC_VAR_DOINHERIT );

        var_Create( p_input, "audio-desync",
                    VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
        var_Create( p_input, "cr-average",
                    VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
        var_Create( p_input, "clock-synchro",
                    VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);

        var_Create( p_input, "bookmarks", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        var_Create( p_input, "programs", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        var_Create( p_input, "program", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
        var_Create( p_input, "rate", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );
    }

    /* */
    var_Create( p_input, "input-record-native", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    /* */
    var_Create( p_input, "access", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_input, "demux", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_input, "demux-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_input, "stream-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT );

    /* Meta */
    var_Create( p_input, "meta-title", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_input, "meta-author", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_input, "meta-artist", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_input, "meta-genre", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_input, "meta-copyright", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_Create( p_input, "meta-description", VLC_VAR_STRING|VLC_VAR_DOINHERIT);
    var_Create( p_input, "meta-date", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_input, "meta-url", VLC_VAR_STRING | VLC_VAR_DOINHERIT );

    /* Inherited by demux/subtitle.c */
    var_Create( p_input, "sub-original-fps", VLC_VAR_FLOAT );
}
