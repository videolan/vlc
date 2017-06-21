/*****************************************************************************
 * aligned_alloc.c: C11 aligned_alloc() replacement
 *****************************************************************************
 * Copyright © 2012, 2017 Rémi Denis-Courmont
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

#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#if !defined (HAVE_POSIX_MEMALIGN)
# include <malloc.h>
#endif

void *aligned_alloc(size_t align, size_t size)
{
    /* align must be a power of 2 */
    /* size must be a multiple of align */
    if ((align & (align - 1)) || (size & (align - 1)))
    {
        errno = EINVAL;
        return NULL;
    }

#ifdef HAVE_POSIX_MEMALIGN
    if (align < sizeof (void *)) /* POSIX does not allow small alignment */
        align = sizeof (void *);

    void *ptr;
    int err = posix_memalign(&ptr, align, size);
    if (err)
    {
        errno = err;
        ptr = NULL;
    }
    return ptr;

#elif defined(HAVE_MEMALIGN)
    return memalign(align, size);
#elif defined (_WIN32) && defined(__MINGW32__)
    return __mingw_aligned_malloc(size, align);
#elif defined (_WIN32) && defined(_MSC_VER)
    return _aligned_malloc(size, align);
#else
#warning unsupported aligned allocation!
    if (size > 0)
        errno = ENOMEM;
    return NULL;
#endif
}
