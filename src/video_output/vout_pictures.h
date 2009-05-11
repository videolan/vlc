/*****************************************************************************
 * vout_pictures.h : picture management definitions
 *****************************************************************************
 * Copyright (C) 2002-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Fourcc definitions that we can handle internally
 *****************************************************************************/

/* Alignment of critical dynamic data structure
 *
 * Not all platforms support memalign so we provide a vlc_memalign wrapper
 * void *vlc_memalign( size_t align, size_t size, void **pp_orig )
 * *pp_orig is the pointer that has to be freed afterwards.
 */
static inline
void *vlc_memalign (void **pp, size_t align, size_t size)
{
#if defined (HAVE_POSIX_MEMALIGN)
    return posix_memalign (pp, align, size) ? NULL : *pp;
#elif defined (HAVE_MEMALIGN)
    return *pp = memalign (align, size);
#else
    unsigned char *ptr;

    if (align < 1)
        return NULL;

    align--;
    ptr = malloc (size + align);
    if (ptr == NULL)
        return NULL;

    *pp = ptr;
    ptr += align;
    return (void *)(((uintptr_t)ptr) & ~align);
#endif
}

