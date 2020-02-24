/*****************************************************************************
 * thread.c : android pthread back-end for LibVLC
 *****************************************************************************
 * Copyright (C) 1999-2016 VLC authors and VideoLAN
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Clément Sténac
 *          Rémi Denis-Courmont
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

#include "libvlc.h"
#include <signal.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdnoreturn.h>
#include <time.h>
#include <assert.h>

#include <sys/types.h>
#include <unistd.h> /* fsync() */
#include <pthread.h>
#include <sched.h>

/* debug */

#ifndef NDEBUG
static void
vlc_thread_fatal_print (const char *action, int error,
                        const char *function, const char *file, unsigned line)
{
    const char *msg = vlc_strerror_c(error);
    fprintf(stderr, "LibVLC fatal error %s (%d) in thread %lu "
            "at %s:%u in %s\n Error message: %s\n",
            action, error, vlc_thread_id (), file, line, function, msg);
    fflush (stderr);
}

# define VLC_THREAD_ASSERT( action ) do { \
    if (unlikely(val)) { \
        vlc_thread_fatal_print (action, val, __func__, __FILE__, __LINE__); \
        assert (!action); \
    } \
} while(0)
#else
# define VLC_THREAD_ASSERT( action ) ((void)val)
#endif

struct vlc_thread
{
    pthread_t      thread;

    void *(*entry)(void*);
    void *data;

    struct
    {
        atomic_uint *addr; /// Non-null if waiting on futex
        vlc_mutex_t lock ; /// Protects futex address
    } wait;

    atomic_bool killed;
    bool killable;
};

static thread_local struct vlc_thread *thread = NULL;

void vlc_threads_setup (libvlc_int_t *p_libvlc)
{
    (void)p_libvlc;
}

/* pthread */
static void clean_detached_thread(void *data)
{
    struct vlc_thread *th = data;

    /* release thread handle */
    free(th);
}

static void *detached_thread(void *data)
{
    vlc_thread_t th = data;

    thread = th;

    vlc_cleanup_push(clean_detached_thread, th);
    th->entry(th->data);
    vlc_cleanup_pop();
    clean_detached_thread(th);
    return NULL;
}

static void *joinable_thread(void *data)
{
    vlc_thread_t th = data;

    thread = th;
    return th->entry(th->data);
}

static int vlc_clone_attr (vlc_thread_t *th, void *(*entry) (void *),
                           void *data, bool detach)
{
    vlc_thread_t thread = malloc (sizeof (*thread));
    if (unlikely(thread == NULL))
        return ENOMEM;

    int ret;

    sigset_t oldset;
    {
        sigset_t set;
        sigemptyset (&set);
        sigdelset (&set, SIGHUP);
        sigaddset (&set, SIGINT);
        sigaddset (&set, SIGQUIT);
        sigaddset (&set, SIGTERM);

        sigaddset (&set, SIGPIPE); /* We don't want this one, really! */
        pthread_sigmask (SIG_BLOCK, &set, &oldset);
    }

    atomic_store(&thread->killed, false);
    thread->killable = true;
    thread->entry = entry;
    thread->data = data;
    thread->wait.addr = NULL;
    vlc_mutex_init(&thread->wait.lock);

    pthread_attr_t attr;
    pthread_attr_init (&attr);
    pthread_attr_setdetachstate (&attr, detach ? PTHREAD_CREATE_DETACHED
                                               : PTHREAD_CREATE_JOINABLE);

    ret = pthread_create (&thread->thread, &attr,
                          detach ? detached_thread : joinable_thread, thread);
    pthread_attr_destroy (&attr);

    pthread_sigmask (SIG_SETMASK, &oldset, NULL);
    *th = thread;
    return ret;
}

int vlc_clone (vlc_thread_t *th, void *(*entry) (void *), void *data,
               int priority)
{
    (void) priority;
    return vlc_clone_attr (th, entry, data, false);
}

void vlc_join (vlc_thread_t handle, void **result)
{
    int val = pthread_join (handle->thread, result);
    VLC_THREAD_ASSERT ("joining thread");
    clean_detached_thread(handle);
}

int vlc_clone_detach (vlc_thread_t *th, void *(*entry) (void *), void *data,
                      int priority)
{
    vlc_thread_t dummy;
    if (th == NULL)
        th = &dummy;

    (void) priority;
    return vlc_clone_attr (th, entry, data, true);
}

int vlc_set_priority (vlc_thread_t th, int priority)
{
    (void) th; (void) priority;
    return VLC_SUCCESS;
}

void vlc_cancel (vlc_thread_t thread_id)
{
    atomic_uint *addr;

    atomic_store(&thread_id->killed, true);

    vlc_mutex_lock(&thread_id->wait.lock);
    addr = thread_id->wait.addr;
    if (addr != NULL)
    {
        atomic_fetch_or_explicit(addr, 1, memory_order_relaxed);
        vlc_atomic_notify_all(addr);
    }
    vlc_mutex_unlock(&thread_id->wait.lock);
}

int vlc_savecancel (void)
{
    if (!thread) /* not created by VLC, can't be cancelled */
        return true;

    int oldstate = thread->killable;
    thread->killable = false;
    return oldstate;
}

void vlc_restorecancel (int state)
{
    if (!thread) /* not created by VLC, can't be cancelled */
        return;

    thread->killable = state;
}

void vlc_testcancel (void)
{
    if (!thread) /* not created by VLC, can't be cancelled */
        return;
    if (!thread->killable)
        return;
    if (!atomic_load(&thread->killed))
        return;

    pthread_exit(NULL);
}

void vlc_cancel_addr_set(atomic_uint *addr)
{
    vlc_thread_t th = thread;
    if (th == NULL)
        return;

    vlc_mutex_lock(&th->wait.lock);
    assert(th->wait.addr == NULL);
    th->wait.addr = addr;
    vlc_mutex_unlock(&th->wait.lock);
}

void vlc_cancel_addr_clear(atomic_uint *addr)
{
    vlc_thread_t th = thread;
    if (th == NULL)
        return;

    vlc_mutex_lock(&th->wait.lock);
    assert(th->wait.addr == addr);
    th->wait.addr = NULL;
    (void) addr;
    vlc_mutex_unlock(&th->wait.lock);
}

/* threadvar */

int vlc_threadvar_create (vlc_threadvar_t *key, void (*destr) (void *))
{
    return pthread_key_create (key, destr);
}

void vlc_threadvar_delete (vlc_threadvar_t *p_tls)
{
    pthread_key_delete (*p_tls);
}

int vlc_threadvar_set (vlc_threadvar_t key, void *value)
{
    return pthread_setspecific (key, value);
}

void *vlc_threadvar_get (vlc_threadvar_t key)
{
    return pthread_getspecific (key);
}

/* time */
vlc_tick_t vlc_tick_now (void)
{
    struct timespec ts;

    if (unlikely(clock_gettime (CLOCK_MONOTONIC, &ts) != 0))
        abort ();

    return vlc_tick_from_timespec( &ts );
}

/* cpu */

unsigned vlc_GetCPUCount(void)
{
    return sysconf(_SC_NPROCESSORS_ONLN);
}
