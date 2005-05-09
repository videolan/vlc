/*****************************************************************************
 * net.c:
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@videolan.org>
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
#include <stdlib.h>
#include <vlc/vlc.h>

#include <errno.h>

#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif

#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
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
#   include <sys/socket.h>
#   include <netinet/in.h>
#   ifdef HAVE_ARPA_INET_H
#       include <arpa/inet.h>
#   endif
#   include <netdb.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( WIN32 ) && !defined( UNDER_CE )
#   include <io.h>
#endif

#include "network.h"

#ifndef INADDR_ANY
#   define INADDR_ANY  0x00000000
#endif
#ifndef INADDR_NONE
#   define INADDR_NONE 0xFFFFFFFF
#endif

static int SocksNegociate( vlc_object_t *, int fd, int i_socks_version,
                           char *psz_socks_user, char *psz_socks_passwd );
static int SocksHandshakeTCP( vlc_object_t *,
                              int fd, int i_socks_version,
                              char *psz_socks_user, char *psz_socks_passwd,
                              const char *psz_host, int i_port );

/*****************************************************************************
 * net_ConvertIPv4:
 *****************************************************************************
 * Open a TCP connection and return a handle
 *****************************************************************************/
int net_ConvertIPv4( uint32_t *p_addr, const char * psz_address )
{
    /* Reset struct */
    if( !*psz_address )
    {
        *p_addr = INADDR_ANY;
    }
    else
    {
        struct hostent *p_hostent;

        /* Try to convert address directly from in_addr - this will work if
         * psz_address is dotted decimal. */
#ifdef HAVE_ARPA_INET_H
        if( !inet_aton( psz_address, (struct in_addr *)p_addr ) )
#else
        *p_addr = inet_addr( psz_address );
        if( *p_addr == INADDR_NONE )
#endif
        {
            /* We have a fqdn, try to find its address */
            if ( (p_hostent = gethostbyname( psz_address )) == NULL )
            {
                return VLC_EGENERIC;
            }

            /* Copy the first address of the host in the socket address */
            memcpy( p_addr, p_hostent->h_addr_list[0],
                    p_hostent->h_length );
        }
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * __net_OpenTCP:
 *****************************************************************************
 * Open a TCP connection and return a handle
 *****************************************************************************/
int __net_OpenTCP( vlc_object_t *p_this, const char *psz_host, int i_port )
{
    vlc_value_t      val;
    void            *private;

    char            *psz_network = "";
    network_socket_t sock;
    module_t         *p_network;

    /* Check if we have force ipv4 or ipv6 */
    var_Create( p_this, "ipv4", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Get( p_this, "ipv4", &val );
    if( val.b_bool )
    {
        psz_network = "ipv4";
    }

    var_Create( p_this, "ipv6", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Get( p_this, "ipv6", &val );
    if( val.b_bool )
    {
        psz_network = "ipv6";
    }

    /* Prepare the network_socket_t structure */
    sock.i_type = NETWORK_TCP;
    sock.psz_bind_addr   = "";
    sock.i_bind_port     = 0;
    sock.i_ttl           = 0;

    var_Create( p_this, "socks", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_this, "socks", &val );
    if( *val.psz_string && *val.psz_string != ':' )
    {
        char *psz = strchr( val.psz_string, ':' );

        if( psz )
            *psz++ = '\0';

        sock.psz_server_addr = (char*)val.psz_string;
        sock.i_server_port   = psz ? atoi( psz ) : 1080;

        msg_Dbg( p_this, "net: connecting to '%s:%d' for '%s:%d'",
                 sock.psz_server_addr, sock.i_server_port,
                 psz_host, i_port );
    }
    else
    {
        sock.psz_server_addr = (char*)psz_host;
        sock.i_server_port   = i_port;
        msg_Dbg( p_this, "net: connecting to '%s:%d'", psz_host, i_port );
    }


    private = p_this->p_private;
    p_this->p_private = (void*)&sock;
    if( !( p_network = module_Need( p_this, "network", psz_network, 0 ) ) )
    {
        msg_Dbg( p_this, "net: connection to '%s:%d' failed",
                 psz_host, i_port );
        return -1;
    }
    module_Unneed( p_this, p_network );
    p_this->p_private = private;

    if( *val.psz_string && *val.psz_string != ':' )
    {
        char *psz_user = var_CreateGetString( p_this, "socks-user" );
        char *psz_pwd  = var_CreateGetString( p_this, "socks-pwd" );

        if( SocksHandshakeTCP( p_this, sock.i_handle, 5,
                               psz_user, psz_pwd,
                               psz_host, i_port ) )
        {
            msg_Err( p_this, "failed to use the SOCKS server" );
            net_Close( sock.i_handle );
            return -1;
        }

        free( psz_user );
        free( psz_pwd );
    }
    free( val.psz_string );

    return sock.i_handle;
}

/*****************************************************************************
 * __net_ListenTCP:
 *****************************************************************************
 * Open a TCP listening socket and return it
 *****************************************************************************/
int __net_ListenTCP( vlc_object_t *p_this, char *psz_host, int i_port )
{
    vlc_value_t      val;
    void            *private;

    char            *psz_network = "";
    network_socket_t sock;
    module_t         *p_network;

    /* Check if we have force ipv4 or ipv6 */
    var_Create( p_this, "ipv4", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Get( p_this, "ipv4", &val );
    if( val.b_bool )
    {
        psz_network = "ipv4";
    }

    var_Create( p_this, "ipv6", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Get( p_this, "ipv6", &val );
    if( val.b_bool )
    {
        psz_network = "ipv6";
    }

    /* Prepare the network_socket_t structure */
    sock.i_type = NETWORK_TCP_PASSIVE;
    sock.psz_bind_addr   = "";
    sock.i_bind_port     = 0;
    sock.psz_server_addr = psz_host;
    sock.i_server_port   = i_port;
    sock.i_ttl           = 0;

    msg_Dbg( p_this, "net: listening to '%s:%d'", psz_host, i_port );
    private = p_this->p_private;
    p_this->p_private = (void*)&sock;
    if( !( p_network = module_Need( p_this, "network", psz_network, 0 ) ) )
    {
        msg_Dbg( p_this, "net: listening to '%s:%d' failed",
                 psz_host, i_port );
        return -1;
    }
    module_Unneed( p_this, p_network );
    p_this->p_private = private;

    return sock.i_handle;
}

/*****************************************************************************
 * __net_Accept:
 *****************************************************************************
 * Accept a connection on a listening socket and return it
 *****************************************************************************/
int __net_Accept( vlc_object_t *p_this, int fd, mtime_t i_wait )
{
    vlc_bool_t b_die = p_this->b_die, b_block = (i_wait < 0);
    struct timeval timeout;
    fd_set fds_r, fds_e;
    int i_ret;

    while( p_this->b_die == b_die )
    {
        /* Initialize file descriptor set */
        FD_ZERO( &fds_r );
        FD_SET( fd, &fds_r );
        FD_ZERO( &fds_e );
        FD_SET( fd, &fds_e );

        timeout.tv_sec = 0;
        timeout.tv_usec = b_block ? 500000 : i_wait;

        i_ret = select(fd + 1, &fds_r, NULL, &fds_e, &timeout);
        if( (i_ret < 0 && errno == EINTR) || i_ret == 0 )
        {
            if( b_block ) continue;
            else return -1;
        }
        else if( i_ret < 0 )
        {
#if defined(WIN32) || defined(UNDER_CE)
            msg_Err( p_this, "network select error (%i)", WSAGetLastError() );
#else
            msg_Err( p_this, "network select error (%s)", strerror(errno) );
#endif
            return -1;
        }

        if( ( i_ret = accept( fd, 0, 0 ) ) <= 0 )
        {
#if defined(WIN32) || defined(UNDER_CE)
            msg_Err( p_this, "accept failed (%i)", WSAGetLastError() );
#else
            msg_Err( p_this, "accept failed (%s)", strerror(errno) );
#endif
            return -1;
        }

        return i_ret;
    }

    return -1;
}

/*****************************************************************************
 * __net_OpenUDP:
 *****************************************************************************
 * Open a UDP connection and return a handle
 *****************************************************************************/
int __net_OpenUDP( vlc_object_t *p_this, char *psz_bind, int i_bind,
                   char *psz_server, int i_server )
{
    vlc_value_t      val;
    void            *private;

    char            *psz_network = "";
    network_socket_t sock;
    module_t         *p_network;


    /* Check if we have force ipv4 or ipv6 */
    var_Create( p_this, "ipv4", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Get( p_this, "ipv4", &val );
    if( val.b_bool )
    {
        psz_network = "ipv4";
    }

    var_Create( p_this, "ipv6", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Get( p_this, "ipv6", &val );
    if( val.b_bool )
    {
        psz_network = "ipv6";
    }
    if( psz_server == NULL ) psz_server = "";
    if( psz_bind   == NULL ) psz_bind   = "";

    /* Prepare the network_socket_t structure */
    sock.i_type = NETWORK_UDP;
    sock.psz_bind_addr   = psz_bind;
    sock.i_bind_port     = i_bind;
    sock.psz_server_addr = psz_server;
    sock.i_server_port   = i_server;
    sock.i_ttl           = 0;

    msg_Dbg( p_this, "net: connecting to '%s:%d@%s:%d'",
             psz_server, i_server, psz_bind, i_bind );
    private = p_this->p_private;
    p_this->p_private = (void*)&sock;
    if( !( p_network = module_Need( p_this, "network", psz_network, 0 ) ) )
    {
        msg_Dbg( p_this, "net: connection to '%s:%d@%s:%d' failed",
                 psz_server, i_server, psz_bind, i_bind );
        return -1;
    }
    module_Unneed( p_this, p_network );
    p_this->p_private = private;

    return sock.i_handle;
}

/*****************************************************************************
 * __net_Close:
 *****************************************************************************
 * Close a network handle
 *****************************************************************************/
void net_Close( int fd )
{
#ifdef UNDER_CE
    CloseHandle( (HANDLE)fd );
#elif defined( WIN32 )
    closesocket( fd );
#else
    close( fd );
#endif
}

/*****************************************************************************
 * __net_Read:
 *****************************************************************************
 * Read from a network socket
 * If b_rety is true, then we repeat until we have read the right amount of
 * data
 *****************************************************************************/
int __net_Read( vlc_object_t *p_this, int fd, v_socket_t *p_vs,
                uint8_t *p_data, int i_data, vlc_bool_t b_retry )
{
    struct timeval  timeout;
    fd_set          fds_r, fds_e;
    int             i_recv;
    int             i_total = 0;
    int             i_ret;
    vlc_bool_t      b_die = p_this->b_die;

    while( i_data > 0 )
    {
        do
        {
            if( p_this->b_die != b_die )
            {
                return 0;
            }

            /* Initialize file descriptor set */
            FD_ZERO( &fds_r );
            FD_SET( fd, &fds_r );
            FD_ZERO( &fds_e );
            FD_SET( fd, &fds_e );

            /* We'll wait 0.5 second if nothing happens */
            timeout.tv_sec = 0;
            timeout.tv_usec = 500000;

        } while( (i_ret = select(fd + 1, &fds_r, NULL, &fds_e, &timeout)) == 0
                 || ( i_ret < 0 && errno == EINTR ) );

        if( i_ret < 0 )
        {
#if defined(WIN32) || defined(UNDER_CE)
            msg_Err( p_this, "network select error" );
#else
            msg_Err( p_this, "network select error (%s)", strerror(errno) );
#endif
            return i_total > 0 ? i_total : -1;
        }

        if( ( i_recv = (p_vs != NULL)
              ? p_vs->pf_recv( p_vs->p_sys, p_data, i_data )
              : recv( fd, p_data, i_data, 0 ) ) < 0 )
        {
#if defined(WIN32) || defined(UNDER_CE)
            if( WSAGetLastError() == WSAEWOULDBLOCK )
            {
                /* only happens with p_vs (SSL) - not really an error */
            }
            else
            /* For udp only */
            /* On win32 recv() will fail if the datagram doesn't fit inside
             * the passed buffer, even though the buffer will be filled with
             * the first part of the datagram. */
            if( WSAGetLastError() == WSAEMSGSIZE )
            {
                msg_Err( p_this, "recv() failed. "
                         "Increase the mtu size (--mtu option)" );
                i_total += i_data;
            }
            else
                msg_Err( p_this, "recv failed (%i)", WSAGetLastError() );
#else
            /* EAGAIN only happens with p_vs (SSL) and it's not an error */
            if( errno != EAGAIN )
                msg_Err( p_this, "recv failed (%s)", strerror(errno) );
#endif
            return i_total > 0 ? i_total : -1;
        }
        else if( i_recv == 0 )
        {
            /* Connection closed */
            b_retry = VLC_FALSE;
        }

        p_data += i_recv;
        i_data -= i_recv;
        i_total+= i_recv;
        if( !b_retry )
        {
            break;
        }
    }
    return i_total;
}

/*****************************************************************************
 * __net_ReadNonBlock:
 *****************************************************************************
 * Read from a network socket, non blocking mode (with timeout)
 *****************************************************************************/
int __net_ReadNonBlock( vlc_object_t *p_this, int fd, v_socket_t *p_vs,
                        uint8_t *p_data, int i_data, mtime_t i_wait)
{
    struct timeval  timeout;
    fd_set          fds_r, fds_e;
    int             i_recv;
    int             i_ret;

    /* Initialize file descriptor set */
    FD_ZERO( &fds_r );
    FD_SET( fd, &fds_r );
    FD_ZERO( &fds_e );
    FD_SET( fd, &fds_e );

    timeout.tv_sec = 0;
    timeout.tv_usec = i_wait;

    i_ret = select(fd + 1, &fds_r, NULL, &fds_e, &timeout);

    if( i_ret < 0 && errno == EINTR )
    {
        return 0;
    }
    else if( i_ret < 0 )
    {
#if defined(WIN32) || defined(UNDER_CE)
        msg_Err( p_this, "network select error" );
#else
        msg_Err( p_this, "network select error (%s)", strerror(errno) );
#endif
        return -1;
    }
    else if( i_ret == 0)
    {
        return 0;
    }
    else
    {
#if !defined(UNDER_CE)
        if( fd == 0/*STDIN_FILENO*/ ) i_recv = read( fd, p_data, i_data ); else
#endif
        if( ( i_recv = (p_vs != NULL)
              ? p_vs->pf_recv( p_vs->p_sys, p_data, i_data )
              : recv( fd, p_data, i_data, 0 ) ) <= 0 )
        {
#if defined(WIN32) || defined(UNDER_CE)
            /* For udp only */
            /* On win32 recv() will fail if the datagram doesn't fit inside
             * the passed buffer, even though the buffer will be filled with
             * the first part of the datagram. */
            if( WSAGetLastError() == WSAEMSGSIZE )
            {
                msg_Err( p_this, "recv() failed. "
                         "Increase the mtu size (--mtu option)" );
            }
            else
                msg_Err( p_this, "recv failed (%i)", WSAGetLastError() );
#else
            msg_Err( p_this, "recv failed (%s)", strerror(errno) );
#endif
            return -1;
        }

        return i_recv ? i_recv : -1;  /* !i_recv -> connection closed if tcp */
    }

    /* We will never be here */
    return -1;
}

/*****************************************************************************
 * __net_Select:
 *****************************************************************************
 * Read from several sockets (with timeout). Takes data from the first socket
 * that has some.
 *****************************************************************************/
int __net_Select( vlc_object_t *p_this, int *pi_fd, v_socket_t **pp_vs,
                  int i_fd, uint8_t *p_data, int i_data, mtime_t i_wait )
{
    struct timeval  timeout;
    fd_set          fds_r, fds_e;
    int             i_recv;
    int             i_ret;
    int             i;
    int             i_max_fd = 0;

    /* Initialize file descriptor set */
    FD_ZERO( &fds_r );
    FD_ZERO( &fds_e );

    for( i = 0 ; i < i_fd ; i++)
    {
        if( pi_fd[i] > i_max_fd ) i_max_fd = pi_fd[i];
        FD_SET( pi_fd[i], &fds_r );
        FD_SET( pi_fd[i], &fds_e );
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = i_wait;

    i_ret = select( i_max_fd + 1, &fds_r, NULL, &fds_e, &timeout );

    if( i_ret < 0 && errno == EINTR )
    {
        return 0;
    }
    else if( i_ret < 0 )
    {
        msg_Err( p_this, "network select error (%s)", strerror(errno) );
        return -1;
    }
    else if( i_ret == 0 )
    {
        return 0;
    }
    else
    {
        for( i = 0 ; i < i_fd ; i++)
        {
            if( FD_ISSET( pi_fd[i], &fds_r ) )
            {
                i_recv = ((pp_vs != NULL) && (pp_vs[i] != NULL))
                         ? pp_vs[i]->pf_recv( pp_vs[i]->p_sys, p_data, i_data )
                         : recv( pi_fd[i], p_data, i_data, 0 );
                if( i_recv <= 0 )
                {
#ifdef WIN32
                    /* For udp only */
                    /* On win32 recv() will fail if the datagram doesn't
                     * fit inside the passed buffer, even though the buffer
                     *  will be filled with the first part of the datagram. */
                    if( WSAGetLastError() == WSAEMSGSIZE )
                    {
                        msg_Err( p_this, "recv() failed. "
                             "Increase the mtu size (--mtu option)" );
                    }
                    else
                        msg_Err( p_this, "recv failed (%i)",
                                        WSAGetLastError() );
#else
                     msg_Err( p_this, "recv failed (%s)", strerror(errno) );
#endif
                    return VLC_EGENERIC;
                }

                return i_recv;
            }
        }
    }

    /* We will never be here */
    return -1;
}


/* Write exact amount requested */
int __net_Write( vlc_object_t *p_this, int fd, v_socket_t *p_vs,
                 uint8_t *p_data, int i_data )
{
    struct timeval  timeout;
    fd_set          fds_w, fds_e;
    int             i_send;
    int             i_total = 0;
    int             i_ret;

    vlc_bool_t      b_die = p_this->b_die;

    while( i_data > 0 )
    {
        do
        {
            if( p_this->b_die != b_die )
            {
                return 0;
            }

            /* Initialize file descriptor set */
            FD_ZERO( &fds_w );
            FD_SET( fd, &fds_w );
            FD_ZERO( &fds_e );
            FD_SET( fd, &fds_e );

            /* We'll wait 0.5 second if nothing happens */
            timeout.tv_sec = 0;
            timeout.tv_usec = 500000;

        } while( (i_ret = select(fd + 1, NULL, &fds_w, &fds_e, &timeout)) == 0
                 || ( i_ret < 0 && errno == EINTR ) );

        if( i_ret < 0 )
        {
#if defined(WIN32) || defined(UNDER_CE)
            msg_Err( p_this, "network select error" );
#else
            msg_Err( p_this, "network select error (%s)", strerror(errno) );
#endif
            return i_total > 0 ? i_total : -1;
        }

        if( ( i_send = (p_vs != NULL)
                       ? p_vs->pf_send( p_vs->p_sys, p_data, i_data )
                       : send( fd, p_data, i_data, 0 ) ) < 0 )
        {
            /* XXX With udp for example, it will issue a message if the host
             * isn't listening */
            /* msg_Err( p_this, "send failed (%s)", strerror(errno) ); */
            return i_total > 0 ? i_total : -1;
        }

        p_data += i_send;
        i_data -= i_send;
        i_total+= i_send;
    }
    return i_total;
}

char *__net_Gets( vlc_object_t *p_this, int fd, v_socket_t *p_vs )
{
    char *psz_line = malloc( 1024 );
    int  i_line = 0;
    int  i_max = 1024;


    for( ;; )
    {
        if( net_Read( p_this, fd, p_vs, &psz_line[i_line], 1, VLC_TRUE ) != 1 )
        {
            psz_line[i_line] = '\0';
            break;
        }
        i_line++;

        if( psz_line[i_line-1] == '\n' )
        {
            psz_line[i_line] = '\0';
            break;
        }

        if( i_line >= i_max - 1 )
        {
            i_max += 1024;
            psz_line = realloc( psz_line, i_max );
        }
    }

    if( i_line <= 0 )
    {
        free( psz_line );
        return NULL;
    }

    while( i_line >= 1 &&
           ( psz_line[i_line-1] == '\n' || psz_line[i_line-1] == '\r' ) )
    {
        i_line--;
        psz_line[i_line] = '\0';
    }
    return psz_line;
}

int net_Printf( vlc_object_t *p_this, int fd, v_socket_t *p_vs,
                const char *psz_fmt, ... )
{
    int i_ret;
    va_list args;
    va_start( args, psz_fmt );
    i_ret = net_vaPrintf( p_this, fd, p_vs, psz_fmt, args );
    va_end( args );

    return i_ret;
}

int __net_vaPrintf( vlc_object_t *p_this, int fd, v_socket_t *p_vs,
                    const char *psz_fmt, va_list args )
{
    char    *psz;
    int     i_size, i_ret;

    vasprintf( &psz, psz_fmt, args );
    i_size = strlen( psz );
    i_ret = __net_Write( p_this, fd, p_vs, psz, i_size ) < i_size ? -1 : i_size;
    free( psz );

    return i_ret;
}



/*****************************************************************************
 * SocksNegociate:
 *****************************************************************************
 * Negociate authentication with a SOCKS server.
 *****************************************************************************/
static int SocksNegociate( vlc_object_t *p_obj,
                           int fd, int i_socks_version,
                           char *psz_socks_user,
                           char *psz_socks_passwd )
{
    uint8_t buffer[128+2*256];
    int i_len;
    vlc_bool_t b_auth = VLC_FALSE;

    if( i_socks_version != 5 )
        return VLC_SUCCESS;

    /* We negociate authentication */

    if( psz_socks_user && psz_socks_passwd &&
        *psz_socks_user && *psz_socks_passwd )
        b_auth = VLC_TRUE;

    buffer[0] = i_socks_version;    /* SOCKS version */
    if( b_auth )
    {
        buffer[1] = 2;                  /* Number of methods */
        buffer[2] = 0x00;               /* - No auth required */
        buffer[3] = 0x02;               /* - USer/Password */
        i_len = 4;
    }
    else
    {
        buffer[1] = 1;                  /* Number of methods */
        buffer[2] = 0x00;               /* - No auth required */
        i_len = 3;
    }
    
    if( net_Write( p_obj, fd, NULL, buffer, i_len ) != i_len )
        return VLC_EGENERIC;
    if( net_Read( p_obj, fd, NULL, buffer, 2, VLC_TRUE ) != 2 )
        return VLC_EGENERIC;

    msg_Dbg( p_obj, "socks: v=%d method=%x", buffer[0], buffer[1] );

    if( buffer[1] == 0x00 )
    {
        msg_Dbg( p_obj, "socks: no authentication required" );
    }
    else if( buffer[1] == 0x02 )
    {
        int i_len1 = __MIN( strlen(psz_socks_user), 255 );
        int i_len2 = __MIN( strlen(psz_socks_passwd), 255 );
        msg_Dbg( p_obj, "socks: username/password authentication" );

        /* XXX: we don't support user/pwd > 255 (truncated)*/
        buffer[0] = i_socks_version;        /* Version */
        buffer[1] = i_len1;                 /* User length */
        memcpy( &buffer[2], psz_socks_user, i_len1 );
        buffer[2+i_len1] = i_len2;          /* Password length */
        memcpy( &buffer[2+i_len1+1], psz_socks_passwd, i_len2 );

        i_len = 3 + i_len1 + i_len2;

        if( net_Write( p_obj, fd, NULL, buffer, i_len ) != i_len )
            return VLC_EGENERIC;

        if( net_Read( p_obj, fd, NULL, buffer, 2, VLC_TRUE ) != 2 )
            return VLC_EGENERIC;

        msg_Dbg( p_obj, "socks: v=%d status=%x", buffer[0], buffer[1] );
        if( buffer[1] != 0x00 )
        {
            msg_Err( p_obj, "socks: authentication rejected" );
            return VLC_EGENERIC;
        }
    }
    else
    {
        if( b_auth )
            msg_Err( p_obj, "socks: unsupported authentication method %x",
                     buffer[0] );
        else
            msg_Err( p_obj, "socks: authentification needed" );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SocksHandshakeTCP:
 *****************************************************************************
 * Open a TCP connection using a SOCKS server and return a handle (RFC 1928)
 *****************************************************************************/
static int SocksHandshakeTCP( vlc_object_t *p_obj,
                              int fd,
                              int i_socks_version,
                              char *psz_socks_user, char *psz_socks_passwd,
                              const char *psz_host, int i_port )
{
    uint8_t buffer[128+2*256];

    if( i_socks_version != 4 && i_socks_version != 5 )
    {
        msg_Warn( p_obj, "invalid socks protocol version %d", i_socks_version );
        i_socks_version = 5;
    }

    if( i_socks_version == 5 && 
        SocksNegociate( p_obj, fd, i_socks_version,
                        psz_socks_user, psz_socks_passwd ) )
        return VLC_EGENERIC;

    if( i_socks_version == 4 )
    {
        uint32_t addr;

        /* v4 only support ipv4 */
        if( net_ConvertIPv4( &addr, psz_host ) )
            return VLC_EGENERIC;

        buffer[0] = i_socks_version;
        buffer[1] = 0x01;               /* CONNECT */
        SetWBE( &buffer[2], i_port );   /* Port */
        memcpy( &buffer[4], &addr, 4 ); /* Addresse */
        buffer[8] = 0;                  /* Empty user id */

        if( net_Write( p_obj, fd, NULL, buffer, 9 ) != 9 )
            return VLC_EGENERIC;
        if( net_Read( p_obj, fd, NULL, buffer, 8, VLC_TRUE ) != 8 )
            return VLC_EGENERIC;

        msg_Dbg( p_obj, "socks: v=%d cd=%d",
                 buffer[0], buffer[1] );

        if( buffer[1] != 90 )
            return VLC_EGENERIC;
    }
    else if( i_socks_version == 5 )
    {
        int i_hlen = __MIN(strlen( psz_host ), 255);
        int i_len;

        buffer[0] = i_socks_version;    /* Version */
        buffer[1] = 0x01;               /* Cmd: connect */
        buffer[2] = 0x00;               /* Reserved */
        buffer[3] = 3;                  /* ATYP: for now domainname */

        buffer[4] = i_hlen;
        memcpy( &buffer[5], psz_host, i_hlen );
        SetWBE( &buffer[5+i_hlen], i_port );

        i_len = 5 + i_hlen + 2;


        if( net_Write( p_obj, fd, NULL, buffer, i_len ) != i_len )
            return VLC_EGENERIC;

        /* Read the header */
        if( net_Read( p_obj, fd, NULL, buffer, 5, VLC_TRUE ) != 5 )
            return VLC_EGENERIC;

        msg_Dbg( p_obj, "socks: v=%d rep=%d atyp=%d",
                 buffer[0], buffer[1], buffer[3] );

        if( buffer[1] != 0x00 )
        {
            msg_Err( p_obj, "socks: CONNECT request failed\n" );
            return VLC_EGENERIC;
        }

        /* Read the remaining bytes */
        if( buffer[3] == 0x01 )
            i_len = 4-1 + 2;
        else if( buffer[3] == 0x03 )
            i_len = buffer[4] + 2;
        else if( buffer[3] == 0x04 )
            i_len = 16-1+2;
        else 
            return VLC_EGENERIC;

        if( net_Read( p_obj, fd, NULL, buffer, i_len, VLC_TRUE ) != i_len )
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}


