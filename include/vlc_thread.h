/*******************************************************************************
 * vlc_thread.h : thread implementation for vieolan client
 * (c)1999 VideoLAN
 ******************************************************************************/

#include <pthread.h>

/*******************************************************************************
 * types definition
 ******************************************************************************/

typedef pthread_t        vlc_thread_t;
typedef pthread_mutex_t  vlc_mutex_t;
typedef pthread_cond_t   vlc_cond_t;

typedef void *(*vlc_thread_func)(void *data);

/******************************************************************************
 * Prototypes
 ******************************************************************************/

static __inline__ int    vlc_thread_create    ( vlc_thread_t * thread, char * name,
                                               vlc_thread_func func, void * data );
static __inline__ void   vlc_thread_exit      ( );
static __inline__ void   vlc_thread_join      ( vlc_thread_t thread );

static __inline__ int    vlc_mutex_init       ( vlc_mutex_t * mutex );
static __inline__ int    vlc_mutex_lock       ( vlc_mutex_t * mutex );
static __inline__ int    vlc_mutex_unlock     ( vlc_mutex_t * mtex );

static __inline__ int    vlc_cond_init        ( vlc_cond_t * condvar );
static __inline__ int    vlc_cond_signal      ( vlc_cond_t * condvar );
static __inline__ int    vlc_cond_wait        ( vlc_cond_t * condvar, vlc_mutex_t * mutex );

//static _inline__ int    vlc_cond_timedwait   ( vlc_cond_t * condvar, vlc_mutex_t * mutex,
//                              mtime_t absoute_timeout_time );

/*******************************************************************************
 * vlc_thread_create
 ******************************************************************************/

static __inline__ int vlc_thread_create(
    vlc_thread_t * thread,
    char * name,
    vlc_thread_func func,
    void * data)
{
    return pthread_create( thread, NULL, func, data );
}

/******************************************************************************
 * vlc_thread_exit
 *******************************************************************************/

static __inline__ void vlc_thread_exit()
{
    pthread_exit( 0 );
}

/*******************************************************************************
 * vlc_thread_exit
 ******************************************************************************/

static __inline__ void vlc_thread_join( vlc_thread_t thread )
{
    pthread_join( thread, NULL );
}

/*******************************************************************************
 * vlc_mutex_init
 *******************************************************************************/

static __inline__ int vlc_mutex_init( vlc_mutex_t * mutex )
{
    return pthread_mutex_init( mutex, NULL );
}

/*******************************************************************************
 * vlc_mutex_lock
 *******************************************************************************/

static __inline__ int vlc_mutex_lock( vlc_mutex_t * mutex )
{
    return pthread_mutex_lock( mutex );
}

/*******************************************************************************
 * vlc_mutex_unlock
 *******************************************************************************/

static __inline__ int vlc_mutex_unlock( vlc_mutex_t * mutex )
{
    return pthread_mutex_unlock( mutex );
}

/*******************************************************************************
 * vlc_cond_init
 *******************************************************************************/

static __inline__ int vlc_cond_init( vlc_cond_t * condvar )
{
    return pthread_cond_init( condvar, NULL );
}

/*******************************************************************************
 * vlc_cond_signal
 *******************************************************************************/

static __inline__ int vlc_cond_signal( vlc_cond_t * condvar )
{
    return pthread_cond_signal( condvar );
}
/*******************************************************************************
 * vlc_cond_wait
 *******************************************************************************/

static __inline__ int vlc_cond_wait( vlc_cond_t * condvar, vlc_mutex_t * mutex )
{
    return pthread_cond_wait( condvar, mutex );
}
