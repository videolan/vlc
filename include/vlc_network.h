/*****************************************************************************
 * vlc_network.h: interface to communicate with network plug-ins
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * Copyright © 2006-2007 Rémi Denis-Courmont
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

#if !defined( __LIBVLC__ )
  #error You are not libvlc or one of its plugins. You cannot include this file
#endif

#ifndef __VLC_NETWORK_H
# define __VLC_NETWORK_H

#if defined( WIN32 )
#   if defined(UNDER_CE) && defined(sockaddr_storage)
#       undef sockaddr_storage
#   endif
#   if defined(UNDER_CE)
#       define HAVE_STRUCT_ADDRINFO
#   else
#       define _NO_OLDNAMES 1
#       include <io.h>
#   endif
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   define ENETUNREACH WSAENETUNREACH
#   define net_errno (WSAGetLastError())
extern const char *net_strerror( int val );
#   ifndef IPV6_V6ONLY
#       define IPV6_V6ONLY 27
#   endif
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
#   define net_errno errno
#endif

# ifdef __cplusplus
extern "C" {
# endif

/* Portable networking layer communication */
int net_Socket (vlc_object_t *obj, int family, int socktype, int proto);
int net_SetupSocket (int fd);

#define net_Connect(a, b, c, d, e) __net_Connect(VLC_OBJECT(a), b, c, d, e)
VLC_EXPORT( int, __net_Connect, (vlc_object_t *p_this, const char *psz_host, int i_port, int socktype, int protocol) );

VLC_EXPORT( int *, net_Listen, (vlc_object_t *p_this, const char *psz_host, int i_port, int protocol) );

#define net_ListenTCP(a, b, c) net_Listen(VLC_OBJECT(a), b, c, IPPROTO_TCP)
#define net_ConnectTCP(a, b, c) __net_ConnectTCP(VLC_OBJECT(a), b, c)

static inline int __net_ConnectTCP (vlc_object_t *obj, const char *host, int port)
{
    return __net_Connect (obj, host, port, SOCK_STREAM, IPPROTO_TCP);
}

#define net_Accept(a, b, c) __net_Accept(VLC_OBJECT(a), b, c)
VLC_EXPORT( int, __net_Accept, ( vlc_object_t *, int *, mtime_t ) );

#define net_ConnectDgram(a, b, c, d, e ) __net_ConnectDgram(VLC_OBJECT(a), b, c, d, e)
VLC_EXPORT( int, __net_ConnectDgram, ( vlc_object_t *p_this, const char *psz_host, int i_port, int hlim, int proto ) );

static inline int net_ConnectUDP (vlc_object_t *obj, const char *host, int port, int hlim)
{
    return net_ConnectDgram (obj, host, port, hlim, IPPROTO_UDP);
}

#define net_OpenDgram( a, b, c, d, e, g, h ) __net_OpenDgram(VLC_OBJECT(a), b, c, d, e, g, h)
VLC_EXPORT( int, __net_OpenDgram, ( vlc_object_t *p_this, const char *psz_bind, int i_bind, const char *psz_server, int i_server, int family, int proto ) );

static inline int net_ListenUDP1 (vlc_object_t *obj, const char *host, int port)
{
    return net_OpenDgram (obj, host, port, NULL, 0, 0, IPPROTO_UDP);
}

VLC_EXPORT( void, net_ListenClose, ( int *fd ) );

int net_Subscribe (vlc_object_t *obj, int fd, const struct sockaddr *addr,
                   socklen_t addrlen);

VLC_EXPORT( int, net_SetCSCov, ( int fd, int sendcov, int recvcov ) );

/* Functions to read from or write to the networking layer */
struct virtual_socket_t
{
    void *p_sys;
    int (*pf_recv) ( void *, void *, int );
    int (*pf_send) ( void *, const void *, int );
};

#define net_Read(a,b,c,d,e,f) __net_Read(VLC_OBJECT(a),b,c,d,e,f)
VLC_EXPORT( ssize_t, __net_Read, ( vlc_object_t *p_this, int fd, const v_socket_t *, uint8_t *p_data, size_t i_data, vlc_bool_t b_retry ) );

#define net_Select(a,b,c,d,e,f) __net_Select(VLC_OBJECT(a),b,c,d,e,f)
VLC_EXPORT( ssize_t, __net_Select, ( vlc_object_t *p_this, const int *pi_fd, int i_fd, uint8_t *p_data, size_t i_data, int i_timeout ) );

#define net_Write(a,b,c,d,e) __net_Write(VLC_OBJECT(a),b,c,d,e)
VLC_EXPORT( ssize_t, __net_Write, ( vlc_object_t *p_this, int fd, const v_socket_t *, const uint8_t *p_data, size_t i_data ) );

#define net_Gets(a,b,c) __net_Gets(VLC_OBJECT(a),b,c)
VLC_EXPORT( char *, __net_Gets, ( vlc_object_t *p_this, int fd, const v_socket_t * ) );

VLC_EXPORT( ssize_t, net_Printf, ( vlc_object_t *p_this, int fd, const v_socket_t *, const char *psz_fmt, ... ) ATTRIBUTE_FORMAT( 4, 5 ) );

#define net_vaPrintf(a,b,c,d,e) __net_vaPrintf(VLC_OBJECT(a),b,c,d,e)
VLC_EXPORT( ssize_t, __net_vaPrintf, ( vlc_object_t *p_this, int fd, const v_socket_t *, const char *psz_fmt, va_list args ) );


#ifndef HAVE_INET_PTON
/* only in core, so no need for C++ extern "C" */
    VLC_EXPORT (int, inet_pton, (int af, const char *src, void *dst) );
#endif

#ifndef HAVE_INET_NTOP
#ifdef WIN32
/* only in core, so no need for C++ extern "C" */
    VLC_EXPORT (const char *, inet_ntop, (int af, const void *src,
                                          char *dst, socklen_t cnt) );
#endif
#endif

#ifndef HAVE_POLL
enum
{
    POLLIN=1,
    POLLOUT=2,
    POLLPRI=4,
    POLLERR=8,  // unsupported stub
    POLLHUP=16, // unsupported stub
    POLLNVAL=32 // unsupported stub
};

struct pollfd
{
    int fd;
    int events;
    int revents;
};

int poll (struct pollfd *fds, unsigned nfds, int timeout);
#endif

#ifdef WIN32
/* Microsoft: same semantic, same value, different name... go figure */
# define SHUT_RD SD_RECEIVE
# define SHUT_WR SD_SEND
# define SHUT_RDWR SD_BOTH
# define net_Close( fd ) closesocket ((SOCKET)fd)
#else
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
# define net_Close( fd ) (void)close (fd)
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
#ifndef EAI_OVERFLOW
#  define EAI_OVERFLOW -11
#endif
# ifndef EAI_SYSTEM
#  define EAI_SYSTEM -12
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


static inline vlc_bool_t
net_SockAddrIsMulticast (const struct sockaddr *addr, socklen_t len)
{
    switch (addr->sa_family)
    {
#ifdef IN_MULTICAST
        case AF_INET:
        {
            const struct sockaddr_in *v4 = (const struct sockaddr_in *)addr;
            if ((size_t)len < sizeof (*v4))
                return VLC_FALSE;
            return IN_MULTICAST (ntohl (v4->sin_addr.s_addr)) != 0;
        }
#endif

#ifdef IN6_IS_ADDR_MULTICAST
        case AF_INET6:
        {
            const struct sockaddr_in6 *v6 = (const struct sockaddr_in6 *)addr;
            if ((size_t)len < sizeof (*v6))
                return VLC_FALSE;
            return IN6_IS_ADDR_MULTICAST (&v6->sin6_addr) != 0;
        }
#endif
    }

    return VLC_FALSE;
}



/**
 * net_AddressIsMulticast
 * @return VLC_FALSE iff the psz_addr does not specify a multicast address,
 * or the address is not a valid address.
 */
static inline vlc_bool_t net_AddressIsMulticast( vlc_object_t *p_object, const char *psz_addr )
{
    struct addrinfo hints, *res;

    memset (&hints, 0, sizeof (hints));
    hints.ai_socktype = SOCK_DGRAM; /* UDP */
    hints.ai_flags = AI_NUMERICHOST;

    int i = vlc_getaddrinfo (p_object, psz_addr, 0,
                             &hints, &res);
    if (i)
    {
        msg_Err (p_object, "invalid address \"%s\" for net_AddressIsMulticast (%s)",
                 psz_addr, vlc_gai_strerror (i));
        return VLC_FALSE;
    }

    vlc_bool_t b = net_SockAddrIsMulticast (res->ai_addr, res->ai_addrlen);
    vlc_freeaddrinfo (res);
    return b;
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

static inline uint16_t net_GetPort (const struct sockaddr *addr)
{
    switch (addr->sa_family)
    {
#ifdef AF_INET6
        case AF_INET6:
            return ((const struct sockaddr_in6 *)addr)->sin6_port;
#endif
        case AF_INET:
            return ((const struct sockaddr_in *)addr)->sin_port;
    }
    return 0;
}

static inline void net_SetPort (struct sockaddr *addr, uint16_t port)
{
    switch (addr->sa_family)
    {
#ifdef AF_INET6
        case AF_INET6:
            ((struct sockaddr_in6 *)addr)->sin6_port = port;
        break;
#endif
        case AF_INET:
            ((struct sockaddr_in *)addr)->sin_port = port;
        break;
    }
}
# ifdef __cplusplus
}
# endif

#endif
