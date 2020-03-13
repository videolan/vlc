/*****************************************************************************
 * clock_nanosleep.c: Test for clock_nanosleep implementation
 *****************************************************************************
 * Copyright © 2020 VideoLAN & VLC authors
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
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

#include "config.h"

#include <stdbool.h>
#include <unistd.h>
#undef NDEBUG
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <sys/errno.h>

#define VLC_NSEC_PER_SEC 1000000000ull

#define VLC_TEST(id, flags, tp, eret, eerr)                  \
do {                                                         \
    assert( clock_nanosleep(id, flags, tp, NULL) == eret );  \
    if (eret != 0) assert( eerr == errno );                  \
} while (0)

// Thread entry function for cancellation test
static void *waiting_thread_entry(void *unused)
{
    (void)unused;

    struct timespec ts = { 5, 0 };
    assert(clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL) == 0);
    assert(0);
    return NULL;
}

int main(void)
{
    alarm(1);

    // Invalid timespec (negative)
    {
        struct timespec inv_ts = { -1, -1 };
        VLC_TEST(CLOCK_MONOTONIC, 0,             &inv_ts, -1, EINVAL);
        VLC_TEST(CLOCK_MONOTONIC, TIMER_ABSTIME, &inv_ts, -1, EINVAL);
        VLC_TEST(CLOCK_REALTIME,  0,             &inv_ts, -1, EINVAL);
        VLC_TEST(CLOCK_REALTIME,  TIMER_ABSTIME, &inv_ts, -1, EINVAL);
    }

    // Invalid timespec (nsec out of range)
    {
        struct timespec inv_ts = { -1, VLC_NSEC_PER_SEC };
        VLC_TEST(CLOCK_MONOTONIC, 0,             &inv_ts, -1, EINVAL);
        VLC_TEST(CLOCK_MONOTONIC, TIMER_ABSTIME, &inv_ts, -1, EINVAL);
        VLC_TEST(CLOCK_REALTIME,  0,             &inv_ts, -1, EINVAL);
        VLC_TEST(CLOCK_REALTIME,  TIMER_ABSTIME, &inv_ts, -1, EINVAL);
    }

    // Valid timespec (0 sec, 0 nsec)
    {
        struct timespec ts = { 0, 0 };
        VLC_TEST(CLOCK_MONOTONIC, 0,             &ts, 0, 0);
        VLC_TEST(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, 0, 0);
        VLC_TEST(CLOCK_REALTIME,  0,             &ts, 0, 0);
        VLC_TEST(CLOCK_REALTIME,  TIMER_ABSTIME, &ts, 0, 0);
    }

    // Valid timespec (wait for nearly one second each)
    {
        alarm(3);
        struct timespec ts = { 0, VLC_NSEC_PER_SEC - 1 };
        VLC_TEST(CLOCK_MONOTONIC, 0,             &ts, 0, 0);
        VLC_TEST(CLOCK_REALTIME,  0,             &ts, 0, 0);
    }

    // Absolute monotonic clock waiting (wait for one second)
    {
        alarm(3);
        struct timespec ts, ts_now;

        clock_gettime(CLOCK_MONOTONIC, &ts);
        ts.tv_sec += 1;

        VLC_TEST(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, 0, 0);

        clock_gettime(CLOCK_MONOTONIC, &ts_now);
        assert(ts.tv_sec <= ts_now.tv_sec);
    }

    // Absolute realtime clock in the past
    {
        alarm(1);
        struct timespec ts = { 10, 10 };
        VLC_TEST(CLOCK_REALTIME,  TIMER_ABSTIME, &ts, 0, 0);
    }

    // Test thread cancellation
    {
        alarm(1);
        pthread_t th;
        pthread_create(&th, NULL, waiting_thread_entry, NULL);
        pthread_cancel(th);
        pthread_join(th, NULL);
    }

}
