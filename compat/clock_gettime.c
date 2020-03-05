/*****************************************************************************
 * clock_gettime.c: POSIX clock_gettime() replacement
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
#include <sys/sysctl.h>

/**
 * Get the absolute time at which the system was booted
 * 
 * This time is changed whenever the clock is adjusted to
 * correctly reflect the boot time with the adjusted clock,
 * so just querying it once and reusing the value is not safe.
 *
 * \param[out]  tv   Timeval struct to write the boottime to
 * 
 * \note
 * The boot time only has microsecond precision
 *
 * \return 0 on success, else -1 and errno set
 */
static int vlc__get_system_boottime(struct timeval *tv)
{
    int ret = sysctl((int[]){ CTL_KERN, KERN_BOOTTIME }, 2,
                     tv, &(size_t){ sizeof(*tv) }, NULL, 0);

    if (ret != 0)
        return errno;

    return 0;
}

/**
 * Get the monotonic time (CLOCK_MONOTONIC)
 * 
 * Calculates a monotically incrasing time since system boot
 * that continues to increment when the system is asleep.
 * 
 * Warnings to everyone trying to simplify this:
 * - Using mach_continuous_time is not equivalent to this, see
 *   the darwin manpage about CLOCK_MONOTONIC_RAW for an explanation.
 * - Using mach_absolute_time is not equivalent to this, as it does
 *   not continue to increment when the system is asleep.
 * - The boot time is queried twice in a loop and only used if both
 *   match. This is done to detect if the boot time change since we
 *   obtained the current time of day, as the boot time can change
 *   when the system wallclock is adjusted, as that will adjust the
 *   boot time accordingly.
 *
 * \param[out]  tv   Timeval struct to write the monotonic time to
 *
 * \return 0 on success, else -1 and errno set
 */
static int vlc__get_monotonic(struct timeval *tv)
{
    int ret;
    struct timeval currenttime;
    struct timeval boottime_begin;
    struct timeval boottime_end;

    do {
        ret = vlc__get_system_boottime(&boottime_begin);
        if (ret != 0)
            return ret;

        ret = gettimeofday(&currenttime, NULL);
        if (ret != 0)
            return ret;

        ret = vlc__get_system_boottime(&boottime_end);
        if (ret != 0)
            return ret;
    } while (timercmp(&boottime_begin, &boottime_end, !=));

    timersub(&currenttime, &boottime_begin, tv);
    return 0;
}

int clock_gettime(clockid_t clock_id, struct timespec *tp)
{
    int ret = 0;
    struct timeval tv;

    switch (clock_id) {
        case CLOCK_MONOTONIC:
            ret = vlc__get_monotonic(&tv);
            break;
        case CLOCK_REALTIME:
            ret = gettimeofday(&tv, NULL);
            break;
        default:
            errno = EINVAL;
            return -1;
    }

    if (ret == 0)
        TIMEVAL_TO_TIMESPEC(&tv, tp);
    return ret;
}

#endif
