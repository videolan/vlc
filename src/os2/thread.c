/*****************************************************************************
 * thread.c : OS/2 back-end for LibVLC
 *****************************************************************************
 * Copyright (C) 1999-2011 VLC authors and VideoLAN
 *
 * Authors: KO Myung-Hun <komh@chollian.net>
 *          Jean-Marc Dressler <polux@via.ecp.fr>
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

#include "libvlc.h"
#include <stdarg.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <sys/time.h>
#include <sys/select.h>

static vlc_threadvar_t thread_key;

/**
 * Per-thread data
 */
struct vlc_thread
{
    TID            tid;
    HEV            cancel_event;
    HEV            done_event;
    int            cancel_sock;

    bool           detached;
    bool           killable;
    bool           killed;
    vlc_cleanup_t *cleaners;

    void        *(*entry) (void *);
    void          *data;
};

static void vlc_cancel_self (PVOID dummy);

static ULONG vlc_DosWaitEventSemEx( HEV hev, ULONG ulTimeout, BOOL fCancelable )
{
    HMUX      hmux;
    SEMRECORD asr[ 2 ];
    ULONG     ulUser;
    int       n;
    ULONG     rc;

    struct vlc_thread *th = vlc_threadvar_get( thread_key );
    if( th == NULL || !fCancelable )
    {
        /* Main thread - cannot be cancelled anyway
         * Alien thread - out of our control
         */
        if( hev != NULLHANDLE )
            return DosWaitEventSem( hev, ulTimeout );

        return DosSleep( ulTimeout );
    }

    n = 0;
    if( hev != NULLHANDLE )
    {
        asr[ n ].hsemCur = ( HSEM )hev;
        asr[ n ].ulUser  = 0;
        n++;
    }
    asr[ n ].hsemCur = ( HSEM )th->cancel_event;
    asr[ n ].ulUser  = 0xFFFF;
    n++;

    DosCreateMuxWaitSem( NULL, &hmux, n, asr, DCMW_WAIT_ANY );
    rc = DosWaitMuxWaitSem( hmux, ulTimeout, &ulUser );
    DosCloseMuxWaitSem( hmux );
    if( rc )
        return rc;

    if( ulUser == 0xFFFF )
    {
        vlc_cancel_self( th );
        return ERROR_INTERRUPT;
    }

    return NO_ERROR;
}

static ULONG vlc_WaitForSingleObject (HEV hev, ULONG ulTimeout)
{
    return vlc_DosWaitEventSemEx( hev, ulTimeout, TRUE );
}

static ULONG vlc_Sleep (ULONG ulTimeout)
{
    ULONG rc = vlc_DosWaitEventSemEx( NULLHANDLE, ulTimeout, TRUE );

    return ( rc != ERROR_TIMEOUT ) ? rc : 0;
}

static vlc_mutex_t super_mutex;
static vlc_cond_t  super_variable;
extern vlc_rwlock_t config_lock;

int _CRT_init(void);
void _CRT_term(void);

unsigned long _System _DLL_InitTerm(unsigned long, unsigned long);

unsigned long _System _DLL_InitTerm(unsigned long hmod, unsigned long flag)
{
    VLC_UNUSED (hmod);

    switch (flag)
    {
        case 0 :    /* Initialization */
            if(_CRT_init() == -1)
                return 0;

            vlc_mutex_init (&super_mutex);
            vlc_cond_init (&super_variable);
            vlc_threadvar_create (&thread_key, NULL);
            vlc_rwlock_init (&config_lock);
            vlc_CPU_init ();

            return 1;

        case 1 :    /* Termination */
            vlc_rwlock_destroy (&config_lock);
            vlc_threadvar_delete (&thread_key);
            vlc_cond_destroy (&super_variable);
            vlc_mutex_destroy (&super_mutex);

            _CRT_term();

            return 1;
    }

    return 0;   /* Failed */
}

/*** Mutexes ***/
void vlc_mutex_init( vlc_mutex_t *p_mutex )
{
    /* This creates a recursive mutex. This is OK as fast mutexes have
     * no defined behavior in case of recursive locking. */
    DosCreateMutexSem( NULL, &p_mutex->hmtx, 0, FALSE );
    p_mutex->dynamic = true;
}

void vlc_mutex_init_recursive( vlc_mutex_t *p_mutex )
{
    DosCreateMutexSem( NULL, &p_mutex->hmtx, 0, FALSE );
    p_mutex->dynamic = true;
}


void vlc_mutex_destroy (vlc_mutex_t *p_mutex)
{
    assert (p_mutex->dynamic);
    DosCloseMutexSem( p_mutex->hmtx );
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

    DosRequestMutexSem(p_mutex->hmtx, SEM_INDEFINITE_WAIT);
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

    return DosRequestMutexSem( p_mutex->hmtx, 0 ) ? EBUSY : 0;
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

    DosReleaseMutexSem( p_mutex->hmtx );
}

/*** Condition variables ***/
#undef CLOCK_REALTIME
#undef CLOCK_MONOTONIC
enum
{
    CLOCK_REALTIME=0, /* must be zero for VLC_STATIC_COND */
    CLOCK_MONOTONIC,
};

static void vlc_cond_init_common (vlc_cond_t *p_condvar, unsigned clock)
{
    /* Create a manual-reset event (manual reset is needed for broadcast). */
    if (DosCreateEventSem (NULL, &p_condvar->hev, 0, FALSE))
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
    DosCloseEventSem( p_condvar->hev );
}

void vlc_cond_signal (vlc_cond_t *p_condvar)
{
    if (!p_condvar->hev)
        return;

    /* This is suboptimal but works. */
    vlc_cond_broadcast (p_condvar);
}

void vlc_cond_broadcast (vlc_cond_t *p_condvar)
{
    if (!p_condvar->hev)
        return;

    /* Wake all threads up (as the event HANDLE has manual reset) */
    DosPostEventSem( p_condvar->hev );
}

void vlc_cond_wait (vlc_cond_t *p_condvar, vlc_mutex_t *p_mutex)
{
    ULONG ulPost;
    ULONG rc;

    if (!p_condvar->hev)
    {   /* FIXME FIXME FIXME */
        msleep (50000);
        return;
    }

    do
    {
        vlc_testcancel();

        vlc_mutex_unlock (p_mutex);
        rc = vlc_WaitForSingleObject( p_condvar->hev, SEM_INDEFINITE_WAIT );
        vlc_mutex_lock (p_mutex);
    } while( rc == ERROR_INTERRUPT );

    DosResetEventSem( p_condvar->hev, &ulPost );
}

int vlc_cond_timedwait (vlc_cond_t *p_condvar, vlc_mutex_t *p_mutex,
                        mtime_t deadline)
{
    ULONG   ulTimeout;
    ULONG   ulPost;
    ULONG   rc;

    if (!p_condvar->hev)
    {   /* FIXME FIXME FIXME */
        msleep (50000);
        return 0;
    }

    do
    {
        vlc_testcancel();

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

        ulTimeout = ( total > 0x7fffffff ) ? 0x7fffffff : total;

        vlc_mutex_unlock (p_mutex);
        rc = vlc_WaitForSingleObject( p_condvar->hev, ulTimeout );
        vlc_mutex_lock (p_mutex);
    } while( rc == ERROR_INTERRUPT );

    DosResetEventSem( p_condvar->hev, &ulPost );

    return rc ? ETIMEDOUT : 0;
}

/*** Thread-specific variables (TLS) ***/
struct vlc_threadvar
{
    PULONG                id;
    void                (*destroy) (void *);
    struct vlc_threadvar *prev;
    struct vlc_threadvar *next;
} *vlc_threadvar_last = NULL;

int vlc_threadvar_create (vlc_threadvar_t *p_tls, void (*destr) (void *))
{
    ULONG rc;

    struct vlc_threadvar *var = malloc (sizeof (*var));
    if (unlikely(var == NULL))
        return errno;

    rc = DosAllocThreadLocalMemory( 1, &var->id );
    if( rc )
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

    DosFreeThreadLocalMemory( var->id );
    free (var);
}

int vlc_threadvar_set (vlc_threadvar_t key, void *value)
{
    *key->id = ( ULONG )value;
    return 0;
}

void *vlc_threadvar_get (vlc_threadvar_t key)
{
    return ( void * )*key->id;
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
    vlc_mutex_lock (&super_mutex);
    for (key = vlc_threadvar_last; key != NULL; key = key->prev)
    {
        void *value = vlc_threadvar_get (key);
        if (value != NULL && key->destroy != NULL)
        {
            vlc_mutex_unlock (&super_mutex);
            vlc_threadvar_set (key, NULL);
            key->destroy (value);
            goto retry;
        }
    }
    vlc_mutex_unlock (&super_mutex);

    if (th->detached)
    {
        DosCloseEventSem (th->cancel_event);
        DosCloseEventSem (th->done_event );

        soclose (th->cancel_sock);

        free (th);
    }
}

static void vlc_entry( void *p )
{
    struct vlc_thread *th = p;

    vlc_threadvar_set (thread_key, th);
    th->killable = true;
    th->data = th->entry (th->data);
    DosPostEventSem( th->done_event );
    vlc_thread_cleanup (th);
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

    if( DosCreateEventSem (NULL, &th->cancel_event, 0, FALSE))
        goto error;
    if( DosCreateEventSem (NULL, &th->done_event, 0, FALSE))
        goto error;

    th->cancel_sock = socket (AF_LOCAL, SOCK_STREAM, 0);
    if( th->cancel_sock < 0 )
        goto error;

    th->tid = _beginthread (vlc_entry, NULL, 1024 * 1024, th);
    if((int)th->tid == -1)
        goto error;

    if (p_handle != NULL)
        *p_handle = th;

    if (priority)
        DosSetPriority(PRTYS_THREAD,
                       HIBYTE(priority),
                       LOBYTE(priority),
                       th->tid);

    return 0;

error:
    soclose (th->cancel_sock);
    DosCloseEventSem (th->cancel_event);
    DosCloseEventSem (th->done_event);
    free (th);

    return ENOMEM;
}

int vlc_clone (vlc_thread_t *p_handle, void *(*entry) (void *),
                void *data, int priority)
{
    return vlc_clone_attr (p_handle, false, entry, data, priority);
}

void vlc_join (vlc_thread_t th, void **result)
{
    ULONG rc;

    do
    {
        vlc_testcancel();
        rc = vlc_WaitForSingleObject( th->done_event, SEM_INDEFINITE_WAIT );
    } while( rc == ERROR_INTERRUPT );

    if (result != NULL)
        *result = th->data;

    DosCloseEventSem( th->cancel_event );
    DosCloseEventSem( th->done_event );

    soclose( th->cancel_sock );

    free( th );
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
    if (DosSetPriority(PRTYS_THREAD,
                       HIBYTE(priority),
                       LOBYTE(priority),
                       th->tid))
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

/*** Thread cancellation ***/

/* APC procedure for thread cancellation */
static void vlc_cancel_self (PVOID self)
{
    struct vlc_thread *th = self;

    if (likely(th != NULL))
        th->killed = true;
}

void vlc_cancel (vlc_thread_t thread_id)
{
    DosPostEventSem( thread_id->cancel_event );
    so_cancel( thread_id->cancel_sock );
}

int vlc_savecancel (void)
{
    int state;

    struct vlc_thread *th = vlc_threadvar_get (thread_key);
    if (th == NULL)
        return false; /* Main thread - cannot be cancelled anyway */

    state = th->killable;
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

    /* This check is needed for the case that vlc_cancel() is followed by
     * vlc_testcancel() without any cancellation point */
    if( DosWaitEventSem( th->cancel_event, 0 ) == NO_ERROR )
        vlc_cancel_self( th );

    if (th->killable && th->killed)
    {
        for (vlc_cleanup_t *p = th->cleaners; p != NULL; p = p->next)
             p->proc (p->data);

        DosPostEventSem( th->done_event );
        th->data = NULL; /* TODO: special value? */
        vlc_thread_cleanup (th);
        _endthread();
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

static int vlc_select( int nfds, fd_set *rdset, fd_set *wrset, fd_set *exset,
                       struct timeval *timeout )
{
    struct vlc_thread *th = vlc_threadvar_get( thread_key );

    int rc;

    if( th )
    {
        FD_SET( th->cancel_sock, rdset );

        nfds = MAX( nfds, th->cancel_sock + 1 );
    }

    rc = select( nfds, rdset, wrset, exset, timeout );

    vlc_testcancel();

    return rc;

}

int vlc_poll( struct pollfd *fds, unsigned nfds, int timeout )
{
    fd_set rdset, wrset, exset;

    struct timeval tv = { 0, 0 };

    int val = -1;

    FD_ZERO( &rdset );
    FD_ZERO( &wrset );
    FD_ZERO( &exset );
    for( unsigned i = 0; i < nfds; i++ )
    {
        int fd = fds[ i ].fd;
        if( val < fd )
            val = fd;

        if(( unsigned )fd >= FD_SETSIZE )
        {
            errno = EINVAL;
            return -1;
        }

        if( fds[ i ].events & POLLIN )
            FD_SET( fd, &rdset );
        if( fds[ i ].events & POLLOUT )
            FD_SET( fd, &wrset );
        if( fds[ i ].events & POLLPRI )
            FD_SET( fd, &exset );
    }

    if( timeout >= 0 )
    {
        div_t d    = div( timeout, 1000 );
        tv.tv_sec  = d.quot;
        tv.tv_usec = d.rem * 1000;
    }

    val = vlc_select( val + 1, &rdset, &wrset, &exset,
                      ( timeout >= 0 ) ? &tv : NULL );
    if( val == -1 )
        return -1;

    for( unsigned i = 0; i < nfds; i++ )
    {
        int fd = fds[ i ].fd;
        fds[ i ].revents = ( FD_ISSET( fd, &rdset ) ? POLLIN  : 0 )
                         | ( FD_ISSET( fd, &wrset ) ? POLLOUT : 0 )
                         | ( FD_ISSET( fd, &exset ) ? POLLPRI : 0 );
    }

    return val;
}

#define Q2LL( q )   ( *( long long * )&( q ))

/*** Clock ***/
mtime_t mdate (void)
{
    /* We don't need the real date, just the value of a high precision timer */
    QWORD counter;
    ULONG freq;
    if (DosTmrQueryTime(&counter) || DosTmrQueryFreq(&freq))
        abort();

    /* Convert to from (1/freq) to microsecond resolution */
    /* We need to split the division to avoid 63-bits overflow */
    lldiv_t d = lldiv (Q2LL(counter), freq);

    return (d.quot * 1000000) + ((d.rem * 1000000) / freq);
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
    TID    tid;
    HEV    hev;
    HTIMER htimer;
    ULONG  interval;
    bool   quit;
    void (*func) (void *);
    void  *data;
};

static void vlc_timer_do (void *arg)
{
    struct vlc_timer *timer = arg;

    while (1)
    {
        ULONG count;

        DosWaitEventSem (timer->hev, SEM_INDEFINITE_WAIT);
        DosResetEventSem (timer->hev, &count);

        if (timer->quit)
            break;

        timer->func (timer->data);

        if (timer->interval)
            DosAsyncTimer (timer->interval, (HSEM)timer->hev, &timer->htimer);
    }
}

int vlc_timer_create (vlc_timer_t *id, void (*func) (void *), void *data)
{
    struct vlc_timer *timer = malloc (sizeof (*timer));

    if (timer == NULL)
        return ENOMEM;

    timer->func = func;
    timer->data = data;

    DosCreateEventSem (NULL, &timer->hev, DC_SEM_SHARED, FALSE);
    timer->htimer = NULLHANDLE;
    timer->interval = 0;
    timer->quit = false;
    timer->tid  = _beginthread (vlc_timer_do, NULL, 1024 * 1024, timer);

    *id = timer;
    return 0;
}

void vlc_timer_destroy (vlc_timer_t timer)
{
    if (timer->htimer != NULLHANDLE)
        DosStopTimer (timer->htimer);

    timer->quit = true;
    DosPostEventSem (timer->hev);
    DosWaitThread (&timer->tid, DCWW_WAIT);
    DosCloseEventSem (timer->hev);

    free (timer);
}

void vlc_timer_schedule (vlc_timer_t timer, bool absolute,
                         mtime_t value, mtime_t interval)
{
    if (timer->htimer != NULLHANDLE)
    {
        DosStopTimer (timer->htimer);
        timer->htimer = NULLHANDLE;
        timer->interval = 0;
    }

    if (value == 0)
        return; /* Disarm */

    if (absolute)
        value -= mdate ();
    value = (value + 999) / 1000;
    interval = (interval + 999) / 1000;

    timer->interval = interval;
    if (DosAsyncTimer (value, (HSEM)timer->hev, &timer->htimer))
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
    ULONG numprocs = 1;

    DosQuerySysInfo(QSV_NUMPROCESSORS, QSV_NUMPROCESSORS,
                    &numprocs, sizeof(numprocs));

    return numprocs;
}
