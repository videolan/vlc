/*****************************************************************************
 * threads.h : threads implementation for the VideoLAN client
 * This header provides a portable threads implementation.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: threads.h,v 1.42 2002/04/27 22:11:22 gbazin Exp $
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include <stdio.h>

#if defined(GPROF) || defined(DEBUG)
#   include <sys/time.h>
#endif

#if defined( PTH_INIT_IN_PTH_H )                                  /* GNU Pth */
#   include <pth.h>

#elif defined( ST_INIT_IN_ST_H )                            /* State threads */
#   include <st.h>

#elif defined( WIN32 )
#   include <process.h>

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )  /* pthreads (like Linux & BSD) */
#   include <pthread.h>
#   ifdef DEBUG
/* Needed for pthread_cond_timedwait */
#       include <errno.h>
#   endif
/* This is not prototyped under Linux, though it exists. */
int pthread_mutexattr_setkind_np( pthread_mutexattr_t *attr, int kind );

#elif defined( HAVE_CTHREADS_H )                                  /* GNUMach */
#   include <cthreads.h>

#elif defined( HAVE_KERNEL_SCHEDULER_H )                             /* BeOS */
#   include <kernel/OS.h>
#   include <kernel/scheduler.h>
#   include <byteorder.h>

#else
#   error no threads available on your system !

#endif

/*****************************************************************************
 * Constants
 *****************************************************************************
 * These constants are used by all threads in *_CreateThread() and
 * *_DestroyThreads() functions. Since those calls are non-blocking, an integer
 * value is used as a shared flag to represent the status of the thread.
 *****************************************************************************/

/* Void status - this value can be used to make sure no operation is currently
 * in progress on the concerned thread in an array of recorded threads */
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
 * Type definitions
 *****************************************************************************/

#if defined( PTH_INIT_IN_PTH_H )
typedef pth_t            vlc_thread_t;
typedef pth_mutex_t      vlc_mutex_t;
typedef pth_cond_t       vlc_cond_t;

#elif defined( ST_INIT_IN_ST_H )
typedef st_thread_t *    vlc_thread_t;
typedef st_mutex_t *     vlc_mutex_t;
typedef st_cond_t *      vlc_cond_t;

#elif defined( WIN32 )
typedef HANDLE vlc_thread_t;

typedef struct
{
    CRITICAL_SECTION csection;
    HANDLE           mutex;
} vlc_mutex_t;

typedef struct
{
    int             i_waiting_threads;
    HANDLE          signal;
} vlc_cond_t;

typedef unsigned (__stdcall *PTHREAD_START) (void *);

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
typedef pthread_t        vlc_thread_t;
typedef pthread_mutex_t  vlc_mutex_t;
typedef pthread_cond_t   vlc_cond_t;

#elif defined( HAVE_CTHREADS_H )
typedef cthread_t        vlc_thread_t;

/* Those structs are the ones defined in /include/cthreads.h but we need
 * to handle (*foo) where foo is a (mutex_t) while they handle (foo) where
 * foo is a (mutex_t*) */
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

#elif defined( HAVE_KERNEL_SCHEDULER_H )
/* This is the BeOS implementation of the vlc threads, note that the mutex is
 * not a real mutex and the cond_var is not like a pthread cond_var but it is
 * enough for what wee need */

typedef thread_id vlc_thread_t;

typedef struct
{
    int32           init;
    sem_id          lock;
} vlc_mutex_t;

typedef struct
{
    int32           init;
    thread_id       thread;
} vlc_cond_t;

#endif

typedef void *(*vlc_thread_func_t)(void *p_data);


/*****************************************************************************
 * Prototype for GPROF wrapper
 *****************************************************************************/

#ifdef GPROF
/* Wrapper function for profiling */
static void *      vlc_thread_wrapper ( void *p_wrapper );

#   ifdef WIN32

#       define ITIMER_REAL 1
#       define ITIMER_PROF 2

struct itimerval
{
    struct timeval it_value;
    struct timeval it_interval;
};

int setitimer(int kind, const struct itimerval* itnew, struct itimerval* itold);

#   endif /* WIN32 */

typedef struct wrapper_s
{
    /* Data lock access */
    vlc_mutex_t lock;
    vlc_cond_t  wait;

    /* Data used to spawn the real thread */
    vlc_thread_func_t func;
    void *p_data;

    /* Profiling timer passed to the thread */
    struct itimerval itimer;

} wrapper_t;

#endif /* GPROF */
