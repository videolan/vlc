/*****************************************************************************
 * vlcplugin.cpp: a VLC plugin for Mozilla
 *****************************************************************************
 * Copyright (C) 2002-2010 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Damien Fouilleul <damienf.fouilleul@laposte.net>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "config.h"

#include "vlcplugin.h"

#include "control/npolibvlc.h"

#include <ctype.h>

#if defined(XP_UNIX)
#   include <pthread.h>
#elif defined(XP_WIN)
    /* windows headers */
#   include <winbase.h>
#else
#warning "locking not implemented for this platform"
#endif

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

/*****************************************************************************
 * utilitiy functions
 *****************************************************************************/
static void plugin_lock_init(plugin_lock_t *lock)
{
    assert(lock);

#if defined(XP_UNIX)
    pthread_mutex_init(&lock->mutex, NULL);
#elif defined(XP_WIN)
    InitializeCriticalSection(&lock->cs);
#else
#warning "locking not implemented in this platform"
#endif
}

static void plugin_lock_destroy(plugin_lock_t *lock)
{
    assert(lock);

#if defined(XP_UNIX)
    pthread_mutex_destroy(&lock->mutex);
#elif defined(XP_WIN)
    DeleteCriticalSection(&lock->cs);
#else
#warning "locking not implemented in this platform"
#endif
}

static void plugin_lock(plugin_lock_t *lock)
{
    assert(lock);

#if defined(XP_UNIX)
    pthread_mutex_lock(&lock->mutex);
#elif defined(XP_WIN)
    EnterCriticalSection(&lock->cs);
#else
#warning "locking not implemented in this platform"
#endif
}

static void plugin_unlock(plugin_lock_t *lock)
{
    assert(lock);

#if defined(XP_UNIX)
    pthread_mutex_unlock(&lock->mutex);
#elif defined(XP_WIN)
    LeaveCriticalSection(&lock->cs);
#else
#warning "locking not implemented in this platform"
#endif
}

/*****************************************************************************
 * Event Object
 *****************************************************************************/
static void handle_input_event(const libvlc_event_t* event, void *param);
static void handle_changed_event(const libvlc_event_t* event, void *param);

static vlcplugin_event_t vlcevents[] = {
    { "MediaPlayerMediaChanged", libvlc_MediaPlayerMediaChanged, handle_input_event },
    { "MediaPlayerNothingSpecial", libvlc_MediaPlayerNothingSpecial, handle_input_event },
    { "MediaPlayerOpening", libvlc_MediaPlayerOpening, handle_input_event },
    { "MediaPlayerBuffering", libvlc_MediaPlayerBuffering, handle_input_event },
    { "MediaPlayerPlaying", libvlc_MediaPlayerPlaying, handle_input_event },
    { "MediaPlayerPaused", libvlc_MediaPlayerPaused, handle_input_event },
    { "MediaPlayerStopped", libvlc_MediaPlayerStopped, handle_input_event },
    { "MediaPlayerForward", libvlc_MediaPlayerForward, handle_input_event },
    { "MediaPlayerBackward", libvlc_MediaPlayerBackward, handle_input_event },
    { "MediaPlayerEndReached", libvlc_MediaPlayerEndReached, handle_input_event },
    { "MediaPlayerEncounteredError", libvlc_MediaPlayerEncounteredError, handle_input_event },
    { "MediaPlayerTimeChanged", libvlc_MediaPlayerTimeChanged, handle_changed_event },
    { "MediaPlayerPositionChanged", libvlc_MediaPlayerPositionChanged, handle_changed_event },
    { "MediaPlayerSeekableChanged", libvlc_MediaPlayerSeekableChanged, handle_changed_event },
    { "MediaPlayerPausableChanged", libvlc_MediaPlayerPausableChanged, handle_changed_event },
    { "MediaPlayerTitleChanged", libvlc_MediaPlayerTitleChanged, handle_changed_event },
    { "MediaPlayerLengthChanged", libvlc_MediaPlayerLengthChanged, handle_changed_event },
};

static void handle_input_event(const libvlc_event_t* event, void *param)
{
    VlcPlugin *plugin = (VlcPlugin*)param;
    switch( event->type )
    {
        case libvlc_MediaPlayerNothingSpecial:
        case libvlc_MediaPlayerOpening:
        case libvlc_MediaPlayerBuffering:
        case libvlc_MediaPlayerPlaying:
        case libvlc_MediaPlayerPaused:
        case libvlc_MediaPlayerStopped:
        case libvlc_MediaPlayerForward:
        case libvlc_MediaPlayerBackward:
        case libvlc_MediaPlayerEndReached:
        case libvlc_MediaPlayerEncounteredError:
            plugin->event_callback(event, NULL, 0, param);
            break;
        default: /* ignore all other libvlc_event_type_t */
            break;
    }
}

static void handle_changed_event(const libvlc_event_t* event, void *param)
{
    uint32_t   npcount = 1;
    NPVariant *npparam = (NPVariant *) NPN_MemAlloc( sizeof(NPVariant) * npcount );

    VlcPlugin *plugin = (VlcPlugin*)param;
    switch( event->type )
    {
        case libvlc_MediaPlayerTimeChanged:
            DOUBLE_TO_NPVARIANT(event->u.media_player_time_changed.new_time, npparam[0]);
            break;
        case libvlc_MediaPlayerPositionChanged:
            DOUBLE_TO_NPVARIANT(event->u.media_player_position_changed.new_position, npparam[0]);
            break;
        case libvlc_MediaPlayerSeekableChanged:
            BOOLEAN_TO_NPVARIANT(event->u.media_player_seekable_changed.new_seekable, npparam[0]);
            break;
        case libvlc_MediaPlayerPausableChanged:
            BOOLEAN_TO_NPVARIANT(event->u.media_player_pausable_changed.new_pausable, npparam[0]);
            break;
        case libvlc_MediaPlayerTitleChanged:
            BOOLEAN_TO_NPVARIANT(event->u.media_player_title_changed.new_title, npparam[0]);
            break;
        case libvlc_MediaPlayerLengthChanged:
            DOUBLE_TO_NPVARIANT(event->u.media_player_length_changed.new_length, npparam[0]);
            break;
        default: /* ignore all other libvlc_event_type_t */
            NPN_MemFree( npparam );
            return;
    }
    plugin->event_callback(event, npparam, npcount, param);
}

bool EventObj::init()
{
    plugin_lock_init(&lock);
    return true;
}

EventObj::~EventObj()
{
    plugin_lock_destroy(&lock);
}

void EventObj::deliver(NPP browser)
{
    plugin_lock(&lock);

    for( ev_l::iterator iter = _elist.begin(); iter != _elist.end(); ++iter )
    {
        for( lr_l::iterator j = _llist.begin(); j != _llist.end(); ++j )
        {
            if( j->event_type() == iter->event_type() )
            {
                NPVariant result;
                NPVariant *params = iter->params();
                uint32_t   count  = iter->count();
                assert( params );

                NPObject *listener = j->listener();
                assert( listener );

                NPN_InvokeDefault( browser, listener, params, count, &result );
                NPN_ReleaseVariantValue( &result );

                for( uint32_t n = 0; n < count; n++ )
                {
                    if( NPVARIANT_IS_STRING(params[n]) )
                        NPN_MemFree( (void*) NPVARIANT_TO_STRING(params[n]).UTF8Characters );
                    else if( NPVARIANT_IS_OBJECT(params[n]) )
                    {
                        NPN_ReleaseObject( NPVARIANT_TO_OBJECT(params[n]) );
                        NPN_MemFree( (void*)NPVARIANT_TO_OBJECT(params[n]) );
                    }
                }
                NPN_MemFree( params );
            }
        }
    }
    _elist.clear();

    plugin_unlock(&lock);
}

void EventObj::callback(const libvlc_event_t* event,
                        NPVariant *npparams, uint32_t count)
{
    plugin_lock(&lock);
    _elist.push_back(VLCEvent(event->type, npparams, count));
    plugin_unlock(&lock);
}

vlcplugin_event_t *EventObj::find_event(const char *s) const
{
    for( int i = 0; i < ARRAY_SIZE(vlcevents); i++ )
    {
        if( strncmp(vlcevents[i].name, s, strlen(vlcevents[i].name)) == 0 )
            return &vlcevents[i];
    }
    return NULL;
}

const char *EventObj::find_name(const libvlc_event_t *event)
{
    for( int i = 0; i < ARRAY_SIZE(vlcevents); i++ )
    {
        if( vlcevents[i].libvlc_type == event->type )
            return vlcevents[i].name;
    }
    return NULL;
}

bool EventObj::insert(const NPString &name, NPObject *listener, bool bubble)
{
    vlcplugin_event_t *event = find_event(name.UTF8Characters);
    if( !event )
        return false;

    lr_l::iterator iter;
    for( iter = _llist.begin(); iter != _llist.end(); ++iter )
        if( iter->listener() == listener && iter->bubble() == bubble )
            break;

    if( iter == _llist.end() )
    {
        _llist.push_back(Listener(event, listener, bubble));
        return true;
    }

    return false;
}

bool EventObj::remove(const NPString &name, NPObject *listener, bool bubble)
{
    vlcplugin_event_t *event = find_event(name.UTF8Characters);
    if( !event )
        return false;

    for( lr_l::iterator iter = _llist.begin(); iter !=_llist.end(); iter++ )
    {
        if( iter->event_type() == event->libvlc_type &&
            iter->listener() == listener &&
            iter->bubble() == bubble )
        {
            iter = _llist.erase(iter);
            return true;
        }
    }

    return false;
}

void EventObj::hook_manager( libvlc_event_manager_t *em, void *userdata )
{
    _em = em;

    if( _em )
    {
        /* attach all libvlc events we care about */
        for( int i = 0; i < ARRAY_SIZE(vlcevents); i++ )
        {
            libvlc_event_attach( _em, vlcevents[i].libvlc_type,
                                      vlcevents[i].libvlc_callback,
                                      userdata );
        }
    }
}

void EventObj::unhook_manager( void *userdata )
{
    if( _em )
    {
		/* detach all libvlc events we cared about */
        for( int i = 0; i < ARRAY_SIZE(vlcevents); i++ )
        {
            libvlc_event_detach( _em, vlcevents[i].libvlc_type,
                                      vlcevents[i].libvlc_callback,
                                      userdata );
        }
    }
}

/*****************************************************************************
 * VlcPlugin constructor and destructor
 *****************************************************************************/
#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
VlcPlugin::VlcPlugin( NPP instance, uint16 mode ) :
#else
VlcPlugin::VlcPlugin( NPP instance, uint16_t mode ) :
#endif
    i_npmode(mode),
    b_stream(0),
    b_autoplay(1),
    b_toolbar(0),
    psz_text(NULL),
    psz_target(NULL),
    playlist_index(-1),
    libvlc_instance(NULL),
    libvlc_media_list(NULL),
    libvlc_media_player(NULL),
    p_scriptClass(NULL),
    p_browser(instance),
    psz_baseURL(NULL)
#if defined(XP_WIN)
    ,pf_wndproc(NULL)
#endif
#if defined(XP_UNIX)
    ,i_width((unsigned)-1)
    ,i_height((unsigned)-1)
    ,i_tb_width(0)
    ,i_tb_height(0)
    ,i_last_position(0)
    ,p_btnPlay(NULL)
    ,p_btnPause(NULL)
    ,p_btnStop(NULL)
    ,p_btnMute(NULL)
    ,p_btnUnmute(NULL)
    ,p_btnFullscreen(NULL)
    ,p_btnTime(NULL)
    ,p_timeline(NULL)
#endif
{
    memset(&npwindow, 0, sizeof(NPWindow));
#if defined(XP_UNIX)
    memset(&npvideo, 0, sizeof(Window));
    memset(&npcontrol, 0, sizeof(Window));
#endif
}

static bool boolValue(const char *value) {
    return ( !strcmp(value, "1") ||
             !strcasecmp(value, "true") ||
             !strcasecmp(value, "yes") );
}

void VlcPlugin::eventAsync(void *param)
{
    VlcPlugin *plugin = (VlcPlugin*)param;
    plugin->events.deliver(plugin->getBrowser());
}

void VlcPlugin::event_callback(const libvlc_event_t* event,
                NPVariant *npparams, uint32_t npcount, void *userdata)
{
    VlcPlugin *plugin = (VlcPlugin*)userdata;
#if defined(XP_UNIX)
    plugin->events.callback(event, npparams, npcount);
    NPN_PluginThreadAsyncCall(plugin->getBrowser(), eventAsync, plugin);
#else
#warning NPN_PluginThreadAsyncCall not implemented yet.
    printf("No NPN_PluginThreadAsyncCall(), doing nothing.\n");
#endif
}

NPError VlcPlugin::init(int argc, char* const argn[], char* const argv[])
{
    /* prepare VLC command line */
    const char *ppsz_argv[32];
    int ppsz_argc = 0;

#ifndef NDEBUG
    ppsz_argv[ppsz_argc++] = "--no-plugins-cache";
#endif

    /* locate VLC module path */
#ifdef XP_MACOSX
    ppsz_argv[ppsz_argc++] = "--plugin-path=/Library/Internet\\ Plug-Ins/VLC\\ Plugin.plugin/Contents/MacOS/plugins";
    ppsz_argv[ppsz_argc++] = "--vout=minimal_macosx";
#elif defined(XP_WIN)
    HKEY h_key;
    DWORD i_type, i_data = MAX_PATH + 1;
    char p_data[MAX_PATH + 1];
    if( RegOpenKeyEx( HKEY_LOCAL_MACHINE, "Software\\VideoLAN\\VLC",
                      0, KEY_READ, &h_key ) == ERROR_SUCCESS )
    {
         if( RegQueryValueEx( h_key, "InstallDir", 0, &i_type,
                              (LPBYTE)p_data, &i_data ) == ERROR_SUCCESS )
         {
             if( i_type == REG_SZ )
             {
                 strcat( p_data, "\\plugins" );
                 ppsz_argv[ppsz_argc++] = "--plugin-path";
                 ppsz_argv[ppsz_argc++] = p_data;
             }
         }
         RegCloseKey( h_key );
    }
    ppsz_argv[ppsz_argc++] = "--no-one-instance";

#endif /* XP_MACOSX */

    /* common settings */
    ppsz_argv[ppsz_argc++] = "-vv";
    ppsz_argv[ppsz_argc++] = "--no-stats";
    ppsz_argv[ppsz_argc++] = "--no-media-library";
    ppsz_argv[ppsz_argc++] = "--intf=dummy";
    ppsz_argv[ppsz_argc++] = "--no-video-title-show";
    ppsz_argv[ppsz_argc++] = "--no-xlib";

    const char *progid = NULL;

    /* parse plugin arguments */
    for( int i = 0; (i < argc) && (ppsz_argc < 32); i++ )
    {
       /* fprintf(stderr, "argn=%s, argv=%s\n", argn[i], argv[i]); */

        if( !strcmp( argn[i], "target" )
         || !strcmp( argn[i], "mrl")
         || !strcmp( argn[i], "filename")
         || !strcmp( argn[i], "src") )
        {
            psz_target = argv[i];
        }
        else if( !strcmp( argn[i], "text" ) )
        {
            free( psz_text );
            psz_text = strdup( argv[i] );
        }
        else if( !strcmp( argn[i], "autoplay")
              || !strcmp( argn[i], "autostart") )
        {
            b_autoplay = boolValue(argv[i]);
        }
        else if( !strcmp( argn[i], "fullscreen" ) )
        {
            if( boolValue(argv[i]) )
            {
                ppsz_argv[ppsz_argc++] = "--fullscreen";
            }
            else
            {
                ppsz_argv[ppsz_argc++] = "--no-fullscreen";
            }
        }
        else if( !strcmp( argn[i], "mute" ) )
        {
            if( boolValue(argv[i]) )
            {
                ppsz_argv[ppsz_argc++] = "--volume=0";
            }
        }
        else if( !strcmp( argn[i], "loop")
              || !strcmp( argn[i], "autoloop") )
        {
            if( boolValue(argv[i]) )
            {
                ppsz_argv[ppsz_argc++] = "--loop";
            }
            else
            {
                ppsz_argv[ppsz_argc++] = "--no-loop";
            }
        }
        else if( !strcmp( argn[i], "version")
              || !strcmp( argn[i], "progid") )
        {
            progid = argv[i];
        }
        else if( !strcmp( argn[i], "toolbar" ) )
        {
/* FIXME: Remove this when toolbar functionality has been implemented on
 * MacOS X and Win32 for Firefox/Mozilla/Safari. */
#ifdef XP_UNIX
            b_toolbar = boolValue(argv[i]);
#endif
        }
    }

    libvlc_instance = libvlc_new(ppsz_argc, ppsz_argv);
    if( !libvlc_instance )
        return NPERR_GENERIC_ERROR;
    libvlc_media_list = libvlc_media_list_new(libvlc_instance);

    /*
    ** fetch plugin base URL, which is the URL of the page containing the plugin
    ** this URL is used for making absolute URL from relative URL that may be
    ** passed as an MRL argument
    */
    NPObject *plugin = NULL;

    if( NPERR_NO_ERROR == NPN_GetValue(p_browser, NPNVWindowNPObject, &plugin) )
    {
        /*
        ** is there a better way to get that info ?
        */
        static const char docLocHref[] = "document.location.href";
        NPString script;
        NPVariant result;

        script.UTF8Characters = docLocHref;
        script.UTF8Length = sizeof(docLocHref)-1;

        if( NPN_Evaluate(p_browser, plugin, &script, &result) )
        {
            if( NPVARIANT_IS_STRING(result) )
            {
                NPString &location = NPVARIANT_TO_STRING(result);

                psz_baseURL = (char *) malloc(location.UTF8Length+1);
                if( psz_baseURL )
                {
                    strncpy(psz_baseURL, location.UTF8Characters, location.UTF8Length);
                    psz_baseURL[location.UTF8Length] = '\0';
                }
            }
            NPN_ReleaseVariantValue(&result);
        }
        NPN_ReleaseObject(plugin);
    }

    if( psz_target )
    {
        // get absolute URL from src
        char *psz_absurl = getAbsoluteURL(psz_target);
        psz_target = psz_absurl ? psz_absurl : strdup(psz_target);
    }

    /* assign plugin script root class */
    /* new APIs */
    p_scriptClass = RuntimeNPClass<LibvlcRootNPObject>::getClass();

    if( !events.init() )
        return NPERR_GENERIC_ERROR;

    return NPERR_NO_ERROR;
}

VlcPlugin::~VlcPlugin()
{
    free(psz_baseURL);
    free(psz_target);
    free(psz_text);

    if( libvlc_media_player )
    {
        if( playlist_isplaying() )
            playlist_stop();
        events.unhook_manager( this );
        libvlc_media_player_release( libvlc_media_player );
    }
    if( libvlc_media_list )
        libvlc_media_list_release( libvlc_media_list );
    if( libvlc_instance )
        libvlc_release( libvlc_instance );
}

/*****************************************************************************
 * VlcPlugin playlist replacement methods
 *****************************************************************************/
void VlcPlugin::set_player_window()
{
#ifdef XP_UNIX
    libvlc_media_player_set_xwindow(libvlc_media_player,
                                    (uint32_t)getVideoWindow());
#endif
#ifdef XP_MACOSX
    // XXX FIXME insert appropriate call here
#endif
#ifdef XP_WIN
    libvlc_media_player_set_hwnd(libvlc_media_player,
                                 getWindow().window);
#endif
}

int VlcPlugin::playlist_add( const char *mrl )
{
    int item = -1;
    libvlc_media_t *p_m = libvlc_media_new_location(libvlc_instance,mrl);
    if( !p_m )
        return -1;
    assert( libvlc_media_list );
    libvlc_media_list_lock(libvlc_media_list);
    if( !libvlc_media_list_add_media(libvlc_media_list,p_m) )
        item = libvlc_media_list_count(libvlc_media_list)-1;
    libvlc_media_list_unlock(libvlc_media_list);

    libvlc_media_release(p_m);

    return item;
}

int VlcPlugin::playlist_add_extended_untrusted( const char *mrl, const char *name,
                    int optc, const char **optv )
{
    libvlc_media_t *p_m;
    int item = -1;

    assert( libvlc_media_list );

    p_m = libvlc_media_new_location(libvlc_instance, mrl);
    if( !p_m )
        return -1;

    for( int i = 0; i < optc; ++i )
        libvlc_media_add_option_flag(p_m, optv[i], libvlc_media_option_unique);

    libvlc_media_list_lock(libvlc_media_list);
    if( !libvlc_media_list_add_media(libvlc_media_list,p_m) )
        item = libvlc_media_list_count(libvlc_media_list)-1;
    libvlc_media_list_unlock(libvlc_media_list);
    libvlc_media_release(p_m);

    return item;
}

bool VlcPlugin::playlist_select( int idx )
{
    libvlc_media_t *p_m = NULL;

    assert( libvlc_media_list );

    libvlc_media_list_lock(libvlc_media_list);

    int count = libvlc_media_list_count(libvlc_media_list);
    if( idx<0||idx>=count )
        goto bad_unlock;

    playlist_index = idx;

    p_m = libvlc_media_list_item_at_index(libvlc_media_list,playlist_index);
    libvlc_media_list_unlock(libvlc_media_list);

    if( !p_m )
        return false;

    if( libvlc_media_player )
    {
        if( playlist_isplaying() )
            playlist_stop();
        events.unhook_manager( this );
        libvlc_media_player_release( libvlc_media_player );
        libvlc_media_player = NULL;
    }

    libvlc_media_player = libvlc_media_player_new_from_media( p_m );
    if( libvlc_media_player )
    {
        set_player_window();

        libvlc_event_manager_t *p_em;
        p_em = libvlc_media_player_event_manager( libvlc_media_player );
        events.hook_manager( p_em, this );
    }

    libvlc_media_release( p_m );
    return true;

bad_unlock:
    libvlc_media_list_unlock( libvlc_media_list );
    return false;
}

int VlcPlugin::playlist_delete_item( int idx )
{
    if( !libvlc_media_list )
        return -1;
    libvlc_media_list_lock(libvlc_media_list);
    int ret = libvlc_media_list_remove_index(libvlc_media_list,idx);
    libvlc_media_list_unlock(libvlc_media_list);
    return ret;
}

void VlcPlugin::playlist_clear()
{
    if( libvlc_media_list )
        libvlc_media_list_release(libvlc_media_list);
    libvlc_media_list = libvlc_media_list_new(getVLC());
}

int VlcPlugin::playlist_count()
{
    int items_count = 0;
    if( !libvlc_media_list )
        return items_count;
    libvlc_media_list_lock(libvlc_media_list);
    items_count = libvlc_media_list_count(libvlc_media_list);
    libvlc_media_list_unlock(libvlc_media_list);
    return items_count;
}

void VlcPlugin::toggle_fullscreen()
{
    if( playlist_isplaying() )
        libvlc_toggle_fullscreen(libvlc_media_player);
}

void VlcPlugin::set_fullscreen( int yes )
{
    if( playlist_isplaying() )
        libvlc_set_fullscreen(libvlc_media_player,yes);
}

int  VlcPlugin::get_fullscreen()
{
    int r = 0;
    if( playlist_isplaying() )
        r = libvlc_get_fullscreen(libvlc_media_player);
    return r;
}

bool  VlcPlugin::player_has_vout()
{
    bool r = false;
    if( playlist_isplaying() )
        r = libvlc_media_player_has_vout(libvlc_media_player);
    return r;
}

/*****************************************************************************
 * VlcPlugin methods
 *****************************************************************************/

char *VlcPlugin::getAbsoluteURL(const char *url)
{
    if( NULL != url )
    {
        // check whether URL is already absolute
        const char *end=strchr(url, ':');
        if( (NULL != end) && (end != url) )
        {
            // validate protocol header
            const char *start = url;
            char c = *start;
            if( isalpha(c) )
            {
                ++start;
                while( start != end )
                {
                    c  = *start;
                    if( ! (isalnum(c)
                       || ('-' == c)
                       || ('+' == c)
                       || ('.' == c)
                       || ('/' == c)) ) /* VLC uses / to allow user to specify a demuxer */
                        // not valid protocol header, assume relative URL
                        goto relativeurl;
                    ++start;
                }
                /* we have a protocol header, therefore URL is absolute */
                return strdup(url);
            }
            // not a valid protocol header, assume relative URL
        }

relativeurl:

        if( psz_baseURL )
        {
            size_t baseLen = strlen(psz_baseURL);
            char *href = (char *) malloc(baseLen+strlen(url)+1);
            if( href )
            {
                /* prepend base URL */
                memcpy(href, psz_baseURL, baseLen+1);

                /*
                ** relative url could be empty,
                ** in which case return base URL
                */
                if( '\0' == *url )
                    return href;

                /*
                ** locate pathname part of base URL
                */

                /* skip over protocol part  */
                char *pathstart = strchr(href, ':');
                char *pathend = href+baseLen;
                if( pathstart )
                {
                    if( '/' == *(++pathstart) )
                    {
                        if( '/' == *(++pathstart) )
                        {
                            ++pathstart;
                        }
                    }
                    /* skip over host part */
                    pathstart = strchr(pathstart, '/');
                    if( ! pathstart )
                    {
                        // no path, add a / past end of url (over '\0')
                        pathstart = pathend;
                        *pathstart = '/';
                    }
                }
                else
                {
                    /* baseURL is just a UNIX path */
                    if( '/' != *href )
                    {
                        /* baseURL is not an absolute path */
                        free(href);
                        return NULL;
                    }
                    pathstart = href;
                }

                /* relative URL made of an absolute path ? */
                if( '/' == *url )
                {
                    /* replace path completely */
                    strcpy(pathstart, url);
                    return href;
                }

                /* find last path component and replace it */
                while( '/' != *pathend)
                    --pathend;

                /*
                ** if relative url path starts with one or more '../',
                ** factor them out of href so that we return a
                ** normalized URL
                */
                while( pathend != pathstart )
                {
                    const char *p = url;
                    if( '.' != *p )
                        break;
                    ++p;
                    if( '\0' == *p  )
                    {
                        /* relative url is just '.' */
                        url = p;
                        break;
                    }
                    if( '/' == *p  )
                    {
                        /* relative url starts with './' */
                        url = ++p;
                        continue;
                    }
                    if( '.' != *p )
                        break;
                    ++p;
                    if( '\0' == *p )
                    {
                        /* relative url is '..' */
                    }
                    else
                    {
                        if( '/' != *p )
                            break;
                        /* relative url starts with '../' */
                        ++p;
                    }
                    url = p;
                    do
                    {
                        --pathend;
                    }
                    while( '/' != *pathend );
                }
                /* skip over '/' separator */
                ++pathend;
                /* concatenate remaining base URL and relative URL */
                strcpy(pathend, url);
            }
            return href;
        }
    }
    return NULL;
}

#if defined(XP_UNIX)
int  VlcPlugin::setSize(unsigned width, unsigned height)
{
    int diff = (width != i_width) || (height != i_height);

    i_width = width;
    i_height = height;

    /* return size */
    return diff;
}

#define BTN_SPACE ((unsigned int)4)
void VlcPlugin::showToolbar()
{
    const NPWindow& window = getWindow();
    Window control = getControlWindow();
    Window video = getVideoWindow();
    Display *p_display = ((NPSetWindowCallbackStruct *)window.ws_info)->display;
    unsigned int i_height = 0, i_width = BTN_SPACE;

    /* load icons */
    if( !p_btnPlay )
        XpmReadFileToImage( p_display, DATA_PATH "/mozilla/play.xpm",
                            &p_btnPlay, NULL, NULL);
    if( p_btnPlay )
    {
        i_height = __MAX( i_height, p_btnPlay->height );
    }
    if( !p_btnPause )
        XpmReadFileToImage( p_display, DATA_PATH "/mozilla/pause.xpm",
                            &p_btnPause, NULL, NULL);
    if( p_btnPause )
    {
        i_height = __MAX( i_height, p_btnPause->height );
    }
    i_width += __MAX( p_btnPause->width, p_btnPlay->width );

    if( !p_btnStop )
        XpmReadFileToImage( p_display, DATA_PATH "/mozilla/stop.xpm",
                            &p_btnStop, NULL, NULL );
    if( p_btnStop )
    {
        i_height = __MAX( i_height, p_btnStop->height );
        i_width += BTN_SPACE + p_btnStop->width;
    }
    if( !p_timeline )
        XpmReadFileToImage( p_display, DATA_PATH "/mozilla/time_line.xpm",
                            &p_timeline, NULL, NULL);
    if( p_timeline )
    {
        i_height = __MAX( i_height, p_timeline->height );
        i_width += BTN_SPACE + p_timeline->width;
    }
    if( !p_btnTime )
        XpmReadFileToImage( p_display, DATA_PATH "/mozilla/time_icon.xpm",
                            &p_btnTime, NULL, NULL);
    if( p_btnTime )
    {
        i_height = __MAX( i_height, p_btnTime->height );
        i_width += BTN_SPACE + p_btnTime->width;
    }
    if( !p_btnFullscreen )
        XpmReadFileToImage( p_display, DATA_PATH "/mozilla/fullscreen.xpm",
                            &p_btnFullscreen, NULL, NULL);
    if( p_btnFullscreen )
    {
        i_height = __MAX( i_height, p_btnFullscreen->height );
        i_width += BTN_SPACE + p_btnFullscreen->width;
    }
    if( !p_btnMute )
        XpmReadFileToImage( p_display, DATA_PATH "/mozilla/volume_max.xpm",
                            &p_btnMute, NULL, NULL);
    if( p_btnMute )
    {
        i_height = __MAX( i_height, p_btnMute->height );
    }
    if( !p_btnUnmute )
        XpmReadFileToImage( p_display, DATA_PATH "/mozilla/volume_mute.xpm",
                            &p_btnUnmute, NULL, NULL);
    if( p_btnUnmute )
    {
        i_height = __MAX( i_height, p_btnUnmute->height );
    }
    i_width += BTN_SPACE + __MAX( p_btnUnmute->width, p_btnMute->width );

    setToolbarSize( i_width, i_height );

    if( !p_btnPlay || !p_btnPause || !p_btnStop || !p_timeline ||
        !p_btnTime || !p_btnFullscreen || !p_btnMute || !p_btnUnmute )
        fprintf(stderr, "Error: some button images not found in %s\n", DATA_PATH );

    /* reset panels position and size */
    /* XXX  use i_width */
    XResizeWindow( p_display, video, window.width, window.height - i_height);
    XMoveWindow( p_display, control, 0, window.height - i_height );
    XResizeWindow( p_display, control, window.width, i_height -1);

    b_toolbar = 1; /* says toolbar is now shown */
    redrawToolbar();
}

void VlcPlugin::hideToolbar()
{
    const NPWindow& window = getWindow();
    Display *p_display = ((NPSetWindowCallbackStruct *)window.ws_info)->display;
    Window control = getControlWindow();
    Window video = getVideoWindow();

    i_tb_width = i_tb_height = 0;

    if( p_btnPlay )  XDestroyImage( p_btnPlay );
    if( p_btnPause ) XDestroyImage( p_btnPause );
    if( p_btnStop )  XDestroyImage( p_btnStop );
    if( p_timeline ) XDestroyImage( p_timeline );
    if( p_btnTime )  XDestroyImage( p_btnTime );
    if( p_btnFullscreen ) XDestroyImage( p_btnFullscreen );
    if( p_btnMute )  XDestroyImage( p_btnMute );
    if( p_btnUnmute ) XDestroyImage( p_btnUnmute );

    p_btnPlay = NULL;
    p_btnPause = NULL;
    p_btnStop = NULL;
    p_timeline = NULL;
    p_btnTime = NULL;
    p_btnFullscreen = NULL;
    p_btnMute = NULL;
    p_btnUnmute = NULL;

    /* reset panels position and size */
    /* XXX  use i_width */
    XResizeWindow( p_display, video, window.width, window.height );
    XMoveWindow( p_display, control, 0, window.height-1 );
    XResizeWindow( p_display, control, window.width, 1 );

    b_toolbar = 0; /* says toolbar is now hidden */
    redrawToolbar();
}

void VlcPlugin::redrawToolbar()
{
    int is_playing = 0;
    bool b_mute = false;
    unsigned int dst_x, dst_y;
    GC gc;
    XGCValues gcv;
    unsigned int i_tb_width, i_tb_height;

    /* This method does nothing if toolbar is hidden. */
    if( !b_toolbar || !libvlc_media_player )
        return;

    const NPWindow& window = getWindow();
    Window control = getControlWindow();
    Display *p_display = ((NPSetWindowCallbackStruct *)window.ws_info)->display;

    getToolbarSize( &i_tb_width, &i_tb_height );

    /* get mute info */
    b_mute = libvlc_audio_get_mute( libvlc_media_player );

    gcv.foreground = BlackPixel( p_display, 0 );
    gc = XCreateGC( p_display, control, GCForeground, &gcv );

    XFillRectangle( p_display, control, gc,
                    0, 0, window.width, i_tb_height );
    gcv.foreground = WhitePixel( p_display, 0 );
    XChangeGC( p_display, gc, GCForeground, &gcv );

    /* position icons */
    dst_x = BTN_SPACE;
    dst_y = i_tb_height >> 1; /* baseline = vertical middle */

    if( p_btnPause && (is_playing == 1) )
    {
        XPutImage( p_display, control, gc, p_btnPause, 0, 0, dst_x,
                   dst_y - (p_btnPause->height >> 1),
                   p_btnPause->width, p_btnPause->height );
        dst_x += BTN_SPACE + p_btnPause->width;
    }
    else if( p_btnPlay )
    {
        XPutImage( p_display, control, gc, p_btnPlay, 0, 0, dst_x,
                   dst_y - (p_btnPlay->height >> 1),
                   p_btnPlay->width, p_btnPlay->height );
        dst_x += BTN_SPACE + p_btnPlay->width;
    }

    if( p_btnStop )
        XPutImage( p_display, control, gc, p_btnStop, 0, 0, dst_x,
                   dst_y - (p_btnStop->height >> 1),
                   p_btnStop->width, p_btnStop->height );

    dst_x += BTN_SPACE + ( p_btnStop ? p_btnStop->width : 0 );

    if( p_btnFullscreen )
        XPutImage( p_display, control, gc, p_btnFullscreen, 0, 0, dst_x,
                   dst_y - (p_btnFullscreen->height >> 1),
                   p_btnFullscreen->width, p_btnFullscreen->height );

    dst_x += BTN_SPACE + ( p_btnFullscreen ? p_btnFullscreen->width : 0 );

    if( p_btnUnmute && b_mute )
    {
        XPutImage( p_display, control, gc, p_btnUnmute, 0, 0, dst_x,
                   dst_y - (p_btnUnmute->height >> 1),
                   p_btnUnmute->width, p_btnUnmute->height );

        dst_x += BTN_SPACE + ( p_btnUnmute ? p_btnUnmute->width : 0 );
    }
    else if( p_btnMute )
    {
        XPutImage( p_display, control, gc, p_btnMute, 0, 0, dst_x,
                   dst_y - (p_btnMute->height >> 1),
                   p_btnMute->width, p_btnMute->height );

        dst_x += BTN_SPACE + ( p_btnMute ? p_btnMute->width : 0 );
    }

    if( p_timeline )
        XPutImage( p_display, control, gc, p_timeline, 0, 0, dst_x,
                   dst_y - (p_timeline->height >> 1),
                   (window.width-(dst_x+BTN_SPACE)), p_timeline->height );

    /* get movie position in % */
    if( playlist_isplaying() )
    {
        i_last_position = (int)((window.width-(dst_x+BTN_SPACE))*
                   libvlc_media_player_get_position(libvlc_media_player));
    }

    if( p_btnTime )
        XPutImage( p_display, control, gc, p_btnTime,
                   0, 0, (dst_x+i_last_position),
                   dst_y - (p_btnTime->height >> 1),
                   p_btnTime->width, p_btnTime->height );

    XFreeGC( p_display, gc );
}

vlc_toolbar_clicked_t VlcPlugin::getToolbarButtonClicked( int i_xpos, int i_ypos )
{
    unsigned int i_dest = BTN_SPACE;
    int is_playing = 0;
    bool b_mute = false;

#ifndef NDEBUG
    fprintf( stderr, "ToolbarButtonClicked:: "
                     "trying to match (%d,%d) (%d,%d)\n",
             i_xpos, i_ypos, i_tb_height, i_tb_width );
#endif
    if( i_ypos >= i_tb_width )
        return clicked_Unknown;

    /* Note: the order of testing is dependend on the original
     * drawing positions of the icon buttons. Buttons are tested
     * left to right.
     */

    /* get isplaying */
    is_playing = playlist_isplaying();

    /* get mute info */
    if( libvlc_media_player )
        b_mute = libvlc_audio_get_mute( libvlc_media_player );

    /* is Pause of Play button clicked */
    if( (is_playing != 1) &&
        (i_xpos >= (BTN_SPACE>>1)) &&
        (i_xpos <= i_dest + p_btnPlay->width + (BTN_SPACE>>1)) )
        return clicked_Play;
    else if( (i_xpos >= (BTN_SPACE>>1))  &&
             (i_xpos <= i_dest + p_btnPause->width) )
        return clicked_Pause;

    /* is Stop button clicked */
    if( is_playing != 1 )
        i_dest += (p_btnPlay->width + (BTN_SPACE>>1));
    else
        i_dest += (p_btnPause->width + (BTN_SPACE>>1));

    if( (i_xpos >= i_dest) &&
        (i_xpos <= i_dest + p_btnStop->width + (BTN_SPACE>>1)) )
        return clicked_Stop;

    /* is Fullscreen button clicked */
    i_dest += (p_btnStop->width + (BTN_SPACE>>1));
    if( (i_xpos >= i_dest) &&
        (i_xpos <= i_dest + p_btnFullscreen->width + (BTN_SPACE>>1)) )
        return clicked_Fullscreen;

    /* is Mute or Unmute button clicked */
    i_dest += (p_btnFullscreen->width + (BTN_SPACE>>1));
    if( !b_mute && (i_xpos >= i_dest) &&
        (i_xpos <= i_dest + p_btnMute->width + (BTN_SPACE>>1)) )
        return clicked_Mute;
    else if( (i_xpos >= i_dest) &&
             (i_xpos <= i_dest + p_btnUnmute->width + (BTN_SPACE>>1)) )
        return clicked_Unmute;

    /* is timeline clicked */
    if( !b_mute )
        i_dest += (p_btnMute->width + (BTN_SPACE>>1));
    else
        i_dest += (p_btnUnmute->width + (BTN_SPACE>>1));
    if( (i_xpos >= i_dest) &&
        (i_xpos <= i_dest + p_timeline->width + (BTN_SPACE>>1)) )
        return clicked_timeline;

    /* is time button clicked */
    i_dest += (p_timeline->width + (BTN_SPACE>>1));
    if( (i_xpos >= i_dest) &&
        (i_xpos <= i_dest + p_btnTime->width + (BTN_SPACE>>1)) )
        return clicked_Time;

    return clicked_Unknown;
}
#undef BTN_SPACE
#endif

// Verifies the version of the NPAPI.
// The eventListeners use a NPAPI function available
// since Gecko 1.9.
bool VlcPlugin::canUseEventListener()
{
    int plugin_major, plugin_minor;
    int browser_major, browser_minor;

    NPN_Version(&plugin_major, &plugin_minor,
                &browser_major, &browser_minor);

    if (browser_minor >= 19 || browser_major > 0)
        return true;
    return false;
}
