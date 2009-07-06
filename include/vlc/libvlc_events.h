/*****************************************************************************
 * libvlc_events.h:  libvlc_events external API structure
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 * $Id $
 *
 * Authors: Filippo Carone <littlejohn@videolan.org>
 *          Pierre d'Herbemont <pdherbemont@videolan.org>
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

#ifndef LIBVLC_EVENTS_H
#define LIBVLC_EVENTS_H 1

/**
 * \file
 * This file defines libvlc_event external API
 */

# ifdef __cplusplus
extern "C" {
# endif

/*****************************************************************************
 * Events handling
 *****************************************************************************/

/** \defgroup libvlc_event libvlc_event
 * \ingroup libvlc_core
 * LibVLC Available Events
 * @{
 */

#ifdef __cplusplus
enum libvlc_event_type_e {
#else
enum libvlc_event_type_t {
#endif
    /* Append new event types at the end.
     * Do not remove, insert or re-order any entry. */
    libvlc_MediaMetaChanged,
    libvlc_MediaSubItemAdded,
    libvlc_MediaDurationChanged,
    libvlc_MediaPreparsedChanged,
    libvlc_MediaFreed,
    libvlc_MediaStateChanged,

    libvlc_MediaPlayerNothingSpecial,
    libvlc_MediaPlayerOpening,
    libvlc_MediaPlayerBuffering,
    libvlc_MediaPlayerPlaying,
    libvlc_MediaPlayerPaused,
    libvlc_MediaPlayerStopped,
    libvlc_MediaPlayerForward,
    libvlc_MediaPlayerBackward,
    libvlc_MediaPlayerEndReached,
    libvlc_MediaPlayerEncounteredError,
    libvlc_MediaPlayerTimeChanged,
    libvlc_MediaPlayerPositionChanged,
    libvlc_MediaPlayerSeekableChanged,
    libvlc_MediaPlayerPausableChanged,

    libvlc_MediaListItemAdded,
    libvlc_MediaListWillAddItem,
    libvlc_MediaListItemDeleted,
    libvlc_MediaListWillDeleteItem,

    libvlc_MediaListViewItemAdded,
    libvlc_MediaListViewWillAddItem,
    libvlc_MediaListViewItemDeleted,
    libvlc_MediaListViewWillDeleteItem,

    libvlc_MediaListPlayerPlayed,
    libvlc_MediaListPlayerNextItemSet,
    libvlc_MediaListPlayerStopped,

    libvlc_MediaDiscovererStarted,
    libvlc_MediaDiscovererEnded,

    libvlc_MediaPlayerTitleChanged,
    libvlc_MediaPlayerSnapshotTaken,
    libvlc_MediaPlayerLengthChanged,

    libvlc_VlmMediaAdded,
    libvlc_VlmMediaRemoved,
    libvlc_VlmMediaChanged,
    libvlc_VlmMediaInstanceStarted,
    libvlc_VlmMediaInstanceStopped,
    /* New event types HERE */
};

/**
 * An Event
 * \param type the even type
 * \param p_obj the sender object
 * \param u Event dependent content
 */

struct libvlc_event_t
{
    libvlc_event_type_t type;
    void * p_obj;
    union event_type_specific
    {
        /* media descriptor */
        struct
        {
            libvlc_meta_t meta_type;
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
            int new_status;
        } media_preparsed_changed;
        struct
        {
            libvlc_media_t * md;
        } media_freed;
        struct
        {
            libvlc_state_t new_state;
        } media_state_changed;

        /* media instance */
        struct
        {
            float new_position;
        } media_player_position_changed;
        struct
        {
            libvlc_time_t new_time;
        } media_player_time_changed;
        struct
        {
            int new_title;
        } media_player_title_changed;
        struct
        {
            uint64_t new_seekable; /* FIXME: that's a boolean! */
        } media_player_seekable_changed;
        struct
        {
            uint64_t new_pausable; /* FIXME: that's a BOOL!!! */
        } media_player_pausable_changed;

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
        } media_list_will_add_item;
        struct
        {
            libvlc_media_t * item;
            int index;
        } media_list_item_deleted;
        struct
        {
            libvlc_media_t * item;
            int index;
        } media_list_will_delete_item;

        /* media list view */
        struct
        {
            libvlc_media_t * item;
            int index;
        } media_list_view_item_added;
        struct
        {
            libvlc_media_t * item;
            int index;
        } media_list_view_will_add_item;
        struct
        {
            libvlc_media_t * item;
            int index;
        } media_list_view_item_deleted;
        struct
        {
            libvlc_media_t * item;
            int index;
        } media_list_view_will_delete_item;

        /* snapshot taken */
        struct
        {
             char* psz_filename ;
        } media_player_snapshot_taken ;

        /* Length changed */
        struct
        {
            libvlc_time_t   new_length;
        } media_player_length_changed;

        /* VLM media */
        struct
        {
            const char * psz_media_name;
        } vlm_media_event;

    } u;
};


/**@} */

# ifdef __cplusplus
}
# endif

#endif /* _LIBVLC_EVENTS_H */
