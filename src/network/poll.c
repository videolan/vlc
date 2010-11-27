/*****************************************************************************
 * poll.c: I/O event multiplexing
 *****************************************************************************
 * Copyright © 2007 Rémi Denis-Courmont
 * $Id$
 *
 * Author: Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#ifdef HAVE_MAEMO
# include <vlc_network.h>
# include <signal.h>
# include <errno.h>
# include <poll.h>

int vlc_poll (struct pollfd *fds, unsigned nfds, int timeout)
{
    struct timespec tsbuf, *ts;
    sigset_t set;
    int canc, ret;

    if (timeout != -1)
    {
        div_t d = div (timeout, 1000);
        tsbuf.tv_sec = d.quot;
        tsbuf.tv_nsec = d.rem * 1000000;
        ts = &tsbuf;
    }
    else
        ts = NULL;

    pthread_sigmask (SIG_BLOCK, NULL, &set);
    sigdelset (&set, SIGRTMIN);

    canc = vlc_savecancel ();
    ret = ppoll (fds, nfds, ts, &set);
    vlc_restorecancel (canc);

    vlc_testcancel ();
    return ret;
}

#elif defined (HAVE_POLL)
# include <vlc_network.h>

struct pollfd;

int vlc_poll (struct pollfd *fds, unsigned nfds, int timeout)
{
    (void)fds; (void)nfds; (void)timeout;
    abort ();
}

#elif defined (WIN32)

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef FD_SETSIZE
/* No, it's not as simple as #undef FD_SETSIZE */
# error Header inclusion order compromised!
#endif
#define FD_SETSIZE 0
#include <vlc_network.h>

int vlc_poll (struct pollfd *fds, unsigned nfds, int timeout)
{
    size_t setsize = sizeof (fd_set) + nfds * sizeof (SOCKET);
    fd_set *rdset = malloc (setsize);
    fd_set *wrset = malloc (setsize);
    fd_set *exset = malloc (setsize);
    struct timeval tv = { 0, 0 };
    int val;

    if (unlikely(rdset == NULL || wrset == NULL || exset == NULL))
    {
        free (rdset);
        free (wrset);
        free (exset);
        errno = ENOMEM;
        return -1;
    }

/* Winsock FD_SET uses FD_SETSIZE in its expansion */
#undef FD_SETSIZE
#define FD_SETSIZE (nfds)

resume:
    val = -1;
    vlc_testcancel ();

    FD_ZERO (rdset);
    FD_ZERO (wrset);
    FD_ZERO (exset);
    for (unsigned i = 0; i < nfds; i++)
    {
        int fd = fds[i].fd;
        if (val < fd)
            val = fd;

        /* With POSIX, FD_SET & FD_ISSET are not defined if fd is negative or
         * bigger or equal than FD_SETSIZE. That is one of the reasons why VLC
         * uses poll() rather than select(). Most POSIX systems implement
         * fd_set has a bit field with no sanity checks. This is especially bad
         * on systems (such as BSD) that have no process open files limit by
         * default, such that it is quite feasible to get fd >= FD_SETSIZE.
         * The next instructions will result in a buffer overflow if run on
         * a POSIX system, and the later FD_ISSET would perform an undefined
         * memory read.
         *
         * With Winsock, fd_set is a table of integers. This is awfully slow.
         * However, FD_SET and FD_ISSET silently and safely discard excess
         * entries. Here, overflow cannot happen anyway: fd_set of adequate
         * size are allocated.
         * Note that Vista has a much nicer WSAPoll(), but Mingw does not
         * support it yet.
         */
        if (fds[i].events & POLLIN)
            FD_SET ((SOCKET)fd, rdset);
        if (fds[i].events & POLLOUT)
            FD_SET ((SOCKET)fd, wrset);
        if (fds[i].events & POLLPRI)
            FD_SET ((SOCKET)fd, exset);
    }

#ifndef HAVE_ALERTABLE_SELECT
# warning FIXME! Fix cancellation and remove this crap.
    if ((timeout < 0) || (timeout > 50))
    {
        tv.tv_sec = 0;
        tv.tv_usec = 50000;
    }
    else
#endif
    if (timeout >= 0)
    {
        div_t d = div (timeout, 1000);
        tv.tv_sec = d.quot;
        tv.tv_usec = d.rem * 1000;
    }

    val = select (val + 1, rdset, wrset, exset,
                  /*(timeout >= 0) ?*/ &tv /*: NULL*/);

#ifndef HAVE_ALERTABLE_SELECT
    if (val == 0)
    {
        if (timeout > 0)
            timeout -= (timeout > 50) ? 50 : timeout;
        if (timeout != 0)
            goto resume;
    }
#endif

    if (val == -1)
        return -1;

    for (unsigned i = 0; i < nfds; i++)
    {
        int fd = fds[i].fd;
        fds[i].revents = (FD_ISSET (fd, rdset) ? POLLIN : 0)
                       | (FD_ISSET (fd, wrset) ? POLLOUT : 0)
                       | (FD_ISSET (fd, exset) ? POLLPRI : 0);
    }
    free (exset);
    free (wrset);
    free (rdset);
    return val;
}

#else
# error poll() not implemented!
#endif
