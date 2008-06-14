/*****************************************************************************
 * vlcplugin.cpp: a VLC plugin for Mozilla
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Damien Fouilleul <damienf.fouilleul@laposte.net>
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

#ifdef HAVE_MOZILLA_CONFIG_H
#   include <mozilla-config.h>
#endif

#include "vlcplugin.h"
#include "control/npolibvlc.h"

#include <ctype.h>

/*****************************************************************************
 * VlcPlugin constructor and destructor
 *****************************************************************************/
VlcPlugin::VlcPlugin( NPP instance, uint16 mode ) :
    i_npmode(mode),
    b_stream(0),
    b_autoplay(1),
    b_toolbar(0),
    psz_target(NULL),
    libvlc_instance(NULL),
    libvlc_log(NULL),
    p_scriptClass(NULL),
    p_browser(instance),
    psz_baseURL(NULL)
#if XP_WIN
    ,pf_wndproc(NULL)
#endif
#if XP_UNIX
    ,i_width((unsigned)-1)
    ,i_height((unsigned)-1)
    ,i_tb_width(0)
    ,i_tb_height(0)
    ,i_last_position(0)
#endif
{
    memset(&npwindow, 0, sizeof(NPWindow));
}

static bool boolValue(const char *value) {
    return ( !strcmp(value, "1") ||
             !strcasecmp(value, "true") ||
             !strcasecmp(value, "yes") );
}

NPError VlcPlugin::init(int argc, char* const argn[], char* const argv[])
{
    /* prepare VLC command line */
    char *ppsz_argv[32];
    int ppsz_argc = 0;

    /* locate VLC module path */
#ifdef XP_MACOSX
    ppsz_argv[ppsz_argc++] = "--plugin-path";
    ppsz_argv[ppsz_argc++] = "/Library/Internet Plug-Ins/VLC Plugin.plugin/"
                             "Contents/MacOS/modules";
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
    ppsz_argv[ppsz_argc++] = "--ignore-config";
    ppsz_argv[ppsz_argc++] = "--intf";
    ppsz_argv[ppsz_argc++] = "dummy";

    const char *progid = NULL;

    /* parse plugin arguments */
    for( int i = 0; i < argc ; i++ )
    {
        fprintf(stderr, "argn=%s, argv=%s\n", argn[i], argv[i]);

        if( !strcmp( argn[i], "target" )
         || !strcmp( argn[i], "mrl")
         || !strcmp( argn[i], "filename")
         || !strcmp( argn[i], "src") )
        {
            psz_target = argv[i];
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
                ppsz_argv[ppsz_argc++] = "--volume";
                ppsz_argv[ppsz_argc++] = "0";
            }
        }
        else if( !strcmp( argn[i], "loop")
              || !strcmp( argn[i], "autoloop") )
        {
            if( boolValue(argv[i]) )
            {
                ppsz_argv[ppsz_argc++] = "--loop";
            }
            else {
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
            b_toolbar = boolValue(argv[i]);
        }
    }

    libvlc_exception_t ex;
    libvlc_exception_init(&ex);

    libvlc_instance = libvlc_new(ppsz_argc, ppsz_argv, &ex);
    if( libvlc_exception_raised(&ex) )
    {
        libvlc_exception_clear(&ex);
        return NPERR_GENERIC_ERROR;
    }

    /*
    ** fetch plugin base URL, which is the URL of the page containing the plugin
    ** this URL is used for making absolute URL from relative URL that may be
    ** passed as an MRL argument
    */
    NPObject *plugin;

    if( NPERR_NO_ERROR == NPN_GetValue(p_browser, NPNVWindowNPObject, &plugin) )
    {
        /*
        ** is there a better way to get that info ?
        */
        static const char docLocHref[] = "document.location.href";
        NPString script;
        NPVariant result;

        script.utf8characters = docLocHref;
        script.utf8length = sizeof(docLocHref)-1;

        if( NPN_Evaluate(p_browser, plugin, &script, &result) )
        {
            if( NPVARIANT_IS_STRING(result) )
            {
                NPString &location = NPVARIANT_TO_STRING(result);

                psz_baseURL = new char[location.utf8length+1];
                if( psz_baseURL )
                {
                    strncpy(psz_baseURL, location.utf8characters, location.utf8length);
                    psz_baseURL[location.utf8length] = '\0';
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

    return NPERR_NO_ERROR;
}

VlcPlugin::~VlcPlugin()
{
    delete psz_baseURL;
    delete psz_target;
    if( libvlc_log )
        libvlc_log_close(libvlc_log, NULL);
    if( libvlc_instance )
        libvlc_release(libvlc_instance);
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
            char *href = new char[baseLen+strlen(url)+1];
            if( href )
            {
                /* prepend base URL */
                strcpy(href, psz_baseURL);

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
                char *pathend;
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
                    pathend = href+baseLen;
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
                        return NULL;
                    }
                    pathstart = href;
                    pathend = href+baseLen;
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

#if XP_UNIX
int  VlcPlugin::setSize(unsigned width, unsigned height)
{
    int diff = (width != i_width) || (height != i_height);

    i_width = width;
    i_height = height;

    /* return size */
    return diff;
}

void VlcPlugin::showToolbar()
{
    const NPWindow& window = getWindow();
    Window control = getControlWindow();
    Display *p_display = ((NPSetWindowCallbackStruct *)window.ws_info)->display;
    unsigned int i_height = 0, i_width = 0;

    /* load icons */
    XpmReadFileToImage( p_display, DATA_PATH "/mozilla/play.xpm",
                        &p_btnPlay, NULL, NULL);
    if( p_btnPlay )
    {
        i_height = __MAX( i_height, p_btnPlay->height );
        i_width  = __MAX( i_width,  p_btnPlay->width );
    }
    XpmReadFileToImage( p_display, DATA_PATH "/mozilla/pause.xpm",
                        &p_btnPause, NULL, NULL);
    if( p_btnPause )
    {
        i_height = __MAX( i_height, p_btnPause->height );
        i_width  = __MAX( i_width,  p_btnPause->width );
    }
    XpmReadFileToImage( p_display, DATA_PATH "/mozilla/stop.xpm",
                        &p_btnStop, NULL, NULL );
    if( p_btnStop )
    {
        i_height = __MAX( i_height, p_btnStop->height );
        i_width  = __MAX( i_width,  p_btnStop->width );
    }
    XpmReadFileToImage( p_display, DATA_PATH "/mozilla/time_line.xpm",
                        &p_timeline, NULL, NULL);
    if( p_timeline )
    {
        i_height = __MAX( i_height, p_timeline->height );
        i_width  = __MAX( i_width,  p_timeline->width );
    }
    XpmReadFileToImage( p_display, DATA_PATH "/mozilla/time_icon.xpm",
                        &p_btnTime, NULL, NULL);
    if( p_btnTime )
    {
        i_height = __MAX( i_height, p_btnTime->height );
        i_width  = __MAX( i_width,  p_btnTime->width );
    }
    XpmReadFileToImage( p_display, DATA_PATH "/mozilla/fullscreen.xpm",
                        &p_btnFullscreen, NULL, NULL);
    if( p_btnFullscreen )
    {
        i_height = __MAX( i_height, p_btnFullscreen->height );
        i_width  = __MAX( i_width,  p_btnFullscreen->width );
    }
    XpmReadFileToImage( p_display, DATA_PATH "/mozilla/volume_max.xpm",
                        &p_btnMute, NULL, NULL);
    if( p_btnMute )
    {
        i_height = __MAX( i_height, p_btnMute->height );
        i_width  = __MAX( i_width,  p_btnMute->width );
    }
    XpmReadFileToImage( p_display, DATA_PATH "/mozilla/volume_mute.xpm",
                        &p_btnUnmute, NULL, NULL);
    if( p_btnUnmute )
    {
        i_height = __MAX( i_height, p_btnUnmute->height );
        i_width  = __MAX( i_width,  p_btnUnmute->width );
    }
    setToolbarSize( i_width, i_height );

    if( !p_btnPlay || !p_btnPause || !p_btnStop || !p_timeline ||
        !p_btnTime || !p_btnFullscreen || !p_btnMute || !p_btnUnmute )
        fprintf(stderr, "Error: some button images not found in %s\n", DATA_PATH );
}

void VlcPlugin::hideToolbar()
{
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
}

void VlcPlugin::redrawToolbar()
{
    libvlc_media_player_t *p_md = NULL;
    libvlc_exception_t ex;
    float f_position = 0.0;
    int i_playing = 0;
    bool b_mute = false;
    unsigned int dst_x, dst_y;
    GC gc;
    XGCValues gcv;
#define BTN_SPACE ((unsigned int)4)

    const NPWindow& window = getWindow();
    Window control = getControlWindow();
    Display *p_display = ((NPSetWindowCallbackStruct *)window.ws_info)->display;

    /* get media instance */
    libvlc_exception_init( &ex );
    p_md = libvlc_playlist_get_media_player( getVLC(), &ex );
    libvlc_exception_clear( &ex );

    /* get isplaying */
    libvlc_exception_init( &ex );
    i_playing = libvlc_playlist_isplaying( getVLC(), &ex );
    libvlc_exception_clear( &ex );

    /* get mute info */
    libvlc_exception_init(&ex);
    b_mute = libvlc_audio_get_mute( getVLC(), &ex );
    libvlc_exception_clear( &ex );

    /* get movie position in % */
    if( i_playing == 1 )
    {
        libvlc_exception_init( &ex );
        f_position = libvlc_media_player_get_position( p_md, &ex ) * 100;
        libvlc_exception_clear( &ex );
    }
    libvlc_media_player_release( p_md );

    gcv.foreground = BlackPixel( p_display, 0 );
    gc = XCreateGC( p_display, control, GCForeground, &gcv );

    XFillRectangle( p_display, control, gc,
                    0, 0, window.width, i_tb_height );
    gcv.foreground = WhitePixel( p_display, 0 );
    XChangeGC( p_display, gc, GCForeground, &gcv );

    /* position icons */
    dst_x = 4; dst_y = 4;

    fprintf( stderr, ">>>>>> is playing = %d\n", i_playing );
    if( p_btnPause && (i_playing == 1) )
    {
        XPutImage( p_display, control, gc, p_btnPause, 0, 0, dst_x, dst_y,
                   p_btnPause->width, p_btnPause->height );
    }
    else if( p_btnPlay )
    {
        XPutImage( p_display, control, gc, p_btnPlay, 0, 0, dst_x, dst_y,
                   p_btnPlay->width, p_btnPlay->height );
    }

    dst_x += BTN_SPACE + ( p_btnPlay ? p_btnPlay->width : 0 );
    dst_y = 4;

    if( p_btnStop )
        XPutImage( p_display, control, gc, p_btnStop, 0, 0, dst_x, dst_y,
                   p_btnStop->width, p_btnStop->height );

    dst_x += BTN_SPACE + ( p_btnStop ? p_btnStop->width : 0 );
    dst_y = 4;

    if( p_btnFullscreen )
        XPutImage( p_display, control, gc, p_btnFullscreen, 0, 0, dst_x, dst_y,
                   p_btnFullscreen->width, p_btnFullscreen->height );

    dst_x += BTN_SPACE + ( p_btnFullscreen ? p_btnFullscreen->width : 0 );
    dst_y = 4;

    if( p_btnUnmute && b_mute )
    {
        XPutImage( p_display, control, gc, p_btnUnmute, 0, 0, dst_x, dst_y,
                   p_btnUnmute->width, p_btnUnmute->height );

        dst_x += BTN_SPACE + ( p_btnUnmute ? p_btnUnmute->width : 0 );
        dst_y = 4;
    }
    else if( p_btnMute )
    {
        XPutImage( p_display, control, gc, p_btnMute, 0, 0, dst_x, dst_y,
                   p_btnMute->width, p_btnMute->height );

        dst_x += BTN_SPACE + ( p_btnMute ? p_btnMute->width : 0 );
        dst_y = 4;
    }

    if( p_timeline )
        XPutImage( p_display, control, gc, p_timeline, 0, 0, dst_x, dst_y,
                   (window.width-(dst_x+BTN_SPACE)), p_timeline->height );

    if( f_position > 0 )
        i_last_position = (((float)window.width-8.0)/100.0)*f_position;

    if( p_btnTime )
        XPutImage( p_display, control, gc, p_btnTime,
                   0, 0, (dst_x+i_last_position), dst_y,
                   p_btnTime->width, p_btnTime->height );

    XFreeGC( p_display, gc );
}
#endif
