/*****************************************************************************
 * rootwrap.c
 *****************************************************************************
 * Copyright © 2005 Rémi Denis-Courmont
 * $Id$
 *
 * Author: Rémi Denis-Courmont <rem # videolan.org>
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

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if defined (HAVE_GETEUID) && !defined (SYS_BEOS)
# define ENABLE_ROOTWRAP 1
#endif

#ifdef ENABLE_ROOTWRAP

#include <stdlib.h> /* exit() */
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/uio.h>
#include <sys/resource.h> /* getrlimit() */
#include <sys/wait.h>
#include <sys/un.h>
#include <pwd.h> /* getpwnam(), getpwuid() */
#include <grp.h> /* setgroups() */
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>

/*#ifndef HAVE_CLEARENV
extern char **environ;

static int clearenv (void)
{
    environ = NULL;
    return 0;
}
#endif*/

/**
 * Tries to find a real non-root user to use
 */
static struct passwd *guess_user (void)
{
    const char *name;
    struct passwd *pw;
    uid_t uid;

    /* Try real UID */
    uid = getuid ();
    if (uid)
        if ((pw = getpwuid (uid)) != NULL)
            return pw;

    /* Try sudo */
    name = getenv ("SUDO_USER");
    if (name != NULL)
        if ((pw = getpwnam (name)) != NULL)
            return pw;

    /* Try VLC_USER */
    name = getenv ("VLC_USER");
    if (name != NULL)
        if ((pw = getpwnam (name)) != NULL)
            return pw;

    /* Try vlc */
    if ((pw = getpwnam ("vlc")) != NULL)
        return pw;

    return getpwuid (0);
}


static int is_allowed_port (uint16_t port)
{
    port = ntohs (port);

    return (port == 80) || (port == 443) || (port == 554);
}


static int send_err (int fd, int err)
{
    return send (fd, &err, sizeof (err), 0) == sizeof (err) ? 0 : -1;
}

/**
 * Ugly POSIX(?) code to pass a file descriptor to another process
 */
static int send_fd (int p, int fd)
{
    struct msghdr hdr;
    struct iovec iov;
    struct cmsghdr *cmsg;
    char buf[CMSG_SPACE (sizeof (fd))];
    int val = 0;

    hdr.msg_name = NULL;
    hdr.msg_namelen = 0;
    hdr.msg_iov = &iov;
    hdr.msg_iovlen = 1;
    hdr.msg_control = buf;
    hdr.msg_controllen = sizeof (buf);

    iov.iov_base = &val;
    iov.iov_len = sizeof (val);

    cmsg = CMSG_FIRSTHDR (&hdr);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN (sizeof (fd));
    memcpy (CMSG_DATA (cmsg), &fd, sizeof (fd));
    hdr.msg_controllen = cmsg->cmsg_len;

    return sendmsg (p, &hdr, 0) == sizeof (val) ? 0 : -1;
}


/**
 * Background process run as root to open privileged TCP ports.
 */
static void rootprocess (int fd)
{
    struct sockaddr_storage ss;

    /* TODO:
     *  - use libcap if available,
     *  - call chroot
     */
    while (recv (fd, &ss, sizeof (ss), 0) == sizeof (ss))
    {
        unsigned len;
        int sock;

        switch (ss.ss_family)
        {
            case AF_INET:
                if (!is_allowed_port (((struct sockaddr_in *)&ss)->sin_port))
                {
                    if (send_err (fd, EACCES))
                        return;
                    continue;
                }
                len = sizeof (struct sockaddr_in);
                break;

#ifdef AF_INET6
            case AF_INET6:
                if (!is_allowed_port (((struct sockaddr_in6 *)&ss)->sin6_port))
                {
                    if (send_err (fd, EACCES))
                        return;
                    continue;
                }
                len = sizeof (struct sockaddr_in6);
                break;
#endif

            default:
                if (send_err (fd, EAFNOSUPPORT))
                    return;
                continue;
        }

        sock = socket (ss.ss_family, SOCK_STREAM, IPPROTO_TCP);
        if (sock != -1)
        {
            const int val = 1;

            setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof (val));
#ifdef AF_INET6
            if (ss.ss_family == AF_INET6)
                setsockopt (sock, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof (val));
#endif
            if (bind (sock, (struct sockaddr *)&ss, len) == 0)
            {
                send_fd (fd, sock);
                close (sock);
                continue;
            }
        }
        send_err (fd, errno);
    }
}

static int rootwrap_sock = -1;
static pid_t rootwrap_pid = -1;

static void close_rootwrap (void)
{
    close (rootwrap_sock);
    waitpid (rootwrap_pid, NULL, 0);
}

void rootwrap (void)
{
    struct rlimit lim;
    int fd, pair[2];
    struct passwd *pw;
    uid_t u;

    u = geteuid ();
    /* Are we running with root privileges? */
    if (u != 0)
    {
        setuid (u);
        return;
    }

    /* Make sure 0, 1 and 2 are opened, and only these. */
    if (getrlimit (RLIMIT_NOFILE, &lim))
        exit (1);

    for (fd = 3; ((unsigned)fd) < lim.rlim_cur; fd++)
        close (fd);

    fd = dup (2);
    if (fd <= 2)
        exit (1);
    close (fd);

    fputs ("Starting VLC root wrapper...", stderr);

    pw = guess_user ();
    if (pw == NULL)
        return; /* Should we rather print an error and exit ? */

    u = pw->pw_uid,
    fprintf (stderr, " using UID %u (%s)\n", (unsigned)u, pw->pw_name);
    if (u == 0)
    {
        fputs ("***************************************\n"
               "* Running VLC as root is discouraged. *\n"
               "***************************************\n"
               "\n"
               " It is potentially dangerous, "
                "and might not even work properly.\n", stderr);
        return;
    }

    /* GID */
    initgroups (pw->pw_name, pw->pw_gid);
    setgid (pw->pw_gid);

    if (socketpair (AF_LOCAL, SOCK_STREAM, 0, pair))
    {
        perror ("socketpair");
        goto nofork;
    }

    switch (rootwrap_pid = fork ())
    {
        case -1:
            perror ("fork");
            close (pair[0]);
            close (pair[1]);
            break;

        case 0:
            close (0);
            close (1);
            close (2);
            close (pair[0]);
            rootprocess (pair[1]);
            exit (0);

        default:
            close (pair[1]);
            rootwrap_sock = pair[0];
            break;
    }

nofork:
    /* UID */
    setuid (u);

    atexit (close_rootwrap);
}


/**
 * Ugly POSIX(?) code to receive a file descriptor from another process
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
         && (cmsg->cmsg_type = SCM_RIGHTS)
         && (cmsg->cmsg_len >= CMSG_LEN (sizeof (fd))))
        {
            memcpy (&fd, CMSG_DATA (cmsg), sizeof (fd));
            return fd;
        }
    }

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
    int fd;

    if (rootwrap_sock == -1)
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
    memcpy (&ss, addr, alen > sizeof (ss) ? sizeof (ss) : alen);

    pthread_mutex_lock (&mutex);
    if (send (rootwrap_sock, &ss, sizeof (ss), 0) != sizeof (ss))
        return -1;

    fd = recv_fd (rootwrap_sock);
    pthread_mutex_unlock (&mutex);

    if (fd != -1)
    {
        int val;

        val = fcntl (fd, F_GETFL, 0);
        fcntl (fd, F_SETFL, ((val != -1) ? val : 0) | O_NONBLOCK);
    }

    return fd;
}

#else
# include <stddef.h>

struct sockaddr;

void rootwrap (void)
{
}

int rootwrap_bind (int family, int socktype, int protocol,
                   const struct sockaddr *addr, size_t alen)
{
    return -1;
}

#endif /* ENABLE_ROOTWRAP */
