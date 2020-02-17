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

static void wait_bucket_leave(void *data)
{
    struct wait_bucket *bucket = data;

    bucket->waiters--;
    pthread_mutex_unlock(&bucket->lock);
}

void vlc_atomic_wait(void *addr, unsigned value)
{
    atomic_uint *futex = addr;
    struct wait_bucket *bucket = wait_bucket_enter(futex);

    pthread_cleanup_push(wait_bucket_leave, bucket);

    if (value == atomic_load_explicit(futex, memory_order_relaxed))
        pthread_cond_wait(&bucket->wait, &bucket->lock);

    pthread_cleanup_pop(1);
}

int vlc_atomic_timedwait(void *addr, unsigned value, vlc_tick_t deadline)
{
    atomic_uint *futex = addr;
    struct wait_bucket *bucket;
    struct timespec ts;
    vlc_tick_t delay = deadline - vlc_tick_now();
    lldiv_t d = lldiv((delay >= 0) ? delay : 0, CLOCK_FREQ);
    int ret = 0;

    /* TODO: use monotonic clock directly */
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += d.quot;
    ts.tv_nsec += NS_FROM_VLC_TICK(d.rem);

    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }

    bucket = wait_bucket_enter(futex);
    pthread_cleanup_push(wait_bucket_leave, bucket);

    if (value == atomic_load_explicit(futex, memory_order_relaxed))
        ret = pthread_cond_timedwait(&bucket->wait, &bucket->lock, &ts);

    pthread_cleanup_pop(1);
    return ret;
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
