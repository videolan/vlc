/*****************************************************************************
 * telnet.c: VLM interface plugin
 *****************************************************************************
 * Copyright (C) 2000-2005 VideoLAN
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
#else
#   include <sys/socket.h>
#endif

#include "network.h"

#include "vlc_vlm.h"

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
    set_shortname( "Telnet" );
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_GENERAL );
    add_integer( "telnet-port", 4212, NULL, TELNETPORT_TEXT,
                 TELNETPORT_LONGTEXT, VLC_TRUE );
    add_string( "telnet-password", "admin", NULL, TELNETPWD_TEXT,
                TELNETPWD_LONGTEXT, VLC_TRUE );
    set_description( _("VLM remote control interface") );
    add_category_hint( "VLM", NULL, VLC_FALSE );
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

static char *MessageToString( vlm_message_t *, int );
static void Write_message( telnet_client_t *, vlm_message_t *, char *, int );

struct intf_sys_t
{
   telnet_client_t **clients;
   int             i_clients;
   int             *pi_fd;
   vlm_t           *mediatheque;
};

/*****************************************************************************
 * Open: initialize dummy interface
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;
    vlm_t *mediatheque;
    int i_telnetport;

    if( !(mediatheque = vlm_New( p_intf )) )
    {
        msg_Err( p_intf, "cannot start VLM" );
        return VLC_EGENERIC;
    }

    msg_Info( p_intf, "Using the VLM interface plugin..." );

    i_telnetport = config_GetInt( p_intf, "telnet-port" );

    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( ( p_intf->p_sys->pi_fd = net_ListenTCP( p_intf , "", i_telnetport ) )
                == NULL )
    {
        msg_Err( p_intf, "cannot listen for telnet" );
        free( p_intf->p_sys );
        return VLC_EGENERIC;
    }
    msg_Info( p_intf, "Telnet interface started on port: %d", i_telnetport );

    p_intf->p_sys->i_clients   = 0;
    p_intf->p_sys->clients     = NULL;
    p_intf->p_sys->mediatheque = mediatheque;
    p_intf->pf_run = Run;

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
    struct timeval  timeout;
    char           *psz_password;

    psz_password = config_GetPsz( p_intf, "telnet-password" );

    while( !p_intf->b_die )
    {
        fd_set fds_read, fds_write;
        int    i_handle_max = 0;
        int    i_ret, i_len, fd, i;

        /* if a new client wants to communicate */
        fd = net_Accept( p_intf, p_sys->pi_fd, p_sys->i_clients > 0 ? 0 : -1 );
        if( fd > 0 )
        {
            telnet_client_t *cl;

            /* to be non blocking */
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
            Write_message( cl, NULL, "Password:\xff\xfb\x01", WRITE_MODE_PWD );

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

        i_ret = select( i_handle_max + 1, &fds_read, &fds_write, 0, &timeout );
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
                int i_recv;

                while( (i_recv=recv( cl->fd, cl->p_buffer_read, 1, 0 )) > 0 &&
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

                    if( i_end != 0 ) break;
                }

                if( cl->p_buffer_read - cl->buffer_read == 999 )
                {
                    Write_message( cl, NULL, "Line too long\r\n",
                                   cl->i_mode + 2 );
                }

                if (i_recv == 0)
                {
                    net_Close( cl->fd );
                    TAB_REMOVE( p_intf->p_sys->i_clients ,
                                p_intf->p_sys->clients , cl );
                    free( cl );
                }
            }
        }

        /* and now we should bidouille the data we received / send */
        for( i = 0 ; i < p_sys->i_clients ; i++ )
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
                if( strcmp( psz_password, cl->buffer_read ) == 0 )
                {
                    Write_message( cl, NULL, "\xff\xfc\x01\r\nWelcome, "
                                   "Master\r\n> ", WRITE_MODE_CMD );
                }
                else
                {
                    /* wrong password */
                    Write_message( cl, NULL, "\r\nWrong password. ",
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
                    free( cl );
                }
                else if( !strncmp( cl->buffer_read, "shutdown", 8 ) )
                {
                    msg_Err( p_intf, "shutdown requested" );
                    p_intf->p_vlc->b_die = VLC_TRUE;
                }
                else
                {
                    vlm_message_t *message;

                    /* create a standard string */
                    *cl->p_buffer_read = '\0';

                    vlm_ExecuteCommand( p_sys->mediatheque, cl->buffer_read,
                                        &message );
                    Write_message( cl, message, NULL, WRITE_MODE_CMD );
                    vlm_MessageDelete( message );
                }
            }
        }
    }
}

static void Write_message( telnet_client_t *client, vlm_message_t *message,
                           char *string_message, int i_mode )
{
    char *psz_message;

    client->p_buffer_read = client->buffer_read;
    (client->p_buffer_read)[0] = 0; // if (cl->p_buffer_read)[0] = '\n'
    if( client->buffer_write ) free( client->buffer_write );

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
        return strdup( STRING_CR STRING_TAIL );
    }

    i_message += strlen( message->psz_name ) + i_level * sizeof( "    " ) + 1;
    psz_message = malloc( i_message ); *psz_message = 0;
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
