/*****************************************************************************
 * wait.c : pthread back-end for vlc_atomic_wait
 *****************************************************************************
 * Copyright (C) 2019-2020 Rémi Denis-Courmont
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

#include <vlc_common.h>

#include <stdalign.h>
#include <stdatomic.h>
#include <errno.h>
#include <time.h>

#include <sys/types.h>
#include <pthread.h>

static clockid_t vlc_clock_id = CLOCK_REALTIME;

#define WAIT_BUCKET_INIT \
     { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0 }
#define WAIT_BUCKET_INIT_2 WAIT_BUCKET_INIT, WAIT_BUCKET_INIT
#define WAIT_BUCKET_INIT_4 WAIT_BUCKET_INIT_2, WAIT_BUCKET_INIT_2
#define WAIT_BUCKET_INIT_8 WAIT_BUCKET_INIT_4, WAIT_BUCKET_INIT_4
#define WAIT_BUCKET_INIT_16 WAIT_BUCKET_INIT_8, WAIT_BUCKET_INIT_8
#define WAIT_BUCKET_INIT_32 WAIT_BUCKET_INIT_16, WAIT_BUCKET_INIT_16

static struct wait_bucket
{
    pthread_mutex_t lock;
    pthread_cond_t wait;
    unsigned waiters;
} wait_buckets[32] = { WAIT_BUCKET_INIT_32 };

static struct wait_bucket *wait_bucket_get(atomic_uint *addr)
{
    uintptr_t u = (uintptr_t)addr;
    size_t idx = (u / alignof (*addr)) % ARRAY_SIZE(wait_buckets);

    return &wait_buckets[idx];
}

static struct wait_bucket *wait_bucket_enter(atomic_uint *addr)
{
    struct wait_bucket *bucket = wait_bucket_get(addr);

    pthread_mutex_lock(&bucket->lock);
    bucket->waiters++;
    return bucket;
}

static void wait_bucket_leave(struct wait_bucket *bucket)
{
    bucket->waiters--;
    pthread_mutex_unlock(&bucket->lock);
}

void vlc_atomic_wait(void *addr, unsigned value)
{
    atomic_uint *futex = addr;
    struct wait_bucket *bucket = wait_bucket_enter(futex);

    if (value == atomic_load_explicit(futex, memory_order_relaxed)) {
        int canc;

        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &canc);
        pthread_cond_wait(&bucket->wait, &bucket->lock);
        pthread_setcancelstate(canc, NULL);
    }

    wait_bucket_leave(bucket);
}

static int vlc_atomic_timedwait_timespec(void *addr, unsigned value,
                                         const struct timespec *restrict ts)
{
    atomic_uint *futex = addr;
    struct wait_bucket *bucket = wait_bucket_enter(futex);
    int ret = 0;

    if (value == atomic_load_explicit(futex, memory_order_relaxed)) {
        int canc;

        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &canc);
        ret = pthread_cond_timedwait(&bucket->wait, &bucket->lock, ts);
        pthread_setcancelstate(canc, NULL);
    }

    wait_bucket_leave(bucket);
    return ret;
}

static void vlc_timespec_adjust(clockid_t cid, struct timespec *restrict ts)
{
    struct timespec now_from, now_to;
    lldiv_t d;

    if (vlc_clock_id == cid)
        return;

    clock_gettime(cid, &now_from);
    clock_gettime(vlc_clock_id, &now_to);

    d = lldiv((ts->tv_sec - now_from.tv_sec + now_to.tv_sec) * 1000000000LL
              + ts->tv_nsec - now_from.tv_nsec + now_to.tv_nsec, 1000000000LL);

    ts->tv_sec = d.quot;
    ts->tv_nsec = d.rem;
}

int vlc_atomic_timedwait(void *addr, unsigned value, vlc_tick_t deadline)
{
    struct timespec ts = timespec_from_vlc_tick(deadline);

    vlc_timespec_adjust(CLOCK_MONOTONIC, &ts);
    return vlc_atomic_timedwait_timespec(addr, value, &ts);
}

int vlc_atomic_timedwait_daytime(void *addr, unsigned value, time_t deadline)
{
    struct timespec ts = { .tv_sec = deadline, .tv_nsec = 0 };

    vlc_timespec_adjust(CLOCK_REALTIME, &ts);
    return vlc_atomic_timedwait_timespec(addr, value, &ts);
}

void vlc_atomic_notify_one(void *addr)
{
    vlc_atomic_notify_all(addr);
}

void vlc_atomic_notify_all(void *addr)
{
    struct wait_bucket *bucket = wait_bucket_get(addr);

    pthread_mutex_lock(&bucket->lock);
    if (bucket->waiters > 0)
        pthread_cond_broadcast(&bucket->wait);
    pthread_mutex_unlock(&bucket->lock);
}

#ifdef __ELF__
__attribute__((constructor))
static void vlc_atomic_clock_select(void)
{
    pthread_condattr_t attr;

    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);

    for (size_t i = 0; i < ARRAY_SIZE(wait_buckets); i++)
        pthread_cond_init(&wait_buckets[i].wait, &attr);

    pthread_condattr_destroy(&attr);
    vlc_clock_id = CLOCK_MONOTONIC;
}

__attribute__((destructor))
static void vlc_atomic_clock_deselect(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(wait_buckets); i++)
        pthread_cond_destroy(&wait_buckets[i].wait);
}
#endif
