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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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

    /* Append new event types at the end. Do not remove, insert or
     * re-order any entry. The cpp will prepend libvlc_ to the symbols. */
#define DEFINE_LIBVLC_EVENT_TYPES \
    DEF( MediaMetaChanged ), \
    DEF( MediaSubItemAdded ), \
    DEF( MediaDurationChanged ), \
    DEF( MediaPreparsedChanged ), \
    DEF( MediaFreed ), \
    DEF( MediaStateChanged ), \
    \
    DEF( MediaPlayerNothingSpecial ), \
    DEF( MediaPlayerOpening ), \
    DEF( MediaPlayerBuffering ), \
    DEF( MediaPlayerPlaying ), \
    DEF( MediaPlayerPaused ), \
    DEF( MediaPlayerStopped ), \
    DEF( MediaPlayerForward ), \
    DEF( MediaPlayerBackward ), \
    DEF( MediaPlayerEndReached ), \
    DEF( MediaPlayerEncounteredError ), \
    DEF( MediaPlayerTimeChanged ), \
    DEF( MediaPlayerPositionChanged ), \
    DEF( MediaPlayerSeekableChanged ), \
    DEF( MediaPlayerPausableChanged ), \
    \
    DEF( MediaListItemAdded ), \
    DEF( MediaListWillAddItem ), \
    DEF( MediaListItemDeleted ), \
    DEF( MediaListWillDeleteItem ), \
    \
    DEF( MediaListViewItemAdded ), \
    DEF( MediaListViewWillAddItem ), \
    DEF( MediaListViewItemDeleted ), \
    DEF( MediaListViewWillDeleteItem ), \
    \
    DEF( MediaListPlayerPlayed ), \
    DEF( MediaListPlayerNextItemSet ), \
    DEF( MediaListPlayerStopped ), \
    \
    DEF( MediaDiscovererStarted ), \
    DEF( MediaDiscovererEnded ), \
    \
    DEF( MediaPlayerTitleChanged ), \
    DEF( MediaPlayerSnapshotTaken ), \
    DEF( MediaPlayerLengthChanged ), \
    \
    DEF( VlmMediaAdded ), \
    DEF( VlmMediaRemoved ), \
    DEF( VlmMediaChanged ), \
    DEF( VlmMediaInstanceStarted ), \
    DEF( VlmMediaInstanceStopped ), \
    DEF( VlmMediaInstanceStatusInit ), \
    DEF( VlmMediaInstanceStatusOpening ), \
    DEF( VlmMediaInstanceStatusPlaying ), \
    DEF( VlmMediaInstanceStatusPause ), \
    DEF( VlmMediaInstanceStatusEnd ), \
    DEF( VlmMediaInstanceStatusError ), \
    \
    DEF( MediaPlayerMediaChanged ), \
/* New event types HERE */

#ifdef __cplusplus
enum libvlc_event_type_e {
#else
enum libvlc_event_type_t {
#endif
#define DEF(a) libvlc_##a
    DEFINE_LIBVLC_EVENT_TYPES
    libvlc_num_event_types
#undef  DEF
};

/* Implementing libvlc_event_type_name() needs the definition too. */
#ifndef LIBVLC_EVENT_TYPES_KEEP_DEFINE
#undef  DEFINE_LIBVLC_EVENT_TYPES
#endif

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
            int new_seekable;
        } media_player_seekable_changed;
        struct
        {
            int new_pausable;
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

        /* media list player */
        struct
        {
            libvlc_media_t * item;
        } media_list_player_next_item_set;

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
            const char * psz_instance_name;
        } vlm_media_event;

        /* Extra MediaPlayer */
        struct
        {
            libvlc_media_t * new_media;
        } media_player_media_changed;
    } u;
};


/**@} */

# ifdef __cplusplus
}
# endif

#endif /* _LIBVLC_EVENTS_H */
