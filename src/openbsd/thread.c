/*****************************************************************************
 * openbsd/thread.c: OpenBSD specifics for threading
 *****************************************************************************
 * Copyright (C) 2020 Rémi Denis-Courmont
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

#include <unistd.h>
#include <pthread.h>
#include <pthread_np.h>
#include <errno.h>

#include <vlc_common.h>
#include <vlc_threads.h>
#include <vlc_atomic.h>

/* Returns the current thread ID */
unsigned long vlc_thread_id(void)
{
    /* Use thread-local storage for caching the thread ID */
    static _Thread_local pid_t tid = -1;

    if (unlikely(tid == -1))
    {
        tid = getthrid();
        if (unlikely(tid == -1)) /* sanity check */
            tid = 0; /* fallback ID */
    }

    return (unsigned long)tid;
}

/* Sets the current thread's name (safe wrapper) */
void vlc_thread_set_name(const char *name)
{
    if (!name)
        return;

    int ret = pthread_set_name_np(pthread_self(), name);
    if (unlikely(ret != 0))
        msg_Warn(NULL, "Failed to set thread name: %s", strerror(ret));
}
