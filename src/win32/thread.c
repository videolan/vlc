/*****************************************************************************
 * thread.c : Win32 back-end for LibVLC
 *****************************************************************************
 * Copyright (C) 1999-2009 VLC authors and VideoLAN
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Clément Sténac
 *          Rémi Denis-Courmont
 *          Pierre Ynard
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
#include <stdarg.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>
#include <time.h>

/*** Static mutex and condition variable ***/
static vlc_mutex_t super_mutex;
static vlc_cond_t  super_variable;


/*** Common helpers ***/
#if VLC_WINSTORE_APP
static bool isCancelled(void);
#endif

static DWORD vlc_WaitForMultipleObjects (DWORD count, const HANDLE *handles,
                                         DWORD delay)
{
    DWORD ret;
    if (count == 0)
    {
#if VLC_WINSTORE_APP
        do {
            DWORD new_delay = 50;
            if (new_delay > delay)
                new_delay = delay;
            ret = SleepEx (new_delay, TRUE);
            if (delay != INFINITE)
                delay -= new_delay;
            if (isCancelled())
                ret = WAIT_IO_COMPLETION;
        } while (delay && ret == 0);
#else
        ret = SleepEx (delay, TRUE);
#endif

        if (ret == 0)
            ret = WAIT_TIMEOUT;
    }
    else {
#if VLC_WINSTORE_APP
        do {
            DWORD new_delay = 50;
            if (new_delay > delay)
                new_delay = delay;
            ret = WaitForMultipleObjectsEx (count, handles, FALSE, new_delay, TRUE);
            if (delay != INFINITE)
                delay -= new_delay;
            if (isCancelled())
                ret = WAIT_IO_COMPLETION;
        } while (delay && ret == WAIT_TIMEOUT);
#else
        ret = WaitForMultipleObjectsEx (count, handles, FALSE, delay, TRUE);
#endif
    }

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
    VLC_CLOCK_REALTIME=0, /* must be zero for VLC_STATIC_COND */
    VLC_CLOCK_MONOTONIC,
};

static void vlc_cond_init_common(vlc_cond_t *wait, unsigned clock)
{
    wait->semaphore = CreateSemaphore(NULL, 0, 0x7FFFFFFF, NULL);
    if (unlikely(wait->semaphore == NULL))
        abort();
    wait->waiters = 0;
    wait->clock = clock;
}

void vlc_cond_init (vlc_cond_t *p_condvar)
{
    vlc_cond_init_common (p_condvar, VLC_CLOCK_MONOTONIC);
}

void vlc_cond_init_daytime (vlc_cond_t *p_condvar)
{
    vlc_cond_init_common (p_condvar, VLC_CLOCK_REALTIME);
}

void vlc_cond_destroy(vlc_cond_t *wait)
{
    CloseHandle(wait->semaphore);
}

static LONG InterlockedDecrementNonZero(LONG volatile *dst)
{
    LONG cmp, val = 1;

    do
    {
        cmp = val;
        val = InterlockedCompareExchange(dst, val - 1, val);
        if (val == 0)
            return 0;
    }
    while (cmp != val);

    return val;
}

void vlc_cond_signal(vlc_cond_t *wait)
{
    if (wait->semaphore == NULL)
        return;

    if (InterlockedDecrementNonZero(&wait->waiters) > 0)
        ReleaseSemaphore(wait->semaphore, 1, NULL);
}

void vlc_cond_broadcast(vlc_cond_t *wait)
{
    if (wait->semaphore == NULL)
        return;

    LONG waiters = InterlockedExchange(&wait->waiters, 0);
    if (waiters > 0)
        ReleaseSemaphore(wait->semaphore, waiters, NULL);
}

static DWORD vlc_cond_wait_delay(vlc_cond_t *wait, vlc_mutex_t *lock,
                                 DWORD delay)
{
    DWORD result;

    vlc_testcancel();

    if (wait->semaphore == NULL)
    {   /* FIXME FIXME FIXME */
        vlc_mutex_unlock(lock);
        result = SleepEx((delay > 50u) ? 50u : delay, TRUE);
    }
    else
    {
        InterlockedIncrement(&wait->waiters);
        vlc_mutex_unlock(lock);
        result = vlc_WaitForSingleObject(wait->semaphore, delay);
    }
    vlc_mutex_lock(lock);

    if (result == WAIT_IO_COMPLETION)
        vlc_testcancel();
    return result;
}

void vlc_cond_wait(vlc_cond_t *wait, vlc_mutex_t *lock)
{
    vlc_cond_wait_delay(wait, lock, INFINITE);
}

int vlc_cond_timedwait(vlc_cond_t *wait, vlc_mutex_t *lock, mtime_t deadline)
{
    mtime_t total;

    switch (wait->clock)
    {
        case VLC_CLOCK_REALTIME: /* FIXME? sub-second precision */
            total = CLOCK_FREQ * time(NULL);
            break;
        case VLC_CLOCK_MONOTONIC:
            total = mdate();
            break;
        default:
            vlc_assert_unreachable();
    }

    total = (deadline - total) / 1000;
    if (total < 0)
        total = 0;

    DWORD delay = (total > 0x7fffffff) ? 0x7fffffff : total;

    if (vlc_cond_wait_delay(wait, lock, delay) == WAIT_TIMEOUT)
        return ETIMEDOUT;
    return 0;
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

    vlc_mutex_lock (&super_mutex);
    var->prev = vlc_threadvar_last;
    if (var->prev)
        var->prev->next = var;

    vlc_threadvar_last = var;
    vlc_mutex_unlock (&super_mutex);
    return 0;
}

void vlc_threadvar_delete (vlc_threadvar_t *p_tls)
{
    struct vlc_threadvar *var = *p_tls;

    vlc_mutex_lock (&super_mutex);
    if (var->prev != NULL)
        var->prev->next = var->next;

    if (var->next != NULL)
        var->next->prev = var->prev;
    else
        vlc_threadvar_last = var->prev;

    vlc_mutex_unlock (&super_mutex);

    TlsFree (var->id);
    free (var);
}

int vlc_threadvar_set (vlc_threadvar_t key, void *value)
{
    int saved = GetLastError ();
    int val = TlsSetValue (key->id, value) ? ENOMEM : 0;

    if (val == 0)
        SetLastError(saved);
    return val;
}

void *vlc_threadvar_get (vlc_threadvar_t key)
{
    int saved = GetLastError ();
    void *value = TlsGetValue (key->id);

    SetLastError(saved);
    return value;
}

static void vlc_threadvars_cleanup(void)
{
    vlc_threadvar_t key;
retry:
    /* TODO: use RW lock or something similar */
    vlc_mutex_lock(&super_mutex);
    for (key = vlc_threadvar_last; key != NULL; key = key->prev)
    {
        void *value = vlc_threadvar_get(key);
        if (value != NULL && key->destroy != NULL)
        {
            vlc_mutex_unlock(&super_mutex);
            vlc_threadvar_set(key, NULL);
            key->destroy(value);
            goto retry;
        }
    }
    vlc_mutex_unlock(&super_mutex);
}

/*** Threads ***/
static DWORD thread_key;

struct vlc_thread
{
    HANDLE         id;

    bool           killable;
#if !VLC_WINSTORE_APP
    bool           killed;
#else
    atomic_bool    killed;
#endif
    vlc_cleanup_t *cleaners;

    void        *(*entry) (void *);
    void          *data;
};

#if VLC_WINSTORE_APP
static bool isCancelled(void)
{
    struct vlc_thread *th = TlsGetValue(thread_key);
    if (th == NULL)
        return false; /* Main thread - cannot be cancelled anyway */

    return atomic_load(&th->killed);
}
#endif

static unsigned __stdcall vlc_entry (void *p)
{
    struct vlc_thread *th = p;

    TlsSetValue(thread_key, th);
    th->killable = true;
    th->data = th->entry (th->data);
    TlsSetValue(thread_key, NULL);

    if (th->id == NULL) /* Detached thread */
        free(th);
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
    th->killable = false; /* not until vlc_entry() ! */
#if !VLC_WINSTORE_APP
    th->killed = false;
#else
    atomic_init(&th->killed, false);
#endif
    th->cleaners = NULL;

    /* When using the MSVCRT C library you have to use the _beginthreadex
     * function instead of CreateThread, otherwise you'll end up with
     * memory leaks and the signal functions not working (see Microsoft
     * Knowledge Base, article 104641) */
    uintptr_t h = _beginthreadex (NULL, 0, vlc_entry, th, 0, NULL);
    if (h == 0)
    {
        int err = errno;
        free (th);
        return err;
    }

    if (detached)
    {
        CloseHandle((HANDLE)h);
        th->id = NULL;
    }
    else
        th->id = (HANDLE)h;

    if (p_handle != NULL)
        *p_handle = th;

    if (priority)
        SetThreadPriority (th->id, priority);

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
    while (vlc_WaitForSingleObject (th->id, INFINITE) == WAIT_IO_COMPLETION);

    if (result != NULL)
        *result = th->data;
    CloseHandle (th->id);
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

#if !VLC_WINSTORE_APP
/* APC procedure for thread cancellation */
static void CALLBACK vlc_cancel_self (ULONG_PTR self)
{
    struct vlc_thread *th = (void *)self;

    if (likely(th != NULL))
        th->killed = true;
}
#endif

void vlc_cancel (vlc_thread_t th)
{
#if !VLC_WINSTORE_APP
    QueueUserAPC (vlc_cancel_self, th->id, (uintptr_t)th);
#else
    atomic_store (&th->killed, true);
#endif
}

int vlc_savecancel (void)
{
    struct vlc_thread *th = TlsGetValue(thread_key);
    if (th == NULL)
        return false; /* Main thread - cannot be cancelled anyway */

    int state = th->killable;
    th->killable = false;
    return state;
}

void vlc_restorecancel (int state)
{
    struct vlc_thread *th = TlsGetValue(thread_key);
    assert (state == false || state == true);

    if (th == NULL)
        return; /* Main thread - cannot be cancelled anyway */

    assert (!th->killable);
    th->killable = state != 0;
}

void vlc_testcancel (void)
{
    struct vlc_thread *th = TlsGetValue(thread_key);
    if (th == NULL)
        return; /* Main thread - cannot be cancelled anyway */
    if (!th->killable)
        return;
#if !VLC_WINSTORE_APP
    if (likely(!th->killed))
        return;
#else
    if (!atomic_load(&th->killed))
        return;
#endif

    for (vlc_cleanup_t *p = th->cleaners; p != NULL; p = p->next)
        p->proc (p->data);

    th->data = NULL; /* TODO: special value? */
    TlsSetValue(thread_key, NULL);
    if (th->id == NULL) /* Detached thread */
        free(th);
    _endthreadex(0);
}

void vlc_control_cancel (int cmd, ...)
{
    /* NOTE: This function only modifies thread-specific data, so there is no
     * need to lock anything. */
    va_list ap;

    struct vlc_thread *th = TlsGetValue(thread_key);
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
static CRITICAL_SECTION clock_lock;

static mtime_t mdate_giveup (void)
{
    abort ();
}

static mtime_t (*mdate_selected) (void) = mdate_giveup;

mtime_t mdate (void)
{
    return mdate_selected ();
}

static union
{
#if (_WIN32_WINNT < 0x0601)
    struct
    {
        BOOL (*query) (PULONGLONG);
    } interrupt;
#endif
#if (_WIN32_WINNT < 0x0600)
    struct
    {
        ULONGLONG (*get) (void);
    } tick;
#endif
    struct
    {
        LARGE_INTEGER freq;
    } perf;
} clk;

static mtime_t mdate_interrupt (void)
{
    ULONGLONG ts;
    BOOL ret;

#if (_WIN32_WINNT >= 0x0601)
    ret = QueryUnbiasedInterruptTime (&ts);
#else
    ret = clk.interrupt.query (&ts);
#endif
    if (unlikely(!ret))
        abort ();

    /* hundreds of nanoseconds */
    static_assert ((10000000 % CLOCK_FREQ) == 0, "Broken frequencies ratio");
    return ts / (10000000 / CLOCK_FREQ);
}

static mtime_t mdate_tick (void)
{
#if (_WIN32_WINNT >= 0x0600)
    ULONGLONG ts = GetTickCount64 ();
#else
    ULONGLONG ts = clk.tick.get ();
#endif

    /* milliseconds */
    static_assert ((CLOCK_FREQ % 1000) == 0, "Broken frequencies ratio");
    return ts * (CLOCK_FREQ / 1000);
}
#if !VLC_WINSTORE_APP
#include <mmsystem.h>
static mtime_t mdate_multimedia (void)
{
     DWORD ts = timeGetTime ();

    /* milliseconds */
    static_assert ((CLOCK_FREQ % 1000) == 0, "Broken frequencies ratio");
    return ts * (CLOCK_FREQ / 1000);
}
#endif

static mtime_t mdate_perf (void)
{
    /* We don't need the real date, just the value of a high precision timer */
    LARGE_INTEGER counter;
    if (!QueryPerformanceCounter (&counter))
        abort ();

    /* Convert to from (1/freq) to microsecond resolution */
    /* We need to split the division to avoid 63-bits overflow */
    lldiv_t d = lldiv (counter.QuadPart, clk.perf.freq.QuadPart);

    return (d.quot * 1000000) + ((d.rem * 1000000) / clk.perf.freq.QuadPart);
}

static mtime_t mdate_wall (void)
{
    FILETIME ts;
    ULARGE_INTEGER s;

#if (_WIN32_WINNT >= 0x0602) && !VLC_WINSTORE_APP
    GetSystemTimePreciseAsFileTime (&ts);
#else
    GetSystemTimeAsFileTime (&ts);
#endif
    s.LowPart = ts.dwLowDateTime;
    s.HighPart = ts.dwHighDateTime;
    /* hundreds of nanoseconds */
    static_assert ((10000000 % CLOCK_FREQ) == 0, "Broken frequencies ratio");
    return s.QuadPart / (10000000 / CLOCK_FREQ);
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

static void SelectClockSource (vlc_object_t *obj)
{
    EnterCriticalSection (&clock_lock);
    if (mdate_selected != mdate_giveup)
    {
        LeaveCriticalSection (&clock_lock);
        return;
    }

#if VLC_WINSTORE_APP
    const char *name = "perf";
#else
    const char *name = "multimedia";
#endif
    char *str = var_InheritString (obj, "clock-source");
    if (str != NULL)
        name = str;
    if (!strcmp (name, "interrupt"))
    {
        msg_Dbg (obj, "using interrupt time as clock source");
#if (_WIN32_WINNT < 0x0601)
        HANDLE h = GetModuleHandle (_T("kernel32.dll"));
        if (unlikely(h == NULL))
            abort ();
        clk.interrupt.query = (void *)GetProcAddress (h,
                                                      "QueryUnbiasedInterruptTime");
        if (unlikely(clk.interrupt.query == NULL))
            abort ();
#endif
        mdate_selected = mdate_interrupt;
    }
    else
    if (!strcmp (name, "tick"))
    {
        msg_Dbg (obj, "using Windows time as clock source");
#if (_WIN32_WINNT < 0x0600)
        HANDLE h = GetModuleHandle (_T("kernel32.dll"));
        if (unlikely(h == NULL))
            abort ();
        clk.tick.get = (void *)GetProcAddress (h, "GetTickCount64");
        if (unlikely(clk.tick.get == NULL))
            abort ();
#endif
        mdate_selected = mdate_tick;
    }
#if !VLC_WINSTORE_APP
    else
    if (!strcmp (name, "multimedia"))
    {
        TIMECAPS caps;

        msg_Dbg (obj, "using multimedia timers as clock source");
        if (timeGetDevCaps (&caps, sizeof (caps)) != MMSYSERR_NOERROR)
            abort ();
        msg_Dbg (obj, " min period: %u ms, max period: %u ms",
                 caps.wPeriodMin, caps.wPeriodMax);
        mdate_selected = mdate_multimedia;
    }
#endif
    else
    if (!strcmp (name, "perf"))
    {
        msg_Dbg (obj, "using performance counters as clock source");
        if (!QueryPerformanceFrequency (&clk.perf.freq))
            abort ();
        msg_Dbg (obj, " frequency: %llu Hz", clk.perf.freq.QuadPart);
        mdate_selected = mdate_perf;
    }
    else
    if (!strcmp (name, "wall"))
    {
        msg_Dbg (obj, "using system time as clock source");
        mdate_selected = mdate_wall;
    }
    else
    {
        msg_Err (obj, "invalid clock source \"%s\"", name);
        abort ();
    }
    LeaveCriticalSection (&clock_lock);
    free (str);
}

size_t EnumClockSource (vlc_object_t *obj, const char *var,
                        char ***vp, char ***np)
{
    const size_t max = 6;
    char **values = xmalloc (sizeof (*values) * max);
    char **names = xmalloc (sizeof (*names) * max);
    size_t n = 0;

#if (_WIN32_WINNT < 0x0601)
    DWORD version = LOWORD(GetVersion());
    version = (LOBYTE(version) << 8) | (HIBYTE(version) << 0);
#endif

    values[n] = xstrdup ("");
    names[n] = xstrdup (_("Auto"));
    n++;
#if (_WIN32_WINNT < 0x0601)
    if (version >= 0x0601)
#endif
    {
        values[n] = xstrdup ("interrupt");
        names[n] = xstrdup ("Interrupt time");
        n++;
    }
#if (_WIN32_WINNT < 0x0600)
    if (version >= 0x0600)
#endif
    {
        values[n] = xstrdup ("tick");
        names[n] = xstrdup ("Windows time");
        n++;
    }
#if !VLC_WINSTORE_APP
    values[n] = xstrdup ("multimedia");
    names[n] = xstrdup ("Multimedia timers");
    n++;
#endif
    values[n] = xstrdup ("perf");
    names[n] = xstrdup ("Performance counters");
    n++;
    values[n] = xstrdup ("wall");
    names[n] = xstrdup ("System time (DANGEROUS!)");
    n++;

    *vp = values;
    *np = names;
    (void) obj; (void) var;
    return n;
}


/*** Timers ***/
struct vlc_timer
{
    HANDLE handle;
    void (*func) (void *);
    void *data;
};

static void CALLBACK vlc_timer_do (void *val, BOOLEAN timeout)
{
    struct vlc_timer *timer = val;

    assert (timeout);
    timer->func (timer->data);
}

int vlc_timer_create (vlc_timer_t *id, void (*func) (void *), void *data)
{
    struct vlc_timer *timer = malloc (sizeof (*timer));

    if (timer == NULL)
        return ENOMEM;
    timer->func = func;
    timer->data = data;
    timer->handle = INVALID_HANDLE_VALUE;
    *id = timer;
    return 0;
}

void vlc_timer_destroy (vlc_timer_t timer)
{
#if !VLC_WINSTORE_APP
    if (timer->handle != INVALID_HANDLE_VALUE)
        DeleteTimerQueueTimer (NULL, timer->handle, INVALID_HANDLE_VALUE);
#endif
    free (timer);
}

void vlc_timer_schedule (vlc_timer_t timer, bool absolute,
                         mtime_t value, mtime_t interval)
{
    if (timer->handle != INVALID_HANDLE_VALUE)
    {
#if !VLC_WINSTORE_APP
        DeleteTimerQueueTimer (NULL, timer->handle, NULL);
#endif
        timer->handle = INVALID_HANDLE_VALUE;
    }
    if (value == 0)
        return; /* Disarm */

    if (absolute)
        value -= mdate ();
    value = (value + 999) / 1000;
    interval = (interval + 999) / 1000;

#if !VLC_WINSTORE_APP
    if (!CreateTimerQueueTimer (&timer->handle, NULL, vlc_timer_do, timer,
                                value, interval, WT_EXECUTEDEFAULT))
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
    SYSTEM_INFO systemInfo;

    GetNativeSystemInfo(&systemInfo);

    return systemInfo.dwNumberOfProcessors;
}


/*** Initialization ***/
void vlc_threads_setup (libvlc_int_t *p_libvlc)
{
    SelectClockSource (VLC_OBJECT(p_libvlc));
}

extern vlc_rwlock_t config_lock;
BOOL WINAPI DllMain (HINSTANCE, DWORD, LPVOID);

BOOL WINAPI DllMain (HINSTANCE hinstDll, DWORD fdwReason, LPVOID lpvReserved)
{
    (void) hinstDll;
    (void) lpvReserved;

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            thread_key = TlsAlloc();
            if (unlikely(thread_key == TLS_OUT_OF_INDEXES))
                return FALSE;
            InitializeCriticalSection (&clock_lock);
            vlc_mutex_init (&super_mutex);
            vlc_cond_init (&super_variable);
            vlc_rwlock_init (&config_lock);
            vlc_CPU_init ();
            break;

        case DLL_PROCESS_DETACH:
            vlc_rwlock_destroy (&config_lock);
            vlc_cond_destroy (&super_variable);
            vlc_mutex_destroy (&super_mutex);
            DeleteCriticalSection (&clock_lock);
            TlsFree(thread_key);
            break;

        case DLL_THREAD_DETACH:
            vlc_threadvars_cleanup();
            break;
    }
    return TRUE;
}
