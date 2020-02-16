/*****************************************************************************
 * linux/getaddrinfo.c: interruptible DNS resolution for Linux
 *****************************************************************************
 * Copyright (C) 2016 Rémi Denis-Courmont
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

#include <assert.h>
#include <stdio.h>
#include <netdb.h>
#include <signal.h>

#include <vlc_common.h>
#include <vlc_interrupt.h>
#include <vlc_network.h>

static void vlc_getaddrinfo_notify(union sigval val)
{
    vlc_sem_post(val.sival_ptr);
}

int vlc_getaddrinfo_i11e(const char *name, unsigned port,
                         const struct addrinfo *hints,
                         struct addrinfo **res)
{
    struct gaicb req =
    {
        .ar_name = name,
        .ar_service = NULL,
        .ar_request = hints,
    };
    char portbuf[6];
    vlc_sem_t done;

    if (port != 0)
    {
        if ((size_t)snprintf(portbuf, sizeof (portbuf), "%u",
                             port) >= sizeof (portbuf))
            return EAI_NONAME;

        req.ar_service = portbuf;
    }

    struct sigevent sev =
    {
        .sigev_notify = SIGEV_THREAD,
        .sigev_value = { .sival_ptr = &done, },
        .sigev_notify_function = vlc_getaddrinfo_notify,
    };

    vlc_sem_init(&done, 0);

    int val = getaddrinfo_a(GAI_NOWAIT, &(struct gaicb *){ &req }, 1, &sev);
    if (val)
        return val;

    vlc_sem_wait_i11e(&done);

    if (gai_cancel(&req) == EAI_CANCELED)
        vlc_sem_wait(&done);

    while (gai_suspend(&(const struct gaicb *){ &req }, 1, NULL) == EAI_INTR);

    val = gai_error(&req);
    assert(val != EAI_INPROGRESS);

    if (val == 0)
        *res = req.ar_result;

    return val;
}
