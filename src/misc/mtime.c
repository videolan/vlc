/*****************************************************************************
 * mtime.c: high resolution time management functions
 * Functions are prototyped in vlc_mtime.h.
 *****************************************************************************
 * Copyright (C) 1998-2007 the VideoLAN team
 * Copyright © 2006-2007 Rémi Denis-Courmont
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Rémi Denis-Courmont <rem$videolan,org>
 *          Gisle Vanem
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <time.h>                      /* clock_gettime(), clock_nanosleep() */
#include <assert.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>                                           /* select() */
#endif

#ifdef HAVE_KERNEL_OS_H
#   include <kernel/OS.h>
#endif

#if defined( WIN32 ) || defined( UNDER_CE )
#   include <windows.h>
#   include <mmsystem.h>
#endif

#if defined(HAVE_SYS_TIME_H)
#   include <sys/time.h>
#endif

#if defined(__APPLE__) && !defined(__powerpc__) && !defined(__ppc__) && !defined(__ppc64__)
#define USE_APPLE_MACH 1
#   include <mach/mach.h>
#   include <mach/mach_time.h>
#endif

#if !defined(HAVE_STRUCT_TIMESPEC)
struct timespec
{
    time_t  tv_sec;
    int32_t tv_nsec;
};
#endif

#if defined(HAVE_NANOSLEEP) && !defined(HAVE_DECL_NANOSLEEP)
int nanosleep(struct timespec *, struct timespec *);
#endif

#if !defined (_POSIX_CLOCK_SELECTION)
#  define _POSIX_CLOCK_SELECTION (-1)
#endif

# if (_POSIX_CLOCK_SELECTION < 0)
/*
 * We cannot use the monotonic clock if clock selection is not available,
 * as it would screw vlc_cond_timedwait() completely. Instead, we have to
 * stick to the realtime clock. Nevermind it screws everything up when ntpdate
 * warps the wall clock.
 */
#  undef CLOCK_MONOTONIC
#  define CLOCK_MONOTONIC CLOCK_REALTIME
#elif !defined (HAVE_CLOCK_NANOSLEEP)
/* Clock selection without clock in the first place, I don't think so. */
#  error We have quite a situation here! Fix me if it ever happens.
#endif

/**
 * Return a date in a readable format
 *
 * This function converts a mtime date into a string.
 * psz_buffer should be a buffer long enough to store the formatted
 * date.
 * \param date to be converted
 * \param psz_buffer should be a buffer at least MSTRTIME_MAX_SIZE characters
 * \return psz_buffer is returned so this can be used as printf parameter.
 */
char *mstrtime( char *psz_buffer, mtime_t date )
{
    static const mtime_t ll1000 = 1000, ll60 = 60, ll24 = 24;

    snprintf( psz_buffer, MSTRTIME_MAX_SIZE, "%02d:%02d:%02d-%03d.%03d",
             (int) (date / (ll1000 * ll1000 * ll60 * ll60) % ll24),
             (int) (date / (ll1000 * ll1000 * ll60) % ll60),
             (int) (date / (ll1000 * ll1000) % ll60),
             (int) (date / ll1000 % ll1000),
             (int) (date % ll1000) );
    return( psz_buffer );
}

/**
 * Convert seconds to a time in the format h:mm:ss.
 *
 * This function is provided for any interface function which need to print a
 * time string in the format h:mm:ss
 * date.
 * \param secs  the date to be converted
 * \param psz_buffer should be a buffer at least MSTRTIME_MAX_SIZE characters
 * \return psz_buffer is returned so this can be used as printf parameter.
 */
char *secstotimestr( char *psz_buffer, int32_t i_seconds )
{
    if( unlikely(i_seconds < 0) )
    {
        secstotimestr( psz_buffer + 1, -i_seconds );
        *psz_buffer = '-';
        return psz_buffer;
    }

    div_t d;

    d = div( i_seconds, 60 );
    i_seconds = d.rem;
    d = div( d.quot, 60 );

    if( d.quot )
        snprintf( psz_buffer, MSTRTIME_MAX_SIZE, "%u:%02u:%02u",
                 d.quot, d.rem, i_seconds );
    else
        snprintf( psz_buffer, MSTRTIME_MAX_SIZE, "%02u:%02u",
                  d.rem, i_seconds );
    return psz_buffer;
}

#if defined (HAVE_CLOCK_NANOSLEEP)
static unsigned prec = 0;

static void mprec_once( void )
{
    struct timespec ts;
    if( clock_getres( CLOCK_MONOTONIC, &ts ))
        clock_getres( CLOCK_REALTIME, &ts );

    prec = ts.tv_nsec / 1000;
}
#endif

/**
 * Return a value that is no bigger than the clock precision
 * (possibly zero).
 */
static inline unsigned mprec( void )
{
#if defined (HAVE_CLOCK_NANOSLEEP)
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once( &once, mprec_once );
    return prec;
#else
    return 0;
#endif
}

#ifdef USE_APPLE_MACH
static mach_timebase_info_data_t mtime_timebase_info;
static pthread_once_t mtime_timebase_info_once = PTHREAD_ONCE_INIT;
static void mtime_init_timebase(void)
{
    mach_timebase_info(&mtime_timebase_info);
}
#endif

/**
 * Return high precision date
 *
 * Use a 1 MHz clock when possible, or 1 kHz
 *
 * Beware ! It doesn't reflect the actual date (since epoch), but can be the machine's uptime or anything (when monotonic clock is used)
 */
mtime_t mdate( void )
{
    mtime_t res;

#if defined (HAVE_CLOCK_NANOSLEEP)
    struct timespec ts;

    /* Try to use POSIX monotonic clock if available */
    if( clock_gettime( CLOCK_MONOTONIC, &ts ) == EINVAL )
        /* Run-time fallback to real-time clock (always available) */
        (void)clock_gettime( CLOCK_REALTIME, &ts );

    res = ((mtime_t)ts.tv_sec * (mtime_t)1000000)
           + (mtime_t)(ts.tv_nsec / 1000);

#elif defined( HAVE_KERNEL_OS_H )
    res = real_time_clock_usecs();

#elif defined( USE_APPLE_MACH )
    pthread_once(&mtime_timebase_info_once, mtime_init_timebase);
    uint64_t date = mach_absolute_time();

    /* Convert to nanoseconds */
    date *= mtime_timebase_info.numer;
    date /= mtime_timebase_info.denom;

    /* Convert to microseconds */
    res = date / 1000;
#elif defined( WIN32 ) || defined( UNDER_CE )
    /* We don't need the real date, just the value of a high precision timer */
    static mtime_t freq = INT64_C(-1);

    if( freq == INT64_C(-1) )
    {
        /* Extract from the Tcl source code:
         * (http://www.cs.man.ac.uk/fellowsd-bin/TIP/7.html)
         *
         * Some hardware abstraction layers use the CPU clock
         * in place of the real-time clock as a performance counter
         * reference.  This results in:
         *    - inconsistent results among the processors on
         *      multi-processor systems.
         *    - unpredictable changes in performance counter frequency
         *      on "gearshift" processors such as Transmeta and
         *      SpeedStep.
         * There seems to be no way to test whether the performance
         * counter is reliable, but a useful heuristic is that
         * if its frequency is 1.193182 MHz or 3.579545 MHz, it's
         * derived from a colorburst crystal and is therefore
         * the RTC rather than the TSC.  If it's anything else, we
         * presume that the performance counter is unreliable.
         */
        LARGE_INTEGER buf;

        freq = ( QueryPerformanceFrequency( &buf ) &&
                 (buf.QuadPart == INT64_C(1193182) || buf.QuadPart == INT64_C(3579545) ) )
               ? buf.QuadPart : 0;

#if defined( WIN32 )
        /* on windows 2000, XP and Vista detect if there are two
           cores there - that makes QueryPerformanceFrequency in
           any case not trustable?
           (may also be true, for single cores with adaptive
            CPU frequency and active power management?)
        */
        HINSTANCE h_Kernel32 = LoadLibrary(_T("kernel32.dll"));
        if(h_Kernel32)
        {
            void WINAPI (*pf_GetSystemInfo)(LPSYSTEM_INFO);
            pf_GetSystemInfo = (void WINAPI (*)(LPSYSTEM_INFO))
                                GetProcAddress(h_Kernel32, _T("GetSystemInfo"));
            if(pf_GetSystemInfo)
            {
               SYSTEM_INFO system_info;
               pf_GetSystemInfo(&system_info);
               if(system_info.dwNumberOfProcessors > 1)
                  freq = 0;
            }
            FreeLibrary(h_Kernel32);
        }
#endif
    }

    if( freq != 0 )
    {
        LARGE_INTEGER counter;
        QueryPerformanceCounter (&counter);

        /* Convert to from (1/freq) to microsecond resolution */
        /* We need to split the division to avoid 63-bits overflow */
        lldiv_t d = lldiv (counter.QuadPart, freq);

        res = (d.quot * 1000000) + ((d.rem * 1000000) / freq);
    }
    else
    {
        /* Fallback on timeGetTime() which has a millisecond resolution
         * (actually, best case is about 5 ms resolution)
         * timeGetTime() only returns a DWORD thus will wrap after
         * about 49.7 days so we try to detect the wrapping. */

        static CRITICAL_SECTION date_lock;
        static mtime_t i_previous_time = INT64_C(-1);
        static int i_wrap_counts = -1;

        if( i_wrap_counts == -1 )
        {
            /* Initialization */
#if defined( WIN32 )
            i_previous_time = INT64_C(1000) * timeGetTime();
#else
            i_previous_time = INT64_C(1000) * GetTickCount();
#endif
            InitializeCriticalSection( &date_lock );
            i_wrap_counts = 0;
        }

        EnterCriticalSection( &date_lock );
#if defined( WIN32 )
        res = INT64_C(1000) *
            (i_wrap_counts * INT64_C(0x100000000) + timeGetTime());
#else
        res = INT64_C(1000) *
            (i_wrap_counts * INT64_C(0x100000000) + GetTickCount());
#endif
        if( i_previous_time > res )
        {
            /* Counter wrapped */
            i_wrap_counts++;
            res += INT64_C(0x100000000) * 1000;
        }
        i_previous_time = res;
        LeaveCriticalSection( &date_lock );
    }
#elif defined(USE_APPLE_MACH)
    /* The version that should be used, if it was cancelable */
    pthread_once(&mtime_timebase_info_once, mtime_init_timebase);
    uint64_t mach_time = date * 1000 * mtime_timebase_info.denom / mtime_timebase_info.numer;
    mach_wait_until(mach_time);

#else
    struct timeval tv_date;

    /* gettimeofday() cannot fail given &tv_date is a valid address */
    (void)gettimeofday( &tv_date, NULL );
    res = (mtime_t) tv_date.tv_sec * 1000000 + (mtime_t) tv_date.tv_usec;
#endif

    return res;
}

#undef mwait
/**
 * Wait for a date
 *
 * This function uses select() and an system date function to wake up at a
 * precise date. It should be used for process synchronization. If current date
 * is posterior to wished date, the function returns immediately.
 * \param date The date to wake up at
 */
void mwait( mtime_t date )
{
    /* If the deadline is already elapsed, or within the clock precision,
     * do not even bother the system timer. */
    date -= mprec();

#if defined (HAVE_CLOCK_NANOSLEEP)
    lldiv_t d = lldiv( date, 1000000 );
    struct timespec ts = { d.quot, d.rem * 1000 };

    int val;
    while( ( val = clock_nanosleep( CLOCK_MONOTONIC, TIMER_ABSTIME, &ts,
                                    NULL ) ) == EINTR );
    if( val == EINVAL )
    {
        ts.tv_sec = d.quot; ts.tv_nsec = d.rem * 1000;
        while( clock_nanosleep( CLOCK_REALTIME, 0, &ts, NULL ) == EINTR );
    }

#elif defined (WIN32)
    mtime_t i_total;

    while( (i_total = (date - mdate())) > 0 )
    {
        const mtime_t i_sleep = i_total / 1000;
        DWORD i_delay = (i_sleep > 0x7fffffff) ? 0x7fffffff : i_sleep;
        vlc_testcancel();
        SleepEx( i_delay, TRUE );
    }
    vlc_testcancel();

#else
    mtime_t delay = date - mdate();
    if( delay > 0 )
        msleep( delay );

#endif
}


#include "libvlc.h" /* vlc_backtrace() */
#undef msleep

/**
 * Portable usleep(). Cancellation point.
 *
 * \param delay the amount of time to sleep
 */
void msleep( mtime_t delay )
{
#if defined( HAVE_CLOCK_NANOSLEEP )
    lldiv_t d = lldiv( delay, 1000000 );
    struct timespec ts = { d.quot, d.rem * 1000 };

    int val;
    while( ( val = clock_nanosleep( CLOCK_MONOTONIC, 0, &ts, &ts ) ) == EINTR );
    if( val == EINVAL )
    {
        ts.tv_sec = d.quot; ts.tv_nsec = d.rem * 1000;
        while( clock_nanosleep( CLOCK_REALTIME, 0, &ts, &ts ) == EINTR );
    }

#elif defined( HAVE_KERNEL_OS_H )
    snooze( delay );

#elif defined( WIN32 ) || defined( UNDER_CE )
    mwait (mdate () + delay);

#elif defined( HAVE_NANOSLEEP )
    struct timespec ts_delay;

    ts_delay.tv_sec = delay / 1000000;
    ts_delay.tv_nsec = (delay % 1000000) * 1000;

    while( nanosleep( &ts_delay, &ts_delay ) && ( errno == EINTR ) );

#elif defined (USE_APPLE_MACH)
    /* The version that should be used, if it was cancelable */
    pthread_once(&mtime_timebase_info_once, mtime_init_timebase);
    uint64_t mach_time = delay * 1000 * mtime_timebase_info.denom / mtime_timebase_info.numer;
    mach_wait_until(mach_time + mach_absolute_time());

#else
    struct timeval tv_delay;

    tv_delay.tv_sec = delay / 1000000;
    tv_delay.tv_usec = delay % 1000000;

    /* If a signal is caught, you are screwed. Update your OS to nanosleep()
     * or clock_nanosleep() if this is an issue. */
    select( 0, NULL, NULL, NULL, &tv_delay );
#endif
}

/*
 * Date management (internal and external)
 */

/**
 * Initialize a date_t.
 *
 * \param date to initialize
 * \param divider (sample rate) numerator
 * \param divider (sample rate) denominator
 */

void date_Init( date_t *p_date, uint32_t i_divider_n, uint32_t i_divider_d )
{
    p_date->date = 0;
    p_date->i_divider_num = i_divider_n;
    p_date->i_divider_den = i_divider_d;
    p_date->i_remainder = 0;
}

/**
 * Change a date_t.
 *
 * \param date to change
 * \param divider (sample rate) numerator
 * \param divider (sample rate) denominator
 */

void date_Change( date_t *p_date, uint32_t i_divider_n, uint32_t i_divider_d )
{
    /* change time scale of remainder */
    p_date->i_remainder = p_date->i_remainder * i_divider_n / p_date->i_divider_num;
    p_date->i_divider_num = i_divider_n;
    p_date->i_divider_den = i_divider_d;
}

/**
 * Set the date value of a date_t.
 *
 * \param date to set
 * \param date value
 */
void date_Set( date_t *p_date, mtime_t i_new_date )
{
    p_date->date = i_new_date;
    p_date->i_remainder = 0;
}

/**
 * Get the date of a date_t
 *
 * \param date to get
 * \return date value
 */
mtime_t date_Get( const date_t *p_date )
{
    return p_date->date;
}

/**
 * Move forwards or backwards the date of a date_t.
 *
 * \param date to move
 * \param difference value
 */
void date_Move( date_t *p_date, mtime_t i_difference )
{
    p_date->date += i_difference;
}

/**
 * Increment the date and return the result, taking into account
 * rounding errors.
 *
 * \param date to increment
 * \param incrementation in number of samples
 * \return date value
 */
mtime_t date_Increment( date_t *p_date, uint32_t i_nb_samples )
{
    mtime_t i_dividend = (mtime_t)i_nb_samples * 1000000 * p_date->i_divider_den;
    p_date->date += i_dividend / p_date->i_divider_num;
    p_date->i_remainder += (int)(i_dividend % p_date->i_divider_num);

    if( p_date->i_remainder >= p_date->i_divider_num )
    {
        /* This is Bresenham algorithm. */
        assert( p_date->i_remainder < 2*p_date->i_divider_num);
        p_date->date += 1;
        p_date->i_remainder -= p_date->i_divider_num;
    }

    return p_date->date;
}

/**
 * Decrement the date and return the result, taking into account
 * rounding errors.
 *
 * \param date to decrement
 * \param decrementation in number of samples
 * \return date value
 */
mtime_t date_Decrement( date_t *p_date, uint32_t i_nb_samples )
{
    mtime_t i_dividend = (mtime_t)i_nb_samples * 1000000 * p_date->i_divider_den;
    p_date->date -= i_dividend / p_date->i_divider_num;
    unsigned i_rem_adjust = i_dividend % p_date->i_divider_num;

    if( p_date->i_remainder < i_rem_adjust )
    {
        /* This is Bresenham algorithm. */
        assert( p_date->i_remainder > -p_date->i_divider_num);
        p_date->date -= 1;
        p_date->i_remainder += p_date->i_divider_num;
    }

    p_date->i_remainder -= i_rem_adjust;

    return p_date->date;
}

#ifndef HAVE_GETTIMEOFDAY

#ifdef WIN32

/*
 * Number of micro-seconds between the beginning of the Windows epoch
 * (Jan. 1, 1601) and the Unix epoch (Jan. 1, 1970).
 *
 * This assumes all Win32 compilers have 64-bit support.
 */
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS) || defined(__WATCOMC__)
#   define DELTA_EPOCH_IN_USEC  11644473600000000Ui64
#else
#   define DELTA_EPOCH_IN_USEC  11644473600000000ULL
#endif

static uint64_t filetime_to_unix_epoch (const FILETIME *ft)
{
    uint64_t res = (uint64_t) ft->dwHighDateTime << 32;

    res |= ft->dwLowDateTime;
    res /= 10;                   /* from 100 nano-sec periods to usec */
    res -= DELTA_EPOCH_IN_USEC;  /* from Win epoch to Unix epoch */
    return (res);
}

static int gettimeofday (struct timeval *tv, void *tz )
{
    FILETIME  ft;
    uint64_t tim;

    if (!tv) {
        return VLC_EGENERIC;
    }
    GetSystemTimeAsFileTime (&ft);
    tim = filetime_to_unix_epoch (&ft);
    tv->tv_sec  = (long) (tim / 1000000L);
    tv->tv_usec = (long) (tim % 1000000L);
    return (0);
}

#endif

#endif

/**
 * @return NTP 64-bits timestamp in host byte order.
 */
uint64_t NTPtime64 (void)
{
    struct timespec ts;
#if defined (CLOCK_REALTIME)
    clock_gettime (CLOCK_REALTIME, &ts);
#else
    {
        struct timeval tv;
        gettimeofday (&tv, NULL);
        ts.tv_sec = tv.tv_sec;
        ts.tv_nsec = tv.tv_usec * 1000;
    }
#endif

    /* Convert nanoseconds to 32-bits fraction (232 picosecond units) */
    uint64_t t = (uint64_t)(ts.tv_nsec) << 32;
    t /= 1000000000;


    /* There is 70 years (incl. 17 leap ones) offset to the Unix Epoch.
     * No leap seconds during that period since they were not invented yet.
     */
    assert (t < 0x100000000);
    t |= ((70LL * 365 + 17) * 24 * 60 * 60 + ts.tv_sec) << 32;
    return t;
}

