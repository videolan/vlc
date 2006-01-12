/*****************************************************************************
 * vlcplugin.h: a VLC plugin for Mozilla
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*******************************************************************************
 * Instance state information about the plugin.
 ******************************************************************************/

class VlcPlugin
{
public:
             VlcPlugin( NPP ); 
    virtual ~VlcPlugin();

    void     SetInstance( NPP );
    NPP      GetInstance();
    VlcIntf* GetPeer();

    void     SetFileName( const char* );

    /* Window settings */
    NPWindow* p_npwin;
    uint16    i_npmode;
    uint32    i_width, i_height;

#ifdef XP_WIN
    /* Windows data members */
    HWND     p_hwnd;
    WNDPROC  pf_wndproc;
#endif

#ifdef XP_UNIX
    /* UNIX data members */
    Window   window;
    Display *p_display;
#endif

#ifdef XP_MACOSX
    /* MACOS data members */
    NPWindow *window;
#endif


    /* vlc data members */
    int      i_vlc;
    int      b_stream;
    int      b_autoplay;
    char *   psz_target;

private:
    NPP      p_instance;
    VlcPeer* p_peer;
};

/*******************************************************************************
 * Plugin properties.
 ******************************************************************************/
#define PLUGIN_NAME         "VLC multimedia plugin"
#define PLUGIN_DESCRIPTION \
    "VLC multimedia plugin <br>" \
    " <br>" \
    "version %s <br>" \
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
    /* Ogg */ \
    "application/x-ogg:ogg:Ogg stream;" \
    /* explicit plugin call */ \
    "application/x-vlc-plugin::VLC plugin;" \
    /* windows media */ \
    "video/x-ms-asf-plugin:asf,asx:Windows Media Video;" \
    "video/x-ms-asf:asf,asx:Windows Media Video;" \
    "application/x-mplayer2::Windows Media;" \
    "video/x-ms-wmv:wmv:Windows Media;" \
    /* Google VLC mime */ \
    "application/x-google-vlc-plugin::Google VLC plugin" \

