/******************************************************************************
 * mtime.c: high rezolution time management functions
 * (c)1998 VideoLAN
 ******************************************************************************
 * Functions are prototyped in mtime.h.
 ******************************************************************************
 * to-do list:
 *  see if using Linux real-time extensions is possible and profitable
 ******************************************************************************/

/******************************************************************************
 * Preamble
 ******************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

#include "common.h"
#include "mtime.h"

/******************************************************************************
 * mstrtime: return a date in a readable format
 ******************************************************************************
 * This functions is provided for any interface function which need to print a
 * date. psz_buffer should be a buffer long enough to store the formatted
 * date.
 ******************************************************************************/
char *mstrtime( char *psz_buffer, mtime_t date )
{
    sprintf( psz_buffer, "%02d:%02d:%02d-%03d.%03d",
             (int) (date / (1000LL * 1000LL * 60LL * 60LL) % 24LL),
             (int) (date / (1000LL * 1000LL * 60LL) % 60LL),
             (int) (date / (1000LL * 1000LL) % 60LL),
             (int) (date / 1000LL % 1000LL),
             (int) (date % 1000LL) );
    return( psz_buffer );
}

/******************************************************************************
 * mdate: return high precision date (inline function)
 ******************************************************************************
 * Uses the gettimeofday() function when possible (1 MHz resolution) or the
 * ftime() function (1 kHz resolution).
 ******************************************************************************
 * to-do list: ??
 *  implement the function when gettimeofday is not available
 *  this function should be decalred as inline
 ******************************************************************************/
mtime_t mdate( void )
{
    struct timeval tv_date;

    /* gettimeofday() could return an error, and should be tested. However, the
     * only possible error, according to 'man', is EFAULT, which can not happen
     * here, since tv is a local variable. */
    gettimeofday( &tv_date, NULL );
    return( (mtime_t) tv_date.tv_sec * 1000000 + (mtime_t) tv_date.tv_usec );
}

/******************************************************************************
 * mwait: wait for a date (inline function)
 ******************************************************************************
 * This function uses select() and an system date function to wake up at a
 * precise date. It should be used for process synchronization. If current date
 * is posterior to wished date, the function returns immediately.
 ******************************************************************************
 * to-do list:
 *  implement the function when gettimeofday is not available
 *  optimize delay calculation
 *  ?? declare as inline
 ******************************************************************************/
void mwait( mtime_t date )
{
    struct timeval tv_date, tv_delay;
    mtime_t        delay;           /* delay in msec, signed to detect errors */

    /* see mdate() about gettimeofday() possible errors */
    gettimeofday( &tv_date, NULL );

    /* calculate delay and check if current date is before wished date */
    delay = date - (mtime_t) tv_date.tv_sec * 1000000 - (mtime_t) tv_date.tv_usec;
    if( delay <= 0 )                  /* wished date is now or already passed */
    {
        return;
    }
#ifndef usleep
    tv_delay.tv_sec = delay / 1000000;
    tv_delay.tv_usec = delay % 1000000;

    /* see msleep() about select() errors */
    select( 0, NULL, NULL, NULL, &tv_delay );
#else
    usleep( delay );    
#endif
}

/******************************************************************************
 * msleep: more precise sleep() (inline function)                        (ok ?)
 ******************************************************************************
 * Portable usleep() function.
 ******************************************************************************/
void msleep( mtime_t delay )
{
#ifndef usleep
    struct timeval tv_delay;

    tv_delay.tv_sec = delay / 1000000;
    tv_delay.tv_usec = delay % 1000000;
    /* select() return value should be tested, since several possible errors
     * can occur. However, they should only happen in very particular occasions
     * (i.e. when a signal is sent to the thread, or when memory is full), and
     * can be ingnored. */
    select( 0, NULL, NULL, NULL, &tv_delay );
#else
    usleep( delay );
#endif
}
