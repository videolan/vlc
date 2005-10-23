/*****************************************************************************
 * ipv4.c: IPv4 network abstraction layer
 *****************************************************************************
 * Copyright (C) 2001-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Mathias Kretschmer <mathias@research.att.com>
 *          Alexis de Lattre <alexis@via.ecp.fr>
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
#include <string.h>

#include <vlc/vlc.h>
#include <errno.h>

#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#if defined(WIN32) || defined(UNDER_CE)
#   if defined(UNDER_CE) && defined(sockaddr_storage)
#       undef sockaddr_storage
#   endif
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   include <iphlpapi.h>
#   define close closesocket
#   if defined(UNDER_CE)
#       undef IP_MULTICAST_TTL
#       define IP_MULTICAST_TTL 3
#       undef IP_ADD_MEMBERSHIP
#       define IP_ADD_MEMBERSHIP 5
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
#ifndef IN_MULTICAST
#   define IN_MULTICAST(a) IN_CLASSD(a)
#endif


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int OpenUDP( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define MIFACE_TEXT N_("Multicast output interface")
#define MIFACE_LONGTEXT N_( \
    "Indicate here the multicast output interface. " \
    "This overrides the routing table.")

vlc_module_begin();
    set_shortname( "IPv4" );
    set_description( _("UDP/IPv4 network abstraction layer") );
    set_capability( "network", 50 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_GENERAL );
    set_callbacks( OpenUDP, NULL );
    add_string( "miface-addr", NULL, NULL, MIFACE_TEXT, MIFACE_LONGTEXT, VLC_TRUE );
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

#if defined(WIN32) || defined(UNDER_CE)
# define WINSOCK_STRERROR_SIZE 20
static const char *winsock_strerror( char *buf )
{
    snprintf( buf, WINSOCK_STRERROR_SIZE, "Winsock error %d",
              WSAGetLastError( ) );
    buf[WINSOCK_STRERROR_SIZE - 1] = '\0';
    return buf;
}
#endif


/*****************************************************************************
 * OpenUDP: open a UDP socket
 *****************************************************************************
 * psz_bind_addr, i_bind_port : address and port used for the bind()
 *   system call. If psz_bind_addr == "", the socket is bound to
 *   INADDR_ANY and broadcast reception is enabled. If psz_bind_addr is a
 *   multicast (class D) address, join the multicast group.
 * psz_server_addr, i_server_port : address and port used for the connect()
 *   system call. It can avoid receiving packets from unauthorized IPs.
 *   Its use leads to great confusion and is currently discouraged.
 * This function returns -1 in case of error.
 *****************************************************************************/
static int OpenUDP( vlc_object_t * p_this )
{
    network_socket_t * p_socket = p_this->p_private;
    const char * psz_bind_addr = p_socket->psz_bind_addr;
    int i_bind_port = p_socket->i_bind_port;
    const char * psz_server_addr = p_socket->psz_server_addr;
    int i_server_port = p_socket->i_server_port;

    int i_handle, i_opt;
    struct sockaddr_in sock;
    vlc_value_t val;
#if defined(WIN32) || defined(UNDER_CE)
    char strerror_buf[WINSOCK_STRERROR_SIZE];
# define strerror( x ) winsock_strerror( strerror_buf )
#endif

    /* If IP_ADD_SOURCE_MEMBERSHIP is not defined in the headers
       (because it's not in glibc for example), we have to define the
       headers required for IGMPv3 here */
#ifndef IP_ADD_SOURCE_MEMBERSHIP
    #define IP_ADD_SOURCE_MEMBERSHIP  39
    struct ip_mreq_source {
        struct in_addr  imr_multiaddr;
        struct in_addr  imr_interface;
        struct in_addr  imr_sourceaddr;
     };
#endif

    p_socket->i_handle = -1;

    /* Open a SOCK_DGRAM (UDP) socket, in the AF_INET domain, automatic (0)
     * protocol */
    if( (i_handle = socket( AF_INET, SOCK_DGRAM, 0 )) == -1 )
    {
        msg_Warn( p_this, "cannot create socket (%s)", strerror(errno) );
        return 0;
    }

    /* We may want to reuse an already used socket */
    i_opt = 1;
    if( setsockopt( i_handle, SOL_SOCKET, SO_REUSEADDR,
                    (void *) &i_opt, sizeof( i_opt ) ) == -1 )
    {
        msg_Warn( p_this, "cannot configure socket (SO_REUSEADDR: %s)",
                          strerror(errno));
        close( i_handle );
        return 0;
    }

#ifdef SO_REUSEPORT
    i_opt = 1;
    if( setsockopt( i_handle, SOL_SOCKET, SO_REUSEPORT,
                    (void *) &i_opt, sizeof( i_opt ) ) == -1 )
    {
        msg_Warn( p_this, "cannot configure socket (SO_REUSEPORT)" );
    }
#endif

    /* Increase the receive buffer size to 1/2MB (8Mb/s during 1/2s) to avoid
     * packet loss caused by scheduling problems */
#if !defined( SYS_BEOS )
    i_opt = 0x80000;
    if( setsockopt( i_handle, SOL_SOCKET, SO_RCVBUF, (void *) &i_opt,
                    sizeof( i_opt ) ) == -1 )
        msg_Dbg( p_this, "cannot configure socket (SO_RCVBUF: %s)",
                          strerror(errno));
    i_opt = 0x80000;
    if( setsockopt( i_handle, SOL_SOCKET, SO_SNDBUF, (void *) &i_opt,
                    sizeof( i_opt ) ) == -1 )
        msg_Dbg( p_this, "cannot configure socket (SO_SNDBUF: %s)",
                          strerror(errno));
#endif

    /* Build the local socket */

#if defined( WIN32 ) || defined( UNDER_CE )
    /* Under Win32 and for multicasting, we bind to INADDR_ANY,
     * so let's call BuildAddr with "" instead of psz_bind_addr */
    if( BuildAddr( &sock, IN_MULTICAST( ntohl( inet_addr(psz_bind_addr) ) ) ?
                   "" : psz_bind_addr, i_bind_port ) == -1 )
#else
    if( BuildAddr( &sock, psz_bind_addr, i_bind_port ) == -1 )
#endif
    {
        msg_Dbg( p_this, "could not build local address" );
        close( i_handle );
        return 0;
    }

    /* Bind it */
    if( bind( i_handle, (struct sockaddr *)&sock, sizeof( sock ) ) < 0 )
    {
        msg_Warn( p_this, "cannot bind socket (%s)", strerror(errno) );
        close( i_handle );
        return 0;
    }

#if defined( WIN32 ) || defined( UNDER_CE )
    /* Restore the sock struct so we can spare a few #ifdef WIN32 later on */
    if( IN_MULTICAST( ntohl( inet_addr(psz_bind_addr) ) ) )
    {
        if ( BuildAddr( &sock, psz_bind_addr, i_bind_port ) == -1 )
        {
            msg_Dbg( p_this, "could not build local address" );
            close( i_handle );
            return 0;
        }
    }
#endif

#if !defined( SYS_BEOS )
    /* Allow broadcast reception if we bound on INADDR_ANY */
    if( !*psz_bind_addr )
    {
        i_opt = 1;
        if( setsockopt( i_handle, SOL_SOCKET, SO_BROADCAST, (void*) &i_opt,
                        sizeof( i_opt ) ) == -1 )
            msg_Warn( p_this, "cannot configure socket (SO_BROADCAST: %s)",
                       strerror(errno) );
    }
#endif

#if !defined( SYS_BEOS )
    /* Join the multicast group if the socket is a multicast address */
    if( IN_MULTICAST( ntohl(sock.sin_addr.s_addr) ) )
    {
        /* Determine interface to be used for multicast */
        char * psz_if_addr = config_GetPsz( p_this, "miface-addr" );

        /* If we have a source address, we use IP_ADD_SOURCE_MEMBERSHIP
           so that IGMPv3 aware OSes running on IGMPv3 aware networks
           will do an IGMPv3 query on the network */
        if( *psz_server_addr )
        {
            struct ip_mreq_source imr;

            imr.imr_multiaddr.s_addr = sock.sin_addr.s_addr;
            imr.imr_sourceaddr.s_addr = inet_addr(psz_server_addr);

            if( psz_if_addr != NULL && *psz_if_addr
                && inet_addr(psz_if_addr) != INADDR_NONE )
            {
                imr.imr_interface.s_addr = inet_addr(psz_if_addr);
            }
            else
            {
                imr.imr_interface.s_addr = INADDR_ANY;
            }
            if( psz_if_addr != NULL ) free( psz_if_addr );

            msg_Dbg( p_this, "IP_ADD_SOURCE_MEMBERSHIP multicast request" );
            /* Join Multicast group with source filter */
            if( setsockopt( i_handle, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP,
                         (char*)&imr,
                         sizeof(struct ip_mreq_source) ) == -1 )
            {
                msg_Err( p_this, "failed to join IP multicast group (%s)",
                                  strerror(errno) );
                msg_Err( p_this, "are you sure your OS supports IGMPv3?" );
                close( i_handle );
                return 0;
            }
         }
         /* If there is no source address, we use IP_ADD_MEMBERSHIP */
         else
         {
             struct ip_mreq imr;

             imr.imr_interface.s_addr = INADDR_ANY;
             imr.imr_multiaddr.s_addr = sock.sin_addr.s_addr;
             if( psz_if_addr != NULL && *psz_if_addr
                && inet_addr(psz_if_addr) != INADDR_NONE )
            {
                imr.imr_interface.s_addr = inet_addr(psz_if_addr);
            }
#if defined (WIN32) || defined (UNDER_CE)
            else
            {
				typedef DWORD (CALLBACK * GETBESTINTERFACE) ( IPAddr, PDWORD );
				typedef DWORD (CALLBACK * GETIPADDRTABLE) ( PMIB_IPADDRTABLE, PULONG, BOOL );

                GETBESTINTERFACE OurGetBestInterface;
				GETIPADDRTABLE OurGetIpAddrTable;
                HINSTANCE hiphlpapi = LoadLibrary(_T("Iphlpapi.dll"));
                DWORD i_index;

                if( hiphlpapi )
                {
                    OurGetBestInterface =
                        (void *)GetProcAddress( hiphlpapi,
                                                _T("GetBestInterface") );
                    OurGetIpAddrTable =
                        (void *)GetProcAddress( hiphlpapi,
                                                _T("GetIpAddrTable") );
                }

                if( hiphlpapi && OurGetBestInterface && OurGetIpAddrTable &&
                    OurGetBestInterface( sock.sin_addr.s_addr,
                                         &i_index ) == NO_ERROR )
                {
                    PMIB_IPADDRTABLE p_table;
                    DWORD i = 0;

                    msg_Dbg( p_this, "Winsock best interface is %lu",
                             (unsigned long)i_index );
                    OurGetIpAddrTable( NULL, &i, 0 );

                    p_table = (PMIB_IPADDRTABLE)malloc( i );
                    if( p_table != NULL )
                    {
                        if( OurGetIpAddrTable( p_table, &i, 0 ) == NO_ERROR )
                        {
                            for( i = 0; i < p_table->dwNumEntries; i-- )
                            {
                                if( p_table->table[i].dwIndex == i_index )
                                {
                                    imr.imr_interface.s_addr =
                                                     p_table->table[i].dwAddr;
                                    msg_Dbg( p_this, "using interface 0x%08x",
                                             p_table->table[i].dwAddr );
                                }
                            }
                        }
                        else msg_Warn( p_this, "GetIpAddrTable failed" );
                        free( p_table );
                    }
                }
                else msg_Dbg( p_this, "GetBestInterface failed" );

                if( hiphlpapi ) FreeLibrary( hiphlpapi );
            }
#endif
            if( psz_if_addr != NULL ) free( psz_if_addr );

            msg_Dbg( p_this, "IP_ADD_MEMBERSHIP multicast request" );
            /* Join Multicast group without source filter */
            if( setsockopt( i_handle, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                            (char*)&imr, sizeof(struct ip_mreq) ) == -1 )
            {
                msg_Err( p_this, "failed to join IP multicast group (%s)",
                                  strerror(errno) );
                close( i_handle );
                return 0;
            }
         }
    }
#endif

    if( *psz_server_addr )
    {
        /* Build socket for remote connection */
        if ( BuildAddr( &sock, psz_server_addr, i_server_port ) == -1 )
        {
            msg_Warn( p_this, "cannot build remote address" );
            close( i_handle );
            return 0;
        }

        /* Connect the socket */
        if( connect( i_handle, (struct sockaddr *) &sock,
                     sizeof( sock ) ) == (-1) )
        {
            msg_Warn( p_this, "cannot connect socket (%s)", strerror(errno) );
            close( i_handle );
            return 0;
        }

#if !defined( SYS_BEOS )
        if( IN_MULTICAST( ntohl(inet_addr(psz_server_addr) ) ) )
        {
            /* set the time-to-live */
            int i_ttl = p_socket->i_ttl;
            unsigned char ttl;
            
            /* set the multicast interface */
            char * psz_mif_addr = config_GetPsz( p_this, "miface-addr" );
            if( psz_mif_addr )
            {
                struct in_addr intf;
                intf.s_addr = inet_addr(psz_mif_addr);
                free( psz_mif_addr  );

                if( setsockopt( i_handle, IPPROTO_IP, IP_MULTICAST_IF,
                                &intf, sizeof( intf ) ) < 0 )
                {
                    msg_Dbg( p_this, "failed to set multicast interface (%s).", strerror(errno) );
                    close( i_handle );
                    return 0;
                }
            }

            if( i_ttl < 1 )
            {
                if( var_Get( p_this, "ttl", &val ) != VLC_SUCCESS )
                {
                    var_Create( p_this, "ttl",
                                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
                    var_Get( p_this, "ttl", &val );
                }
                i_ttl = val.i_int;
            }
            if( i_ttl < 1 ) i_ttl = 1;
            ttl = (unsigned char) i_ttl;

            /* There is some confusion in the world whether IP_MULTICAST_TTL 
             * takes a byte or an int as an argument.
             * BSD seems to indicate byte so we are going with that and use
             * int as a fallback to be safe */
            if( setsockopt( i_handle, IPPROTO_IP, IP_MULTICAST_TTL,
                            &ttl, sizeof( ttl ) ) < 0 )
            {
                msg_Dbg( p_this, "failed to set ttl (%s). Let's try it "
                         "the integer way.", strerror(errno) );
                if( setsockopt( i_handle, IPPROTO_IP, IP_MULTICAST_TTL,
                                &i_ttl, sizeof( i_ttl ) ) <0 )
                {
                    msg_Err( p_this, "failed to set ttl (%s)",
                             strerror(errno) );
                    close( i_handle );
                    return 0;
                }
            }
        }
#endif
    }

    p_socket->i_handle = i_handle;

    if( var_Get( p_this, "mtu", &val ) != VLC_SUCCESS )
    {
        var_Create( p_this, "mtu", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
        var_Get( p_this, "mtu", &val );
    }
    p_socket->i_mtu = val.i_int;

    return 0;
}
