/*****************************************************************************
 * io.c: network I/O functions
 *****************************************************************************
 * Copyright (C) 2004-2005, 2007 VLC authors and VideoLAN
 * Copyright © 2005-2006 Rémi Denis-Courmont
 *
 * Authors: Laurent Aimar <fenrir@videolan.org>
 *          Rémi Denis-Courmont
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
#ifdef HAVE_POLL_H
# include <poll.h>
#endif
#ifdef HAVE_LINUX_DCCP_H
/* TODO: use glibc instead of linux-kernel headers */
# include <linux/dccp.h>
# define SOL_DCCP 269
#endif

#include <vlc_common.h>
#include <vlc_network.h>
#include <vlc_interrupt.h>
#if defined (_WIN32)
#   undef EINPROGRESS
#   define EINPROGRESS WSAEWOULDBLOCK
#   undef EWOULDBLOCK
#   define EWOULDBLOCK WSAEWOULDBLOCK
#   undef EAGAIN
#   define EAGAIN WSAEWOULDBLOCK
#endif

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

#ifdef _WIN32
    // Windows expects a BOOL for some getsockopt/setsockopt options
    static_assert(sizeof(int)==sizeof(BOOL), "mismatching type for setsockopt");
#endif
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

int (net_Connect)(vlc_object_t *obj, const char *host, int serv,
                  int type, int proto)
{
    struct addrinfo hints = {
        .ai_socktype = type,
        .ai_protocol = proto,
        .ai_flags = AI_NUMERICSERV | AI_IDN,
    }, *res;
    int ret = -1;

    int val = vlc_getaddrinfo_i11e(host, serv, &hints, &res);
    if (val)
    {
        msg_Err(obj, "cannot resolve %s port %d : %s", host, serv,
                gai_strerror (val));
        return -1;
    }

    vlc_tick_t timeout = VLC_TICK_FROM_MS(var_InheritInteger(obj,
                                                             "ipv4-timeout"));

    for (struct addrinfo *ptr = res; ptr != NULL; ptr = ptr->ai_next)
    {
        int fd = net_Socket(obj, ptr->ai_family,
                            ptr->ai_socktype, ptr->ai_protocol);
        if (fd == -1)
        {
            msg_Dbg(obj, "socket error: %s", vlc_strerror_c(net_errno));
            continue;
        }

        if (connect(fd, ptr->ai_addr, ptr->ai_addrlen))
        {
            if (net_errno != EINPROGRESS && errno != EINTR)
            {
                msg_Err(obj, "connection failed: %s",
                        vlc_strerror_c(net_errno));
                goto next_ai;
            }

            struct pollfd ufd;
            vlc_tick_t deadline = VLC_TICK_INVALID;

            ufd.fd = fd;
            ufd.events = POLLOUT;
            deadline = vlc_tick_now() + timeout;

            do
            {
                vlc_tick_t now = vlc_tick_now();

                if (vlc_killed())
                    goto next_ai;

                if (now > deadline)
                    now = deadline;

                val = vlc_poll_i11e(&ufd, 1, MS_FROM_VLC_TICK(deadline - now));
            }
            while (val == -1 && errno == EINTR);

            switch (val)
            {
                 case -1: /* error */
                     msg_Err(obj, "polling error: %s",
                             vlc_strerror_c(net_errno));
                     goto next_ai;

                 case 0: /* timeout */
                     msg_Warn(obj, "connection timed out");
                     goto next_ai;
            }

            /* There is NO WAY around checking SO_ERROR.
             * Don't ifdef it out!!! */
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &val,
                           &(socklen_t){ sizeof (val) }) || val)
            {
                msg_Err(obj, "connection failed: %s", vlc_strerror_c(val));
                goto next_ai;
            }
        }

        msg_Dbg(obj, "connection succeeded (socket = %d)", fd);
        ret = fd; /* success! */
        break;

next_ai: /* failure */
        net_Close(fd);
    }

    freeaddrinfo(res);
    return ret;
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

        /* Listen */
        if (listen(fd, INT_MAX))
        {
            msg_Err(p_this, "socket listen error: %s",
                    vlc_strerror_c(net_errno));
            net_Close(fd);
            continue;
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

void net_ListenClose(int *fds)
{
    if (fds != NULL)
    {
        for (int *p = fds; *p != -1; p++)
            net_Close(*p);

        free(fds);
    }
}

#undef net_Accept
int net_Accept(vlc_object_t *obj, int *fds)
{
    assert(fds != NULL);

    unsigned n = 0;
    while (fds[n] != -1)
        n++;

    struct pollfd ufd[n];
    /* Initialize file descriptor set */
    for (unsigned i = 0; i < n; i++)
    {
        ufd[i].fd = fds[i];
        ufd[i].events = POLLIN;
    }

    for (;;)
    {
        while (poll(ufd, n, -1) == -1)
        {
            if (net_errno != EINTR)
            {
                msg_Err(obj, "poll error: %s", vlc_strerror_c(net_errno));
                return -1;
            }
        }

        for (unsigned i = 0; i < n; i++)
        {
            if (ufd[i].revents == 0)
                continue;

            int sfd = ufd[i].fd;
            int fd = vlc_accept(sfd, NULL, NULL, true);
            if (fd == -1)
            {
                if (net_errno != EAGAIN)
#if (EAGAIN != EWOULDBLOCK)
                if (net_errno != EWOULDBLOCK)
#endif
                    msg_Err(obj, "accept failed (from socket %d): %s", sfd,
                            vlc_strerror_c(net_errno));
                continue;
            }

            msg_Dbg(obj, "accepted socket %d (from socket %d)", fd, sfd);
            setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                       &(int){ 1 }, sizeof (int));
            /*
             * Move listening socket to the end to let the others in the
             * set a chance next time.
             */
            memmove(fds + i, fds + i + 1, n - (i + 1));
            fds[n - 1] = sfd;
            return fd;
        }
    }
    return -1;
}

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

        ssize_t val = vlc_send_i11e(fd, buf, len, 0);
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
