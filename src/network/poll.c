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

#else
# error poll() not implemented!
#endif
