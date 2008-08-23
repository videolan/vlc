/*****************************************************************************
 * rootwrap.c
 *****************************************************************************
 * Copyright © 2005-2008 Rémi Denis-Courmont
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

#include <stdlib.h> /* exit() */
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/resource.h> /* getrlimit() */
#include <sched.h>
#include <errno.h>
#include <netinet/in.h>

#if defined (AF_INET6) && !defined (IPV6_V6ONLY)
# warning Uho, your IPv6 support is broken and has been disabled. Fix your C library.
# undef AF_INET6
#endif

#ifndef AF_LOCAL
# define AF_LOCAL AF_UNIX
#endif

static inline int is_allowed_port (uint16_t port)
{
    port = ntohs (port);
    return (port == 80) || (port == 443) || (port == 554);
}


static inline int send_err (int fd, int err)
{
    return send (fd, &err, sizeof (err), 0) == sizeof (err) ? 0 : -1;
}

/**
 * Send a file descriptor to another process
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

/* TODO?
 *  - use libcap if available,
 *  - call chroot
 */

int main (int argc, char *argv[])
{
    /* Support for dynamically opening RTSP, HTTP and HTTP/SSL ports */
    int pair[2];

    if (socketpair (AF_LOCAL, SOCK_STREAM, 0, pair))
        return 1;
    if (pair[0] < 3)
        goto error; /* we want 0, 1 and 2 open */

    pid_t pid = fork ();
    switch (pid)
    {
        case -1:
            goto error;

        case 0:
        {
            int null = open ("/dev/null", O_RDWR);
            if (null != -1)
            {
                dup2 (null, 0);
                dup2 (null, 1);
                dup2 (null, 2);
                close (null);
            }
            close (pair[0]);
            setsid ();
            rootprocess (pair[1]);
            exit (0);
        }
    }

    close (pair[1]);
    pair[1] = -1;

    char buf[21];
    snprintf (buf, sizeof (buf), "%d", pair[0]);
    setenv ("VLC_ROOTWRAP_SOCK", buf, 1);

    /* Support for real-time priorities */
#ifdef RLIMIT_RTPRIO
    struct rlimit rlim;
    rlim.rlim_max = rlim.rlim_cur = sched_get_priority_min (SCHED_RR) + 24;
    setrlimit (RLIMIT_RTPRIO, &rlim);
#endif

    uid_t uid = getuid ();
    if (uid == 0)
    {
        const char *sudo = getenv ("SUDO_UID");
        if (sudo)
            uid = atoi (sudo);
    }
    if (uid == 0)
    {
        fprintf (stderr, "Cannot determine unprivileged user for VLC!\n");
        exit (1);
    }
    setuid (uid);

    if (!setuid (0)) /* sanity check: we cannot get root back */
        exit (1);

    /* Yeah, the user can execute just about anything from here.
     * But we've dropped privileges, so it does not matter. */
    if (strlen (argv[0]) < sizeof ("-wrapper"))
        goto error;
    argv[0][strlen (argv[0]) - strlen ("-wrapper")] = '\0';

    (void)argc;
    if (execvp (argv[0], argv))
        perror (argv[0]);

error:
    close (pair[0]);
    if (pair[1] != -1)
        close (pair[1]);
    return 1;
}
