/*****************************************************************************
 * rootbind.c: bind to reserved ports through the root wrapper
 *****************************************************************************
 * Copyright © 2005-2008 Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define _XPG4_2 /* ancilliary data on Solaris */

#if !defined (_WIN32) && !defined (__OS2__)
# define ENABLE_ROOTWRAP 1
#endif

#include <stddef.h>
struct sockaddr;
int rootwrap_bind (int, int, int, const struct sockaddr *, size_t);

#include <errno.h>

#ifdef ENABLE_ROOTWRAP

#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <pthread.h>

/* Required yet non-standard cmsg functions */
#ifndef CMSG_ALIGN
# define CMSG_ALIGN(len) (((len) + sizeof(intptr_t)-1) & ~(sizeof(intptr_t)-1))
#endif
#ifndef CMSG_SPACE
# define CMSG_SPACE(len) (CMSG_ALIGN(sizeof(struct cmsghdr)) + CMSG_ALIGN(len))
#endif
#ifndef CMSG_LEN
# define CMSG_LEN(len) (CMSG_ALIGN(sizeof(struct cmsghdr)) + (len))
#endif

#if defined(__OS2__) && !defined(ALIGN)
/* CMSG_NXTHDR requires this */
# define ALIGN(p) _ALIGN(p)
#endif

/**
 * Receive a file descriptor from another process
 */
static int recv_fd (int p)
{
    struct msghdr hdr;
    struct iovec iov;
    struct cmsghdr *cmsg;
    int val, fd;
    char buf[CMSG_SPACE (sizeof (fd))];

    hdr.msg_name = NULL;
    hdr.msg_namelen = 0;
    hdr.msg_iov = &iov;
    hdr.msg_iovlen = 1;
    hdr.msg_control = buf;
    hdr.msg_controllen = sizeof (buf);

    iov.iov_base = &val;
    iov.iov_len = sizeof (val);

    if (recvmsg (p, &hdr, 0) != sizeof (val))
        return -1;

    for (cmsg = CMSG_FIRSTHDR (&hdr); cmsg != NULL;
         cmsg = CMSG_NXTHDR (&hdr, cmsg))
    {
        if ((cmsg->cmsg_level == SOL_SOCKET)
         && (cmsg->cmsg_type == SCM_RIGHTS)
         && (cmsg->cmsg_len >= CMSG_LEN (sizeof (fd))))
        {
            memcpy (&fd, CMSG_DATA (cmsg), sizeof (fd));
            return fd;
        }
    }

    errno = val;
    return -1;
}

/**
 * Tries to obtain a bound TCP socket from the root process
 */
int rootwrap_bind (int family, int socktype, int protocol,
                   const struct sockaddr *addr, size_t alen)
{
    /* can't use libvlc */
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    struct sockaddr_storage ss;
    int fd, sock = -1;

    const char *sockenv = getenv ("VLC_ROOTWRAP_SOCK");
    if (sockenv != NULL)
        sock = atoi (sockenv);
    if (sock == -1)
    {
        errno = EACCES;
        return -1;
    }

    switch (family)
    {
        case AF_INET:
            if (alen < sizeof (struct sockaddr_in))
            {
                errno = EINVAL;
                return -1;
            }
            break;

#ifdef AF_INET6
        case AF_INET6:
            if (alen < sizeof (struct sockaddr_in6))
            {
                errno = EINVAL;
                return -1;
            }
            break;
#endif

        default:
            errno = EAFNOSUPPORT;
            return -1;
    }

    if (family != addr->sa_family)
    {
        errno = EAFNOSUPPORT;
        return -1;
    }

    /* Only TCP is implemented at the moment */
    if ((socktype != SOCK_STREAM)
     || (protocol && (protocol != IPPROTO_TCP)))
    {
        errno = EACCES;
        return -1;
    }

    memset (&ss, 0, sizeof (ss));
    memcpy (&ss, addr, (alen > sizeof (ss)) ? sizeof (ss) : alen);

    pthread_mutex_lock (&mutex);
    if (send (sock, &ss, sizeof (ss), 0) != sizeof (ss))
    {
        pthread_mutex_unlock (&mutex);
        return -1;
    }

    fd = recv_fd (sock);
    pthread_mutex_unlock (&mutex);
    return fd;
}

#else
int rootwrap_bind (int family, int socktype, int protocol,
                   const struct sockaddr *addr, size_t alen)
{
    (void)family;
    (void)socktype;
    (void)protocol;
    (void)addr;
    (void)alen;
    errno = EACCES;
    return -1;
}

#endif /* ENABLE_ROOTWRAP */
