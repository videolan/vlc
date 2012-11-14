/*****************************************************************************
 * thread.c : android pthread back-end for LibVLC
 *****************************************************************************
 * Copyright (C) 1999-2012 VLC authors and VideoLAN
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

#include <android/log.h>
#include <sys/syscall.h> /* __NR_gettid */

/* helper */
static struct timespec mtime_to_ts (mtime_t date)
{
    lldiv_t d = lldiv (date, CLOCK_FREQ);
    struct timespec ts = { d.quot, d.rem * (1000000000 / CLOCK_FREQ) };

    return ts;
}

/* debug */
#define vlc_assert(x) do { \
    if (unlikely(!x)) { \
    __android_log_print(ANDROID_LOG_ERROR, "vlc", "assert failed %s:%d: %s", \
        __FILE__, __LINE__, #x \
        ); \
        abort(); \
    } \
} while(0)

#ifndef NDEBUG
static void
vlc_thread_fatal (const char *action, int error,
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

    __android_log_print(ANDROID_LOG_ERROR, "vlc",
        "LibVLC fatal error %s (%d) in thread %d "
        "at %s:%u in %s\n Error message: %s\n",
        action, error, syscall (__NR_gettid), file, line, function, msg);

    abort ();
}

# define VLC_THREAD_ASSERT( action ) \
    if (unlikely(val)) \
        vlc_thread_fatal (action, val, __func__, __FILE__, __LINE__)
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
    vlc_assert (pthread_mutex_lock (p_mutex) == EDEADLK);
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
    pthread_cond_t *cond; /// Non-null if thread waiting on cond
    vlc_mutex_t    lock ; /// Protects cond
    vlc_sem_t      finished;

    void *(*entry)(void*);
    void *data;

    vlc_atomic_t killed;
    bool killable;
};

static __thread struct vlc_thread *thread = NULL;

void vlc_threads_setup (libvlc_int_t *p_libvlc)
{
    (void)p_libvlc;
}

/* cond */

void vlc_cond_init (vlc_cond_t *condvar)
{
    if (unlikely(pthread_cond_init (&condvar->cond, NULL)))
        abort ();
    condvar->clock = CLOCK_MONOTONIC;
}

void vlc_cond_init_daytime (vlc_cond_t *condvar)
{
    if (unlikely(pthread_cond_init (&condvar->cond, NULL)))
        abort ();
    condvar->clock = CLOCK_REALTIME;
}

void vlc_cond_destroy (vlc_cond_t *condvar)
{
    int val = pthread_cond_destroy (&condvar->cond);
    VLC_THREAD_ASSERT ("destroying condition");
}

void vlc_cond_signal (vlc_cond_t *condvar)
{
    int val = pthread_cond_signal (&condvar->cond);
    VLC_THREAD_ASSERT ("signaling condition variable");
}

void vlc_cond_broadcast (vlc_cond_t *condvar)
{
    pthread_cond_broadcast (&condvar->cond);
}

void vlc_cond_wait (vlc_cond_t *condvar, vlc_mutex_t *p_mutex)
{
    vlc_thread_t th = thread;

    if (th != NULL)
    {
        vlc_testcancel ();
        if (vlc_mutex_trylock (&th->lock) == 0)
        {
            th->cond = &condvar->cond;
            vlc_mutex_unlock (&th->lock);
        }
        else
        {   /* The lock is already held by another thread.
             * => That other thread has just cancelled this one. */
            vlc_testcancel ();
            /* Cancellation did not occur even though this thread is cancelled.
             * => Cancellation is disabled. */
            th = NULL;
        }
    }

    int val = pthread_cond_wait (&condvar->cond, p_mutex);
    VLC_THREAD_ASSERT ("waiting on condition");

    if (th != NULL)
    {
        if (vlc_mutex_trylock (&th->lock) == 0)
        {
            thread->cond = NULL;
            vlc_mutex_unlock (&th->lock);
        }
        /* Else: This thread was cancelled and is cancellable.
                 vlc_testcancel() will take of it right there: */
        vlc_testcancel();
    }
}

int vlc_cond_timedwait (vlc_cond_t *condvar, vlc_mutex_t *p_mutex,
                        mtime_t deadline)
{
    struct timespec ts = mtime_to_ts (deadline);
    vlc_thread_t th = thread;
    int (*cb)(pthread_cond_t *, pthread_mutex_t *, const struct timespec *);

    if (th != NULL)
    {
        vlc_testcancel ();
        if (vlc_mutex_trylock (&th->lock) == 0)
        {
            th->cond = &condvar->cond;
            vlc_mutex_unlock (&th->lock);
        }
        else
        {   /* The lock is already held by another thread.
             * => That other thread has just cancelled this one. */
            vlc_testcancel ();
            /* Cancellation did not occur even though this thread is cancelled.
             * => Cancellation is disabled. */
            th = NULL;
        }
    }

    switch (condvar->clock)
    {
         case CLOCK_REALTIME:
             cb = pthread_cond_timedwait;
             break;
         case CLOCK_MONOTONIC:
             cb = pthread_cond_timedwait_monotonic_np;
             break;
         default:
             assert (0);
    }

    int val = cb (&condvar->cond, p_mutex, &ts);
    if (val != ETIMEDOUT)
        VLC_THREAD_ASSERT ("timed-waiting on condition");

    if (th != NULL)
    {
        if (vlc_mutex_trylock (&th->lock) == 0)
        {
            thread->cond = NULL;
            vlc_mutex_unlock (&th->lock);
        }
        /* Else: This thread was cancelled and is cancellable.
                 vlc_testcancel() will take of it right there: */
        vlc_testcancel();
    }
    return val;
}

/* pthread */
static void clean_detached_thread(void *data)
{
    struct vlc_thread *thread = data;

    /* release thread handle */
    vlc_mutex_destroy(&thread->lock);
    free(thread);
}

static void *detached_thread(void *data)
{
    vlc_thread_t th = data;

    thread = th;

    vlc_cleanup_push(clean_detached_thread, data);
    th->entry(th->data);
    vlc_cleanup_run();

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
    vlc_cleanup_run();

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


    vlc_sem_init(&thread->finished, 0);
    vlc_atomic_set(&thread->killed, false);
    thread->killable = true;
    thread->cond = NULL;
    thread->entry = entry;
    thread->data = data;
    vlc_mutex_init(&thread->lock);

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
    vlc_mutex_destroy(&handle->lock);
    free(handle);
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
    pthread_cond_t *cond;

    vlc_atomic_set(&thread_id->killed, true);

    vlc_mutex_lock(&thread_id->lock);
    cond = thread_id->cond;
    if (cond)
        pthread_cond_broadcast(cond);
    vlc_mutex_unlock(&thread_id->lock);
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
    if (!vlc_atomic_get(&thread->killed))
        return;

    pthread_exit(NULL);
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

#undef mwait
void mwait (mtime_t deadline)
{
    vlc_mutex_t lock;
    vlc_cond_t wait;

    vlc_mutex_init (&lock);
    vlc_cond_init (&wait);

    vlc_mutex_lock (&lock);
    mutex_cleanup_push (&lock);
    while (!vlc_cond_timedwait (&wait, &lock, deadline));
    vlc_cleanup_run ();

    vlc_cond_destroy (&wait);
    vlc_mutex_destroy (&lock);
}

#undef msleep
void msleep (mtime_t delay)
{
    mwait (mdate () + delay);
}

/* cpu */

unsigned vlc_GetCPUCount(void)
{
    return sysconf(_SC_NPROCESSORS_CONF);
}
