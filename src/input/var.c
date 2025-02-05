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
void vlc_object_InitInputConfig(vlc_object_t *obj,
                                bool playback, bool do_inherit)
{
    /* Create Object Variables for private use only */
    int inherit_flag = do_inherit ? VLC_VAR_DOINHERIT : 0;

    if (playback)
    {
        var_Create(obj, "video", VLC_VAR_BOOL | inherit_flag);
        var_Create(obj, "audio", VLC_VAR_BOOL | inherit_flag);
        var_Create(obj, "spu", VLC_VAR_BOOL | inherit_flag);

        var_Create(obj, "video-track", VLC_VAR_INTEGER|inherit_flag);
        var_Create(obj, "audio-track", VLC_VAR_INTEGER|inherit_flag);
        var_Create(obj, "sub-track", VLC_VAR_INTEGER|inherit_flag);

        var_Create(obj, "audio-language",
                    VLC_VAR_STRING|inherit_flag);
        var_Create(obj, "sub-language",
                    VLC_VAR_STRING|inherit_flag);
        var_Create(obj, "menu-language",
                    VLC_VAR_STRING|inherit_flag);

        var_Create(obj, "video-track-id",
                    VLC_VAR_STRING|inherit_flag);
        var_Create(obj, "audio-track-id",
                    VLC_VAR_STRING|inherit_flag);
        var_Create(obj, "sub-track-id",
                    VLC_VAR_STRING|inherit_flag);

        var_Create(obj, "sub-file", VLC_VAR_STRING | inherit_flag);
        var_Create(obj, "sub-autodetect-file", VLC_VAR_BOOL |
                    inherit_flag);
        var_Create(obj, "sub-autodetect-path", VLC_VAR_STRING |
                    inherit_flag);
        var_Create(obj, "sub-autodetect-fuzzy", VLC_VAR_INTEGER |
                    inherit_flag);

        var_Create(obj, "sout", VLC_VAR_STRING | inherit_flag);
        var_Create(obj, "sout-all",   VLC_VAR_BOOL | inherit_flag);
        var_Create(obj, "sout-audio", VLC_VAR_BOOL | inherit_flag);
        var_Create(obj, "sout-video", VLC_VAR_BOOL | inherit_flag);
        var_Create(obj, "sout-spu", VLC_VAR_BOOL | inherit_flag);
        var_Create(obj, "sout-keep",  VLC_VAR_BOOL | inherit_flag);

        var_Create(obj, "input-repeat",
                    VLC_VAR_INTEGER|inherit_flag);
        var_Create(obj, "start-time", VLC_VAR_FLOAT|inherit_flag);
        var_Create(obj, "stop-time", VLC_VAR_FLOAT|inherit_flag);
        var_Create(obj, "run-time", VLC_VAR_FLOAT|inherit_flag);
        var_Create(obj, "input-fast-seek", VLC_VAR_BOOL|inherit_flag);

        var_Create(obj, "input-slave",
                    VLC_VAR_STRING | inherit_flag);

        var_Create(obj, "audio-desync",
                    VLC_VAR_INTEGER | inherit_flag);
        var_Create(obj, "cr-average",
                    VLC_VAR_INTEGER | inherit_flag);
        var_Create(obj, "clock-synchro",
                    VLC_VAR_INTEGER | inherit_flag);

        var_Create(obj, "bookmarks", VLC_VAR_STRING | inherit_flag);
        var_Create(obj, "programs", VLC_VAR_STRING | inherit_flag);
        var_Create(obj, "program", VLC_VAR_INTEGER | inherit_flag);
        var_Create(obj, "rate", VLC_VAR_FLOAT | inherit_flag);
    }

    /* */
    var_Create(obj, "input-record-native", VLC_VAR_BOOL | inherit_flag);

    /* */
    var_Create(obj, "demux", VLC_VAR_STRING | inherit_flag);
    var_Create(obj, "demux-filter", VLC_VAR_STRING | inherit_flag);
    var_Create(obj, "stream-filter", VLC_VAR_STRING | inherit_flag);

    /* Meta */
    var_Create(obj, "meta-title", VLC_VAR_STRING | inherit_flag);
    var_Create(obj, "meta-author", VLC_VAR_STRING | inherit_flag);
    var_Create(obj, "meta-artist", VLC_VAR_STRING | inherit_flag);
    var_Create(obj, "meta-genre", VLC_VAR_STRING | inherit_flag);
    var_Create(obj, "meta-copyright", VLC_VAR_STRING | inherit_flag);
    var_Create(obj, "meta-description", VLC_VAR_STRING|inherit_flag);
    var_Create(obj, "meta-date", VLC_VAR_STRING | inherit_flag);
    var_Create(obj, "meta-url", VLC_VAR_STRING | inherit_flag);

    /* Inherited by demux/subtitle.c */
    var_Create(obj, "sub-original-fps", VLC_VAR_FLOAT);

    /* used by Medialibrary */
    var_Create(obj, "save-recentplay", VLC_VAR_BOOL | inherit_flag);
}
