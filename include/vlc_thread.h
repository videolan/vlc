/*****************************************************************************
 * vlc_thread.h : thread implementation for vieolan client
 * (c)1999 VideoLAN
 *****************************************************************************
 * This header is supposed to provide a portable threads implementation.
 * Currently, it is a wrapper to either the POSIX pthreads library, or
 * the Mach cthreads (for the GNU/Hurd).
 *****************************************************************************/
#ifdef SYS_GNU
#include <cthreads.h>
#else
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

#else /* SYS_GNU */

typedef pthread_t        vlc_thread_t;
typedef pthread_mutex_t  vlc_mutex_t;
typedef pthread_cond_t   vlc_cond_t;

#endif /* SYS_GNU */

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
#else
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
#else		
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
#else	
    pthread_join( thread, NULL );
#endif
}

/*****************************************************************************
 * vlc_mutex_init: initialize a mutex
 *****************************************************************************/
static __inline__ int vlc_mutex_init( vlc_mutex_t *p_mutex )
{
#ifdef SYS_GNU
    mutex_init( p_mutex );
    return( 0 );
#else
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
#else
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
#else
    return pthread_mutex_unlock( p_mutex );
#endif
}

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
#else			    
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
#else		
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
#else
    return pthread_cond_wait( p_condvar, p_mutex );
#endif
}

