/*****************************************************************************
 * vlc_network.h: interface to communicate with network plug-ins
 *****************************************************************************
 * Copyright (C) 2002-2005 VLC authors and VideoLAN
 * Copyright © 2006-2007 Rémi Denis-Courmont
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_NETWORK_H
# define VLC_NETWORK_H

/**
 * \ingroup os
 * \defgroup net Networking
 * @{
 * \file
 * Definitions for sockets and low-level networking
 * \defgroup sockets Internet sockets
 * @{
 */

#include <sys/types.h>
#include <unistd.h>

#if defined( _WIN32 )
#   define _NO_OLDNAMES 1
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   define net_errno (WSAGetLastError())
#   define net_Close(fd) ((void)closesocket((SOCKET)fd))
#   ifndef IPV6_V6ONLY
#       define IPV6_V6ONLY 27
#   endif
#else
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <netdb.h>
#   define net_errno errno
#   define net_Close(fd) ((void)vlc_close(fd))
#endif

#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL 0
#endif

/**
 * Creates a socket file descriptor.
 *
 * This function creates a socket, similar to the standard socket() function.
 * However, the new file descriptor has the close-on-exec flag set atomically,
 * so as to avoid leaking the descriptor to child processes.
 *
 * The non-blocking flag can also optionally be set.
 *
 * @param pf protocol family
 * @param type socket type
 * @param proto network protocol
 * @param nonblock true to create a non-blocking socket
 * @return a new file descriptor or -1 on error
 */
VLC_API int vlc_socket(int pf, int type, int proto, bool nonblock) VLC_USED;

/**
 * Creates a pair of socket file descriptors.
 *
 * This function creates a pair of sockets that are mutually connected,
 * much like the standard socketpair() function. However, the new file
 * descriptors have the close-on-exec flag set atomically.
 * See also vlc_socket().
 *
 * @param pf protocol family
 * @param type socket type
 * @param proto network protocol
 * @param nonblock true to create non-blocking sockets
 * @retval 0 on success
 * @retval -1 on failure
 */
VLC_API int vlc_socketpair(int pf, int type, int proto, int fds[2],
                           bool nonblock);

struct sockaddr;

/**
 * Accepts an inbound connection request on a listening socket.
 *
 * This function creates a connected socket from a listening socket, much like
 * the standard accept() function. However, the new file descriptor has the
 * close-on-exec flag set atomically. See also vlc_socket().
 *
 * @param lfd listening socket file descriptor
 * @param addr pointer to the peer address or NULL [OUT]
 * @param alen pointer to the length of the peer address or NULL [OUT]
 * @param nonblock whether to put the new socket in non-blocking mode
 * @return a new file descriptor or -1 on error
 */
VLC_API int vlc_accept(int lfd, struct sockaddr *addr, socklen_t *alen,
                       bool nonblock) VLC_USED;

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

VLC_API ssize_t net_Read( vlc_object_t *p_this, int fd, void *p_data, size_t i_data );
#define net_Read(a,b,c,d) net_Read(VLC_OBJECT(a),b,c,d)
VLC_API ssize_t net_Write( vlc_object_t *p_this, int fd, const void *p_data, size_t i_data );
#define net_Write(a,b,c,d) net_Write(VLC_OBJECT(a),b,c,d)
VLC_API char * net_Gets( vlc_object_t *p_this, int fd );
#define net_Gets(a,b) net_Gets(VLC_OBJECT(a),b)


VLC_API ssize_t net_Printf( vlc_object_t *p_this, int fd, const char *psz_fmt, ... ) VLC_FORMAT( 3, 4 );
#define net_Printf(o,fd,...) net_Printf(VLC_OBJECT(o),fd, __VA_ARGS__)
VLC_API ssize_t net_vaPrintf( vlc_object_t *p_this, int fd, const char *psz_fmt, va_list args );
#define net_vaPrintf(a,b,c,d) net_vaPrintf(VLC_OBJECT(a),b,c,d)

VLC_API int vlc_close(int);

/** @} */

#ifdef _WIN32
static inline int vlc_getsockopt(int s, int level, int name,
                                 void *val, socklen_t *len)
{
    return getsockopt(s, level, name, (char *)val, len);
}
#define getsockopt vlc_getsockopt

static inline int vlc_setsockopt(int s, int level, int name,
                                 const void *val, socklen_t len)
{
    return setsockopt(s, level, name, (const char *)val, len);
}
#define setsockopt vlc_setsockopt
#endif

/* Portable network names/addresses resolution layer */

#define NI_MAXNUMERICHOST 64

#ifndef AI_NUMERICSERV
# define AI_NUMERICSERV 0
#endif
#ifndef AI_IDN
# define AI_IDN 0 /* GNU/libc extension */
#endif

#ifdef _WIN32
# if !defined(WINAPI_FAMILY) || WINAPI_FAMILY != WINAPI_FAMILY_APP
#  undef gai_strerror
#  define gai_strerror gai_strerrorA
# endif
#endif

VLC_API int vlc_getnameinfo( const struct sockaddr *, int, char *, int, int *, int );
VLC_API int vlc_getaddrinfo (const char *, unsigned,
                             const struct addrinfo *, struct addrinfo **);
VLC_API int vlc_getaddrinfo_i11e(const char *, unsigned,
                                 const struct addrinfo *, struct addrinfo **);

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

VLC_API char *vlc_getProxyUrl(const char *);

# ifdef __cplusplus
}
# endif

/** @} */

#endif
