/*******************************************************************************
 * thread.h: threads status constants
 * (c)1999 VideoLAN
 *******************************************************************************
 * These constants are used by all threads in *_CreateThread() and 
 * *_DestroyThreads() functions. Since those calls are non-blocking, an integer
 * value is used as a shared flag to represent the status of the thread.
 *******************************************************************************
 * Requires:
 *  none
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




