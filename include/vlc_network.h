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

#ifndef VLC_NETWORK_H
# define VLC_NETWORK_H

/**
 * \file
 * This file defines interface to communicate with network plug-ins
 */

#if defined( WIN32 )
#   if !defined(UNDER_CE)
#       define _NO_OLDNAMES 1
#       include <io.h>
#   endif
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   define ENETUNREACH WSAENETUNREACH
#   define net_errno (WSAGetLastError())
extern const char *net_strerror( int val );

struct iovec
{
    void  *iov_base;
    size_t iov_len;
};

struct msghdr
{
    void         *msg_name;
    size_t        msg_namelen;
    struct iovec *msg_iov;
    size_t        msg_iovlen;
    void         *msg_control;
    size_t        msg_controllen;
    int           msg_flags;
};

#   ifndef IPV6_V6ONLY
#       define IPV6_V6ONLY 27
#   endif
#else
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <netdb.h>
#   define net_errno errno
#endif

#if defined( __SYMBIAN32__ )
#   undef AF_INET6
#   undef IN6_IS_ADDR_MULTICAST
#   undef IPV6_V6ONLY
#   undef IPV6_MULTICAST_HOPS
#   undef IPV6_MULTICAST_IF
#   undef IPV6_TCLASS
#   undef IPV6_JOIN_GROUP
#endif

int vlc_socket (int, int, int, bool nonblock) VLC_USED;

struct sockaddr;
VLC_API int vlc_accept( int, struct sockaddr *, socklen_t *, bool ) VLC_USED;

# ifdef __cplusplus
extern "C" {
# endif

/* Portable networking layer communication */
int net_Socket (vlc_object_t *obj, int family, int socktype, int proto);

VLC_API int net_Connect(vlc_object_t *p_this, const char *psz_host, int i_port, int socktype, int protocol);
#define net_Connect(a, b, c, d, e) net_Connect(VLC_OBJECT(a), b, c, d, e)

VLC_API int * net_Listen(vlc_object_t *p_this, const char *psz_host, int i_port, int socktype, int protocol);

#define net_ListenTCP(a, b, c) net_Listen(VLC_OBJECT(a), b, c, \
                                          SOCK_STREAM, IPPROTO_TCP)

static inline int net_ConnectTCP (vlc_object_t *obj, const char *host, int port)
{
    return net_Connect (obj, host, port, SOCK_STREAM, IPPROTO_TCP);
}
#define net_ConnectTCP(a, b, c) net_ConnectTCP(VLC_OBJECT(a), b, c)

VLC_API int net_AcceptSingle(vlc_object_t *obj, int lfd);

VLC_API int net_Accept( vlc_object_t *, int * );
#define net_Accept(a, b) \
        net_Accept(VLC_OBJECT(a), b)

VLC_API int net_ConnectDgram( vlc_object_t *p_this, const char *psz_host, int i_port, int hlim, int proto );
#define net_ConnectDgram(a, b, c, d, e ) \
        net_ConnectDgram(VLC_OBJECT(a), b, c, d, e)

static inline int net_ConnectUDP (vlc_object_t *obj, const char *host, int port, int hlim)
{
    return net_ConnectDgram (obj, host, port, hlim, IPPROTO_UDP);
}

VLC_API int net_OpenDgram( vlc_object_t *p_this, const char *psz_bind, int i_bind, const char *psz_server, int i_server, int proto );
#define net_OpenDgram( a, b, c, d, e, g ) \
        net_OpenDgram(VLC_OBJECT(a), b, c, d, e, g)

static inline int net_ListenUDP1 (vlc_object_t *obj, const char *host, int port)
{
    return net_OpenDgram (obj, host, port, NULL, 0, IPPROTO_UDP);
}

VLC_API void net_ListenClose( int *fd );

int net_Subscribe (vlc_object_t *obj, int fd, const struct sockaddr *addr,
                   socklen_t addrlen);

VLC_API int net_SetCSCov( int fd, int sendcov, int recvcov );

/* Functions to read from or write to the networking layer */
struct virtual_socket_t
{
    void *p_sys;
    int (*pf_recv) ( void *, void *, size_t );
    int (*pf_send) ( void *, const void *, size_t );
};

VLC_API ssize_t net_Read( vlc_object_t *p_this, int fd, const v_socket_t *, void *p_data, size_t i_data, bool b_retry );
#define net_Read(a,b,c,d,e,f) net_Read(VLC_OBJECT(a),b,c,d,e,f)
VLC_API ssize_t net_Write( vlc_object_t *p_this, int fd, const v_socket_t *, const void *p_data, size_t i_data );
#define net_Write(a,b,c,d,e) net_Write(VLC_OBJECT(a),b,c,d,e)
VLC_API char * net_Gets( vlc_object_t *p_this, int fd, const v_socket_t * );
#define net_Gets(a,b,c) net_Gets(VLC_OBJECT(a),b,c)


VLC_API ssize_t net_Printf( vlc_object_t *p_this, int fd, const v_socket_t *, const char *psz_fmt, ... ) VLC_FORMAT( 4, 5 );
#define net_Printf(o,fd,vs,...) net_Printf(VLC_OBJECT(o),fd,vs, __VA_ARGS__)
VLC_API ssize_t net_vaPrintf( vlc_object_t *p_this, int fd, const v_socket_t *, const char *psz_fmt, va_list args );
#define net_vaPrintf(a,b,c,d,e) net_vaPrintf(VLC_OBJECT(a),b,c,d,e)

struct pollfd;
VLC_API int vlc_poll(struct pollfd *fds, unsigned nfds, int timeout);


#ifdef WIN32
/* Microsoft: same semantic, same value, different name... go figure */
# define SHUT_RD SD_RECEIVE
# define SHUT_WR SD_SEND
# define SHUT_RDWR SD_BOTH
# define net_Close( fd ) closesocket ((SOCKET)fd)
#else
# ifdef __OS2__
#  define SHUT_RD    0
#  define SHUT_WR    1
#  define SHUT_RDWR  2
# endif
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

#ifndef AI_NUMERICSERV
# define AI_NUMERICSERV 0
#endif

VLC_API int vlc_getnameinfo( const struct sockaddr *, int, char *, int, int *, int );
VLC_API int vlc_getaddrinfo( vlc_object_t *, const char *, int, const struct addrinfo *, struct addrinfo ** );


static inline bool
net_SockAddrIsMulticast (const struct sockaddr *addr, socklen_t len)
{
    switch (addr->sa_family)
    {
#ifdef IN_MULTICAST
        case AF_INET:
        {
            const struct sockaddr_in *v4 = (const struct sockaddr_in *)addr;
            if ((size_t)len < sizeof (*v4))
                return false;
            return IN_MULTICAST (ntohl (v4->sin_addr.s_addr)) != 0;
        }
#endif

#ifdef IN6_IS_ADDR_MULTICAST
        case AF_INET6:
        {
            const struct sockaddr_in6 *v6 = (const struct sockaddr_in6 *)addr;
            if ((size_t)len < sizeof (*v6))
                return false;
            return IN6_IS_ADDR_MULTICAST (&v6->sin6_addr) != 0;
        }
#endif
    }

    return false;
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
