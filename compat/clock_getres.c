/*****************************************************************************
 * clock_getres.c: POSIX clock_getres() replacement for macOS
 *****************************************************************************
 * Copyright © 2020 VLC authors and VideoLAN
 *
 * Author: Marvin Scholz <epirat07 at gmail dot com>
 * Enhanced by: Yazan (improvements)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License.
 *****************************************************************************/

#ifdef __APPLE__

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <mach/mach_time.h>

// Define nanosecond per microsecond if not already defined
#ifndef NSEC_PER_USEC
#define NSEC_PER_USEC 1000L
#endif

int clock_getres(clockid_t clock_id, struct timespec *tp)
{
    if (!tp) {  // Safety: check if pointer is valid
        errno = EINVAL;
        return -1;
    }

    switch (clock_id) {
        case CLOCK_REALTIME: {
            // Use gettimeofday for better than microsecond precision
            struct timeval tv;
            if (gettimeofday(&tv, NULL) != 0) {
                return -1;
            }
            tp->tv_sec = 0;
            tp->tv_nsec = 1000; // microsecond precision = 1000 nanoseconds
            break;
        }
        case CLOCK_MONOTONIC: {
            // Use mach_absolute_time for high-resolution monotonic clock
            static mach_timebase_info_data_t timebase;
            if (timebase.denom == 0) {
                mach_timebase_info(&timebase);
            }
            // Minimum possible precision for the system
            tp->tv_sec = 0;
            tp->tv_nsec = (1 * timebase.numer) / timebase.denom;
            if (tp->tv_nsec == 0) tp->tv_nsec = 1; // never leave zero
            break;
        }
        default:
            errno = EINVAL;
            return -1;
    }

    return 0;
}

#endif
