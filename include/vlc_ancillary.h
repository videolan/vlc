/*****************************************************************************
 * vlc_ancillary.h: ancillary management functions
 *****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

#ifndef VLC_ANCILLARY_H
#define VLC_ANCILLARY_H 1

#include <stdint.h>

/**
 * \defgroup ancillary Ancillary
 * \ingroup input
 *
 * Ancillary that can be attached to any vlc_frame_t or picture_t.
 *
 * Ancillaries can be created from:
 *  - packetized demuxer modules,
 *  - packetizer modules,
 *  - decoder modules.
 *
 *  @warning Ancillaries should not be attached from a non packetized demuxer
 *  module since the attachment to the vlc_frame will be lost by the packetizer
 *  module that will be automatically inserted.
 *
 * Ancillaries are automatically forwarded from a vlc_frame_t to an other
 * vlc_frame_t and from a picture_t to an other picture_t. This allow to keep
 * ancillaries untouched when audio filters or video filters are used (these
 * filters don't have to know about the ancillary).
 *
 * Ancillary readers can be either:
 *  - A decoder module,
 *  - An audio output,
 *  - A video output,
 *  - A video or audio filter.
 *
 * @{
 * \file
 * Ancillary definition and functions
 */

/**
 * Ancillary opaque struct, refcounted struct that hold user data with a free
 * callback.
 */
struct vlc_ancillary;

/**
 * ID of an ancillary. Each ancillary user can create its own unique ID via
 * VLC_ANCILLARY_ID.
 */
typedef uint32_t vlc_ancillary_id;
#define VLC_ANCILLARY_ID(a,b,c,d) VLC_FOURCC(a,b,c,d)

/**
 * Callback to free an ancillary data
 */
typedef void (*vlc_ancillary_free_cb)(void *data);

/**
 * Create an ancillary
 *
 * @param data an opaque ancillary, can't be NULL
 * @param id id of ancillary
 * @param free_cb callback to release the data, can be NULL
 * @return a valid vlc_ancillary pointer or NULL in case of allocation error
 */
VLC_API struct vlc_ancillary *
vlc_ancillary_CreateWithFreeCb(void *data, vlc_ancillary_id id,
                               vlc_ancillary_free_cb free_cb);

/**
 * Helper to create an ancillary holding an allocated data
 */
static inline struct vlc_ancillary *
vlc_ancillary_Create(void *data, vlc_ancillary_id id)
{
    return vlc_ancillary_CreateWithFreeCb(data, id, free);
}

/**
 * Release an ancillary
 *
 * If the refcount reachs 0, the free_cb provided by
 * vlc_ancillary_CreateWithFreeCb() is called.
 *
 * @param ancillary ancillary to release
 */
VLC_API void
vlc_ancillary_Release(struct vlc_ancillary *ancillary);

/**
 * Hold an ancillary
 *
 * @param ancillary ancillary to hold
 * @return the same ancillary
 */
VLC_API struct vlc_ancillary *
vlc_ancillary_Hold(struct vlc_ancillary *ancillary);

VLC_API void *
vlc_ancillary_GetData(const struct vlc_ancillary *ancillary);

/** @} */

/** @} */

#endif /* VLC_ANCILLARY_H */
