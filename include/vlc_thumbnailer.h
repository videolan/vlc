/*****************************************************************************
 * vlc_thumbnailer.h: Thumbnailing API
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
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

#ifndef VLC_THUMBNAILER_H
#define VLC_THUMBNAILER_H

#include <vlc_common.h>

typedef struct vlc_thumbnailer_t vlc_thumbnailer_t;
typedef struct vlc_thumbnailer_request_t vlc_thumbnailer_request_t;

/**
 * \brief vlc_thumbnailer_cb defines a callback invoked on thumbnailing completion or error
 *
 * This callback will always be called, provided vlc_thumbnailer_Request returned
 * a non NULL request, and provided the request is not cancelled before its
 * completion.
 * In case of failure, p_thumbnail will be NULL.
 * The picture, if any, is owned by the thumbnailer, and must be acquired by using
 * \link picture_Hold \endlink to use it pass the callback's scope.
 *
 * \param data Is the opaque pointer passed as vlc_thumbnailer_Request last parameter
 * \param thumbnail The generated thumbnail, or NULL in case of failure or timeout
 */
typedef void(*vlc_thumbnailer_cb)( void* data, picture_t* thumbnail );


/**
 * \brief vlc_thumbnailer_Create Creates a thumbnailer object
 * \param parent A VLC object
 * \return A thumbnailer object, or NULL in case of failure
 */
VLC_API vlc_thumbnailer_t*
vlc_thumbnailer_Create( vlc_object_t* p_parent )
VLC_USED;

enum vlc_thumbnailer_seek_speed
{
    /** Precise, but potentially slow */
    VLC_THUMBNAILER_SEEK_PRECISE,
    /** Fast, but potentially imprecise */
    VLC_THUMBNAILER_SEEK_FAST,
};

/**
 * \brief vlc_thumbnailer_RequestByTime Requests a thumbnailer at a given time
 * \param thumbnailer A thumbnailer object
 * \param time The time at which the thumbnail should be taken
 * \param speed The seeking speed \sa{enum vlc_thumbnailer_seek_speed}
 * \param input_item The input item to generate the thumbnail for
 * \param timeout A timeout value, or VLC_TICK_INVALID to disable timeout
 * \param cb A user callback to be called on completion (success & error)
 * \param user_data An opaque value, provided as pf_cb's first parameter
 * \return An opaque request object, or NULL in case of failure
 *
 * If this function returns a valid request object, the callback is guaranteed
 * to be called, even in case of later failure.
 * The returned request object must not be used after the callback has been
 * invoked. That request object is owned by the thumbnailer, and must not be
 * released.
 * The provided input_item will be held by the thumbnailer and can safely be
 * released safely after calling this function.
 */
VLC_API vlc_thumbnailer_request_t*
vlc_thumbnailer_RequestByTime( vlc_thumbnailer_t *thumbnailer,
                               vlc_tick_t time,
                               enum vlc_thumbnailer_seek_speed speed,
                               input_item_t *input_item, vlc_tick_t timeout,
                               vlc_thumbnailer_cb cb, void* user_data );
/**
 * \brief vlc_thumbnailer_RequestByTime Requests a thumbnailer at a given time
 * \param thumbnailer A thumbnailer object
 * \param pos The position at which the thumbnail should be taken
 * \param speed The seeking speed \sa{enum vlc_thumbnailer_seek_speed}
 * \param input_item The input item to generate the thumbnail for
 * \param timeout A timeout value, or VLC_TICK_INVALID to disable timeout
 * \param cb A user callback to be called on completion (success & error)
 * \param user_data An opaque value, provided as pf_cb's first parameter
 * \return An opaque request object, or NULL in case of failure
 *
 * If this function returns a valid request object, the callback is guaranteed
 * to be called, even in case of later failure.
 * The returned request object must not be used after the callback has been
 * invoked. That request object is owned by the thumbnailer, and must not be
 * released.
 * The provided input_item will be held by the thumbnailer and can safely be
 * released after calling this function.
 */
VLC_API vlc_thumbnailer_request_t*
vlc_thumbnailer_RequestByPos( vlc_thumbnailer_t *thumbnailer,
                              float pos,
                              enum vlc_thumbnailer_seek_speed speed,
                              input_item_t *input_item, vlc_tick_t timeout,
                              vlc_thumbnailer_cb cb, void* user_data );

/**
 * \brief vlc_thumbnailer_Cancel Cancel a thumbnail request
 * \param thumbnailer A thumbnailer object
 * \param request An opaque thumbnail request object
 *
 * Cancelling a request will *not* invoke the completion callback.
 * The behavior is undefined if the request is cancelled after its completion.
 */
VLC_API void
vlc_thumbnailer_Cancel( vlc_thumbnailer_t* thumbnailer,
                        vlc_thumbnailer_request_t* request );

/**
 * \brief vlc_thumbnailer_Release releases a thumbnailer and cancel all pending requests
 * \param thumbnailer A thumbnailer object
 */
VLC_API void vlc_thumbnailer_Release( vlc_thumbnailer_t* thumbnailer );

#endif // VLC_THUMBNAILER_H
