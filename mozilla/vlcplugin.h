/*****************************************************************************
 * videolan.c: a VideoLAN plugin for Mozilla
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: vlcplugin.h,v 1.2 2002/07/23 20:12:55 sam Exp $
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
#define PLUGIN_DESCRIPTION \
    "VideoLAN Client Multimedia Player Plugin <br>" \
    " <br>" \
    COPYRIGHT_MESSAGE " <br>" \
    "VideoLAN WWW: <a href=\"http://www.videolan.org/\">http://www.videolan.org/</a>"

#define PLUGIN_MIMETYPES \
    /* MPEG-1 and MPEG-2 */ \
    "audio/mpeg:mp2,mp3,mpga,mpega:MPEG audio;" \
    "audio/x-mpeg:mp2,mp3,mpga,mpega:MPEG audio;" \
    "video/mpeg:mpg,mpeg,mpe:MPEG video;" \
    "video/x-mpeg:mpg,mpeg,mpe:MPEG video;" \
    "video/mpeg-system:mpg,mpeg,mpe,vob:MPEG video;" \
    "video/x-mpeg-system:mpg,mpeg,mpe,vob:MPEG video;" \
    /* MPEG-4 */ \
    "video/mpeg4:mp4,mpg4:MPEG-4 video;" \
    "audio/mpeg4:mp4,mpg4:MPEG-4 audio;" \
    "application/mpeg4-iod:mp4,mpg4:MPEG-4 video;" \
    "application/mpeg4-muxcodetable:mp4,mpg4:MPEG-4 video;" \
    /* AVI */ \
    "video/x-msvideo:avi:AVI video;" \
    /* QuickTime */ \
    "video/quicktime:mov,qt:QuickTime video;" \
    /* explicit plugin call */ \
    "application/x-vlc-plugin::VLC plugin"

