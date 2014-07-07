/*****************************************************************************
 * error.c: POSIX error messages formatting
 *****************************************************************************
 * Copyright © 2013 Rémi Denis-Courmont
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
# include "config.h"
#endif

#include <string.h>
#include <errno.h>
#include <locale.h>
#include <assert.h>

#include <vlc_common.h>

static const char *vlc_strerror_l(int errnum, const char *lname)
{
    int saved_errno = errno;
    locale_t loc = newlocale(LC_MESSAGES_MASK, lname, (locale_t)0);

    if (unlikely(loc == (locale_t)0))
    {
        if (errno == ENOENT) /* fallback to POSIX locale */
            loc = newlocale(LC_MESSAGES_MASK, "C", (locale_t)0);

        if (unlikely(loc == (locale_t)0))
        {
            assert(errno != EINVAL && errno != ENOENT);
            errno = saved_errno;
            return "Error message unavailable";
        }
        errno = saved_errno;
    }

    const char *buf = strerror_l(errnum, loc);

    freelocale(loc);
    return buf;
}

/**
 * Formats an error message in the current locale.
 * @param errnum error number (as in errno.h)
 * @return A string pointer, valid until the next call to a function of the
 * strerror() family in the same thread. This function cannot fail.
 */
const char *vlc_strerror(int errnum)
{
    /* We cannot simply use strerror() here, since it is not thread-safe. */
    return vlc_strerror_l(errnum, "");
}

/**
 * Formats an error message in the POSIX/C locale (i.e. American English).
 * @param errnum error number (as in errno.h)
 * @return A string pointer, valid until the next call to a function of the
 * strerror() family in the same thread. This function cannot fail.
 */
const char *vlc_strerror_c(int errnum)
{
    return vlc_strerror_l(errnum, "C");
}
