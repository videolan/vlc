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
#include <vlc_atomic.h>

#include "libvlc.h"
#include <signal.h>
#include <errno.h>
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
    char buf[1000];
    const char *msg;

    switch (strerror_r (error, buf, sizeof (buf)))
    {
        case 0:
            msg = buf;
            break;
        case ERANGE: /* should never happen */
            msg = "unknown (too big to display)";
            break;
        default:
            msg = "unknown (invalid error number)";
            break;
    }

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

/* mutexes */
void vlc_mutex_init( vlc_mutex_t *p_mutex )
{
    pthread_mutexattr_t attr;

    pthread_mutexattr_init (&attr);
#ifdef NDEBUG
    pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_DEFAULT);
#else
    pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_ERRORCHECK);
#endif
    pthread_mutex_init (p_mutex, &attr);
    pthread_mutexattr_destroy( &attr );
}

void vlc_mutex_init_recursive( vlc_mutex_t *p_mutex )
{
    pthread_mutexattr_t attr;

    pthread_mutexattr_init (&attr);
    pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init (p_mutex, &attr);
    pthread_mutexattr_destroy( &attr );
}


void vlc_mutex_destroy (vlc_mutex_t *p_mutex)
{
    int val = pthread_mutex_destroy( p_mutex );
    VLC_THREAD_ASSERT ("destroying mutex");
}

#ifndef NDEBUG
void vlc_assert_locked (vlc_mutex_t *p_mutex)
{
    assert (pthread_mutex_lock (p_mutex) == EDEADLK);
}
#endif

void vlc_mutex_lock (vlc_mutex_t *p_mutex)
{
    int val = pthread_mutex_lock( p_mutex );
    VLC_THREAD_ASSERT ("locking mutex");
}

int vlc_mutex_trylock (vlc_mutex_t *p_mutex)
{
    int val = pthread_mutex_trylock( p_mutex );

    if (val != EBUSY)
        VLC_THREAD_ASSERT ("locking mutex");
    return val;
}

void vlc_mutex_unlock (vlc_mutex_t *p_mutex)
{
    int val = pthread_mutex_unlock( p_mutex );
    VLC_THREAD_ASSERT ("unlocking mutex");
}

struct vlc_thread
{
    pthread_t      thread;
    vlc_sem_t      finished;

    void *(*entry)(void*);
    void *data;

    struct
    {
        void *addr; /// Non-null if waiting on futex
        vlc_mutex_t lock ; /// Protects futex address
    } wait;

    atomic_bool killed;
    bool killable;
};

static thread_local struct vlc_thread *thread = NULL;

vlc_thread_t vlc_thread_self (void)
{
    return thread;
}

void vlc_threads_setup (libvlc_int_t *p_libvlc)
{
    (void)p_libvlc;
}

/* pthread */
static void clean_detached_thread(void *data)
{
    struct vlc_thread *th = data;

    /* release thread handle */
    vlc_mutex_destroy(&th->wait.lock);
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

static void finish_joinable_thread(void *data)
{
    vlc_thread_t th = data;

    vlc_sem_post(&th->finished);
}

static void *joinable_thread(void *data)
{
    vlc_thread_t th = data;
    void *ret;

    vlc_cleanup_push(finish_joinable_thread, th);
    thread = th;
    ret = th->entry(th->data);
    vlc_cleanup_pop();
    vlc_sem_post(&th->finished);

    return ret;
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

    if (!detach)
        vlc_sem_init(&thread->finished, 0);
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
    vlc_sem_wait (&handle->finished);
    vlc_sem_destroy (&handle->finished);

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
    atomic_int *addr;

    atomic_store(&thread_id->killed, true);

    vlc_mutex_lock(&thread_id->wait.lock);
    addr = thread_id->wait.addr;
    if (addr != NULL)
    {
        atomic_fetch_or_explicit(addr, 1, memory_order_relaxed);
        vlc_addr_broadcast(addr);
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

void vlc_control_cancel(int cmd, ...)
{
    vlc_thread_t th = vlc_thread_self();
    va_list ap;

    if (th == NULL)
        return;

    va_start(ap, cmd);
    switch (cmd)
    {
        case VLC_CANCEL_ADDR_SET:
        {
            void *addr = va_arg(ap, void *);

            vlc_mutex_lock(&th->wait.lock);
            assert(th->wait.addr == NULL);
            th->wait.addr = addr;
            vlc_mutex_unlock(&th->wait.lock);
            break;
        }

        case VLC_CANCEL_ADDR_CLEAR:
        {
            void *addr = va_arg(ap, void *);

            vlc_mutex_lock(&th->wait.lock);
            assert(th->wait.addr == addr);
            th->wait.addr = NULL;
            (void) addr;
            vlc_mutex_unlock(&th->wait.lock);
            break;
        }

        default:
            vlc_assert_unreachable ();
    }
    va_end(ap);
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
mtime_t mdate (void)
{
    struct timespec ts;

    if (unlikely(clock_gettime (CLOCK_MONOTONIC, &ts) != 0))
        abort ();

    return (INT64_C(1000000) * ts.tv_sec) + (ts.tv_nsec / 1000);
}

/* cpu */

unsigned vlc_GetCPUCount(void)
{
    return sysconf(_SC_NPROCESSORS_ONLN);
}
