/*****************************************************************************
 * netbsd/thread.c: NetBSD specifics for threading
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

#include <pthread.h>
#include <lwp.h>

#include <vlc_common.h>
#include <vlc_threads.h>
#include <vlc_atomic.h>

unsigned long vlc_thread_id(void)
{
     static _Thread_local lwpid_t tid = -1;

     if (unlikely(tid == -1))
         tid = _lwp_self();

     return tid;
}

void (vlc_thread_set_name)(const char *name)
{
    pthread_setname_np(pthread_self(), "%s", (void *)name);
}
