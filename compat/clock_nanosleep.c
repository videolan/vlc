/*****************************************************************************
 * clock_nanosleep.c: POSIX clock_nanosleep() replacement
 *****************************************************************************
 * Copyright © 2020 VLC authors and VideoLAN
 *
 * Author: Marvin Scholz <epirat07 at gmail dot com>
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

#ifdef __APPLE__

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <pthread.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/sysctl.h>
#include <mach/clock_types.h>

int clock_nanosleep(clockid_t clock_id, int flags,
        const struct timespec *rqtp, struct timespec *rmtp)
{
    // Validate timespec
    if (rqtp == NULL || rqtp->tv_sec < 0 ||
            rqtp->tv_nsec < 0 || (unsigned long)rqtp->tv_nsec >= NSEC_PER_SEC) {
        errno = EINVAL;
        return -1;
    }

    // Validate clock
    switch (clock_id) {
        case CLOCK_MONOTONIC:
        case CLOCK_REALTIME:
            break;
        default:
            errno = EINVAL;
            return -1;
    }

    if (flags == TIMER_ABSTIME) {
        struct timespec ts_rel;
        struct timespec ts_now;

        do {
            // Get current time with requested clock
            if (clock_gettime(clock_id, &ts_now) != 0)
                return -1;

            // Calculate relative timespec
            ts_rel.tv_sec  = rqtp->tv_sec  - ts_now.tv_sec;
            ts_rel.tv_nsec = rqtp->tv_nsec - ts_now.tv_nsec;
            if (ts_rel.tv_nsec < 0) {
                ts_rel.tv_sec  -= 1;
                ts_rel.tv_nsec += NSEC_PER_SEC;
            }

            // Check if time already elapsed
            if (ts_rel.tv_sec < 0 || (ts_rel.tv_sec == 0 && ts_rel.tv_nsec == 0)) {
                pthread_testcancel();
                return 0;
            }

            // "The absolute clock_nanosleep() function has no effect on the
            // structure referenced by rmtp", so do not pass rmtp here
        } while (nanosleep(&ts_rel, NULL) == 0);

        // If nanosleep failed or was interrupted by a signal,
        // return so the caller can handle it appropriately
        return -1;
    } else if (flags == 0) {
        return nanosleep(rqtp, rmtp);
    } else {
        // Invalid flags
        errno = EINVAL;
        return -1;
    }
}

#endif
