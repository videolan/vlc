/*****************************************************************************
 * telnet.c: VLM interface plugin
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: telnet.c,v 1.1 2004/03/05 14:03:20 garf Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include <vlc/input.h>

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>

#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#if defined( UNDER_CE )
#   include <winsock.h>
#elif defined( WIN32 )
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   ifndef IN_MULTICAST
#       define IN_MULTICAST(a) IN_CLASSD(a)
#   endif
#else
#   include <netdb.h>                                         /* hostent ... */
#   include <sys/socket.h>
#   include <netinet/in.h>
#   ifdef HAVE_ARPA_INET_H
#       include <arpa/inet.h>                    /* inet_ntoa(), inet_aton() */
#   endif
#endif

#include "network.h"

#include "vlm.h"

#if defined( WIN32 ) || defined( UNDER_CE )
#define SOCKET_CLOSE(a)    closesocket(a)
#else
#define SOCKET_CLOSE(a)    close(a)
#endif

#define LISTEN_BACKLOG 100

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

#define TELNETPORT_TEXT N_( "Telnet Interface port" )
#define TELNETPORT_LONGTEXT N_( "Default to 4212" )
#define TELNETPWD_TEXT N_( "Telnet Interface password" )
#define TELNETPWD_LONGTEXT N_( "Default to admin" )

vlc_module_begin();
    add_integer( "telnet-port", 4212, NULL, TELNETPORT_TEXT, TELNETPORT_LONGTEXT, VLC_TRUE );
    add_string( "telnet-password", "admin", NULL,
                    TELNETPWD_TEXT, TELNETPWD_LONGTEXT, VLC_TRUE );
    set_description( _("telnet remote control interface") );
    add_category_hint( N_( "VLM" ), NULL, VLC_FALSE );
    set_capability( "interface", 0 );
    set_callbacks( Open , Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void Run( intf_thread_t * );

typedef struct
{
    int        i_mode; /* read or write */
    int        fd;
    uint8_t    buffer_read[1000]; // 1000 byte per command should be sufficient
    char      *buffer_write;
    uint8_t   *p_buffer_read;
    uint8_t   *p_buffer_write; // the position in the buffer
    int        i_buffer_write; // the number of byte we still have to send
    int        i_tel_cmd; // for specific telnet commands
} telnet_client_t;

static void Write_message( telnet_client_t * , char * , int );
static int  SocketListen( intf_thread_t * , int );

struct intf_sys_t
{
   telnet_client_t **clients;
   int             i_clients;
   int             fd;
   vlm_t          *mediatheque;
};

/*****************************************************************************
 * Open: initialize dummy interface
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;
    int i_telnetport;

    i_telnetport = config_GetInt( p_intf, "telnet-port" );

#ifdef WIN32
    vlc_bool_t b_quiet;
    b_quiet = config_GetInt( p_intf, "dummy-quiet" );
    if( !b_quiet )
        CONSOLE_INTRO_MSG;
#endif

    msg_Info( p_intf, _("Using the VLM interface plugin...") );

    p_intf->pf_run = Run;

    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( ( p_intf->p_sys->fd = SocketListen( p_intf , i_telnetport ) ) < 0 )
    {
        msg_Err( p_intf, "cannot listen for telnet" );
        free( p_intf->p_sys );
        return VLC_EGENERIC;
    }
    p_intf->p_sys->i_clients   = 0;
    p_intf->p_sys->clients     = NULL;
    p_intf->p_sys->mediatheque = vlm_New( p_intf );

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
        free( cl );
    }
    if( p_sys->clients != NULL ) free( p_sys->clients );

    net_Close( p_sys->fd );

    vlm_Delete( p_sys->mediatheque );

    free( p_sys );
}


/*****************************************************************************
 * Run: main loop
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    intf_sys_t     *p_sys = p_intf->p_sys;
    struct timeval  timeout;
    int             i_sock_size = sizeof( struct sockaddr_in );
    char           *s_password;

    s_password = config_GetPsz( p_intf, "telnet-password" );

    while( !p_intf->b_die )
    {
        fd_set          fds_read;
        fd_set          fds_write;
        int             i_handle_max = 0;
        int             i_ret;
        struct          sockaddr_in sock2;
        int             i_len;
        int             fd;
        int             i;

        /* if a new client wants to communicate */
        fd = accept( p_sys->fd, (struct sockaddr *)&sock2, &i_sock_size );
        if( fd > 0 )
        {
            telnet_client_t *cl;

            /* to be non blockant */
#if defined( WIN32 ) || defined( UNDER_CE )
            {
                unsigned long i_dummy = 1;
                ioctlsocket( fd, FIONBIO, &i_dummy );
            }
#else
            fcntl( fd, F_SETFL, O_NONBLOCK );
#endif
            cl = malloc( sizeof( telnet_client_t ));
            cl->i_tel_cmd = 0;
            cl->fd = fd;
            cl->buffer_write = NULL;
            cl->p_buffer_write = cl->buffer_write;
            Write_message( cl, "Password:\xff\xfb\x01", WRITE_MODE_PWD );

            TAB_APPEND( p_sys->i_clients, p_sys->clients, cl );
        }

        /* to do a proper select */
        FD_ZERO( &fds_read );
        FD_ZERO( &fds_write );

        for( i = 0 ; i < p_sys->i_clients ; i++ )
        {
            telnet_client_t *cl = p_sys->clients[i];

            if( cl->i_mode == WRITE_MODE_PWD || cl->i_mode == WRITE_MODE_CMD )
            {
                FD_SET( cl->fd , &fds_write );
            }
            else
            {
                FD_SET( cl->fd , &fds_read );
            }
            i_handle_max = __MAX( i_handle_max, cl->fd );
        }

        timeout.tv_sec = 0;
        timeout.tv_usec = 500*1000;

        i_ret = select( i_handle_max + 1, &fds_read, &fds_write, NULL, &timeout );
        if( i_ret == -1 && errno != EINTR )
        {
            msg_Warn( p_intf, "cannot select sockets" );
            msleep( 1000 );
            continue;
        }
        else if( i_ret <= 0 )
        {
            continue;
        }

        /* check if there is something to do with the socket */
        for( i = 0 ; i < p_sys->i_clients ; i++ )
        {
            telnet_client_t *cl = p_sys->clients[i];

            if( FD_ISSET(cl->fd , &fds_write) && cl->i_buffer_write > 0 )
            {
                i_len = send( cl->fd , cl->p_buffer_write ,
                              cl->i_buffer_write , 0 );
                if( i_len > 0 )
                {
                    cl->p_buffer_write += i_len;
                    cl->i_buffer_write -= i_len;
                }
            }
            else if( FD_ISSET( cl->fd, &fds_read) )
            {
                int i_end = 0;

                while( recv( cl->fd, cl->p_buffer_read, 1, 0 ) > 0 &&
                       cl->p_buffer_read - cl->buffer_read < 999 )
                {
                    switch( cl->i_tel_cmd )
                    {
                        case 0:
                            switch( *cl->p_buffer_read )
                            {
                                case '\r':
                                    break;
                                case '\n':
                                    *cl->p_buffer_read = '\n';
                                    i_end = 1;
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
                            switch( *cl->p_buffer_read )
                            {
                                case TEL_WILL: case TEL_WONT: case TEL_DO: case TEL_DONT:
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

                    if( i_end != 0 )
                    {
                        break;
                    }
                }

                if( cl->p_buffer_read - cl->buffer_read == 999 ) // too long !
                {
                    Write_message( cl , "Line too long\n" , cl->i_mode + 2 );
                }
            }
        }

        /* and now we should bidouille the data we received / send */
        for( i = 0 ; i < p_sys->i_clients ; i++ )
        {
            telnet_client_t *cl = p_sys->clients[i];

            if( cl->i_mode >= WRITE_MODE_PWD && cl->i_buffer_write == 0 ) // we have finished to send
            {
               cl->i_mode -= 2; // corresponding READ MODE
            }
            else if( cl->i_mode == READ_MODE_PWD && *cl->p_buffer_read == '\n' )
            {
                *cl->p_buffer_read = '\0';
                if( strcmp( s_password, cl->buffer_read ) == 0 )
                {
                    Write_message( cl , "\xff\xfc\x01\nWelcome, Master\n> ", WRITE_MODE_CMD );
                }
                else
                {
                    /* wrong password */
                    Write_message( cl , "\n\rTry again, you polio:\n" , WRITE_MODE_PWD );
                }
            }
            else if( cl->i_mode == READ_MODE_CMD && *cl->p_buffer_read == '\n' )
            {
                /* ok, here is a command line */
                if( !strncmp( cl->buffer_read, "logout", 6 ) ||
                    !strncmp( cl->buffer_read, "quit", 4 )  ||
                    !strncmp( cl->buffer_read, "exit", 4 ) )
                {
                    close( cl->fd );
                    TAB_REMOVE( p_intf->p_sys->i_clients , p_intf->p_sys->clients , cl );
                    free( cl );
                }
                else if( !strncmp( cl->buffer_read, "shutdown", 8 ) )
                {
                    msg_Err( p_intf, "shutdown requested" );
                    p_intf->p_vlc->b_die = VLC_TRUE;
                }
                else
                {
                    char *message;

                    /* create a standard string */
                    *cl->p_buffer_read = '\0';

                    vlm_ExecuteCommand( p_sys->mediatheque, cl->buffer_read , &message);

                    Write_message( cl , message , WRITE_MODE_CMD );

                    free( message );
                }
            }
        }
    }
}

static void Write_message( telnet_client_t * client, char * message, int i_mode )
{
    client->p_buffer_read = client->buffer_read;
    (client->p_buffer_read)[0] = 0; // if (cl->p_buffer_read)[0] = '\n'
    if( client->buffer_write ) free( client->buffer_write );
    client->buffer_write = malloc( strlen( message ) + 1 );
    strcpy( client->buffer_write , message );
    client->p_buffer_write = client->buffer_write;
    client->i_buffer_write = strlen( message );
    client->i_mode = i_mode;
}

/* Does what we want except select and accept */
static int SocketListen( intf_thread_t *p_intf , int i_port )
{
    struct sockaddr_in sock;
    int fd;
    int i_opt;
    int i_flags;

    /* open socket */
    fd = socket( AF_INET, SOCK_STREAM, 0 );
    if( fd < 0 )
    {
        msg_Err( p_intf, "cannot open socket" );
        goto socket_failed;
    }
    /* reuse socket */
    i_opt = 1;
    if( setsockopt( fd, SOL_SOCKET, SO_REUSEADDR,
                    (void *) &i_opt, sizeof( i_opt ) ) < 0 )
    {
        msg_Warn( p_intf, "cannot configure socket (SO_REUSEADDR)" );
    }

    /* fill p_socket structure */
    memset( &sock, 0, sizeof( struct sockaddr_in ) );
    sock.sin_family = AF_INET;                             /* family */
    sock.sin_port = htons( (uint16_t)i_port );
    sock.sin_addr.s_addr = INADDR_ANY;

    /* bind it */
    if( bind( fd, (struct sockaddr *)&sock, sizeof( struct sockaddr_in ) ) < 0 )
    {
        msg_Err( p_intf, "cannot bind socket" );
        goto socket_failed;
    }

   /* set to non-blocking */
#if defined( WIN32 ) || defined( UNDER_CE )
    {
        unsigned long i_dummy = 1;
        if( ioctlsocket( fd, FIONBIO, &i_dummy ) != 0 )
        {
            msg_Err( p_intf, "cannot set socket to non-blocking mode" );
            goto socket_failed;
        }
    }
#else
    if( ( i_flags = fcntl( fd, F_GETFL, 0 ) ) < 0 )
    {
        msg_Err( p_intf, "cannot F_GETFL socket" );
        goto socket_failed;
    }
    if( fcntl( fd, F_SETFL, i_flags | O_NONBLOCK ) < 0 )
    {
        msg_Err( p_intf, "cannot F_SETFL O_NONBLOCK" );
        goto socket_failed;
    }
#endif
    /* listen */
    if( listen( fd, LISTEN_BACKLOG ) < 0 )
    {
        msg_Err( p_intf, "cannot listen socket" );
        goto socket_failed;
    }

    return fd;

socket_failed:
    if( fd >= 0 )
    {
        SOCKET_CLOSE( fd );
    }
    return -1;
}

