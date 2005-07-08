/*****************************************************************************
 * vlc_threads.h : threads implementation for the VideoLAN client
 * This header provides portable declarations for mutexes & conditions
 *****************************************************************************
 * Copyright (C) 1999, 2002 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

#if defined(DEBUG) && defined(HAVE_SYS_TIME_H)
#   include <sys/time.h>
#endif

#if defined( PTH_INIT_IN_PTH_H )                                  /* GNU Pth */
#   include <pth.h>

#elif defined( ST_INIT_IN_ST_H )                            /* State threads */
#   include <st.h>

#elif defined( UNDER_CE )
                                                                /* WinCE API */
#elif defined( WIN32 )
#   include <process.h>                                         /* Win32 API */

#elif defined( HAVE_KERNEL_SCHEDULER_H )                             /* BeOS */
#   include <kernel/OS.h>
#   include <kernel/scheduler.h>
#   include <byteorder.h>

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

#else
#   error no threads available on your system !

#endif

/*****************************************************************************
 * Constants
 *****************************************************************************/

/* Thread priorities */
#ifdef SYS_DARWIN
#   define VLC_THREAD_PRIORITY_LOW (-47)
#   define VLC_THREAD_PRIORITY_INPUT 37
#   define VLC_THREAD_PRIORITY_AUDIO 37
#   define VLC_THREAD_PRIORITY_VIDEO (-47)
#   define VLC_THREAD_PRIORITY_OUTPUT 37
#   define VLC_THREAD_PRIORITY_HIGHEST 37

#elif defined(SYS_BEOS)
#   define VLC_THREAD_PRIORITY_LOW 5
#   define VLC_THREAD_PRIORITY_INPUT 10
#   define VLC_THREAD_PRIORITY_AUDIO 10
#   define VLC_THREAD_PRIORITY_VIDEO 5
#   define VLC_THREAD_PRIORITY_OUTPUT 15
#   define VLC_THREAD_PRIORITY_HIGHEST 15

#elif defined(PTHREAD_COND_T_IN_PTHREAD_H)
#   define VLC_THREAD_PRIORITY_LOW 0
#   define VLC_THREAD_PRIORITY_INPUT 20
#   define VLC_THREAD_PRIORITY_AUDIO 10
#   define VLC_THREAD_PRIORITY_VIDEO 0
#   define VLC_THREAD_PRIORITY_OUTPUT 30
#   define VLC_THREAD_PRIORITY_HIGHEST 40

#elif defined(WIN32) || defined(UNDER_CE)
/* Define different priorities for WinNT/2K/XP and Win9x/Me */
#   define VLC_THREAD_PRIORITY_LOW 0
#   define VLC_THREAD_PRIORITY_INPUT \
        (IS_WINNT ? THREAD_PRIORITY_ABOVE_NORMAL : 0)
#   define VLC_THREAD_PRIORITY_AUDIO \
        (IS_WINNT ? THREAD_PRIORITY_HIGHEST : 0)
#   define VLC_THREAD_PRIORITY_VIDEO \
        (IS_WINNT ? 0 : THREAD_PRIORITY_BELOW_NORMAL )
#   define VLC_THREAD_PRIORITY_OUTPUT \
        (IS_WINNT ? THREAD_PRIORITY_ABOVE_NORMAL : 0)
#   define VLC_THREAD_PRIORITY_HIGHEST \
        (IS_WINNT ? THREAD_PRIORITY_TIME_CRITICAL : 0)

#else
#   define VLC_THREAD_PRIORITY_LOW 0
#   define VLC_THREAD_PRIORITY_INPUT 0
#   define VLC_THREAD_PRIORITY_AUDIO 0
#   define VLC_THREAD_PRIORITY_VIDEO 0
#   define VLC_THREAD_PRIORITY_OUTPUT 0
#   define VLC_THREAD_PRIORITY_HIGHEST 0

#endif

/*****************************************************************************
 * Type definitions
 *****************************************************************************/

#if defined( PTH_INIT_IN_PTH_H )
typedef pth_t            vlc_thread_t;
typedef struct
{
    pth_mutex_t mutex;
    vlc_object_t * p_this;
} vlc_mutex_t;
typedef struct
{
    pth_cond_t cond;
    vlc_object_t * p_this;
} vlc_cond_t;

#elif defined( ST_INIT_IN_ST_H )
typedef st_thread_t      vlc_thread_t;
typedef struct
{
    st_mutex_t mutex;
    vlc_object_t * p_this;
} vlc_mutex_t;
typedef struct
{
    st_cond_t cond;
    vlc_object_t * p_this;
} vlc_cond_t;

#elif defined( WIN32 ) || defined( UNDER_CE )
typedef HANDLE vlc_thread_t;
typedef BOOL (WINAPI *SIGNALOBJECTANDWAIT) ( HANDLE, HANDLE, DWORD, BOOL );
typedef unsigned (WINAPI *PTHREAD_START) (void *);

typedef struct
{
    /* WinNT/2K/XP implementation */
    HANDLE              mutex;
    /* Win95/98/ME implementation */
    CRITICAL_SECTION    csection;

    vlc_object_t * p_this;
} vlc_mutex_t;

typedef struct
{
    volatile int        i_waiting_threads;
    /* WinNT/2K/XP implementation */
    HANDLE              event;
    SIGNALOBJECTANDWAIT SignalObjectAndWait;
    /* Win95/98/ME implementation */
    HANDLE              semaphore;
    CRITICAL_SECTION    csection;
    int                 i_win9x_cv;

    vlc_object_t * p_this;
} vlc_cond_t;

#elif defined( HAVE_KERNEL_SCHEDULER_H )
/* This is the BeOS implementation of the vlc threads, note that the mutex is
 * not a real mutex and the cond_var is not like a pthread cond_var but it is
 * enough for what wee need */

typedef thread_id vlc_thread_t;

typedef struct
{
    int32_t         init;
    sem_id          lock;

    vlc_object_t * p_this;
} vlc_mutex_t;

typedef struct
{
    int32_t         init;
    thread_id       thread;

    vlc_object_t * p_this;
} vlc_cond_t;

#elif defined( PTHREAD_COND_T_IN_PTHREAD_H )
typedef pthread_t       vlc_thread_t;
typedef struct
{
    pthread_mutex_t mutex;
    vlc_object_t * p_this;
} vlc_mutex_t;
typedef struct
{
    pthread_cond_t cond;
    vlc_object_t * p_this;
} vlc_cond_t;

#elif defined( HAVE_CTHREADS_H )
typedef cthread_t       vlc_thread_t;

/* Those structs are the ones defined in /include/cthreads.h but we need
 * to handle (&foo) where foo is a (mutex_t) while they handle (foo) where
 * foo is a (mutex_t*) */
typedef struct
{
    spin_lock_t held;
    spin_lock_t lock;
    char *name;
    struct cthread_queue queue;

    vlc_object_t * p_this;
} vlc_mutex_t;

typedef struct
{
    spin_lock_t lock;
    struct cthread_queue queue;
    char *name;
    struct cond_imp *implications;

    vlc_object_t * p_this;
} vlc_cond_t;

#endif

