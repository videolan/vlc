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
#include <vlc_tick.h>

typedef struct vlc_thumbnailer_t vlc_thumbnailer_t;
typedef size_t vlc_thumbnailer_req_id;

#define VLC_THUMBNAILER_REQ_ID_INVALID 0

/**
 * thumbnailer callbacks
 */
struct vlc_thumbnailer_cbs
{
    /**
     * Event received on thumbnailing completion or error
     *
     * This callback will always be called, provided vlc_thumbnailer_Request
     * returned a valid request, and provided the request is not cancelled
     * before its completion.
     *
     * @note This callback is mandatory.
     *
     * In case of failure, timeout or cancellation, p_thumbnail will be NULL.
     * The picture, if any, is owned by the thumbnailer, and must be acquired
     * by using \link picture_Hold \endlink to use it pass the callback's
     * scope.
     *
     * \param item item used for the thumbnailer
     * \param status VLC_SUCCESS in case of success, VLC_ETIMEOUT in case of
     * timeout, -EINTR if cancelled, an error otherwise
     * \param thumbnail The generated thumbnail, or NULL in case of failure or
     * timeout
     * \param data Is the opaque pointer passed as vlc_thumbnailer_Request last
     * parameter
     */
    void (*on_ended)(input_item_t *item, int status, picture_t* thumbnail, void *data);
};

/**
 * \brief vlc_thumbnailer_Create Creates a thumbnailer object
 * \param parent A VLC object
 * @param timeout timeout of the thumbnailer, 0 for no limits.
 * \return A thumbnailer object, or NULL in case of failure
 */
VLC_API vlc_thumbnailer_t*
vlc_thumbnailer_Create(vlc_object_t* parent, vlc_tick_t timeout) VLC_USED;

/**
 * Thumbnailer seek argument
 */
struct vlc_thumbnailer_seek_arg
{
    enum
    {
        /** Don't seek */
        VLC_THUMBNAILER_SEEK_NONE,
        /** Seek by time */
        VLC_THUMBNAILER_SEEK_TIME,
        /** Seek by position */
        VLC_THUMBNAILER_SEEK_POS,
    } type;
    union
    {
        /** Seek time if type == VLC_THUMBNAILER_SEEK_TIME */
        vlc_tick_t time;
        /** Seek position if type == VLC_THUMBNAILER_SEEK_POS */
        double pos;
    };
    enum
    {
        /** Precise, but potentially slow */
        VLC_THUMBNAILER_SEEK_PRECISE,
        /** Fast, but potentially imprecise */
        VLC_THUMBNAILER_SEEK_FAST,
    } speed;
};

/**
 * \brief vlc_thumbnailer_Request Requests a thumbnailer
 * \param thumbnailer A thumbnailer object
 * \param input_item The input item to generate the thumbnail for
 * \param seek_arg pointer to a seek struct, that tell at which time the
 * thumbnail should be taken, NULL to disable seek
 * \param timeout A timeout value, or VLC_TICK_INVALID to disable timeout
 * \param cbs callback to listen to events (can't be NULL)
 * \param cbs_userdata opaque pointer used by the callbacks
 * \return VLC_THUMBNAILER_REQ_ID_INVALID in case of error, or a valid id if the
 * item was scheduled for thumbnailing. If this returns an
 * error, the on_ended callback will *not* be invoked
 *
 * The provided input_item will be held by the thumbnailer and can safely be
 * released safely after calling this function.
 */
VLC_API vlc_thumbnailer_req_id
vlc_thumbnailer_Request( vlc_thumbnailer_t *thumbnailer,
                         input_item_t *input_item,
                         const struct vlc_thumbnailer_seek_arg *seek_arg,
                         const struct vlc_thumbnailer_cbs *cbs,
                         void *cbs_userdata );

/**
 * \brief vlc_thumbnailer_Camcel Cancel a thumbnail request
 * \param thumbnailer A thumbnailer object
 * \param id unique id returned by vlc_thumbnailer_Request*(),
 * VLC_THUMBNAILER_REQ_ID_INVALID to cancels all tasks
 * \return number of tasks cancelled
 */
VLC_API size_t
vlc_thumbnailer_Cancel( vlc_thumbnailer_t* thumbnailer, vlc_thumbnailer_req_id id );

/**
 * \brief vlc_thumbnailer_Delete Deletes a thumbnailer and cancel all pending requests
 * \param thumbnailer A thumbnailer object
 */
VLC_API void vlc_thumbnailer_Delete( vlc_thumbnailer_t* thumbnailer );

/**
 * Do not use, libVLC only fonction, will be removed soon
 */
VLC_API void vlc_thumbnailer_SetTimeout( vlc_thumbnailer_t *thumbnailer,
                                         vlc_tick_t timeout ) VLC_DEPRECATED;

#endif // VLC_THUMBNAILER_H
