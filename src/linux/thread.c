/*****************************************************************************
 * linux/thread.c: Linux specifics for threading
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

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>

#ifndef FUTEX_PRIVATE_FLAG
#define FUTEX_WAKE_PRIVATE FUTEX_WAKE
#define FUTEX_WAIT_PRIVATE FUTEX_WAIT
#endif

#include <vlc_common.h>

unsigned long vlc_thread_id(void)
{
     static __thread pid_t tid = 0;

     if (unlikely(tid == 0))
         tid = syscall(__NR_gettid);

     return tid;
}

static int sys_futex(void *addr, int op, unsigned val,
                     const struct timespec *to, void *addr2, int val3)
{
    return syscall(__NR_futex, addr, op, val, to, addr2, val3);
}

static int vlc_futex_wake(void *addr, int nr)
{
    return sys_futex(addr, FUTEX_WAKE_PRIVATE, nr, NULL, NULL, 0);
}

static int vlc_futex_wait(void *addr, unsigned val, const struct timespec *to)
{
    return sys_futex(addr, FUTEX_WAIT_PRIVATE, val, to, NULL, 0);
}

void vlc_addr_signal(void *addr)
{
    vlc_futex_wake(addr, 1);
}

void vlc_addr_broadcast(void *addr)
{
    vlc_futex_wake(addr, INT_MAX);
}

void vlc_addr_wait(void *addr, unsigned val)
{
    vlc_futex_wait(addr, val, NULL);
}

bool vlc_addr_timedwait(void *addr, unsigned val, mtime_t delay)
{
    lldiv_t d = lldiv(delay, CLOCK_FREQ);
    struct timespec ts = { d.quot, d.rem * (1000000000 / CLOCK_FREQ) };

    return (vlc_futex_wait(addr, val, &ts) == 0 || errno != ETIMEDOUT);
}
