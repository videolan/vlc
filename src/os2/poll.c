/*****************************************************************************
 * poll.c: poll() emulation for OS/2
 *****************************************************************************
 * Copyright © 2011 KO Myung-Hun <komh@chollian.et>
 * Copyright © 2007 Rémi Denis-Courmont
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
# include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

#include <vlc_common.h>
#include <vlc_network.h>

int vlc_poll (struct pollfd *fds, unsigned nfds, int timeout)
{
    fd_set rdset;
    fd_set wrset;
    fd_set exset;
    struct timeval tv = { 0, 0 };
    int val;

resume:
    val = -1;
    vlc_testcancel ();

    FD_ZERO (&rdset);
    FD_ZERO (&wrset);
    FD_ZERO (&exset);
    for (unsigned i = 0; i < nfds; i++)
    {
        int fd = fds[i].fd;

        if (fds[i].fd >= FD_SETSIZE)
        {
            errno = EINVAL;
            return -1;
        }

        if (val < fd)
            val = fd;

        if (fds[i].events & POLLIN)
            FD_SET (fd, &rdset);
        if (fds[i].events & POLLOUT)
            FD_SET (fd, &wrset);
        if (fds[i].events & POLLPRI)
            FD_SET (fd, &exset);
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

    val = select (val + 1, &rdset, &wrset, &exset, &tv);

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
        fds[i].revents = (FD_ISSET (fd, &rdset) ? POLLIN : 0)
                       | (FD_ISSET (fd, &wrset) ? POLLOUT : 0)
                       | (FD_ISSET (fd, &exset) ? POLLPRI : 0);
    }
    return val;
}
