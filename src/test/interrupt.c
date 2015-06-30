/*****************************************************************************
 * interrupt.c: Test for interrupt context API
 *****************************************************************************
 * Copyright (C) 2015 Rémi Denis-Courmont
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

#undef NDEBUG
#include <assert.h>
#include <errno.h>
#include <unistd.h>

#include <vlc_common.h>
#include <vlc_threads.h>
#include <vlc_interrupt.h>

static vlc_sem_t sem;

static void test_context_simple(vlc_interrupt_t *ctx)
{
    vlc_interrupt_t *octx;

    vlc_interrupt_set(ctx);
    octx = vlc_interrupt_set(NULL);
    assert(octx == ctx);
    octx = vlc_interrupt_set(ctx);
    assert(octx == NULL);
    octx = vlc_interrupt_set(ctx);
    assert(octx == ctx);

    /* BIG FAT WARNING: This is only meant to test the vlc_cond_wait_i11e()
     * function. This is NOT a good example of how to use the function in
     * normal code. */
    vlc_sem_post(&sem);
    assert(vlc_sem_wait_i11e(&sem) == 0);

    vlc_interrupt_raise(ctx);
    assert(vlc_sem_wait_i11e(&sem) == EINTR);

    vlc_sem_post(&sem);
    vlc_interrupt_raise(ctx);
    assert(vlc_sem_wait_i11e(&sem) == EINTR);
    assert(vlc_sem_wait_i11e(&sem) == 0);

    vlc_interrupt_raise(ctx);
    vlc_sem_post(&sem);
    assert(vlc_sem_wait_i11e(&sem) == EINTR);
    assert(vlc_sem_wait_i11e(&sem) == 0);

    octx = vlc_interrupt_set(NULL);
    assert(octx == ctx);
    octx = vlc_interrupt_set(NULL);
    assert(octx == NULL);
}

static void *test_thread_simple(void *data)
{
    vlc_interrupt_t *ctx = data;

    vlc_interrupt_set(ctx);
    assert(vlc_sem_wait_i11e(&sem) == EINTR);
    assert(vlc_sem_wait_i11e(&sem) == 0);
    vlc_sem_wait(&sem);

    test_context_simple(ctx);
    return NULL;
}

static void *test_thread_cleanup(void *data)
{
    vlc_interrupt_t *ctx = data;

    test_context_simple(ctx);
    /* Test context clearing on exit */
    vlc_interrupt_set(ctx);
    return NULL;
}

static void *test_thread_cancel(void *data)
{
    vlc_interrupt_t *ctx = data;

    test_context_simple(ctx);
    /* Test context clearing on cancellation */
    vlc_interrupt_set(ctx);
    for (;;)
        pause();

    vlc_assert_unreachable();
}

int main (void)
{
    vlc_interrupt_t *ctx;
    vlc_thread_t th;

    alarm(2);

    ctx = vlc_interrupt_create();
    assert(ctx != NULL);
    vlc_interrupt_destroy(ctx);

    vlc_sem_init(&sem, 0);
    ctx = vlc_interrupt_create();
    assert(ctx != NULL);

    test_context_simple(ctx);

    assert(!vlc_clone(&th, test_thread_simple, ctx, VLC_THREAD_PRIORITY_LOW));
    vlc_interrupt_raise(ctx);
    vlc_sem_post(&sem);
    vlc_sem_post(&sem);
    vlc_join(th, NULL);

    assert(!vlc_clone(&th, test_thread_cleanup, ctx, VLC_THREAD_PRIORITY_LOW));
    vlc_join(th, NULL);

    assert(!vlc_clone(&th, test_thread_cancel, ctx, VLC_THREAD_PRIORITY_LOW));
    vlc_cancel(th);
    vlc_join(th, NULL);

    vlc_interrupt_destroy(ctx);
    vlc_sem_destroy(&sem);
    return 0;
}
