/*******************************************************************************
 * vlc_thread.h : thread implementation for vieolan client
 * (c)1999 VideoLAN
 *******************************************************************************
 * This header is supposed to provide a portable threads implementation. 
 * Currently, it is a wrapper to the POSIX pthreads library.
 *******************************************************************************/
#include <pthread.h>

/******************************************************************************
 * Constants
 ******************************************************************************
 * These constants are used by all threads in *_CreateThread() and 
 * *_DestroyThreads() functions. Since those calls are non-blocking, an integer
 * value is used as a shared flag to represent the status of the thread.
 *******************************************************************************/

/* Void status - this value can be used to be sure, in an array of recorded
 * threads, that no operation is currently in progress on the concerned thread */
#define THREAD_NOP          0                              /* nothing happened */

/* Creation status */
#define THREAD_CREATE       10                       /* thread is initializing */
#define THREAD_START        11                            /* thread has forked */
#define THREAD_READY        19                              /* thread is ready */

/* Destructions status */
#define THREAD_DESTROY      20              /* destruction order has been sent */
#define THREAD_END          21          /* destruction order has been received */
#define THREAD_OVER         29               /* thread does not exist any more */

/* Error status */
#define THREAD_ERROR        30                             /* an error occured */
#define THREAD_FATAL        31    /* an fatal error occured - program must end */

/******************************************************************************
 * Types definition
 ******************************************************************************/
typedef pthread_t        vlc_thread_t;
typedef pthread_mutex_t  vlc_mutex_t;
typedef pthread_cond_t   vlc_cond_t;

typedef void *(*vlc_thread_func_t)(void *p_data);

/******************************************************************************
 * Prototypes
 ******************************************************************************/

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

//static _inline__ int    vlc_cond_timedwait   ( vlc_cond_t * condvar, vlc_mutex_t * mutex,
//                              mtime_t absoute_timeout_time );

/*******************************************************************************
 * vlc_thread_create: create a thread
 ******************************************************************************/
static __inline__ int vlc_thread_create( vlc_thread_t *p_thread,
					 char *psz_name, vlc_thread_func_t func,
					 void *p_data)
{
    return pthread_create( p_thread, NULL, func, p_data );
}

/******************************************************************************
 * vlc_thread_exit: terminate a thread
 *******************************************************************************/
static __inline__ void vlc_thread_exit( void )
{
    pthread_exit( 0 );
}

/*******************************************************************************
 * vlc_thread_join: wait until a thread exits
 ******************************************************************************/
static __inline__ void vlc_thread_join( vlc_thread_t thread )
{
    pthread_join( thread, NULL );
}

/*******************************************************************************
 * vlc_mutex_init: initialize a mutex
 *******************************************************************************/
static __inline__ int vlc_mutex_init( vlc_mutex_t *p_mutex )
{
    return pthread_mutex_init( p_mutex, NULL );
}

/*******************************************************************************
 * vlc_mutex_lock: lock a mutex
 *******************************************************************************/
static __inline__ int vlc_mutex_lock( vlc_mutex_t *p_mutex )
{
    return pthread_mutex_lock( p_mutex );
}

/*******************************************************************************
 * vlc_mutex_unlock: unlock a mutex
 *******************************************************************************/
static __inline__ int vlc_mutex_unlock( vlc_mutex_t *p_mutex )
{
    return pthread_mutex_unlock( p_mutex );
}

/*******************************************************************************
 * vlc_cond_init: initialize a condition
 *******************************************************************************/
static __inline__ int vlc_cond_init( vlc_cond_t *p_condvar )
{
    return pthread_cond_init( p_condvar, NULL );
}

/*******************************************************************************
 * vlc_cond_signal: start a thread on condition completion
 *******************************************************************************/
static __inline__ int vlc_cond_signal( vlc_cond_t *p_condvar )
{
    return pthread_cond_signal( p_condvar );
}

/*******************************************************************************
 * vlc_cond_wait: wait until condition completion
 *******************************************************************************/
static __inline__ int vlc_cond_wait( vlc_cond_t *p_condvar, vlc_mutex_t *p_mutex )
{
    return pthread_cond_wait( p_condvar, p_mutex );
}
