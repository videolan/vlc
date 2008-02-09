/*****************************************************************************
 * io.c: network I/O functions
 *****************************************************************************
 * Copyright (C) 2004-2005, 2007 the VideoLAN team
 * Copyright © 2005-2006 Rémi Denis-Courmont
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@videolan.org>
 *          Rémi Denis-Courmont <rem # videolan.org>
 *          Christophe Mutricy <xtophe at videolan dot org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include <errno.h>
#include <assert.h>

#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif
#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif
#ifdef HAVE_POLL
#   include <poll.h>
#endif

#include <vlc_network.h>

#ifndef INADDR_ANY
#   define INADDR_ANY  0x00000000
#endif
#ifndef INADDR_NONE
#   define INADDR_NONE 0xFFFFFFFF
#endif

#if defined(WIN32) || defined(UNDER_CE)
# undef EAFNOSUPPORT
# define EAFNOSUPPORT WSAEAFNOSUPPORT
#endif

extern int rootwrap_bind (int family, int socktype, int protocol,
                          const struct sockaddr *addr, size_t alen);

int net_SetupSocket (int fd)
{
#if defined (WIN32) || defined (UNDER_CE)
    ioctlsocket (fd, FIONBIO, &(unsigned long){ 1 });
#else
    fcntl (fd, F_SETFD, FD_CLOEXEC);
    fcntl (fd, F_SETFL, fcntl (fd, F_GETFL, 0) | O_NONBLOCK);
#endif

    setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof (int));
    return 0;
}


int net_Socket (vlc_object_t *p_this, int family, int socktype,
                int protocol)
{
    int fd = socket (family, socktype, protocol);
    if (fd == -1)
    {
        if (net_errno != EAFNOSUPPORT)
            msg_Err (p_this, "cannot create socket: %m");
        return -1;
    }

    net_SetupSocket (fd);

#ifdef IPV6_V6ONLY
    /*
     * Accepts only IPv6 connections on IPv6 sockets.
     * If possible, we should open two sockets, but it is not always possible.
     */
    if (family == AF_INET6)
        setsockopt (fd, IPPROTO_IPV6, IPV6_V6ONLY, &(int){ 1 }, sizeof (int));
#endif

#if defined (WIN32) || defined (UNDER_CE)
# ifndef IPV6_PROTECTION_LEVEL
#  warning Please update your C library headers.
#  define IPV6_PROTECTION_LEVEL 23
#  define PROTECTION_LEVEL_UNRESTRICTED 10
# endif
    if (family == AF_INET6)
        setsockopt (fd, IPPROTO_IPV6, IPV6_PROTECTION_LEVEL,
                    &(int){ PROTECTION_LEVEL_UNRESTRICTED }, sizeof (int));
#endif

    return fd;
}


int *net_Listen (vlc_object_t *p_this, const char *psz_host,
                 int i_port, int protocol)
{
    struct addrinfo hints, *res;
    int socktype = SOCK_DGRAM;

    switch( protocol )
    {
        case IPPROTO_TCP:
            socktype = SOCK_STREAM;
            break;
        case 33: /* DCCP */
#ifdef __linux__
# ifndef SOCK_DCCP
#  define SOCK_DCCP 6
# endif
            socktype = SOCK_DCCP;
#endif
            break;
    }

    memset (&hints, 0, sizeof( hints ));
    /* Since we use port numbers rather than service names, the socket type
     * does not really matter. */
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    msg_Dbg (p_this, "net: listening to %s port %d", psz_host, i_port);

    int i_val = vlc_getaddrinfo (p_this, psz_host, i_port, &hints, &res);
    if (i_val)
    {
        msg_Err (p_this, "Cannot resolve %s port %d : %s", psz_host, i_port,
                 vlc_gai_strerror (i_val));
        return NULL;
    }

    int *sockv = NULL;
    unsigned sockc = 0;

    for (struct addrinfo *ptr = res; ptr != NULL; ptr = ptr->ai_next)
    {
        int fd = net_Socket (p_this, ptr->ai_family, socktype, protocol);
        if (fd == -1)
        {
            msg_Dbg (p_this, "socket error: %m");
            continue;
        }

        /* Bind the socket */
#if defined (WIN32) || defined (UNDER_CE)
        /*
         * Under Win32 and for multicasting, we bind to INADDR_ANY.
         * This is of course a severe bug, since the socket would logically
         * receive unicast traffic, and multicast traffic of groups subscribed
         * to via other sockets.
         */
        if (net_SockAddrIsMulticast (ptr->ai_addr, ptr->ai_addrlen)
         && (sizeof (struct sockaddr_storage) >= ptr->ai_addrlen))
        {
            // This works for IPv4 too - don't worry!
            struct sockaddr_in6 dumb =
            {
                .sin6_family = ptr->ai_addr->sa_family,
                .sin6_port =  ((struct sockaddr_in *)(ptr->ai_addr))->sin_port
            };

            bind (fd, (struct sockaddr *)&dumb, ptr->ai_addrlen);
        }
        else
#endif
        if (bind (fd, ptr->ai_addr, ptr->ai_addrlen))
        {
            net_Close (fd);
#if !defined(WIN32) && !defined(UNDER_CE)
            fd = rootwrap_bind (ptr->ai_family, ptr->ai_socktype,
                                protocol ?: ptr->ai_protocol, ptr->ai_addr,
                                ptr->ai_addrlen);
            if (fd != -1)
            {
                msg_Dbg (p_this, "got socket %d from rootwrap", fd);
            }
            else
#endif
            {
                msg_Err (p_this, "socket bind error (%m)");
                continue;
            }
        }

        if (net_SockAddrIsMulticast (ptr->ai_addr, ptr->ai_addrlen))
        {
            if (net_Subscribe (p_this, fd, ptr->ai_addr, ptr->ai_addrlen))
            {
                net_Close (fd);
                continue;
            }
        }

        /* Listen */
        switch (socktype)
        {
            case SOCK_STREAM:
            case SOCK_RDM:
            case SOCK_SEQPACKET:
#ifdef SOCK_DCCP
            case SOCK_DCCP:
#endif
                if (listen (fd, INT_MAX))
                {
                    msg_Err (p_this, "socket listen error (%m)");
                    net_Close (fd);
                    continue;
                }
        }

        int *nsockv = (int *)realloc (sockv, (sockc + 2) * sizeof (int));
        if (nsockv != NULL)
        {
            nsockv[sockc++] = fd;
            sockv = nsockv;
        }
        else
            net_Close (fd);
    }

    vlc_freeaddrinfo (res);

    if (sockv != NULL)
        sockv[sockc] = -1;

    return sockv;
}


/*****************************************************************************
 * __net_Read:
 *****************************************************************************
 * Reads from a network socket.
 * If waitall is true, then we repeat until we have read the right amount of
 * data; in that case, a short count means EOF has been reached or the VLC
 * object has been signaled.
 *****************************************************************************/
ssize_t
__net_Read (vlc_object_t *restrict p_this, int fd, const v_socket_t *vs,
            uint8_t *restrict p_buf, size_t i_buflen, vlc_bool_t waitall)
{
    size_t i_total = 0;
    struct pollfd ufd[2] = {
        { .fd = fd,                           .events = POLLIN },
        { .fd = vlc_object_waitpipe (p_this), .events = POLLIN },
    };

    if (ufd[1].fd == -1)
        return -1; /* vlc_object_waitpipe() sets errno */

    while (i_buflen > 0)
    {
        int val;

        ufd[0].revents = ufd[1].revents = 0;

        val = poll (ufd, sizeof (ufd) / sizeof (ufd[0]), -1);

        if (val < 0)
            goto error;

        if (ufd[1].revents)
        {
            msg_Dbg (p_this, "socket %d polling interrupted", fd);
            if (i_total == 0)
            {
#if defined(WIN32) || defined(UNDER_CE)
                WSASetLastError (WSAEINTR);
#else
                errno = EINTR;
#endif
                goto error;
            }
            break;
        }

        assert (ufd[0].revents);

#ifndef POLLRDHUP /* This is nice but non-portable */
# define POLLRDHUP 0
#endif
        if (i_total > 0)
        {
            /* Errors (-1) and EOF (0) will be returned on next call,
             * otherwise we'd "hide" the error from the caller, which is a
             * bad idea™. */
            if (ufd[0].revents & (POLLERR|POLLNVAL|POLLRDHUP))
                break;
        }

        ssize_t n;
        if (vs != NULL)
        {
            n = vs->pf_recv (vs->p_sys, p_buf, i_buflen);
        }
        else
        {
#ifdef WIN32
            n = recv (fd, p_buf, i_buflen, 0);
#else
            n = read (fd, p_buf, i_buflen);
#endif
        }

        if (n == -1)
        {
#if defined(WIN32) || defined(UNDER_CE)
            switch (WSAGetLastError ())
            {
                case WSAEWOULDBLOCK:
                /* only happens with vs != NULL (TLS) - not really an error */
                    continue;

                case WSAEMSGSIZE:
                /* For UDP only */
                /* On Win32, recv() fails if the datagram doesn't fit inside
                 * the passed buffer, even though the buffer will be filled
                 * with the first part of the datagram. */
                    msg_Err (p_this, "Receive error: "
                                     "Increase the mtu size (--mtu option)");
                    n = i_buflen;
                    break;

                default:
                    goto error;
            }
#else
            /* spurious wake-up or TLS did not yield any actual data */
            if (errno == EAGAIN)
                continue;
            goto error;
#endif
        }

        if (n == 0)
            /* For streams, this means end of file, and there will not be any
             * further data ever on the stream. For datagram sockets, this
             * means empty datagram, and there could be more data coming.
             * However, it makes no sense to set <waitall> with datagrams in the
             * first place.
             */
            break; // EOF

        i_total += n;
        p_buf += n;
        i_buflen -= n;

        if (!waitall)
            break;
    }

    return i_total;

error:
    msg_Err (p_this, "Read error: %m");
    return -1;
}


/* Write exact amount requested */
ssize_t __net_Write( vlc_object_t *p_this, int fd, const v_socket_t *p_vs,
                     const uint8_t *p_data, size_t i_data )
{
    size_t i_total = 0;

    while( i_data > 0 )
    {
        if( p_this->b_die )
            break;

        struct pollfd ufd[1];
        memset (ufd, 0, sizeof (ufd));
        ufd[0].fd = fd;
        ufd[0].events = POLLOUT;

        int val = poll (ufd, 1, 500);
        switch (val)
        {
            case -1:
               msg_Err (p_this, "Write error: %m");
               goto out;

            case 0:
                continue;
        }

        if ((ufd[0].revents & (POLLERR|POLLNVAL|POLLHUP)) && (i_total > 0))
            return i_total; // error will be dequeued separately on next call

        if (p_vs != NULL)
            val = p_vs->pf_send (p_vs->p_sys, p_data, i_data);
        else
#ifdef WIN32
            val = send (fd, p_data, i_data, 0);
#else
            val = write (fd, p_data, i_data);
#endif

        if (val == -1)
        {
            msg_Err (p_this, "Write error: %m");
            break;
        }

        p_data += val;
        i_data -= val;
        i_total += val;
    }

out:
    if ((i_total > 0) || (i_data == 0))
        return i_total;

    return -1;
}

char *__net_Gets( vlc_object_t *p_this, int fd, const v_socket_t *p_vs )
{
    char *psz_line = NULL, *ptr = NULL;
    size_t  i_line = 0, i_max = 0;


    for( ;; )
    {
        if( i_line == i_max )
        {
            i_max += 1024;
            psz_line = realloc( psz_line, i_max );
            ptr = psz_line + i_line;
        }

        if( net_Read( p_this, fd, p_vs, (uint8_t *)ptr, 1, VLC_TRUE ) != 1 )
        {
            if( i_line == 0 )
            {
                free( psz_line );
                return NULL;
            }
            break;
        }

        if ( *ptr == '\n' )
            break;

        i_line++;
        ptr++;
    }

    *ptr-- = '\0';

    if( ( ptr >= psz_line ) && ( *ptr == '\r' ) )
        *ptr = '\0';

    return psz_line;
}

ssize_t net_Printf( vlc_object_t *p_this, int fd, const v_socket_t *p_vs,
                    const char *psz_fmt, ... )
{
    int i_ret;
    va_list args;
    va_start( args, psz_fmt );
    i_ret = net_vaPrintf( p_this, fd, p_vs, psz_fmt, args );
    va_end( args );

    return i_ret;
}

ssize_t __net_vaPrintf( vlc_object_t *p_this, int fd, const v_socket_t *p_vs,
                        const char *psz_fmt, va_list args )
{
    char    *psz;
    int     i_size, i_ret;

    i_size = vasprintf( &psz, psz_fmt, args );
    i_ret = __net_Write( p_this, fd, p_vs, (uint8_t *)psz, i_size ) < i_size
        ? -1 : i_size;
    free( psz );

    return i_ret;
}


/*****************************************************************************
 * inet_pton replacement for obsolete and/or crap operating systems
 *****************************************************************************/
#ifndef HAVE_INET_PTON
int inet_pton(int af, const char *src, void *dst)
{
# ifdef WIN32
    /* As we already know, Microsoft always go its own way, so even if they do
     * provide IPv6, they don't provide the API. */
    struct sockaddr_storage addr;
    int len = sizeof( addr );

    /* Damn it, they didn't even put LPCSTR for the firs parameter!!! */
#ifdef UNICODE
    wchar_t *workaround_for_ill_designed_api =
        malloc( MAX_PATH * sizeof(wchar_t) );
    mbstowcs( workaround_for_ill_designed_api, src, MAX_PATH );
    workaround_for_ill_designed_api[MAX_PATH-1] = 0;
#else
    char *workaround_for_ill_designed_api = strdup( src );
#endif

    if( !WSAStringToAddress( workaround_for_ill_designed_api, af, NULL,
                             (LPSOCKADDR)&addr, &len ) )
    {
        free( workaround_for_ill_designed_api );
        return -1;
    }
    free( workaround_for_ill_designed_api );

    switch( af )
    {
        case AF_INET6:
            memcpy( dst, &((struct sockaddr_in6 *)&addr)->sin6_addr, 16 );
            break;

        case AF_INET:
            memcpy( dst, &((struct sockaddr_in *)&addr)->sin_addr, 4 );
            break;

        default:
            WSASetLastError( WSAEAFNOSUPPORT );
            return -1;
    }
# else
    /* Assume IPv6 is not supported. */
    /* Would be safer and more simpler to use inet_aton() but it is most
     * likely not provided either. */
    uint32_t ipv4;

    if( af != AF_INET )
    {
        errno = EAFNOSUPPORT;
        return -1;
    }

    ipv4 = inet_addr( src );
    if( ipv4 == INADDR_NONE )
        return -1;

    memcpy( dst, &ipv4, 4 );
# endif /* WIN32 */
    return 0;
}
#endif /* HAVE_INET_PTON */

#ifndef HAVE_INET_NTOP
#ifdef WIN32
const char *inet_ntop(int af, const void * src,
                               char * dst, socklen_t cnt)
{
    switch( af )
    {
#ifdef AF_INET6
        case AF_INET6:
            {
                struct sockaddr_in6 addr;
                memset(&addr, 0, sizeof(addr));
                addr.sin6_family = AF_INET6;
                addr.sin6_addr = *((struct in6_addr*)src);
                if( 0 == WSAAddressToStringA((LPSOCKADDR)&addr,
                                             sizeof(struct sockaddr_in6),
                                             NULL, dst, &cnt) )
                {
                    dst[cnt] = '\0';
                    return dst;
                }
                errno = WSAGetLastError();
                return NULL;

            }

#endif
        case AF_INET:
            {
                struct sockaddr_in addr;
                memset(&addr, 0, sizeof(addr));
                addr.sin_family = AF_INET;
                addr.sin_addr = *((struct in_addr*)src);
                if( 0 == WSAAddressToStringA((LPSOCKADDR)&addr,
                                             sizeof(struct sockaddr_in),
                                             NULL, dst, &cnt) )
                {
                    dst[cnt] = '\0';
                    return dst;
                }
                errno = WSAGetLastError();
                return NULL;

            }
    }
    errno = EAFNOSUPPORT;
    return NULL;
}
#endif
#endif
