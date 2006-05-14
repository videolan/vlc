/*****************************************************************************
 * rc.c : remote control stdin/stdout module for vlc
 *****************************************************************************
 * Copyright (C) 2004 - 2005 the VideoLAN team
 * $Id$
 *
 * Author: Peter Surda <shurdeek@panorama.sth.ac.at>
 *         Jean-Paul Saman <jpsaman #_at_# m2x _replaceWith#dot_ nl>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <errno.h>                                                 /* ENOMEM */
#include <stdio.h>
#include <ctype.h>
#include <signal.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/aout.h>
#include <vlc/vout.h>
#include <vlc_video.h>
#include <vlc_osd.h>
#include <vlc_update.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif

#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif
#include <sys/types.h>

#include "vlc_error.h"
#include "network.h"
#include "vlc_url.h"

#if defined(AF_UNIX) && !defined(AF_LOCAL)
#    define AF_LOCAL AF_UNIX
#endif

#if defined(AF_LOCAL) && ! defined(WIN32)
#    include <sys/un.h>
#endif

#define MAX_LINE_LENGTH 256
#define STATUS_CHANGE "status change: "

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate     ( vlc_object_t * );
static void Deactivate   ( vlc_object_t * );
static void Run          ( intf_thread_t * );

static void Help         ( intf_thread_t *, vlc_bool_t );
static void RegisterCallbacks( intf_thread_t * );

static vlc_bool_t ReadCommand( intf_thread_t *, char *, int * );

static playlist_item_t *parse_MRL( intf_thread_t *, char * );

static int  Input        ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  Playlist     ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  Other        ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  Quit         ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  Intf         ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  Volume       ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  VolumeMove   ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  AudioConfig  ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  Menu         ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static void checkUpdates( intf_thread_t *p_intf, char *psz_arg );

/* Status Callbacks */
static int TimeOffsetChanged( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t , void * );
static int VolumeChanged    ( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t, void * );
static int StateChanged     ( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t, void * );
static int RateChanged      ( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t, void * );

struct intf_sys_t
{
    int *pi_socket_listen;
    int i_socket;
    char *psz_unix_path;

    /* status changes */
    vlc_mutex_t       status_lock;
    playlist_status_t i_last_state;

#ifdef WIN32
    HANDLE hConsoleIn;
    vlc_bool_t b_quiet;
#endif
};

#ifdef HAVE_VARIADIC_MACROS
#   define msg_rc( psz_format, args... ) \
      __msg_rc( p_intf, psz_format, ## args )
#endif

void __msg_rc( intf_thread_t *p_intf, const char *psz_fmt, ... )
{
    va_list args;
    va_start( args, psz_fmt );

    if( p_intf->p_sys->i_socket == -1 )
    {
        vprintf( psz_fmt, args );
        printf( "\r\n" );
    }
    else
    {
        net_vaPrintf( p_intf, p_intf->p_sys->i_socket, NULL, psz_fmt, args );
        net_Write( p_intf, p_intf->p_sys->i_socket, NULL, (uint8_t*)"\r\n", 2 );
    }
    va_end( args );
}

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define POS_TEXT N_("Show stream position")
#define POS_LONGTEXT N_("Show the current position in seconds within the " \
                        "stream from time to time." )

#define TTY_TEXT N_("Fake TTY")
#define TTY_LONGTEXT N_("Force the rc module to use stdin as if it was a TTY.")

#define UNIX_TEXT N_("UNIX socket command input")
#define UNIX_LONGTEXT N_("Accept commands over a Unix socket rather than " \
                         "stdin." )

#define HOST_TEXT N_("TCP command input")
#define HOST_LONGTEXT N_("Accept commands over a socket rather than stdin. " \
            "You can set the address and port the interface will bind to." )

#ifdef WIN32
#define QUIET_TEXT N_("Do not open a DOS command box interface")
#define QUIET_LONGTEXT N_( \
    "By default the rc interface plugin will start a DOS command box. " \
    "Enabling the quiet mode will not bring this command box but can also " \
    "be pretty annoying when you want to stop VLC and no video window is " \
    "open." )
#endif

vlc_module_begin();
    set_shortname( _("RC"));
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_MAIN );
    set_description( _("Remote control interface") );
    add_bool( "rc-show-pos", 0, NULL, POS_TEXT, POS_LONGTEXT, VLC_TRUE );
#ifdef HAVE_ISATTY
    add_bool( "rc-fake-tty", 0, NULL, TTY_TEXT, TTY_LONGTEXT, VLC_TRUE );
#endif
    add_string( "rc-unix", 0, NULL, UNIX_TEXT, UNIX_LONGTEXT, VLC_TRUE );
    add_string( "rc-host", 0, NULL, HOST_TEXT, HOST_LONGTEXT, VLC_TRUE );

#ifdef WIN32
    add_bool( "rc-quiet", 0, NULL, QUIET_TEXT, QUIET_LONGTEXT, VLC_FALSE );
#endif

    set_capability( "interface", 20 );
    set_callbacks( Activate, Deactivate );
vlc_module_end();

/*****************************************************************************
 * Activate: initialize and create stuff
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    char *psz_host, *psz_unix_path;
    int  *pi_socket = NULL;

#if defined(HAVE_ISATTY) && !defined(WIN32)
    /* Check that stdin is a TTY */
    if( !config_GetInt( p_intf, "rc-fake-tty" ) && !isatty( 0 ) )
    {
        msg_Warn( p_intf, "fd 0 is not a TTY" );
        return VLC_EGENERIC;
    }
#endif

    psz_unix_path = config_GetPsz( p_intf, "rc-unix" );
    if( psz_unix_path )
    {
        int i_socket;

#if !defined(AF_LOCAL) || defined(WIN32)
        msg_Warn( p_intf, "your OS doesn't support filesystem sockets" );
        free( psz_unix_path );
        return VLC_EGENERIC;
#else
        struct sockaddr_un addr;
        int i_ret;

        memset( &addr, 0, sizeof(struct sockaddr_un) );

        msg_Dbg( p_intf, "trying UNIX socket" );

        if( (i_socket = socket( AF_LOCAL, SOCK_STREAM, 0 ) ) < 0 )
        {
            msg_Warn( p_intf, "can't open socket: %s", strerror(errno) );
            free( psz_unix_path );
            return VLC_EGENERIC;
        }

        addr.sun_family = AF_LOCAL;
        strncpy( addr.sun_path, psz_unix_path, sizeof( addr.sun_path ) );
        addr.sun_path[sizeof( addr.sun_path ) - 1] = '\0';

        if( (i_ret = bind( i_socket, (struct sockaddr*)&addr,
                           sizeof(struct sockaddr_un) ) ) < 0 )
        {
            msg_Warn( p_intf, "couldn't bind socket to address: %s",
                      strerror(errno) );
            free( psz_unix_path );
            net_Close( i_socket );
            return VLC_EGENERIC;
        }

        if( ( i_ret = listen( i_socket, 1 ) ) < 0 )
        {
            msg_Warn( p_intf, "can't listen on socket: %s", strerror(errno));
            free( psz_unix_path );
            net_Close( i_socket );
            return VLC_EGENERIC;
        }

        /* FIXME: we need a core function to merge listening sockets sets */
        pi_socket = calloc( 2, sizeof( int ) );
        if( pi_socket == NULL )
        {
            free( psz_unix_path );
            net_Close( i_socket );
            return VLC_ENOMEM;
        }
        pi_socket[0] = i_socket;
        pi_socket[1] = -1;
#endif
    }

    if( ( pi_socket == NULL ) &&
        ( psz_host = config_GetPsz( p_intf, "rc-host" ) ) != NULL )
    {
        vlc_url_t url;

        vlc_UrlParse( &url, psz_host, 0 );

        msg_Dbg( p_intf, "base: %s, port: %d", url.psz_host, url.i_port );

        pi_socket = net_ListenTCP(p_this, url.psz_host, url.i_port);
        if( pi_socket == NULL )
        {
            msg_Warn( p_intf, "can't listen to %s port %i",
                      url.psz_host, url.i_port );
            vlc_UrlClean( &url );
            free( psz_host );
            return VLC_EGENERIC;
        }

        vlc_UrlClean( &url );
        free( psz_host );
    }

    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( !p_intf->p_sys )
    {
        msg_Err( p_intf, "no memory" );
        return VLC_ENOMEM;
    }

    p_intf->p_sys->pi_socket_listen = pi_socket;
    p_intf->p_sys->i_socket = -1;
    p_intf->p_sys->psz_unix_path = psz_unix_path;
    vlc_mutex_init( p_intf, &p_intf->p_sys->status_lock );
    p_intf->p_sys->i_last_state = PLAYLIST_STOPPED;

    /* Non-buffered stdout */
    setvbuf( stdout, (char *)NULL, _IOLBF, 0 );

    p_intf->pf_run = Run;

#ifdef WIN32
    p_intf->p_sys->b_quiet = config_GetInt( p_intf, "rc-quiet" );
    if( !p_intf->p_sys->b_quiet ) { CONSOLE_INTRO_MSG; }
#else
    CONSOLE_INTRO_MSG;
#endif

    msg_rc( _("Remote control interface initialized. Type `help' for help.") );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: uninitialize and cleanup
 *****************************************************************************/
static void Deactivate( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;

    net_ListenClose( p_intf->p_sys->pi_socket_listen );
    if( p_intf->p_sys->i_socket != -1 )
        net_Close( p_intf->p_sys->i_socket );
    if( p_intf->p_sys->psz_unix_path != NULL )
    {
#if defined(AF_LOCAL) && !defined(WIN32)
        unlink( p_intf->p_sys->psz_unix_path );
#endif
        free( p_intf->p_sys->psz_unix_path );
    }
    vlc_mutex_destroy( &p_intf->p_sys->status_lock );
    free( p_intf->p_sys );
}

/*****************************************************************************
 * RegisterCallbacks: Register callbacks to dynamic variables
 *****************************************************************************/
static void RegisterCallbacks( intf_thread_t *p_intf )
{
    /* Register commands that will be cleaned up upon object destruction */
    var_Create( p_intf, "quit", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "quit", Quit, NULL );
    var_Create( p_intf, "intf", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "intf", Intf, NULL );

    var_Create( p_intf, "add", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "add", Playlist, NULL );
    var_Create( p_intf, "playlist", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "playlist", Playlist, NULL );
    var_Create( p_intf, "play", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "play", Playlist, NULL );
    var_Create( p_intf, "stop", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "stop", Playlist, NULL );
    var_Create( p_intf, "clear", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "clear", Playlist, NULL );
    var_Create( p_intf, "prev", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "prev", Playlist, NULL );
    var_Create( p_intf, "next", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "next", Playlist, NULL );
    var_Create( p_intf, "goto", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "goto", Playlist, NULL );
    var_Create( p_intf, "status", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "status", Playlist, NULL );

    /* marquee on the fly items */
    var_Create( p_intf, "marq-marquee", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "marq-marquee", Other, NULL );
    var_Create( p_intf, "marq-x", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "marq-x", Other, NULL );
    var_Create( p_intf, "marq-y", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "marq-y", Other, NULL );
    var_Create( p_intf, "marq-position", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "marq-position", Other, NULL );
    var_Create( p_intf, "marq-color", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "marq-color", Other, NULL );
    var_Create( p_intf, "marq-opacity", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "marq-opacity", Other, NULL );
    var_Create( p_intf, "marq-timeout", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "marq-timeout", Other, NULL );
    var_Create( p_intf, "marq-size", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "marq-size", Other, NULL );

    var_Create( p_intf, "mosaic-alpha", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "mosaic-alpha", Other, NULL );
    var_Create( p_intf, "mosaic-height", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "mosaic-height", Other, NULL );
    var_Create( p_intf, "mosaic-width", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "mosaic-width", Other, NULL );
    var_Create( p_intf, "mosaic-xoffset", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "mosaic-xoffset", Other, NULL );
    var_Create( p_intf, "mosaic-yoffset", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "mosaic-yoffset", Other, NULL );
    var_Create( p_intf, "mosaic-align", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "mosaic-align", Other, NULL );
    var_Create( p_intf, "mosaic-vborder", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "mosaic-vborder", Other, NULL );
    var_Create( p_intf, "mosaic-hborder", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "mosaic-hborder", Other, NULL );
    var_Create( p_intf, "mosaic-position",
                     VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "mosaic-position", Other, NULL );
    var_Create( p_intf, "mosaic-rows", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "mosaic-rows", Other, NULL );
    var_Create( p_intf, "mosaic-cols", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "mosaic-cols", Other, NULL );
    var_Create( p_intf, "mosaic-order", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "mosaic-order", Other, NULL );
    var_Create( p_intf, "mosaic-keep-aspect-ratio",
                     VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "mosaic-keep-aspect-ratio", Other, NULL );

    /* time on the fly items */
    var_Create( p_intf, "time-format", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "time-format", Other, NULL );
    var_Create( p_intf, "time-x", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "time-x", Other, NULL );
    var_Create( p_intf, "time-y", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "time-y", Other, NULL );
    var_Create( p_intf, "time-position", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "time-position", Other, NULL );
    var_Create( p_intf, "time-color", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "time-color", Other, NULL );
    var_Create( p_intf, "time-opacity", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "time-opacity", Other, NULL );
    var_Create( p_intf, "time-size", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "time-size", Other, NULL );

    /* logo on the fly items */
    var_Create( p_intf, "logo-file", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "logo-file", Other, NULL );
    var_Create( p_intf, "logo-x", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "logo-x", Other, NULL );
    var_Create( p_intf, "logo-y", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "logo-y", Other, NULL );
    var_Create( p_intf, "logo-position", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "logo-position", Other, NULL );
    var_Create( p_intf, "logo-transparency", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "logo-transparency", Other, NULL );

    /* OSD menu commands */
    var_Create( p_intf, "menu", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "menu", Menu, NULL );

    /* DVD commands */
    var_Create( p_intf, "pause", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "pause", Input, NULL );
    var_Create( p_intf, "seek", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "seek", Input, NULL );
    var_Create( p_intf, "title", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "title", Input, NULL );
    var_Create( p_intf, "title_n", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "title_n", Input, NULL );
    var_Create( p_intf, "title_p", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "title_p", Input, NULL );
    var_Create( p_intf, "chapter", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "chapter", Input, NULL );
    var_Create( p_intf, "chapter_n", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "chapter_n", Input, NULL );
    var_Create( p_intf, "chapter_p", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "chapter_p", Input, NULL );

    var_Create( p_intf, "fastforward", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "fastforward", Input, NULL );
    var_Create( p_intf, "rewind", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "rewind", Input, NULL );
    var_Create( p_intf, "faster", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "faster", Input, NULL );
    var_Create( p_intf, "slower", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "slower", Input, NULL );
    var_Create( p_intf, "normal", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "normal", Input, NULL );

    /* audio commands */
    var_Create( p_intf, "volume", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "volume", Volume, NULL );
    var_Create( p_intf, "volup", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "volup", VolumeMove, NULL );
    var_Create( p_intf, "voldown", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "voldown", VolumeMove, NULL );
    var_Create( p_intf, "adev", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "adev", AudioConfig, NULL );
    var_Create( p_intf, "achan", VLC_VAR_STRING | VLC_VAR_ISCOMMAND );
    var_AddCallback( p_intf, "achan", AudioConfig, NULL );
}

/*****************************************************************************
 * Run: rc thread
 *****************************************************************************
 * This part of the interface is in a separate thread so that we can call
 * exec() from within it without annoying the rest of the program.
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    input_thread_t * p_input;
    playlist_t *     p_playlist;

    char       p_buffer[ MAX_LINE_LENGTH + 1 ];
    vlc_bool_t b_showpos = config_GetInt( p_intf, "rc-show-pos" );
    vlc_bool_t b_longhelp = VLC_FALSE;

    int        i_size = 0;
    int        i_oldpos = 0;
    int        i_newpos;

    p_buffer[0] = 0;
    p_input = NULL;
    p_playlist = NULL;

    /* Register commands that will be cleaned up upon object destruction */
    RegisterCallbacks( p_intf );

    /* status callbacks */
    /* Listen to audio volume updates */
    var_AddCallback( p_intf->p_vlc, "volume-change", VolumeChanged, p_intf );

#ifdef WIN32
    /* Get the file descriptor of the console input */
    p_intf->p_sys->hConsoleIn = GetStdHandle(STD_INPUT_HANDLE);
    if( p_intf->p_sys->hConsoleIn == INVALID_HANDLE_VALUE )
    {
        msg_Err( p_intf, "couldn't find user input handle" );
        p_intf->b_die = VLC_TRUE;
    }
#endif

    while( !p_intf->b_die )
    {
        char *psz_cmd, *psz_arg;
        vlc_bool_t b_complete;

        if( p_intf->p_sys->pi_socket_listen != NULL &&
            p_intf->p_sys->i_socket == -1 )
        {
            p_intf->p_sys->i_socket =
                net_Accept( p_intf, p_intf->p_sys->pi_socket_listen, 0 );
        }

        b_complete = ReadCommand( p_intf, p_buffer, &i_size );

        /* Manage the input part */
        if( p_input == NULL )
        {
            if( p_playlist )
            {
                p_input = vlc_object_find( p_playlist, VLC_OBJECT_INPUT,
                                                       FIND_CHILD );
            }
            else
            {
                p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                   FIND_ANYWHERE );
                if( p_input )
                {
                    p_playlist = vlc_object_find( p_input, VLC_OBJECT_PLAYLIST,
                                                           FIND_PARENT );
                }
            }
            /* New input has been registered */
            if( p_input )
            {
                if( !p_input->b_dead || !p_input->b_die )
                {
                    msg_rc( STATUS_CHANGE "( new input: %s )", p_input->input.p_item->psz_uri );
                    msg_rc( STATUS_CHANGE "( audio volume: %d )", config_GetInt( p_intf, "volume" ));
                }
                var_AddCallback( p_input, "state", StateChanged, p_intf );
                var_AddCallback( p_input, "rate-faster", RateChanged, p_intf );
                var_AddCallback( p_input, "rate-slower", RateChanged, p_intf );
                var_AddCallback( p_input, "rate", RateChanged, p_intf );
                var_AddCallback( p_input, "time-offset", TimeOffsetChanged, p_intf );
            }
        }
        else if( p_input->b_dead )
        {
            var_DelCallback( p_input, "state", StateChanged, p_intf );
            var_DelCallback( p_input, "rate-faster", RateChanged, p_intf );
            var_DelCallback( p_input, "rate-slower", RateChanged, p_intf );
            var_DelCallback( p_input, "rate", RateChanged, p_intf );
            var_DelCallback( p_input, "time-offset", TimeOffsetChanged, p_intf );
            vlc_object_release( p_input );
            p_input = NULL;

            if( p_playlist )
            {
                vlc_mutex_lock( &p_playlist->object_lock );
                p_intf->p_sys->i_last_state = (int) PLAYLIST_STOPPED;
                msg_rc( STATUS_CHANGE "( stop state: 0 )" );
                vlc_mutex_unlock( &p_playlist->object_lock );
            }
        }

        if( (p_input != NULL) && !p_input->b_dead && !p_input->b_die &&
            (p_playlist != NULL) )
        {
            vlc_mutex_lock( &p_playlist->object_lock );
            if( (p_intf->p_sys->i_last_state != p_playlist->status.i_status) &&
                (p_playlist->status.i_status == PLAYLIST_STOPPED) )
            {
                p_intf->p_sys->i_last_state = PLAYLIST_STOPPED;
                msg_rc( STATUS_CHANGE "( stop state: 0 )" );
            }
            else if( (p_intf->p_sys->i_last_state != p_playlist->status.i_status) &&
                (p_playlist->status.i_status == PLAYLIST_RUNNING) )
            {
                p_intf->p_sys->i_last_state = p_playlist->status.i_status;
                msg_rc( STATUS_CHANGE "( play state: 1 )" );
            }
            else if( (p_intf->p_sys->i_last_state != p_playlist->status.i_status) &&
                (p_playlist->status.i_status == PLAYLIST_PAUSED) )
            {
                p_intf->p_sys->i_last_state = p_playlist->status.i_status;
                msg_rc( STATUS_CHANGE "( pause state: 2 )" );
            }
            vlc_mutex_unlock( &p_playlist->object_lock );
        }

        if( p_input && b_showpos )
        {
            i_newpos = 100 * var_GetFloat( p_input, "position" );
            if( i_oldpos != i_newpos )
            {
                i_oldpos = i_newpos;
                msg_rc( "pos: %d%%", i_newpos );
            }
        }

        /* Is there something to do? */
        if( !b_complete ) continue;


        /* Skip heading spaces */
        psz_cmd = p_buffer;
        while( *psz_cmd == ' ' )
        {
            psz_cmd++;
        }

        /* Split psz_cmd at the first space and make sure that
         * psz_arg is valid */
        psz_arg = strchr( psz_cmd, ' ' );
        if( psz_arg )
        {
            *psz_arg++ = 0;
            while( *psz_arg == ' ' )
            {
                psz_arg++;
            }
        }
        else
        {
            psz_arg = "";
        }

        /* If the user typed a registered local command, try it */
        if( var_Type( p_intf, psz_cmd ) & VLC_VAR_ISCOMMAND )
        {
            vlc_value_t val;
            int i_ret;

            val.psz_string = psz_arg;
            i_ret = var_Set( p_intf, psz_cmd, val );
            msg_rc( "%s: returned %i (%s)",
                    psz_cmd, i_ret, vlc_error( i_ret ) );
        }
        /* Or maybe it's a global command */
        else if( var_Type( p_intf->p_libvlc, psz_cmd ) & VLC_VAR_ISCOMMAND )
        {
            vlc_value_t val;
            int i_ret;

            val.psz_string = psz_arg;
            /* FIXME: it's a global command, but we should pass the
             * local object as an argument, not p_intf->p_libvlc. */
            i_ret = var_Set( p_intf->p_libvlc, psz_cmd, val );
            if( i_ret != 0 )
            {
                msg_rc( "%s: returned %i (%s)",
                         psz_cmd, i_ret, vlc_error( i_ret ) );
            }
        }
        else if( !strcmp( psz_cmd, "logout" ) )
        {
            /* Close connection */
            if( p_intf->p_sys->i_socket != -1 )
            {
                net_Close( p_intf->p_sys->i_socket );
            }
            p_intf->p_sys->i_socket = -1;
        }
        else if( !strcmp( psz_cmd, "info" ) )
        {
            if( p_input )
            {
                int i, j;
                vlc_mutex_lock( &p_input->input.p_item->lock );
                for ( i = 0; i < p_input->input.p_item->i_categories; i++ )
                {
                    info_category_t *p_category =
                        p_input->input.p_item->pp_categories[i];

                    msg_rc( "+----[ %s ]", p_category->psz_name );
                    msg_rc( "| " );
                    for ( j = 0; j < p_category->i_infos; j++ )
                    {
                        info_t *p_info = p_category->pp_infos[j];
                        msg_rc( "| %s: %s", p_info->psz_name,
                                p_info->psz_value );
                    }
                    msg_rc( "| " );
                }
                msg_rc( "+----[ end of stream info ]" );
                vlc_mutex_unlock( &p_input->input.p_item->lock );
            }
            else
            {
                msg_rc( "no input" );
            }
        }
        else if( !strcmp( psz_cmd, "is_playing" ) )
        {
            if( ! p_input )
            {
                msg_rc( "0" );
            }
            else
            {
                msg_rc( "1" );
            }
        }
        else if( !strcmp( psz_cmd, "get_time" ) )
        {
            if( ! p_input )
            {
                msg_rc("0");
            }
            else
            {
                vlc_value_t time;
                var_Get( p_input, "time", &time );
                msg_rc( "%i", time.i_time / 1000000);
            }
        }
        else if( !strcmp( psz_cmd, "get_length" ) )
        {
            if( ! p_input )
            {
                msg_rc("0");
            }
            else
            {
                vlc_value_t time;
                var_Get( p_input, "length", &time );
                msg_rc( "%i", time.i_time / 1000000);
            }
        }
        else if( !strcmp( psz_cmd, "get_title" ) )
        {
            if( ! p_input )
            {
                msg_rc("");
            }
            else
            {
                msg_rc( "%s", p_input->input.p_item->psz_name );
            }
        }
        else if( !strcmp( psz_cmd, "longhelp" ) || !strncmp( psz_cmd, "h", 1 )
                 || !strncmp( psz_cmd, "H", 1 ) || !strncmp( psz_cmd, "?", 1 ) )
        {
            if( !strcmp( psz_cmd, "longhelp" ) || !strncmp( psz_cmd, "H", 1 ) )
                 b_longhelp = VLC_TRUE;
            else b_longhelp = VLC_FALSE;

            Help( p_intf, b_longhelp );
        }
        else if( !strcmp( psz_cmd, "check-updates" ) )
        {
            checkUpdates( p_intf, psz_arg );
        }
        else switch( psz_cmd[0] )
        {
        case 'f':
        case 'F':
            if( p_input )
            {
                vout_thread_t *p_vout;
                p_vout = vlc_object_find( p_input,
                                          VLC_OBJECT_VOUT, FIND_CHILD );

                if( p_vout )
                {
                    vlc_value_t val;
                    vlc_bool_t b_update = VLC_FALSE;
                    var_Get( p_vout, "fullscreen", &val );
                    val.b_bool = !val.b_bool;
                    if( !strncmp(psz_arg, "on", 2) && (val.b_bool == VLC_TRUE) )
                    {
                        b_update = VLC_TRUE;
                        val.b_bool = VLC_TRUE;
                    }
                    else if( !strncmp(psz_arg, "off", 3)  && (val.b_bool == VLC_FALSE) )
                    {
                        b_update = VLC_TRUE;
                        val.b_bool = VLC_FALSE;
                    }
                    else if( strncmp(psz_arg, "off", 3) && strncmp(psz_arg, "on", 2) )
                        b_update = VLC_TRUE;
                    if( b_update ) var_Set( p_vout, "fullscreen", val );
                    vlc_object_release( p_vout );
                }
            }
            break;

        case 's':
        case 'S':
            ;
            break;

        case '\0':
            /* Ignore empty lines */
            break;

        default:
            msg_rc(_("Unknown command `%s'. Type `help' for help."), psz_cmd);
            break;
        }

        /* Command processed */
        i_size = 0; p_buffer[0] = 0;
    }

    msg_rc( STATUS_CHANGE "( stop state: 0 )" );
    msg_rc( STATUS_CHANGE "( quit )" );

    if( p_input )
    {
        var_DelCallback( p_input, "state", StateChanged, p_intf );
        var_DelCallback( p_input, "rate-faster", RateChanged, p_intf );
        var_DelCallback( p_input, "rate-slower", RateChanged, p_intf );
        var_DelCallback( p_input, "rate", RateChanged, p_intf );
        var_DelCallback( p_input, "time-offset", TimeOffsetChanged, p_intf );
        vlc_object_release( p_input );
        p_input = NULL;
    }

    if( p_playlist )
    {
        vlc_object_release( p_playlist );
        p_playlist = NULL;
    }

    var_DelCallback( p_intf->p_vlc, "volume-change", VolumeChanged, p_intf );
}

static void Help( intf_thread_t *p_intf, vlc_bool_t b_longhelp)
{
    msg_rc(_("+----[ Remote control commands ]"));
    msg_rc(  "| ");
    msg_rc(_("| add XYZ  . . . . . . . . . . add XYZ to playlist"));
    msg_rc(_("| playlist . . .  show items currently in playlist"));
    msg_rc(_("| play . . . . . . . . . . . . . . . . play stream"));
    msg_rc(_("| stop . . . . . . . . . . . . . . . . stop stream"));
    msg_rc(_("| next . . . . . . . . . . . .  next playlist item"));
    msg_rc(_("| prev . . . . . . . . . .  previous playlist item"));
    msg_rc(_("| goto . . . . . . . . . . . .  goto item at index"));
    msg_rc(_("| clear . . . . . . . . . . .   clear the playlist"));
    msg_rc(_("| status . . . . . . . . . current playlist status"));
    msg_rc(_("| title [X]  . . . . set/get title in current item"));
    msg_rc(_("| title_n  . . . . . .  next title in current item"));
    msg_rc(_("| title_p  . . . .  previous title in current item"));
    msg_rc(_("| chapter [X]  . . set/get chapter in current item"));
    msg_rc(_("| chapter_n  . . . .  next chapter in current item"));
    msg_rc(_("| chapter_p  . .  previous chapter in current item"));
    msg_rc(  "| ");
    msg_rc(_("| seek X . seek in seconds, for instance `seek 12'"));
    msg_rc(_("| pause  . . . . . . . . . . . . . .  toggle pause"));
    msg_rc(_("| fastforward  . . . . . .  .  set to maximum rate"));
    msg_rc(_("| rewind  . . . . . . . . . .  set to minimum rate"));
    msg_rc(_("| faster . . . . . . . .  faster playing of stream"));
    msg_rc(_("| slower . . . . . . . .  slower playing of stream"));
    msg_rc(_("| normal . . . . . . . .  normal playing of stream"));
    msg_rc(_("| f [on|off] . . . . . . . . . . toggle fullscreen"));
    msg_rc(_("| info . . .  information about the current stream"));
    msg_rc(_("| get_time . . seconds elapsed since stream's beginning"));
    msg_rc(_("| is_playing . .  1 if a stream plays, 0 otherwise"));
    msg_rc(_("| get_title . . .  the title of the current stream"));
    msg_rc(_("| get_length . .  the length of the current stream"));
    msg_rc(  "| ");
    msg_rc(_("| volume [X] . . . . . . . .  set/get audio volume"));
    msg_rc(_("| volup [X]  . . . . .  raise audio volume X steps"));
    msg_rc(_("| voldown [X]  . . . .  lower audio volume X steps"));
    msg_rc(_("| adev [X] . . . . . . . . .  set/get audio device"));
    msg_rc(_("| achan [X]. . . . . . . .  set/get audio channels"));
    msg_rc(_("| menu [on|off|up|down|left|right|select] use menu"));
    msg_rc(  "| ");

    if (b_longhelp)
    {
        msg_rc(_("| marq-marquee STRING  . . overlay STRING in video"));
        msg_rc(_("| marq-x X . . . . . . . . . . . .offset from left"));
        msg_rc(_("| marq-y Y . . . . . . . . . . . . offset from top"));
        msg_rc(_("| marq-position #. . .  .relative position control"));
        msg_rc(_("| marq-color # . . . . . . . . . . font color, RGB"));
        msg_rc(_("| marq-opacity # . . . . . . . . . . . . . opacity"));
        msg_rc(_("| marq-timeout T. . . . . . . . . . timeout, in ms"));
        msg_rc(_("| marq-size # . . . . . . . . font size, in pixels"));
        msg_rc(  "| ");
        msg_rc(_("| time-format STRING . . . overlay STRING in video"));
        msg_rc(_("| time-x X . . . . . . . . . . . .offset from left"));
        msg_rc(_("| time-y Y . . . . . . . . . . . . offset from top"));
        msg_rc(_("| time-position #. . . . . . . . relative position"));
        msg_rc(_("| time-color # . . . . . . . . . . font color, RGB"));
        msg_rc(_("| time-opacity # . . . . . . . . . . . . . opacity"));
        msg_rc(_("| time-size # . . . . . . . . font size, in pixels"));
        msg_rc(  "| ");
        msg_rc(_("| logo-file STRING . . .the overlay file path/name"));
        msg_rc(_("| logo-x X . . . . . . . . . . . .offset from left"));
        msg_rc(_("| logo-y Y . . . . . . . . . . . . offset from top"));
        msg_rc(_("| logo-position #. . . . . . . . relative position"));
        msg_rc(_("| logo-transparency #. . . . . . . . .transparency"));
        msg_rc(  "| ");
        msg_rc(_("| mosaic-alpha # . . . . . . . . . . . . . . alpha"));
        msg_rc(_("| mosaic-height #. . . . . . . . . . . . . .height"));
        msg_rc(_("| mosaic-width # . . . . . . . . . . . . . . width"));
        msg_rc(_("| mosaic-xoffset # . . . .top left corner position"));
        msg_rc(_("| mosaic-yoffset # . . . .top left corner position"));
        msg_rc(_("| mosaic-align 0..2,4..6,8..10. . .mosaic alignment"));
        msg_rc(_("| mosaic-vborder # . . . . . . . . vertical border"));
        msg_rc(_("| mosaic-hborder # . . . . . . . horizontal border"));
        msg_rc(_("| mosaic-position {0=auto,1=fixed} . . . .position"));
        msg_rc(_("| mosaic-rows #. . . . . . . . . . .number of rows"));
        msg_rc(_("| mosaic-cols #. . . . . . . . . . .number of cols"));
        msg_rc(_("| mosaic-order id(,id)* . . . . order of pictures "));
        msg_rc(_("| mosaic-keep-aspect-ratio {0,1} . . .aspect ratio"));
        msg_rc(  "| ");
        msg_rc(_("| check-updates [newer] [equal] [older]\n"
                 "|               [undef] [info] [source] [binary] [plugin]"));
        msg_rc(  "| ");
    }
    msg_rc(_("| help . . . . . . . . . . . . . this help message"));
    msg_rc(_("| longhelp . . . . . . . . . a longer help message"));
    msg_rc(_("| logout . . . . .  exit (if in socket connection)"));
    msg_rc(_("| quit . . . . . . . . . . . . . . . . .  quit vlc"));
    msg_rc(  "| ");
    msg_rc(_("+----[ end of help ]"));
}

/********************************************************************
 * Status callback routines
 ********************************************************************/
static int TimeOffsetChanged( vlc_object_t *p_this, char const *psz_cmd,
    vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_data;
    input_thread_t *p_input = NULL;

    vlc_mutex_lock( &p_intf->p_sys->status_lock );
    p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    if( p_input )
    {
        msg_rc( STATUS_CHANGE "( time-offset: %d )", var_GetInteger( p_input, "time-offset" ) );
        vlc_object_release( p_input );
    }
    vlc_mutex_unlock( &p_intf->p_sys->status_lock );
    return VLC_SUCCESS;
}

static int VolumeChanged( vlc_object_t *p_this, char const *psz_cmd,
    vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_data;

    vlc_mutex_lock( &p_intf->p_sys->status_lock );
    msg_rc( STATUS_CHANGE "( audio volume: %d )", config_GetInt( p_this, "volume") );
    vlc_mutex_unlock( &p_intf->p_sys->status_lock );
    return VLC_SUCCESS;
}

static int StateChanged( vlc_object_t *p_this, char const *psz_cmd,
    vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_data;
    playlist_t    *p_playlist = NULL;
    input_thread_t *p_input = NULL;

    vlc_mutex_lock( &p_intf->p_sys->status_lock );
    p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    if( p_input )
    {
        p_playlist = vlc_object_find( p_input, VLC_OBJECT_PLAYLIST, FIND_PARENT );
        if( p_playlist )
        {
            char cmd[5] = "";
            switch( p_playlist->status.i_status )
            {
            case PLAYLIST_STOPPED:
                strncpy( &cmd[0], "stop", 4);
                cmd[4] = '\0';
                break;
            case PLAYLIST_RUNNING:
                strncpy( &cmd[0], "play", 4);
                cmd[4] = '\0';
                break;
            case PLAYLIST_PAUSED:
                strncpy( &cmd[0], "pause", 5);
                cmd[5] = '\0';
                break;
            } /* var_GetInteger( p_input, "state" )  */
            msg_rc( STATUS_CHANGE "( %s state: %d )", &cmd[0], newval.i_int );
            vlc_object_release( p_playlist );
        }
        vlc_object_release( p_input );
    }
    vlc_mutex_unlock( &p_intf->p_sys->status_lock );
    return VLC_SUCCESS;
}

static int RateChanged( vlc_object_t *p_this, char const *psz_cmd,
    vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_data;
    input_thread_t *p_input = NULL;

    vlc_mutex_lock( &p_intf->p_sys->status_lock );
    p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    if( p_input )
    {
        msg_rc( STATUS_CHANGE "( new rate: %d )", var_GetInteger( p_input, "rate" ) );
        vlc_object_release( p_input );
    }
    vlc_mutex_unlock( &p_intf->p_sys->status_lock );
    return VLC_SUCCESS;
}

/********************************************************************
 * Command routines
 ********************************************************************/
static int Input( vlc_object_t *p_this, char const *psz_cmd,
                  vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    input_thread_t *p_input;
    vlc_value_t     val;

    p_input = vlc_object_find( p_this, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    if( !p_input ) return VLC_ENOOBJ;

    var_Get( p_input, "state", &val );
    if( ( ( val.i_int == PAUSE_S ) || ( val.i_int == PLAYLIST_PAUSED ) ) &&
        ( strcmp( psz_cmd, "pause" ) != 0 ) )
    {
        msg_rc( _("Press menu select or pause to continue.") );
        vlc_object_release( p_input );
        return VLC_EGENERIC;
    }

    /* Parse commands that only require an input */
    if( !strcmp( psz_cmd, "pause" ) )
    {
        val.i_int = config_GetInt( p_intf, "key-play-pause" );
        var_Set( p_intf->p_vlc, "key-pressed", val );
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }
    else if( !strcmp( psz_cmd, "seek" ) )
    {
        if( strlen( newval.psz_string ) > 0 &&
            newval.psz_string[strlen( newval.psz_string ) - 1] == '%' )
        {
            val.f_float = (float)atoi( newval.psz_string ) / 100.0;
            var_Set( p_input, "position", val );
        }
        else
        {
            val.i_time = ((int64_t)atoi( newval.psz_string )) * 1000000;
            var_Set( p_input, "time", val );
        }
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }
    else if ( !strcmp( psz_cmd, "fastforward" ) )
    {
        val.i_int = config_GetInt( p_intf, "key-jump+extrashort" );
        var_Set( p_intf->p_vlc, "key-pressed", val );
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }
    else if ( !strcmp( psz_cmd, "rewind" ) )
    {
        val.i_int = config_GetInt( p_intf, "key-jump-extrashort" );
        var_Set( p_intf->p_vlc, "key-pressed", val );
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }
    else if ( !strcmp( psz_cmd, "faster" ) )
    {
        val.b_bool = VLC_TRUE;
        var_Set( p_input, "rate-faster", val );
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }
    else if ( !strcmp( psz_cmd, "slower" ) )
    {
        val.b_bool = VLC_TRUE;
        var_Set( p_input, "rate-slower", val );
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }
    else if ( !strcmp( psz_cmd, "normal" ) )
    {
        val.i_int = INPUT_RATE_DEFAULT;
        var_Set( p_input, "rate", val );
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }
    else if( !strcmp( psz_cmd, "chapter" ) ||
             !strcmp( psz_cmd, "chapter_n" ) ||
             !strcmp( psz_cmd, "chapter_p" ) )
    {
        if( !strcmp( psz_cmd, "chapter" ) )
        {
            if ( *newval.psz_string )
            {
                /* Set. */
                val.i_int = atoi( newval.psz_string );
                var_Set( p_input, "chapter", val );
            }
            else
            {
                vlc_value_t val_list;

                /* Get. */
                var_Get( p_input, "chapter", &val );
                var_Change( p_input, "chapter", VLC_VAR_GETCHOICES,
                            &val_list, NULL );
                msg_rc( "Currently playing chapter %d/%d.",
                        val.i_int, val_list.p_list->i_count );
                var_Change( p_this, "chapter", VLC_VAR_FREELIST,
                            &val_list, NULL );
            }
        }
        else if( !strcmp( psz_cmd, "chapter_n" ) )
        {
            val.b_bool = VLC_TRUE;
            var_Set( p_input, "next-chapter", val );
        }
        else if( !strcmp( psz_cmd, "chapter_p" ) )
        {
            val.b_bool = VLC_TRUE;
            var_Set( p_input, "prev-chapter", val );
        }
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }
    else if( !strcmp( psz_cmd, "title" ) ||
             !strcmp( psz_cmd, "title_n" ) ||
             !strcmp( psz_cmd, "title_p" ) )
    {
        if( !strcmp( psz_cmd, "title" ) )
        {
            if ( *newval.psz_string )
            {
                /* Set. */
                val.i_int = atoi( newval.psz_string );
                var_Set( p_input, "title", val );
            }
            else
            {
                vlc_value_t val_list;

                /* Get. */
                var_Get( p_input, "title", &val );
                var_Change( p_input, "title", VLC_VAR_GETCHOICES,
                            &val_list, NULL );
                msg_rc( "Currently playing title %d/%d.",
                        val.i_int, val_list.p_list->i_count );
                var_Change( p_this, "title", VLC_VAR_FREELIST,
                            &val_list, NULL );
            }
        }
        else if( !strcmp( psz_cmd, "title_n" ) )
        {
            val.b_bool = VLC_TRUE;
            var_Set( p_input, "next-title", val );
        }
        else if( !strcmp( psz_cmd, "title_p" ) )
        {
            val.b_bool = VLC_TRUE;
            var_Set( p_input, "prev-title", val );
        }

        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }

    /* Never reached. */
    vlc_object_release( p_input );
    return VLC_EGENERIC;
}

static int Playlist( vlc_object_t *p_this, char const *psz_cmd,
                     vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vlc_value_t val;
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    playlist_t *p_playlist;

    p_playlist = vlc_object_find( p_this, VLC_OBJECT_PLAYLIST,
                                           FIND_ANYWHERE );
    if( !p_playlist )
    {
        msg_Err( p_this, "no playlist" );
        return VLC_ENOOBJ;
    }

    if( p_playlist->p_input )
    {
        vlc_value_t val;
        var_Get( p_playlist->p_input, "state", &val );
        if( ( val.i_int == PAUSE_S ) || ( val.i_int == PLAYLIST_PAUSED ) )
        {
            msg_rc( _("Type 'menu select' or 'pause' to continue.") );
            vlc_object_release( p_playlist );
            return VLC_EGENERIC;
        }
    }

    /* Parse commands that require a playlist */
    if( !strcmp( psz_cmd, "prev" ) )
    {
        playlist_Prev( p_playlist );
    }
    else if( !strcmp( psz_cmd, "next" ) )
    {
        playlist_Next( p_playlist );
    }
    else if( !strcmp( psz_cmd, "play" ) )
    {
        msg_Warn( p_playlist, "play" );
        playlist_Play( p_playlist );
    }
    else if (!strcmp( psz_cmd, "goto" ) )
    {
        msg_Err( p_playlist, "goto is deprecated" );
    }
    else if( !strcmp( psz_cmd, "stop" ) )
    {
        playlist_Stop( p_playlist );
    }
    else if( !strcmp( psz_cmd, "clear" ) )
    {
        playlist_Stop( p_playlist );
        vlc_mutex_lock( &p_playlist->object_lock );
        playlist_Clear( p_playlist );
        vlc_mutex_unlock( &p_playlist->object_lock );
    }
    else if( !strcmp( psz_cmd, "add" ) &&
             newval.psz_string && *newval.psz_string )
    {
        playlist_item_t *p_item = parse_MRL( p_intf, newval.psz_string );

        if( p_item )
        {
            msg_rc( "Trying to add %s to playlist.", newval.psz_string );
//            playlist_AddItem( p_playlist, p_item,
//                              PLAYLIST_GO|PLAYLIST_APPEND, PLAYLIST_END );
        }
    }
    else if( !strcmp( psz_cmd, "playlist" ) )
    {
        int i;
        playlist_view_t *p_view;
        playlist_NodeDump( p_playlist, p_playlist->p_root_category, 0 );
        playlist_NodeDump( p_playlist, p_playlist->p_root_onelevel, 0 );
    }
    else if( !strcmp( psz_cmd, "status" ) )
    {
        if( p_playlist->p_input )
        {
            /* Replay the current state of the system. */
            msg_rc( STATUS_CHANGE "( new input: %s )", p_playlist->p_input->input.p_item->psz_uri );
            msg_rc( STATUS_CHANGE "( audio volume: %d )", config_GetInt( p_intf, "volume" ));

            vlc_mutex_lock( &p_playlist->object_lock );
            switch( p_playlist->status.i_status )
            {
                case PLAYLIST_STOPPED:
                    msg_rc( STATUS_CHANGE "( stop state: 0 )" );
                    break;
                case PLAYLIST_RUNNING:
                    msg_rc( STATUS_CHANGE "( play state: 1 )" );
                    break;
                case PLAYLIST_PAUSED:
                    msg_rc( STATUS_CHANGE "( pause state: 2 )" );
                    break;
                default:
                    msg_rc( STATUS_CHANGE "( state unknown )" );
                    break;
            }
            vlc_mutex_unlock( &p_playlist->object_lock );
        }
    }

    /*
     * sanity check
     */
    else
    {
        msg_rc( "unknown command!" );
    }

    vlc_object_release( p_playlist );
    return VLC_SUCCESS;
}

static int Other( vlc_object_t *p_this, char const *psz_cmd,
                     vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    vlc_object_t  *p_playlist;
    vlc_value_t    val;
    vlc_object_t  *p_input;

    p_playlist = vlc_object_find( p_this, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( !p_playlist )
    {
        return VLC_ENOOBJ;
    }

    p_input = vlc_object_find( p_this, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    if( !p_input )
    {
        vlc_object_release( p_playlist );
        return VLC_ENOOBJ;
    }

    if( p_input )
    {
        var_Get( p_input, "state", &val );
        if( ( val.i_int == PAUSE_S ) || ( val.i_int == PLAYLIST_PAUSED ) )
        {
            msg_rc( _("Type 'pause' to continue.") );
            vlc_object_release( p_playlist );
            vlc_object_release( p_input );
            return VLC_EGENERIC;
        }
    }

    /* Parse miscellaneous commands */
    if( !strcmp( psz_cmd, "marq-marquee" ) )
    {
        if( strlen( newval.psz_string ) > 0 )
        {
            val.psz_string = newval.psz_string;
            var_Set( p_input->p_libvlc, "marq-marquee", val );
        }
        else
        {
                val.psz_string = "";
                var_Set( p_input->p_libvlc, "marq-marquee", val);
        }
    }
    else if( !strcmp( psz_cmd, "marq-x" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "marq-x", val );
        }
    }
    else if( !strcmp( psz_cmd, "marq-y" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "marq-y", val );
        }
    }
    else if( !strcmp( psz_cmd, "marq-position" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "marq-position", val );
        }
    }
    else if( !strcmp( psz_cmd, "marq-color" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = strtol( newval.psz_string, NULL, 0 );
            var_Set( p_input->p_libvlc, "marq-color", val );
        }
    }
    else if( !strcmp( psz_cmd, "marq-opacity" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = strtol( newval.psz_string, NULL, 0 );
            var_Set( p_input->p_libvlc, "marq-opacity", val );
        }
    }
    else if( !strcmp( psz_cmd, "marq-size" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "marq-size", val );
        }
    }
    else if( !strcmp( psz_cmd, "marq-timeout" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input, "marq-timeout", val );
        }
    }
    else if( !strcmp( psz_cmd, "mosaic-alpha" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "mosaic-alpha", val );
        }
    }
    else if( !strcmp( psz_cmd, "mosaic-height" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "mosaic-height", val );
        }
    }
    else if( !strcmp( psz_cmd, "mosaic-width" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "mosaic-width", val );
        }
    }
    else if( !strcmp( psz_cmd, "mosaic-xoffset" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "mosaic-xoffset", val );
        }
    }
    else if( !strcmp( psz_cmd, "mosaic-yoffset" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "mosaic-yoffset", val );
        }
    }
    else if( !strcmp( psz_cmd, "mosaic-align" ) )
    {
        if( strlen( newval.psz_string ) > 0 )
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "mosaic-align", val );
        }
    }
    else if( !strcmp( psz_cmd, "mosaic-vborder" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "mosaic-vborder", val );
        }
    }
    else if( !strcmp( psz_cmd, "mosaic-hborder" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "mosaic-hborder", val );
        }
    }
    else if( !strcmp( psz_cmd, "mosaic-position" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "mosaic-position", val );
        }
    }
    else if( !strcmp( psz_cmd, "mosaic-rows" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "mosaic-rows", val );
        }
    }
    else if( !strcmp( psz_cmd, "mosaic-cols" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "mosaic-cols", val );
        }
    }
    else if( !strcmp( psz_cmd, "mosaic-order" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.psz_string = newval.psz_string;
            var_Set( p_input->p_libvlc, "mosaic-order", val );
        }
    }
    else if( !strcmp( psz_cmd, "mosaic-keep-aspect-ratio" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "mosaic-keep-aspect-ratio", val );
        }
    }
    else if( !strcmp( psz_cmd, "time-format" ) )
    {
        if( strlen( newval.psz_string ) > 0 )
        {
            val.psz_string = newval.psz_string;
            var_Set( p_input->p_libvlc, "time-format", val );
        }
        else
        {
            val.psz_string = "";
            var_Set( p_input->p_libvlc, "time-format", val);
        }
    }
    else if( !strcmp( psz_cmd, "time-x" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "time-x", val );
        }
    }
    else if( !strcmp( psz_cmd, "time-y" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "time-y", val );
        }
    }
    else if( !strcmp( psz_cmd, "time-position" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "time-position", val );
        }
    }
    else if( !strcmp( psz_cmd, "time-color" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = strtol( newval.psz_string, NULL, 0 );
            var_Set( p_input->p_libvlc, "time-color", val );
        }
    }
    else if( !strcmp( psz_cmd, "time-opacity" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = strtol( newval.psz_string, NULL, 0 );
            var_Set( p_input->p_libvlc, "time-opacity", val );
        }
    }
    else if( !strcmp( psz_cmd, "time-size" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "time-size", val );
        }
    }
    else if( !strcmp( psz_cmd, "logo-file" ) )
    {
        if( strlen( newval.psz_string ) > 0 )
        {
            val.psz_string = newval.psz_string;
            var_Set( p_input->p_libvlc, "logo-file", val );
        }
    }
    else if( !strcmp( psz_cmd, "logo-x" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "logo-x", val );
        }
    }
    else if( !strcmp( psz_cmd, "logo-y" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "logo-y", val );
        }
    }
    else if( !strcmp( psz_cmd, "logo-position" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = atoi( newval.psz_string );
            var_Set( p_input->p_libvlc, "logo-position", val );
        }
    }
    else if( !strcmp( psz_cmd, "logo-transparency" ) )
    {
        if( strlen( newval.psz_string ) > 0)
        {
            val.i_int = strtol( newval.psz_string, NULL, 0 );
            var_Set( p_input->p_libvlc, "logo-transparency", val );
        }
    }

    /*
     * sanity check
     */
    else
    {
        msg_rc( "Unknown command!" );
    }

    vlc_object_release( p_playlist );
    vlc_object_release( p_input );
    return VLC_SUCCESS;
}

static int Quit( vlc_object_t *p_this, char const *psz_cmd,
                 vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    playlist_t *p_playlist;

    p_playlist = vlc_object_find( p_this, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist )
    {
        playlist_Stop( p_playlist );
        vlc_object_release( p_playlist );
    }
    p_this->p_vlc->b_die = VLC_TRUE;
    return VLC_SUCCESS;
}

static int Intf( vlc_object_t *p_this, char const *psz_cmd,
                 vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_newintf = NULL;

    p_newintf = intf_Create( p_this->p_vlc, newval.psz_string, 0, NULL );
    if( p_newintf )
    {
        p_newintf->b_block = VLC_FALSE;
        if( intf_RunThread( p_newintf ) )
        {
            vlc_object_detach( p_newintf );
            intf_Destroy( p_newintf );
        }
    }

    return VLC_SUCCESS;
}

static int Volume( vlc_object_t *p_this, char const *psz_cmd,
                   vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    input_thread_t *p_input = NULL;
    int i_error = VLC_EGENERIC;

    p_input = vlc_object_find( p_this, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    if( !p_input )
        return VLC_ENOOBJ;

    if( p_input )
    {
        vlc_value_t val;

        var_Get( p_input, "state", &val );
        if( ( val.i_int == PAUSE_S ) || ( val.i_int == PLAYLIST_PAUSED ) )
        {
            msg_rc( _("Type 'menu select' or 'pause' to continue.") );
            vlc_object_release( p_input );
            return VLC_EGENERIC;
        }
        vlc_object_release( p_input );
    }

    if ( *newval.psz_string )
    {
        /* Set. */
        audio_volume_t i_volume = atoi( newval.psz_string );
        if ( (i_volume > (audio_volume_t)AOUT_VOLUME_MAX) )
        {
            msg_rc( "Volume must be in the range %d-%d.", AOUT_VOLUME_MIN,
                    AOUT_VOLUME_MAX );
            i_error = VLC_EBADVAR;
        }
        else
        {
            if( i_volume == AOUT_VOLUME_MIN )
            {
                vlc_value_t keyval;

                keyval.i_int = config_GetInt( p_intf, "key-vol-mute" );
                var_Set( p_intf->p_vlc, "key-pressed", keyval );
            }
            i_error = aout_VolumeSet( p_this, i_volume );
            osd_Volume( p_this );
            msg_rc( STATUS_CHANGE "( audio volume: %d )", i_volume );
        }
    }
    else
    {
        /* Get. */
        audio_volume_t i_volume;
        if ( aout_VolumeGet( p_this, &i_volume ) < 0 )
        {
            i_error = VLC_EGENERIC;
        }
        else
        {
            msg_rc( STATUS_CHANGE "( audio volume: %d )", i_volume );
            i_error = VLC_SUCCESS;
        }
    }

    return i_error;
}

static int VolumeMove( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    audio_volume_t i_volume;
    input_thread_t *p_input = NULL;
    int i_nb_steps = atoi(newval.psz_string);
    int i_error = VLC_SUCCESS;
    int i_volume_step = 0;

    p_input = vlc_object_find( p_this, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    if( !p_input )
        return VLC_ENOOBJ;

    if( p_input )
    {
        vlc_value_t val;

        var_Get( p_input, "state", &val );
        if( ( val.i_int == PAUSE_S ) || ( val.i_int == PLAYLIST_PAUSED ) )
        {
            msg_rc( _("Type 'menu select' or 'pause' to continue.") );
            vlc_object_release( p_input );
            return VLC_EGENERIC;
        }
        vlc_object_release( p_input );
    }

    i_volume_step = config_GetInt( p_intf->p_vlc, "volume-step" );
    if ( i_nb_steps <= 0 || i_nb_steps > (AOUT_VOLUME_MAX/i_volume_step) )
    {
        i_nb_steps = 1;
    }

    if ( !strcmp(psz_cmd, "volup") )
    {
        if ( aout_VolumeUp( p_this, i_nb_steps, &i_volume ) < 0 )
            i_error = VLC_EGENERIC;
    }
    else
    {
        if ( aout_VolumeDown( p_this, i_nb_steps, &i_volume ) < 0 )
            i_error = VLC_EGENERIC;
    }
    osd_Volume( p_this );

    if ( !i_error ) msg_rc( STATUS_CHANGE "( audio volume: %d )", i_volume );
    return i_error;
}

static int AudioConfig( vlc_object_t *p_this, char const *psz_cmd,
                        vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    input_thread_t *p_input = NULL;
    aout_instance_t * p_aout;
    const char * psz_variable;
    vlc_value_t val_name;
    int i_error;

    p_input = vlc_object_find( p_this, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    if( !p_input )
        return VLC_ENOOBJ;

    if( p_input )
    {
        vlc_value_t val;

        var_Get( p_input, "state", &val );
        if( ( val.i_int == PAUSE_S ) || ( val.i_int == PLAYLIST_PAUSED ) )        {
            msg_rc( _("Type 'menu select' or 'pause' to continue.") );
            vlc_object_release( p_input );
            return VLC_EGENERIC;
        }
        vlc_object_release( p_input );
    }

    p_aout = vlc_object_find( p_this, VLC_OBJECT_AOUT, FIND_ANYWHERE );
    if ( p_aout == NULL ) return VLC_ENOOBJ;

    if ( !strcmp( psz_cmd, "adev" ) )
    {
        psz_variable = "audio-device";
    }
    else
    {
        psz_variable = "audio-channels";
    }

    /* Get the descriptive name of the variable */
    var_Change( (vlc_object_t *)p_aout, psz_variable, VLC_VAR_GETTEXT,
                 &val_name, NULL );
    if( !val_name.psz_string ) val_name.psz_string = strdup(psz_variable);

    if ( !*newval.psz_string )
    {
        /* Retrieve all registered ***. */
        vlc_value_t val, text;
        int i, i_value;

        if ( var_Get( (vlc_object_t *)p_aout, psz_variable, &val ) < 0 )
        {
            vlc_object_release( (vlc_object_t *)p_aout );
            return VLC_EGENERIC;
        }
        i_value = val.i_int;

        if ( var_Change( (vlc_object_t *)p_aout, psz_variable,
                         VLC_VAR_GETLIST, &val, &text ) < 0 )
        {
            vlc_object_release( (vlc_object_t *)p_aout );
            return VLC_EGENERIC;
        }

        msg_rc( "+----[ %s ]", val_name.psz_string );
        for ( i = 0; i < val.p_list->i_count; i++ )
        {
            if ( i_value == val.p_list->p_values[i].i_int )
                msg_rc( "| %i - %s *", val.p_list->p_values[i].i_int,
                        text.p_list->p_values[i].psz_string );
            else
                msg_rc( "| %i - %s", val.p_list->p_values[i].i_int,
                        text.p_list->p_values[i].psz_string );
        }
        var_Change( (vlc_object_t *)p_aout, psz_variable, VLC_VAR_FREELIST,
                    &val, &text );
        msg_rc( "+----[ end of %s ]", val_name.psz_string );

        if( val_name.psz_string ) free( val_name.psz_string );
        i_error = VLC_SUCCESS;
    }
    else
    {
        vlc_value_t val;
        val.i_int = atoi( newval.psz_string );

        i_error = var_Set( (vlc_object_t *)p_aout, psz_variable, val );
    }
    vlc_object_release( (vlc_object_t *)p_aout );

    return i_error;
}

/* OSD menu commands */
static int Menu( vlc_object_t *p_this, char const *psz_cmd,
    vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    playlist_t    *p_playlist = NULL;
    vlc_value_t val;
    int i_error = VLC_EGENERIC;

    if ( !*newval.psz_string )
    {
        msg_rc( _("Please provide one of the following parameters:") );
        msg_rc( "[on|off|up|down|left|right|select]" );
        return i_error;
    }

    p_playlist = vlc_object_find( p_this, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( !p_playlist )
        return VLC_ENOOBJ;

    if( p_playlist->p_input )
    {
        var_Get( p_playlist->p_input, "state", &val );
        if( ( ( val.i_int == PAUSE_S ) || ( val.i_int == PLAYLIST_PAUSED ) ) &&
            ( strcmp( newval.psz_string, "select" ) != 0 ) )
        {
            msg_rc( _("Type 'menu select' or 'pause' to continue.") );
            vlc_object_release( p_playlist );
            return VLC_EGENERIC;
        }
    }
    vlc_object_release( p_playlist );

    val.psz_string = strdup( newval.psz_string );
    if( !strcmp( val.psz_string, "on" ) || !strcmp( val.psz_string, "show" ))
        osd_MenuShow( p_this );
    else if( !strcmp( val.psz_string, "off" ) || !strcmp( val.psz_string, "hide" ) )
        osd_MenuHide( p_this );
    else if( !strcmp( val.psz_string, "up" ) )
        osd_MenuUp( p_this );
    else if( !strcmp( val.psz_string, "down" ) )
        osd_MenuDown( p_this );
    else if( !strcmp( val.psz_string, "left" ) )
        osd_MenuPrev( p_this );
    else if( !strcmp( val.psz_string, "right" ) )
        osd_MenuNext( p_this );
    else if( !strcmp( val.psz_string, "select" ) )
        osd_MenuActivate( p_this );
    else
    {
        msg_rc( _("Please provide one of the following parameters:") );
        msg_rc( "[on|off|up|down|left|right|select]" );
        if( val.psz_string ) free( val.psz_string );
            return i_error;
    }

    i_error = VLC_SUCCESS;
    if( val.psz_string ) free( val.psz_string );
    return i_error;
}

#ifdef WIN32
vlc_bool_t ReadWin32( intf_thread_t *p_intf, char *p_buffer, int *pi_size )
{
    INPUT_RECORD input_record;
    DWORD i_dw;

    /* On Win32, select() only works on socket descriptors */
    while( WaitForSingleObject( p_intf->p_sys->hConsoleIn,
                                INTF_IDLE_SLEEP/1000 ) == WAIT_OBJECT_0 )
    {
        while( !p_intf->b_die && *pi_size < MAX_LINE_LENGTH &&
               ReadConsoleInput( p_intf->p_sys->hConsoleIn, &input_record,
                                 1, &i_dw ) )
        {
            if( input_record.EventType != KEY_EVENT ||
                !input_record.Event.KeyEvent.bKeyDown ||
                input_record.Event.KeyEvent.wVirtualKeyCode == VK_SHIFT ||
                input_record.Event.KeyEvent.wVirtualKeyCode == VK_CONTROL||
                input_record.Event.KeyEvent.wVirtualKeyCode == VK_MENU ||
                input_record.Event.KeyEvent.wVirtualKeyCode == VK_CAPITAL )
            {
                /* nothing interesting */
                continue;
            }

            p_buffer[ *pi_size ] = input_record.Event.KeyEvent.uChar.AsciiChar;

            /* Echo out the command */
            putc( p_buffer[ *pi_size ], stdout );

            /* Handle special keys */
            if( p_buffer[ *pi_size ] == '\r' || p_buffer[ *pi_size ] == '\n' )
            {
                putc( '\n', stdout );
                break;
            }
            switch( p_buffer[ *pi_size ] )
            {
            case '\b':
                if( *pi_size )
                {
                    *pi_size -= 2;
                    putc( ' ', stdout );
                    putc( '\b', stdout );
                }
                break;
            case '\r':
                (*pi_size) --;
                break;
            }

            (*pi_size)++;
        }

        if( *pi_size == MAX_LINE_LENGTH ||
            p_buffer[ *pi_size ] == '\r' || p_buffer[ *pi_size ] == '\n' )
        {
            p_buffer[ *pi_size ] = 0;
            return VLC_TRUE;
        }
    }

    return VLC_FALSE;
}
#endif

vlc_bool_t ReadCommand( intf_thread_t *p_intf, char *p_buffer, int *pi_size )
{
    int i_read = 0;

#ifdef WIN32
    if( p_intf->p_sys->i_socket == -1 && !p_intf->p_sys->b_quiet )
        return ReadWin32( p_intf, p_buffer, pi_size );
    else if( p_intf->p_sys->i_socket == -1 )
    {
        msleep( INTF_IDLE_SLEEP );
        return VLC_FALSE;
    }
#endif

    while( !p_intf->b_die && *pi_size < MAX_LINE_LENGTH &&
           (i_read = net_ReadNonBlock( p_intf, p_intf->p_sys->i_socket == -1 ?
                       0 /*STDIN_FILENO*/ : p_intf->p_sys->i_socket, NULL,
                  (uint8_t *)p_buffer + *pi_size, 1, INTF_IDLE_SLEEP ) ) > 0 )
    {
        if( p_buffer[ *pi_size ] == '\r' || p_buffer[ *pi_size ] == '\n' )
            break;

        (*pi_size)++;
    }

    /* Connection closed */
    if( i_read == -1 )
    {
        p_intf->p_sys->i_socket = -1;
        p_buffer[ *pi_size ] = 0;
        return VLC_TRUE;
    }

    if( *pi_size == MAX_LINE_LENGTH ||
        p_buffer[ *pi_size ] == '\r' || p_buffer[ *pi_size ] == '\n' )
    {
        p_buffer[ *pi_size ] = 0;
        return VLC_TRUE;
    }

    return VLC_FALSE;
}

/*****************************************************************************
 * parse_MRL: build a playlist item from a full mrl
 *****************************************************************************
 * MRL format: "simplified-mrl [:option-name[=option-value]]"
 * We don't check for '"' or '\'', we just assume that a ':' that follows a
 * space is a new option. Should be good enough for our purpose.
 *****************************************************************************/
static playlist_item_t *parse_MRL( intf_thread_t *p_intf, char *psz_mrl )
{
#define SKIPSPACE( p ) { while( *p && ( *p == ' ' || *p == '\t' ) ) p++; }
#define SKIPTRAILINGSPACE( p, d ) \
    { char *e=d; while( e > p && (*(e-1)==' ' || *(e-1)=='\t') ){e--;*e=0;} }

    playlist_item_t *p_item = NULL;
    char *psz_item = NULL, *psz_item_mrl = NULL, *psz_orig;
    char **ppsz_options = NULL;
    int i, i_options = 0;

    if( !psz_mrl ) return 0;

    psz_mrl = psz_orig = strdup( psz_mrl );
    while( *psz_mrl )
    {
        SKIPSPACE( psz_mrl );
        psz_item = psz_mrl;

        for( ; *psz_mrl; psz_mrl++ )
        {
            if( (*psz_mrl == ' ' || *psz_mrl == '\t') && psz_mrl[1] == ':' )
            {
                /* We have a complete item */
                break;
            }
            if( (*psz_mrl == ' ' || *psz_mrl == '\t') &&
                (psz_mrl[1] == '"' || psz_mrl[1] == '\'') && psz_mrl[2] == ':')
            {
                /* We have a complete item */
                break;
            }
        }

        if( *psz_mrl ) { *psz_mrl = 0; psz_mrl++; }
        SKIPTRAILINGSPACE( psz_item, psz_item + strlen( psz_item ) );

        /* Remove '"' and '\'' if necessary */
        if( *psz_item == '"' && psz_item[strlen(psz_item)-1] == '"' )
        { psz_item++; psz_item[strlen(psz_item)-1] = 0; }
        if( *psz_item == '\'' && psz_item[strlen(psz_item)-1] == '\'' )
        { psz_item++; psz_item[strlen(psz_item)-1] = 0; }

        if( !psz_item_mrl ) psz_item_mrl = psz_item;
        else if( *psz_item )
        {
            i_options++;
            ppsz_options = realloc( ppsz_options, i_options * sizeof(char *) );
            ppsz_options[i_options - 1] = &psz_item[1];
        }

        if( *psz_mrl ) SKIPSPACE( psz_mrl );
    }

    /* Now create a playlist item */
    if( psz_item_mrl )
    {
        p_item = playlist_ItemNew( p_intf, psz_item_mrl, psz_item_mrl );
        for( i = 0; i < i_options; i++ )
        {
            playlist_ItemAddOption( p_item, ppsz_options[i] );
        }
    }

    if( i_options ) free( ppsz_options );
    free( psz_orig );

    return p_item;
}

/*****************************************************************************
 * checkUpdates : check for updates
 ****************************************************************************/
static void checkUpdates( intf_thread_t *p_intf, char *psz_arg )
{
    update_iterator_t *p_uit;
    update_t *p_u = update_New( p_intf );
    if( p_u == NULL ) return;
    p_uit = update_iterator_New( p_u );
    if( p_uit )
    {
        int s = 0, t = 0;

        if( strstr( psz_arg, "newer" ) )
            s |= UPDATE_RELEASE_STATUS_NEWER;
        if( strstr( psz_arg, "equal" ) )
            s |= UPDATE_RELEASE_STATUS_EQUAL;
        if( strstr( psz_arg, "older" ) )
            s |= UPDATE_RELEASE_STATUS_OLDER;
        if( s ) p_uit->i_rs = s;
        else p_uit->i_rs = UPDATE_RELEASE_STATUS_NEWER;

        if( strstr( psz_arg, "undef" ) )
            t |= UPDATE_FILE_TYPE_UNDEF;
        if( strstr( psz_arg, "info" ) )
            t |= UPDATE_FILE_TYPE_INFO;
        if( strstr( psz_arg, "source" ) )
            t |= UPDATE_FILE_TYPE_SOURCE;
        if( strstr( psz_arg, "binary" ) )
            t |= UPDATE_FILE_TYPE_BINARY;
        if( strstr( psz_arg, "plugin" ) )
            t |= UPDATE_FILE_TYPE_PLUGIN;
        if( t ) p_uit->i_t = t;

        update_Check( p_u, VLC_FALSE );
        update_iterator_Action( p_uit, UPDATE_MIRROR );
        msg_rc( "\nUsing mirror: %s (%s) [%s]",
                p_uit->mirror.psz_name,
                p_uit->mirror.psz_location,
                p_uit->mirror.psz_type );
        while( (s = update_iterator_Action( p_uit, UPDATE_FILE )) != UPDATE_FAIL )
        {
            char *psz_tmp;
            if( s & UPDATE_RELEASE )
            {
                switch( p_uit->release.i_status )
                {
                    case UPDATE_RELEASE_STATUS_OLDER:
                        psz_tmp = strdup( "older" );
                        break;
                    case UPDATE_RELEASE_STATUS_EQUAL:
                        psz_tmp = strdup( "equal" );
                        break;
                    case UPDATE_RELEASE_STATUS_NEWER:
                        psz_tmp = strdup( "newer" );
                        break;
                    default:
                        psz_tmp = strdup( "?!?" );
                        break;
                }
                msg_rc( "\n+----[ VLC %s %s (%s) ] ",
                        p_uit->release.psz_version,
                        p_uit->release.psz_svn_revision,
                        psz_tmp );
                free( psz_tmp );
            }
            switch( p_uit->file.i_type )
            {
                case UPDATE_FILE_TYPE_UNDEF:
                    psz_tmp = strdup( "undef" );
                    break;
                case UPDATE_FILE_TYPE_INFO:
                    psz_tmp = strdup( "info" );
                    break;
                case UPDATE_FILE_TYPE_SOURCE:
                    psz_tmp = strdup( "source" );
                    break;
                case UPDATE_FILE_TYPE_BINARY:
                    psz_tmp = strdup( "binary" );
                    break;
                case UPDATE_FILE_TYPE_PLUGIN:
                    psz_tmp = strdup( "plugin" );
                    break;
                default:
                    psz_tmp = strdup( "?!?" );
                    break;
            }
            msg_rc( "| %s (%s)", p_uit->file.psz_description, psz_tmp );
            free( psz_tmp );
            if( p_uit->file.l_size )
            {
                if( p_uit->file.l_size > 1024 * 1024 * 1024 )
                    asprintf( &psz_tmp, "(%ld GB)",
                              p_uit->file.l_size / (1024*1024*1024) );
                if( p_uit->file.l_size > 1024 * 1024 )
                    asprintf( &psz_tmp, "(%ld MB)",
                              p_uit->file.l_size / (1024*1024) );
                else if( p_uit->file.l_size > 1024 )
                    asprintf( &psz_tmp, "(%ld kB)",
                              p_uit->file.l_size / 1024 );
                else
                    asprintf( &psz_tmp, "(%ld B)", p_uit->file.l_size );
            }
            else
            {
                psz_tmp = strdup( "" );
            }
            msg_rc( "| %s %s", p_uit->file.psz_url, psz_tmp );
            msg_rc( "+----" );
            free( psz_tmp );
        }
        msg_rc( "" );
        update_iterator_Delete( p_uit );
    }
    update_Delete( p_u );
}
