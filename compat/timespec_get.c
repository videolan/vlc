/*****************************************************************************
 * timespec_get.c: C11 timespec_get() replacement
 *****************************************************************************
 * Copyright © 2015 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef _WIN32
#include <windows.h>

int timespec_get(struct timespec *ts, int base)
{
    FILETIME ft;
    ULARGE_INTEGER s;
    ULONGLONG t;

    if (base != TIME_UTC)
        return 0;

    GetSystemTimeAsFileTime(&ft);
    s.LowPart = ft.dwLowDateTime;
    s.HighPart = ft.dwHighDateTime;
    t = s.QuadPart - 116444736000000000ULL;
    ts->tv_sec = t / 10000000;
    ts->tv_nsec = ((int) (t % 10000000)) * 100;
    return base;
}
#else /* !_WIN32 */

#include <time.h>
#include <unistd.h> /* _POSIX_TIMERS */
#ifndef _POSIX_TIMERS
#define _POSIX_TIMERS (-1)
#endif
#if (_POSIX_TIMERS <= 0)
# include <sys/time.h> /* gettimeofday() */
#endif

int timespec_get(struct timespec *ts, int base)
{
    switch (base)
    {
        case TIME_UTC:
#if (_POSIX_TIMERS >= 0)
            if (clock_gettime(CLOCK_REALTIME, ts) == 0)
                break;
#endif
#if (_POSIX_TIMERS <= 0)
        {
            struct timeval tv;

            if (gettimeofday(&tv, NULL) == 0)
            {
                ts->tv_sec = tv.tv_sec;
                ts->tv_nsec = tv.tv_usec * 1000;
                break;
            }
        }
#endif
            /* fall through */
        default:
            return 0;
    }
    return base;
}
#endif /* !_WIN32 */
