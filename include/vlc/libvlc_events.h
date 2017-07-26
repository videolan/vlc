/*****************************************************************************
 * libvlc_events.h:  libvlc_events external API structure
 *****************************************************************************
 * Copyright (C) 1998-2010 VLC authors and VideoLAN
 * $Id $
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

/**
 * \file
 * This file defines libvlc_event external API
 */

# ifdef __cplusplus
extern "C" {
# endif

typedef struct libvlc_renderer_item_t libvlc_renderer_item_t;

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
     * Keep this in sync with lib/event.c:libvlc_event_type_name(). */
    libvlc_MediaMetaChanged=0,
    libvlc_MediaSubItemAdded,
    libvlc_MediaDurationChanged,
    libvlc_MediaParsedChanged,
    libvlc_MediaFreed,
    libvlc_MediaStateChanged,
    libvlc_MediaSubItemTreeAdded,

    libvlc_MediaPlayerMediaChanged=0x100,
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
    libvlc_MediaPlayerTitleChanged,
    libvlc_MediaPlayerSnapshotTaken,
    libvlc_MediaPlayerLengthChanged,
    libvlc_MediaPlayerVout,
    libvlc_MediaPlayerScrambledChanged,
    libvlc_MediaPlayerESAdded,
    libvlc_MediaPlayerESDeleted,
    libvlc_MediaPlayerESSelected,
    libvlc_MediaPlayerCorked,
    libvlc_MediaPlayerUncorked,
    libvlc_MediaPlayerMuted,
    libvlc_MediaPlayerUnmuted,
    libvlc_MediaPlayerAudioVolume,
    libvlc_MediaPlayerAudioDevice,
    libvlc_MediaPlayerChapterChanged,

    libvlc_MediaListItemAdded=0x200,
    libvlc_MediaListWillAddItem,
    libvlc_MediaListItemDeleted,
    libvlc_MediaListWillDeleteItem,
    libvlc_MediaListEndReached,

    libvlc_MediaListViewItemAdded=0x300,
    libvlc_MediaListViewWillAddItem,
    libvlc_MediaListViewItemDeleted,
    libvlc_MediaListViewWillDeleteItem,

    libvlc_MediaListPlayerPlayed=0x400,
    libvlc_MediaListPlayerNextItemSet,
    libvlc_MediaListPlayerStopped,

    /**
     * \deprecated Useless event, it will be triggered only when calling
     * libvlc_media_discoverer_start()
     */
    libvlc_MediaDiscovererStarted=0x500,
    /**
     * \deprecated Useless event, it will be triggered only when calling
     * libvlc_media_discoverer_stop()
     */
    libvlc_MediaDiscovererEnded,

    libvlc_RendererDiscovererItemAdded,
    libvlc_RendererDiscovererItemDeleted,

    libvlc_VlmMediaAdded=0x600,
    libvlc_VlmMediaRemoved,
    libvlc_VlmMediaChanged,
    libvlc_VlmMediaInstanceStarted,
    libvlc_VlmMediaInstanceStopped,
    libvlc_VlmMediaInstanceStatusInit,
    libvlc_VlmMediaInstanceStatusOpening,
    libvlc_VlmMediaInstanceStatusPlaying,
    libvlc_VlmMediaInstanceStatusPause,
    libvlc_VlmMediaInstanceStatusEnd,
    libvlc_VlmMediaInstanceStatusError
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
            int new_status; /**< see @ref libvlc_media_parsed_status_t */
        } media_parsed_changed;
        struct
        {
            libvlc_media_t * md;
        } media_freed;
        struct
        {
            int new_state; /**< see @ref libvlc_state_t */
        } media_state_changed;
        struct
        {
            libvlc_media_t * item;
        } media_subitemtree_added;

        /* media instance */
        struct
        {
            float new_cache;
        } media_player_buffering;
        struct
        {
            int new_chapter;
        } media_player_chapter_changed;
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
        struct
        {
            int new_scrambled;
        } media_player_scrambled_changed;
        struct
        {
            int new_count;
        } media_player_vout;

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

        struct
        {
            libvlc_track_type_t i_type;
            int                 i_id;
        } media_player_es_changed;

        struct
        {
            float volume;
        } media_player_audio_volume;

        struct
        {
            const char *device;
        } media_player_audio_device;

        struct
        {
            libvlc_renderer_item_t *item;
        } renderer_discoverer_item_added;
        struct
        {
            libvlc_renderer_item_t *item;
        } renderer_discoverer_item_deleted;
    } u; /**< Type-dependent event description */
} libvlc_event_t;


/**@} */

# ifdef __cplusplus
}
# endif

#endif /* _LIBVLC_EVENTS_H */
