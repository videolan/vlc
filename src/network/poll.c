/*****************************************************************************
 * poll.c: I/O event multiplexing
 *****************************************************************************
 * Copyright © 2007-2008 Rémi Denis-Courmont
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
#include <vlc_network.h>

#ifndef WIN32
struct pollfd;

int vlc_poll (struct pollfd *fds, unsigned nfds, int timeout)
{
    (void)fds; (void)nfds; (void)timeout;
    abort ();
}
#else
#include <string.h>
#include <stdlib.h>
#include <vlc_network.h>

int vlc_poll (struct pollfd *fds, unsigned nfds, int timeout)
{
    WSAEVENT phEvents[nfds];
    DWORD val;

    vlc_testcancel ();

    for (unsigned i = 0; i < nfds; i++)
    {
        long events = FD_CLOSE;

        if (fds[i].events & POLLIN)
            events |= FD_READ | FD_ACCEPT;
        if (fds[i].events & POLLOUT)
            events |= FD_WRITE;
        if (fds[i].events & POLLPRI)
            events |= FD_OOB;
        fds[i].revents = 0;

        phEvents[i] = WSACreateEvent ();
        WSAEventSelect (fds[i].fd, phEvents[i], events);
    }

    int ret = 0, n = 0;

    switch (WaitForMultipleObjectsEx (nfds, phEvents, FALSE, timeout, TRUE))
    {
      case WAIT_IO_COMPLETION:
        WSASetLastError (WSAEINTR);
        ret = -1;
        break;
      case WAIT_TIMEOUT:
        ret = 0;
        break;
      default:
        for (unsigned i = 0; i < nfds; i++)
        {
            WSANETWORKEVENTS events;
            if (WSAEnumNetworkEvents (fds[i].fd, phEvents[i], &events))
            {
                fds[i].revents |= POLLNVAL;
                ret = -1;
                continue;
            }
            if (events.lNetworkEvents & FD_CLOSE)
               fds[i].revents |= POLLHUP | (fds[i].events & POLLIN);
            if (events.lNetworkEvents & FD_ACCEPT)
               fds[i].revents |= POLLIN;
            if (events.lNetworkEvents & FD_OOB)
               fds[i].revents |= POLLPRI;
            if (events.lNetworkEvents & FD_READ)
               fds[i].revents |= POLLIN;
            if (events.lNetworkEvents & FD_WRITE)
            {
                fds[i].revents |= POLLOUT;
                if (events.iErrorCode[FD_WRITE_BIT])
                    fds[i].revents |= POLLERR;
            }
            if (fds[i].revents)
                n++;
        }
        if (ret == 0)
            ret = n;
    }

    for (unsigned i = 0; i < nfds; i++)
        WSACloseEvent (phEvents[i]);
    vlc_testcancel ();

    return ret;
}
#endif /* WIN32 */
