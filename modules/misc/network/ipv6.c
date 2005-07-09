/*****************************************************************************
 * ipv6.c: IPv6 network abstraction layer
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Alexis Guillard <alexis.guillard@bt.com>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Remco Poortinga <poortinga@telin.nl>
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
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <vlc/vlc.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 )
#   include <io.h>
#endif

#ifdef WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#elif !defined( SYS_BEOS ) && !defined( SYS_NTO )
#   include <netdb.h>                                         /* hostent ... */
#   include <sys/socket.h>
#   include <netinet/in.h>
#endif

#include "network.h"

#if defined(WIN32)
static const struct in6_addr in6addr_any = {{IN6ADDR_ANY_INIT}};
/* the following will have to be removed when w32api defines them */
#   ifndef IPPROTO_IPV6
#      define IPPROTO_IPV6 41 
#   endif
#   ifndef IPV6_JOIN_GROUP
#      define IPV6_JOIN_GROUP 12
#   endif
#   ifndef IPV6_MULTICAST_HOPS
#      define IPV6_MULTICAST_HOPS 10
#   endif
#   ifndef IPV6_UNICAST_HOPS
#      define IPV6_UNICAST_HOPS 4
#   endif
#   define close closesocket
#endif

#ifndef MCAST_JOIN_SOURCE_GROUP
#   define MCAST_JOIN_SOURCE_GROUP         46
struct group_source_req
{
       uint32_t           gsr_interface;  /* interface index */
       struct sockaddr_storage gsr_group;      /* group address */
       struct sockaddr_storage gsr_source;     /* source address */
};
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int OpenUDP( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("IPv6 network abstraction layer") );
    set_capability( "network", 40 );
    set_callbacks( OpenUDP, NULL );
vlc_module_end();

/*****************************************************************************
 * BuildAddr: utility function to build a struct sockaddr_in6
 *****************************************************************************/
static int BuildAddr( vlc_object_t *p_this, struct sockaddr_in6 *p_socket,
                      const char *psz_address, int i_port )
{
    struct addrinfo hints, *res;
    int i;

    memset( &hints, 0, sizeof( hints ) );
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    i = vlc_getaddrinfo( p_this, psz_address, 0, &hints, &res );
    if( i )
    {
        msg_Dbg( p_this, "%s: %s", psz_address, vlc_gai_strerror( i ) );
        return -1;
    }
    if ( res->ai_addrlen > sizeof (struct sockaddr_in6) )
    {
        vlc_freeaddrinfo( res );
        return -1;
    }

    memcpy( p_socket, res->ai_addr, res->ai_addrlen );
    vlc_freeaddrinfo( res );
    p_socket->sin6_port = htons( i_port );

    return 0;
}

/*****************************************************************************
 * OpenUDP: open a UDP socket
 *****************************************************************************
 * psz_bind_addr, i_bind_port : address and port used for the bind()
 *   system call. If psz_bind_addr == NULL, the socket is bound to
 *   in6addr_any and broadcast reception is enabled. If i_bind_port == 0,
 *   1234 is used. If psz_bind_addr is a multicast (class D) address,
 *   join the multicast group.
 * psz_server_addr, i_server_port : address and port used for the connect()
 *   system call. It can avoid receiving packets from unauthorized IPs.
 *   Its use leads to great confusion and is currently discouraged.
 * This function returns -1 in case of error.
 *****************************************************************************/
static int OpenUDP( vlc_object_t * p_this )
{
    network_socket_t *p_socket = p_this->p_private;
    const char *psz_bind_addr = p_socket->psz_bind_addr;
    int i_bind_port = p_socket->i_bind_port;
    const char *psz_server_addr = p_socket->psz_server_addr;
    int i_server_port = p_socket->i_server_port;
    int i_handle, i_opt;
    struct sockaddr_in6 sock;
    vlc_value_t val;

    /* Open a SOCK_DGRAM (UDP) socket, in the AF_INET6 domain, automatic (0)
     * protocol */
    if( (i_handle = socket( AF_INET6, SOCK_DGRAM, 0 )) == -1 )
    {
        msg_Warn( p_this, "cannot create socket (%s)", strerror(errno) );
        return( -1 );
    }

#ifdef WIN32
# ifdef IPV6_PROTECTION_LEVEL
        if( ptr->ai_family == AF_INET6 )
        {
            i_val = PROTECTION_LEVEL_UNRESTRICTED;
            setsockopt( fd, IPPROTO_IPV6, IPV6_PROTECTION_LEVEL, &i_val,
                        sizeof( i_val ) );
        }
# else
#  warning You are using outdated headers for Winsock !
# endif
#endif

    /* We may want to reuse an already used socket */
    i_opt = 1;
    if( setsockopt( i_handle, SOL_SOCKET, SO_REUSEADDR,
                    (void *) &i_opt, sizeof( i_opt ) ) == -1 )
    {
        msg_Warn( p_this, "cannot configure socket (SO_REUSEADDR: %s)",
                         strerror(errno) );
        close( i_handle );
        return( -1 );
    }

    /* Increase the receive buffer size to 1/2MB (8Mb/s during 1/2s) to avoid
     * packet loss caused by scheduling problems */
    i_opt = 0x80000;
    if( setsockopt( i_handle, SOL_SOCKET, SO_RCVBUF,
                    (void *) &i_opt, sizeof( i_opt ) ) == -1 )
    {
        msg_Warn( p_this, "cannot configure socket (SO_RCVBUF: %s)",
                          strerror(errno) );
    }

    /* Build the local socket */
    if ( BuildAddr( p_this, &sock, psz_bind_addr, i_bind_port ) == -1 )        
    {
        close( i_handle );
        return( -1 );
    }

#if defined(WIN32)
    /* Under Win32 and for multicasting, we bind to IN6ADDR_ANY */
    if( IN6_IS_ADDR_MULTICAST(&sock.sin6_addr) )
    {
        struct sockaddr_in6 sockany = sock;
        sockany.sin6_addr = in6addr_any;
        sockany.sin6_scope_id = 0;

        /* Bind it */
        if( bind( i_handle, (struct sockaddr *)&sockany, sizeof( sock ) ) < 0 )
        {
            msg_Warn( p_this, "cannot bind socket (%s)", strerror(errno) );
            close( i_handle );
            return( -1 );
        }
    }
    else
#endif
    /* Bind it */
    if( bind( i_handle, (struct sockaddr *)&sock, sizeof( sock ) ) < 0 )
    {
        msg_Warn( p_this, "cannot bind socket (%s)", strerror(errno) );
        close( i_handle );
        return( -1 );
    }

    /* Allow broadcast reception if we bound on in6addr_any */
    if( !*psz_bind_addr )
    {
        i_opt = 1;
        if( setsockopt( i_handle, SOL_SOCKET, SO_BROADCAST,
                        (void*) &i_opt, sizeof( i_opt ) ) == -1 )
        {
            msg_Warn( p_this, "IPv6 warning: cannot configure socket "
                              "(SO_BROADCAST: %s)", strerror(errno) );
        }
    }

    /* Join the multicast group if the socket is a multicast address */
#if defined( WIN32 ) || defined( HAVE_IF_NAMETOINDEX )
    if( IN6_IS_ADDR_MULTICAST(&sock.sin6_addr) )
    {
        if(*psz_server_addr)
        {
            struct group_source_req imr;
            struct sockaddr_in6 *p_sin6;

            imr.gsr_interface = 0;
            imr.gsr_group.ss_family = AF_INET6;
            imr.gsr_source.ss_family = AF_INET6;
            p_sin6 = (struct sockaddr_in6 *)&imr.gsr_group;
            p_sin6->sin6_addr = sock.sin6_addr;

            /* Build socket for remote connection */
            msg_Dbg( p_this, "psz_server_addr : %s", psz_server_addr);

            if ( BuildAddr( p_this, &sock, psz_server_addr, i_server_port ) )
            {
                msg_Warn( p_this, "cannot build remote address" );
                close( i_handle );
                return( -1 );
            }
            p_sin6 = (struct sockaddr_in6 *)&imr.gsr_source;
            p_sin6->sin6_addr = sock.sin6_addr;

            msg_Dbg( p_this, "IPV6_ADD_SOURCE_MEMBERSHIP multicast request" );
            if( setsockopt( i_handle, IPPROTO_IPV6, MCAST_JOIN_SOURCE_GROUP,
                          (char *)&imr, sizeof(struct group_source_req) ) == -1 )
            {

                msg_Err( p_this, "failed to join IP multicast group (%s)",
                                                          strerror(errno) );
            }    
        }
        else
        {
        
            struct ipv6_mreq     imr;
            int                  res;

            imr.ipv6mr_interface = sock.sin6_scope_id;
            imr.ipv6mr_multiaddr = sock.sin6_addr;
            res = setsockopt(i_handle, IPPROTO_IPV6, IPV6_JOIN_GROUP, (void*) &imr,
#if defined(WIN32)
                         sizeof(imr) + 4); /* Doesn't work without this */
#else
                         sizeof(imr));
#endif

            if( res == -1 )
            {
                msg_Err( p_this, "cannot join multicast group" );
            } 
        }
    }
#else
    msg_Warn( p_this, "Multicast IPv6 is not supported on your OS" );
#endif


    if( *psz_server_addr )
    {
        int ttl = p_socket->i_ttl;
        if( ttl < 1 )
        {
            ttl = config_GetInt( p_this, "ttl" );
        }
        if( ttl < 1 ) ttl = 1;

        /* Build socket for remote connection */
        if ( BuildAddr( p_this, &sock, psz_server_addr, i_server_port ) == -1 )
        {
            msg_Warn( p_this, "cannot build remote address" );
            close( i_handle );
            return( -1 );
        }

        /* Connect the socket */
        if( connect( i_handle, (struct sockaddr *) &sock,
                     sizeof( sock ) ) == (-1) )
        {
            msg_Warn( p_this, "cannot connect socket (%s)", strerror(errno) );
            close( i_handle );
            return( -1 );
        }

        /* Set the time-to-live */
        if( ttl > 1 )
        {
#if defined( WIN32 ) || defined( HAVE_IF_NAMETOINDEX )
            if( IN6_IS_ADDR_MULTICAST(&sock.sin6_addr) )
            {
                if( setsockopt( i_handle, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
                                (void *)&ttl, sizeof( ttl ) ) < 0 )
                {
                    msg_Err( p_this, "failed to set multicast ttl (%s)",
                             strerror(errno) );
                }
            }
            else
#endif
            {
                if( setsockopt( i_handle, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
                                (void *)&ttl, sizeof( ttl ) ) < 0 )
                {
                    msg_Err( p_this, "failed to set unicast ttl (%s)",
                              strerror(errno) );
                }
            }
        }
    }

    p_socket->i_handle = i_handle;

    var_Create( p_this, "mtu", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_this, "mtu", &val );
    p_socket->i_mtu = val.i_int;

    return( 0 );
}
