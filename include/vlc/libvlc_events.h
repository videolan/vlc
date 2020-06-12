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

/**
 * \file
 * This file defines libvlc_event external API
 */

# ifdef __cplusplus
extern "C" {
# endif

typedef struct libvlc_renderer_item_t libvlc_renderer_item_t;
typedef struct libvlc_title_description_t libvlc_title_description_t;

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
     * Metadata of a \link #libvlc_media_t media item\endlink changed
     */
    libvlc_MediaMetaChanged=0,
    /**
     * Subitem was added to a \link #libvlc_media_t media item\endlink
     * \see libvlc_media_subitems()
     */
    libvlc_MediaSubItemAdded,
    /**
     * Duration of a \link #libvlc_media_t media item\endlink changed
     * \see libvlc_media_get_duration()
     */
    libvlc_MediaDurationChanged,
    /**
     * Parsing state of a \link #libvlc_media_t media item\endlink changed
     * \see libvlc_media_parse_with_options(),
     *      libvlc_media_get_parsed_status(),
     *      libvlc_media_parse_stop()
     */
    libvlc_MediaParsedChanged,
    /**
     * A \link #libvlc_media_t media item\endlink was freed
     */
    libvlc_MediaFreed,
    /**
     * \link #libvlc_state_t State\endlink of the \link
     * #libvlc_media_t media item\endlink changed
     * \see libvlc_media_get_state()
     */
    libvlc_MediaStateChanged,
    /**
     * Subitem tree was added to a \link #libvlc_media_t media item\endlink
     */
    libvlc_MediaSubItemTreeAdded,
    /**
     * A thumbnail generation for this \link #libvlc_media_t media \endlink completed.
     * \see libvlc_media_get_thumbnail()
     */
    libvlc_MediaThumbnailGenerated,

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
    /* libvlc_MediaPlayerTitleChanged, */
    libvlc_MediaPlayerSnapshotTaken = libvlc_MediaPlayerPausableChanged + 2,
    libvlc_MediaPlayerLengthChanged,
    libvlc_MediaPlayerVout,
    libvlc_MediaPlayerScrambledChanged,
    /** A track was added, cf. media_player_es_changed in \ref libvlc_event_t.u
     * to get the id of the new track. */
    libvlc_MediaPlayerESAdded,
    /** A track was removed, cf. media_player_es_changed in \ref
     * libvlc_event_t.u to get the id of the removed track. */
    libvlc_MediaPlayerESDeleted,
    /** Tracks were selected or unselected, cf.
     * media_player_es_selection_changed in \ref libvlc_event_t.u to get the
     * unselected and/or the selected track ids. */
    libvlc_MediaPlayerESSelected,
    libvlc_MediaPlayerCorked,
    libvlc_MediaPlayerUncorked,
    libvlc_MediaPlayerMuted,
    libvlc_MediaPlayerUnmuted,
    libvlc_MediaPlayerAudioVolume,
    libvlc_MediaPlayerAudioDevice,
    /** A track was updated, cf. media_player_es_changed in \ref
     * libvlc_event_t.u to get the id of the updated track. */
    libvlc_MediaPlayerESUpdated,
    /**
     * The title list changed, call
     * libvlc_media_player_get_full_title_descriptions() to get the new list.
     */
    libvlc_MediaPlayerTitleListChanged,
    /**
     * The title selection changed, cf media_player_title_selection_changed in
     * \ref libvlc_event_t.u
     */
    libvlc_MediaPlayerTitleSelectionChanged,
    libvlc_MediaPlayerChapterChanged,

    /**
     * A \link #libvlc_media_t media item\endlink was added to a
     * \link #libvlc_media_list_t media list\endlink.
     */
    libvlc_MediaListItemAdded=0x200,
    /**
     * A \link #libvlc_media_t media item\endlink is about to get
     * added to a \link #libvlc_media_list_t media list\endlink.
     */
    libvlc_MediaListWillAddItem,
    /**
     * A \link #libvlc_media_t media item\endlink was deleted from
     * a \link #libvlc_media_list_t media list\endlink.
     */
    libvlc_MediaListItemDeleted,
    /**
     * A \link #libvlc_media_t media item\endlink is about to get
     * deleted from a \link #libvlc_media_list_t media list\endlink.
     */
    libvlc_MediaListWillDeleteItem,
    /**
     * A \link #libvlc_media_list_t media list\endlink has reached the
     * end.
     * All \link #libvlc_media_t items\endlink were either added (in
     * case of a \ref libvlc_media_discoverer_t) or parsed (preparser).
     */
    libvlc_MediaListEndReached,

    /**
     * \deprecated No longer used.
     * This belonged to the removed libvlc_media_list_view_t
     */
    libvlc_MediaListViewItemAdded=0x300,
    /**
     * \deprecated No longer used.
     * This belonged to the removed libvlc_media_list_view_t
     */
    libvlc_MediaListViewWillAddItem,
    /**
     * \deprecated No longer used.
     * This belonged to the removed libvlc_media_list_view_t
     */
    libvlc_MediaListViewItemDeleted,
    /**
     * \deprecated No longer used.
     * This belonged to the removed libvlc_media_list_view_t
     */
    libvlc_MediaListViewWillDeleteItem,

    /**
     * Playback of a \link #libvlc_media_list_player_t media list
     * player\endlink has started.
     */
    libvlc_MediaListPlayerPlayed=0x400,

    /**
     * The current \link #libvlc_media_t item\endlink of a
     * \link #libvlc_media_list_player_t media list player\endlink
     * has changed to a different item.
     */
    libvlc_MediaListPlayerNextItemSet,

    /**
     * Playback of a \link #libvlc_media_list_player_t media list
     * player\endlink has stopped.
     */
    libvlc_MediaListPlayerStopped,

    /**
     * A new \link #libvlc_renderer_item_t renderer item\endlink was found by a
     * \link #libvlc_renderer_discoverer_t renderer discoverer\endlink.
     * The renderer item is valid until deleted.
     */
    libvlc_RendererDiscovererItemAdded=0x502,

    /**
     * A previously discovered \link #libvlc_renderer_item_t renderer item\endlink
     * was deleted by a \link #libvlc_renderer_discoverer_t renderer discoverer\endlink.
     * The renderer item is no longer valid.
     */
    libvlc_RendererDiscovererItemDeleted,
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
            libvlc_picture_t* p_thumbnail;
        } media_thumbnail_generated;
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
            const libvlc_title_description_t *title;
            int index;
        } media_player_title_selection_changed;
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

        /* Extra MediaPlayer */
        struct
        {
            libvlc_media_t * new_media;
        } media_player_media_changed;

        /* ESAdded, ESDeleted, ESUpdated */
        struct
        {
            libvlc_track_type_t i_type;
            int i_id; /**< Deprecated, use psz_id */
            /** Call libvlc_media_player_get_track_from_id() to get the track
             * description. */
            const char *psz_id;
        } media_player_es_changed;

        /* ESSelected */
        struct
        {
            libvlc_track_type_t i_type;
            const char *psz_unselected_id;
            const char *psz_selected_id;
        } media_player_es_selection_changed;

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
