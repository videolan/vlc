/*****************************************************************************
 * threads.h : thread implementation for VideoLAN client
 * This header is supposed to provide a portable threads implementation.
 * Currently, it is a wrapper to either the POSIX pthreads library, or
 * the Mach cthreads (for the GNU/Hurd).
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/


#ifdef SYS_GNU
#include <cthreads.h>
#endif

#ifdef SYS_BEOS
#include <kernel/OS.h>
#include <kernel/scheduler.h>
#endif

#if defined(SYS_LINUX) || defined(SYS_BSD)
#include <pthread.h>
#endif

/*****************************************************************************
 * Constants
 *****************************************************************************
 * These constants are used by all threads in *_CreateThread() and
 * *_DestroyThreads() functions. Since those calls are non-blocking, an integer
 * value is used as a shared flag to represent the status of the thread.
 *****************************************************************************/

/* Void status - this value can be used to be sure, in an array of recorded
 * threads, that no operation is currently in progress on the concerned thread */
#define THREAD_NOP          0                            /* nothing happened */

/* Creation status */
#define THREAD_CREATE       10                     /* thread is initializing */
#define THREAD_START        11                          /* thread has forked */
#define THREAD_READY        19                            /* thread is ready */

/* Destructions status */
#define THREAD_DESTROY      20            /* destruction order has been sent */
#define THREAD_END          21        /* destruction order has been received */
#define THREAD_OVER         29             /* thread does not exist any more */

/* Error status */
#define THREAD_ERROR        30                           /* an error occured */
#define THREAD_FATAL        31  /* an fatal error occured - program must end */

/*****************************************************************************
 * Types definition
 *****************************************************************************/

#ifdef SYS_GNU

typedef cthread_t        vlc_thread_t;

/* those structs are the ones defined in /include/cthreads.h but we need
 *  * to handle *foo where foo is a mutex_t */
typedef struct s_mutex {
    spin_lock_t held;
    spin_lock_t lock;
    char *name;
    struct cthread_queue queue;
} vlc_mutex_t;

typedef struct s_condition {
    spin_lock_t lock;
    struct cthread_queue queue;
    char *name;
    struct cond_imp *implications;
} vlc_cond_t;

#endif /* SYS_GNU */

#ifdef SYS_BEOS

typedef thread_id vlc_thread_t;

typedef struct
{
    int32           init;
    sem_id          lock;
    thread_id       owner;
} vlc_mutex_t;

typedef struct
{
    int32           init;
    sem_id          sem;
    sem_id          handshakeSem;
    sem_id          signalSem;
    volatile int32  nw;
    volatile int32  ns;
} vlc_cond_t;

#endif /* SYS_BEOS */

#if defined(SYS_LINUX) || defined(SYS_BSD)

typedef pthread_t        vlc_thread_t;
typedef pthread_mutex_t  vlc_mutex_t;
typedef pthread_cond_t   vlc_cond_t;

#endif /* SYS_LINUX || SYS_BSD */

typedef void *(*vlc_thread_func_t)(void *p_data);

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

static __inline__ int  vlc_thread_create( vlc_thread_t *p_thread, char *psz_name,
                                          vlc_thread_func_t func, void *p_data );
static __inline__ void vlc_thread_exit  ( void );
static __inline__ void vlc_thread_join  ( vlc_thread_t thread );

static __inline__ int  vlc_mutex_init   ( vlc_mutex_t *p_mutex );
static __inline__ int  vlc_mutex_lock   ( vlc_mutex_t *p_mutex );
static __inline__ int  vlc_mutex_unlock ( vlc_mutex_t *p_mutex );

static __inline__ int  vlc_cond_init    ( vlc_cond_t *p_condvar );
static __inline__ int  vlc_cond_signal  ( vlc_cond_t *p_condvar );
static __inline__ int  vlc_cond_wait    ( vlc_cond_t *p_condvar, vlc_mutex_t *p_mutex );

#if 0
static _inline__ int    vlc_cond_timedwait   ( vlc_cond_t * condvar, vlc_mutex_t * mutex,
                              mtime_t absoute_timeout_time );
#endif

/*****************************************************************************
 * vlc_thread_create: create a thread
 *****************************************************************************/
static __inline__ int vlc_thread_create( vlc_thread_t *p_thread,
                                         char *psz_name, vlc_thread_func_t func,
                                         void *p_data)
{
#ifdef SYS_GNU
    *p_thread = cthread_fork( (cthread_fn_t)func, (any_t)p_data );
    return( 0 );
#endif

#ifdef SYS_BEOS
    *p_thread = spawn_thread( (thread_func)func, psz_name, B_NORMAL_PRIORITY, p_data );
    return resume_thread( *p_thread );
#endif

#if defined(SYS_LINUX) || defined(SYS_BSD)
    return pthread_create( p_thread, NULL, func, p_data );
#endif
}

/*****************************************************************************
 * vlc_thread_exit: terminate a thread
 *****************************************************************************/
static __inline__ void vlc_thread_exit( void )
{
#ifdef SYS_GNU
    int result;
    cthread_exit( &result );
#endif

#ifdef SYS_BEOS
    exit_thread( 0 );
#endif

#if defined(SYS_LINUX) || defined(SYS_BSD)	
    pthread_exit( 0 );
#endif
}

/*****************************************************************************
 * vlc_thread_join: wait until a thread exits
 *****************************************************************************/
static __inline__ void vlc_thread_join( vlc_thread_t thread )
{
#ifdef SYS_GNU
    cthread_join( thread );
#endif

#ifdef SYS_BEOS
    int32 exit_value;	
    wait_for_thread( thread, &exit_value );
#endif

#if defined(SYS_LINUX) || defined(SYS_BSD)	
    pthread_join( thread, NULL );
#endif
}

#ifdef SYS_BEOS
/* lazy_init_mutex */
static __inline__ void lazy_init_mutex(vlc_mutex_t* p_mutex)
{
    int32 v = atomic_or( &p_mutex->init, 1 );
    if( 2000 == v ) // we're the first, so do the init
    {
        vlc_mutex_init( p_mutex );
    }
    else // we're not the first, so wait until the init is finished
    {
        while( p_mutex->init != 9999 ) snooze( 10000 );
    }
}
#endif

/*****************************************************************************
 * vlc_mutex_init: initialize a mutex
 *****************************************************************************/
static __inline__ int vlc_mutex_init( vlc_mutex_t *p_mutex )
{
#ifdef SYS_GNU
    mutex_init( p_mutex );
    return( 0 );
#endif

#ifdef SYS_BEOS
    // check the arguments and whether it's already been initialized
    if( !p_mutex ) return B_BAD_VALUE;
    if( p_mutex->init == 9999 ) return EALREADY;

    p_mutex->lock = create_sem( 1, "BeMutex" );
    p_mutex->owner = -1;
    p_mutex->init = 9999;
    return B_OK;
#endif

#if defined(SYS_LINUX) || defined(SYS_BSD)	
    return pthread_mutex_init( p_mutex, NULL );
#endif
}

/*****************************************************************************
 * vlc_mutex_lock: lock a mutex
 *****************************************************************************/
static __inline__ int vlc_mutex_lock( vlc_mutex_t *p_mutex )
{
#ifdef SYS_GNU
    mutex_lock( p_mutex );
    return( 0 );
#endif

#ifdef SYS_BEOS
    status_t err;

    if( !p_mutex ) return B_BAD_VALUE;
    if( p_mutex->init < 2000 ) return B_NO_INIT;
    lazy_init_mutex( p_mutex );

    err = acquire_sem( p_mutex->lock );
    if( !err ) p_mutex->owner = find_thread( NULL );
    return err;
#endif

#if defined(SYS_LINUX) || defined(SYS_BSD)	
    return pthread_mutex_lock( p_mutex );
#endif
}

/*****************************************************************************
 * vlc_mutex_unlock: unlock a mutex
 *****************************************************************************/
static __inline__ int vlc_mutex_unlock( vlc_mutex_t *p_mutex )
{
#ifdef SYS_GNU
    mutex_unlock( p_mutex );
    return( 0 );
#endif

#ifdef SYS_BEOS
    if(! p_mutex) return B_BAD_VALUE;
    if( p_mutex->init < 2000 ) return B_NO_INIT;
    lazy_init_mutex( p_mutex );

    if( p_mutex->owner != find_thread(NULL) ) return ENOLCK;
    p_mutex->owner = -1;
    release_sem( p_mutex->lock );
    return B_OK;
#endif

#if defined(SYS_LINUX) || defined(SYS_BSD)	
    return pthread_mutex_unlock( p_mutex );
#endif
}

#ifdef SYS_BEOS
/* lazy_init_cond */
static __inline__ void lazy_init_cond( vlc_cond_t* p_condvar )
{
    int32 v = atomic_or( &p_condvar->init, 1 );
    if( 2000 == v ) // we're the first, so do the init
    {
        vlc_cond_init( p_condvar );
    }
    else // we're not the first, so wait until the init is finished
    {
        while( p_condvar->init != 9999 ) snooze( 10000 );
    }
}
#endif

/*****************************************************************************
 * vlc_cond_init: initialize a condition
 *****************************************************************************/
static __inline__ int vlc_cond_init( vlc_cond_t *p_condvar )
{
#ifdef SYS_GNU
    /* condition_init() */
    spin_lock_init( &p_condvar->lock );
    cthread_queue_init( &p_condvar->queue );
    p_condvar->name = 0;
    p_condvar->implications = 0;

    return( 0 );
#endif

#ifdef SYS_BEOS
    if( !p_condvar ) return B_BAD_VALUE;
    if( p_condvar->init == 9999 ) return EALREADY;

    p_condvar->sem = create_sem( 0, "CVSem" );
    p_condvar->handshakeSem = create_sem( 0, "CVHandshake" );
    p_condvar->signalSem = create_sem( 1, "CVSignal" );
    p_condvar->ns = p_condvar->nw = 0;
    p_condvar->init = 9999;
    return B_OK;
#endif

#if defined(SYS_LINUX) || defined(SYS_BSD)	
    return pthread_cond_init( p_condvar, NULL );
#endif
}

/*****************************************************************************
 * vlc_cond_signal: start a thread on condition completion
 *****************************************************************************/
static __inline__ int vlc_cond_signal( vlc_cond_t *p_condvar )
{
#ifdef SYS_GNU
    /* condition_signal() */
    if ( p_condvar->queue.head || p_condvar->implications )
    {
        cond_signal( (condition_t)p_condvar );
    }
    return( 0 );
#endif

#ifdef SYS_BEOS
    status_t err = B_OK;

    if( !p_condvar ) return B_BAD_VALUE;
    if( p_condvar->init < 2000 ) return B_NO_INIT;
    lazy_init_cond( p_condvar );

    if( acquire_sem(p_condvar->signalSem) == B_INTERRUPTED) return B_INTERRUPTED;

    if( p_condvar->nw > p_condvar->ns )
    {
        p_condvar->ns += 1;
        release_sem( p_condvar->sem );
        release_sem( p_condvar->signalSem );
        while( acquire_sem(p_condvar->handshakeSem) == B_INTERRUPTED )
            { err = B_INTERRUPTED; }
    }
    else
    {
        release_sem( p_condvar->signalSem );
    }
    return err;
#endif

#if defined(SYS_LINUX) || defined(SYS_BSD)	
    return pthread_cond_signal( p_condvar );
#endif
}

/*****************************************************************************
 * vlc_cond_wait: wait until condition completion
 *****************************************************************************/
static __inline__ int vlc_cond_wait( vlc_cond_t *p_condvar, vlc_mutex_t *p_mutex )
{
#ifdef SYS_GNU
    condition_wait( (condition_t)p_condvar, (mutex_t)p_mutex );
    return( 0 );
#endif

#ifdef SYS_BEOS
    status_t err;

    if( !p_condvar ) return B_BAD_VALUE;
    if( !p_mutex ) return B_BAD_VALUE;
    if( p_condvar->init < 2000 ) return B_NO_INIT;
    lazy_init_cond( p_condvar );

    if( acquire_sem(p_condvar->signalSem) == B_INTERRUPTED ) return B_INTERRUPTED;
    p_condvar->nw += 1;
    release_sem( p_condvar->signalSem );

    vlc_mutex_unlock( p_mutex );
    err = acquire_sem( p_condvar->sem );

    while( acquire_sem(p_condvar->signalSem) == B_INTERRUPTED)
        { err = B_INTERRUPTED; }
    if( p_condvar->ns > 0 )
    {
        release_sem( p_condvar->handshakeSem );
        p_condvar->ns -= 1;
    }
    p_condvar->nw -= 1;
    release_sem( p_condvar->signalSem );

    while( vlc_mutex_lock(p_mutex) == B_INTERRUPTED)
        { err = B_INTERRUPTED; }
    return err;
#endif

#if defined(SYS_LINUX) || defined(SYS_BSD)	
    return pthread_cond_wait( p_condvar, p_mutex );
#endif
}
