/*****************************************************************************
 * posix_memalign.c: POSIX posix_memalign() replacement
 *****************************************************************************
 * Copyright © 2012, 2019 Rémi Denis-Courmont
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

#include <errno.h>
#include <stdlib.h>
#include <limits.h>

#ifdef HAVE_MEMALIGN
# include <malloc.h>
#else

static void *memalign(size_t align, size_t size)
{
    void *p = malloc(size);

    if ((uintptr_t)p & (align - 1)) {
        free(p);
        p = NULL;
    }

    return p;
}

#endif

static int check_align(size_t align)
{
    if (align & (align - 1)) /* must be a power of two */
        return EINVAL;
    if (align < sizeof (void *)) /* must be a multiple of sizeof (void *) */
        return EINVAL;
    return 0;
}

int posix_memalign(void **ptr, size_t align, size_t size)
{
    int val = check_align(align);
    if (val)
        return val;

    /* Unlike posix_memalign(), legacy memalign() requires that size be a
     * multiple of align.
     */
    if (size > (SIZE_MAX / 2))
        return ENOMEM;

    size += (-size) & (align - 1);

    int saved_errno = errno;
    void *p = memalign(align, size);
    if (p == NULL) {
        val = errno;
        errno = saved_errno;
        return val;
    }

    *ptr = p;
    return 0;
}
