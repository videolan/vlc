/*****************************************************************************
 * udp.c:
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
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include "network.h"

#ifdef WIN32
#   if defined(UNDER_CE)
#       undef IP_MULTICAST_TTL
#       define IP_MULTICAST_TTL 3
#       undef IP_ADD_MEMBERSHIP
#       define IP_ADD_MEMBERSHIP 5
#   endif
#   define EAFNOSUPPORT WSAEAFNOSUPPORT
#endif

#ifndef SOL_IP
# define SOL_IP IPPROTO_IP
#endif
#ifndef SOL_IPV6
# define SOL_IPV6 IPPROTO_IPV6
#endif
#ifndef IPPROTO_IPV6
# define IPPROTO_IPV6 41
#endif

extern int net_Socket( vlc_object_t *p_this, int i_family, int i_socktype,
                       int i_protocol );


static int net_SetMcastHopLimit( vlc_object_t *p_this,
                                 int fd, int family, int hlim )
{
#ifndef SYS_BEOS
    int proto, cmd;

    /* There is some confusion in the world whether IP_MULTICAST_TTL 
     * takes a byte or an int as an argument.
     * BSD seems to indicate byte so we are going with that and use
     * int as a fallback to be safe */
    switch( family )
    {
        case AF_INET:
            proto = SOL_IP;
            cmd = IP_MULTICAST_TTL;
            break;

#ifdef IPV6_MULTICAST_HOPS
        case AF_INET6:
            proto = SOL_IPV6;
            cmd = IPV6_MULTICAST_HOPS;
            break;
#endif

        default:
            msg_Warn( p_this, "%s", strerror( EAFNOSUPPORT ) );
            return VLC_EGENERIC;
    }

    if( setsockopt( fd, proto, cmd, &hlim, sizeof( hlim ) ) < 0 )
    {
        /* BSD compatibility */
        unsigned char buf;

        buf = (unsigned char)(( hlim > 255 ) ? 255 : hlim);
        if( setsockopt( fd, proto, cmd, &buf, sizeof( buf ) ) )
            return VLC_EGENERIC;
    }
#endif
    return VLC_SUCCESS;
}


static int net_SetMcastSource( vlc_object_t *p_this,
                                int fd, int family, const char *str )
{
#ifndef SYS_BEOS
    switch( family )
    {
        case AF_INET:
        {
            struct in_addr addr;

            if( inet_pton( AF_INET, str, &addr) <= 0 )
            {
                msg_Err( p_this, "Invalid multicast interface %s",
                         str );
                return VLC_EGENERIC;
            }

            if( setsockopt( fd, IPPROTO_IP, IP_MULTICAST_IF, &addr,
                            sizeof( addr ) ) < 0 )
            {
                msg_Dbg( p_this, "Cannot set multicast interface (%s)",
                         strerror(errno) );
                return VLC_EGENERIC;
            }
            break;
        }

#ifdef IPV6_MULTICAST_IF
/* FIXME: TODO */
#endif
        default:
            msg_Warn( p_this, "%s", strerror( EAFNOSUPPORT ) );
            return VLC_EGENERIC;
    }

#endif
    return VLC_SUCCESS;
}

/*****************************************************************************
 * __net_ConnectUDP:
 *****************************************************************************
 * Open a UDP socket to send data to a defined destination, with an optional
 * hop limit.
 *****************************************************************************/
int __net_ConnectUDP( vlc_object_t *p_this, const char *psz_host, int i_port,
                      int i_hlim )
{
    struct addrinfo hints, *res, *ptr;
    int             i_val, i_handle = -1;
    vlc_bool_t      b_unreach = VLC_FALSE;

    if( i_port == 0 )
        i_port = 1234; /* historical VLC thing */

    if( i_hlim < 1 )
    {
        i_hlim = var_CreateGetInteger( p_this, "ttl" );
        if( i_hlim < 1 )
            i_hlim = 1;
    }

    memset( &hints, 0, sizeof( hints ) );
    hints.ai_socktype = SOCK_DGRAM;

    msg_Dbg( p_this, "net: connecting to %s port %d", psz_host, i_port );

    i_val = vlc_getaddrinfo( p_this, psz_host, i_port, &hints, &res );
    if( i_val )
    {
        msg_Err( p_this, "cannot resolve %s port %d : %s", psz_host, i_port,
                 vlc_gai_strerror( i_val ) );
        return -1;
    }

    for( ptr = res; ptr != NULL; ptr = ptr->ai_next )
    {
        int fd;
        char *psz_mif_addr;

        fd = net_Socket( p_this, ptr->ai_family, ptr->ai_socktype,
                         ptr->ai_protocol );
        if( fd == -1 )
            continue;
#if !defined( SYS_BEOS )
        else
        {
            int i_val;

            /* Increase the receive buffer size to 1/2MB (8Mb/s during 1/2s)
             * to avoid packet loss caused by scheduling problems */
            i_val = 0x80000;
            setsockopt( fd, SOL_SOCKET, SO_RCVBUF, (void *)&i_val,
                        sizeof( i_val ) );
            i_val = 0x80000;
            setsockopt( fd, SOL_SOCKET, SO_SNDBUF, (void *)&i_val,
                        sizeof( i_val ) );

            /* Allow broadcast sending */
            i_val = 1;
            setsockopt( i_handle, SOL_SOCKET, SO_BROADCAST, (void*)&i_val,
                        sizeof( i_val ) );
        }
#endif

        net_SetMcastHopLimit( p_this, fd, ptr->ai_family, i_hlim );
        psz_mif_addr = config_GetPsz( p_this, "miface-addr" );
        if( psz_mif_addr != NULL )
        {
            net_SetMcastSource( p_this, fd, ptr->ai_family, psz_mif_addr );
            free( psz_mif_addr );
        }

        if( connect( fd, ptr->ai_addr, ptr->ai_addrlen ) == 0 )
        {
            /* success */
            i_handle = fd;
            break;
        }

#if defined( WIN32 ) || defined( UNDER_CE )
        if( WSAGetLastError () == WSAENETUNREACH )
#else
        if( errno == ENETUNREACH )
#endif
            b_unreach = VLC_TRUE;
        else
        {
            msg_Warn( p_this, "%s port %d : %s", psz_host, i_port,
                      strerror( errno ) );
            net_Close( fd );
            continue;
        }
    }

    vlc_freeaddrinfo( res );

    if( i_handle == -1 )
    {
        if( b_unreach )
            msg_Err( p_this, "Host %s port %d is unreachable", psz_host,
                     i_port );
        return -1;
    }

    return i_handle;
}

/*****************************************************************************
 * __net_OpenUDP:
 *****************************************************************************
 * Open a UDP connection and return a handle
 *****************************************************************************/
int __net_OpenUDP( vlc_object_t *p_this, const char *psz_bind, int i_bind,
                   const char *psz_server, int i_server )
{
    vlc_value_t      v4, v6;
    void            *private;
    network_socket_t sock;
    module_t         *p_network = NULL;

    if( ( psz_server != NULL ) && ( psz_server[0] == '\0' ) )
        msg_Warn( p_this, "calling net_OpenUDP with an explicit destination "
                  "is obsolete - use net_ConnectUDP instead" );
    if( i_server != 0 )
        msg_Warn( p_this, "calling net_OpenUDP with an explicit destination "
                  "port is obsolete - use __net_ConnectUDP instead" );

    if( psz_server == NULL ) psz_server = "";
    if( psz_bind == NULL ) psz_bind = "";

    /* Prepare the network_socket_t structure */
    sock.psz_bind_addr   = psz_bind;
    sock.i_bind_port     = i_bind;
    sock.psz_server_addr = psz_server;
    sock.i_server_port   = i_server;
    sock.i_ttl           = 0;
    sock.v6only          = 0;
    sock.i_handle        = -1;

    msg_Dbg( p_this, "net: connecting to '[%s]:%d@[%s]:%d'",
             psz_server, i_server, psz_bind, i_bind );

    /* Check if we have force ipv4 or ipv6 */
    var_Create( p_this, "ipv4", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Get( p_this, "ipv4", &v4 );
    var_Create( p_this, "ipv6", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Get( p_this, "ipv6", &v6 );

    if( !v4.b_bool )
    {
        if( v6.b_bool )
            sock.v6only = 1;

        /* try IPv6 first (unless IPv4 forced) */
        private = p_this->p_private;
        p_this->p_private = (void*)&sock;
        p_network = module_Need( p_this, "network", "ipv6", VLC_TRUE );

        if( p_network != NULL )
            module_Unneed( p_this, p_network );

        p_this->p_private = private;

        /*
         * Check if the IP stack can receive IPv4 packets on IPv6 sockets.
         * If yes, then it is better to use the IPv6 socket.
         * Otherwise, if we also get an IPv4, we have to choose, so we use
         * IPv4 only.
         */
        if( ( sock.i_handle != -1 ) && ( ( sock.v6only == 0 ) || v6.b_bool ) )
            return sock.i_handle;
    }

    if( !v6.b_bool )
    {
        int fd6 = sock.i_handle;

        /* also try IPv4 (unless IPv6 forced) */
        private = p_this->p_private;
        p_this->p_private = (void*)&sock;
        p_network = module_Need( p_this, "network", "ipv4", VLC_TRUE );

        if( p_network != NULL )
            module_Unneed( p_this, p_network );

        p_this->p_private = private;

        if( fd6 != -1 )
        {
            if( sock.i_handle != -1 )
            {
                msg_Warn( p_this, "net: lame IPv6/IPv4 dual-stack present. "
                                  "Using only IPv4." );
                net_Close( fd6 );
            }
            else
                sock.i_handle = fd6;
        }
    }

    if( sock.i_handle == -1 )
        msg_Dbg( p_this, "net: connection to '[%s]:%d@[%s]:%d' failed",
                psz_server, i_server, psz_bind, i_bind );

    return sock.i_handle;
}
