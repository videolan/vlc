/*****************************************************************************
 * vlc_picture_pool.h: picture pool definitions
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 * $Id$
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
 *
 * XXX it is not thread safe, all pool manipulations and picture_Release
 * must be properly locked if needed.
 */
typedef struct picture_pool_t picture_pool_t;

/**
 * Picture pool configuration
 */
typedef struct {
    int       picture_count;
    picture_t **picture;

    int       (*lock)(picture_t *);
    void      (*unlock)(picture_t *);
} picture_pool_configuration_t;

/**
 * It creates a picture_pool_t wrapping the given configuration.
 *
 * It avoids useless picture creations/destructions.
 * The given picture must not have a reference count greater than 1.
 * The pool takes ownership of the picture and MUST not be used directly.
 * When deleted, the pool will release the pictures using picture_Release.
 * If defined, picture_pool_configuration_t::lock will be called before
 * a picture is used, and picture_pool_configuration_t::unlock will be called
 * as soon as a picture is unused. They are allowed to modify picture_t::p and
 * access picture_t::p_sys.
 */
VLC_API picture_pool_t * picture_pool_NewExtended( const picture_pool_configuration_t * ) VLC_USED;

/**
 * It creates a picture_pool_t wrapping the given arrays of picture.
 *
 * It is provided as convenience.
 */
VLC_API picture_pool_t * picture_pool_New( int picture_count, picture_t *picture[] ) VLC_USED;

/**
 * It creates a picture_pool_t creating images using the given format.
 *
 * Provided for convenience.
 */
VLC_API picture_pool_t * picture_pool_NewFromFormat( const video_format_t *, int picture_count ) VLC_USED;

/**
 * It destroys a pool created by picture_pool_New.
 *
 * All pictures must already be released to the pool. The pool will then
 * released them.
 */
VLC_API void picture_pool_Delete( picture_pool_t * );

/**
 * It retreives a picture_t from a pool.
 *
 * The picture must be release by using picture_Release.
 */
VLC_API picture_t * picture_pool_Get( picture_pool_t * ) VLC_USED;

/**
 * It forces the next picture_pool_Get to return a picture even if no
 * pictures are free.
 *
 * If b_reset is true, all pictures will be marked as free.
 *
 * It does it by releasing itself the oldest used picture if none is
 * available.
 * XXX it should be used with great care, the only reason you may need
 * it is to workaround a bug.
 */
VLC_API void picture_pool_NonEmpty( picture_pool_t *, bool reset );

/**
 * It reserves picture_count pictures from the given pool and returns
 * a new pool with thoses pictures.
 *
 * The master pool must be full.
 * The returned pool must be deleted before the master pool.
 * When deleted, all pictures return to the master pool.
 */
VLC_API picture_pool_t * picture_pool_Reserve(picture_pool_t *, int picture_count) VLC_USED;

/**
 * It returns the size of the given pool.
 */
VLC_API int picture_pool_GetSize(picture_pool_t *);


#endif /* VLC_PICTURE_POOL_H */

