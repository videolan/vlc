/*****************************************************************************
 * vlc_poll.h : poll implementation for the VideoLAN client
 *****************************************************************************
 * Copyright (C) 1999, 2002 VLC authors and VideoLAN
 * Copyright © 2007-2016 Rémi Denis-Courmont
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

#ifndef VLC_POLL_H_
#define VLC_POLL_H_

#include <vlc_threads.h>

/**
 * \ingroup os
 * \defgroup thread_poll Poll implementations
 * @{
 * \file
 * Poll implementations
 */

#if defined (_WIN32)
static inline int vlc_poll(struct pollfd *fds, unsigned nfds, int timeout)
{
    int val;

    vlc_testcancel();
    val = poll(fds, nfds, timeout);
    if (val < 0)
        vlc_testcancel();
    return val;
}
# define poll(u,n,t) vlc_poll(u, n, t)

#elif defined (__OS2__)
static inline int vlc_poll (struct pollfd *fds, unsigned nfds, int timeout)
{
    static int (*vlc_poll_os2)(struct pollfd *, unsigned, int) = NULL;

    if (!vlc_poll_os2)
    {
        HMODULE hmod;
        CHAR szFailed[CCHMAXPATH];

        if (DosLoadModule(szFailed, sizeof(szFailed), "vlccore", &hmod))
            return -1;

        if (DosQueryProcAddr(hmod, 0, "_vlc_poll_os2", (PFN *)&vlc_poll_os2))
            return -1;
    }

    return (*vlc_poll_os2)(fds, nfds, timeout);
}
# define poll(u,n,t) vlc_poll(u, n, t)

#elif defined (__ANDROID__)      /* pthreads subset without pthread_cancel() */
static inline int vlc_poll (struct pollfd *fds, unsigned nfds, int timeout)
{
    int val;

    do
    {
        int ugly_timeout = ((unsigned)timeout >= 50) ? 50 : timeout;
        if (timeout >= 0)
            timeout -= ugly_timeout;

        vlc_testcancel ();
        val = poll (fds, nfds, ugly_timeout);
    }
    while (val == 0 && timeout != 0);

    return val;
}

# define poll(u,n,t) vlc_poll(u, n, t)

#else /* POSIX threads */

#endif

/** @} */

#endif /* !VLC_POLL_H_ */
