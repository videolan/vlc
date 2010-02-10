/*****************************************************************************
 * thread.c : Win32 back-end for LibVLC
 *****************************************************************************
 * Copyright (C) 1999-2009 the VideoLAN team
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Clément Sténac
 *          Rémi Denis-Courmont
 *          Pierre Ynard
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include "libvlc.h"
#include <stdarg.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>
#ifdef UNDER_CE
# include <mmsystem.h>
#endif

static vlc_threadvar_t cancel_key;

/**
 * Per-thread cancellation data
 */
typedef struct vlc_cancel_t
{
    vlc_cleanup_t *cleaners;
#ifdef UNDER_CE
    HANDLE         cancel_event;
#endif
    bool           killable;
    bool           killed;
} vlc_cancel_t;

#ifndef UNDER_CE
# define VLC_CANCEL_INIT { NULL, true, false }
#else
# define VLC_CANCEL_INIT { NULL, NULL, true, false }
#endif

#ifdef UNDER_CE
static void CALLBACK vlc_cancel_self (ULONG_PTR dummy);

static DWORD vlc_cancelable_wait (DWORD count, const HANDLE *handles,
                                  DWORD delay)
{
    vlc_cancel_t *nfo = vlc_threadvar_get (cancel_key);
    if (nfo == NULL)
    {
        /* Main thread - cannot be cancelled anyway */
        return WaitForMultipleObjects (count, handles, FALSE, delay);
    }
    HANDLE new_handles[count + 1];
    memcpy(new_handles, handles, count * sizeof(HANDLE));
    new_handles[count] = nfo->cancel_event;
    DWORD result = WaitForMultipleObjects (count + 1, new_handles, FALSE,
                                           delay);
    if (result == WAIT_OBJECT_0 + count)
    {
        vlc_cancel_self (NULL);
        return WAIT_IO_COMPLETION;
    }
    else
    {
        return result;
    }
}

DWORD SleepEx (DWORD dwMilliseconds, BOOL bAlertable)
{
    if (bAlertable)
    {
        DWORD result = vlc_cancelable_wait (0, NULL, dwMilliseconds);
        return (result == WAIT_TIMEOUT) ? 0 : WAIT_IO_COMPLETION;
    }
    else
    {
        Sleep(dwMilliseconds);
        return 0;
    }
}

DWORD WaitForSingleObjectEx (HANDLE hHandle, DWORD dwMilliseconds,
                             BOOL bAlertable)
{
    if (bAlertable)
    {
        /* The MSDN documentation specifies different return codes,
         * but in practice they are the same. We just check that it
         * remains so. */
#if WAIT_ABANDONED != WAIT_ABANDONED_0
# error Windows headers changed, code needs to be rewritten!
#endif
        return vlc_cancelable_wait (1, &hHandle, dwMilliseconds);
    }
    else
    {
        return WaitForSingleObject (hHandle, dwMilliseconds);
    }
}

DWORD WaitForMultipleObjectsEx (DWORD nCount, const HANDLE *lpHandles,
                                BOOL bWaitAll, DWORD dwMilliseconds,
                                BOOL bAlertable)
{
    if (bAlertable)
    {
        /* We do not support the bWaitAll case */
        assert (! bWaitAll);
        return vlc_cancelable_wait (nCount, lpHandles, dwMilliseconds);
    }
    else
    {
        return WaitForMultipleObjects (nCount, lpHandles, bWaitAll,
                                       dwMilliseconds);
    }
}
#endif

vlc_mutex_t super_mutex;
vlc_cond_t  super_variable;

BOOL WINAPI DllMain (HINSTANCE hinstDll, DWORD fdwReason, LPVOID lpvReserved)
{
    (void) hinstDll;
    (void) lpvReserved;

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            vlc_mutex_init (&super_mutex);
            vlc_cond_init (&super_variable);
            vlc_threadvar_create (&cancel_key, free);
            break;

        case DLL_PROCESS_DETACH:
            vlc_threadvar_delete( &cancel_key );
            vlc_cond_destroy (&super_variable);
            vlc_mutex_destroy (&super_mutex);
            break;
    }
    return TRUE;
}

/*** Mutexes ***/
void vlc_mutex_init( vlc_mutex_t *p_mutex )
{
    /* This creates a recursive mutex. This is OK as fast mutexes have
     * no defined behavior in case of recursive locking. */
    InitializeCriticalSection (&p_mutex->mutex);
    p_mutex->dynamic = true;
}

void vlc_mutex_init_recursive( vlc_mutex_t *p_mutex )
{
    InitializeCriticalSection( &p_mutex->mutex );
    p_mutex->dynamic = true;
}


void vlc_mutex_destroy (vlc_mutex_t *p_mutex)
{
    assert (p_mutex->dynamic);
    DeleteCriticalSection (&p_mutex->mutex);
}

void vlc_mutex_lock (vlc_mutex_t *p_mutex)
{
    if (!p_mutex->dynamic)
    {   /* static mutexes */
        int canc = vlc_savecancel ();
        assert (p_mutex != &super_mutex); /* this one cannot be static */

        vlc_mutex_lock (&super_mutex);
        while (p_mutex->locked)
        {
            p_mutex->contention++;
            vlc_cond_wait (&super_variable, &super_mutex);
            p_mutex->contention--;
        }
        p_mutex->locked = true;
        vlc_mutex_unlock (&super_mutex);
        vlc_restorecancel (canc);
        return;
    }

    EnterCriticalSection (&p_mutex->mutex);
}

int vlc_mutex_trylock (vlc_mutex_t *p_mutex)
{
    if (!p_mutex->dynamic)
    {   /* static mutexes */
        int ret = EBUSY;

        assert (p_mutex != &super_mutex); /* this one cannot be static */
        vlc_mutex_lock (&super_mutex);
        if (!p_mutex->locked)
        {
            p_mutex->locked = true;
            ret = 0;
        }
        vlc_mutex_unlock (&super_mutex);
        return ret;
    }

    return TryEnterCriticalSection (&p_mutex->mutex) ? 0 : EBUSY;
}

void vlc_mutex_unlock (vlc_mutex_t *p_mutex)
{
    if (!p_mutex->dynamic)
    {   /* static mutexes */
        assert (p_mutex != &super_mutex); /* this one cannot be static */

        vlc_mutex_lock (&super_mutex);
        assert (p_mutex->locked);
        p_mutex->locked = false;
        if (p_mutex->contention)
            vlc_cond_broadcast (&super_variable);
        vlc_mutex_unlock (&super_mutex);
        return;
    }

    LeaveCriticalSection (&p_mutex->mutex);
}

/*** Condition variables ***/
enum
{
    CLOCK_MONOTONIC,
    CLOCK_REALTIME,
};

static void vlc_cond_init_common (vlc_cond_t *p_condvar, unsigned clock)
{
    /* Create a manual-reset event (manual reset is needed for broadcast). */
    p_condvar->handle = CreateEvent (NULL, TRUE, FALSE, NULL);
    if (!p_condvar->handle)
        abort();
    p_condvar->clock = clock;
}

void vlc_cond_init (vlc_cond_t *p_condvar)
{
    vlc_cond_init_common (p_condvar, CLOCK_MONOTONIC);
}

void vlc_cond_init_daytime (vlc_cond_t *p_condvar)
{
    vlc_cond_init_common (p_condvar, CLOCK_REALTIME);
}

void vlc_cond_destroy (vlc_cond_t *p_condvar)
{
    CloseHandle (p_condvar->handle);
}

void vlc_cond_signal (vlc_cond_t *p_condvar)
{
    /* NOTE: This will cause a broadcast, that is wrong.
     * This will also wake up the next waiting thread if no threads are yet
     * waiting, which is also wrong. However both of these issues are allowed
     * by the provision for spurious wakeups. Better have too many wakeups
     * than too few (= deadlocks). */
    SetEvent (p_condvar->handle);
}

void vlc_cond_broadcast (vlc_cond_t *p_condvar)
{
    SetEvent (p_condvar->handle);
}

void vlc_cond_wait (vlc_cond_t *p_condvar, vlc_mutex_t *p_mutex)
{
    DWORD result;

    assert (p_mutex->dynamic); /* TODO */
    do
    {
        vlc_testcancel ();
        LeaveCriticalSection (&p_mutex->mutex);
        result = WaitForSingleObjectEx (p_condvar->handle, INFINITE, TRUE);
        EnterCriticalSection (&p_mutex->mutex);
    }
    while (result == WAIT_IO_COMPLETION);

    assert (result != WAIT_ABANDONED); /* another thread failed to cleanup! */
    assert (result != WAIT_FAILED);
    ResetEvent (p_condvar->handle);
}

int vlc_cond_timedwait (vlc_cond_t *p_condvar, vlc_mutex_t *p_mutex,
                        mtime_t deadline)
{
    DWORD result;

    assert (p_mutex->dynamic); /* TODO */
    do
    {
        vlc_testcancel ();

        mtime_t total;
        switch (p_condvar->clock)
        {
            case CLOCK_MONOTONIC:
                total = mdate();
                break;
            case CLOCK_REALTIME: /* FIXME? sub-second precision */
                total = CLOCK_FREQ * time (NULL);
                break;
            default:
                assert (0);
        }
        total = (deadline - total) / 1000;
        if( total < 0 )
            total = 0;

        DWORD delay = (total > 0x7fffffff) ? 0x7fffffff : total;
        LeaveCriticalSection (&p_mutex->mutex);
        result = WaitForSingleObjectEx (p_condvar->handle, delay, TRUE);
        EnterCriticalSection (&p_mutex->mutex);
    }
    while (result == WAIT_IO_COMPLETION);

    assert (result != WAIT_ABANDONED);
    assert (result != WAIT_FAILED);
    ResetEvent (p_condvar->handle);

    return (result == WAIT_OBJECT_0) ? 0 : ETIMEDOUT;
}

/*** Semaphore ***/
void vlc_sem_init (vlc_sem_t *sem, unsigned value)
{
    *sem = CreateSemaphore (NULL, value, 0x7fffffff, NULL);
    if (*sem == NULL)
        abort ();
}

void vlc_sem_destroy (vlc_sem_t *sem)
{
    CloseHandle (*sem);
}

int vlc_sem_post (vlc_sem_t *sem)
{
    ReleaseSemaphore (*sem, 1, NULL);
    return 0; /* FIXME */
}

void vlc_sem_wait (vlc_sem_t *sem)
{
    DWORD result;

    do
    {
        vlc_testcancel ();
        result = WaitForSingleObjectEx (*sem, INFINITE, TRUE);
    }
    while (result == WAIT_IO_COMPLETION);
}

/*** Read/write locks */
/* SRW (Slim Read Write) locks are available in Vista+ only */
void vlc_rwlock_init (vlc_rwlock_t *lock)
{
    vlc_mutex_init (&lock->mutex);
    vlc_cond_init (&lock->read_wait);
    vlc_cond_init (&lock->write_wait);
    lock->readers = 0; /* active readers */
    lock->writers = 0; /* waiting or active writers */
    lock->writer = 0; /* ID of active writer */
}

/**
 * Destroys an initialized unused read/write lock.
 */
void vlc_rwlock_destroy (vlc_rwlock_t *lock)
{
    vlc_cond_destroy (&lock->read_wait);
    vlc_cond_destroy (&lock->write_wait);
    vlc_mutex_destroy (&lock->mutex);
}

/**
 * Acquires a read/write lock for reading. Recursion is allowed.
 */
void vlc_rwlock_rdlock (vlc_rwlock_t *lock)
{
    vlc_mutex_lock (&lock->mutex);
    while (lock->writer != 0)
        vlc_cond_wait (&lock->read_wait, &lock->mutex);
    if (lock->readers == ULONG_MAX)
        abort ();
    lock->readers++;
    vlc_mutex_unlock (&lock->mutex);
}

/**
 * Acquires a read/write lock for writing. Recursion is not allowed.
 */
void vlc_rwlock_wrlock (vlc_rwlock_t *lock)
{
    vlc_mutex_lock (&lock->mutex);
    if (lock->writers == ULONG_MAX)
        abort ();
    lock->writers++;
    while ((lock->readers > 0) || (lock->writer != 0))
        vlc_cond_wait (&lock->write_wait, &lock->mutex);
    lock->writers--;
    lock->writer = GetCurrentThreadId ();
    vlc_mutex_unlock (&lock->mutex);
}

/**
 * Releases a read/write lock.
 */
void vlc_rwlock_unlock (vlc_rwlock_t *lock)
{
    vlc_mutex_lock (&lock->mutex);
    if (lock->readers > 0)
        lock->readers--; /* Read unlock */
    else
        lock->writer = 0; /* Write unlock */

    if (lock->writers > 0)
    {
        if (lock->readers == 0)
            vlc_cond_signal (&lock->write_wait);
    }
    else
        vlc_cond_broadcast (&lock->read_wait);
    vlc_mutex_unlock (&lock->mutex);
}

/*** Thread-specific variables (TLS) ***/
int vlc_threadvar_create (vlc_threadvar_t *p_tls, void (*destr) (void *))
{
#warning FIXME: use destr() callback and stop leaking!
    VLC_UNUSED( destr );

    *p_tls = TlsAlloc();
    return (*p_tls == TLS_OUT_OF_INDEXES) ? EAGAIN : 0;
}

void vlc_threadvar_delete (vlc_threadvar_t *p_tls)
{
    TlsFree (*p_tls);
}

/**
 * Sets a thread-local variable.
 * @param key thread-local variable key (created with vlc_threadvar_create())
 * @param value new value for the variable for the calling thread
 * @return 0 on success, a system error code otherwise.
 */
int vlc_threadvar_set (vlc_threadvar_t key, void *value)
{
    return TlsSetValue (key, value) ? ENOMEM : 0;
}

/**
 * Gets the value of a thread-local variable for the calling thread.
 * This function cannot fail.
 * @return the value associated with the given variable for the calling
 * or NULL if there is no value.
 */
void *vlc_threadvar_get (vlc_threadvar_t key)
{
    return TlsGetValue (key);
}


/*** Threads ***/
void vlc_threads_setup (libvlc_int_t *p_libvlc)
{
    (void) p_libvlc;
}

struct vlc_entry_data
{
    void * (*func) (void *);
    void *  data;
    vlc_sem_t ready;
#ifdef UNDER_CE
    HANDLE  cancel_event;
#endif
};

static unsigned __stdcall vlc_entry (void *p)
{
    struct vlc_entry_data *entry = p;
    vlc_cancel_t cancel_data = VLC_CANCEL_INIT;
    void *(*func) (void *) = entry->func;
    void *data = entry->data;

#ifdef UNDER_CE
    cancel_data.cancel_event = entry->cancel_event;
#endif
    vlc_threadvar_set (cancel_key, &cancel_data);
    vlc_sem_post (&entry->ready);
    func (data);
    return 0;
}

int vlc_clone (vlc_thread_t *p_handle, void * (*entry) (void *), void *data,
               int priority)
{
    int err = ENOMEM;
    HANDLE hThread;

    struct vlc_entry_data *entry_data = malloc (sizeof (*entry_data));
    if (entry_data == NULL)
        return ENOMEM;
    entry_data->func = entry;
    entry_data->data = data;
    vlc_sem_init (&entry_data->ready, 0);

#ifndef UNDER_CE
    /* When using the MSVCRT C library you have to use the _beginthreadex
     * function instead of CreateThread, otherwise you'll end up with
     * memory leaks and the signal functions not working (see Microsoft
     * Knowledge Base, article 104641) */
    hThread = (HANDLE)(uintptr_t)
        _beginthreadex (NULL, 0, vlc_entry, entry_data, CREATE_SUSPENDED, NULL);
    if (! hThread)
    {
        err = errno;
        goto error;
    }

    *p_handle = hThread;

#else
    vlc_thread_t th = malloc (sizeof (*th));
    if (th == NULL)
        goto error;
    th->cancel_event = CreateEvent (NULL, FALSE, FALSE, NULL);
    if (th->cancel_event == NULL)
    {
        free (th);
        goto error;
    }
    entry_data->cancel_event = th->cancel_event;

    /* Not sure if CREATE_SUSPENDED + ResumeThread() is any useful on WinCE.
     * Thread handles act up, too. */
    th->handle = CreateThread (NULL, 128*1024, vlc_entry, entry_data,
                               CREATE_SUSPENDED, NULL);
    if (th->handle == NULL)
    {
        CloseHandle (th->cancel_event);
        free (th);
        goto error;
    }

    *p_handle = th;
    hThread = th->handle;

#endif

    ResumeThread (hThread);
    if (priority)
        SetThreadPriority (hThread, priority);

    /* Prevent cancellation until cancel_data is initialized. */
    /* XXX: This could be postponed to vlc_cancel() or avoided completely by
     * passing the "right" pointer to vlc_cancel_self(). */
    vlc_sem_wait (&entry_data->ready);
    vlc_sem_destroy (&entry_data->ready);
    free (entry_data);

    return 0;

error:
    vlc_sem_destroy (&entry_data->ready);
    free (entry_data);
    return err;
}

void vlc_join (vlc_thread_t handle, void **result)
{
#ifdef UNDER_CE
# define handle handle->handle
#endif
    do
        vlc_testcancel ();
    while (WaitForSingleObjectEx (handle, INFINITE, TRUE)
                                                        == WAIT_IO_COMPLETION);

    CloseHandle (handle);
    assert (result == NULL); /* <- FIXME if ever needed */
#ifdef UNDER_CE
# undef handle
    CloseHandle (handle->cancel_event);
    free (handle);
#endif
}

void vlc_detach (vlc_thread_t handle)
{
#ifndef UNDER_CE
    CloseHandle (handle);
#else
    /* FIXME: handle->cancel_event leak */
    CloseHandle (handle->handle);
    free (handle);
#endif
}

/*** Thread cancellation ***/

/* APC procedure for thread cancellation */
static void CALLBACK vlc_cancel_self (ULONG_PTR dummy)
{
    vlc_cancel_t *nfo = vlc_threadvar_get (cancel_key);

    if (likely(nfo != NULL))
        nfo->killed = true;

    (void)dummy;
}

void vlc_cancel (vlc_thread_t thread_id)
{
#ifndef UNDER_CE
    QueueUserAPC (vlc_cancel_self, thread_id, 0);
#else
    SetEvent (thread_id->cancel_event);
#endif
}

int vlc_savecancel (void)
{
    int state;

    vlc_cancel_t *nfo = vlc_threadvar_get (cancel_key);
    if (nfo == NULL)
        return false; /* Main thread - cannot be cancelled anyway */

    state = nfo->killable;
    nfo->killable = false;
    return state;
}

void vlc_restorecancel (int state)
{
    vlc_cancel_t *nfo = vlc_threadvar_get (cancel_key);
    assert (state == false || state == true);

    if (nfo == NULL)
        return; /* Main thread - cannot be cancelled anyway */

    assert (!nfo->killable);
    nfo->killable = state != 0;
}

void vlc_testcancel (void)
{
    vlc_cancel_t *nfo = vlc_threadvar_get (cancel_key);
    if (nfo == NULL)
        return; /* Main thread - cannot be cancelled anyway */

    if (nfo->killable && nfo->killed)
    {
        for (vlc_cleanup_t *p = nfo->cleaners; p != NULL; p = p->next)
             p->proc (p->data);
#ifndef UNDER_CE
        _endthreadex(0);
#else
        ExitThread(0);
#endif
    }
}

void vlc_control_cancel (int cmd, ...)
{
    /* NOTE: This function only modifies thread-specific data, so there is no
     * need to lock anything. */
    va_list ap;

    vlc_cancel_t *nfo = vlc_threadvar_get (cancel_key);
    if (nfo == NULL)
        return; /* Main thread - cannot be cancelled anyway */

    va_start (ap, cmd);
    switch (cmd)
    {
        case VLC_CLEANUP_PUSH:
        {
            /* cleaner is a pointer to the caller stack, no need to allocate
             * and copy anything. As a nice side effect, this cannot fail. */
            vlc_cleanup_t *cleaner = va_arg (ap, vlc_cleanup_t *);
            cleaner->next = nfo->cleaners;
            nfo->cleaners = cleaner;
            break;
        }

        case VLC_CLEANUP_POP:
        {
            nfo->cleaners = nfo->cleaners->next;
            break;
        }
    }
    va_end (ap);
}


/*** Timers ***/
struct vlc_timer
{
#ifndef UNDER_CE
    HANDLE handle;
#else
    unsigned id;
    unsigned interval;
#endif
    void (*func) (void *);
    void *data;
};

#ifndef UNDER_CE
static void CALLBACK vlc_timer_do (void *val, BOOLEAN timeout)
{
    struct vlc_timer *timer = val;

    assert (timeout);
    timer->func (timer->data);
}
#else
static void CALLBACK vlc_timer_do (unsigned timer_id, unsigned msg,
                                   DWORD_PTR user, DWORD_PTR unused1,
                                   DWORD_PTR unused2)
{
    struct vlc_timer *timer = (struct vlc_timer *) user;
    assert (timer_id == timer->id);
    (void) msg;
    (void) unused1;
    (void) unused2;

    timer->func (timer->data);

    if (timer->interval)
    {
        mtime_t interval = timer->interval * 1000;
        vlc_timer_schedule (timer, false, interval, interval);
    }
}
#endif

int vlc_timer_create (vlc_timer_t *id, void (*func) (void *), void *data)
{
    struct vlc_timer *timer = malloc (sizeof (*timer));

    if (timer == NULL)
        return ENOMEM;
    timer->func = func;
    timer->data = data;
#ifndef UNDER_CE
    timer->handle = INVALID_HANDLE_VALUE;
#else
    timer->id = 0;
    timer->interval = 0;
#endif
    *id = timer;
    return 0;
}

void vlc_timer_destroy (vlc_timer_t timer)
{
#ifndef UNDER_CE
    if (timer->handle != INVALID_HANDLE_VALUE)
        DeleteTimerQueueTimer (NULL, timer->handle, INVALID_HANDLE_VALUE);
#else
    if (timer->id)
        timeKillEvent (timer->id);
    /* FIXME: timers that have not yet completed will trigger use-after-free */
#endif
    free (timer);
}

void vlc_timer_schedule (vlc_timer_t timer, bool absolute,
                         mtime_t value, mtime_t interval)
{
#ifndef UNDER_CE
    if (timer->handle != INVALID_HANDLE_VALUE)
    {
        DeleteTimerQueueTimer (NULL, timer->handle, NULL);
        timer->handle = INVALID_HANDLE_VALUE;
    }
#else
    if (timer->id)
    {
        timeKillEvent (timer->id);
        timer->id = 0;
        timer->interval = 0;
    }
#endif
    if (value == 0)
        return; /* Disarm */

    if (absolute)
        value -= mdate ();
    value = (value + 999) / 1000;
    interval = (interval + 999) / 1000;

#ifndef UNDER_CE
    if (!CreateTimerQueueTimer (&timer->handle, NULL, vlc_timer_do, timer,
                                value, interval, WT_EXECUTEDEFAULT))
#else
    TIMECAPS caps;
    timeGetDevCaps (&caps, sizeof(caps));

    unsigned delay = value;
    delay = __MAX(delay, caps.wPeriodMin);
    delay = __MIN(delay, caps.wPeriodMax);

    unsigned event = TIME_ONESHOT;

    if (interval == delay)
        event = TIME_PERIODIC;
    else if (interval)
        timer->interval = interval;

    timer->id = timeSetEvent (delay, delay / 20, vlc_timer_do, (DWORD) timer,
                              event);
    if (!timer->id)
#endif
        abort ();
}

unsigned vlc_timer_getoverrun (vlc_timer_t timer)
{
    (void)timer;
    return 0;
}
