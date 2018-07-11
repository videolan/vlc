/*****************************************************************************
 * io.c: network I/O functions
 *****************************************************************************
 * Copyright (C) 2004-2005, 2007 VLC authors and VideoLAN
 * Copyright © 2005-2006 Rémi Denis-Courmont
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@videolan.org>
 *          Rémi Denis-Courmont <rem # videolan.org>
 *          Christophe Mutricy <xtophe at videolan dot org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>

#include <unistd.h>
#ifdef HAVE_LINUX_DCCP_H
/* TODO: use glibc instead of linux-kernel headers */
# include <linux/dccp.h>
# define SOL_DCCP 269
#endif

#include <vlc_common.h>
#include <vlc_network.h>
#include <vlc_interrupt.h>

extern int rootwrap_bind (int family, int socktype, int protocol,
                          const struct sockaddr *addr, size_t alen);

int net_Socket (vlc_object_t *p_this, int family, int socktype,
                int protocol)
{
    int fd = vlc_socket (family, socktype, protocol, true);
    if (fd == -1)
    {
        if (net_errno != EAFNOSUPPORT)
            msg_Err (p_this, "cannot create socket: %s",
                     vlc_strerror_c(net_errno));
        return -1;
    }

    setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof (int));

#ifdef IPV6_V6ONLY
    /*
     * Accepts only IPv6 connections on IPv6 sockets.
     * If possible, we should open two sockets, but it is not always possible.
     */
    if (family == AF_INET6)
        setsockopt (fd, IPPROTO_IPV6, IPV6_V6ONLY, &(int){ 1 }, sizeof (int));
#endif

#if defined (_WIN32)
# ifndef IPV6_PROTECTION_LEVEL
#  warning Please update your C library headers.
#  define IPV6_PROTECTION_LEVEL 23
#  define PROTECTION_LEVEL_UNRESTRICTED 10
# endif
    if (family == AF_INET6)
        setsockopt (fd, IPPROTO_IPV6, IPV6_PROTECTION_LEVEL,
                    &(int){ PROTECTION_LEVEL_UNRESTRICTED }, sizeof (int));
#endif

#ifdef DCCP_SOCKOPT_SERVICE
    if (socktype == SOL_DCCP)
    {
        char *dccps = var_InheritString (p_this, "dccp-service");
        if (dccps != NULL)
        {
            setsockopt (fd, SOL_DCCP, DCCP_SOCKOPT_SERVICE, dccps,
                        (strlen (dccps) + 3) & ~3);
            free (dccps);
        }
    }
#endif

    return fd;
}


int *net_Listen (vlc_object_t *p_this, const char *psz_host,
                 unsigned i_port, int type, int protocol)
{
    struct addrinfo hints = {
        .ai_socktype = type,
        .ai_protocol = protocol,
        .ai_flags = AI_PASSIVE | AI_NUMERICSERV | AI_IDN,
    }, *res;

    msg_Dbg (p_this, "net: listening to %s port %u",
             (psz_host != NULL) ? psz_host : "*", i_port);

    int i_val = vlc_getaddrinfo (psz_host, i_port, &hints, &res);
    if (i_val)
    {
        msg_Err (p_this, "Cannot resolve %s port %u : %s",
                 (psz_host != NULL) ? psz_host : "", i_port,
                 gai_strerror (i_val));
        return NULL;
    }

    int *sockv = NULL;
    unsigned sockc = 0;

    for (struct addrinfo *ptr = res; ptr != NULL; ptr = ptr->ai_next)
    {
        int fd = net_Socket (p_this, ptr->ai_family, ptr->ai_socktype,
                             ptr->ai_protocol);
        if (fd == -1)
        {
            msg_Dbg (p_this, "socket error: %s", vlc_strerror_c(net_errno));
            continue;
        }

        /* Bind the socket */
#if defined (_WIN32)
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
            int err = net_errno;
            net_Close (fd);
#if !defined(_WIN32)
            fd = rootwrap_bind (ptr->ai_family, ptr->ai_socktype,
                                ptr->ai_protocol,
                                ptr->ai_addr, ptr->ai_addrlen);
            if (fd != -1)
            {
                msg_Dbg (p_this, "got socket %d from rootwrap", fd);
            }
            else
#endif
            {
                msg_Err (p_this, "socket bind error: %s", vlc_strerror_c(err));
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
        switch (ptr->ai_socktype)
        {
            case SOCK_STREAM:
            case SOCK_RDM:
            case SOCK_SEQPACKET:
#ifdef SOCK_DCCP
            case SOCK_DCCP:
#endif
                if (listen (fd, INT_MAX))
                {
                    msg_Err (p_this, "socket listen error: %s",
                             vlc_strerror_c(net_errno));
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

    freeaddrinfo (res);

    if (sockv != NULL)
        sockv[sockc] = -1;

    return sockv;
}

/**
 * Reads data from a socket, blocking until all requested data is received or
 * the end of the stream is reached.
 * This function is a cancellation point.
 * @return -1 on error, or the number of bytes of read.
 */
ssize_t (net_Read)(vlc_object_t *restrict obj, int fd,
                   void *restrict buf, size_t len)
{
    size_t rd = 0;

    do
    {
        if (vlc_killed())
        {
            vlc_testcancel();
            errno = EINTR;
            return -1;
        }

        ssize_t val = vlc_recv_i11e(fd, buf, len, 0);
        if (val < 0)
        {
            if (errno == EINTR || errno == EAGAIN)
                continue;
#ifdef _WIN32
            else if (WSAGetLastError() == WSAEMSGSIZE) /* datagram too big */
            {
                msg_Warn(obj, "read truncated to %zu bytes", len);
                val = len;
            }
#endif
            else
            {
                msg_Err(obj, "read error: %s", vlc_strerror_c(errno));
                return rd ? (ssize_t)rd : -1;
            }
        }

        rd += val;

        if (val == 0)
            break;

        assert(len >= (size_t)val);
        len -= val;
        buf = ((char *)buf) + val;
    }
    while (len > 0);

    return rd;
}

/**
 * Writes data to a socket.
 * This blocks until all data is written or an error occurs.
 *
 * This function is a cancellation point.
 *
 * @return the total number of bytes written, or -1 if an error occurs
 * before any data is written.
 */
ssize_t (net_Write)(vlc_object_t *obj, int fd, const void *buf, size_t len)
{
    size_t written = 0;

    do
    {
        if (vlc_killed())
        {
            vlc_testcancel();
            errno = EINTR;
            return -1;
        }

        ssize_t val = vlc_send_i11e (fd, buf, len, MSG_NOSIGNAL);
        if (val == -1)
        {
            if (errno == EINTR || errno == EAGAIN)
                continue;

            msg_Err(obj, "write error: %s", vlc_strerror_c(errno));
            return written ? (ssize_t)written : -1;
        }

        if (val == 0)
            break;

        written += val;
        assert(len >= (size_t)val);
        len -= val;
        buf = ((const char *)buf) + val;
    }
    while (len > 0);

    return written;
}

#undef net_Gets
/**
 * Reads a line from a file descriptor.
 * This function is not thread-safe; the same file descriptor I/O cannot be
 * read by another thread at the same time (although it can be written to).
 *
 * @note This only works with stream-oriented file descriptors, not with
 * datagram or packet-oriented ones.
 *
 * @return nul-terminated heap-allocated string, or NULL on I/O error.
 */
char *net_Gets(vlc_object_t *obj, int fd)
{
    char *buf = NULL;
    size_t size = 0, len = 0;

    for (;;)
    {
        if (len == size)
        {
            if (unlikely(size >= (1 << 16)))
            {
                errno = EMSGSIZE;
                goto error; /* put sane buffer size limit */
            }

            char *newbuf = realloc(buf, size + 1024);
            if (unlikely(newbuf == NULL))
                goto error;
            buf = newbuf;
            size += 1024;
        }
        assert(len < size);

        ssize_t val = vlc_recv_i11e(fd, buf + len, size - len, MSG_PEEK);
        if (val <= 0)
            goto error;

        char *end = memchr(buf + len, '\n', val);
        if (end != NULL)
            val = (end + 1) - (buf + len);
        if (recv(fd, buf + len, val, 0) != val)
            goto error;
        len += val;
        if (end != NULL)
            break;
    }

    assert(len > 0);
    buf[--len] = '\0';
    if (len > 0 && buf[--len] == '\r')
        buf[len] = '\0';
    return buf;
error:
    msg_Err(obj, "read error: %s", vlc_strerror_c(errno));
    free(buf);
    return NULL;
}

#undef net_Printf
ssize_t net_Printf( vlc_object_t *p_this, int fd, const char *psz_fmt, ... )
{
    int i_ret;
    va_list args;
    va_start( args, psz_fmt );
    i_ret = net_vaPrintf( p_this, fd, psz_fmt, args );
    va_end( args );

    return i_ret;
}

#undef net_vaPrintf
ssize_t net_vaPrintf( vlc_object_t *p_this, int fd,
                      const char *psz_fmt, va_list args )
{
    char    *psz;
    int      i_ret;

    int i_size = vasprintf( &psz, psz_fmt, args );
    if( i_size == -1 )
        return -1;
    i_ret = net_Write( p_this, fd, psz, i_size ) < i_size
        ? -1 : i_size;
    free( psz );

    return i_ret;
}
