/*****************************************************************************
 * vlc_memory.h: Memory functions
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 *
 * Authors: JP Dinger <jpd at videolan dot org>
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

#ifndef VLC_MEMORY_H
#define VLC_MEMORY_H 1

#include <stdlib.h>

/**
 * \defgroup memory Memory
 * @{
 * \file
 * Memory fixups
 */

/**
 * This wrapper around realloc() will free the input pointer when
 * realloc() returns NULL. The use case ptr = realloc(ptr, newsize) will
 * cause a memory leak when ptr pointed to a heap allocation before,
 * leaving the buffer allocated but unreferenced. vlc_realloc() is a
 * drop-in replacement for that use case (and only that use case).
 */
static inline void *realloc_or_free( void *p, size_t sz )
{
    void *n = realloc(p,sz);
    if( !n )
        free(p);
    return n;
}

/**
 * @}
 */

#endif
