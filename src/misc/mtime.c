/*****************************************************************************
 * mtime.c: high rezolution time management functions
 * Functions are prototyped in mtime.h.
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: mtime.c,v 1.17 2001/04/28 03:36:25 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
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

/*
 * TODO:
 *  see if using Linux real-time extensions is possible and profitable
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdio.h>                                              /* sprintf() */
#include <unistd.h>                                              /* select() */
#include <sys/time.h>

#ifdef HAVE_KERNEL_OS_H
#include <kernel/OS.h>
#endif

#ifdef WIN32
#include <windows.h>
#endif

#include "config.h"
#include "common.h"
#include "mtime.h"

/*****************************************************************************
 * mstrtime: return a date in a readable format
 *****************************************************************************
 * This functions is provided for any interface function which need to print a
 * date. psz_buffer should be a buffer long enough to store the formatted
 * date.
 *****************************************************************************/
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

/*****************************************************************************
 * mdate: return high precision date (inline function)
 *****************************************************************************
 * Uses the gettimeofday() function when possible (1 MHz resolution) or the
 * ftime() function (1 kHz resolution).
 *****************************************************************************/
mtime_t mdate( void )
{
#if defined( HAVE_KERNEL_OS_H )
    return( real_time_clock_usecs() );

#elif defined( WIN32 )
    /* We don't get the real date, just the value of a high precision timer.
     * this is because the usual time functions have at best only a milisecond
     * resolution */
    mtime_t freq,usec_time;

    if( !QueryPerformanceFrequency((LARGE_INTEGER *)&freq) )
    {
        /* Milisecond resolution */
        FILETIME file_time;
        GetSystemTimeAsFileTime((FILETIME *)&file_time);
        usec_time *= 1000;
    }
    else
    {
        /* Microsecond resolution */
        QueryPerformanceCounter((LARGE_INTEGER *)&usec_time);
   	usec_time /= (freq/1000000);
    }
    return( usec_time );

#else
    struct timeval tv_date;

    /* gettimeofday() could return an error, and should be tested. However, the
     * only possible error, according to 'man', is EFAULT, which can not happen
     * here, since tv is a local variable. */
    gettimeofday( &tv_date, NULL );
    return( (mtime_t) tv_date.tv_sec * 1000000 + (mtime_t) tv_date.tv_usec );

#endif
}

/*****************************************************************************
 * mwait: wait for a date (inline function)
 *****************************************************************************
 * This function uses select() and an system date function to wake up at a
 * precise date. It should be used for process synchronization. If current date
 * is posterior to wished date, the function returns immediately.
 *****************************************************************************/
void mwait( mtime_t date )
{
#if defined( HAVE_KERNEL_OS_H )
    mtime_t delay;
    
    delay = date - real_time_clock_usecs();
    if( delay <= 0 )
    {
        return;
    }
    snooze( delay );

#elif defined( WIN32 )
    mtime_t usec_time, delay;

    usec_time = mdate();
    delay = date - usec_time;
    if( delay <= 0 )
    {
        return;
    }
    /* Sleep only has milisecond resolution */
    Sleep( (DWORD)(delay/1000) );

#else

#   ifdef HAVE_USLEEP
    struct timeval tv_date;
#   else
    struct timeval tv_date, tv_delay;
#   endif
    mtime_t        delay;          /* delay in msec, signed to detect errors */

    /* see mdate() about gettimeofday() possible errors */
    gettimeofday( &tv_date, NULL );

    /* calculate delay and check if current date is before wished date */
    delay = date - (mtime_t) tv_date.tv_sec * 1000000 - (mtime_t) tv_date.tv_usec - 10000;
    /* Linux/i386 has a granularity of 10 ms. It's better to be in advance
     * than to be late. */
    if( delay <= 0 )                 /* wished date is now or already passed */
    {
        return;
    }

#   ifdef HAVE_USLEEP
    usleep( delay );
#   else
    tv_delay.tv_sec = delay / 1000000;
    tv_delay.tv_usec = delay % 1000000;

    /* see msleep() about select() errors */
    select( 0, NULL, NULL, NULL, &tv_delay );
#   endif

#endif
}

/*****************************************************************************
 * msleep: more precise sleep() (inline function)                        (ok ?)
 *****************************************************************************
 * Portable usleep() function.
 *****************************************************************************/
void msleep( mtime_t delay )
{
#if defined( HAVE_KERNEL_OS_H )
    snooze( delay );

#elif defined( WIN32 )
    Sleep( delay/1000 );             /* Sleep only has milisecond resolution */
  /* Maybe we could use the multimedia timer to reach the right resolution,  */
  /* or the old Winsock select() function ?*/

#elif defined( HAVE_USLEEP )
    usleep( delay );

#else
    struct timeval tv_delay;

    tv_delay.tv_sec = delay / 1000000;
    tv_delay.tv_usec = delay % 1000000;
    /* select() return value should be tested, since several possible errors
     * can occur. However, they should only happen in very particular occasions
     * (i.e. when a signal is sent to the thread, or when memory is full), and
     * can be ingnored. */
    select( 0, NULL, NULL, NULL, &tv_delay );

#endif
}

