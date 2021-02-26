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
#include <sys/stat.h>
#include <fcntl.h>
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

struct intf_sys_t
{
    vlc_thread_t thread;
    void *commands;
    void *player_cli;

#ifndef _WIN32
    vlc_mutex_t clients_lock;
    struct vlc_list clients;
#else
    HANDLE hConsoleIn;
    bool b_quiet;
    int i_socket;
#endif
    int *pi_socket_listen;
};

#define MAX_LINE_LENGTH 1024

struct command {
    union {
        const char *name;
        struct cli_handler handler;
    };
    void *data;
};

static int cmdcmp(const void *a, const void *b)
{
    const char *const *na = a;
    const char *const *nb = b;

    return strcmp(*na, *nb);
}

void RegisterHandlers(intf_thread_t *intf, const struct cli_handler *handlers,
                      size_t count, void *opaque)
{
    intf_sys_t *sys = intf->p_sys;

    for (size_t i = 0; i < count; i++)
    {
        struct command *cmd = malloc(sizeof (*cmd));
        if (unlikely(cmd == NULL))
            break;

        cmd->handler = handlers[i];
        cmd->data = opaque;

        struct command **pp = tsearch(&cmd->name, &sys->commands, cmdcmp);
        if (unlikely(pp == NULL))
        {
            free(cmd);
            continue;
        }

        assert(*pp == cmd); /* Fails if duplicate command */
    }
}

static int Help(struct cli_client *cl, const char *const *args, size_t count,
                void *data)
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
    (void) args; (void) count; (void) data;
    return 0;
}

static int Intf(struct cli_client *cl, const char *const *args, size_t count,
                void *data)
{
    intf_thread_t *intf = data;

    (void) cl;

    return intf_Create(vlc_object_instance(intf), count == 1 ? "" : args[1]);
}

static int Quit(struct cli_client *cl, const char *const *args, size_t count,
                void *data)
{
    intf_thread_t *intf = data;

    libvlc_Quit(vlc_object_instance(intf));
    (void) cl; (void) args; (void) count;
    return 0;
}

static int LogOut(struct cli_client *cl, const char *const *args, size_t count,
                  void *data)
{
    /* Close connection */
#ifndef _WIN32
    /* Force end-of-file on the file descriptor. */
    int fd = vlc_open("/dev/null", O_RDONLY);
    if (fd != -1)
    {   /* POSIX requires flushing before, and seeking after, replacing a
         * file descriptor underneath an I/O stream.
         */
        int fd2 = fileno(cl->stream);

        fflush(cl->stream);
        if (fd2 != 1)
            vlc_dup2(fd, fd2);
        else
            dup2(fd, fd2);
        fseek(cl->stream, 0, SEEK_SET);
        vlc_close(fd);
    }
    (void) data;
#else
    intf_thread_t *intf = data;
    intf_sys_t *sys = intf->p_sys;

    if (sys->i_socket != -1)
    {
        net_Close(sys->i_socket);
        sys->i_socket = -1;
    }
#endif
    (void) args; (void) count;
    return 0;
}

static int KeyAction(struct cli_client *cl, const char *const *args, size_t n,
                     void *data)
{
    intf_thread_t *intf = data;
    vlc_object_t *vlc = VLC_OBJECT(vlc_object_instance(intf));

    if (n != 2)
        return VLC_EGENERIC; /* EINVAL */

    var_SetInteger(vlc, "key-action", vlc_actions_get_id(args[1]));
    (void) cl;
    return 0;
}

static const struct cli_handler cmds[] =
{
    { "longhelp", Help },
    { "h", Help },
    { "help", Help },
    { "H", Help },
    { "?", Help },
    { "logout", LogOut },
    { "quit", Quit },

    { "intf", Intf },
    { "key", KeyAction },
    { "hotkey", KeyAction },
};

static int Process(intf_thread_t *intf, struct cli_client *cl, const char *line)
{
    intf_sys_t *sys = intf->p_sys;
    /* Skip heading spaces */
    const char *cmd = line + strspn(line, " ");
    int ret;

    if (*cmd == '\0')
        return 0; /* Ignore empty line */

#ifdef HAVE_WORDEXP
    wordexp_t we;
    int val = wordexp(cmd, &we, 0);

    if (val != 0)
    {
        if (val == WRDE_NOSPACE)
        {
            ret = VLC_ENOMEM;
error:      wordfree(&we);
        }
        else
            ret = VLC_EGENERIC;

        cli_printf(cl, N_("parse error"));
        return ret;
    }

    size_t count = we.we_wordc;
    const char **args = vlc_alloc(count, sizeof (*args));
    if (unlikely(args == NULL))
    {
        ret = VLC_ENOMEM;
        goto error;
    }

    for (size_t i = 0; i < we.we_wordc; i++)
        args[i] = we.we_wordv[i];
#else
    char *cmd_dup = strdup(cmd);
    if (unlikely(cmd_dup == NULL))
        return VLC_ENOMEM;
    /* Split psz_cmd at the first space and make sure that
     * psz_arg is valid */
    const char *args[] = { cmd_dup, NULL };
    size_t count = 1;
    char *arg = strchr(cmd_dup, ' ');

    if (arg != NULL)
    {
        *(arg++) = '\0';
        arg += strspn(arg, " ");

        if (*arg)
            args[count++] = arg;
    }
#endif

    if (count > 0)
    {
        const struct command **pp = tfind(&args[0], &sys->commands, cmdcmp);

        if (pp != NULL)
        {
            const struct command *c = *pp;;

            ret = c->handler.callback(cl, args, count, c->data);
        }
        else
        {
            cli_printf(cl, _("Unknown command `%s'. Type `help' for help."),
                      args[0]);
            ret = VLC_EGENERIC;
        }
    }

#ifdef HAVE_WORDEXP
    free(args);
    wordfree(&we);
#else
    free(cmd_dup);
#endif
    return ret;
}

#ifndef _WIN32
static ssize_t cli_writev(struct cli_client *cl,
                          const struct iovec *iov, unsigned iovlen)
{
    ssize_t val;

    vlc_mutex_lock(&cl->output_lock);
    if (cl->fd != -1)
        val = vlc_writev(cl->fd, iov, iovlen);
    else
        errno = EPIPE, val = -1;
    vlc_mutex_unlock(&cl->output_lock);
    return val;
}

static int cli_vprintf(struct cli_client *cl, const char *fmt, va_list args)
{
    char *msg;
    int len = vasprintf(&msg, fmt, args);

    if (likely(len >= 0))
    {
        struct iovec iov[2] = { { msg, len }, { (char *)"\n", 1 } };

        cli_writev(cl, iov, ARRAY_SIZE(iov));
        len++;
        free(msg);
    }
    return len;
}

int cli_printf(struct cli_client *cl, const char *fmt, ...)
{
    va_list ap;
    int len;

    va_start(ap, fmt);
    len = cli_vprintf(cl, fmt, ap);
    va_end(ap);
    return len;
}

static void msg_vprint(intf_thread_t *p_intf, const char *fmt, va_list args)
{
    intf_sys_t *sys = p_intf->p_sys;
    struct cli_client *cl;

    vlc_mutex_lock(&sys->clients_lock);
    vlc_list_foreach (cl, &sys->clients, node)
        cli_vprintf(cl, fmt, args);
    vlc_mutex_unlock(&sys->clients_lock);
}

void msg_print(intf_thread_t *intf, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    msg_vprint(intf, fmt, ap);
    va_end(ap);
}

#ifdef __OS2__
static char *os2_fgets(char *buffer, int n, FILE *stream)
{
    if( stream == stdin )
    {
        /* stdin ? Then wait for a stroke before entering into fgets() for
         * cancellation. */
        KBDKEYINFO key;

        while( KbdPeek( &key, 0 ) || !( key.fbStatus & 0x40 ))
            vlc_tick_sleep( INTF_IDLE_SLEEP );
    }

    vlc_testcancel();

    return fgets(buffer, n, stream);
}

#define fgets(buffer, n, stream) os2_fgets(buffer, n, stream)
#endif

static void *cli_client_thread(void *data)
{
    struct cli_client *cl = data;
    intf_thread_t *intf = cl->intf;
    char cmd[MAX_LINE_LENGTH + 1];

    while (fgets(cmd, sizeof (cmd), cl->stream) != NULL)
    {
        int canc = vlc_savecancel();
        if (cmd[0] != '\0')
            cmd[strlen(cmd) - 1] = '\0'; /* remove trailing LF */
        Process(intf, cl, cmd);
        vlc_restorecancel(canc);
    }

    if (cl->stream == stdin)
    {
        int canc = vlc_savecancel();
        libvlc_Quit(vlc_object_instance(intf));
        vlc_restorecancel(canc);
    }

    atomic_store_explicit(&cl->zombie, true, memory_order_release);
    return NULL;
}

static struct cli_client *cli_client_new(intf_thread_t *intf, int fd,
                                         FILE *stream)
{
    struct cli_client *cl = malloc(sizeof (*cl));
    if (unlikely(cl == NULL))
        return NULL;

    cl->stream = stream;
    cl->fd = fd;
    atomic_init(&cl->zombie, false);
    cl->intf = intf;
    vlc_mutex_init(&cl->output_lock);

    if (vlc_clone(&cl->thread, cli_client_thread, cl, VLC_THREAD_PRIORITY_LOW))
    {
        free(cl);
        cl = NULL;
    }
    return cl;
}

/**
 * Creates a client from a file descriptor.
 *
 * This works with (pseudo-)terminals, stream sockets, serial ports, etc.
 */
static struct cli_client *cli_client_new_fd(intf_thread_t *intf, int fd)
{
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);

    FILE *stream = fdopen(fd, "r");
    if (stream == NULL)
    {
        vlc_close(fd);
        return NULL;
    }

    struct cli_client *cl = cli_client_new(intf, fd, stream);
    if (unlikely(cl == NULL))
        fclose(stream);
    return cl;
}

/**
 * Creates a client from the standard input and output.
 */
static struct cli_client *cli_client_new_std(intf_thread_t *intf)
{
    return cli_client_new(intf, 1, stdin);
}

static void cli_client_delete(struct cli_client *cl)
{
    vlc_cancel(cl->thread);
    vlc_join(cl->thread, NULL);

    if (cl->stream != stdin)
        fclose(cl->stream);
    free(cl);
}

static void *Run(void *data)
{
    intf_thread_t *intf = data;
    intf_sys_t *sys = intf->p_sys;

    assert(sys->pi_socket_listen != NULL);

    for (;;)
    {
        int fd = net_Accept(intf, sys->pi_socket_listen);
        if (fd == -1)
            continue;

        int canc = vlc_savecancel();
        struct cli_client *cl = cli_client_new_fd(intf, fd);

        if (cl != NULL)
        {
            vlc_mutex_lock(&sys->clients_lock);
            vlc_list_append(&cl->node, &sys->clients);
            vlc_mutex_unlock(&sys->clients_lock);
        }

        /* Reap any dead client */
        vlc_list_foreach (cl, &sys->clients, node)
            if (atomic_load_explicit(&cl->zombie, memory_order_acquire))
            {
                vlc_mutex_lock(&sys->clients_lock);
                vlc_list_remove(&cl->node);
                vlc_mutex_unlock(&sys->clients_lock);
                cli_client_delete(cl);
            }

        vlc_restorecancel(canc);
    }
}

#else
static void msg_vprint(intf_thread_t *p_intf, const char *psz_fmt, va_list args)
{
    char fmt_eol[strlen (psz_fmt) + 3], *msg;
    int len;

    snprintf (fmt_eol, sizeof (fmt_eol), "%s\r\n", psz_fmt);
    len = vasprintf( &msg, fmt_eol, args );

    if( len < 0 )
        return;

    if( p_intf->p_sys->i_socket == -1 )
        utf8_fprintf( stdout, "%s", msg );
    else
        net_Write( p_intf, p_intf->p_sys->i_socket, msg, len );

    free( msg );
}

void msg_print(intf_thread_t *intf, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    msg_vprint(intf, fmt, ap);
    va_end(ap);
}

int cli_printf(struct cli_client *cl, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    msg_vprint(cl->intf, fmt, ap);
    va_end(ap);
    return VLC_SUCCESS;
}

#if !VLC_WINSTORE_APP
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
#if !VLC_WINSTORE_APP
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

    int  i_size = 0;
    int  canc = vlc_savecancel( );

    p_buffer[0] = 0;

#if !VLC_WINSTORE_APP
    /* Get the file descriptor of the console input */
    p_intf->p_sys->hConsoleIn = GetStdHandle(STD_INPUT_HANDLE);
    if( p_intf->p_sys->hConsoleIn == INVALID_HANDLE_VALUE )
    {
        msg_Err( p_intf, "couldn't find user input handle" );
        return NULL;
    }
#endif

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

        /* Is there something to do? */
        if( !b_complete ) continue;

        struct cli_client cl = { p_intf };

        Process(p_intf, &cl, p_buffer);

        /* Command processed */
        i_size = 0; p_buffer[0] = 0;
    }

    vlc_assert_unreachable();
}

#undef msg_rc
#define msg_rc(...)  msg_print(p_intf, __VA_ARGS__)
#include "../intromsg.h"
#endif

/*****************************************************************************
 * Activate: initialize and create stuff
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    struct cli_client *cl;
    char *psz_host;
    int  *pi_socket = NULL;

    intf_sys_t *p_sys = vlc_obj_malloc(p_this, sizeof (*p_sys));
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    p_intf->p_sys = p_sys;
    p_sys->commands = NULL;
#ifndef _WIN32
    vlc_mutex_init(&p_sys->clients_lock);
    vlc_list_init(&p_sys->clients);
#endif
    RegisterHandlers(p_intf, cmds, ARRAY_SIZE(cmds), p_intf);

    p_sys->player_cli = RegisterPlayer(p_intf);
    if (unlikely(p_sys->player_cli == NULL))
        goto error;

    RegisterPlaylist(p_intf);

#ifndef _WIN32
# ifndef HAVE_ISATTY
#  define isatty(fd) (fd, 0)
# endif
    /* Start CLI on the standard input if it is an actual console */
    if (isatty(fileno(stdin)) || var_InheritBool(p_intf, "rc-fake-tty"))
    {
        cl = cli_client_new_std(p_intf);
        if (cl == NULL)
            goto error;
        vlc_list_append(&cl->node, &p_sys->clients);
    }

#ifdef AF_LOCAL
    char *psz_unix_path = var_InheritString(p_intf, "rc-unix");
    if( psz_unix_path )
    {
        int i_socket;
        struct sockaddr_un addr = { .sun_family = AF_LOCAL };
        struct stat st;

        msg_Dbg( p_intf, "trying UNIX socket" );

        /* The given unix path cannot be longer than sun_path - 1 to take into
         * account the terminated null character. */
        size_t len = strlen(psz_unix_path);
        if (len >= sizeof (addr.sun_path))
        {
            msg_Err( p_intf, "rc-unix value is longer than expected" );
            goto error;
        }
        memcpy(addr.sun_path, psz_unix_path, len + 1);
        free(psz_unix_path);

        if( (i_socket = vlc_socket( PF_LOCAL, SOCK_STREAM, 0, false ) ) < 0 )
        {
            msg_Warn( p_intf, "can't open socket: %s", vlc_strerror_c(errno) );
            goto error;
        }

        if (vlc_stat(addr.sun_path, &st) == 0 && S_ISSOCK(st.st_mode))
        {
            msg_Dbg(p_intf, "unlinking old %s socket", addr.sun_path);
            unlink(addr.sun_path);
        }

        if (bind(i_socket, (struct sockaddr *)&addr, sizeof (addr))
         || listen(i_socket, 1))
        {
            msg_Warn (p_intf, "can't listen on socket: %s",
                      vlc_strerror_c(errno));
            net_Close( i_socket );
            goto error;
        }

        /* FIXME: we need a core function to merge listening sockets sets */
        pi_socket = calloc( 2, sizeof( int ) );
        if( pi_socket == NULL )
        {
            net_Close( i_socket );
            goto error;
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
                goto error;
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
            goto error;
        }

        vlc_UrlClean( &url );
        free( psz_host );
    }

    p_sys->pi_socket_listen = pi_socket;

#ifndef _WIN32
    /* Line-buffered stdout */
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (pi_socket != NULL)
#else
    p_sys->i_socket = -1;
#if VLC_WINSTORE_APP
    p_sys->b_quiet = true;
#else
    p_sys->b_quiet = var_InheritBool( p_intf, "rc-quiet" );
    if( !p_sys->b_quiet )
        intf_consoleIntroMsg( p_intf );
#endif
#endif
    if( vlc_clone( &p_sys->thread, Run, p_intf, VLC_THREAD_PRIORITY_LOW ) )
        goto error;

    msg_print(p_intf, "%s",
             _("Remote control interface initialized. Type `help' for help."));

    return VLC_SUCCESS;

error:
    if (p_sys->player_cli != NULL)
        DeregisterPlayer(p_intf, p_sys->player_cli);
#ifndef _WIN32
    vlc_list_foreach (cl, &p_sys->clients, node)
        cli_client_delete(cl);
#endif
    tdestroy(p_sys->commands, free);
    net_ListenClose( pi_socket );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Deactivate: uninitialize and cleanup
 *****************************************************************************/
static void Deactivate( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

#ifndef _WIN32
    if (p_sys->pi_socket_listen != NULL)
#endif
    {
        vlc_cancel(p_sys->thread);
        vlc_join(p_sys->thread, NULL);
    }

    DeregisterPlayer(p_intf, p_sys->player_cli);

#ifndef _WIN32
    struct cli_client *cl;

    vlc_list_foreach (cl, &p_sys->clients, node)
        cli_client_delete(cl);
#endif
    tdestroy(p_sys->commands, free);

    if (p_sys->pi_socket_listen != NULL)
    {
        net_ListenClose(p_sys->pi_socket_listen);
#ifdef _WIN32
        if (p_sys->i_socket != -1)
            net_Close(p_sys->i_socket);
#endif
    }
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
    add_bool("rc-show-pos", false, POS_TEXT, POS_LONGTEXT)

#ifdef _WIN32
    add_bool("rc-quiet", false, QUIET_TEXT, QUIET_LONGTEXT)
#else
#if defined (HAVE_ISATTY)
    add_bool("rc-fake-tty", false, TTY_TEXT, TTY_LONGTEXT)
#endif
#ifdef AF_LOCAL
    add_string("rc-unix", NULL, UNIX_TEXT, UNIX_LONGTEXT)
#endif
#endif
    add_string("rc-host", NULL, HOST_TEXT, HOST_LONGTEXT)

    set_capability("interface", 20)

    set_callbacks(Activate, Deactivate)
    add_shortcut("cli", "rc", "oldrc")
vlc_module_end()
