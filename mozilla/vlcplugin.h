/*****************************************************************************
 * videolan.c: a VideoLAN plugin for Mozilla
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: vlcplugin.h,v 1.1 2002/07/04 18:11:57 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*******************************************************************************
 * Instance state information about the plugin.
 ******************************************************************************/
typedef struct _PluginInstance
{
    NPWindow* fWindow;
    uint16 fMode;

    /* UNIX data members */
    Window window;
    Display *display;
    uint32 x, y;
    uint32 width, height;

    /* vlc data members */
    vlc_t *p_vlc;
    int b_stream;
    char *psz_target;

} PluginInstance;

/*******************************************************************************
 * Plugin properties.
 ******************************************************************************/
#define PLUGIN_NAME         "VideoLAN Client Plug-in"
#define PLUGIN_DESCRIPTION  "VideoLAN Client (VLC) Multimedia Player Plug-in"

#define PLUGIN_MIMETYPES \
    /* MPEG audio */ \
    "audio/mpeg:mp2,mp3:MPEG audio;" \
    "audio/x-mpeg:mp2,mp3:MPEG audio;" \
    /* MPEG video */ \
    "video/mpeg:mpg,mpeg:MPEG video;" \
    "video/x-mpeg:mpg,mpeg:MPEG video;" \
    "video/mpeg-system:mpg,mpeg:MPEG video;" \
    "video/x-mpeg-system:mpg,mpeg:MPEG video;" \
    /* AVI video */ \
    "video/x-msvideo:avi:AVI video;" \
    /* explicit plugin call */ \
    "application/x-vlc-plugin::VLC plugin"

