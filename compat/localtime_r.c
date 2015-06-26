/*****************************************************************************
 * localtime_r.c: POSIX localtime_r() replacement
 *****************************************************************************
 * Copyright © 1998-2015 VLC authors and VideoLAN
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

#if defined(__STDC_LIB_EXT1__) && (__STDC_LIB_EXT1__ >= 20112L)
# define __STDC_WANT_LIB_EXT1__ 1
#else
# define __STDC_WANT_LIB_EXT1__ 0
#endif
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <time.h>

/* If localtime_r() is not provided, we assume localtime() uses
 * thread-specific storage. */
struct tm *localtime_r (const time_t *timep, struct tm *result)
{
#if (__STDC_WANT_LIB_EXT1__)
    return localtime_s(timep, result);
#elif defined (_WIN32)
    return ((errno = localtime_s(result, timep)) == 0) ? result : NULL;
#else
# warning localtime_r() not implemented!
    return gmtime_r(timep, result);
#endif
}
