/*****************************************************************************
 * vlcplugin.h: a VLC plugin for Mozilla
 *****************************************************************************
 * Copyright (C) 2002-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
            Damien Fouilleul <damienf@videolan.org>
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
#ifndef __VLCPLUGIN_H__
#define __VLCPLUGIN_H__

#include <vlc/libvlc.h>
#include <npapi.h>
#include "control/nporuntime.h"

#if !defined(XP_MACOSX) && !defined(XP_UNIX) && !defined(XP_WIN)
#define XP_UNIX 1
#elif defined(XP_MACOSX)
#undef XP_UNIX
#endif

#ifdef XP_WIN
    /* Windows stuff */
#endif

#ifdef XP_MACOSX
    /* Mac OS X stuff */
#   include <Quickdraw.h>
#endif

#ifdef XP_UNIX
    /* X11 stuff */
#   include <X11/Xlib.h>
#   include <X11/Intrinsic.h>
#   include <X11/StringDefs.h>
#   include <X11/X.h>

#   ifndef __APPLE__
#       include <X11/xpm.h>
#   endif
#endif

#ifndef __MAX
#   define __MAX(a, b)   ( ((a) > (b)) ? (a) : (b) )
#endif
#ifndef __MIN
#   define __MIN(a, b)   ( ((a) < (b)) ? (a) : (b) )
#endif

class VlcPlugin
{
public:
             VlcPlugin( NPP, uint16 );
    virtual ~VlcPlugin();

    NPError             init(int argc, char* const argn[], char* const argv[]);
    libvlc_instance_t*  getVLC()
                            { return libvlc_instance; };
    NPP                 getBrowser()
                            { return p_browser; };
    char*               getAbsoluteURL(const char *url);
    NPWindow&           getWindow()
                            { return npwindow; };
    void                setWindow(const NPWindow &window)
                            { npwindow = window; };

    NPClass*            getScriptClass()
                            { return p_scriptClass; };

    void                setLog(libvlc_log_t *log)
                            { libvlc_log = log; };
    libvlc_log_t*       getLog()
                            { return libvlc_log; };
#if XP_WIN
    WNDPROC             getWindowProc()
                            { return pf_wndproc; };
    void                setWindowProc(WNDPROC wndproc)
                            { pf_wndproc = wndproc; };
#endif

#if XP_UNIX
    int                 setSize(unsigned width, unsigned height);
    Window              getVideoWindow()
                            { return npvideo; };
    void                setVideoWindow(Window window)
                            { npvideo = window; };
    Window              getControlWindow()
                            { return npcontrol; };
    void                setControlWindow(Window window)
                            { npcontrol = window; };

    void                showToolbar();
    void                hideToolbar();
    void                redrawToolbar();
    void                getToolbarSize(unsigned int *width, unsigned int *height)
                            { *width = i_tb_width; *height = i_tb_height; };
    int                 setToolbarSize(unsigned int width, unsigned int height)
                            { i_tb_width = width; i_tb_height = height; return 1; };
#endif

    uint16    i_npmode; /* either NP_EMBED or NP_FULL */

    /* plugin properties */
    int      b_stream;
    int      b_autoplay;
    int      b_toolbar;
    char *   psz_target;

private:
    /* VLC reference */
    libvlc_instance_t   *libvlc_instance;
    libvlc_log_t        *libvlc_log;
    NPClass             *p_scriptClass;

    /* browser reference */
    NPP     p_browser;
    char*   psz_baseURL;

    /* display settings */
    NPWindow  npwindow;
#if XP_WIN
    WNDPROC   pf_wndproc;
#endif
#if XP_UNIX
    unsigned int     i_width, i_height;
    unsigned int     i_tb_width, i_tb_height;
    Window           npvideo, npcontrol;

    XImage *p_btnPlay;
    XImage *p_btnPause;
    XImage *p_btnStop;
    XImage *p_timeline;
    XImage *p_btnTime;
    XImage *p_btnFullscreen;
    XImage *p_btnMute;
    XImage *p_btnUnmute;

    int i_last_position;
#endif
};

/*******************************************************************************
 * Plugin properties.
 ******************************************************************************/
#define PLUGIN_NAME         "VLC Multimedia Plug-in"
#define PLUGIN_DESCRIPTION \
    "Version %s, copyright 1996-2007 The VideoLAN Team" \
    "<br><a href=\"http://www.videolan.org/\">http://www.videolan.org/</a>"

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
    /* OGG */ \
    "application/x-ogg:ogg:Ogg stream;" \
    "application/ogg:ogg:Ogg stream;" \
    /* VLC */ \
    "application/x-vlc-plugin:vlc:VLC plug-in;" \
    /* Windows Media */ \
    "video/x-ms-asf-plugin:asf,asx:Windows Media Video;" \
    "video/x-ms-asf:asf,asx:Windows Media Video;" \
    "application/x-mplayer2::Windows Media;" \
    "video/x-ms-wmv:wmv:Windows Media;" \
    "video/x-ms-wvx:wvx:Windows Media Video;" \
    /* Google VLC */ \
    "application/x-google-vlc-plugin::Google VLC plug-in;" \
    /* WAV audio */ \
    "audio/wav:wav:WAV audio;" \
    "audio/x-wav:wav:WAV audio;" \
    /* 3GPP */ \
    "audio/3gpp:3gp,3gpp:3GPP audio;" \
    "video/3gpp:3gp,3gpp:3GPP video;" \
    /* 3GPP2 */ \
    "audio/3gpp2:3g2,3gpp2:3GPP2 audio;" \
    "video/3gpp2:3g2,3gpp2:3GPP2 video;" \
    /* DIVX */ \
    "video/divx:divx:DivX video;" \
    /* FLV */ \
    "video/flv:flv:FLV video;" \
    "video/x-flv:flv:FLV video;"



#endif
