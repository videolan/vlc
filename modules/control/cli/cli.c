/*****************************************************************************
 * cli.c : remote control stdin/stdout module for vlc
 *****************************************************************************
 * Copyright (C) 2004-2009 the VideoLAN team
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

#include <errno.h>                                                 /* ENOMEM */
#include <assert.h>
#include <math.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef HAVE_WORDEXP_H
#include <wordexp.h>
#endif
#ifdef HAVE_SEARCH_H
#include <search.h>
#endif

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_player.h>
#include <vlc_actions.h>
#include <vlc_fs.h>
#include <vlc_network.h>
#include <vlc_url.h>
#include <vlc_charset.h>

#if defined(PF_UNIX) && !defined(PF_LOCAL)
#    define PF_LOCAL PF_UNIX
#endif

#if defined(AF_LOCAL) && ! defined(_WIN32)
#    include <sys/un.h>
#endif

#include "cli.h"

#define MAX_LINE_LENGTH 1024

void msg_print(intf_thread_t *p_intf, const char *psz_fmt, ...)
{
    va_list args;
    char fmt_eol[strlen (psz_fmt) + 3], *msg;
    int len;

    snprintf (fmt_eol, sizeof (fmt_eol), "%s\r\n", psz_fmt);
    va_start( args, psz_fmt );
    len = vasprintf( &msg, fmt_eol, args );
    va_end( args );

    if( len < 0 )
        return;

    if( p_intf->p_sys->i_socket == -1 )
#ifdef _WIN32
        utf8_fprintf( stdout, "%s", msg );
#else
        vlc_write( 1, msg, len );
#endif
    else
        net_Write( p_intf, p_intf->p_sys->i_socket, msg, len );

    free( msg );
}

static int cmdcmp(const void *a, const void *b)
{
    const char *const *na = a;
    const char *const *nb = b;

    return strcmp(*na, *nb);
}

void RegisterHandlers(intf_thread_t *intf, const struct cli_handler *handlers,
                      size_t count)
{
    intf_sys_t *sys = intf->p_sys;

    for (size_t i = 0; i < count; i++)
    {
        const char *const *name = &handlers[i].name;
        const char *const **pp;

        pp = tsearch(name, &sys->commands, cmdcmp);

        if (unlikely(pp == NULL))
            continue;

        assert(*pp == name); /* Fails if duplicate command */
    }
}

#if defined (_WIN32) && !VLC_WINSTORE_APP
# include "../intromsg.h"
#endif

static void Help( intf_thread_t *p_intf, const char *const *args, size_t count)
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
    msg_rc("%s", _("| record [on|off] . . . . . . . . . . toggle recording"));
    msg_rc("%s", _("| strack [X] . . . . . . . . .  set/get subtitle track"));
    msg_rc("%s", _("| key [hotkey name] . . . . . .  simulate hotkey press"));
    msg_rc(  "| ");
    msg_rc("%s", _("| help . . . . . . . . . . . . . . . this help message"));
    msg_rc("%s", _("| logout . . . . . . .  exit (if in socket connection)"));
    msg_rc("%s", _("| quit . . . . . . . . . . . . . . . . . . .  quit vlc"));
    msg_rc(  "| ");
    msg_rc("%s", _("+----[ end of help ]"));
    (void) args; (void) count;
}

static void Intf(intf_thread_t *intf, const char *const *args, size_t count)
{
    intf_Create(vlc_object_instance(intf), count == 1 ? "" : args[1]);
}

static void Quit(intf_thread_t *intf, const char *const *args, size_t count)
{
    libvlc_Quit(vlc_object_instance(intf));
    (void) args; (void) count;
}

static void LogOut(intf_thread_t *intf, const char *const *args, size_t count)
{
    intf_sys_t *sys = intf->p_sys;

    /* Close connection */
    if (sys->i_socket != -1)
    {
        net_Close(sys->i_socket);
        sys->i_socket = -1;
    }
    (void) args; (void) count;
}

static void KeyAction(intf_thread_t *intf, const char *const *args, size_t n)
{
    vlc_object_t *vlc = VLC_OBJECT(vlc_object_instance(intf));

    if (n > 1)
        var_SetInteger(vlc, "key-action", vlc_actions_get_id(args[1]));
}

static const struct cli_handler cmds[] =
{
    { "playlist", PlaylistList },
    { "sort", PlaylistSort },
    { "play", PlaylistPlay },
    { "stop", PlaylistStop },
    { "clear", PlaylistClear },
    { "prev", PlaylistPrev },
    { "next", PlaylistNext },
    { "status", PlaylistStatus },
    { "pause", PlayerPause },
    { "title_n", PlayerTitleNext },
    { "title_p", PlayerTitlePrev },
    { "chapter_n", PlayerChapterNext },
    { "chapter_p", PlayerChapterPrev },
    { "fastforward", PlayerFastForward },
    { "rewind", PlayerRewind },
    { "faster", PlayerFaster },
    { "slower", PlayerSlower },
    { "normal", PlayerNormal },
    { "frame", PlayerFrame },
    { "info", PlayerItemInfo },
    { "get_time", PlayerGetTime },
    { "get_length", PlayerGetLength },
    { "get_title", PlayerGetTitle },
    { "snapshot", PlayerVoutSnapshot },

    { "is_playing", IsPlaying },
    { "stats", Statistics },
    { "longhelp", Help },
    { "h", Help },
    { "help", Help },
    { "H", Help },
    { "?", Help },
    { "logout", LogOut },
    { "quit", Quit },

    { "intf", Intf },
    { "add", PlaylistAdd },
    { "repeat", PlaylistRepeat },
    { "loop", PlaylistLoop },
    { "random", PlaylistRandom },
    { "enqueue", PlaylistEnqueue },
    { "goto", PlaylistGoto },

    /* DVD commands */
    { "seek", Input },
    { "title", Input },
    { "chapter", Input },

    { "atrack", Input },
    { "vtrack", Input },
    { "strack", Input },
    { "record", Input },
    { "f", PlayerFullscreen },
    { "fs", PlayerFullscreen },
    { "fullscreen", PlayerFullscreen },

    /* video commands */
    { "vratio", VideoConfig },
    { "vcrop", VideoConfig },
    { "vzoom", VideoConfig },

    /* audio commands */
    { "volume", Volume },
    { "volup", VolumeMove },
    { "voldown", VolumeMove },
    { "adev", AudioDevice },
    { "achan", AudioChannel },

    { "key", KeyAction },
    { "hotkey", KeyAction },
};

static void UnknownCmd(intf_thread_t *intf, const char *const *args,
                       size_t count)
{
    msg_print(intf, _("Unknown command `%s'. Type `help' for help."), args[0]);
    (void) count;
}

static void Process(intf_thread_t *intf, const char *line)
{
    intf_sys_t *sys = intf->p_sys;
    /* Skip heading spaces */
    const char *cmd = line + strspn(line, " ");

    if (*cmd == '\0')
        return; /* Ignore empty line */

#ifdef HAVE_WORDEXP_H
    wordexp_t we;
    int val = wordexp(cmd, &we, 0);

    if (val != 0)
    {
        if (val == WRDE_NOSPACE)
error:      wordfree(&we);
        msg_print(intf, N_("parse error"));
        return;
    }

    size_t count = we.we_wordc;
    const char **args = vlc_alloc(count, sizeof (*args));
    if (unlikely(args == NULL))
        goto error;

    for (size_t i = 0; i < we.we_wordc; i++)
        args[i] = we.we_wordv[i];
#else
    /* Split psz_cmd at the first space and make sure that
     * psz_arg is valid */
    const char *args[] = { cmd, NULL };
    size_t count = 1;
    char *arg = strchr(cmd, ' ');

    if (arg != NULL)
    {
        *(arg++) = '\0';
        arg += strspn(arg, " ");

        if (*arg)
            count++;
    }
#endif

    if (count > 0)
    {
        void (*cb)(intf_thread_t *, const char *const *, size_t) = UnknownCmd;
        const struct cli_handler **h = tfind(&args[0], &sys->commands, cmdcmp);

        if (h != NULL)
            cb = (*h)->callback;

        cb(intf, args, count);
    }

#ifdef HAVE_WORDEXP_H
    free(args);
    wordfree(&we);
#endif
}


#if defined(_WIN32) && !VLC_WINSTORE_APP
static bool ReadWin32( intf_thread_t *p_intf, unsigned char *p_buffer, int *pi_size )
{
    INPUT_RECORD input_record;
    DWORD i_dw;

    /* On Win32, select() only works on socket descriptors */
    while( WaitForSingleObjectEx( p_intf->p_sys->hConsoleIn,
                                MS_FROM_VLC_TICK(INTF_IDLE_SLEEP), TRUE ) == WAIT_OBJECT_0 )
    {
        // Prefer to fail early when there's not enough space to store a 4 bytes
        // UTF8 character. The function will be immediatly called again and we won't
        // lose an input
        while( *pi_size < MAX_LINE_LENGTH - 4 &&
               ReadConsoleInput( p_intf->p_sys->hConsoleIn, &input_record, 1, &i_dw ) )
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
            if( input_record.Event.KeyEvent.uChar.AsciiChar == '\n' ||
                input_record.Event.KeyEvent.uChar.AsciiChar == '\r' )
            {
                putc( '\n', stdout );
                break;
            }
            switch( input_record.Event.KeyEvent.uChar.AsciiChar )
            {
            case '\b':
                if ( *pi_size == 0 )
                    break;
                if ( *pi_size > 1 && (p_buffer[*pi_size - 1] & 0xC0) == 0x80 )
                {
                    // pi_size currently points to the character to be written, so
                    // we need to roll back from 2 bytes to start erasing the previous
                    // character
                    (*pi_size) -= 2;
                    unsigned int nbBytes = 1;
                    while( *pi_size > 0 && (p_buffer[*pi_size] & 0xC0) == 0x80 )
                    {
                        (*pi_size)--;
                        nbBytes++;
                    }
                    assert( clz( (unsigned char)~(p_buffer[*pi_size]) ) == nbBytes + 1 );
                    // The first utf8 byte will be overriden by a \0
                }
                else
                    (*pi_size)--;
                p_buffer[*pi_size] = 0;

                fputs( "\b \b", stdout );
                break;
            default:
            {
                WCHAR psz_winput[] = { input_record.Event.KeyEvent.uChar.UnicodeChar, L'\0' };
                char* psz_input = FromWide( psz_winput );
                int input_size = strlen(psz_input);
                if ( *pi_size + input_size > MAX_LINE_LENGTH )
                {
                    p_buffer[ *pi_size ] = 0;
                    return false;
                }
                strcpy( (char*)&p_buffer[*pi_size], psz_input );
                utf8_fprintf( stdout, "%s", psz_input );
                free(psz_input);
                *pi_size += input_size;
            }
            }
        }

        p_buffer[ *pi_size ] = 0;
        return true;
    }

    vlc_testcancel ();

    return false;
}
#endif

static bool ReadCommand(intf_thread_t *p_intf, char *p_buffer, int *pi_size)
{
#if defined(_WIN32) && !VLC_WINSTORE_APP
    if( p_intf->p_sys->i_socket == -1 && !p_intf->p_sys->b_quiet )
        return ReadWin32( p_intf, (unsigned char*)p_buffer, pi_size );
    else if( p_intf->p_sys->i_socket == -1 )
    {
        vlc_tick_sleep( INTF_IDLE_SLEEP );
        return false;
    }
#endif

    while( *pi_size < MAX_LINE_LENGTH )
    {
        if( p_intf->p_sys->i_socket == -1 )
        {
            if( read( 0/*STDIN_FILENO*/, p_buffer + *pi_size, 1 ) <= 0 )
            {   /* Standard input closed: exit */
                libvlc_Quit( vlc_object_instance(p_intf) );
                p_buffer[*pi_size] = 0;
                return true;
            }
        }
        else
        {   /* Connection closed */
            if( net_Read( p_intf, p_intf->p_sys->i_socket, p_buffer + *pi_size,
                          1 ) <= 0 )
            {
                net_Close( p_intf->p_sys->i_socket );
                p_intf->p_sys->i_socket = -1;
                p_buffer[*pi_size] = 0;
                return true;
            }
        }

        if( p_buffer[ *pi_size ] == '\r' || p_buffer[ *pi_size ] == '\n' )
            break;

        (*pi_size)++;
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

#if defined(_WIN32) && !VLC_WINSTORE_APP
    /* Get the file descriptor of the console input */
    p_intf->p_sys->hConsoleIn = GetStdHandle(STD_INPUT_HANDLE);
    if( p_intf->p_sys->hConsoleIn == INVALID_HANDLE_VALUE )
    {
        msg_Err( p_intf, "couldn't find user input handle" );
        return NULL;
    }
#endif

    /* Register commands that will be cleaned up upon object destruction */
    vlc_player_t *player = vlc_playlist_GetPlayer(p_sys->playlist);
    input_item_t *item = NULL;

    /* status callbacks */

    for( ;; )
    {
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

        vlc_player_Lock(player);
        /* Manage the input part */
        if( item == NULL )
        {
            item = vlc_player_GetCurrentMedia(player);
            /* New input has been registered */
            if( item )
            {
                char *psz_uri = input_item_GetURI( item );
                msg_rc( STATUS_CHANGE "( new input: %s )", psz_uri );
                free( psz_uri );
            }
        }

        if( item && b_showpos )
        {
            i_newpos = 100 * vlc_player_GetPosition( player );
            if( i_oldpos != i_newpos )
            {
                i_oldpos = i_newpos;
                msg_rc( "pos: %d%%", i_newpos );
            }
        }
        vlc_player_Unlock(player);

        /* Is there something to do? */
        if( !b_complete ) continue;

        Process(p_intf, p_buffer);

        /* Command processed */
        i_size = 0; p_buffer[0] = 0;
    }

    msg_rc( STATUS_CHANGE "( stop state: 0 )" );
    msg_rc( STATUS_CHANGE "( quit )" );

    vlc_restorecancel( canc );

    return NULL;
}

/*****************************************************************************
 * Activate: initialize and create stuff
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    /* FIXME: This function is full of memory leaks and bugs in error paths. */
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
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
#ifdef AF_LOCAL
    psz_unix_path = var_InheritString( p_intf, "rc-unix" );
    if( psz_unix_path )
    {
        int i_socket;
        struct sockaddr_un addr;

        memset( &addr, 0, sizeof(struct sockaddr_un) );

        msg_Dbg( p_intf, "trying UNIX socket" );

        /* The given unix path cannot be longer than sun_path - 1 to take into
         * account the terminated null character. */
        if ( strlen(psz_unix_path) + 1 >= sizeof( addr.sun_path ) )
        {
            msg_Err( p_intf, "rc-unix value is longer than expected" );
            return VLC_EGENERIC;
        }

        if( (i_socket = vlc_socket( PF_LOCAL, SOCK_STREAM, 0, false ) ) < 0 )
        {
            msg_Warn( p_intf, "can't open socket: %s", vlc_strerror_c(errno) );
            free( psz_unix_path );
            return VLC_EGENERIC;
        }

        addr.sun_family = AF_LOCAL;
        strncpy( addr.sun_path, psz_unix_path, sizeof( addr.sun_path ) - 1 );
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
                msg_Err (p_intf, "cannot bind UNIX socket at %s: %s",
                         psz_unix_path, vlc_strerror_c(errno));
                free (psz_unix_path);
                net_Close (i_socket);
                return VLC_EGENERIC;
            }
        }

        if( listen( i_socket, 1 ) )
        {
            msg_Warn (p_intf, "can't listen on socket: %s",
                      vlc_strerror_c(errno));
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
    }
#endif /* AF_LOCAL */
#endif /* !_WIN32 */

    if( ( pi_socket == NULL ) &&
        ( psz_host = var_InheritString( p_intf, "rc-host" ) ) != NULL )
    {
        vlc_url_t url;

        vlc_UrlParse( &url, psz_host );
        if( url.psz_host == NULL )
        {
            vlc_UrlClean( &url );
            char *psz_backward_compat_host;
            if( asprintf( &psz_backward_compat_host, "//%s", psz_host ) < 0 )
            {
                free( psz_host );
                return VLC_EGENERIC;
            }
            free( psz_host );
            psz_host = psz_backward_compat_host;
            vlc_UrlParse( &url, psz_host );
        }

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
    p_sys->commands = NULL;
    p_sys->pi_socket_listen = pi_socket;
    p_sys->i_socket = -1;
#ifdef AF_LOCAL
    p_sys->psz_unix_path = psz_unix_path;
#endif
    p_sys->playlist = vlc_intf_GetMainPlaylist(p_intf);;

    RegisterHandlers(p_intf, cmds, ARRAY_SIZE(cmds));

    /* Non-buffered stdout */
    setvbuf( stdout, (char *)NULL, _IOLBF, 0 );

#if VLC_WINSTORE_APP
    p_sys->b_quiet = true;
#elif defined(_WIN32)
    p_sys->b_quiet = var_InheritBool( p_intf, "rc-quiet" );
    if( !p_sys->b_quiet )
        intf_consoleIntroMsg( p_intf );
#endif

    p_sys->player_cli = RegisterPlayer(p_intf);
    if (unlikely(p_sys->player_cli == NULL))
        goto error;

    if( vlc_clone( &p_sys->thread, Run, p_intf, VLC_THREAD_PRIORITY_LOW ) )
        goto error;

    msg_rc( "%s", _("Remote control interface initialized. Type `help' for help.") );

    return VLC_SUCCESS;

error:
    net_ListenClose( pi_socket );
    free( psz_unix_path );
    free( p_sys );
    return VLC_EGENERIC;
}

static void dummy_free(void *p)
{
    (void) p;
}

/*****************************************************************************
 * Deactivate: uninitialize and cleanup
 *****************************************************************************/
static void Deactivate( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    vlc_cancel( p_sys->thread );
    vlc_join( p_sys->thread, NULL );

    DeregisterPlayer(p_intf, p_sys->player_cli);
    tdestroy(p_sys->commands, dummy_free);

    net_ListenClose( p_sys->pi_socket_listen );
    if( p_sys->i_socket != -1 )
        net_Close( p_sys->i_socket );
#if defined(AF_LOCAL) && !defined(_WIN32)
    if( p_sys->psz_unix_path != NULL )
    {
        unlink( p_sys->psz_unix_path );
        free( p_sys->psz_unix_path );
    }
#endif
    free( p_sys );
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

#ifdef _WIN32
#define QUIET_TEXT N_("Do not open a DOS command box interface")
#define QUIET_LONGTEXT N_( \
    "By default the rc interface plugin will start a DOS command box. " \
    "Enabling the quiet mode will not bring this command box but can also " \
    "be pretty annoying when you want to stop VLC and no video window is " \
    "open." )
#endif

vlc_module_begin()
    set_shortname(N_("RC"))
    set_category(CAT_INTERFACE)
    set_subcategory(SUBCAT_INTERFACE_MAIN)
    set_description(N_("Remote control interface"))
    add_bool("rc-show-pos", false, POS_TEXT, POS_LONGTEXT, true)

#ifdef _WIN32
    add_bool("rc-quiet", false, QUIET_TEXT, QUIET_LONGTEXT, false)
#else
#if defined (HAVE_ISATTY)
    add_bool("rc-fake-tty", false, TTY_TEXT, TTY_LONGTEXT, true)
#endif
#ifdef AF_LOCAL
    add_string("rc-unix", NULL, UNIX_TEXT, UNIX_LONGTEXT, true)
#endif
#endif
    add_string("rc-host", NULL, HOST_TEXT, HOST_LONGTEXT, true)

    set_capability("interface", 20)

    set_callbacks(Activate, Deactivate)
    add_shortcut("cli", "rc", "oldrc")
vlc_module_end()
