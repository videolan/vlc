/*****************************************************************************
 * ipv4.c: IPv4 network abstraction layer
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: ipv4.c,v 1.14 2003/02/18 18:33:44 titer Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Mathias Kretschmer <mathias@research.att.com>
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
#include <string.h>
#include <vlc/vlc.h>

#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif
#ifdef HAVE_ERRNO_H
#   include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 ) && !defined( UNDER_CE )
#   include <io.h>
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

#ifndef INADDR_ANY
#   define INADDR_ANY  0x00000000
#endif
#ifndef INADDR_NONE
#   define INADDR_NONE 0xFFFFFFFF
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int NetOpen( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("IPv4 network abstraction layer") );
    set_capability( "network", 50 );
    set_callbacks( NetOpen, NULL );
vlc_module_end();

/*****************************************************************************
 * BuildAddr: utility function to build a struct sockaddr_in
 *****************************************************************************/
static int BuildAddr( struct sockaddr_in * p_socket,
                      const char * psz_address, int i_port )
{
    /* Reset struct */
    memset( p_socket, 0, sizeof( struct sockaddr_in ) );
    p_socket->sin_family = AF_INET;                                /* family */
    p_socket->sin_port = htons( (uint16_t)i_port );
    if( !*psz_address )
    {
        p_socket->sin_addr.s_addr = INADDR_ANY;
    }
    else
    {
        struct hostent    * p_hostent;

        /* Try to convert address directly from in_addr - this will work if
         * psz_address is dotted decimal. */
#ifdef HAVE_ARPA_INET_H
        if( !inet_aton( psz_address, &p_socket->sin_addr ) )
#else
        p_socket->sin_addr.s_addr = inet_addr( psz_address );
        if( p_socket->sin_addr.s_addr == INADDR_NONE )
#endif
        {
            /* We have a fqdn, try to find its address */
            if ( (p_hostent = gethostbyname( psz_address )) == NULL )
            {
                return( -1 );
            }

            /* Copy the first address of the host in the socket address */
            memcpy( &p_socket->sin_addr, p_hostent->h_addr_list[0],
                     p_hostent->h_length );
        }
    }
    return( 0 );
}

/*****************************************************************************
 * OpenUDP: open a UDP socket
 *****************************************************************************
 * psz_bind_addr, i_bind_port : address and port used for the bind()
 *   system call. If psz_bind_addr == "", the socket is bound to
 *   INADDR_ANY and broadcast reception is enabled. If i_bind_port == 0,
 *   1234 is used. If psz_bind_addr is a multicast (class D) address,
 *   join the multicast group.
 * psz_server_addr, i_server_port : address and port used for the connect()
 *   system call. It can avoid receiving packets from unauthorized IPs.
 *   Its use leads to great confusion and is currently discouraged.
 * This function returns -1 in case of error.
 *****************************************************************************/
static int OpenUDP( vlc_object_t * p_this, network_socket_t * p_socket )
{
    char * psz_bind_addr = p_socket->psz_bind_addr;
    int i_bind_port = p_socket->i_bind_port;
    char * psz_server_addr = p_socket->psz_server_addr;
    int i_server_port = p_socket->i_server_port;
#if defined( WIN32 ) && !defined( UNDER_CE )
    char * psz_bind_win32;        /* WIN32 multicast kludge */
#endif

    int i_handle, i_opt;
    socklen_t i_opt_size;
    struct sockaddr_in sock;

    /* Open a SOCK_DGRAM (UDP) socket, in the AF_INET domain, automatic (0)
     * protocol */
    if( (i_handle = socket( AF_INET, SOCK_DGRAM, 0 )) == -1 )
    {
#ifdef HAVE_ERRNO_H
        msg_Err( p_this, "cannot create socket (%s)", strerror(errno) );
#else
        msg_Err( p_this, "cannot create socket" );
#endif
        return( -1 );
    }

    /* We may want to reuse an already used socket */
    i_opt = 1;
    if( setsockopt( i_handle, SOL_SOCKET, SO_REUSEADDR,
                    (void *) &i_opt, sizeof( i_opt ) ) == -1 )
    {
#ifdef HAVE_ERRNO_H
        msg_Err( p_this, "cannot configure socket (SO_REUSEADDR: %s)",
                          strerror(errno));
#else
        msg_Err( p_this, "cannot configure socket (SO_REUSEADDR)" );
#endif
#if defined( WIN32 ) || defined( UNDER_CE )
        closesocket( i_handle );
#else
        close( i_handle );
#endif
        return( -1 );
    }

    /* Increase the receive buffer size to 1/2MB (8Mb/s during 1/2s) to avoid
     * packet loss caused by scheduling problems */
    i_opt = 0x80000;
#if defined( SYS_BEOS )
    if( setsockopt( i_handle, SOL_SOCKET, SO_NONBLOCK,
#else
    if( setsockopt( i_handle, SOL_SOCKET, SO_RCVBUF,
#endif
                    (void *) &i_opt, sizeof( i_opt ) ) == -1 )
    {
#ifdef HAVE_ERRNO_H
        msg_Dbg( p_this, "cannot configure socket (SO_RCVBUF: %s)",
                          strerror(errno));
#else
        msg_Warn( p_this, "cannot configure socket (SO_RCVBUF)" );
#endif
    }

    /* Check if we really got what we have asked for, because Linux, etc.
     * will silently limit the max buffer size to net.core.rmem_max which
     * is typically only 65535 bytes */
    i_opt = 0;
    i_opt_size = sizeof( i_opt );
#if defined( SYS_BEOS )
    if( getsockopt( i_handle, SOL_SOCKET, SO_NONBLOCK,
#else
    if( getsockopt( i_handle, SOL_SOCKET, SO_RCVBUF,
#endif
                    (void*) &i_opt, &i_opt_size ) == -1 )
    {
#ifdef HAVE_ERRNO_H
        msg_Warn( p_this, "cannot query socket (SO_RCVBUF: %s)",
                          strerror(errno) );
#else
        msg_Warn( p_this, "cannot query socket (SO_RCVBUF)" );
#endif
    }
    else if( i_opt < 0x80000 )
    {
        msg_Dbg( p_this, "socket buffer size is 0x%x instead of 0x%x",
                         i_opt, 0x80000 );
    }


    /* Build the local socket */

#if defined( WIN32 ) && !defined( UNDER_CE )
    /* Under Win32 and for the multicast, we bind on INADDR_ANY,
     * so let's call BuildAddr with "" instead of psz_bind_addr */
    psz_bind_win32 = psz_bind_addr ;

    /* Check if this is a multicast socket */
    if (IN_MULTICAST( ntohl( inet_addr(psz_bind_addr) ) ) )
    {
        psz_bind_win32 = "";
    }
    if ( BuildAddr( &sock, psz_bind_win32, i_bind_port ) == -1 )
#else
    if ( BuildAddr( &sock, psz_bind_addr, i_bind_port ) == -1 )
#endif
    {
        msg_Dbg( p_this, "could not build local address" );
#if defined( WIN32 ) || defined( UNDER_CE )
        closesocket( i_handle );
#else
        close( i_handle );
#endif
        return( -1 );
    }

    /* Bind it */
    if( bind( i_handle, (struct sockaddr *)&sock, sizeof( sock ) ) < 0 )
    {
#ifdef HAVE_ERRNO_H
        msg_Err( p_this, "cannot bind socket (%s)", strerror(errno) );
#else
        msg_Err( p_this, "cannot bind socket" );
#endif
#if defined( WIN32 ) || defined( UNDER_CE )
        closesocket( i_handle );
#else
        close( i_handle );
#endif
        return( -1 );
    }

    /* Allow broadcast reception if we bound on INADDR_ANY */
    if( !*psz_bind_addr )
    {
        i_opt = 1;
#if defined( SYS_BEOS )
        if( setsockopt( i_handle, SOL_SOCKET, SO_NONBLOCK,
#else
        if( setsockopt( i_handle, SOL_SOCKET, SO_BROADCAST,
#endif
                        (void*) &i_opt, sizeof( i_opt ) ) == -1 )
        {
#ifdef HAVE_ERRNO_H
            msg_Warn( p_this, "cannot configure socket (SO_BROADCAST: %s)",
                       strerror(errno) );
#else
            msg_Warn( p_this, "cannot configure socket (SO_BROADCAST)" );
#endif
        }
    }

#if !defined( UNDER_CE ) && !defined( SYS_BEOS )
    /* Join the multicast group if the socket is a multicast address */
#ifndef IN_MULTICAST
#   define IN_MULTICAST(a)         IN_CLASSD(a)
#endif

#ifndef WIN32
    if( IN_MULTICAST( ntohl(sock.sin_addr.s_addr) ) )
    {
        struct ip_mreq imr;
        char * psz_if_addr = config_GetPsz( p_this, "iface-addr" );
        imr.imr_multiaddr.s_addr = sock.sin_addr.s_addr;
#else
    if( IN_MULTICAST( ntohl(inet_addr(psz_bind_addr) ) ) )
    {
        struct ip_mreq imr;
        char * psz_if_addr = config_GetPsz( p_this, "iface-addr" );
        imr.imr_multiaddr.s_addr = inet_addr(psz_bind_addr);
#endif
        if ( psz_if_addr != NULL && *psz_if_addr
              && inet_addr(psz_if_addr) != INADDR_NONE )
        {
            imr.imr_interface.s_addr = inet_addr(psz_if_addr);
        }
        else
        {
            imr.imr_interface.s_addr = INADDR_ANY;
        }
        if ( psz_if_addr != NULL ) free( psz_if_addr );

        if( setsockopt( i_handle, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                        (char*)&imr, sizeof(struct ip_mreq) ) == -1 )
        {
#ifdef HAVE_ERRNO_H
            msg_Warn( p_this, "failed to join IP multicast group (%s)",
                              strerror(errno) );
#else
            msg_Warn( p_this, "failed to join IP multicast group" );
#endif
#if defined( WIN32 ) || defined( UNDER_CE )
            closesocket( i_handle );
#else
            close( i_handle );
#endif
            return( -1 );
        }
    }
#endif /* UNDER_CE */

    if( *psz_server_addr )
    {
        /* Build socket for remote connection */
        if ( BuildAddr( &sock, psz_server_addr, i_server_port ) == -1 )
        {
            msg_Err( p_this, "cannot build remote address" );
#if defined( WIN32 ) || defined( UNDER_CE )
            closesocket( i_handle );
#else
            close( i_handle );
#endif
            return( -1 );
        }

        /* Connect the socket */
        if( connect( i_handle, (struct sockaddr *) &sock,
                     sizeof( sock ) ) == (-1) )
        {
#ifdef HAVE_ERRNO_H
            msg_Err( p_this, "cannot connect socket (%s)", strerror(errno) );
#else
            msg_Err( p_this, "cannot connect socket" );
#endif
#if defined( WIN32 ) || defined( UNDER_CE )
            closesocket( i_handle );
#else
            close( i_handle );
#endif
            return( -1 );
        }

#ifndef WIN32
        if( IN_MULTICAST( ntohl(sock.sin_addr.s_addr) ) )
#else
        if( IN_MULTICAST( ntohl(inet_addr(psz_server_addr) ) ) )
#endif
        {
            /* set the time-to-live */
            int ttl = config_GetInt( p_this, "ttl" );
            if( ttl < 1 )
                ttl = 1;

            if( setsockopt( i_handle, IPPROTO_IP, IP_MULTICAST_TTL,
                            &ttl, sizeof( ttl ) ) < 0 )
            {
#ifdef HAVE_ERRNO_H
                msg_Warn( p_this, "failed to set ttl (%s)",
                          strerror(errno) );
#else
                msg_Warn( p_this, "failed to set ttl" );
#endif
#if defined( WIN32 ) || defined( UNDER_CE )
                closesocket( i_handle );
#else
                close( i_handle );
#endif
                return( -1 );
            }
        }
    }

    p_socket->i_handle = i_handle;
    p_socket->i_mtu = config_GetInt( p_this, "mtu" );
    return( 0 );
}

/*****************************************************************************
 * OpenTCP: open a TCP socket
 *****************************************************************************
 * psz_server_addr, i_server_port : address and port used for the connect()
 *   system call. If i_server_port == 0, 80 is used.
 * Other parameters are ignored.
 * This function returns -1 in case of error.
 *****************************************************************************/
static int OpenTCP( vlc_object_t * p_this, network_socket_t * p_socket )
{
    char * psz_server_addr = p_socket->psz_server_addr;
    int i_server_port = p_socket->i_server_port;

    int i_handle;
    struct sockaddr_in sock;

    if( i_server_port == 0 )
    {
        i_server_port = 80;
    }

    /* Open a SOCK_STREAM (TCP) socket, in the AF_INET domain, automatic (0)
     * protocol */
    if( (i_handle = socket( AF_INET, SOCK_STREAM, 0 )) == -1 )
    {
#ifdef HAVE_ERRNO_H
        msg_Err( p_this, "cannot create socket (%s)", strerror(errno) );
#else
        msg_Err( p_this, "cannot create socket" );
#endif
        return( -1 );
    }

    /* Build remote address */
    if ( BuildAddr( &sock, psz_server_addr, i_server_port ) == -1 )
    {
        msg_Dbg( p_this, "could not build local address" );
#if defined( WIN32 ) || defined( UNDER_CE )
        closesocket( i_handle );
#else
        close( i_handle );
#endif
        return( -1 );
    }

    /* Connect the socket */
    if( connect( i_handle, (struct sockaddr *) &sock,
                 sizeof( sock ) ) == (-1) )
    {
#ifdef HAVE_ERRNO_H
        msg_Err( p_this, "cannot connect socket (%s)", strerror(errno) );
#else
        msg_Err( p_this, "cannot connect socket" );
#endif
#if defined( WIN32 ) || defined( UNDER_CE )
        closesocket( i_handle );
#else
        close( i_handle );
#endif
        return( -1 );
    }

    p_socket->i_handle = i_handle;
    p_socket->i_mtu = 0; /* There is no MTU notion in TCP */

    return( 0 );
}

/*****************************************************************************
 * NetOpen: wrapper around OpenUDP and OpenTCP
 *****************************************************************************/
static int NetOpen( vlc_object_t * p_this )
{
    network_socket_t * p_socket = p_this->p_private;

    if( p_socket->i_type == NETWORK_UDP )
    {
        return OpenUDP( p_this, p_socket );
    }
    else
    {
        return OpenTCP( p_this, p_socket );
    }
}
