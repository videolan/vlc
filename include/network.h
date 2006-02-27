/*****************************************************************************
 * network.h: interface to communicate with network plug-ins
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef __VLC_NETWORK_H
# define __VLC_NETWORK_H

#if defined( WIN32 )
#   if defined(UNDER_CE) && defined(sockaddr_storage)
#       undef sockaddr_storage
#   endif
#   if defined(UNDER_CE)
#       define HAVE_STRUCT_ADDRINFO
#   else
#       include <io.h>
#   endif
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   define ENETUNREACH WSAENETUNREACH
#else
#   if HAVE_SYS_SOCKET_H
#      include <sys/socket.h>
#   endif
#   if HAVE_NETINET_IN_H
#      include <netinet/in.h>
#   endif
#   if HAVE_ARPA_INET_H
#      include <arpa/inet.h>
#   elif defined( SYS_BEOS )
#      include <net/netdb.h>
#   endif
#   include <netdb.h>
#endif

# ifdef __cplusplus
extern "C" {
# endif

/*****************************************************************************
 * network_socket_t: structure passed to a network plug-in to define the
 *                   kind of socket we want
 *****************************************************************************/
struct network_socket_t
{
    const char *psz_bind_addr;
    int i_bind_port;

    const char *psz_server_addr;
    int i_server_port;

    int i_ttl;

    int v6only;

    /* Return values */
    int i_handle;
    size_t i_mtu;
};

/* Portable networking layer communication */
#define net_ConnectTCP(a, b, c) __net_ConnectTCP(VLC_OBJECT(a), b, c)
#define net_OpenTCP(a, b, c) __net_ConnectTCP(VLC_OBJECT(a), b, c)
VLC_EXPORT( int, __net_ConnectTCP, ( vlc_object_t *p_this, const char *psz_host, int i_port ) );

#define net_ListenTCP(a, b, c) __net_ListenTCP(VLC_OBJECT(a), b, c)
VLC_EXPORT( int *, __net_ListenTCP, ( vlc_object_t *, const char *, int ) );

#define net_Accept(a, b, c) __net_Accept(VLC_OBJECT(a), b, c)
VLC_EXPORT( int, __net_Accept, ( vlc_object_t *, int *, mtime_t ) );

#define net_ConnectUDP(a, b, c, d ) __net_ConnectUDP(VLC_OBJECT(a), b, c, d)
VLC_EXPORT( int, __net_ConnectUDP, ( vlc_object_t *p_this, const char *psz_host, int i_port, int hlim ) );

#define net_OpenUDP(a, b, c, d, e ) __net_OpenUDP(VLC_OBJECT(a), b, c, d, e)
VLC_EXPORT( int, __net_OpenUDP, ( vlc_object_t *p_this, const char *psz_bind, int i_bind, const char *psz_server, int i_server ) );

VLC_EXPORT( void, net_Close, ( int fd ) );
VLC_EXPORT( void, net_ListenClose, ( int *fd ) );


/* Functions to read from or write to the networking layer */
struct virtual_socket_t
{
    void *p_sys;
    int (*pf_recv) ( void *, void *, int );
    int (*pf_send) ( void *, const void *, int );
};

#define net_Read(a,b,c,d,e,f) __net_Read(VLC_OBJECT(a),b,c,d,e,f)
VLC_EXPORT( int, __net_Read, ( vlc_object_t *p_this, int fd, v_socket_t *, uint8_t *p_data, int i_data, vlc_bool_t b_retry ) );

#define net_ReadNonBlock(a,b,c,d,e,f) __net_ReadNonBlock(VLC_OBJECT(a),b,c,d,e,f)
VLC_EXPORT( int, __net_ReadNonBlock, ( vlc_object_t *p_this, int fd, v_socket_t *, uint8_t *p_data, int i_data, mtime_t i_wait ) );

#define net_Select(a,b,c,d,e,f,g) __net_Select(VLC_OBJECT(a),b,c,d,e,f,g)
VLC_EXPORT( int, __net_Select, ( vlc_object_t *p_this, int *pi_fd, v_socket_t **, int i_fd, uint8_t *p_data, int i_data, mtime_t i_wait ) );

#define net_Write(a,b,c,d,e) __net_Write(VLC_OBJECT(a),b,c,d,e)
VLC_EXPORT( int, __net_Write, ( vlc_object_t *p_this, int fd, v_socket_t *, const uint8_t *p_data, int i_data ) );

#define net_Gets(a,b,c) __net_Gets(VLC_OBJECT(a),b,c)
VLC_EXPORT( char *, __net_Gets, ( vlc_object_t *p_this, int fd, v_socket_t * ) );

VLC_EXPORT( int, net_Printf, ( vlc_object_t *p_this, int fd, v_socket_t *, const char *psz_fmt, ... ) );

#define net_vaPrintf(a,b,c,d,e) __net_vaPrintf(VLC_OBJECT(a),b,c,d,e)
VLC_EXPORT( int, __net_vaPrintf, ( vlc_object_t *p_this, int fd, v_socket_t *, const char *psz_fmt, va_list args ) );


#if !HAVE_INET_PTON
/* only in core, so no need for C++ extern "C" */
int inet_pton(int af, const char *src, void *dst);
#endif


/*****************************************************************************
 * net_StopRecv/Send
 *****************************************************************************
 * Wrappers for shutdown()
 *****************************************************************************/
#if defined (SHUT_WR)
/* the standard way */
# define net_StopSend( fd ) (void)shutdown( fd, SHUT_WR )
# define net_StopRecv( fd ) (void)shutdown( fd, SHUT_RD )
#elif defined (SD_SEND)
/* the Microsoft seemingly-purposedly-different-for-the-sake-of-it way */
# define net_StopSend( fd ) (void)shutdown( fd, SD_SEND )
# define net_StopRecv( fd ) (void)shutdown( fd, SD_RECEIVE )
#else
# ifndef SYS_BEOS /* R5 just doesn't have a working shutdown() */
#  warning FIXME: implement shutdown on your platform!
# endif
# define net_StopSend( fd ) (void)0
# define net_StopRecv( fd ) (void)0
#endif

/* Portable network names/addresses resolution layer */

/* GAI error codes */
# ifndef EAI_BADFLAGS
#  define EAI_BADFLAGS -1
# endif
# ifndef EAI_NONAME
#  define EAI_NONAME -2
# endif
# ifndef EAI_AGAIN
#  define EAI_AGAIN -3
# endif
# ifndef EAI_FAIL
#  define EAI_FAIL -4
# endif
# ifndef EAI_NODATA
#  define EAI_NODATA -5
# endif
# ifndef EAI_FAMILY
#  define EAI_FAMILY -6
# endif
# ifndef EAI_SOCKTYPE
#  define EAI_SOCKTYPE -7
# endif
# ifndef EAI_SERVICE
#  define EAI_SERVICE -8
# endif
# ifndef EAI_ADDRFAMILY
#  define EAI_ADDRFAMILY -9
# endif
# ifndef EAI_MEMORY
#  define EAI_MEMORY -10
# endif
# ifndef EAI_SYSTEM
#  define EAI_SYSTEM -11
# endif


# ifndef NI_MAXHOST
#  define NI_MAXHOST 1025
#  define NI_MAXSERV 32
# endif
# define NI_MAXNUMERICHOST 64

# ifndef NI_NUMERICHOST
#  define NI_NUMERICHOST 0x01
#  define NI_NUMERICSERV 0x02
#  define NI_NOFQDN      0x04
#  define NI_NAMEREQD    0x08
#  define NI_DGRAM       0x10
# endif

# ifndef HAVE_STRUCT_ADDRINFO
struct addrinfo
{
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    size_t ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;
    struct addrinfo *ai_next;
};
#  define AI_PASSIVE     1
#  define AI_CANONNAME   2
#  define AI_NUMERICHOST 4
# endif /* if !HAVE_STRUCT_ADDRINFO */

VLC_EXPORT( const char *, vlc_gai_strerror, ( int ) );
VLC_EXPORT( int, vlc_getnameinfo, ( const struct sockaddr *, int, char *, int, int *, int ) );
VLC_EXPORT( int, vlc_getaddrinfo, ( vlc_object_t *, const char *, int, const struct addrinfo *, struct addrinfo ** ) );
VLC_EXPORT( void, vlc_freeaddrinfo, ( struct addrinfo * ) );

/*****************************************************************************
 * net_AddressIsMulticast: This function returns VLC_FALSE if the psz_addr does
 * not specify a multicast address or if the address is not a valid address.
 *****************************************************************************/
static inline vlc_bool_t net_AddressIsMulticast( vlc_object_t *p_object, const char *psz_addr )
{
    struct addrinfo hints, *res;
    vlc_bool_t b_multicast = VLC_FALSE;
    int i;

    memset( &hints, 0, sizeof( hints ) );
    hints.ai_socktype = SOCK_DGRAM; /* UDP */
    hints.ai_flags = AI_NUMERICHOST;

    i = vlc_getaddrinfo( p_object, psz_addr, 0,
                         &hints, &res );
    if( i )
    {
        msg_Err( p_object, "Invalid node for net_AddressIsMulticast: %s : %s",
                 psz_addr, vlc_gai_strerror( i ) );
        return VLC_FALSE;
    }

    if( res->ai_family == AF_INET )
    {
#if !defined( SYS_BEOS )
        struct sockaddr_in *v4 = (struct sockaddr_in *) res->ai_addr;
        b_multicast = ( ntohl( v4->sin_addr.s_addr ) >= 0xe0000000 )
                   && ( ntohl( v4->sin_addr.s_addr ) <= 0xefffffff );
#endif
    }
#if defined( WIN32 ) || defined( HAVE_GETADDRINFO )
    else if( res->ai_family == AF_INET6 )
    {
        struct sockaddr_in6 *v6 = (struct sockaddr_in6 *)res->ai_addr;
        b_multicast = IN6_IS_ADDR_MULTICAST( &v6->sin6_addr );
    }
#endif

    vlc_freeaddrinfo( res );
    return b_multicast;
}

static inline int net_GetSockAddress( int fd, char *address, int *port )
{
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof( addr );

    return getsockname( fd, (struct sockaddr *)&addr, &addrlen )
        || vlc_getnameinfo( (struct sockaddr *)&addr, addrlen, address,
                            NI_MAXNUMERICHOST, port, NI_NUMERICHOST )
        ? VLC_EGENERIC : 0;
}

static inline int net_GetPeerAddress( int fd, char *address, int *port )
{
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof( addr );

    return getpeername( fd, (struct sockaddr *)&addr, &addrlen )
        || vlc_getnameinfo( (struct sockaddr *)&addr, addrlen, address,
                            NI_MAXNUMERICHOST, port, NI_NUMERICHOST )
        ? VLC_EGENERIC : 0;
}

# ifdef __cplusplus
}
# endif

#endif
