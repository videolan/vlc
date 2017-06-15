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
#include <vlc_network.h>

static vlc_sem_t sem;
static int fds[2];

static void interrupt_callback(void *data)
{
    vlc_sem_post(data);
}

static void test_context_simple(vlc_interrupt_t *ctx)
{
    vlc_interrupt_t *octx;
    char c;

    vlc_interrupt_set(ctx);
    octx = vlc_interrupt_set(NULL);
    assert(octx == ctx);
    octx = vlc_interrupt_set(ctx);
    assert(octx == NULL);
    octx = vlc_interrupt_set(ctx);
    assert(octx == ctx);

    vlc_interrupt_register(interrupt_callback, &sem);
    vlc_interrupt_raise(ctx);
    vlc_sem_wait(&sem);
    vlc_interrupt_unregister();

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

    assert(vlc_mwait_i11e(1) == 0);
    vlc_interrupt_raise(ctx);
    assert(vlc_mwait_i11e(CLOCK_FREQ * 10000000) == EINTR);

    assert(vlc_poll_i11e(NULL, 0, 1) == 0);
    vlc_interrupt_raise(ctx);
    assert(vlc_poll_i11e(NULL, 0, 1000000000) == -1);
    assert(errno == EINTR);

    c = 12;
    assert(vlc_write_i11e(fds[0], &c, 1) == 1);
    c = 0;
    assert(vlc_read_i11e(fds[1], &c, 1) == 1 && c == 12);
    vlc_interrupt_raise(ctx);
    assert(vlc_read_i11e(fds[1], &c, 1) == -1);
    assert(errno == EINTR);

    c = 42;
    assert(vlc_sendto_i11e(fds[0], &c, 1, 0, NULL, 0) == 1);
    c = 0;
    assert(vlc_recvfrom_i11e(fds[1], &c, 1, 0, NULL, 0) == 1 && c == 42);
    vlc_interrupt_raise(ctx);
    assert(vlc_recvfrom_i11e(fds[1], &c, 1, 0, NULL, 0) == -1);
    assert(errno == EINTR);

    vlc_interrupt_raise(ctx);
    assert(vlc_accept_i11e(fds[1], NULL, NULL, true) < 0);

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

    int canc = vlc_savecancel();
    test_context_simple(ctx);
    vlc_restorecancel(canc);

    /* Test context clearing on cancellation */
    vlc_interrupt_set(ctx);
    for (;;)
        pause();

    vlc_assert_unreachable();
}

static void unreachable_callback(void *data)
{
    (void) data;
    abort();
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

    assert(vlc_socketpair(PF_LOCAL, SOCK_STREAM, 0, fds, false) == 0);

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

    /* Tests without interrupt context */
    vlc_sem_post(&sem);
    assert(vlc_sem_wait_i11e(&sem) == 0);
    assert(vlc_mwait_i11e(1) == 0);
    assert(vlc_poll_i11e(NULL, 0, 1) == 0);

    vlc_interrupt_register(unreachable_callback, NULL);
    vlc_interrupt_unregister();

    void *data[2];
    vlc_interrupt_forward_start(ctx, data);
    assert(vlc_interrupt_forward_stop(data) == 0);

    vlc_close(fds[1]);
    vlc_close(fds[0]);
    vlc_sem_destroy(&sem);
    return 0;
}
