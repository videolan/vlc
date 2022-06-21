/*****************************************************************************
 * vlc_picture_pool.h: picture pool definitions
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#ifndef VLC_PICTURE_POOL_H
#define VLC_PICTURE_POOL_H 1

/**
 * \file
 * This file defines picture pool structures and functions in vlc
 */

#include <vlc_picture.h>

/**
 * Picture pool handle
 */
typedef struct picture_pool_t picture_pool_t;

/**
 * Creates a pool of preallocated pictures. Free pictures can be allocated from
 * the pool, and are returned to the pool when they are no longer referenced.
 *
 * This avoids allocating and deallocationg pictures repeatedly, and ensures
 * that memory consumption remains within limits.
 *
 * To obtain a picture from the pool, use picture_pool_Get(). To increase and
 * decrease the reference count, use picture_Hold() and picture_Release()
 * respectively.
 *
 * @param count number of pictures in the array
 * @param tab array of pictures
 *
 * @return a pointer to the new pool on success, or NULL on error
 * (pictures are <b>not</b> released on error)
 */
VLC_API picture_pool_t * picture_pool_New(unsigned count,
                                          picture_t *const *tab) VLC_USED;

/**
 * Allocates pictures from the heap and creates a picture pool with them.
 * This is a convenience wrapper for picture_NewFromFormat() and
 * picture_pool_New().
 *
 * @param fmt video format of pictures to allocate from the heap
 * @param count number of pictures to allocate
 *
 * @return a pointer to the new pool on success, NULL on error
 */
VLC_API picture_pool_t * picture_pool_NewFromFormat(const video_format_t *fmt,
                                                    unsigned count) VLC_USED;

/**
 * Releases a pool created by picture_pool_New()
 * or picture_pool_NewFromFormat().
 *
 * @note If there are no pending references to the pooled pictures, and the
 * picture_resource_t.pf_destroy callback was not NULL, it will be invoked.
 * Otherwise the default callback will be used.
 *
 * @warning If there are pending references (a.k.a. late pictures), the
 * pictures will remain valid until the all pending references are dropped by
 * picture_Release().
 */
VLC_API void picture_pool_Release( picture_pool_t * );

/**
 * Obtains a picture from a pool if any is immediately available.
 *
 * The picture must be released with picture_Release().
 *
 * @return a picture, or NULL if all pictures in the pool are allocated
 *
 * @note This function is thread-safe.
 */
VLC_API picture_t * picture_pool_Get( picture_pool_t * ) VLC_USED;

/**
 * Obtains a picture from a pool.
 *
 * The picture must be released with picture_Release().
 *
 * @return a picture or NULL on memory error
 *
 * @note This function is thread-safe.
 */
VLC_API picture_t *picture_pool_Wait(picture_pool_t *) VLC_USED;

/**
 * Reserves pictures from a pool and creates a new pool with those.
 *
 * When the new pool is released, pictures are returned to the master pool.
 * If the master pool was already released, pictures will be destroyed.
 *
 * @param count number of picture to reserve
 *
 * @return the new pool, or NULL if there were not enough pictures available
 * or on error
 *
 * @note This function is thread-safe (but it might return NULL if other
 * threads have already allocated too many pictures).
 */
VLC_API picture_pool_t * picture_pool_Reserve(picture_pool_t *, unsigned count)
VLC_USED;

/**
 * @return the total number of pictures in the given pool
 * @note This function is thread-safe.
 */
unsigned picture_pool_GetSize(const picture_pool_t *);


#endif /* VLC_PICTURE_POOL_H */

