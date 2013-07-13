/*****************************************************************************
 * rc.c : remote control stdin/stdout module for vlc
 *****************************************************************************
 * Copyright (C) 2004-2009 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <errno.h>                                                 /* ENOMEM */
#include <signal.h>
#include <assert.h>
#include <math.h>

#include <vlc_interface.h>
#include <vlc_aout.h>
#include <vlc_vout.h>
#include <vlc_playlist.h>
#include <vlc_keys.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#include <sys/types.h>

#include <vlc_network.h>
#include <vlc_url.h>

#include <vlc_charset.h>

#if defined(PF_UNIX) && !defined(PF_LOCAL)
#    define PF_LOCAL PF_UNIX
#endif

#if defined(AF_LOCAL) && ! defined(_WIN32)
#    include <sys/un.h>
#endif

#define MAX_LINE_LENGTH 1024
#define STATUS_CHANGE "status change: "

/* input_state_e from <vlc_input.h> */
static const char *ppsz_input_state[] = {
    [INIT_S] = N_("Initializing"),
    [OPENING_S] = N_("Opening"),
    [PLAYING_S] = N_("Play"),
    [PAUSE_S] = N_("Pause"),
    [END_S] = N_("End"),
    [ERROR_S] = N_("Error"),
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate     ( vlc_object_t * );
static void Deactivate   ( vlc_object_t * );
static void *Run         ( void * );

static void Help         ( intf_thread_t * );
static void RegisterCallbacks( intf_thread_t * );

static bool ReadCommand( intf_thread_t *, char *, int * );

static input_item_t *parse_MRL( const char * );

static int  Input        ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  Playlist     ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  Quit         ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  Intf         ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  Volume       ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  VolumeMove   ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  VideoConfig  ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  AudioDevice  ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  AudioChannel ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );
static int  Statistics   ( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );

static int updateStatistics( intf_thread_t *, input_item_t *);

/* Status Callbacks */
static int VolumeChanged( vlc_object_t *, char const *,
                          vlc_value_t, vlc_value_t, void * );
static int InputEvent( vlc_object_t *, char const *,
                       vlc_value_t, vlc_value_t, void * );

struct intf_sys_t
{
    int *pi_socket_listen;
    int i_socket;
    char *psz_unix_path;
    vlc_thread_t thread;

    /* status changes */
    vlc_mutex_t       status_lock;
    int               i_last_state;
    playlist_t        *p_playlist;
    input_thread_t    *p_input;
    bool              b_input_buffering;

#ifdef _WIN32
    HANDLE hConsoleIn;
    bool b_quiet;
#endif
};

VLC_FORMAT(2, 3)
static void msg_rc( intf_thread_t *p_intf, const char *psz_fmt, ... )
{
    va_list args;
    char fmt_eol[strlen (psz_fmt) + 3];

    snprintf (fmt_eol, sizeof (fmt_eol), "%s\r\n", psz_fmt);
    va_start( args, psz_fmt );

    if( p_intf->p_sys->i_socket == -1 )
        utf8_vfprintf( stdout, fmt_eol, args );
    else
        net_vaPrintf( p_intf, p_intf->p_sys->i_socket, NULL, fmt_eol, args );
    va_end( args );
}
#define msg_rc( ... ) msg_rc( p_intf, __VA_ARGS__ )

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

#ifdef _WIN32
#define QUIET_TEXT N_("Do not open a DOS command box interface")
#define QUIET_LONGTEXT N_( \
    "By default the rc interface plugin will start a DOS command box. " \
    "Enabling the quiet mode will not bring this command box but can also " \
    "be pretty annoying when you want to stop VLC and no video window is " \
    "open." )
#endif

vlc_module_begin ()
    set_shortname( N_("RC"))
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_MAIN )
    set_description( N_("Remote control interface") )
    add_bool( "rc-show-pos", false, POS_TEXT, POS_LONGTEXT, true )

#ifdef _WIN32
    add_bool( "rc-quiet", false, QUIET_TEXT, QUIET_LONGTEXT, false )
#else
#if defined (HAVE_ISATTY)
    add_bool( "rc-fake-tty", false, TTY_TEXT, TTY_LONGTEXT, true )
#endif
    add_string( "rc-unix", NULL, UNIX_TEXT, UNIX_LONGTEXT, true )
#endif
    add_string( "rc-host", NULL, HOST_TEXT, HOST_LONGTEXT, true )

    set_capability( "interface", 20 )

    set_callbacks( Activate, Deactivate )
#ifdef _WIN32
    add_shortcut( "rc" )
#endif
vlc_module_end ()

/*****************************************************************************
 * Activate: initialize and create stuff
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    /* FIXME: This function is full of memory leaks and bugs in error paths. */
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    playlist_t *p_playlist = pl_Get( p_intf );
    char *psz_host, *psz_unix_path = NULL;
    int  *pi_socket = NULL;

#ifndef _WIN32
#if defined(HAVE_ISATTY)
    /* Check that stdin is a TTY */
    if( !var_InheritBool( p_intf, "rc-fake-tty" ) && !isatty( 0 ) )
    {
        msg_Warn( p_intf, "fd 0 is not a TTY" );
        return VLC_EGENERIC;
    }
#endif

    psz_unix_path = var_InheritString( p_intf, "rc-unix" );
    if( psz_unix_path )
    {
        int i_socket;

#ifndef AF_LOCAL
        msg_Warn( p_intf, "your OS doesn't support filesystem sockets" );
        free( psz_unix_path );
        return VLC_EGENERIC;
#else
        struct sockaddr_un addr;

        memset( &addr, 0, sizeof(struct sockaddr_un) );

        msg_Dbg( p_intf, "trying UNIX socket" );

        if( (i_socket = vlc_socket( PF_LOCAL, SOCK_STREAM, 0, false ) ) < 0 )
        {
            msg_Warn( p_intf, "can't open socket: %m" );
            free( psz_unix_path );
            return VLC_EGENERIC;
        }

        addr.sun_family = AF_LOCAL;
        strncpy( addr.sun_path, psz_unix_path, sizeof( addr.sun_path ) );
        addr.sun_path[sizeof( addr.sun_path ) - 1] = '\0';

        if (bind (i_socket, (struct sockaddr *)&addr, sizeof (addr))
         && (errno == EADDRINUSE)
         && connect (i_socket, (struct sockaddr *)&addr, sizeof (addr))
         && (errno == ECONNREFUSED))
        {
            msg_Info (p_intf, "Removing dead UNIX socket: %s", psz_unix_path);
            unlink (psz_unix_path);

            if (bind (i_socket, (struct sockaddr *)&addr, sizeof (addr)))
            {
                msg_Err (p_intf, "cannot bind UNIX socket at %s: %m",
                         psz_unix_path);
                free (psz_unix_path);
                net_Close (i_socket);
                return VLC_EGENERIC;
            }
        }

        if( listen( i_socket, 1 ) )
        {
            msg_Warn( p_intf, "can't listen on socket: %m");
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
#endif /* AF_LOCAL */
    }
#endif /* !_WIN32 */

    if( ( pi_socket == NULL ) &&
        ( psz_host = var_InheritString( p_intf, "rc-host" ) ) != NULL )
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

    intf_sys_t *p_sys = malloc( sizeof( *p_sys ) );
    if( unlikely(p_sys == NULL) )
    {
        net_ListenClose( pi_socket );
        free( psz_unix_path );
        return VLC_ENOMEM;
    }

    p_intf->p_sys = p_sys;
    p_sys->pi_socket_listen = pi_socket;
    p_sys->i_socket = -1;
    p_sys->psz_unix_path = psz_unix_path;
    vlc_mutex_init( &p_sys->status_lock );
    p_sys->i_last_state = PLAYLIST_STOPPED;
    p_sys->b_input_buffering = false;
    p_sys->p_playlist = p_playlist;
    p_sys->p_input = NULL;

    /* Non-buffered stdout */
    setvbuf( stdout, (char *)NULL, _IOLBF, 0 );

#ifdef _WIN32
    p_sys->b_quiet = var_InheritBool( p_intf, "rc-quiet" );
    if( !p_sys->b_quiet )
#endif
    {
        CONSOLE_INTRO_MSG;
    }

    if( vlc_clone( &p_sys->thread, Run, p_intf, VLC_THREAD_PRIORITY_LOW ) )
        abort();

    msg_rc( "%s", _("Remote control interface initialized. Type `help' for help.") );

    /* Listen to audio volume updates */
    var_AddCallback( p_sys->p_playlist, "volume", VolumeChanged, p_intf );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: uninitialize and cleanup
 *****************************************************************************/
static void Deactivate( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    vlc_cancel( p_sys->thread );
    var_DelCallback( p_sys->p_playlist, "volume", VolumeChanged, p_intf );
    vlc_join( p_sys->thread, NULL );

    if( p_sys->p_input != NULL )
    {
        var_DelCallback( p_sys->p_input, "intf-event", InputEvent, p_intf );
        vlc_object_release( p_sys->p_input );
    }

    net_ListenClose( p_sys->pi_socket_listen );
    if( p_sys->i_socket != -1 )
        net_Close( p_sys->i_socket );
    if( p_sys->psz_unix_path != NULL )
    {
#if defined(AF_LOCAL) && !defined(_WIN32)
        unlink( p_sys->psz_unix_path );
#endif
        free( p_sys->psz_unix_path );
    }
    vlc_mutex_destroy( &p_sys->status_lock );
    free( p_sys );
}

/*****************************************************************************
 * RegisterCallbacks: Register callbacks to dynamic variables
 *****************************************************************************/
static void RegisterCallbacks( intf_thread_t *p_intf )
{
    /* Register commands that will be cleaned up upon object destruction */
#define ADD( name, type, target )                                   \
    var_Create( p_intf, name, VLC_VAR_ ## type | VLC_VAR_ISCOMMAND ); \
    var_AddCallback( p_intf, name, target, NULL );
    ADD( "quit", VOID, Quit )
    ADD( "intf", STRING, Intf )

    ADD( "add", STRING, Playlist )
    ADD( "repeat", STRING, Playlist )
    ADD( "loop", STRING, Playlist )
    ADD( "random", STRING, Playlist )
    ADD( "enqueue", STRING, Playlist )
    ADD( "playlist", VOID, Playlist )
    ADD( "sort", VOID, Playlist )
    ADD( "play", VOID, Playlist )
    ADD( "stop", VOID, Playlist )
    ADD( "clear", VOID, Playlist )
    ADD( "prev", VOID, Playlist )
    ADD( "next", VOID, Playlist )
    ADD( "goto", INTEGER, Playlist )
    ADD( "status", INTEGER, Playlist )

    /* DVD commands */
    ADD( "pause", VOID, Input )
    ADD( "seek", INTEGER, Input )
    ADD( "title", STRING, Input )
    ADD( "title_n", VOID, Input )
    ADD( "title_p", VOID, Input )
    ADD( "chapter", STRING, Input )
    ADD( "chapter_n", VOID, Input )
    ADD( "chapter_p", VOID, Input )

    ADD( "fastforward", VOID, Input )
    ADD( "rewind", VOID, Input )
    ADD( "faster", VOID, Input )
    ADD( "slower", VOID, Input )
    ADD( "normal", VOID, Input )
    ADD( "frame", VOID, Input )

    ADD( "atrack", STRING, Input )
    ADD( "vtrack", STRING, Input )
    ADD( "strack", STRING, Input )

    /* video commands */
    ADD( "vratio", STRING, VideoConfig )
    ADD( "vcrop", STRING, VideoConfig )
    ADD( "vzoom", STRING, VideoConfig )
    ADD( "snapshot", VOID, VideoConfig )

    /* audio commands */
    ADD( "volume", STRING, Volume )
    ADD( "volup", STRING, VolumeMove )
    ADD( "voldown", STRING, VolumeMove )
    ADD( "adev", STRING, AudioDevice )
    ADD( "achan", STRING, AudioChannel )

    /* misc menu commands */
    ADD( "stats", BOOL, Statistics )

#undef ADD
}

/*****************************************************************************
 * Run: rc thread
 *****************************************************************************
 * This part of the interface is in a separate thread so that we can call
 * exec() from within it without annoying the rest of the program.
 *****************************************************************************/
static void *Run( void *data )
{
    intf_thread_t *p_intf = data;
    intf_sys_t *p_sys = p_intf->p_sys;

    char p_buffer[ MAX_LINE_LENGTH + 1 ];
    bool b_showpos = var_InheritBool( p_intf, "rc-show-pos" );

    int  i_size = 0;
    int  i_oldpos = 0;
    int  i_newpos;
    int  canc = vlc_savecancel( );

    p_buffer[0] = 0;

#ifdef _WIN32
    /* Get the file descriptor of the console input */
    p_intf->p_sys->hConsoleIn = GetStdHandle(STD_INPUT_HANDLE);
    if( p_intf->p_sys->hConsoleIn == INVALID_HANDLE_VALUE )
    {
        msg_Err( p_intf, "couldn't find user input handle" );
        return;
    }
#endif

    /* Register commands that will be cleaned up upon object destruction */
    RegisterCallbacks( p_intf );

    /* status callbacks */

    for( ;; )
    {
        char *psz_cmd, *psz_arg;
        bool b_complete;

        vlc_restorecancel( canc );

        if( p_sys->pi_socket_listen != NULL && p_sys->i_socket == -1 )
        {
            p_sys->i_socket =
                net_Accept( p_intf, p_sys->pi_socket_listen );
            if( p_sys->i_socket == -1 ) continue;
        }

        b_complete = ReadCommand( p_intf, p_buffer, &i_size );
        canc = vlc_savecancel( );

        /* Manage the input part */
        if( p_sys->p_input == NULL )
        {
            p_sys->p_input = playlist_CurrentInput( p_sys->p_playlist );
            /* New input has been registered */
            if( p_sys->p_input )
            {
                char *psz_uri = input_item_GetURI( input_GetItem( p_sys->p_input ) );
                msg_rc( STATUS_CHANGE "( new input: %s )", psz_uri );
                free( psz_uri );

                var_AddCallback( p_sys->p_input, "intf-event", InputEvent, p_intf );
            }
        }
#warning This is not reliable...
        else if( p_sys->p_input->b_dead )
        {
            var_DelCallback( p_sys->p_input, "intf-event", InputEvent, p_intf );
            vlc_object_release( p_sys->p_input );
            p_sys->p_input = NULL;

            p_sys->i_last_state = PLAYLIST_STOPPED;
            msg_rc( STATUS_CHANGE "( stop state: 0 )" );
        }

        if( p_sys->p_input != NULL )
        {
            playlist_t *p_playlist = p_sys->p_playlist;

            PL_LOCK;
            int status = playlist_Status( p_playlist );
            PL_UNLOCK;

            if( p_sys->i_last_state != status )
            {
                if( status == PLAYLIST_STOPPED )
                {
                    p_sys->i_last_state = PLAYLIST_STOPPED;
                    msg_rc( STATUS_CHANGE "( stop state: 5 )" );
                }
                else if( status == PLAYLIST_RUNNING )
                {
                    p_sys->i_last_state = PLAYLIST_RUNNING;
                    msg_rc( STATUS_CHANGE "( play state: 3 )" );
                }
                else if( status == PLAYLIST_PAUSED )
                {
                    p_sys->i_last_state = PLAYLIST_PAUSED;
                    msg_rc( STATUS_CHANGE "( pause state: 4 )" );
                }
            }
        }

        if( p_sys->p_input && b_showpos )
        {
            i_newpos = 100 * var_GetFloat( p_sys->p_input, "position" );
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
            psz_arg = (char*)"";
        }

        /* If the user typed a registered local command, try it */
        if( var_Type( p_intf, psz_cmd ) & VLC_VAR_ISCOMMAND )
        {
            vlc_value_t val;
            int i_ret;
            val.psz_string = psz_arg;

            if ((var_Type( p_intf, psz_cmd) & VLC_VAR_CLASS) == VLC_VAR_VOID)
                i_ret = var_TriggerCallback( p_intf, psz_cmd );
            else
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
            if ((var_Type( p_intf->p_libvlc, psz_cmd) & VLC_VAR_CLASS) == VLC_VAR_VOID)
                i_ret = var_TriggerCallback( p_intf, psz_cmd );
            else
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
            if( p_sys->i_socket != -1 )
            {
                net_Close( p_sys->i_socket );
                p_sys->i_socket = -1;
            }
        }
        else if( !strcmp( psz_cmd, "info" ) )
        {
            if( p_sys->p_input )
            {
                int i, j;
                vlc_mutex_lock( &input_GetItem(p_sys->p_input)->lock );
                for ( i = 0; i < input_GetItem(p_sys->p_input)->i_categories; i++ )
                {
                    info_category_t *p_category = input_GetItem(p_sys->p_input)
                                                        ->pp_categories[i];

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
                vlc_mutex_unlock( &input_GetItem(p_sys->p_input)->lock );
            }
            else
            {
                msg_rc( "no input" );
            }
        }
        else if( !strcmp( psz_cmd, "is_playing" ) )
        {
            if( p_sys->p_input == NULL )
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
            if( p_sys->p_input == NULL )
            {
                msg_rc("0");
            }
            else
            {
                vlc_value_t time;
                var_Get( p_sys->p_input, "time", &time );
                msg_rc( "%"PRIu64, time.i_time / 1000000);
            }
        }
        else if( !strcmp( psz_cmd, "get_length" ) )
        {
            if( p_sys->p_input == NULL )
            {
                msg_rc("0");
            }
            else
            {
                vlc_value_t time;
                var_Get( p_sys->p_input, "length", &time );
                msg_rc( "%"PRIu64, time.i_time / 1000000);
            }
        }
        else if( !strcmp( psz_cmd, "get_title" ) )
        {
            if( p_sys->p_input == NULL )
            {
                msg_rc("%s", "");
            }
            else
            {
                msg_rc( "%s", input_GetItem(p_sys->p_input)->psz_name );
            }
        }
        else if( !strcmp( psz_cmd, "longhelp" ) || !strncmp( psz_cmd, "h", 1 )
                 || !strncmp( psz_cmd, "H", 1 ) || !strncmp( psz_cmd, "?", 1 ) )
        {
            Help( p_intf );
        }
        else if( !strcmp( psz_cmd, "key" ) || !strcmp( psz_cmd, "hotkey" ) )
        {
            var_SetInteger( p_intf->p_libvlc, "key-action",
                            vlc_GetActionId( psz_arg ) );
        }
        else switch( psz_cmd[0] )
        {
        case 'f':
        case 'F':
        {
            bool fs;

            if( !strncasecmp( psz_arg, "on", 2 ) )
                var_SetBool( p_sys->p_playlist, "fullscreen", fs = true );
            else if( !strncasecmp( psz_arg, "off", 3 ) )
                var_SetBool( p_sys->p_playlist, "fullscreen", fs = false );
            else
                fs = var_ToggleBool( p_sys->p_playlist, "fullscreen" );

            if( p_sys->p_input == NULL )
            {
                vout_thread_t *p_vout = input_GetVout( p_sys->p_input );
                if( p_vout )
                {
                    var_SetBool( p_vout, "fullscreen", fs );
                    vlc_object_release( p_vout );
                }
            }
            break;
        }
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

    vlc_restorecancel( canc );

    return NULL;
}

static void Help( intf_thread_t *p_intf)
{
    msg_rc("%s", _("+----[ Remote control commands ]"));
    msg_rc(  "| ");
    msg_rc("%s", _("| add XYZ  . . . . . . . . . . . . add XYZ to playlist"));
    msg_rc("%s", _("| enqueue XYZ  . . . . . . . . . queue XYZ to playlist"));
    msg_rc("%s", _("| playlist . . . . .  show items currently in playlist"));
    msg_rc("%s", _("| play . . . . . . . . . . . . . . . . . . play stream"));
    msg_rc("%s", _("| stop . . . . . . . . . . . . . . . . . . stop stream"));
    msg_rc("%s", _("| next . . . . . . . . . . . . . .  next playlist item"));
    msg_rc("%s", _("| prev . . . . . . . . . . . .  previous playlist item"));
    msg_rc("%s", _("| goto . . . . . . . . . . . . . .  goto item at index"));
    msg_rc("%s", _("| repeat [on|off] . . . .  toggle playlist item repeat"));
    msg_rc("%s", _("| loop [on|off] . . . . . . . . . toggle playlist loop"));
    msg_rc("%s", _("| random [on|off] . . . . . . .  toggle random jumping"));
    msg_rc("%s", _("| clear . . . . . . . . . . . . . . clear the playlist"));
    msg_rc("%s", _("| status . . . . . . . . . . . current playlist status"));
    msg_rc("%s", _("| title [X]  . . . . . . set/get title in current item"));
    msg_rc("%s", _("| title_n  . . . . . . . .  next title in current item"));
    msg_rc("%s", _("| title_p  . . . . . .  previous title in current item"));
    msg_rc("%s", _("| chapter [X]  . . . . set/get chapter in current item"));
    msg_rc("%s", _("| chapter_n  . . . . . .  next chapter in current item"));
    msg_rc("%s", _("| chapter_p  . . . .  previous chapter in current item"));
    msg_rc(  "| ");
    msg_rc("%s", _("| seek X . . . seek in seconds, for instance `seek 12'"));
    msg_rc("%s", _("| pause  . . . . . . . . . . . . . . . .  toggle pause"));
    msg_rc("%s", _("| fastforward  . . . . . . . .  .  set to maximum rate"));
    msg_rc("%s", _("| rewind  . . . . . . . . . . . .  set to minimum rate"));
    msg_rc("%s", _("| faster . . . . . . . . . .  faster playing of stream"));
    msg_rc("%s", _("| slower . . . . . . . . . .  slower playing of stream"));
    msg_rc("%s", _("| normal . . . . . . . . . .  normal playing of stream"));
    msg_rc("%s", _("| frame. . . . . . . . . .  play frame by frame"));
    msg_rc("%s", _("| f [on|off] . . . . . . . . . . . . toggle fullscreen"));
    msg_rc("%s", _("| info . . . . .  information about the current stream"));
    msg_rc("%s", _("| stats  . . . . . . . .  show statistical information"));
    msg_rc("%s", _("| get_time . . seconds elapsed since stream's beginning"));
    msg_rc("%s", _("| is_playing . . . .  1 if a stream plays, 0 otherwise"));
    msg_rc("%s", _("| get_title . . . . .  the title of the current stream"));
    msg_rc("%s", _("| get_length . . . .  the length of the current stream"));
    msg_rc(  "| ");
    msg_rc("%s", _("| volume [X] . . . . . . . . . .  set/get audio volume"));
    msg_rc("%s", _("| volup [X]  . . . . . . .  raise audio volume X steps"));
    msg_rc("%s", _("| voldown [X]  . . . . . .  lower audio volume X steps"));
    msg_rc("%s", _("| adev [device]  . . . . . . . .  set/get audio device"));
    msg_rc("%s", _("| achan [X]. . . . . . . . . .  set/get audio channels"));
    msg_rc("%s", _("| atrack [X] . . . . . . . . . . . set/get audio track"));
    msg_rc("%s", _("| vtrack [X] . . . . . . . . . . . set/get video track"));
    msg_rc("%s", _("| vratio [X]  . . . . . . . set/get video aspect ratio"));
    msg_rc("%s", _("| vcrop [X]  . . . . . . . . . . .  set/get video crop"));
    msg_rc("%s", _("| vzoom [X]  . . . . . . . . . . .  set/get video zoom"));
    msg_rc("%s", _("| snapshot . . . . . . . . . . . . take video snapshot"));
    msg_rc("%s", _("| strack [X] . . . . . . . . .  set/get subtitle track"));
    msg_rc("%s", _("| key [hotkey name] . . . . . .  simulate hotkey press"));
    msg_rc("%s", _("| menu . . [on|off|up|down|left|right|select] use menu"));
    msg_rc(  "| ");
    msg_rc("%s", _("| help . . . . . . . . . . . . . . . this help message"));
    msg_rc("%s", _("| logout . . . . . . .  exit (if in socket connection)"));
    msg_rc("%s", _("| quit . . . . . . . . . . . . . . . . . . .  quit vlc"));
    msg_rc(  "| ");
    msg_rc("%s", _("+----[ end of help ]"));
}

/********************************************************************
 * Status callback routines
 ********************************************************************/
static int VolumeChanged( vlc_object_t *p_this, char const *psz_cmd,
    vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    (void) p_this;
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(newval);
    intf_thread_t *p_intf = (intf_thread_t*)p_data;

    vlc_mutex_lock( &p_intf->p_sys->status_lock );
    msg_rc( STATUS_CHANGE "( audio volume: %ld )",
            lroundf(newval.f_float * AOUT_VOLUME_DEFAULT) );
    vlc_mutex_unlock( &p_intf->p_sys->status_lock );
    return VLC_SUCCESS;
}

static void StateChanged( intf_thread_t *p_intf, input_thread_t *p_input )
{
    playlist_t *p_playlist = p_intf->p_sys->p_playlist;

    PL_LOCK;
    const int i_status = playlist_Status( p_playlist );
    PL_UNLOCK;

    /* */
    const char *psz_cmd;
    switch( i_status )
    {
    case PLAYLIST_STOPPED:
        psz_cmd = "stop";
        break;
    case PLAYLIST_RUNNING:
        psz_cmd = "play";
        break;
    case PLAYLIST_PAUSED:
        psz_cmd = "pause";
        break;
    default:
        psz_cmd = "";
        break;
    }

    /* */
    const int i_state = var_GetInteger( p_input, "state" );

    vlc_mutex_lock( &p_intf->p_sys->status_lock );
    msg_rc( STATUS_CHANGE "( %s state: %d ): %s", psz_cmd,
            i_state, ppsz_input_state[i_state] );
    vlc_mutex_unlock( &p_intf->p_sys->status_lock );
}
static void RateChanged( intf_thread_t *p_intf,
                         input_thread_t *p_input )
{
    vlc_mutex_lock( &p_intf->p_sys->status_lock );
    msg_rc( STATUS_CHANGE "( new rate: %.3f )",
            var_GetFloat( p_input, "rate" ) );
    vlc_mutex_unlock( &p_intf->p_sys->status_lock );
}
static void PositionChanged( intf_thread_t *p_intf,
                             input_thread_t *p_input )
{
    vlc_mutex_lock( &p_intf->p_sys->status_lock );
    if( p_intf->p_sys->b_input_buffering )
        msg_rc( STATUS_CHANGE "( time: %"PRId64"s )",
                (var_GetTime( p_input, "time" )/1000000) );
    p_intf->p_sys->b_input_buffering = false;
    vlc_mutex_unlock( &p_intf->p_sys->status_lock );
}
static void CacheChanged( intf_thread_t *p_intf )
{
    vlc_mutex_lock( &p_intf->p_sys->status_lock );
    p_intf->p_sys->b_input_buffering = true;
    vlc_mutex_unlock( &p_intf->p_sys->status_lock );
}

static int InputEvent( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(psz_cmd);
    VLC_UNUSED(oldval);
    input_thread_t *p_input = (input_thread_t*)p_this;
    intf_thread_t *p_intf = p_data;

    switch( newval.i_int )
    {
    case INPUT_EVENT_STATE:
    case INPUT_EVENT_DEAD:
        StateChanged( p_intf, p_input );
        break;
    case INPUT_EVENT_RATE:
        RateChanged( p_intf, p_input );
        break;
    case INPUT_EVENT_POSITION:
        PositionChanged( p_intf, p_input );
        break;
    case INPUT_EVENT_CACHE:
        CacheChanged( p_intf );
        break;
    default:
        break;
    }
    return VLC_SUCCESS;
}

/********************************************************************
 * Command routines
 ********************************************************************/
static int Input( vlc_object_t *p_this, char const *psz_cmd,
                  vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(oldval); VLC_UNUSED(p_data);
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    input_thread_t *p_input =
        playlist_CurrentInput( p_intf->p_sys->p_playlist );
    int i_error = VLC_EGENERIC;

    if( !p_input )
        return VLC_ENOOBJ;

    int state = var_GetInteger( p_input, "state" );
    if( ( state == PAUSE_S ) &&
        ( strcmp( psz_cmd, "pause" ) != 0 ) && (strcmp( psz_cmd,"frame") != 0 ) )
    {
        msg_rc( "%s", _("Press menu select or pause to continue.") );
    }
    else
    /* Parse commands that only require an input */
    if( !strcmp( psz_cmd, "pause" ) )
    {
        playlist_Pause( p_intf->p_sys->p_playlist );
        i_error = VLC_SUCCESS;
    }
    else if( !strcmp( psz_cmd, "seek" ) )
    {
        if( strlen( newval.psz_string ) > 0 &&
            newval.psz_string[strlen( newval.psz_string ) - 1] == '%' )
        {
            float f = atof( newval.psz_string ) / 100.0;
            var_SetFloat( p_input, "position", f );
        }
        else
        {
            mtime_t t = ((int64_t)atoi( newval.psz_string )) * CLOCK_FREQ;
            var_SetTime( p_input, "time", t );
        }
        i_error = VLC_SUCCESS;
    }
    else if ( !strcmp( psz_cmd, "fastforward" ) )
    {
        if( var_GetBool( p_input, "can-rate" ) )
        {
            float f_rate = var_GetFloat( p_input, "rate" );
            f_rate = (f_rate < 0) ? -f_rate : f_rate * 2;
            var_SetFloat( p_input, "rate", f_rate );
        }
        else
        {
            var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_JUMP_FORWARD_EXTRASHORT );
        }
        i_error = VLC_SUCCESS;
    }
    else if ( !strcmp( psz_cmd, "rewind" ) )
    {
        if( var_GetBool( p_input, "can-rewind" ) )
        {
            float f_rate = var_GetFloat( p_input, "rate" );
            f_rate = (f_rate > 0) ? -f_rate : f_rate * 2;
            var_SetFloat( p_input, "rate", f_rate );
        }
        else
        {
            var_SetInteger( p_intf->p_libvlc, "key-action", ACTIONID_JUMP_BACKWARD_EXTRASHORT );
        }
        i_error = VLC_SUCCESS;
    }
    else if ( !strcmp( psz_cmd, "faster" ) )
    {
        var_TriggerCallback( p_intf->p_sys->p_playlist, "rate-faster" );
        i_error = VLC_SUCCESS;
    }
    else if ( !strcmp( psz_cmd, "slower" ) )
    {
        var_TriggerCallback( p_intf->p_sys->p_playlist, "rate-slower" );
        i_error = VLC_SUCCESS;
    }
    else if ( !strcmp( psz_cmd, "normal" ) )
    {
        var_SetFloat( p_intf->p_sys->p_playlist, "rate", 1. );
        i_error = VLC_SUCCESS;
    }
    else if ( !strcmp( psz_cmd, "frame" ) )
    {
	var_TriggerCallback( p_input, "frame-next" );
        i_error = VLC_SUCCESS;
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
                var_SetInteger( p_input, "chapter", atoi( newval.psz_string ) );
            }
            else
            {
                /* Get. */
                int i_chap = var_GetInteger( p_input, "chapter" );
                int i_chapter_count = var_CountChoices( p_input, "chapter" );
                msg_rc( "Currently playing chapter %d/%d.", i_chap,
                        i_chapter_count );
            }
        }
        else if( !strcmp( psz_cmd, "chapter_n" ) )
            var_TriggerCallback( p_input, "next-chapter" );
        else if( !strcmp( psz_cmd, "chapter_p" ) )
            var_TriggerCallback( p_input, "prev-chapter" );
        i_error = VLC_SUCCESS;
    }
    else if( !strcmp( psz_cmd, "title" ) ||
             !strcmp( psz_cmd, "title_n" ) ||
             !strcmp( psz_cmd, "title_p" ) )
    {
        if( !strcmp( psz_cmd, "title" ) )
        {
            if ( *newval.psz_string )
                /* Set. */
                var_SetInteger( p_input, "title", atoi( newval.psz_string ) );
            else
            {
                /* Get. */
                int i_title = var_GetInteger( p_input, "title" );
                int i_title_count = var_CountChoices( p_input, "title" );
                msg_rc( "Currently playing title %d/%d.", i_title,
                        i_title_count );
            }
        }
        else if( !strcmp( psz_cmd, "title_n" ) )
            var_TriggerCallback( p_input, "next-title" );
        else if( !strcmp( psz_cmd, "title_p" ) )
            var_TriggerCallback( p_input, "prev-title" );

        i_error = VLC_SUCCESS;
    }
    else if(    !strcmp( psz_cmd, "atrack" )
             || !strcmp( psz_cmd, "vtrack" )
             || !strcmp( psz_cmd, "strack" ) )
    {
        const char *psz_variable;
        vlc_value_t val_name;

        if( !strcmp( psz_cmd, "atrack" ) )
        {
            psz_variable = "audio-es";
        }
        else if( !strcmp( psz_cmd, "vtrack" ) )
        {
            psz_variable = "video-es";
        }
        else
        {
            psz_variable = "spu-es";
        }

        /* Get the descriptive name of the variable */
        var_Change( p_input, psz_variable, VLC_VAR_GETTEXT,
                     &val_name, NULL );
        if( !val_name.psz_string ) val_name.psz_string = strdup(psz_variable);

        if( newval.psz_string && *newval.psz_string )
        {
            /* set */
            i_error = var_SetInteger( p_input, psz_variable,
                                      atoi( newval.psz_string ) );
        }
        else
        {
            /* get */
            vlc_value_t val, text;
            int i, i_value;

            if ( var_Get( p_input, psz_variable, &val ) < 0 )
                goto out;
            i_value = val.i_int;

            if ( var_Change( p_input, psz_variable,
                             VLC_VAR_GETLIST, &val, &text ) < 0 )
                goto out;

            msg_rc( "+----[ %s ]", val_name.psz_string );
            for ( i = 0; i < val.p_list->i_count; i++ )
            {
                if ( i_value == val.p_list->p_values[i].i_int )
                    msg_rc( "| %"PRId64" - %s *",
                            val.p_list->p_values[i].i_int,
                            text.p_list->p_values[i].psz_string );
                else
                    msg_rc( "| %"PRId64" - %s",
                            val.p_list->p_values[i].i_int,
                            text.p_list->p_values[i].psz_string );
            }
            var_FreeList( &val, &text );
            msg_rc( "+----[ end of %s ]", val_name.psz_string );
        }
        free( val_name.psz_string );
    }
out:
    vlc_object_release( p_input );
    return i_error;
}

static void print_playlist( intf_thread_t *p_intf, playlist_item_t *p_item, int i_level )
{
    int i;
    char psz_buffer[MSTRTIME_MAX_SIZE];
    for( i = 0; i< p_item->i_children; i++ )
    {
        if( p_item->pp_children[i]->p_input->i_duration != -1 )
        {
            secstotimestr( psz_buffer, p_item->pp_children[i]->p_input->i_duration / 1000000 );
            msg_rc( "|%*s- %s (%s)", 2 * i_level, "", p_item->pp_children[i]->p_input->psz_name, psz_buffer );
        }
        else
            msg_rc( "|%*s- %s", 2 * i_level, "", p_item->pp_children[i]->p_input->psz_name );

        if( p_item->pp_children[i]->i_children >= 0 )
            print_playlist( p_intf, p_item->pp_children[i], i_level + 1 );
    }
}

static int Playlist( vlc_object_t *p_this, char const *psz_cmd,
                     vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(oldval); VLC_UNUSED(p_data);
    vlc_value_t val;

    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    playlist_t *p_playlist = p_intf->p_sys->p_playlist;
    input_thread_t * p_input = playlist_CurrentInput( p_playlist );

    if( p_input )
    {
        int state = var_GetInteger( p_input, "state" );
        vlc_object_release( p_input );

        if( state == PAUSE_S )
        {
            msg_rc( "%s", _("Type 'menu select' or 'pause' to continue.") );
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
    else if( !strcmp( psz_cmd, "repeat" ) )
    {
        bool b_update = true;

        var_Get( p_playlist, "repeat", &val );

        if( strlen( newval.psz_string ) > 0 )
        {
            if ( ( !strncmp( newval.psz_string, "on", 2 )  &&  val.b_bool ) ||
                 ( !strncmp( newval.psz_string, "off", 3 ) && !val.b_bool ) )
            {
                b_update = false;
            }
        }

        if ( b_update )
        {
            val.b_bool = !val.b_bool;
            var_Set( p_playlist, "repeat", val );
        }
        msg_rc( "Setting repeat to %d", val.b_bool );
    }
    else if( !strcmp( psz_cmd, "loop" ) )
    {
        bool b_update = true;

        var_Get( p_playlist, "loop", &val );

        if( strlen( newval.psz_string ) > 0 )
        {
            if ( ( !strncmp( newval.psz_string, "on", 2 )  &&  val.b_bool ) ||
                 ( !strncmp( newval.psz_string, "off", 3 ) && !val.b_bool ) )
            {
                b_update = false;
            }
        }

        if ( b_update )
        {
            val.b_bool = !val.b_bool;
            var_Set( p_playlist, "loop", val );
        }
        msg_rc( "Setting loop to %d", val.b_bool );
    }
    else if( !strcmp( psz_cmd, "random" ) )
    {
        bool b_update = true;

        var_Get( p_playlist, "random", &val );

        if( strlen( newval.psz_string ) > 0 )
        {
            if ( ( !strncmp( newval.psz_string, "on", 2 )  &&  val.b_bool ) ||
                 ( !strncmp( newval.psz_string, "off", 3 ) && !val.b_bool ) )
            {
                b_update = false;
            }
        }

        if ( b_update )
        {
            val.b_bool = !val.b_bool;
            var_Set( p_playlist, "random", val );
        }
        msg_rc( "Setting random to %d", val.b_bool );
    }
    else if (!strcmp( psz_cmd, "goto" ) )
    {
        PL_LOCK;
        unsigned i_pos = atoi( newval.psz_string );
        unsigned i_size = p_playlist->items.i_size;

        if( i_pos <= 0 )
            msg_rc( "%s", _("Error: `goto' needs an argument greater than zero.") );
        else if( i_pos <= i_size )
        {
            playlist_item_t *p_item, *p_parent;
            p_item = p_parent = p_playlist->items.p_elems[i_pos-1];
            while( p_parent->p_parent )
                p_parent = p_parent->p_parent;
            playlist_Control( p_playlist, PLAYLIST_VIEWPLAY, pl_Locked,
                    p_parent, p_item );
        }
        else
            msg_rc( vlc_ngettext("Playlist has only %u element",
                                 "Playlist has only %u elements", i_size),
                     i_size );
        PL_UNLOCK;
    }
    else if( !strcmp( psz_cmd, "stop" ) )
    {
        playlist_Stop( p_playlist );
    }
    else if( !strcmp( psz_cmd, "clear" ) )
    {
        playlist_Stop( p_playlist );
        playlist_Clear( p_playlist, pl_Unlocked );
    }
    else if( !strcmp( psz_cmd, "add" ) &&
             newval.psz_string && *newval.psz_string )
    {
        input_item_t *p_item = parse_MRL( newval.psz_string );

        if( p_item )
        {
            msg_rc( "Trying to add %s to playlist.", newval.psz_string );
            int i_ret =playlist_AddInput( p_playlist, p_item,
                     PLAYLIST_GO|PLAYLIST_APPEND, PLAYLIST_END, true,
                     pl_Unlocked );
            vlc_gc_decref( p_item );
            if( i_ret != VLC_SUCCESS )
            {
                return VLC_EGENERIC;
            }
        }
    }
    else if( !strcmp( psz_cmd, "enqueue" ) &&
             newval.psz_string && *newval.psz_string )
    {
        input_item_t *p_item = parse_MRL( newval.psz_string );

        if( p_item )
        {
            msg_rc( "trying to enqueue %s to playlist", newval.psz_string );
            if( playlist_AddInput( p_playlist, p_item,
                               PLAYLIST_APPEND, PLAYLIST_END, true,
                               pl_Unlocked ) != VLC_SUCCESS )
            {
                return VLC_EGENERIC;
            }
        }
    }
    else if( !strcmp( psz_cmd, "playlist" ) )
    {
        msg_rc( "+----[ Playlist ]" );
        print_playlist( p_intf, p_playlist->p_root_category, 0 );
        msg_rc( "+----[ End of playlist ]" );
    }

    else if( !strcmp( psz_cmd, "sort" ))
    {
        PL_LOCK;
        playlist_RecursiveNodeSort( p_playlist, p_playlist->p_root_onelevel,
                                    SORT_ARTIST, ORDER_NORMAL );
        PL_UNLOCK;
    }
    else if( !strcmp( psz_cmd, "status" ) )
    {
        input_thread_t * p_input = playlist_CurrentInput( p_playlist );
        if( p_input )
        {
            /* Replay the current state of the system. */
            char *psz_uri =
                    input_item_GetURI( input_GetItem( p_input ) );
            vlc_object_release( p_input );
            if( likely(psz_uri != NULL) )
            {
                msg_rc( STATUS_CHANGE "( new input: %s )", psz_uri );
                free( psz_uri );
            }
        }

        float volume = playlist_VolumeGet( p_playlist );
        if( volume >= 0.f )
            msg_rc( STATUS_CHANGE "( audio volume: %ld )",
                    lroundf(volume * AOUT_VOLUME_DEFAULT) );

        int status;
        PL_LOCK;
        status = playlist_Status(p_playlist);
        PL_UNLOCK;
        switch( status )
        {
            case PLAYLIST_STOPPED:
                msg_rc( STATUS_CHANGE "( stop state: 5 )" );
                break;
            case PLAYLIST_RUNNING:
                msg_rc( STATUS_CHANGE "( play state: 3 )" );
                break;
            case PLAYLIST_PAUSED:
                msg_rc( STATUS_CHANGE "( pause state: 4 )" );
                break;
            default:
                msg_rc( STATUS_CHANGE "( unknown state: -1 )" );
                break;
        }
    }

    /*
     * sanity check
     */
    else
    {
        msg_rc( "unknown command!" );
    }

    return VLC_SUCCESS;
}

static int Quit( vlc_object_t *p_this, char const *psz_cmd,
                 vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(p_data); VLC_UNUSED(psz_cmd);
    VLC_UNUSED(oldval); VLC_UNUSED(newval);

    libvlc_Quit( p_this->p_libvlc );
    return VLC_SUCCESS;
}

static int Intf( vlc_object_t *p_this, char const *psz_cmd,
                 vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    return intf_Create( p_this->p_libvlc, newval.psz_string );
}

static int Volume( vlc_object_t *p_this, char const *psz_cmd,
                   vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    playlist_t *p_playlist = p_intf->p_sys->p_playlist;
    input_thread_t *p_input = playlist_CurrentInput( p_playlist );
    int i_error = VLC_EGENERIC;

    if( !p_input )
        return VLC_ENOOBJ;

    if( p_input )
    {
        int state = var_GetInteger( p_input, "state" );
        vlc_object_release( p_input );
        if( state == PAUSE_S )
        {
            msg_rc( "%s", _("Type 'menu select' or 'pause' to continue.") );
            return VLC_EGENERIC;
        }
    }

    if ( *newval.psz_string )
    {
        /* Set. */
        int i_volume = atoi( newval.psz_string );
        if( !playlist_VolumeSet( p_playlist,
                             i_volume / (float)AOUT_VOLUME_DEFAULT ) )
            i_error = VLC_SUCCESS;
        playlist_MuteSet( p_playlist, i_volume == 0 );
        msg_rc( STATUS_CHANGE "( audio volume: %d )", i_volume );
    }
    else
    {
        /* Get. */
        msg_rc( STATUS_CHANGE "( audio volume: %ld )",
               lroundf( playlist_VolumeGet( p_playlist ) * AOUT_VOLUME_DEFAULT ) );
        i_error = VLC_SUCCESS;
    }

    return i_error;
}

static int VolumeMove( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(oldval); VLC_UNUSED(p_data);
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    float volume;
    input_thread_t *p_input =
        playlist_CurrentInput( p_intf->p_sys->p_playlist );
    int i_nb_steps = atoi(newval.psz_string);
    int i_error = VLC_SUCCESS;

    if( !p_input )
        return VLC_ENOOBJ;

    int state = var_GetInteger( p_input, "state" );
    vlc_object_release( p_input );
    if( state == PAUSE_S )
    {
        msg_rc( "%s", _("Type 'menu select' or 'pause' to continue.") );
        return VLC_EGENERIC;
    }

    if( !strcmp(psz_cmd, "voldown") )
        i_nb_steps *= -1;
    if( playlist_VolumeUp( p_intf->p_sys->p_playlist, i_nb_steps, &volume ) < 0 )
        i_error = VLC_EGENERIC;

    if ( !i_error )
        msg_rc( STATUS_CHANGE "( audio volume: %ld )",
                lroundf( volume * AOUT_VOLUME_DEFAULT ) );
    return i_error;
}


static int VideoConfig( vlc_object_t *p_this, char const *psz_cmd,
                        vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(oldval); VLC_UNUSED(p_data);
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    input_thread_t *p_input =
        playlist_CurrentInput( p_intf->p_sys->p_playlist );
    vout_thread_t * p_vout;
    const char * psz_variable = NULL;
    int i_error = VLC_SUCCESS;

    if( !p_input )
        return VLC_ENOOBJ;

    p_vout = input_GetVout( p_input );
    vlc_object_release( p_input );
    if( !p_vout )
        return VLC_ENOOBJ;

    if( !strcmp( psz_cmd, "vcrop" ) )
    {
        psz_variable = "crop";
    }
    else if( !strcmp( psz_cmd, "vratio" ) )
    {
        psz_variable = "aspect-ratio";
    }
    else if( !strcmp( psz_cmd, "vzoom" ) )
    {
        psz_variable = "zoom";
    }
    else if( !strcmp( psz_cmd, "snapshot" ) )
    {
        psz_variable = "video-snapshot";
    }
    else
        /* This case can't happen */
        assert( 0 );

    if( newval.psz_string && *newval.psz_string )
    {
        /* set */
        if( !strcmp( psz_variable, "zoom" ) )
        {
            vlc_value_t val;
            val.f_float = atof( newval.psz_string );
            i_error = var_Set( p_vout, psz_variable, val );
        }
        else
        {
            i_error = var_Set( p_vout, psz_variable, newval );
        }
    }
    else if( !strcmp( psz_cmd, "snapshot" ) )
    {
        var_TriggerCallback( p_vout, psz_variable );
    }
    else
    {
        /* get */
        vlc_value_t val_name;
        vlc_value_t val, text;
        int i;
        float f_value = 0.;
        char *psz_value = NULL;

        if ( var_Get( p_vout, psz_variable, &val ) < 0 )
        {
            vlc_object_release( p_vout );
            return VLC_EGENERIC;
        }
        if( !strcmp( psz_variable, "zoom" ) )
        {
            f_value = val.f_float;
        }
        else
        {
            psz_value = val.psz_string;
        }

        if ( var_Change( p_vout, psz_variable,
                         VLC_VAR_GETLIST, &val, &text ) < 0 )
        {
            vlc_object_release( p_vout );
            free( psz_value );
            return VLC_EGENERIC;
        }

        /* Get the descriptive name of the variable */
        var_Change( p_vout, psz_variable, VLC_VAR_GETTEXT,
                    &val_name, NULL );
        if( !val_name.psz_string ) val_name.psz_string = strdup(psz_variable);

        msg_rc( "+----[ %s ]", val_name.psz_string );
        if( !strcmp( psz_variable, "zoom" ) )
        {
            for ( i = 0; i < val.p_list->i_count; i++ )
            {
                if ( f_value == val.p_list->p_values[i].f_float )
                    msg_rc( "| %f - %s *", val.p_list->p_values[i].f_float,
                            text.p_list->p_values[i].psz_string );
                else
                    msg_rc( "| %f - %s", val.p_list->p_values[i].f_float,
                            text.p_list->p_values[i].psz_string );
            }
        }
        else
        {
            for ( i = 0; i < val.p_list->i_count; i++ )
            {
                if ( !strcmp( psz_value, val.p_list->p_values[i].psz_string ) )
                    msg_rc( "| %s - %s *", val.p_list->p_values[i].psz_string,
                            text.p_list->p_values[i].psz_string );
                else
                    msg_rc( "| %s - %s", val.p_list->p_values[i].psz_string,
                            text.p_list->p_values[i].psz_string );
            }
            free( psz_value );
        }
        var_FreeList( &val, &text );
        msg_rc( "+----[ end of %s ]", val_name.psz_string );

        free( val_name.psz_string );
    }
    vlc_object_release( p_vout );
    return i_error;
}

static int AudioDevice( vlc_object_t *obj, char const *cmd,
                        vlc_value_t old, vlc_value_t cur, void *dummy )
{
    intf_thread_t *p_intf = (intf_thread_t *)obj;
    audio_output_t *p_aout = playlist_GetAout( pl_Get(p_intf) );
    if( p_aout == NULL )
        return VLC_ENOOBJ;

    if( !*cur.psz_string )
    {
        char **ids, **names;
        int n = aout_DevicesList( p_aout, &ids, &names );
        if( n < 0 )
            goto out;

        char *dev = aout_DeviceGet( p_aout );
        const char *devstr = (dev != NULL) ? dev : "";

        msg_rc( "+----[ %s ]", cmd );
        for ( int i = 0; i < n; i++ )
        {
            const char *fmt = "| %s - %s";

            if( !strcmp(devstr, ids[i]) )
                fmt = "| %s - %s *";
            msg_rc( fmt, ids[i], names[i] );
            free( names[i] );
            free( ids[i] );
        }
        msg_rc( "+----[ end of %s ]", cmd );

        free( dev );
        free( names );
        free( ids );
    }
    else
        aout_DeviceSet( p_aout, cur.psz_string );
out:
    vlc_object_release( p_aout );
    (void) old; (void) dummy;
    return VLC_SUCCESS;
}

static int AudioChannel( vlc_object_t *obj, char const *cmd,
                         vlc_value_t old, vlc_value_t cur, void *dummy )
{
    intf_thread_t *p_intf = (intf_thread_t*)obj;
    vlc_object_t *p_aout = (vlc_object_t *)playlist_GetAout( pl_Get(p_intf) );
    if ( p_aout == NULL )
         return VLC_ENOOBJ;

    int ret = VLC_SUCCESS;

    if ( !*cur.psz_string )
    {
        /* Retrieve all registered ***. */
        vlc_value_t val, text;
        if ( var_Change( p_aout, "stereo-mode",
                         VLC_VAR_GETLIST, &val, &text ) < 0 )
        {
            ret = VLC_ENOVAR;
            goto out;
        }

        int i_value = var_GetInteger( p_aout, "stereo-mode" );

        msg_rc( "+----[ %s ]", cmd );
        for ( int i = 0; i < val.p_list->i_count; i++ )
        {
            if ( i_value == val.p_list->p_values[i].i_int )
                msg_rc( "| %"PRId64" - %s *", val.p_list->p_values[i].i_int,
                        text.p_list->p_values[i].psz_string );
            else
                msg_rc( "| %"PRId64" - %s", val.p_list->p_values[i].i_int,
                        text.p_list->p_values[i].psz_string );
        }
        var_FreeList( &val, &text );
        msg_rc( "+----[ end of %s ]", cmd );
    }
    else
        ret = var_SetInteger( p_aout, "stereo-mode", atoi( cur.psz_string ) );
out:
    vlc_object_release( p_aout );
    (void) old; (void) dummy;
    return ret;
}

static int Statistics ( vlc_object_t *p_this, char const *psz_cmd,
    vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(newval); VLC_UNUSED(p_data);
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    input_thread_t *p_input =
        playlist_CurrentInput( p_intf->p_sys->p_playlist );

    if( !p_input )
        return VLC_ENOOBJ;

    updateStatistics( p_intf, input_GetItem(p_input) );
    vlc_object_release( p_input );
    return VLC_SUCCESS;
}

static int updateStatistics( intf_thread_t *p_intf, input_item_t *p_item )
{
    if( !p_item ) return VLC_EGENERIC;

    vlc_mutex_lock( &p_item->lock );
    vlc_mutex_lock( &p_item->p_stats->lock );
    msg_rc( "+----[ begin of statistical info ]" );

    /* Input */
    msg_rc("%s", _("+-[Incoming]"));
    msg_rc(_("| input bytes read : %8.0f KiB"),
            (float)(p_item->p_stats->i_read_bytes)/1024 );
    msg_rc(_("| input bitrate    :   %6.0f kb/s"),
            (float)(p_item->p_stats->f_input_bitrate)*8000 );
    msg_rc(_("| demux bytes read : %8.0f KiB"),
            (float)(p_item->p_stats->i_demux_read_bytes)/1024 );
    msg_rc(_("| demux bitrate    :   %6.0f kb/s"),
            (float)(p_item->p_stats->f_demux_bitrate)*8000 );
    msg_rc(_("| demux corrupted  :    %5"PRIi64),
            p_item->p_stats->i_demux_corrupted );
    msg_rc(_("| discontinuities  :    %5"PRIi64),
            p_item->p_stats->i_demux_discontinuity );
    msg_rc("|");
    /* Video */
    msg_rc("%s", _("+-[Video Decoding]"));
    msg_rc(_("| video decoded    :    %5"PRIi64),
            p_item->p_stats->i_decoded_video );
    msg_rc(_("| frames displayed :    %5"PRIi64),
            p_item->p_stats->i_displayed_pictures );
    msg_rc(_("| frames lost      :    %5"PRIi64),
            p_item->p_stats->i_lost_pictures );
    msg_rc("|");
    /* Audio*/
    msg_rc("%s", _("+-[Audio Decoding]"));
    msg_rc(_("| audio decoded    :    %5"PRIi64),
            p_item->p_stats->i_decoded_audio );
    msg_rc(_("| buffers played   :    %5"PRIi64),
            p_item->p_stats->i_played_abuffers );
    msg_rc(_("| buffers lost     :    %5"PRIi64),
            p_item->p_stats->i_lost_abuffers );
    msg_rc("|");
    /* Sout */
    msg_rc("%s", _("+-[Streaming]"));
    msg_rc(_("| packets sent     :    %5"PRIi64),
           p_item->p_stats->i_sent_packets );
    msg_rc(_("| bytes sent       : %8.0f KiB"),
            (float)(p_item->p_stats->i_sent_bytes)/1024 );
    msg_rc(_("| sending bitrate  :   %6.0f kb/s"),
            (float)(p_item->p_stats->f_send_bitrate*8)*1000 );
    msg_rc("|");
    msg_rc( "+----[ end of statistical info ]" );
    vlc_mutex_unlock( &p_item->p_stats->lock );
    vlc_mutex_unlock( &p_item->lock );

    return VLC_SUCCESS;
}

#ifdef _WIN32
static bool ReadWin32( intf_thread_t *p_intf, char *p_buffer, int *pi_size )
{
    INPUT_RECORD input_record;
    DWORD i_dw;

    /* On Win32, select() only works on socket descriptors */
    while( WaitForSingleObject( p_intf->p_sys->hConsoleIn,
                                INTF_IDLE_SLEEP/1000 ) == WAIT_OBJECT_0 )
    {
        while( *pi_size < MAX_LINE_LENGTH &&
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
            return true;
        }
    }

    return false;
}
#endif

bool ReadCommand( intf_thread_t *p_intf, char *p_buffer, int *pi_size )
{
    int i_read = 0;

#ifdef _WIN32
    if( p_intf->p_sys->i_socket == -1 && !p_intf->p_sys->b_quiet )
        return ReadWin32( p_intf, p_buffer, pi_size );
    else if( p_intf->p_sys->i_socket == -1 )
    {
        msleep( INTF_IDLE_SLEEP );
        return false;
    }
#endif

    while( *pi_size < MAX_LINE_LENGTH &&
           (i_read = net_Read( p_intf, p_intf->p_sys->i_socket == -1 ?
                       0 /*STDIN_FILENO*/ : p_intf->p_sys->i_socket, NULL,
                  (uint8_t *)p_buffer + *pi_size, 1, false ) ) > 0 )
    {
        if( p_buffer[ *pi_size ] == '\r' || p_buffer[ *pi_size ] == '\n' )
            break;

        (*pi_size)++;
    }

    /* Connection closed */
    if( i_read <= 0 )
    {
        if( p_intf->p_sys->i_socket != -1 )
        {
            net_Close( p_intf->p_sys->i_socket );
            p_intf->p_sys->i_socket = -1;
        }
        else
        {
            /* Standard input closed: exit */
            vlc_value_t empty;
            Quit( VLC_OBJECT(p_intf), NULL, empty, empty, NULL );
        }

        p_buffer[ *pi_size ] = 0;
        return true;
    }

    if( *pi_size == MAX_LINE_LENGTH ||
        p_buffer[ *pi_size ] == '\r' || p_buffer[ *pi_size ] == '\n' )
    {
        p_buffer[ *pi_size ] = 0;
        return true;
    }

    return false;
}

/*****************************************************************************
 * parse_MRL: build a input item from a full mrl
 *****************************************************************************
 * MRL format: "simplified-mrl [:option-name[=option-value]]"
 * We don't check for '"' or '\'', we just assume that a ':' that follows a
 * space is a new option. Should be good enough for our purpose.
 *****************************************************************************/
static input_item_t *parse_MRL( const char *mrl )
{
#define SKIPSPACE( p ) { while( *p == ' ' || *p == '\t' ) p++; }
#define SKIPTRAILINGSPACE( p, d ) \
    { char *e=d; while( e > p && (*(e-1)==' ' || *(e-1)=='\t') ){e--;*e=0;} }

    input_item_t *p_item = NULL;
    char *psz_item = NULL, *psz_item_mrl = NULL, *psz_orig, *psz_mrl;
    char **ppsz_options = NULL;
    int i, i_options = 0;

    if( !mrl ) return 0;

    psz_mrl = psz_orig = strdup( mrl );
    if( !psz_mrl )
        return NULL;
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

        if( !psz_item_mrl )
        {
            if( strstr( psz_item, "://" ) != NULL )
                psz_item_mrl = strdup( psz_item );
            else
                psz_item_mrl = vlc_path2uri( psz_item, NULL );
            if( psz_item_mrl == NULL )
            {
                free( psz_orig );
                return NULL;
            }
        }
        else if( *psz_item )
        {
            i_options++;
            ppsz_options = xrealloc( ppsz_options, i_options * sizeof(char *) );
            ppsz_options[i_options - 1] = &psz_item[1];
        }

        if( *psz_mrl ) SKIPSPACE( psz_mrl );
    }

    /* Now create a playlist item */
    if( psz_item_mrl )
    {
        p_item = input_item_New( psz_item_mrl, NULL );
        for( i = 0; i < i_options; i++ )
        {
            input_item_AddOption( p_item, ppsz_options[i], VLC_INPUT_OPTION_TRUSTED );
        }
        free( psz_item_mrl );
    }

    if( i_options ) free( ppsz_options );
    free( psz_orig );

    return p_item;
}
