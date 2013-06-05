/*****************************************************************************
 * posix_memalign.c: POSIX posix_memalign() replacement
 *****************************************************************************
 * Copyright © 2012 Rémi Denis-Courmont
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

#include <stdlib.h>
#include <errno.h>

static int check_align (size_t align)
{
    for (size_t i = sizeof (void *); i != 0; i *= 2)
        if (align == i)
            return 0;
    return EINVAL;
}

#if !defined (_WIN32) && !defined (__APPLE__)
#include <malloc.h>

int posix_memalign (void **ptr, size_t align, size_t size)
{
    if (check_align (align))
        return EINVAL;

    int saved_errno = errno;
    void *p = memalign (align, size);
    if (p == NULL)
    {
        errno = saved_errno;
        return ENOMEM;
    }

    *ptr = p;
    return 0;
}

#else

int posix_memalign (void **ptr, size_t align, size_t size)
{
    if (check_align (align))
        return EINVAL;

    *ptr = NULL;
    return size ? ENOMEM : 0;
}

#endif
