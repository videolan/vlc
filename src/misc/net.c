/*****************************************************************************
 * net.c:
 *****************************************************************************
 * Copyright (C) 2004-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@videolan.org>
 *          Rémi Denis-Courmont <rem # videolan.org>
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

#if defined( WIN32 ) || defined( UNDER_CE )
#   if defined(UNDER_CE) && defined(sockaddr_storage)
#       undef sockaddr_storage
#   endif
#   include <winsock2.h>
#   include <ws2tcpip.h>
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

static int net_Socket( vlc_object_t *p_this, int i_family, int i_socktype,
                       int i_protocol )
{
    int fd, i_val;

    fd = socket( i_family, i_socktype, i_protocol );
    if( fd == -1 )
    {
#if defined(WIN32) || defined(UNDER_CE)
        msg_Warn( p_this, "cannot create socket (%i)",
                  WSAGetLastError() );
#else
        msg_Warn( p_this, "cannot create socket (%s)",
                  strerror( errno ) );
#endif
        return -1;
    }

        /* Set to non-blocking */
#if defined( WIN32 ) || defined( UNDER_CE )
    {
        unsigned long i_dummy = 1;
        if( ioctlsocket( fd, FIONBIO, &i_dummy ) != 0 )
            msg_Err( p_this, "cannot set socket to non-blocking mode" );
    }
#else
    if( ( ( i_val = fcntl( fd, F_GETFL, 0 ) ) < 0 ) ||
        ( fcntl( fd, F_SETFL, i_val | O_NONBLOCK ) < 0 ) )
        msg_Err( p_this, "cannot set socket to non-blocking mode (%s)",
                 strerror( errno ) );
#endif

    i_val = 1;
    setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, (void *)&i_val,
                sizeof( i_val ) );

#ifdef IPV6_V6ONLY
    /*
     * Accepts only IPv6 connections on IPv6 sockets
     * (and open an IPv4 socket later as well if needed).
     * Only Linux and FreeBSD can map IPv4 connections on IPv6 sockets,
     * so this allows for more uniform handling across platforms. Besides,
     * it makes sure that IPv4 addresses will be printed as w.x.y.z rather
     * than ::ffff:w.x.y.z
     */
    if( i_family == AF_INET6 )
        setsockopt( fd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&i_val,
                    sizeof( i_val ) );
#endif

#if defined( WIN32 ) || defined( UNDER_CE )
# ifdef IPV6_PROTECTION_LEVEL
    if( i_family == AF_INET6 )
    {
        i_val = PROTECTION_LEVEL_UNRESTRICTED;
        setsockopt( fd, IPPROTO_IPV6, IPV6_PROTECTION_LEVEL, &i_val,
                    sizeof( i_val ) );
    }
# else
# warning You are using outdated headers for Winsock !
# endif
#endif
    return fd;
}

/*****************************************************************************
 * __net_OpenTCP:
 *****************************************************************************
 * Open a TCP connection and return a handle
 *****************************************************************************/
int __net_OpenTCP( vlc_object_t *p_this, const char *psz_host, int i_port )
{
    struct addrinfo hints, *res, *ptr;
    const char      *psz_realhost;
    char            *psz_socks;
    int             i_realport, i_val, i_handle = -1;

    if( i_port == 0 )
        i_port = 80; /* historical VLC thing */

    memset( &hints, 0, sizeof( hints ) );
    hints.ai_socktype = SOCK_STREAM;

    psz_socks = var_CreateGetString( p_this, "socks" );
    if( *psz_socks && *psz_socks != ':' )
    {
        char *psz = strchr( psz_socks, ':' );

        if( psz )
            *psz++ = '\0';

        psz_realhost = psz_socks;
        i_realport = ( psz != NULL ) ? atoi( psz ) : 1080;

        msg_Dbg( p_this, "net: connecting to '%s:%d' for '%s:%d'",
                 psz_realhost, i_realport, psz_host, i_port );
    }
    else
    {
        psz_realhost = psz_host;
        i_realport = i_port;

        msg_Dbg( p_this, "net: connecting to '%s:%d'", psz_realhost,
                 i_realport );
    }

    i_val = vlc_getaddrinfo( p_this, psz_realhost, i_realport, &hints, &res );
    if( i_val )
    {
        msg_Err( p_this, "cannot resolve '%s:%d' : %s", psz_realhost,
                 i_realport, vlc_gai_strerror( i_val ) );
        free( psz_socks );
        return -1;
    }

    for( ptr = res; (ptr != NULL) && (i_handle == -1); ptr = ptr->ai_next )
    {
        int fd;

        fd = net_Socket( p_this, ptr->ai_family, ptr->ai_socktype,
                         ptr->ai_protocol );
        if( fd == -1 )
            continue;

        if( connect( fd, ptr->ai_addr, ptr->ai_addrlen ) )
        {
            socklen_t i_val_size = sizeof( i_val );
            div_t d;
            struct timeval tv;
            vlc_value_t timeout;
#if defined( WIN32 ) || defined( UNDER_CE )
            if( WSAGetLastError() != WSAEWOULDBLOCK )
            {
                msg_Warn( p_this, "connection to %s:%d failed (%d)", psz_host,
                          i_port, WSAGetLastError( ) );
                net_Close( fd );
                continue;
            }
#else
            if( errno != EINPROGRESS )
            {
                msg_Warn( p_this, "connection to %s:%d : %s", psz_host,
                          i_port, strerror( errno ) );
                net_Close( fd );
                continue;
            }
#endif

            var_Create( p_this, "ipv4-timeout",
                        VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
            var_Get( p_this, "ipv4-timeout", &timeout );
            if( timeout.i_int < 0 )
            {
                msg_Err( p_this, "invalid negative value for ipv4-timeout" );
                timeout.i_int = 0;
            }
            d = div( timeout.i_int, 100 );

            msg_Dbg( p_this, "connection in progress" );
            do
            {
                fd_set fds;

                if( p_this->b_die )
                {
                    msg_Dbg( p_this, "connection aborted" );
                    net_Close( fd );
                    vlc_freeaddrinfo( res );
                    free( psz_socks );
                    return -1;
                }

                /* Initialize file descriptor set */
                FD_ZERO( &fds );
                FD_SET( fd, &fds );

                /* We'll wait 0.1 second if nothing happens */
                tv.tv_sec = 0;
                tv.tv_usec = (d.quot > 0) ? 100000 : (1000 * d.rem);

                i_val = select( fd + 1, NULL, &fds, NULL, &tv );

                if( d.quot <= 0 )
                {
                    msg_Dbg( p_this, "connection timed out" );
                    net_Close( fd );
                    fd = -1;
                    break;
                }

                d.quot--;
            }
            while( ( i_val == 0 ) || ( ( i_val < 0 ) &&
#if defined( WIN32 ) || defined( UNDER_CE )
                            ( WSAGetLastError() == WSAEWOULDBLOCK )
#else
                            ( errno == EINTR )
#endif
                     ) );

            if( fd == -1 )
                continue; /* timeout */

            if( i_val < 0 )
            {
                msg_Warn( p_this, "connection aborted (select failed)" );
                net_Close( fd );
                continue;
            }

#if !defined( SYS_BEOS ) && !defined( UNDER_CE )
            if( getsockopt( fd, SOL_SOCKET, SO_ERROR, (void*)&i_val,
                            &i_val_size ) == -1 || i_val != 0 )
            {
#ifdef WIN32
                msg_Warn( p_this, "connection to %s:%d failed (%d)", psz_host,
                          i_port, WSAGetLastError( ) );
#else
                msg_Warn( p_this, "connection to %s:%d : %s", psz_host,
                          i_port, strerror( i_val ) );
#endif
                net_Close( fd );
                continue;
            }
#endif
        }
        i_handle = fd; /* success! */
    }
    
    vlc_freeaddrinfo( res );

    if( *psz_socks && *psz_socks != ':' )
    {
        char *psz_user = var_CreateGetString( p_this, "socks-user" );
        char *psz_pwd  = var_CreateGetString( p_this, "socks-pwd" );

        if( SocksHandshakeTCP( p_this, i_handle, 5, psz_user, psz_pwd,
                               psz_host, i_port ) )
        {
            msg_Err( p_this, "failed to use the SOCKS server" );
            net_Close( i_handle );
            i_handle = -1;
        }

        free( psz_user );
        free( psz_pwd );
    }
    free( psz_socks );

    return i_handle;
}


/*****************************************************************************
 * __net_ListenTCP:
 *****************************************************************************
 * Open TCP passive "listening" socket(s)
 * This function returns NULL in case of error.
 *****************************************************************************/
int *__net_ListenTCP( vlc_object_t *p_this, const char *psz_host, int i_port )
{
    struct addrinfo hints, *res, *ptr;
    int             i_val, *pi_handles, i_size;

    memset( &hints, 0, sizeof( hints ) );
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    msg_Dbg( p_this, "net: listening to '%s:%d'", psz_host, i_port );

    i_val = vlc_getaddrinfo( p_this, psz_host, i_port, &hints, &res );
    if( i_val )
    {
        msg_Err( p_this, "cannot resolve '%s:%d' : %s", psz_host, i_port,
                 vlc_gai_strerror( i_val ) );
        return NULL;
    }

    pi_handles = NULL;
    i_size = 1;

    for( ptr = res; ptr != NULL; ptr = ptr->ai_next )
    {
        int fd, *newpi;

        fd = net_Socket( p_this, ptr->ai_family, ptr->ai_socktype,
                         ptr->ai_protocol );
        if( fd == -1 )
            continue;

        /* Bind the socket */
        if( bind( fd, ptr->ai_addr, ptr->ai_addrlen ) )
        {
#if defined(WIN32) || defined(UNDER_CE)
            msg_Warn( p_this, "cannot bind socket (%i)", WSAGetLastError( ) );
#else
            msg_Warn( p_this, "cannot bind socket (%s)", strerror( errno ) );
#endif
            net_Close( fd );
            continue;
        }
 
        /* Listen */
        if( listen( fd, 100 ) == -1 )
        {
#if defined(WIN32) || defined(UNDER_CE)
            msg_Err( p_this, "cannot bring socket in listening mode (%i)",
                     WSAGetLastError());
#else
            msg_Err( p_this, "cannot bring the socket in listening mode (%s)",
                     strerror( errno ) );
#endif
            net_Close( fd );
            continue;
        }

        newpi = (int *)realloc( pi_handles, (++i_size) * sizeof( int ) );
        if( newpi == NULL )
        {
            net_Close( fd );
            break;
        }
        else
        {
            newpi[i_size - 2] = fd;
            pi_handles = newpi;
        }
    }
    
    vlc_freeaddrinfo( res );

    if( pi_handles != NULL )
        pi_handles[i_size - 1] = -1;
    return pi_handles;
}

/*****************************************************************************
 * __net_Accept:
 *****************************************************************************
 * Accept a connection on a set of listening sockets and return it
 *****************************************************************************/
int __net_Accept( vlc_object_t *p_this, int *pi_fd, mtime_t i_wait )
{
    vlc_bool_t b_die = p_this->b_die, b_block = (i_wait < 0);

    while( p_this->b_die == b_die )
    {
        int i_val = -1, *pi, *pi_end;
        struct timeval timeout;
        fd_set fds_r, fds_e;

        pi = pi_fd;

        /* Initialize file descriptor set */
        FD_ZERO( &fds_r );
        FD_ZERO( &fds_e );

        for( pi = pi_fd; *pi != -1; pi++ )
        {
            int i_fd = *pi;

            if( i_fd > i_val )
                i_val = i_fd;

            FD_SET( i_fd, &fds_r );
            FD_SET( i_fd, &fds_e );
        }
        pi_end = pi;

        timeout.tv_sec = 0;
        timeout.tv_usec = b_block ? 500000 : i_wait;

        i_val = select( i_val + 1, &fds_r, NULL, &fds_e, &timeout );
        if( ( ( i_val < 0 ) && ( errno == EINTR ) ) || i_val == 0 )
        {
            if( b_block )
                continue;
            else
                return -1;
        }
        else if( i_val < 0 )
        {
#if defined(WIN32) || defined(UNDER_CE)
            msg_Err( p_this, "network select error (%i)", WSAGetLastError() );
#else
            msg_Err( p_this, "network select error (%s)", strerror( errno ) );
#endif
            return -1;
        }

        for( pi = pi_fd; *pi != -1; pi++ )
        {
            int i_fd = *pi;

            if( !FD_ISSET( i_fd, &fds_r ) && !FD_ISSET( i_fd, &fds_e ) )
                continue;

            i_val = accept( i_fd, NULL, 0 );
            if( i_val < 0 )
            {
#if defined(WIN32) || defined(UNDER_CE)
                msg_Err( p_this, "accept failed (%i)", WSAGetLastError() );
#else
                msg_Err( p_this, "accept failed (%s)", strerror( errno ) );
#endif
            }
            else
            {
                /*
                 * This round-robin trick ensures that the first sockets in
                 * pi_fd won't prevent the last ones from getting accept'ed.
                 */
                --pi_end;
                memmove( pi, pi + 1, pi_end - pi );
                *pi_end = i_fd;
                return i_val;
            }
        }
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

void net_ListenClose( int *pi_fd )
{
    if( pi_fd != NULL )
    {
        int *pi;

        for( pi = pi_fd; *pi != -1; pi++ )
            net_Close( *pi );
        free( pi_fd );
    }
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
            msg_Err( p_this, "network select error (%d)", WSAGetLastError() );
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
            else if( WSAGetLastError() == WSAEINTR ) continue;
            else msg_Err( p_this, "recv failed (%i)", WSAGetLastError() );
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
        msg_Err( p_this, "network select error (%d)", WSAGetLastError() );
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
            else msg_Err( p_this, "recv failed (%i)", WSAGetLastError() );
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
                    else msg_Err( p_this, "recv failed (%i)",
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
            msg_Err( p_this, "network select error (%d)", WSAGetLastError() );
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
    char *psz_line = NULL, *ptr = NULL;
    size_t  i_line = 0, i_max = 0;


    for( ;; )
    {
        if( i_line == i_max )
        {
            i_max += 1024;
            psz_line = realloc( psz_line, i_max );
            ptr = psz_line + i_line;
        }

        if( net_Read( p_this, fd, p_vs, (uint8_t *)ptr, 1, VLC_TRUE ) != 1 )
        {
            if( i_line == 0 )
            {
                free( psz_line );
                return NULL;
            }
            break;
        }

        if ( *ptr == '\n' )
            break;

        i_line++;
        ptr++;
    }

    *ptr-- = '\0';

    if( ( ptr >= psz_line ) && ( *ptr == '\r' ) )
        *ptr = '\0';

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
    i_ret = __net_Write( p_this, fd, p_vs, (uint8_t *)psz, i_size ) < i_size
        ? -1 : i_size;
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
        struct addrinfo hints = { 0 }, *p_res;

        /* v4 only support ipv4 */
        hints.ai_family = AF_INET;
        if( vlc_getaddrinfo( p_obj, psz_host, 0, &hints, &p_res ) )
            return VLC_EGENERIC;

        buffer[0] = i_socks_version;
        buffer[1] = 0x01;               /* CONNECT */
        SetWBE( &buffer[2], i_port );   /* Port */
        memcpy( &buffer[4],             /* Address */
                &((struct sockaddr_in *)(p_res->ai_addr))->sin_addr, 4 );
        vlc_freeaddrinfo( p_res );

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

