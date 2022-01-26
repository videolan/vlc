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

#include <assert.h>
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
#define FUTEX_WAIT_BITSET_PRIVATE FUTEX_WAIT_BITSET
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
    /* The futex Linux kernel system call exists in two variants:
     * - the original one for use with long time_t, which suffers from the
     *   year 2038 problem on 32-bit architectures,
     * - the "time64" one for use with 64-bit time_t, which lacks backward
     *   binary compatibility with 32-bit long time_t on 32-bit architectures.
     */
    static_assert (sizeof (time_t) == sizeof (long) || sizeof (time_t) == 8,
                   "Unrecognised time_t type definition");

#if !defined (__NR_futex)
    /* Recent 32-bit platforms (e.g. riscv32) only support 64-bit time_t. */
    static_assert (sizeof (time_t) == 8, "Expected 64-bit time_t");
    const long num = __NR_futex_time64;
#elif !defined (__NR_futex_time64)
    static_assert (sizeof (time_t) == sizeof (long), "Expected long time_t");
    const long num = __NR_futex;
#else
    const long num = sizeof (time_t) == sizeof (long)
                     ? __NR_futex : __NR_futex_time64;
#endif
    return syscall(num, addr, op, val, to, addr2, val3);
}

static int vlc_futex_wake(void *addr, int nr)
{
    return sys_futex(addr, FUTEX_WAKE_PRIVATE, nr, NULL, NULL, 0);
}

static int vlc_futex_wait(void *addr, unsigned flags,
                          unsigned val, const struct timespec *to)
{
    return sys_futex(addr, FUTEX_WAIT_BITSET_PRIVATE | flags, val, to, NULL,
                     FUTEX_BITSET_MATCH_ANY);
}

void vlc_atomic_notify_one(void *addr)
{
    vlc_futex_wake(addr, 1);
}

void vlc_atomic_notify_all(void *addr)
{
    vlc_futex_wake(addr, INT_MAX);
}

void vlc_atomic_wait(void *addr, unsigned val)
{
    vlc_futex_wait(addr, 0, val, NULL);
}

int vlc_atomic_timedwait(void *addr, unsigned val, vlc_tick_t deadline)
{
    struct timespec ts = timespec_from_vlc_tick(deadline);

    if (vlc_futex_wait(addr, 0, val, &ts) == 0)
        return 0;

    switch (errno) {
        case EINTR:
        case EAGAIN:
            return 0;
        case EFAULT:
        case EINVAL:
            vlc_assert_unreachable(); /* BUG! */
        default:
            break;
     }
     return errno;
}

int vlc_atomic_timedwait_daytime(void *addr, unsigned val, time_t deadline)
{
    struct timespec ts = { .tv_sec = deadline, .tv_nsec = 0 };

    if (vlc_futex_wait(addr, FUTEX_CLOCK_REALTIME, val, &ts) == 0)
        return 0;

    switch (errno) {
        case EINTR:
        case EAGAIN:
            return 0;
        case EFAULT:
        case EINVAL:
            vlc_assert_unreachable(); /* BUG! */
        default:
            break;
     }
     return errno;
}
