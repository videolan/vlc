/*****************************************************************************
 * ipv6.c: IPv6 network abstraction layer
 *****************************************************************************
 * Copyright (C) 2002-2005 VideoLAN
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
#   include <net/if.h>
#   ifdef HAVE_ARPA_INET_H
#       include <arpa/inet.h>                    /* inet_ntoa(), inet_aton() */
#   endif
#endif

#include "network.h"

#if defined(WIN32)
static const struct in6_addr in6addr_any = {{IN6ADDR_ANY_INIT}};
/* the following will have to be removed when w32api defines them */
#ifndef IPPROTO_IPV6
#   define IPPROTO_IPV6 41 
#endif
#ifndef IPV6_JOIN_GROUP
#   define IPV6_JOIN_GROUP 12
#endif
#ifndef IPV6_MULTICAST_HOPS
#   define IPV6_MULTICAST_HOPS 10
#endif
#ifndef IPV6_UNICAST_HOPS
#   define IPV6_UNICAST_HOPS 4
#endif
#   define close closesocket
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int NetOpen( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("IPv6 network abstraction layer") );
    set_capability( "network", 40 );
    set_callbacks( NetOpen, NULL );
vlc_module_end();

/*****************************************************************************
 * BuildAddr: utility function to build a struct sockaddr_in6
 *****************************************************************************/
static int BuildAddr( vlc_object_t * p_this, struct sockaddr_in6 * p_socket,
                      const char * psz_bind_address, int i_port )
{
    char * psz_multicast_interface = "";
    char * psz_backup = strdup(psz_bind_address);
    char * psz_address = psz_backup;

#if defined(WIN32)
    /* Try to get getaddrinfo() and freeaddrinfo() from wship6.dll */
    typedef int (CALLBACK * GETADDRINFO) ( const char *nodename,
                                            const char *servname,
                                            const struct addrinfo *hints,
                                            struct addrinfo **res );
    typedef void (CALLBACK * FREEADDRINFO) ( struct addrinfo FAR *ai );

    struct addrinfo hints, *res;
    GETADDRINFO _getaddrinfo = NULL;
    FREEADDRINFO _freeaddrinfo = NULL;

    HINSTANCE wship6_dll = LoadLibrary("wship6.dll");
    if( wship6_dll )
    {
        _getaddrinfo = (GETADDRINFO) GetProcAddress( wship6_dll,
                                                     "getaddrinfo" );
        _freeaddrinfo = (FREEADDRINFO) GetProcAddress( wship6_dll,
                                                       "freeaddrinfo" );
    }
    if( !_getaddrinfo || !_freeaddrinfo )
    {
        msg_Warn( p_this, "no IPv6 stack installed" );
        if( wship6_dll ) FreeLibrary( wship6_dll );
        free( psz_backup );
        return( -1 );
    }
#endif

    /* Reset struct */
    memset( p_socket, 0, sizeof( struct sockaddr_in6 ) );
    p_socket->sin6_family = AF_INET6;                              /* family */
    p_socket->sin6_port = htons( i_port );
    if( !*psz_address )
    {
        p_socket->sin6_addr = in6addr_any;
    }
    else if( psz_address[0] == '['
              && psz_address[strlen(psz_address) - 1] == ']' )
    {
        psz_address[strlen(psz_address) - 1] = '\0';
        psz_address++;

        /* see if there is an interface name in there... */
        if( (psz_multicast_interface = strchr(psz_address, '%')) != NULL )
        {
            *psz_multicast_interface = '\0';
            psz_multicast_interface++;
            msg_Dbg( p_this, "Interface name specified: \"%s\"",
                             psz_multicast_interface );

            /* now convert that interface name to an index */
#if defined( WIN32 )
            /* FIXME ?? */
            p_socket->sin6_scope_id = atol(psz_multicast_interface);
#elif defined( HAVE_IF_NAMETOINDEX )
            p_socket->sin6_scope_id = if_nametoindex(psz_multicast_interface);
#endif
            msg_Dbg( p_this, " = #%i", p_socket->sin6_scope_id );
        }

#if !defined( WIN32 )
        inet_pton(AF_INET6, psz_address, &p_socket->sin6_addr.s6_addr); 

#else
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET6;

        if( _getaddrinfo( psz_address, NULL, &hints, &res ) != 0 )
        {
            FreeLibrary( wship6_dll );
            free( psz_backup );
            return( -1 );
        }
        memcpy( &p_socket->sin6_addr,
                &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr,
                sizeof(struct in6_addr) );
        _freeaddrinfo( res );

#endif
    }
    else
    {
#ifdef HAVE_GETHOSTBYNAME2
        struct hostent    * p_hostent;

        /* We have a fqdn, try to find its address */
        if ( (p_hostent = gethostbyname2( psz_address, AF_INET6 )) == NULL )
        {
            msg_Warn( p_this, "IPv6 error: unknown host %s", psz_address );
            free( psz_backup );
            return( -1 );
        }

        /* Copy the first address of the host in the socket address */
        memcpy( &p_socket->sin6_addr, p_hostent->h_addr_list[0],
                 p_hostent->h_length );

#elif defined(WIN32)
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET6;

        if( _getaddrinfo( psz_address, NULL, &hints, &res ) != 0 )
        {
            FreeLibrary( wship6_dll );
            free( psz_backup );
            return( -1 );
        }
        memcpy( &p_socket->sin6_addr,
                &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr,
                sizeof(struct in6_addr) );
        _freeaddrinfo( res );

#else
        msg_Warn( p_this, "IPv6 error: IPv6 address %s is invalid",
                 psz_address );
        free( psz_backup );
        return( -1 );
#endif
    }

#if defined(WIN32)
    FreeLibrary( wship6_dll );
#endif

    free( psz_backup );
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
static int OpenUDP( vlc_object_t * p_this, network_socket_t * p_socket )
{
    char * psz_bind_addr = p_socket->psz_bind_addr;
    int i_bind_port = p_socket->i_bind_port;
    char * psz_server_addr = p_socket->psz_server_addr;
    int i_server_port = p_socket->i_server_port;

    int i_handle, i_opt;
    socklen_t i_opt_size;
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
        if( ptr->ai_family == PF_INET6 )
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

    /* Check if we really got what we have asked for, because Linux, etc.
     * will silently limit the max buffer size to net.core.rmem_max which
     * is typically only 65535 bytes */
    i_opt = 0;
    i_opt_size = sizeof( i_opt );
    if( getsockopt( i_handle, SOL_SOCKET, SO_RCVBUF,
                    (void*) &i_opt, &i_opt_size ) == -1 )
    {
        msg_Warn( p_this, "cannot query socket (SO_RCVBUF: %s)",
                          strerror(errno) );
    }
    else if( i_opt < 0x80000 )
    {
        msg_Warn( p_this, "Socket buffer size is 0x%x instead of 0x%x",
                          i_opt, 0x80000 );
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
    } else
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

/*****************************************************************************
 * NetOpen: wrapper around OpenUDP, ListenTCP and OpenTCP
 *****************************************************************************/
static int NetOpen( vlc_object_t * p_this )
{
    network_socket_t * p_socket = p_this->p_private;

    return OpenUDP( p_this, p_socket );
}
