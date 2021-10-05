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

#include <stdalign.h>
#include <stdatomic.h>

#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include <sys/time.h>
#include <sys/select.h>

#include <sys/builtin.h>

#include <sys/stat.h>

/* Static mutex and condition variable */
static vlc_mutex_t super_mutex;
static vlc_cond_t  super_variable;

/* Threads */
static vlc_threadvar_t thread_key;

struct vlc_thread
{
    TID            tid;
    HEV            cancel_event;
    HEV            done_event;
    int            cancel_sock;

    bool           killable;
    atomic_bool    killed;
    vlc_cleanup_t *cleaners;

    void        *(*entry) (void *);
    void          *data;

    struct
    {
        atomic_uint *addr;
        HMTX        lock;
    } wait;
};

static void vlc_cancel_self (PVOID dummy);

static ULONG vlc_DosWaitEventSemEx( HEV hev, ULONG ulTimeout )
{
    HMUX      hmux;
    SEMRECORD asr[ 2 ];
    ULONG     ulUser;
    int       n;
    ULONG     rc;

    struct vlc_thread *th = vlc_threadvar_get(thread_key);
    if( th == NULL || !th->killable )
    {
        /* Main thread - cannot be cancelled anyway
         * Alien thread - out of our control
         * Cancel disabled thread - ignore cancel
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
    return vlc_DosWaitEventSemEx( hev, ulTimeout );
}

static ULONG vlc_Sleep (ULONG ulTimeout)
{
    ULONG rc = vlc_DosWaitEventSemEx( NULLHANDLE, ulTimeout );

    return ( rc != ERROR_TIMEOUT ) ? rc : 0;
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

static struct wait_bucket
{
    HMTX lock;
    HEV wait;
    unsigned waiters;
} wait_buckets[ 32 ];

static void wait_bucket_init( void )
{
    for( size_t i = 0; i < ARRAY_SIZE( wait_buckets ); i++ )
    {
        struct wait_bucket *bucket = wait_buckets + i;

        DosCreateMutexSem( NULL, &bucket->lock, 0L, FALSE );
        DosCreateEventSem( NULL, &bucket->wait, 0L, FALSE );
    }
}

static void wait_bucket_destroy( void )
{
    for( size_t i = 0; i < ARRAY_SIZE( wait_buckets ); i++ )
    {
        struct wait_bucket *bucket = wait_buckets + i;

        DosCloseMutexSem( bucket->lock );
        DosCloseEventSem( bucket->wait );
    }
}

static struct wait_bucket *wait_bucket_get( atomic_uint *addr )
{
    uintptr_t u = ( uintptr_t )addr;
    size_t idx = ( u / alignof ( *addr )) % ARRAY_SIZE( wait_buckets );

    return &wait_buckets[ idx ];
}

static struct wait_bucket *wait_bucket_enter( atomic_uint *addr )
{
    struct wait_bucket *bucket = wait_bucket_get(addr);

    DosRequestMutexSem( bucket->lock, SEM_INDEFINITE_WAIT );
    bucket->waiters++;

    return bucket;
}

static void wait_bucket_leave( void *data )
{
    struct wait_bucket *bucket = data;

    bucket->waiters--;
    DosReleaseMutexSem( bucket->lock );
}

void vlc_atomic_wait( void *addr, unsigned value )
{
    atomic_uint *futex = addr;
    struct wait_bucket *bucket = wait_bucket_enter( futex );

    vlc_cleanup_push( wait_bucket_leave, bucket );

    if( value == atomic_load_explicit( futex, memory_order_relaxed ))
    {
        ULONG count;

        DosReleaseMutexSem( bucket->lock );
        vlc_WaitForSingleObject( bucket->wait, SEM_INDEFINITE_WAIT );
        DosResetEventSem( bucket->wait, &count );
        DosRequestMutexSem( bucket->lock, SEM_INDEFINITE_WAIT );
    }
    else
        vlc_testcancel();

    wait_bucket_leave( bucket );
    vlc_cleanup_pop();
}

int vlc_atomic_timedwait(void *addr, unsigned value, vlc_tick_t deadline)
{
    atomic_uint *futex = addr;
    struct wait_bucket *bucket = wait_bucket_enter( futex );

    ULONG rc = 0;

    vlc_cleanup_push( wait_bucket_leave, bucket );

    if( value == atomic_load_explicit( futex, memory_order_relaxed ))
    {
        vlc_tick_t delay;

        DosReleaseMutexSem( bucket->lock );

        do
        {
            ULONG ms;
            ULONG count;

            delay = deadline - vlc_tick_now();

            if( delay < 0 )
                ms = 0;
            else if( delay >= VLC_TICK_FROM_MS( LONG_MAX ))
                ms = LONG_MAX;
            else
                ms = MS_FROM_VLC_TICK( delay );

            rc = vlc_WaitForSingleObject( bucket->wait, ms );
            if( rc == 0 )
            {
                DosResetEventSem( bucket->wait, &count );
                break;
            }
        } while( delay > 0 );

        DosRequestMutexSem( bucket->lock, SEM_INDEFINITE_WAIT );
    }
    else
        vlc_testcancel();

    wait_bucket_leave( bucket );
    vlc_cleanup_pop();

    return rc == 0 ? 0 : ETIMEDOUT;
}

int vlc_atomic_timedwait_daytime(void *addr, unsigned value, time_t deadline)
{
    atomic_uint *futex = addr;
    struct wait_bucket *bucket = wait_bucket_enter( futex );

    ULONG rc = 0;

    vlc_cleanup_push( wait_bucket_leave, bucket );

    if( value == atomic_load_explicit( futex, memory_order_relaxed ))
    {
        vlc_tick_t delay;

        DosReleaseMutexSem( bucket->lock );

        do
        {
            ULONG ms;
            ULONG count;

            delay = deadline - time( NULL );

            if( delay < 0 )
                ms = 0;
            else if( delay >= ( LONG_MAX / 1000 ))
                ms = LONG_MAX;
            else
                ms = delay * 1000;

            rc = vlc_WaitForSingleObject( bucket->wait, ms );
            if( rc == 0 )
            {
                DosResetEventSem( bucket->wait, &count );
                break;
            }
        } while( delay > 0 );

        DosRequestMutexSem( bucket->lock, SEM_INDEFINITE_WAIT );
    }
    else
        vlc_testcancel();

    wait_bucket_leave( bucket );
    vlc_cleanup_pop();

    return rc == 0 ? 0 : ETIMEDOUT;
}

void vlc_atomic_notify_one(void *addr)
{
    vlc_atomic_notify_all(addr);
}

void vlc_atomic_notify_all(void *addr)
{
    struct wait_bucket *bucket = wait_bucket_get(addr);

    DosRequestMutexSem( bucket->lock, SEM_INDEFINITE_WAIT );

    if( bucket->waiters > 0 )
        DosPostEventSem( bucket->wait );

    DosReleaseMutexSem( bucket->lock);
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

int vlc_clone (vlc_thread_t *p_handle, void *(*entry) (void *),
               void *data, int priority)
{
    struct vlc_thread *th = malloc (sizeof (*th));
    if (unlikely(th == NULL))
        return ENOMEM;
    th->entry = entry;
    th->data = data;
    th->killable = false; /* not until vlc_entry() ! */
    atomic_init (&th->killed, false);
    th->cleaners = NULL;
    th->wait.addr = NULL;

    if( DosCreateEventSem (NULL, &th->cancel_event, 0, FALSE))
        goto error;
    if( DosCreateEventSem (NULL, &th->done_event, 0, FALSE))
        goto error;

    th->cancel_sock = socket (AF_LOCAL, SOCK_STREAM, 0);
    if( th->cancel_sock < 0 )
        goto error;

    if( DosCreateMutexSem (NULL, &th->wait.lock, 0, FALSE))
        goto error;

    th->tid = _beginthread (vlc_entry, NULL, 1024 * 1024, th);
    if((int)th->tid == -1)
        goto error;

    if (p_handle != NULL)
        *p_handle = th;

    if (priority)
    {
        LONG delta = PRTYD_MAXIMUM;

        if (priority != VLC_THREAD_PRIORITY_HIGHEST)
            delta >>= 1;

        DosSetPriority(PRTYS_THREAD, PRTYC_REGULAR, delta, th->tid );
    }

    return 0;

error:
    DosCloseMutexSem (th->wait.lock);
    soclose (th->cancel_sock);
    DosCloseEventSem (th->cancel_event);
    DosCloseEventSem (th->done_event);
    free (th);

    return ENOMEM;
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

    DosCloseMutexSem( th->wait.lock );

    free( th );
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

unsigned long vlc_thread_id (void)
{
    return _gettid();
}

void vlc_thread_set_name(const char *name)
{
    VLC_UNUSED(name);
}

/*** Thread cancellation ***/

/* APC procedure for thread cancellation */
static void vlc_cancel_self (PVOID self)
{
    struct vlc_thread *th = self;

    if (likely(th != NULL))
        atomic_store_explicit (&th->killed, true, memory_order_relaxed);
}

void vlc_cancel (vlc_thread_t th)
{
    atomic_store_explicit( &th->killed, true, memory_order_relaxed );

    DosRequestMutexSem( th->wait.lock, SEM_INDEFINITE_WAIT );
    if( th->wait.addr != NULL )
    {
        atomic_fetch_or_explicit( th->wait.addr, 1, memory_order_relaxed );
        vlc_atomic_notify_all( th->wait.addr );
    }
    DosReleaseMutexSem( th->wait.lock );

    DosPostEventSem( th->cancel_event );
    so_cancel( th->cancel_sock );
}

int vlc_savecancel (void)
{
    int state;

    struct vlc_thread *th = vlc_threadvar_get(thread_key);
    if (th == NULL)
        return false; /* Main thread - cannot be cancelled anyway */

    state = th->killable;
    th->killable = false;
    return state;
}

void vlc_restorecancel (int state)
{
    struct vlc_thread *th = vlc_threadvar_get(thread_key);
    assert (state == false || state == true);

    if (th == NULL)
        return; /* Main thread - cannot be cancelled anyway */

    assert (!th->killable);
    th->killable = state != 0;
}

void vlc_testcancel (void)
{
    struct vlc_thread *th = vlc_threadvar_get(thread_key);
    if (th == NULL)
        return; /* Main thread - cannot be cancelled anyway */

    if (!th->killable)
        return;

    /* This check is needed for the case that vlc_cancel() is followed by
     * vlc_testcancel() without any cancellation point */
    if( DosWaitEventSem( th->cancel_event, 0 ) == NO_ERROR )
        vlc_cancel_self( th );

    if( !atomic_load_explicit( &th->killed, memory_order_relaxed ))
        return;

    th->killable = true; /* Do not re-enter cancellation cleanup */

    for (vlc_cleanup_t *p = th->cleaners; p != NULL; p = p->next)
         p->proc (p->data);

    DosPostEventSem( th->done_event );
    th->data = NULL; /* TODO: special value? */
    vlc_thread_cleanup (th);
    _endthread();
}

void vlc_control_cancel (vlc_cleanup_t *cleaner)
{
    /* NOTE: This function only modifies thread-specific data, so there is no
     * need to lock anything. */

    struct vlc_thread *th = vlc_threadvar_get(thread_key);
    if (th == NULL)
        return; /* Main thread - cannot be cancelled anyway */

    if (cleaner != NULL)
    {
        /* cleaner is a pointer to the caller stack, no need to allocate
            * and copy anything. As a nice side effect, this cannot fail. */
        cleaner->next = th->cleaners;
        th->cleaners = cleaner;
    }
    else
    {
        th->cleaners = th->cleaners->next;
    }
}

void vlc_cancel_addr_set( atomic_uint *addr )
{
    struct vlc_thread *th = vlc_threadvar_get( thread_key );
    if( th == NULL )
        return; /* Main thread - cannot be cancelled anyway */

    DosRequestMutexSem( th->wait.lock, SEM_INDEFINITE_WAIT );
    assert( th->wait.addr == NULL );
    th->wait.addr = addr;
    DosReleaseMutexSem( th->wait.lock );
}

void vlc_cancel_addr_clear( atomic_uint *addr )
{
    struct vlc_thread *th = vlc_threadvar_get( thread_key );
    if( th == NULL )
        return; /* Main thread - cannot be cancelled anyway */

    DosRequestMutexSem( th->wait.lock, SEM_INDEFINITE_WAIT );
    assert( th->wait.addr == addr );
    th->wait.addr = NULL;
    DosReleaseMutexSem( th->wait.lock );
}

static int vlc_select( int nfds, fd_set *rdset, fd_set *wrset, fd_set *exset,
                       struct timeval *timeout )
{
    struct vlc_thread *th = vlc_threadvar_get(thread_key);

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

/* Export vlc_poll_os2 directly regardless of EXPORTS of .def */
__declspec(dllexport)
int vlc_poll_os2( struct pollfd *fds, unsigned nfds, int timeout );

__declspec(dllexport)
int vlc_poll_os2( struct pollfd *fds, unsigned nfds, int timeout )
{
    fd_set rdset, wrset, exset;

    int non_sockets = 0;

    struct timeval tv = { 0, 0 };

    int val = -1;

    FD_ZERO( &rdset );
    FD_ZERO( &wrset );
    FD_ZERO( &exset );
    for( unsigned i = 0; i < nfds; i++ )
    {
        int fd = fds[ i ].fd;
        struct stat stbuf;

        fds[ i ].revents = 0;

        if( fstat( fd, &stbuf ) == -1 ||
            (errno = 0, !S_ISSOCK( stbuf.st_mode )))
        {
            if( fd >= 0 )
            {
                /* If regular files, assume readiness for requested modes */
                fds[ i ].revents = ( !errno && S_ISREG( stbuf.st_mode ))
                                   ? ( fds[ i ].events &
                                       ( POLLIN | POLLOUT | POLLPRI ))
                                   : POLLNVAL;

                non_sockets++;
            }

            continue;
        }

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

    if( non_sockets > 0 )
        timeout = 0;    /* Just check pending sockets */

    /* Sockets included ? */
    if( val != -1)
    {
        struct timeval *ptv = NULL;

        if( timeout >= 0 )
        {
            div_t d    = div( timeout, 1000 );
            tv.tv_sec  = d.quot;
            tv.tv_usec = d.rem * 1000;

            ptv = &tv;
        }

        if (vlc_select( val + 1, &rdset, &wrset, &exset, ptv ) == -1)
            return -1;
    }

    val = 0;
    for( unsigned i = 0; i < nfds; i++ )
    {
        int fd = fds[ i ].fd;

        if( fd >= 0 && fds[ i ].revents == 0 )
        {
            fds[ i ].revents = ( FD_ISSET( fd, &rdset ) ? POLLIN  : 0 )
                             | ( FD_ISSET( fd, &wrset ) ? POLLOUT : 0 )
                             | ( FD_ISSET( fd, &exset ) ? POLLPRI : 0 );
        }

        if( fds[ i ].revents != 0 )
            val++;
    }

    return val;
}

#define Q2LL( q )   ( *( long long * )&( q ))

/*** Clock ***/
vlc_tick_t vlc_tick_now (void)
{
    /* We don't need the real date, just the value of a high precision timer */
    QWORD counter;
    ULONG freq;
    if (DosTmrQueryTime(&counter) || DosTmrQueryFreq(&freq))
        abort();

    /* Convert to from (1/freq) to microsecond resolution */
    /* We need to split the division to avoid 63-bits overflow */
    lldiv_t d = lldiv (Q2LL(counter), freq);

    return vlc_tick_from_sec( d.quot ) + vlc_tick_from_samples(d.rem, freq);
}

#undef vlc_tick_wait
void vlc_tick_wait (vlc_tick_t deadline)
{
    vlc_tick_t delay;

    vlc_testcancel();
    while ((delay = (deadline - vlc_tick_now())) > 0)
    {
        delay /= 1000;
        if (unlikely(delay > 0x7fffffff))
            delay = 0x7fffffff;
        vlc_Sleep (delay);
        vlc_testcancel();
    }
}

#undef vlc_tick_sleep
void vlc_tick_sleep (vlc_tick_t delay)
{
    vlc_tick_wait (vlc_tick_now () + delay);
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
                         vlc_tick_t value, vlc_tick_t interval)
{
    if (timer->htimer != NULLHANDLE)
    {
        DosStopTimer (timer->htimer);
        timer->htimer = NULLHANDLE;
        timer->interval = 0;
    }

    if (value == VLC_TIMER_DISARM)
        return; /* Disarm */

    if (absolute)
        value -= vlc_tick_now ();
    value = (value + 999) / 1000;
    interval = (interval + 999) / 1000;

    timer->interval = MS_FROM_VLC_TICK(interval);
    if (DosAsyncTimer (MS_FROM_VLC_TICK(value), (HSEM)timer->hev, &timer->htimer))
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

            wait_bucket_init();

            vlc_mutex_init (&super_mutex);
            vlc_cond_init (&super_variable);
            vlc_threadvar_create (&thread_key, NULL);

            return 1;

        case 1 :    /* Termination */
            vlc_threadvar_delete (&thread_key);

            wait_bucket_destroy();

            _CRT_term();

            return 1;
    }

    return 0;   /* Failed */
}
