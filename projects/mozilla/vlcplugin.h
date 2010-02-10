/*****************************************************************************
 * vlcplugin.h: a VLC plugin for Mozilla
 *****************************************************************************
 * Copyright (C) 2002-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Damien Fouilleul <damienf@videolan.org>
 *          Jean-Paul Saman <jpsaman@videolan.org>
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

#include <vlc/vlc.h>
#include <pthread.h>
#include <npapi.h>
#include <vector>

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

typedef enum vlc_toolbar_clicked_e {
    clicked_Unknown = 0,
    clicked_Play,
    clicked_Pause,
    clicked_Stop,
    clicked_timeline,
    clicked_Time,
    clicked_Fullscreen,
    clicked_Mute,
    clicked_Unmute
} vlc_toolbar_clicked_t;


// Note that the accessor functions are unsafe, but this is handled in
// the next layer up. 64bit uints can be substituted to taste (shift=6).
template<size_t M> class bitmap
{
private:
    typedef uint32_t bitu_t; enum { shift=5 };
    enum { bmax=M, bpu=1<<shift, mask=bpu-1, units=(bmax+bpu-1)/bpu };
    bitu_t bits[units];
public:
    bool get(size_t idx) const { return bits[idx>>shift]&(1<<(idx&mask)); }
    void set(size_t idx)       { bits[idx>>shift]|=  1<<(idx&mask);  }
    void reset(size_t idx)     { bits[idx>>shift]&=~(1<<(idx&mask)); }
    void toggle(size_t idx)    { bits[idx>>shift]^=  1<<(idx&mask);  }
    size_t maxbit() const      { return bmax; }
    void clear()               { memset(bits,0,sizeof(bits)); }
    bitmap() { clear(); }
    ~bitmap() { }
    bool empty() const { // naive invert() will break this
        for(size_t i=0;i<units;++i)
            if(bits[i]) return false;
        return true;
    }
};

typedef bitmap<libvlc_num_event_types> eventtypes_bitmap_t;


class EventObj: private eventtypes_bitmap_t
{
private:
    typedef libvlc_event_type_t event_t;
    bool have_event(event_t e) const { return e<maxbit()?get(e):false; }

    class Listener: public eventtypes_bitmap_t
    {
    public:
        Listener(event_t e,NPObject *o,bool b): _l(o), _b(b)
            { NPN_RetainObject(o); set(e); }
        Listener(): _l(NULL), _b(false) { }
        ~Listener() { if(_l) NPN_ReleaseObject(_l); }
        NPObject *listener() const { return _l; }
        bool bubble() const { return _b; }
    private:
        NPObject *_l;
        bool _b;
    };

    libvlc_event_manager_t *_em;
    libvlc_callback_t _cb;
    void *_ud;
public:
    EventObj(): _em(NULL)  { /* deferred to init() */ }
    bool init() { return pthread_mutex_init(&mutex, NULL) == 0; }
    ~EventObj() { pthread_mutex_destroy(&mutex); }

    void deliver(NPP browser);
    void callback(const libvlc_event_t*);
    bool insert(const NPString &, NPObject *, bool);
    bool remove(const NPString &, NPObject *, bool);
    void unhook_manager();
    void hook_manager(libvlc_event_manager_t *,libvlc_callback_t, void *);
private:
    event_t find_event(const char *s) const;
    typedef std::vector<Listener> lr_l;
    typedef std::vector<libvlc_event_type_t> ev_l;
    lr_l _llist;
    ev_l _elist;

    pthread_mutex_t mutex;

    bool ask_for_event(event_t e);
    void unask_for_event(event_t e);
};


class VlcPlugin
{
public:
             VlcPlugin( NPP, uint16 );
    virtual ~VlcPlugin();

    NPError             init(int argc, char* const argn[], char* const argv[]);
    libvlc_instance_t*  getVLC()
                            { return libvlc_instance; };
    libvlc_media_player_t* getMD()
    {
        if( !libvlc_media_player )
        {
             libvlc_printerr("no mediaplayer");
        }
        return libvlc_media_player;
    }
    NPP                 getBrowser()
                            { return p_browser; };
    char*               getAbsoluteURL(const char *url);
    NPWindow&           getWindow()
                            { return npwindow; };
    void                setWindow(const NPWindow &window)
                            { npwindow = window; };

    NPClass*            getScriptClass()
                            { return p_scriptClass; };

#if defined(XP_WIN)
    WNDPROC             getWindowProc()
                            { return pf_wndproc; };
    void                setWindowProc(WNDPROC wndproc)
                            { pf_wndproc = wndproc; };
#endif

#if defined(XP_UNIX)
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
    vlc_toolbar_clicked_t getToolbarButtonClicked( int i_xpos, int i_ypos );
#endif

    uint16    i_npmode; /* either NP_EMBED or NP_FULL */

    /* plugin properties */
    int      b_stream;
    int      b_autoplay;
    int      b_toolbar;
    char *   psz_text;
    char *   psz_target;

    void playlist_play()
    {
        if( libvlc_media_player||playlist_select(0) )
            libvlc_media_player_play(libvlc_media_player);
    }
    void playlist_play_item(int idx)
    {
        if( playlist_select(idx) )
            libvlc_media_player_play(libvlc_media_player);
    }
    void playlist_stop()
    {
        if( libvlc_media_player )
            libvlc_media_player_stop(libvlc_media_player);
    }
    void playlist_next()
    {
        if( playlist_select(playlist_index+1) )
            libvlc_media_player_play(libvlc_media_player);
    }
    void playlist_prev()
    {
        if( playlist_select(playlist_index-1) )
            libvlc_media_player_play(libvlc_media_player);
    }
    void playlist_pause()
    {
        if( libvlc_media_player )
            libvlc_media_player_pause(libvlc_media_player);
    }
    int playlist_isplaying()
    {
        int is_playing = 0;
        if( libvlc_media_player )
            is_playing = libvlc_media_player_is_playing(
                                libvlc_media_player );
        return is_playing;
    }

    int playlist_add( const char * );
    int playlist_add_extended_untrusted( const char *, const char *, int,
                                const char ** );
    int playlist_delete_item( int );
    void playlist_clear();
    int  playlist_count();

    void toggle_fullscreen();
    void set_fullscreen( int );
    int  get_fullscreen();

    bool  player_has_vout();


    static bool canUseEventListener();

    EventObj events;
private:
    bool playlist_select(int);
    void set_player_window();

    /* VLC reference */
    int                 playlist_index;
    libvlc_instance_t   *libvlc_instance;
    libvlc_media_list_t *libvlc_media_list;
    libvlc_media_player_t *libvlc_media_player;
    NPClass             *p_scriptClass;

    /* browser reference */
    NPP     p_browser;
    char*   psz_baseURL;

    /* display settings */
    NPWindow  npwindow;
#if defined(XP_WIN)
    WNDPROC   pf_wndproc;
#endif
#if defined(XP_UNIX)
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

    static void eventAsync(void *);
    static void event_callback(const libvlc_event_t *, void *);
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
    /* M3U */ \
    "audio/x-mpegurl:m3u:MPEG audio;" \
    /* MPEG-4 */ \
    "video/mp4:mp4,mpg4:MPEG-4 video;" \
    "audio/mp4:mp4,mpg4:MPEG-4 audio;" \
    "audio/x-m4a:m4a:MPEG-4 audio;" \
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
    "audio/x-ms-wma:wma:Windows Media Audio;" \
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
    "video/x-flv:flv:FLV video;" \
    /* Matroska */ \
    "video/x-matroska:mkv:Matroska video;" \
    "audio/x-matroska:mka:Matroska audio;" \
    /* XSPF */ \
    "application/xspf+xml:xspf:Playlist xspf;"

#endif
