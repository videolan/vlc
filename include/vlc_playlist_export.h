/*****************************************************************************
 * vlc_playlist_export.h
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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

#ifndef VLC_PLAYLIST_EXPORT_H
#define VLC_PLAYLIST_EXPORT_H

#include <vlc_playlist.h>

/** API for playlist export modules */

/**
 * Opaque structure giving a read-only view of a playlist.
 *
 * The view is only valid until the playlist lock is released.
 */
struct vlc_playlist_view;

/**
 * Return the number of items in the view.
 *
 * The underlying playlist must be locked.
 *
 * \param view the playlist view
 */
VLC_API size_t
vlc_playlist_view_Count(struct vlc_playlist_view *view);

/**
 * Return the item at a given index.
 *
 * The index must be in range (less than vlc_playlist_view_Count()).
 *
 * The underlying playlist must be locked.
 *
 * \param view  the playlist view
 * \param index the index
 * \return the playlist item
 */
VLC_API vlc_playlist_item_t *
vlc_playlist_view_Get(struct vlc_playlist_view *view, size_t index);

/**
 * Structure received by playlist export module.
 */
struct vlc_playlist_export
{
    struct vlc_object_t obj;
    char *base_url;
    FILE *file;
    struct vlc_playlist_view *playlist_view;
};

#endif
