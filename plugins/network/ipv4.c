/*****************************************************************************
 * ipv4.c: IPv4 network abstraction layer
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: ipv4.c,v 1.12.2.1 2002/07/19 21:12:18 massiot Exp $
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
#include <sys/types.h>
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
    SET_DESCRIPTION( _("IPv4 network abstraction layer") )
    ADD_CAPABILITY( NETWORK, 50 )
    ADD_SHORTCUT( "ipv4" )
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
 * BuildAddr: utility function to build a struct sockaddr_in
 *****************************************************************************/
static int BuildAddr( struct sockaddr_in * p_socket,
                      const char * psz_address, int i_port )
{
    /* Reset struct */
    memset( p_socket, 0, sizeof( struct sockaddr_in ) );
    p_socket->sin_family = AF_INET;                                /* family */
    p_socket->sin_port = htons( i_port );
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
        if( (p_socket->sin_addr.s_addr = inet_addr( psz_address )) == -1 )
#endif
        {
            /* We have a fqdn, try to find its address */
            if ( (p_hostent = gethostbyname( psz_address )) == NULL )
            {
                intf_ErrMsg( "BuildLocalAddr: unknown host %s", psz_address );
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
static int OpenUDP( network_socket_t * p_socket )
{
    char * psz_bind_addr = p_socket->psz_bind_addr;
    int i_bind_port = p_socket->i_bind_port;
    char * psz_server_addr = p_socket->psz_server_addr;
    int i_server_port = p_socket->i_server_port;
#ifdef WIN32
    char * psz_bind_win32;        /* WIN32 multicast kludge */
#endif

    int i_handle, i_opt, i_opt_size;
    struct sockaddr_in sock;

    if( i_bind_port == 0 )
    {
        i_bind_port = config_GetIntVariable( "server-port" );
    }

    /* Open a SOCK_DGRAM (UDP) socket, in the AF_INET domain, automatic (0)
     * protocol */
    if( (i_handle = socket( AF_INET, SOCK_DGRAM, 0 )) == -1 )
    {
        intf_ErrMsg( "ipv4 error: cannot create socket (%s)", strerror(errno) );
        return( -1 );
    }

    /* We may want to reuse an already used socket */
    i_opt = 1;
    if( setsockopt( i_handle, SOL_SOCKET, SO_REUSEADDR,
                    (void *) &i_opt, sizeof( i_opt ) ) == -1 )
    {
        intf_ErrMsg( "ipv4 error: cannot configure socket (SO_REUSEADDR: %s)",
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
                      "ipv4 warning: cannot configure socket (SO_RCVBUF: %s)",
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
        intf_WarnMsg( 1, "ipv4 warning: cannot query socket (SO_RCVBUF: %s)",
                         strerror(errno));
    }
    else if( i_opt < 0x80000 )
    {
        intf_WarnMsg( 1, "ipv4 warning: socket buffer size is 0x%x"
                         " instead of 0x%x", i_opt, 0x80000 );
    }
    
    
    /* Build the local socket */

#ifdef WIN32
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
        close( i_handle );
        return( -1 );
    }
 
    /* Bind it */
    if( bind( i_handle, (struct sockaddr *)&sock, sizeof( sock ) ) < 0 )
    {
        intf_ErrMsg( "ipv4 error: cannot bind socket (%s)", strerror(errno) );
        close( i_handle );
        return( -1 );
    }

    /* Allow broadcast reception if we bound on INADDR_ANY */
    if( !*psz_bind_addr )
    {
        i_opt = 1;
        if( setsockopt( i_handle, SOL_SOCKET, SO_BROADCAST,
                        (void*) &i_opt, sizeof( i_opt ) ) == -1 )
        {
            intf_WarnMsg( 1,
                    "ipv4 warning: cannot configure socket (SO_BROADCAST: %s)",
                    strerror(errno));
        }
    }
 
    /* Join the multicast group if the socket is a multicast address */
#ifndef IN_MULTICAST
#   define IN_MULTICAST(a)         IN_CLASSD(a)
#endif

#ifndef WIN32
    if( IN_MULTICAST( ntohl(sock.sin_addr.s_addr) ) )
    {
        struct ip_mreq imr;
        imr.imr_interface.s_addr = INADDR_ANY;
        imr.imr_multiaddr.s_addr = sock.sin_addr.s_addr;
#else
    if( IN_MULTICAST( ntohl(inet_addr(psz_bind_addr) ) ) )
    {
        struct ip_mreq imr;
        imr.imr_interface.s_addr = INADDR_ANY;
        imr.imr_multiaddr.s_addr = inet_addr(psz_bind_addr);
#endif                
        if( setsockopt( i_handle, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                        (char*)&imr, sizeof(struct ip_mreq) ) == -1 )
        {
            intf_ErrMsg( "ipv4 error: failed to join IP multicast group (%s)",
                         strerror(errno) );
            close( i_handle );
            return( -1 );
        }
    }

    if( *psz_server_addr )
    {
        /* Build socket for remote connection */
        if ( BuildAddr( &sock, psz_server_addr, i_server_port ) == -1 )
        {
            intf_ErrMsg( "ipv4 error: cannot build remote address" );
            close( i_handle );
            return( -1 );
        }
 
        /* Connect the socket */
        if( connect( i_handle, (struct sockaddr *) &sock,
                     sizeof( sock ) ) == (-1) )
        {
            intf_ErrMsg( "ipv4 error: cannot connect socket (%s)",
                         strerror(errno) );
            close( i_handle );
            return( -1 );
        }
    }

    p_socket->i_handle = i_handle;
    p_socket->i_mtu = config_GetIntVariable( "mtu" );
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
    struct sockaddr_in sock;

    if( i_server_port == 0 )
    {
        i_server_port = 80;
    }

    /* Open a SOCK_STREAM (TCP) socket, in the AF_INET domain, automatic (0)
     * protocol */
    if( (i_handle = socket( AF_INET, SOCK_STREAM, 0 )) == -1 )
    {
        intf_ErrMsg( "ipv4 error: cannot create socket (%s)", strerror(errno) );
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
        intf_ErrMsg( "ipv4 error: cannot connect socket (%s)",
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
