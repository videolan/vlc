/*****************************************************************************
 * ipv6.c: IPv6 network abstraction layer
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: ipv6.c,v 1.2 2002/03/11 07:23:09 gbazin Exp $
 *
 * Authors: Alexis Guillard <alexis.guillard@bt.com>
 *          Christophe Massiot <massiot@via.ecp.fr>
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
#include <sys/socket.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <videolan/vlc.h>

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
#   ifdef HAVE_ARPA_INET_H
#       include <arpa/inet.h>                    /* inet_ntoa(), inet_aton() */
#   endif
#endif

#include "network.h"

/* Default MTU used for UDP socket. FIXME: we should issue some ioctl()
 * call to get that value from the interface driver. */
#define DEFAULT_MTU 1500

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void getfunctions( function_list_t * );
static int  NetworkOpen( struct network_socket_s * );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP
 
MODULE_INIT_START
    SET_DESCRIPTION( "IPv6 network abstraction layer" )
    ADD_CAPABILITY( NETWORK, 40 )
    ADD_SHORTCUT( "ipv6" )
MODULE_INIT_STOP
 
MODULE_ACTIVATE_START
    getfunctions( &p_module->p_functions->network );
MODULE_ACTIVATE_STOP
 
MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void getfunctions( function_list_t * p_function_list )
{
#define f p_function_list->functions.network
    f.pf_open = NetworkOpen;
#undef f
}

/*****************************************************************************
 * BuildAddr: utility function to build a struct sockaddr_in6
 *****************************************************************************/
static int BuildAddr( struct sockaddr_in6 * p_socket,
                      char * psz_address, int i_port )
{
    /* Reset struct */
    memset( p_socket, 0, sizeof( struct sockaddr_in6 ) );
    p_socket->sin6_family = AF_INET6;                               /* family */
    p_socket->sin6_port = htons( i_port );
    if( psz_address == NULL )
    {
        p_socket->sin6_addr = in6addr_any;
    }
    else if( *psz_address != '['
              || psz_address[strlen(psz_address) - 1] != ']' )
    {
        intf_ErrMsg( "ipv6: IPv6 address is invalid, discarding" );
        return( -1 );
    } 
    else
    {
        psz_address++;
        psz_address[strlen(psz_address) - 1] = '\0' ;
        inet_pton(AF_INET6, psz_address, &p_socket->sin6_addr.s6_addr); 
    }
    return( 0 );
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
static int OpenUDP( network_socket_t * p_socket )
{
    char * psz_bind_addr = p_socket->psz_bind_addr;
    int i_bind_port = p_socket->i_bind_port;
    char * psz_server_addr = p_socket->psz_server_addr;
    int i_server_port = p_socket->i_server_port;

    int i_handle, i_opt, i_opt_size;
    struct sockaddr_in6 sock;

    if( i_bind_port == 0 )
    {
        i_bind_port = config_GetIntVariable( "server_port" );
    }

    /* Open a SOCK_DGRAM (UDP) socket, in the AF_INET6 domain, automatic (0)
     * protocol */
    if( (i_handle = socket( AF_INET6, SOCK_DGRAM, 0 )) == -1 )
    {
        intf_ErrMsg( "ipv6 error: cannot create socket (%s)", strerror(errno) );
        return( -1 );
    }

    /* We may want to reuse an already used socket */
    i_opt = 1;
    if( setsockopt( i_handle, SOL_SOCKET, SO_REUSEADDR,
                    (void *) &i_opt, sizeof( i_opt ) ) == -1 )
    {
        intf_ErrMsg( "ipv6 error: cannot configure socket (SO_REUSEADDR: %s)",
                     strerror(errno));
        close( i_handle );
        return( -1 );
    }

    /* Increase the receive buffer size to 1/2MB (8Mb/s during 1/2s) to avoid
     * packet loss caused by scheduling problems */
    i_opt = 0x80000;
    if( setsockopt( i_handle, SOL_SOCKET, SO_RCVBUF,
                    (void *) &i_opt, sizeof( i_opt ) ) == -1 )
    {
        intf_WarnMsg( 1,
                      "ipv6 warning: cannot configure socket (SO_RCVBUF: %s)",
                      strerror(errno));
    }
 
    /* Check if we really got what we have asked for, because Linux, etc.
     * will silently limit the max buffer size to net.core.rmem_max which
     * is typically only 65535 bytes */
    i_opt = 0;
    i_opt_size = sizeof( i_opt );
    if( getsockopt( i_handle, SOL_SOCKET, SO_RCVBUF,
                    (void*) &i_opt, &i_opt_size ) == -1 )
    {
        intf_WarnMsg( 1, "ipv6 warning: cannot query socket (SO_RCVBUF: %s)",
                         strerror(errno));
    }
    else if( i_opt < 0x80000 )
    {
        intf_WarnMsg( 1, "ipv6 warning: socket buffer size is 0x%x"
                         " instead of 0x%x", i_opt, 0x80000 );
    }
    
    /* Build the local socket */
    if ( BuildAddr( &sock, psz_bind_addr, i_bind_port ) == -1 )        
    {
        close( i_handle );
        return( -1 );
    }
 
    /* Bind it */
    if( bind( i_handle, (struct sockaddr *)&sock, sizeof( sock ) ) < 0 )
    {
        intf_ErrMsg( "ipv6 error: cannot bind socket (%s)", strerror(errno) );
        close( i_handle );
        return( -1 );
    }

    /* Allow broadcast reception if we bound on in6addr_any */
    if( psz_bind_addr == NULL )
    {
        i_opt = 1;
        if( setsockopt( i_handle, SOL_SOCKET, SO_BROADCAST,
                        (void*) &i_opt, sizeof( i_opt ) ) == -1 )
        {
            intf_WarnMsg( 1,
                    "ipv6 warning: cannot configure socket (SO_BROADCAST: %s)",
                    strerror(errno));
        }
    }
 
    /* Join the multicast group if the socket is a multicast address */
    /* FIXME: To be written */

    if( psz_server_addr != NULL )
    {
        /* Build socket for remote connection */
        if ( BuildAddr( &sock, psz_server_addr, i_server_port ) == -1 )
        {
            intf_ErrMsg( "ipv6 error: cannot build remote address" );
            close( i_handle );
            return( -1 );
        }
 
        /* Connect the socket */
        if( connect( i_handle, (struct sockaddr *) &sock,
                     sizeof( sock ) ) == (-1) )
        {
            intf_ErrMsg( "ipv6 error: cannot connect socket (%s)",
                         strerror(errno) );
            close( i_handle );
            return( -1 );
        }
    }

    p_socket->i_handle = i_handle;
    p_socket->i_mtu = DEFAULT_MTU;
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
static int OpenTCP( network_socket_t * p_socket )
{
    char * psz_server_addr = p_socket->psz_server_addr;
    int i_server_port = p_socket->i_server_port;

    int i_handle;
    struct sockaddr_in6 sock;

    if( i_server_port == 0 )
    {
        i_server_port = 80;
    }

    /* Open a SOCK_STREAM (TCP) socket, in the AF_INET6 domain, automatic (0)
     * protocol */
    if( (i_handle = socket( AF_INET6, SOCK_STREAM, 0 )) == -1 )
    {
        intf_ErrMsg( "ipv6 error: cannot create socket (%s)", strerror(errno) );
        return( -1 );
    }

    /* Build remote address */
    if ( BuildAddr( &sock, psz_server_addr, i_server_port ) == -1 )
    {
        close( i_handle );
        return( -1 );
    }

    /* Connect the socket */
    if( connect( i_handle, (struct sockaddr *) &sock,
                 sizeof( sock ) ) == (-1) )
    {
        intf_ErrMsg( "ipv6 error: cannot connect socket (%s)",
                     strerror(errno) );
        close( i_handle );
        return( -1 );
    }

    p_socket->i_handle = i_handle;
    p_socket->i_mtu = 0; /* There is no MTU notion in TCP */

    return( 0 );
}

/*****************************************************************************
 * NetworkOpen: wrapper around OpenUDP and OpenTCP
 *****************************************************************************/
static int NetworkOpen( network_socket_t * p_socket )
{
    if( p_socket->i_type == NETWORK_UDP )
    {
        return OpenUDP( p_socket );
    }
    else
    {
        return OpenTCP( p_socket );
    }
}


