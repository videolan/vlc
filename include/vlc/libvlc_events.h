/*****************************************************************************
 * libvlc_events.h:  libvlc_events external API structure
 *****************************************************************************
 * Copyright (C) 1998-2010 VLC authors and VideoLAN
 *
 * Authors: Filippo Carone <littlejohn@videolan.org>
 *          Pierre d'Herbemont <pdherbemont@videolan.org>
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

#ifndef LIBVLC_EVENTS_H
#define LIBVLC_EVENTS_H 1

# include <vlc/libvlc.h>
# include <vlc/libvlc_picture.h>
# include <vlc/libvlc_media_track.h>
# include <vlc/libvlc_media.h>

/**
 * \file
 * This file defines libvlc_event external API
 */

# ifdef __cplusplus
extern "C" {
# else
#  include <stdbool.h>
# endif

typedef struct libvlc_renderer_item_t libvlc_renderer_item_t;
typedef struct libvlc_title_description_t libvlc_title_description_t;
typedef struct libvlc_picture_t libvlc_picture_t;
typedef struct libvlc_picture_list_t libvlc_picture_list_t;
typedef struct libvlc_media_t libvlc_media_t;
typedef struct libvlc_media_list_t libvlc_media_list_t;

/**
 * \ingroup libvlc_event
 * @{
 */

/**
 * Event types
 */
enum libvlc_event_e {
    /* Append new event types at the end of a category.
     * Do not remove, insert or re-order any entry.
     */

    /**
     * 1 or several Metadata of a \link #libvlc_media_t media item\endlink changed
     */
    libvlc_MediaMetaChanged=0,
    /**
     * Subitem was added to a \link #libvlc_media_t media item\endlink
     * \see libvlc_media_subitems()
     */
    libvlc_MediaSubItemAdded,
    /**
     * Deprecated, use libvlc_MediaParsedChanged or libvlc_MediaPlayerLengthChanged.
     */
    libvlc_MediaDurationChanged,
    /**
     * Parsing state of a \link #libvlc_media_t media item\endlink changed
     * \see libvlc_media_parse_request(),
     *      libvlc_media_get_parsed_status(),
     *      libvlc_media_parse_stop()
     */
    libvlc_MediaParsedChanged,

    /* Removed: libvlc_MediaFreed, */
    /* Removed: libvlc_MediaStateChanged */

    /**
     * Subitem tree was added to a \link #libvlc_media_t media item\endlink
     */
    libvlc_MediaSubItemTreeAdded = libvlc_MediaParsedChanged + 3,
    /**
     * A thumbnail generation for this \link #libvlc_media_t media \endlink completed.
     * \see libvlc_media_thumbnail_request_by_time()
     * \see libvlc_media_thumbnail_request_by_pos()
     */
    libvlc_MediaThumbnailGenerated,
    /**
     * One or more embedded thumbnails were found during the media preparsing
     * The user can hold these picture(s) using libvlc_picture_retain if they
     * wish to use them
     */
    libvlc_MediaAttachedThumbnailsFound,
};

/**
 * A LibVLC event
 */
typedef struct libvlc_event_t
{
    int   type; /**< Event type (see @ref libvlc_event_e) */
    void *p_obj; /**< Object emitting the event */
    union
    {
        /* media descriptor */
        struct
        {
            libvlc_meta_t meta_type; /**< Deprecated, any meta_type can change */
        } media_meta_changed;
        struct
        {
            libvlc_media_t * new_child;
        } media_subitem_added;
        struct
        {
            int64_t new_duration;
        } media_duration_changed;
        struct
        {
            int new_status; /**< see @ref libvlc_media_parsed_status_t */
        } media_parsed_changed;
        struct
        {
            int new_state; /**< see @ref libvlc_state_t */
        } media_state_changed;
        struct
        {
            libvlc_picture_t* p_thumbnail;
        } media_thumbnail_generated;
        struct
        {
            libvlc_media_t * item;
        } media_subitemtree_added;
        struct
        {
            libvlc_picture_list_t* thumbnails;
        } media_attached_thumbnails_found;

        /* media list */
        struct
        {
            libvlc_media_t * item;
            int index;
        } media_list_item_added;
        struct
        {
            libvlc_media_t * item;
            int index;
        } media_list_item_deleted;
    } u; /**< Type-dependent event description */
} libvlc_event_t;


/**@} */

# ifdef __cplusplus
}
# endif

#endif /* _LIBVLC_EVENTS_H */
