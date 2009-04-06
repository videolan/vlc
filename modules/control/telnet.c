/*****************************************************************************
 * telnet.c: VLM interface plugin
 *****************************************************************************
 * Copyright (C) 2000-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Simon Latapie <garf@videolan.org>
 *          Laurent Aimar <fenrir@videolan.org>
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
#include <vlc_interface.h>
#include <vlc_input.h>

#include <stdbool.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>

#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif
#ifdef HAVE_POLL
#   include <poll.h>
#endif

#include <vlc_network.h>
#include <vlc_url.h>
#include <vlc_vlm.h>

#define READ_MODE_PWD 1
#define READ_MODE_CMD 2
#define WRITE_MODE_PWD 3 // when we write the word "Password:"
#define WRITE_MODE_CMD 4

/* telnet commands */
#define TEL_WILL    251
#define TEL_WONT    252
#define TEL_DO      253
#define TEL_DONT    254
#define TEL_IAC     255
#define TEL_ECHO    1

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define TELNETHOST_TEXT N_( "Host" )
#define TELNETHOST_LONGTEXT N_( "This is the host on which the " \
    "interface will listen. It defaults to all network interfaces (0.0.0.0)." \
    " If you want this interface to be available only on the local " \
    "machine, enter \"127.0.0.1\"." )
#define TELNETPORT_TEXT N_( "Port" )
#define TELNETPORT_LONGTEXT N_( "This is the TCP port on which this " \
    "interface will listen. It defaults to 4212." )
#define TELNETPORT_DEFAULT 4212
#define TELNETPWD_TEXT N_( "Password" )
#define TELNETPWD_LONGTEXT N_( "A single administration password is used " \
    "to protect this interface. The default value is \"admin\"." )
#define TELNETPWD_DEFAULT "admin"

vlc_module_begin ()
    set_shortname( "Telnet" )
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    add_string( "telnet-host", "", NULL, TELNETHOST_TEXT,
                 TELNETHOST_LONGTEXT, true )
    add_integer( "telnet-port", TELNETPORT_DEFAULT, NULL, TELNETPORT_TEXT,
                 TELNETPORT_LONGTEXT, true )
    add_password( "telnet-password", TELNETPWD_DEFAULT, NULL, TELNETPWD_TEXT,
                TELNETPWD_LONGTEXT, true )
    set_description( N_("VLM remote control interface") )
    add_category_hint( "VLM", NULL, false )
    set_capability( "interface", 0 )
    set_callbacks( Open , Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void Run( intf_thread_t * );

typedef struct
{
    int        i_mode; /* read or write */
    int        fd;
    char       buffer_read[1000]; // 1000 byte per command should be sufficient
    char      *buffer_write;
    char      *p_buffer_read;
    char      *p_buffer_write; // the position in the buffer
    int        i_buffer_write; // the number of byte we still have to send
    int        i_tel_cmd; // for specific telnet commands

} telnet_client_t;

static char *MessageToString( vlm_message_t *, int );
static void Write_message( telnet_client_t *, vlm_message_t *, const char *, int );

struct intf_sys_t
{
   telnet_client_t **clients;
   int             i_clients;
   int             *pi_fd;
   vlm_t           *mediatheque;
};

/*
 * getPort: Decide which port to use. There are two possibilities to
 * specify a port: integrated in the --telnet-host option with :PORT
 * or using the --telnet-port option. The --telnet-port option has
 * precedence.
 * This code relies upon the fact the url.i_port is 0 if the :PORT
 * option is missing from --telnet-host.
 */
static int getPort(intf_thread_t *p_intf, const vlc_url_t *url, int i_port)
{
    if (i_port == TELNETPORT_DEFAULT && url->i_port != 0)
        i_port = url->i_port;
    if (url->i_port != 0 && url->i_port != i_port)
        // Print error if two different ports have been specified
        msg_Warn( p_intf, "ignoring port %d (using %d)", url->i_port, i_port );
    return i_port;
}

/*****************************************************************************
 * Open: initialize dummy interface
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;
    vlm_t *mediatheque;
    char *psz_address;
    vlc_url_t url;
    int i_telnetport;

    if( !(mediatheque = vlm_New( p_intf )) )
    {
        msg_Err( p_intf, "cannot start VLM" );
        return VLC_EGENERIC;
    }

    msg_Info( p_intf, "using the VLM interface plugin..." );

    i_telnetport = config_GetInt( p_intf, "telnet-port" );
    psz_address  = config_GetPsz( p_intf, "telnet-host" );

    vlc_UrlParse(&url, psz_address, 0);
    free( psz_address );

    // There might be two ports given, resolve any potentially
    // conflict
    url.i_port = getPort(p_intf, &url, i_telnetport);

    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( !p_intf->p_sys )
    {
        vlm_Delete( mediatheque );
        vlc_UrlClean( &url );
        return VLC_ENOMEM;
    }
    if( ( p_intf->p_sys->pi_fd = net_ListenTCP( p_intf, url.psz_host, url.i_port ) ) == NULL )
    {
        msg_Err( p_intf, "cannot listen for telnet" );
        vlm_Delete( mediatheque );
        vlc_UrlClean( &url );
        free( p_intf->p_sys );
        return VLC_EGENERIC;
    }
    msg_Info( p_intf,
              "telnet interface started on interface %s %d",
              url.psz_host, url.i_port );

    p_intf->p_sys->i_clients   = 0;
    p_intf->p_sys->clients     = NULL;
    p_intf->p_sys->mediatheque = mediatheque;
    p_intf->pf_run = Run;

    vlc_UrlClean( &url );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    intf_sys_t    *p_sys  = p_intf->p_sys;
    int i;

    for( i = 0; i < p_sys->i_clients; i++ )
    {
        telnet_client_t *cl = p_sys->clients[i];
        net_Close( cl->fd );
        free( cl->buffer_write );
        free( cl );
    }
    free( p_sys->clients );

    net_ListenClose( p_sys->pi_fd );

    vlm_Delete( p_sys->mediatheque );

    free( p_sys );
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    intf_sys_t     *p_sys = p_intf->p_sys;
    char           *psz_password;
    unsigned        nlisten = 0;

    for (const int *pfd = p_sys->pi_fd; *pfd != -1; pfd++)
        nlisten++; /* How many listening sockets do we have? */

    /* FIXME: make sure config_* is cancel-safe */
    psz_password = config_GetPsz( p_intf, "telnet-password" );
    vlc_cleanup_push( free, psz_password );

    for( ;; )
    {
        unsigned ncli = p_sys->i_clients;
        struct pollfd ufd[ncli + nlisten];

        for (unsigned i = 0; i < ncli; i++)
        {
            telnet_client_t *cl = p_sys->clients[i];

            ufd[i].fd = cl->fd;
            if( (cl->i_mode == WRITE_MODE_PWD) || (cl->i_mode == WRITE_MODE_CMD) )
                ufd[i].events = POLLOUT;
            else
                ufd[i].events = POLLIN;
            ufd[i].revents = 0;
        }

        for (unsigned i = 0; i < nlisten; i++)
        {
            ufd[ncli + i].fd = p_sys->pi_fd[i];
            ufd[ncli + i].events = POLLIN;
            ufd[ncli + i].revents = 0;
        }

        switch (poll (ufd, sizeof (ufd) / sizeof (ufd[0]), -1))
        {
            case -1:
                if (net_errno != EINTR)
                {
                    msg_Err (p_intf, "network poll error");
                    msleep (1000);
                    continue;
                }
            case 0:
                continue;
        }

        int canc = vlc_savecancel ();
        /* check if there is something to do with the socket */
        for (unsigned i = 0; i < ncli; i++)
        {
            telnet_client_t *cl = p_sys->clients[i];

            if (ufd[i].revents & (POLLERR|POLLHUP))
            {
            drop:
                net_Close( cl->fd );
                TAB_REMOVE( p_intf->p_sys->i_clients ,
                            p_intf->p_sys->clients , cl );
                free( cl->buffer_write );
                free( cl );
                continue;
            }

            if (ufd[i].revents & POLLOUT && (cl->i_buffer_write > 0))
            {
                ssize_t i_len;

                i_len = send( cl->fd, cl->p_buffer_write ,
                              cl->i_buffer_write, 0 );
                if( i_len > 0 )
                {
                    cl->p_buffer_write += i_len;
                    cl->i_buffer_write -= i_len;
                }
            }
            if (ufd[i].revents & POLLIN)
            {
                bool end = false;
                ssize_t i_recv;

                while( ((i_recv=recv( cl->fd, cl->p_buffer_read, 1, 0 )) > 0) &&
                       ((cl->p_buffer_read - cl->buffer_read) < 999) )
                {
                    switch( cl->i_tel_cmd )
                    {
                    case 0:
                        switch( *(uint8_t *)cl->p_buffer_read )
                        {
                        case '\r':
                            break;
                        case '\n':
                            *cl->p_buffer_read = '\n';
                            end = true;
                            break;
                        case TEL_IAC: // telnet specific command
                            cl->i_tel_cmd = 1;
                            cl->p_buffer_read++;
                            break;
                        default:
                            cl->p_buffer_read++;
                            break;
                        }
                        break;
                    case 1:
                        switch( *(uint8_t *)cl->p_buffer_read )
                        {
                        case TEL_WILL: case TEL_WONT:
                        case TEL_DO: case TEL_DONT:
                            cl->i_tel_cmd++;
                            cl->p_buffer_read++;
                            break;
                        default:
                            cl->i_tel_cmd = 0;
                            cl->p_buffer_read--;
                            break;
                        }
                        break;
                    case 2:
                        cl->i_tel_cmd = 0;
                        cl->p_buffer_read -= 2;
                        break;
                    }

                    if( end ) break;
                }

                if( (cl->p_buffer_read - cl->buffer_read) == 999 )
                {
                    Write_message( cl, NULL, "Line too long\r\n",
                                   cl->i_mode + 2 );
                }

#ifdef WIN32
                if( i_recv <= 0 && WSAGetLastError() == WSAEWOULDBLOCK )
                {
                    errno = EAGAIN;
                }
#endif
                if( i_recv == 0 || ( i_recv == -1 && ( end || errno != EAGAIN ) ) )
                    goto drop;
            }
        }

        /* and now we should bidouille the data we received / send */
        for(int i = 0 ; i < p_sys->i_clients ; i++ )
        {
            telnet_client_t *cl = p_sys->clients[i];

            if( cl->i_mode >= WRITE_MODE_PWD && cl->i_buffer_write == 0 )
            {
               // we have finished to send
               cl->i_mode -= 2; // corresponding READ MODE
            }
            else if( cl->i_mode == READ_MODE_PWD &&
                     *cl->p_buffer_read == '\n' )
            {
                *cl->p_buffer_read = '\0';
                if( !psz_password || !strcmp( psz_password, cl->buffer_read ) )
                {
                    Write_message( cl, NULL, "\xff\xfc\x01\r\nWelcome, "
                                   "Master\r\n> ", WRITE_MODE_CMD );
                }
                else
                {
                    /* wrong password */
                    Write_message( cl, NULL,
                                   "\r\nWrong password.\r\nPassword: ",
                                   WRITE_MODE_PWD );
                }
            }
            else if( cl->i_mode == READ_MODE_CMD &&
                     *cl->p_buffer_read == '\n' )
            {
                /* ok, here is a command line */
                if( !strncmp( cl->buffer_read, "logout", 6 ) ||
                    !strncmp( cl->buffer_read, "quit", 4 )  ||
                    !strncmp( cl->buffer_read, "exit", 4 ) )
                {
                    net_Close( cl->fd );
                    TAB_REMOVE( p_intf->p_sys->i_clients ,
                                p_intf->p_sys->clients , cl );
                    free( cl->buffer_write );
                    free( cl );
                }
                else if( !strncmp( cl->buffer_read, "shutdown", 8 ) )
                {
                    msg_Err( p_intf, "shutdown requested" );
                    libvlc_Quit( p_intf->p_libvlc );
                }
                else if( *cl->buffer_read == '@'
                          && strchr( cl->buffer_read, ' ' ) )
                {
                    /* Module specific commands (use same syntax as in the
                     * rc interface) */
                    char *psz_name = cl->buffer_read + 1;
                    char *psz_cmd, *psz_arg, *psz_msg;
                    int i_ret;

                    psz_cmd = strchr( cl->buffer_read, ' ' );
                    *psz_cmd = '\0';  psz_cmd++;
                    if( ( psz_arg = strchr( psz_cmd, '\n' ) ) ) *psz_arg = '\0';
                    if( ( psz_arg = strchr( psz_cmd, '\r' ) ) ) *psz_arg = '\0';
                    if( ( psz_arg = strchr( psz_cmd, ' ' ) )
                        && *psz_arg )
                    {
                        *psz_arg = '\0';
                        psz_arg++;
                    }

                    i_ret = var_Command( p_intf, psz_name, psz_cmd, psz_arg,
                                         &psz_msg );

                    if( psz_msg )
                    {
                        vlm_message_t *message;
                        message = vlm_MessageNew( "Module command", "%s", psz_msg );
                        Write_message( cl, message, NULL, WRITE_MODE_CMD );
                        vlm_MessageDelete( message );
                        free( psz_msg );
                    }
                }
                else
                {
                    vlm_message_t *message;

                    /* create a standard string */
                    *cl->p_buffer_read = '\0';

                    vlm_ExecuteCommand( p_sys->mediatheque, cl->buffer_read,
                                        &message );
                    if( !strncmp( cl->buffer_read, "help", 4 ) )
                    {
                        vlm_message_t *p_my_help =
                            vlm_MessageSimpleNew( "Telnet Specific Commands:" );
                        vlm_MessageAdd( p_my_help,
                            vlm_MessageSimpleNew( "logout, quit, exit" ) );
                        vlm_MessageAdd( p_my_help,
                            vlm_MessageSimpleNew( "shutdown" ) );
                        vlm_MessageAdd( p_my_help,
                            vlm_MessageSimpleNew( "@moduleinstance command argument" ) );
                        vlm_MessageAdd( message, p_my_help );
                    }
                    Write_message( cl, message, NULL, WRITE_MODE_CMD );
                    vlm_MessageDelete( message );
                }
            }
        }

        /* handle new connections */
        for (unsigned i = 0; i < nlisten; i++)
        {
            int fd;

            if (ufd[ncli + i].revents == 0)
                continue;

            fd = net_AcceptSingle (VLC_OBJECT(p_intf), ufd[ncli + i].fd);
            if (fd == -1)
                continue;

            telnet_client_t *cl = calloc( 1, sizeof( telnet_client_t ));
            if (cl == NULL)
            {
                net_Close (fd);
                continue;
            }

            cl->i_tel_cmd = 0;
            cl->fd = fd;
            cl->buffer_write = NULL;
            cl->p_buffer_write = cl->buffer_write;
            Write_message( cl, NULL,
                           "Password: \xff\xfb\x01" , WRITE_MODE_PWD );
            TAB_APPEND( p_sys->i_clients, p_sys->clients, cl );
        }
        vlc_restorecancel( canc );
    }
    vlc_cleanup_run ();
}

static void Write_message( telnet_client_t *client, vlm_message_t *message,
                           const char *string_message, int i_mode )
{
    char *psz_message;

    client->p_buffer_read = client->buffer_read;
    (client->p_buffer_read)[0] = 0; // if (cl->p_buffer_read)[0] = '\n'
    free( client->buffer_write );

    /* generate the psz_message string */
    if( message )
    {
        /* ok, look for vlm_message_t */
        psz_message = MessageToString( message, 0 );
    }
    else
    {
        /* it is a basic string_message */
        psz_message = strdup( string_message );
    }

    client->buffer_write = client->p_buffer_write = psz_message;
    client->i_buffer_write = strlen( psz_message );
    client->i_mode = i_mode;
}

/* We need the level of the message to put a beautiful indentation.
 * first level is 0 */
static char *MessageToString( vlm_message_t *message, int i_level )
{
#define STRING_CR "\r\n"
#define STRING_TAIL "> "

    char *psz_message;
    int i, i_message = sizeof( STRING_TAIL );

    if( !message || !message->psz_name )
    {
        return strdup( STRING_CR STRING_TAIL );
    }
    else if( !i_level && !message->i_child && !message->psz_value  )
    {
        /* A command is successful. Don't write anything */
        return strdup( /*STRING_CR*/ STRING_TAIL );
    }

    i_message += strlen( message->psz_name ) + i_level * sizeof( "    " ) + 1;
    psz_message = malloc( i_message );
    *psz_message = 0;
    for( i = 0; i < i_level; i++ ) strcat( psz_message, "    " );
    strcat( psz_message, message->psz_name );

    if( message->psz_value )
    {
        i_message += sizeof( " : " ) + strlen( message->psz_value ) +
            sizeof( STRING_CR );
        psz_message = realloc( psz_message, i_message );
        strcat( psz_message, " : " );
        strcat( psz_message, message->psz_value );
        strcat( psz_message, STRING_CR );
    }
    else
    {
        i_message += sizeof( STRING_CR );
        psz_message = realloc( psz_message, i_message );
        strcat( psz_message, STRING_CR );
    }

    for( i = 0; i < message->i_child; i++ )
    {
        char *child_message =
            MessageToString( message->child[i], i_level + 1 );

        i_message += strlen( child_message );
        psz_message = realloc( psz_message, i_message );
        strcat( psz_message, child_message );
        free( child_message );
    }

    if( i_level == 0 ) strcat( psz_message, STRING_TAIL );

    return psz_message;
}
