/*****************************************************************************
 * clock_getres.c: POSIX clock_getres() replacement
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

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <mach/clock_types.h>

int clock_getres(clockid_t clock_id, struct timespec *tp)
{
    switch (clock_id) {
        case CLOCK_MONOTONIC:
        case CLOCK_REALTIME:
            // For realtime, it is using gettimeofday and for
            // the monotonic time it is relative to the system
            // boot time. Both of these use timeval, which has
            // at best microsecond precision.
            tp->tv_sec = 0;
            tp->tv_nsec = NSEC_PER_USEC;
            break;
        default:
            errno = EINVAL;
            return -1;
    }

    return 0;
}

#endif
