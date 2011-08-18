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
#include "config/configuration.h"

static vlc_threadvar_t thread_key;

/**
 * Per-thread data
 */
struct vlc_thread
{
    HANDLE         id;
#ifdef UNDER_CE
    HANDLE         cancel_event;
#endif

    bool           detached;
    bool           killable;
    bool           killed;
    vlc_cleanup_t *cleaners;

    void        *(*entry) (void *);
    void          *data;
};

static CRITICAL_SECTION super_mutex;
static HANDLE           super_cond;

BOOL WINAPI DllMain (HINSTANCE, DWORD, LPVOID);

BOOL WINAPI DllMain (HINSTANCE hinstDll, DWORD fdwReason, LPVOID lpvReserved)
{
    (void) hinstDll;
    (void) lpvReserved;

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            super_cond = CreateEvent (NULL, TRUE, FALSE, NULL);
            if (unlikely(!super_cond))
                return FALSE;
            InitializeCriticalSection (&super_mutex);
            vlc_threadvar_create (&thread_key, NULL);
            vlc_rwlock_init (&config_lock);
            break;

        case DLL_PROCESS_DETACH:
            vlc_rwlock_destroy (&config_lock);
            vlc_threadvar_delete (&thread_key);
            DeleteCriticalSection (&super_mutex);
            CloseHandle (super_cond);
            break;
    }
    return TRUE;
}

static void CALLBACK vlc_cancel_self (ULONG_PTR);

static DWORD vlc_WaitForMultipleObjects (DWORD count, const HANDLE *handles,
                                         DWORD delay)
{
    DWORD ret;
#ifdef UNDER_CE
    HANDLE buf[count + 1];

    struct vlc_thread *th = vlc_threadvar_get (thread_key);
    if (th != NULL)
    {
        memcpy (buf, handles, count * sizeof(HANDLE));
        buf[count++] = th->cancel_event;
        handles = buf;
    }

    if (count == 0)
    {
         Sleep (delay);
         ret = WAIT_TIMEOUT;
    }
    else
        ret = WaitForMultipleObjects (count, handles, FALSE, delay);

    if ((th != NULL) && (ret == WAIT_OBJECT_0 + count - 1))
    {
        vlc_cancel_self ((uintptr_t)th);
        ret = WAIT_IO_COMPLETION;
    }
#else
    if (count == 0)
    {
        ret = SleepEx (delay, TRUE);
        if (ret == 0)
            ret = WAIT_TIMEOUT;
    }
    else
        ret = WaitForMultipleObjectsEx (count, handles, FALSE, delay, TRUE);
#endif
    /* We do not abandon objects... this would be a bug */
    assert (ret < WAIT_ABANDONED_0 || WAIT_ABANDONED_0 + count - 1 < ret);

    if (unlikely(ret == WAIT_FAILED))
        abort (); /* We are screwed! */
    return ret;
}

static DWORD vlc_WaitForSingleObject (HANDLE handle, DWORD delay)
{
    return vlc_WaitForMultipleObjects (1, &handle, delay);
}

static DWORD vlc_Sleep (DWORD delay)
{
    DWORD ret = vlc_WaitForMultipleObjects (0, NULL, delay);
    return (ret != WAIT_TIMEOUT) ? ret : 0;
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
    {   /* static mutexes (inefficient on Windows) */
        EnterCriticalSection (&super_mutex);
        while (p_mutex->locked)
        {
            p_mutex->contention++;
            LeaveCriticalSection (&super_mutex);
            WaitForSingleObject (super_cond, INFINITE);
            EnterCriticalSection (&super_mutex);
            assert (p_mutex->contention > 0);
            p_mutex->contention--;
        }
        p_mutex->locked = true;
        LeaveCriticalSection (&super_mutex);
        return;
    }

    EnterCriticalSection (&p_mutex->mutex);
}

int vlc_mutex_trylock (vlc_mutex_t *p_mutex)
{
    if (!p_mutex->dynamic)
    {   /* static mutexes */
        int ret = EBUSY;

        EnterCriticalSection (&super_mutex);
        if (!p_mutex->locked)
        {
            p_mutex->locked = true;
            ret = 0;
        }
        LeaveCriticalSection (&super_mutex);
        return ret;
    }

    return TryEnterCriticalSection (&p_mutex->mutex) ? 0 : EBUSY;
}

void vlc_mutex_unlock (vlc_mutex_t *p_mutex)
{
    if (!p_mutex->dynamic)
    {   /* static mutexes */
        EnterCriticalSection (&super_mutex);
        assert (p_mutex->locked);
        p_mutex->locked = false;
        if (p_mutex->contention > 0)
            SetEvent (super_cond);
        LeaveCriticalSection (&super_mutex);
        return;
    }

    LeaveCriticalSection (&p_mutex->mutex);
}

/*** Condition variables ***/
enum
{
    CLOCK_REALTIME=0, /* must be zero for VLC_STATIC_COND */
    CLOCK_MONOTONIC,
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
    if (!p_condvar->handle)
        return;

    /* This is suboptimal but works. */
    vlc_cond_broadcast (p_condvar);
}

void vlc_cond_broadcast (vlc_cond_t *p_condvar)
{
    if (!p_condvar->handle)
        return;

    /* Wake all threads up (as the event HANDLE has manual reset) */
    SetEvent (p_condvar->handle);
}

void vlc_cond_wait (vlc_cond_t *p_condvar, vlc_mutex_t *p_mutex)
{
    DWORD result;

    if (!p_condvar->handle)
    {   /* FIXME FIXME FIXME */
        msleep (50000);
        return;
    }

    do
    {
        vlc_testcancel ();
        vlc_mutex_unlock (p_mutex);
        result = vlc_WaitForSingleObject (p_condvar->handle, INFINITE);
        vlc_mutex_lock (p_mutex);
    }
    while (result == WAIT_IO_COMPLETION);

    ResetEvent (p_condvar->handle);
}

int vlc_cond_timedwait (vlc_cond_t *p_condvar, vlc_mutex_t *p_mutex,
                        mtime_t deadline)
{
    DWORD result;

    if (!p_condvar->handle)
    {   /* FIXME FIXME FIXME */
        msleep (50000);
        return 0;
    }

    do
    {
        vlc_testcancel ();

        mtime_t total;
        switch (p_condvar->clock)
        {
            case CLOCK_REALTIME: /* FIXME? sub-second precision */
                total = CLOCK_FREQ * time (NULL);
                break;
            default:
                assert (p_condvar->clock == CLOCK_MONOTONIC);
                total = mdate();
                break;
        }
        total = (deadline - total) / 1000;
        if( total < 0 )
            total = 0;

        DWORD delay = (total > 0x7fffffff) ? 0x7fffffff : total;
        vlc_mutex_unlock (p_mutex);
        result = vlc_WaitForSingleObject (p_condvar->handle, delay);
        vlc_mutex_lock (p_mutex);
    }
    while (result == WAIT_IO_COMPLETION);

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
        result = vlc_WaitForSingleObject (*sem, INFINITE);
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
    lock->writers = 0; /* waiting writers */
    lock->writer = 0; /* ID of active writer */
}

void vlc_rwlock_destroy (vlc_rwlock_t *lock)
{
    vlc_cond_destroy (&lock->read_wait);
    vlc_cond_destroy (&lock->write_wait);
    vlc_mutex_destroy (&lock->mutex);
}

void vlc_rwlock_rdlock (vlc_rwlock_t *lock)
{
    vlc_mutex_lock (&lock->mutex);
    /* Recursive read-locking is allowed. With the infos available:
     *  - the loosest possible condition (no active writer) is:
     *     (lock->writer != 0)
     *  - the strictest possible condition is:
     *     (lock->writer != 0 || (lock->readers == 0 && lock->writers > 0))
     *  or (lock->readers == 0 && (lock->writer != 0 || lock->writers > 0))
     */
    while (lock->writer != 0)
    {
        assert (lock->readers == 0);
        vlc_cond_wait (&lock->read_wait, &lock->mutex);
    }
    if (unlikely(lock->readers == ULONG_MAX))
        abort ();
    lock->readers++;
    vlc_mutex_unlock (&lock->mutex);
}

static void vlc_rwlock_rdunlock (vlc_rwlock_t *lock)
{
    vlc_mutex_lock (&lock->mutex);
    assert (lock->readers > 0);

    /* If there are no readers left, wake up a writer. */
    if (--lock->readers == 0 && lock->writers > 0)
        vlc_cond_signal (&lock->write_wait);
    vlc_mutex_unlock (&lock->mutex);
}

void vlc_rwlock_wrlock (vlc_rwlock_t *lock)
{
    vlc_mutex_lock (&lock->mutex);
    if (unlikely(lock->writers == ULONG_MAX))
        abort ();
    lock->writers++;
    /* Wait until nobody owns the lock in either way. */
    while ((lock->readers > 0) || (lock->writer != 0))
        vlc_cond_wait (&lock->write_wait, &lock->mutex);
    lock->writers--;
    assert (lock->writer == 0);
    lock->writer = GetCurrentThreadId ();
    vlc_mutex_unlock (&lock->mutex);
}

static void vlc_rwlock_wrunlock (vlc_rwlock_t *lock)
{
    vlc_mutex_lock (&lock->mutex);
    assert (lock->writer == GetCurrentThreadId ());
    assert (lock->readers == 0);
    lock->writer = 0; /* Write unlock */

    /* Let reader and writer compete. Scheduler decides who wins. */
    if (lock->writers > 0)
        vlc_cond_signal (&lock->write_wait);
    vlc_cond_broadcast (&lock->read_wait);
    vlc_mutex_unlock (&lock->mutex);
}

void vlc_rwlock_unlock (vlc_rwlock_t *lock)
{
    /* Note: If the lock is held for reading, lock->writer is nul.
     * If the lock is held for writing, only this thread can store a value to
     * lock->writer. Either way, lock->writer is safe to fetch here. */
    if (lock->writer != 0)
        vlc_rwlock_wrunlock (lock);
    else
        vlc_rwlock_rdunlock (lock);
}

/*** Thread-specific variables (TLS) ***/
struct vlc_threadvar
{
    DWORD                 id;
    void                (*destroy) (void *);
    struct vlc_threadvar *prev;
    struct vlc_threadvar *next;
} *vlc_threadvar_last = NULL;

int vlc_threadvar_create (vlc_threadvar_t *p_tls, void (*destr) (void *))
{
    struct vlc_threadvar *var = malloc (sizeof (*var));
    if (unlikely(var == NULL))
        return errno;

    var->id = TlsAlloc();
    if (var->id == TLS_OUT_OF_INDEXES)
    {
        free (var);
        return EAGAIN;
    }
    var->destroy = destr;
    var->next = NULL;
    *p_tls = var;

    EnterCriticalSection (&super_mutex);
    var->prev = vlc_threadvar_last;
    vlc_threadvar_last = var;
    LeaveCriticalSection (&super_mutex);
    return 0;
}

void vlc_threadvar_delete (vlc_threadvar_t *p_tls)
{
    struct vlc_threadvar *var = *p_tls;

    EnterCriticalSection (&super_mutex);
    if (var->prev != NULL)
        var->prev->next = var->next;
    else
        vlc_threadvar_last = var->next;
    if (var->next != NULL)
        var->next->prev = var->prev;
    LeaveCriticalSection (&super_mutex);

    TlsFree (var->id);
    free (var);
}

int vlc_threadvar_set (vlc_threadvar_t key, void *value)
{
    return TlsSetValue (key->id, value) ? ENOMEM : 0;
}

void *vlc_threadvar_get (vlc_threadvar_t key)
{
    return TlsGetValue (key->id);
}

/*** Threads ***/
void vlc_threads_setup (libvlc_int_t *p_libvlc)
{
    (void) p_libvlc;
}

static void vlc_thread_cleanup (struct vlc_thread *th)
{
    vlc_threadvar_t key;

retry:
    /* TODO: use RW lock or something similar */
    EnterCriticalSection (&super_mutex);
    for (key = vlc_threadvar_last; key != NULL; key = key->prev)
    {
        void *value = vlc_threadvar_get (key);
        if (value != NULL && key->destroy != NULL)
        {
            EnterCriticalSection (&super_mutex);
            vlc_threadvar_set (key, NULL);
            key->destroy (value);
            goto retry;
        }
    }
    EnterCriticalSection (&super_mutex);

    if (th->detached)
    {
        CloseHandle (th->id);
#ifdef UNDER_CE
        CloseHandle (th->cancel_event);
#endif
        free (th);
    }
}

static unsigned __stdcall vlc_entry (void *p)
{
    struct vlc_thread *th = p;

    vlc_threadvar_set (thread_key, th);
    th->killable = true;
    th->data = th->entry (th->data);
    vlc_thread_cleanup (th);
    return 0;
}

static int vlc_clone_attr (vlc_thread_t *p_handle, bool detached,
                           void *(*entry) (void *), void *data, int priority)
{
    struct vlc_thread *th = malloc (sizeof (*th));
    if (unlikely(th == NULL))
        return ENOMEM;
    th->entry = entry;
    th->data = data;
    th->detached = detached;
    th->killable = false; /* not until vlc_entry() ! */
    th->killed = false;
    th->cleaners = NULL;

    HANDLE hThread;
#ifndef UNDER_CE
    /* When using the MSVCRT C library you have to use the _beginthreadex
     * function instead of CreateThread, otherwise you'll end up with
     * memory leaks and the signal functions not working (see Microsoft
     * Knowledge Base, article 104641) */
    uintptr_t h;

    h = _beginthreadex (NULL, 0, vlc_entry, th, CREATE_SUSPENDED, NULL);
    if (h == 0)
    {
        int err = errno;
        free (th);
        return err;
    }
    hThread = (HANDLE)h;

#else
    th->cancel_event = CreateEvent (NULL, FALSE, FALSE, NULL);
    if (th->cancel_event == NULL)
    {
        free (th);
        return ENOMEM;
    }

    /* Not sure if CREATE_SUSPENDED + ResumeThread() is any useful on WinCE.
     * Thread handles act up, too. */
    hThread = CreateThread (NULL, 128*1024, vlc_entry, th,
                            CREATE_SUSPENDED, NULL);
    if (hThread == NULL)
    {
        CloseHandle (th->cancel_event);
        free (th);
        return ENOMEM;
    }

#endif
    /* Thread is suspended, so we can safely set th->id */
    th->id = hThread;
    if (p_handle != NULL)
        *p_handle = th;

    if (priority)
        SetThreadPriority (hThread, priority);

    ResumeThread (hThread);

    return 0;
}

int vlc_clone (vlc_thread_t *p_handle, void *(*entry) (void *),
                void *data, int priority)
{
    return vlc_clone_attr (p_handle, false, entry, data, priority);
}

void vlc_join (vlc_thread_t th, void **result)
{
    do
        vlc_testcancel ();
    while (vlc_WaitForSingleObject (th->id, INFINITE)
                                                        == WAIT_IO_COMPLETION);

    if (result != NULL)
        *result = th->data;
    CloseHandle (th->id);
#ifdef UNDER_CE
    CloseHandle (th->cancel_event);
#endif
    free (th);
}

int vlc_clone_detach (vlc_thread_t *p_handle, void *(*entry) (void *),
                      void *data, int priority)
{
    vlc_thread_t th;
    if (p_handle == NULL)
        p_handle = &th;

    return vlc_clone_attr (p_handle, true, entry, data, priority);
}

int vlc_set_priority (vlc_thread_t th, int priority)
{
    if (!SetThreadPriority (th->id, priority))
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

/*** Thread cancellation ***/

/* APC procedure for thread cancellation */
static void CALLBACK vlc_cancel_self (ULONG_PTR self)
{
    struct vlc_thread *th = (void *)self;

    if (likely(th != NULL))
        th->killed = true;
}

void vlc_cancel (vlc_thread_t th)
{
#ifndef UNDER_CE
    QueueUserAPC (vlc_cancel_self, th->id, (uintptr_t)th);
#else
    SetEvent (th->cancel_event);
#endif
}

int vlc_savecancel (void)
{
    struct vlc_thread *th = vlc_threadvar_get (thread_key);
    if (th == NULL)
        return false; /* Main thread - cannot be cancelled anyway */

    int state = th->killable;
    th->killable = false;
    return state;
}

void vlc_restorecancel (int state)
{
    struct vlc_thread *th = vlc_threadvar_get (thread_key);
    assert (state == false || state == true);

    if (th == NULL)
        return; /* Main thread - cannot be cancelled anyway */

    assert (!th->killable);
    th->killable = state != 0;
}

void vlc_testcancel (void)
{
    struct vlc_thread *th = vlc_threadvar_get (thread_key);
    if (th == NULL)
        return; /* Main thread - cannot be cancelled anyway */

    if (th->killable && th->killed)
    {
        for (vlc_cleanup_t *p = th->cleaners; p != NULL; p = p->next)
             p->proc (p->data);

        th->data = NULL; /* TODO: special value? */
        vlc_thread_cleanup (th);
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

    struct vlc_thread *th = vlc_threadvar_get (thread_key);
    if (th == NULL)
        return; /* Main thread - cannot be cancelled anyway */

    va_start (ap, cmd);
    switch (cmd)
    {
        case VLC_CLEANUP_PUSH:
        {
            /* cleaner is a pointer to the caller stack, no need to allocate
             * and copy anything. As a nice side effect, this cannot fail. */
            vlc_cleanup_t *cleaner = va_arg (ap, vlc_cleanup_t *);
            cleaner->next = th->cleaners;
            th->cleaners = cleaner;
            break;
        }

        case VLC_CLEANUP_POP:
        {
            th->cleaners = th->cleaners->next;
            break;
        }
    }
    va_end (ap);
}


/*** Clock ***/
mtime_t mdate (void)
{
    /* We don't need the real date, just the value of a high precision timer */
    LARGE_INTEGER counter, freq;
    if (!QueryPerformanceCounter (&counter)
     || !QueryPerformanceFrequency (&freq))
        abort();

    /* Convert to from (1/freq) to microsecond resolution */
    /* We need to split the division to avoid 63-bits overflow */
    lldiv_t d = lldiv (counter.QuadPart, freq.QuadPart);

    return (d.quot * 1000000) + ((d.rem * 1000000) / freq.QuadPart);
}

#undef mwait
void mwait (mtime_t deadline)
{
    mtime_t delay;

    vlc_testcancel();
    while ((delay = (deadline - mdate())) > 0)
    {
        delay /= 1000;
        if (unlikely(delay > 0x7fffffff))
            delay = 0x7fffffff;
        vlc_Sleep (delay);
        vlc_testcancel();
    }
}

#undef msleep
void msleep (mtime_t delay)
{
    mwait (mdate () + delay);
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


/*** CPU ***/
unsigned vlc_GetCPUCount (void)
{
#ifndef UNDER_CE
    DWORD process;
    DWORD system;

    if (GetProcessAffinityMask (GetCurrentProcess(), &process, &system))
        return popcount (system);
#endif
     return 1;
}
