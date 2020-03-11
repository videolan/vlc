/*****************************************************************************
 * vlc_media_library.h: SQL-based media library
 *****************************************************************************
 * Copyright (C) 2008-2010 the VideoLAN Team and AUTHORS
 *
 * Authors: Antoine Lejeune <phytos@videolan.org>
 *          Jean-Philippe André <jpeg@videolan.org>
 *          Rémi Duraffort <ivoire@videolan.org>
 *          Adrien Maglo <magsoft@videolan.org>
 *          Srikanth Raju <srikiraju at gmail dot com>
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

#ifndef VLC_MEDIA_LIBRARY_H
# define VLC_MEDIA_LIBRARY_H

#include <assert.h>
#include <vlc_common.h>

# ifdef __cplusplus
extern "C" {
# endif

typedef enum vlc_ml_media_type_t
{
    VLC_ML_MEDIA_TYPE_UNKNOWN,
    VLC_ML_MEDIA_TYPE_VIDEO,
    VLC_ML_MEDIA_TYPE_AUDIO,
} vlc_ml_media_type_t;

typedef enum vlc_ml_media_subtype_t
{
    VLC_ML_MEDIA_SUBTYPE_UNKNOWN,
    VLC_ML_MEDIA_SUBTYPE_SHOW_EPISODE,
    VLC_ML_MEDIA_SUBTYPE_MOVIE,
    VLC_ML_MEDIA_SUBTYPE_ALBUMTRACK,
} vlc_ml_media_subtype_t;

typedef enum vlc_ml_file_type_t
{
    VLC_ML_FILE_TYPE_UNKNOWN,
    VLC_ML_FILE_TYPE_MAIN,
    VLC_ML_FILE_TYPE_PART,
    VLC_ML_FILE_TYPE_SOUNDTRACK,
    VLC_ML_FILE_TYPE_SUBTITLE,
    VLC_ML_FILE_TYPE_PLAYLIST,
} vlc_ml_file_type_t;

typedef enum vlc_ml_track_type_t
{
    VLC_ML_TRACK_TYPE_UNKNOWN,
    VLC_ML_TRACK_TYPE_VIDEO,
    VLC_ML_TRACK_TYPE_AUDIO,
} vlc_ml_track_type_t;

typedef enum vlc_ml_thumbnail_size_t
{
    VLC_ML_THUMBNAIL_SMALL,
    VLC_ML_THUMBNAIL_BANNER,

    VLC_ML_THUMBNAIL_SIZE_COUNT
} vlc_ml_thumbnail_size_t;

typedef enum vlc_ml_history_type_t
{
    VLC_ML_HISTORY_TYPE_MEDIA,
    VLC_ML_HISTORY_TYPE_NETWORK,
} vlc_ml_history_type_t;

typedef struct vlc_ml_thumbnail_t
{
    char* psz_mrl;
    /**
     * True if a thumbnail is available, or if thumbnail generation was
     * attempted but failed
     */
    bool b_generated;
} vlc_ml_thumbnail_t;

typedef struct vlc_ml_movie_t
{
    char* psz_summary;
    char* psz_imdb_id;
} vlc_ml_movie_t;

typedef struct vlc_ml_show_episode_t
{
    char* psz_summary;
    char* psz_tvdb_id;
    uint32_t i_episode_nb;
    uint32_t i_season_number;
} vlc_ml_show_episode_t;

typedef struct vlc_ml_show_t
{
    int64_t i_id;
    char* psz_name;
    char* psz_summary;
    char* psz_artwork_mrl;
    char* psz_tvdb_id;
    unsigned int i_release_year;
    uint32_t i_nb_episodes;
    uint32_t i_nb_seasons;
} vlc_ml_show_t;

typedef struct vlc_ml_album_track_t
{
    int64_t i_artist_id;
    int64_t i_album_id;
    int64_t i_genre_id;

    int i_track_nb;
    int i_disc_nb;
} vlc_ml_album_track_t;

typedef struct vlc_ml_label_t
{
    int64_t i_id;
    char* psz_name;
} vlc_ml_label_t;

typedef struct vlc_ml_label_list_t
{
    size_t i_nb_items;
    vlc_ml_label_t p_items[];
} vlc_ml_label_list_t;

typedef struct vlc_ml_file_t
{
    char* psz_mrl;
    vlc_ml_file_type_t i_type;
    bool b_external;
    bool b_removable;
    bool b_present;
} vlc_ml_file_t;

typedef struct vlc_ml_file_list_t
{
    size_t i_nb_items;
    vlc_ml_file_t p_items[];
} vlc_ml_file_list_t;

typedef struct vlc_ml_media_track_t
{
    char* psz_codec;
    char* psz_language;
    char* psz_description;
    vlc_ml_track_type_t i_type;
    uint32_t i_bitrate;
    union
    {
        struct
        {
            // Audio
            uint32_t i_nbChannels;
            uint32_t i_sampleRate;
        } a;
        struct
        {
            // Video
            uint32_t i_height;
            uint32_t i_width;
            uint32_t i_sarNum;
            uint32_t i_sarDen;
            uint32_t i_fpsNum;
            uint32_t i_fpsDen;
        } v;
    };
} vlc_ml_media_track_t;

typedef struct vlc_ml_media_track_list_t
{
    size_t i_nb_items;
    vlc_ml_media_track_t p_items[];
} vlc_ml_media_track_list_t;

typedef struct vlc_ml_media_t
{
    int64_t i_id;

    vlc_ml_media_type_t i_type;
    vlc_ml_media_subtype_t i_subtype;

    vlc_ml_file_list_t* p_files;
    vlc_ml_media_track_list_t* p_tracks;

    int32_t i_year;
    /* Duration in milliseconds */
    int64_t i_duration;
    uint32_t i_playcount;
    time_t i_last_played_date;
    char* psz_title;

    vlc_ml_thumbnail_t thumbnails[VLC_ML_THUMBNAIL_SIZE_COUNT];

    bool b_is_favorite;

    union
    {
        vlc_ml_show_episode_t show_episode;
        vlc_ml_movie_t movie;
        vlc_ml_album_track_t album_track;
    };
} vlc_ml_media_t;

typedef struct vlc_ml_playlist_t
{
    int64_t i_id;
    char* psz_name;
    uint32_t i_creation_date;
    char* psz_artwork_mrl;
} vlc_ml_playlist_t;

typedef struct vlc_ml_artist_t
{
    int64_t i_id;
    char* psz_name;
    char* psz_shortbio;
    vlc_ml_thumbnail_t thumbnails[VLC_ML_THUMBNAIL_SIZE_COUNT];
    char* psz_mb_id;

    unsigned int i_nb_album;
    unsigned int i_nb_tracks;
} vlc_ml_artist_t;

typedef struct vlc_ml_artist_list_t
{
    size_t i_nb_items;
    vlc_ml_artist_t p_items[];
} vlc_ml_artist_list_t;

typedef struct vlc_ml_album_t {
    int64_t i_id;
    char* psz_title;
    char* psz_summary;
    vlc_ml_thumbnail_t thumbnails[VLC_ML_THUMBNAIL_SIZE_COUNT];
    char* psz_artist;
    int64_t i_artist_id;

    size_t i_nb_tracks;
    unsigned int i_duration;
    unsigned int i_year;
} vlc_ml_album_t;

typedef struct vlc_ml_genre_t
{
    int64_t i_id;
    char* psz_name;
    size_t i_nb_tracks;
} vlc_ml_genre_t;

typedef struct vlc_ml_media_list_t
{
    size_t i_nb_items;
    vlc_ml_media_t p_items[];
} vlc_ml_media_list_t;

typedef struct vlc_ml_album_list_t
{
    size_t i_nb_items;
    vlc_ml_album_t p_items[];
} vlc_ml_album_list_t;

typedef struct vlc_ml_show_list_t
{
    size_t i_nb_items;
    vlc_ml_show_t p_items[];
} vlc_ml_show_list_t;

typedef struct vlc_ml_genre_list_t
{
    size_t i_nb_items;
    vlc_ml_genre_t p_items[];
} vlc_ml_genre_list_t;

typedef struct vlc_ml_playlist_list_t
{
    size_t i_nb_items;
    vlc_ml_playlist_t p_items[];
} vlc_ml_playlist_list_t;

typedef struct vlc_ml_entry_point_t
{
    char* psz_mrl; /**< This entrypoint's MRL. Will be NULL if b_present is false */
    bool b_present; /**< The presence state for this entrypoint. */
    bool b_banned; /**< Will be true if the user required this entrypoint to be excluded */
} vlc_ml_entry_point_t;

typedef struct vlc_ml_entry_point_list_t
{
    size_t i_nb_items;
    vlc_ml_entry_point_t p_items[];
} vlc_ml_entry_point_list_t;

typedef struct vlc_ml_bookmark_t
{
    int64_t i_media_id; /**< The associated media ID */
    int64_t i_time; /**< The bookmark time. The unit is arbitrary */
    char* psz_name; /**< The bookmark name */
    char* psz_description; /**< The bookmark description */
} vlc_ml_bookmark_t;

typedef struct vlc_ml_boomkmark_list_t
{
    size_t i_nb_items;
    vlc_ml_bookmark_t p_items[];
} vlc_ml_bookmark_list_t;

/* Opaque medialibrary pointer, to be used by any non-medialibrary module */
typedef struct vlc_medialibrary_t vlc_medialibrary_t;
/* "Private" medialibrary pointer, to be used by the core & medialibrary modules */
typedef struct vlc_medialibrary_module_t vlc_medialibrary_module_t;
/* Opaque event callback type */
typedef struct vlc_ml_event_callback_t vlc_ml_event_callback_t;

typedef enum vlc_ml_sorting_criteria_t
{
    /*
     * Default depends on the entity type:
     * - By track number (and disc number) for album tracks
     * - Alphabetical order for others
     */
    VLC_ML_SORTING_DEFAULT,
    VLC_ML_SORTING_ALPHA,
    VLC_ML_SORTING_DURATION,
    VLC_ML_SORTING_INSERTIONDATE,
    VLC_ML_SORTING_LASTMODIFICATIONDATE,
    VLC_ML_SORTING_RELEASEDATE,
    VLC_ML_SORTING_FILESIZE,
    VLC_ML_SORTING_ARTIST,
    VLC_ML_SORTING_PLAYCOUNT,
    VLC_ML_SORTING_ALBUM,
    VLC_ML_SORTING_FILENAME,
    VLC_ML_SORTING_TRACKNUMBER,
} vlc_ml_sorting_criteria_t;

typedef struct vlc_ml_query_params_t vlc_ml_query_params_t;
struct vlc_ml_query_params_t
{
    const char* psz_pattern;
    uint32_t i_nbResults;
    uint32_t i_offset;
    vlc_ml_sorting_criteria_t i_sort;
    bool b_desc;
};

enum vlc_ml_get_queries
{
    VLC_ML_GET_MEDIA,           /**< arg1: Media    ID; ret: vlc_ml_media_t*    */
    VLC_ML_GET_MEDIA_BY_MRL,    /**< arg1: Media   MRL; ret: vlc_ml_media_t*    */
    VLC_ML_GET_INPUT_ITEM,      /**< arg1: Media    ID; ret: input_item_t*      */
    VLC_ML_GET_INPUT_ITEM_BY_MRL,/**< arg1: Media  MRL; ret: input_item_t*      */
    VLC_ML_GET_ALBUM,           /**< arg1: Album    ID; ret: vlc_ml_album_t*    */
    VLC_ML_GET_ARTIST,          /**< arg1: Artist   ID; ret: vlc_ml_artist_t*   */
    VLC_ML_GET_GENRE,           /**< arg1: Genre    ID; ret: vlc_ml_genre_t*    */
    VLC_ML_GET_SHOW,            /**< arg1: Show     ID; ret: vlc_ml_show_t*     */
    VLC_ML_GET_PLAYLIST,        /**< arg1: Playlist ID; ret: vlc_ml_playlist_t* */
};

enum vlc_ml_list_queries
{
    /* General listing: */

    VLC_ML_LIST_VIDEOS,           /**< arg1 (out): vlc_ml_media_list_t**                            */
    VLC_ML_COUNT_VIDEOS,          /**< arg1 (out): size_t*                                          */
    VLC_ML_LIST_AUDIOS,           /**< arg1 (out): vlc_ml_media_list_t**                            */
    VLC_ML_COUNT_AUDIOS,          /**< arg1 (out): size_t*                                          */
    VLC_ML_LIST_ALBUMS,           /**< arg1 (out): vlc_ml_album_list_t**                            */
    VLC_ML_COUNT_ALBUMS,          /**< arg1 (out): size_t*                                          */
    VLC_ML_LIST_GENRES,           /**< arg1 (out): vlc_ml_genre_list_t**                            */
    VLC_ML_COUNT_GENRES,          /**< arg1 (out): size_t*                                          */
    VLC_ML_LIST_ARTISTS,          /**< arg1 bool: includeAll; arg2 (out): vlc_ml_genre_list_t**     */
    VLC_ML_COUNT_ARTISTS,         /**< arg1 bool: includeAll; arg2 (out): size_t*                   */
    VLC_ML_LIST_SHOWS,            /**< arg1 (out): vlc_ml_show_list_t**                             */
    VLC_ML_COUNT_SHOWS,           /**< arg1 (out): size_t*                                          */
    VLC_ML_LIST_PLAYLISTS,        /**< arg1 (out): vlc_ml_playlist_list_t**                         */
    VLC_ML_COUNT_PLAYLISTS,       /**< arg1 (out): size_t*                                          */
    VLC_ML_LIST_HISTORY,          /**< arg1 (out): vlc_ml_media_list_t**                            */
    VLC_ML_LIST_STREAM_HISTORY,   /**< arg1 (out): vlc_ml_media_list_t**                            */

    /* Album specific listings */
    VLC_ML_LIST_ALBUM_TRACKS,     /**< arg1: The album id. arg2 (out): vlc_ml_media_list_t**  */
    VLC_ML_COUNT_ALBUM_TRACKS,    /**< arg1: The album id. arg2 (out): size_t*  */
    VLC_ML_LIST_ALBUM_ARTISTS,    /**< arg1: The album id. arg2 (out): vlc_ml_album_list_t**  */
    VLC_ML_COUNT_ALBUM_ARTISTS,    /**< arg1: The album id. arg2 (out): size_t*  */

    /* Artist specific listings */
    VLC_ML_LIST_ARTIST_ALBUMS,  /**< arg1: The artist id. arg2(out): vlc_ml_album_list_t**    */
    VLC_ML_COUNT_ARTIST_ALBUMS, /**< arg1: The artist id. arg2(out): size_t*              */
    VLC_ML_LIST_ARTIST_TRACKS,  /**< arg1: The artist id. arg2(out): vlc_ml_media_list_t**    */
    VLC_ML_COUNT_ARTIST_TRACKS, /**< arg1: The artist id. arg2(out): size_t*              */

    /* Genre specific listings */
    VLC_ML_LIST_GENRE_ARTISTS,    /**< arg1: genre id;  arg2 (out): vlc_ml_artist_list_t**  */
    VLC_ML_COUNT_GENRE_ARTISTS,   /**< arg1: genre id;  arg2 (out): size_t*             */
    VLC_ML_LIST_GENRE_TRACKS,     /**< arg1: genre id;  arg2 (out): vlc_ml_media_list_t**   */
    VLC_ML_COUNT_GENRE_TRACKS,    /**< arg1: genre id;  arg2 (out): size_t*             */
    VLC_ML_LIST_GENRE_ALBUMS,     /**< arg1: genre id;  arg2 (out): vlc_ml_album_list_t**   */
    VLC_ML_COUNT_GENRE_ALBUMS,    /**< arg1: genre id;  arg2 (out): size_t*             */

    /* Show specific listings */
    VLC_ML_LIST_SHOW_EPISODES,    /**< arg1: show id; arg2(out): vlc_ml_media_list_t**  */
    VLC_ML_COUNT_SHOW_EPISODES,   /**< arg1: show id; arg2(out): size_t*                */

    /* Media specific listings */
    VLC_ML_LIST_MEDIA_LABELS,     /**< arg1: media id;  arg2 (out): vlc_ml_label_list_t**    */
    VLC_ML_COUNT_MEDIA_LABELS,    /**< arg1: media id;  arg2 (out): size_t*              */
    VLC_ML_LIST_MEDIA_BOOKMARKS,  /**< arg1: media id;  arg2 (out): vlc_ml_bookmark_list_t** */

    /* Playlist specific listings */
    VLC_ML_LIST_PLAYLIST_MEDIA,   /**< arg1: playlist id; arg2 (out): vlc_ml_media_list_t** */
    VLC_ML_COUNT_PLAYLIST_MEDIA,  /**< arg1: playlist id; arg2 (out): size_t* */

    /* Children entities listing */
    VLC_ML_LIST_MEDIA_OF,         /**< arg1: parent entity type; arg2: parent entity id; arg3(out): ml_media_list_t** */
    VLC_ML_COUNT_MEDIA_OF,        /**< arg1: parent entity type; arg2: parent entity id; arg3(out): size_t* */
    VLC_ML_LIST_ARTISTS_OF,       /**< arg1: parent entity type; arg2: parent entity id; arg3(out): ml_artist_list_t** */
    VLC_ML_COUNT_ARTISTS_OF,      /**< arg1: parent entity type; arg2: parent entity id; arg3(out): size_t* */
    VLC_ML_LIST_ALBUMS_OF,        /**< arg1: parent entity type; arg2: parent entity id; arg3(out): ml_album_list_t** */
    VLC_ML_COUNT_ALBUMS_OF,       /**< arg1: parent entity type; arg2: parent entity id; arg3(out): size_t* */
};

enum vlc_ml_parent_type
{
    VLC_ML_PARENT_UNKNOWN,
    VLC_ML_PARENT_ALBUM,
    VLC_ML_PARENT_ARTIST,
    VLC_ML_PARENT_SHOW,
    VLC_ML_PARENT_GENRE,
    VLC_ML_PARENT_PLAYLIST,
};

enum vlc_ml_control
{
    /* Adds a folder to discover through the medialibrary */
    VLC_ML_ADD_FOLDER,              /**< arg1: mrl (const char*)  res: can't fail */
    VLC_ML_REMOVE_FOLDER,           /**< arg1: mrl (const char*)  res: can't fail */
    VLC_ML_BAN_FOLDER,              /**< arg1: mrl (const char*)  res: can't fail */
    VLC_ML_UNBAN_FOLDER,            /**< arg1: mrl (const char*)  res: can't fail */
    VLC_ML_LIST_FOLDERS,            /**< arg1: entrypoints (vlc_ml_entry_point_list_t**); res: can fail */
    VLC_ML_IS_INDEXED,              /**< arg1: mrl (const char*) arg2 (out): bool*;       res: can fail */
    /**
     * Reload a specific folder, or all.
     * arg1: mrl (const char*), NULL to reload all folders
     * res: can't fail
     */
    VLC_ML_RELOAD_FOLDER,

    /* Pause/resume background operations, such as media discovery & media analysis */
    VLC_ML_PAUSE_BACKGROUND,        /**< no args; can't fail */
    VLC_ML_RESUME_BACKGROUND,       /**< no args; can't fail */

    /* Misc operations */
    VLC_ML_CLEAR_HISTORY,           /**< no args; can't fail */

    /* Create media */
    VLC_ML_NEW_EXTERNAL_MEDIA,      /**< arg1: const char*; arg2(out): vlc_ml_media_t** */
    VLC_ML_NEW_STREAM,              /**< arg1: const char*; arg2(out): vlc_ml_media_t** */

    /* Media management */
    VLC_ML_MEDIA_INCREASE_PLAY_COUNT,       /**< arg1: media id; can fail */
    VLC_ML_MEDIA_GET_MEDIA_PLAYBACK_STATE,  /**< arg1: media id; arg2: vlc_ml_playback_state; arg3: char**; */
    VLC_ML_MEDIA_SET_MEDIA_PLAYBACK_STATE,  /**< arg1: media id; arg2: vlc_ml_playback_state; arg3: const char*; */
    VLC_ML_MEDIA_GET_ALL_MEDIA_PLAYBACK_STATES, /**< arg1: media id; arg2(out): vlc_ml_playback_states_all* */
    VLC_ML_MEDIA_SET_ALL_MEDIA_PLAYBACK_STATES, /**< arg1: media id; arg2: const vlc_ml_playback_states_all* */
    VLC_ML_MEDIA_SET_THUMBNAIL,             /**< arg1: media id; arg2: const char*; arg3: vlc_ml_thumbnail_size_t */
    VLC_ML_MEDIA_GENERATE_THUMBNAIL,        /**< arg1: media id; arg2: vlc_ml_thumbnail_size_t; arg3: width; arg4: height; arg5: position */
    VLC_ML_MEDIA_ADD_EXTERNAL_MRL,          /**< arg1: media id; arg2: const char*; arg3: type(vlc_ml_file_type_t) */
    VLC_ML_MEDIA_SET_TYPE,                  /**< arg1: media id; arg2: vlc_ml_media_type_t */
    VLC_ML_MEDIA_ADD_BOOKMARK,              /**< arg1: media id; arg2: int64_t */
    VLC_ML_MEDIA_REMOVE_BOOKMARK,           /**< arg1: media id; arg2: int64_t */
    VLC_ML_MEDIA_REMOVE_ALL_BOOKMARKS,      /**< arg1: media id */
    VLC_ML_MEDIA_UPDATE_BOOKMARK,           /**< arg1: media id; arg2: int64_t; arg3: const char*; arg4: const char* */
};

/**
 * User playback settings.
 * All values/units are up to the caller and are not interpreted by the media
 * library.
 * All values are stored and returned as strings.
 * When calling vlc_medialibrary_t::pf_control with vlc_ml_MEDIA_GET_MEDIA_PLAYBACK_STATE,
 * the value will be returned stored in the provided char**. If the state was
 * not set yet, NULL will be returned.
 * When setting a state, NULL can be provided as a value to unset it.
 */
enum vlc_ml_playback_state
{
    VLC_ML_PLAYBACK_STATE_RATING,
    VLC_ML_PLAYBACK_STATE_PROGRESS,
    VLC_ML_PLAYBACK_STATE_SPEED,
    VLC_ML_PLAYBACK_STATE_TITLE,
    VLC_ML_PLAYBACK_STATE_CHAPTER,
    VLC_ML_PLAYBACK_STATE_PROGRAM,
    VLC_ML_PLAYBACK_STATE_SEEN,
    VLC_ML_PLAYBACK_STATE_VIDEO_TRACK,
    VLC_ML_PLAYBACK_STATE_ASPECT_RATIO,
    VLC_ML_PLAYBACK_STATE_ZOOM,
    VLC_ML_PLAYBACK_STATE_CROP,
    VLC_ML_PLAYBACK_STATE_DEINTERLACE,
    VLC_ML_PLAYBACK_STATE_VIDEO_FILTER,
    VLC_ML_PLAYBACK_STATE_AUDIO_TRACK,
    VLC_ML_PLAYBACK_STATE_GAIN,
    VLC_ML_PLAYBACK_STATE_AUDIO_DELAY,
    VLC_ML_PLAYBACK_STATE_SUBTITLE_TRACK,
    VLC_ML_PLAYBACK_STATE_SUBTITLE_DELAY,
    VLC_ML_PLAYBACK_STATE_APP_SPECIFIC,
};

typedef struct vlc_ml_playback_states_all
{
    float progress;
    float rate;
    float zoom;
    int current_title;
    char* current_video_track;
    char* current_audio_track;
    char *current_subtitle_track;
    char* aspect_ratio;
    char* crop;
    char* deinterlace;
    char* video_filter;
} vlc_ml_playback_states_all;

enum vlc_ml_event_type
{
    /**
     * Entity modification callbacks. The affected entity will be passed:
     * - As a vlc_ml_<type>_t, depending on the type of the modified/inserted
     * entity, in vlc_ml_event_t::modification::p_<type>
     * for ADDED and UPDATED variants.
     * - as an id, in vlc_ml_event_t::deletion::i_entity_id
     * When _DELETED callbacks get invoked, the entity will already have been
     * deleted from the database, and cannot be retrieved anymore
     */
    VLC_ML_EVENT_MEDIA_ADDED,
    VLC_ML_EVENT_MEDIA_UPDATED,
    VLC_ML_EVENT_MEDIA_DELETED,
    VLC_ML_EVENT_ARTIST_ADDED,
    VLC_ML_EVENT_ARTIST_UPDATED,
    VLC_ML_EVENT_ARTIST_DELETED,
    VLC_ML_EVENT_ALBUM_ADDED,
    VLC_ML_EVENT_ALBUM_UPDATED,
    VLC_ML_EVENT_ALBUM_DELETED,
    VLC_ML_EVENT_PLAYLIST_ADDED,
    VLC_ML_EVENT_PLAYLIST_UPDATED,
    VLC_ML_EVENT_PLAYLIST_DELETED,
    VLC_ML_EVENT_GENRE_ADDED,
    VLC_ML_EVENT_GENRE_UPDATED,
    VLC_ML_EVENT_GENRE_DELETED,
    VLC_ML_EVENT_BOOKMARKS_ADDED,
    VLC_ML_EVENT_BOOKMARKS_UPDATED,
    VLC_ML_EVENT_BOOKMARKS_DELETED,
    /**
     * A discovery started.
     * For each VLC_ML_EVENT_DISCOVERY_STARTED event, there will be
     * 1 VLC_ML_EVENT_DISCOVERY_COMPLETED event, and N
     * VLC_ML_EVENT_DISCOVERY_PROGRESS events.
     * The entry point being discovered is stored in
     * vlc_ml_event_t::discovery_started::psz_entry_point.
     */
    VLC_ML_EVENT_DISCOVERY_STARTED,
    /**
     * Sent when a discovery or reload operation starts analyzing a new folder.
     * The discovered entry point is stored in
     * vlc_ml_event_t::discovery_progress::psz_entry_point.
     */
    VLC_ML_EVENT_DISCOVERY_PROGRESS,
    /**
     * Sent when an entry point discovery is completed.
     * The entry point that was being discovered is stored in
     * vlc_ml_event_t::discovery_completed::psz_entry_point.
     * The success or failure state is stored in
     * vlc_ml_event_t::discovery_completed::b_success
     */
    VLC_ML_EVENT_DISCOVERY_COMPLETED,
    /**
     * An entry point reload operation started.
     * For all the entry points being reloaded, N VLC_EVENT_DISCOVERY_PROGRESS
     * and 1 VLC_EVENT_RELOAD_COMPLETED event will be sent.
     * The entry point being reloaded is stored in
     * vlc_ml_event_t::reload_started::psz_entry_point.
     */
    VLC_ML_EVENT_RELOAD_STARTED,
    /**
     * Sent when an entry point reload is completed.
     * The entry point that was being reloaded is stored in
     * vlc_ml_event_t::reload_completed::psz_entry_point.
     * The success or failure state is stored in
     * vlc_ml_event_t::reload_completed::b_success
     */
    VLC_ML_EVENT_RELOAD_COMPLETED,
    /**
     * Sent when a new entry point gets added to the database.
     * The entry point that was added is stored in
     * vlc::ml_event_t::entry_point_added::psz_entry_point, and the success or failure
     * state is stored in vlc_ml_event_t::entry_point_added::b_success
     * If successful, this event won't be emited again for this entry point.
     * In case of failure, this event will be fired again if the same entry point
     * is queued for discovery again.
     */
    VLC_ML_EVENT_ENTRY_POINT_ADDED,
    /**
     * Sent when an entry point removal request has been processed.
     * The removed entry point is stored in
     * vlc_ml_event_t::entry_point_removed::psz_entry_point and the success or failure
     * state is stored in vlc_ml_event_t::entry_point_removed::b_success
     */
    VLC_ML_EVENT_ENTRY_POINT_REMOVED,
    /**
     * Sent when an entry point ban request has been processed.
     * The banned entry point is stored in
     * vlc_ml_event_t::entry_point_banned::psz_entry_point and the operation success
     * state is stored in vlc_ml_event_t::entry_point_banned::b_success
     */
    VLC_ML_EVENT_ENTRY_POINT_BANNED,
    /**
     * Sent when an entry point unban request has been processed.
     * The unbanned entry point is stored in
     * vlc_ml_event_t::entry_point_unbanned::psz_entry_point and the operation success
     * state is stored in vlc_ml_event_t::entry_point_unbanned::b_success
     */
    VLC_ML_EVENT_ENTRY_POINT_UNBANNED,
    /**
     * Sent when a discoverer or parser threads changes its idle state.
     * The idle state is stored in vlc_ml_event_t::background_idle_changed.b_idle.
     * False means at least one background thread is in running, true means
     * both discoverer & parser threads are paused.
     */
    VLC_ML_EVENT_BACKGROUND_IDLE_CHANGED,
    /**
     * Sent when the parsing progress percentage gets updated.
     * The percentage is stored as a [0;100] integer, in
     * vlc_ml_event_t::parsing_progress::i_percent
     * This value might decrease as more media get discovered, but it will only
     * increase once all discovery operations are completed.
     */
    VLC_ML_EVENT_PARSING_PROGRESS_UPDATED,
    /**
     * Sent after a media thumbnail was generated, or if it's generation failed.
     * The media is stored in vlc_ml_event_t::media_thumbnail_generated::p_media
     * and the success state is stored in
     * vlc_ml_event_t::media_thumbnail_generated::b_success
     */
    VLC_ML_EVENT_MEDIA_THUMBNAIL_GENERATED,
    /**
     * Sent after the history gets changed. It can be either cleaned, or simply
     * modified because a media was recently played/removed from the history.
     * The history type (media/network) is stored in
     * vlc_ml_event_t::history_changed::history_type
     */
    VLC_ML_EVENT_HISTORY_CHANGED,
    /**
     * Sent when an application requested rescan starts being processed.
     */
    VLC_ML_EVENT_RESCAN_STARTED,
};

typedef struct vlc_ml_event_t
{
    int i_type;
    union
    {
        struct
        {
            const char* psz_entry_point;
        } discovery_started;
        struct
        {
            const char* psz_entry_point;
        } discovery_progress;
        struct
        {
            const char* psz_entry_point;
            bool b_success;
        } discovery_completed;
        struct
        {
            const char* psz_entry_point;
        } reload_started;
        struct
        {
            const char* psz_entry_point;
            bool b_success;
        } reload_completed;
        struct
        {
            const char* psz_entry_point;
            bool b_success;
        } entry_point_added;
        struct
        {
            const char* psz_entry_point;
            bool b_success;
        } entry_point_removed;
        struct
        {
            const char* psz_entry_point;
            bool b_success;
        } entry_point_banned;
        struct
        {
            const char* psz_entry_point;
            bool b_success;
        } entry_point_unbanned;
        struct
        {
            uint8_t i_percent;
        } parsing_progress;
        union
        {
            const vlc_ml_media_t* p_media;
            const vlc_ml_artist_t* p_artist;
            const vlc_ml_album_t* p_album;
            const vlc_ml_playlist_t* p_playlist;
            const vlc_ml_genre_t* p_genre;
            const vlc_ml_bookmark_t* p_bookmark;
        } creation;
        struct
        {
            int64_t i_entity_id;
        } modification;
        struct
        {
            int64_t i_entity_id;
        } deletion;
        struct
        {
            bool b_idle;
        } background_idle_changed;
        struct
        {
            const vlc_ml_media_t* p_media;
            vlc_ml_thumbnail_size_t i_size;
            bool b_success;
        } media_thumbnail_generated;
        struct
        {
            vlc_ml_history_type_t history_type;
        } history_changed;
    };
} vlc_ml_event_t;

typedef void (*vlc_ml_callback_t)( void* p_data, const vlc_ml_event_t* p_event );

typedef struct vlc_medialibrary_callbacks_t
{
    void (*pf_send_event)( vlc_medialibrary_module_t* p_ml, const vlc_ml_event_t* p_event );
} vlc_medialibrary_callbacks_t;

struct vlc_medialibrary_module_t
{
    struct vlc_object_t obj;

    module_t *p_module;

    void* p_sys;

    int (*pf_control)( struct vlc_medialibrary_module_t* p_ml, int i_query, va_list args );
    /**
     * List some entities from the medialibrary.
     *
     * \param p_ml The medialibrary module instance.
     * \param i_query The type search to be performed. \see vlc_ml_list enumeration
     * \param p_params A pointer to a vlc_ml_query_params_t structure, or NULL for
     * the default parameters (alphabetical ascending sort, no pagination)
     *
     * \return VLC_SUCCESS or an error code
     *
     * Refer to the individual list of vlc_ml_list requests for the additional
     * per-query input/ouput parameters values & types
     */
    int (*pf_list)( struct vlc_medialibrary_module_t* p_ml, int i_query,
                    const vlc_ml_query_params_t* p_params, va_list args );

    /**
     * Get a specific entity by its id or another unique value
     *
     * \return The required entity, or a NULL pointer if couldn't be found.
     *
     * Refer to the list of queries for the specific return type
     */
    void* (*pf_get)( struct vlc_medialibrary_module_t* p_ml, int i_query, va_list args );

    const vlc_medialibrary_callbacks_t* cbs;
};

vlc_medialibrary_t* libvlc_MlCreate( libvlc_int_t* p_libvlc );
void libvlc_MlRelease( vlc_medialibrary_t* p_ml );

VLC_API vlc_medialibrary_t* vlc_ml_instance_get( vlc_object_t* p_obj ) VLC_USED;
#define vlc_ml_instance_get(x) vlc_ml_instance_get( VLC_OBJECT(x) )

VLC_API void* vlc_ml_get( vlc_medialibrary_t* p_ml, int i_query, ... ) VLC_USED;
VLC_API int vlc_ml_control( vlc_medialibrary_t* p_ml, int i_query, ... ) VLC_USED;
VLC_API int vlc_ml_list( vlc_medialibrary_t* p_ml, int i_query,
                             const vlc_ml_query_params_t* p_params, ... );

/**
 * \brief Registers a medialibrary callback.
 * \returns A handle to the callback, to be passed to vlc_ml_event_unregister_callback
 */
VLC_API vlc_ml_event_callback_t*
vlc_ml_event_register_callback( vlc_medialibrary_t* p_ml, vlc_ml_callback_t cb, void* p_data );

/**
 * \brief Unregisters a medialibrary callback
 * \param p_handle The handled returned by vlc_ml_register_callback
 */
VLC_API void vlc_ml_event_unregister_callback( vlc_medialibrary_t* p_ml,
                                               vlc_ml_event_callback_t* p_callback );
/**
 * \brief Unregisters a medialibrary callback from the said callback.
 * \param p_callback The handle returned by vlc_ml_register_callback
 *
 * This must only be called synchronously from the callback function provided to
 * vlc_ml_event_register_callback
 * The p_callback handle must be considered invalid when this function returns
 */
VLC_API void vlc_ml_event_unregister_from_callback( vlc_medialibrary_t* p_ml,
                                                    vlc_ml_event_callback_t* p_callback );


VLC_API void vlc_ml_show_release( vlc_ml_show_t* p_show );
VLC_API void vlc_ml_artist_release( vlc_ml_artist_t* p_artist );
VLC_API void vlc_ml_genre_release( vlc_ml_genre_t* p_genre );
VLC_API void vlc_ml_media_release( vlc_ml_media_t* p_media );
VLC_API void vlc_ml_album_release( vlc_ml_album_t* p_album );
VLC_API void vlc_ml_playlist_release( vlc_ml_playlist_t* p_playlist );

VLC_API void vlc_ml_label_list_release( vlc_ml_label_list_t* p_list );
VLC_API void vlc_ml_file_list_release( vlc_ml_file_list_t* p_list );
VLC_API void vlc_ml_artist_list_release( vlc_ml_artist_list_t* p_list );
VLC_API void vlc_ml_media_list_release( vlc_ml_media_list_t* p_list );
VLC_API void vlc_ml_album_list_release( vlc_ml_album_list_t* p_list );
VLC_API void vlc_ml_show_list_release( vlc_ml_show_list_t* p_list );
VLC_API void vlc_ml_genre_list_release( vlc_ml_genre_list_t* p_list );
VLC_API void vlc_ml_playlist_list_release( vlc_ml_playlist_list_t* p_list );
VLC_API void vlc_ml_entry_point_list_release( vlc_ml_entry_point_list_t* p_list );
VLC_API void vlc_ml_playback_states_all_release( vlc_ml_playback_states_all* prefs );
VLC_API void vlc_ml_bookmark_release( vlc_ml_bookmark_t* p_bookmark );
VLC_API void vlc_ml_bookmark_list_release( vlc_ml_bookmark_list_t* p_list );

static inline vlc_ml_query_params_t vlc_ml_query_params_create()
{
    return (vlc_ml_query_params_t) {
        .psz_pattern = NULL,
        .i_nbResults = 0,
        .i_offset = 0,
        .i_sort = VLC_ML_SORTING_DEFAULT,
        .b_desc = false
    };
}

static inline int vlc_ml_add_folder( vlc_medialibrary_t* p_ml, const char* psz_folder )
{
    return vlc_ml_control( p_ml, VLC_ML_ADD_FOLDER, psz_folder );
}

static inline int vlc_ml_remove_folder( vlc_medialibrary_t* p_ml, const char* psz_folder )
{
    return vlc_ml_control( p_ml, VLC_ML_REMOVE_FOLDER, psz_folder );
}

static inline int vlc_ml_ban_folder( vlc_medialibrary_t* p_ml, const char* psz_folder )
{
    return vlc_ml_control( p_ml, VLC_ML_BAN_FOLDER, psz_folder );
}

static inline int vlc_ml_unban_folder( vlc_medialibrary_t* p_ml, const char* psz_folder )
{
    return vlc_ml_control( p_ml, VLC_ML_UNBAN_FOLDER, psz_folder );
}

static inline int vlc_ml_list_folder( vlc_medialibrary_t* p_ml,
                                      vlc_ml_entry_point_list_t** pp_entrypoints )
{
    return vlc_ml_control( p_ml, VLC_ML_LIST_FOLDERS, pp_entrypoints );
}

static inline int vlc_ml_is_indexed( vlc_medialibrary_t* p_ml,
                                     const char* psz_mrl, bool* p_res )
{
    return vlc_ml_control( p_ml, VLC_ML_IS_INDEXED, psz_mrl, p_res );
}

static inline int vlc_ml_reload_folder( vlc_medialibrary_t* p_ml, const char* psz_mrl )
{
    return vlc_ml_control( p_ml, VLC_ML_RELOAD_FOLDER, psz_mrl );
}

static inline int vlc_ml_pause_background( vlc_medialibrary_t* p_ml )
{
    return vlc_ml_control( p_ml, VLC_ML_PAUSE_BACKGROUND );
}

static inline int vlc_ml_resume_background( vlc_medialibrary_t* p_ml )
{
    return vlc_ml_control( p_ml, VLC_ML_RESUME_BACKGROUND );
}

static inline int vlc_ml_clear_history( vlc_medialibrary_t* p_ml )
{
    return vlc_ml_control( p_ml, VLC_ML_CLEAR_HISTORY );
}

static inline vlc_ml_media_t* vlc_ml_new_external_media( vlc_medialibrary_t* p_ml, const char* psz_mrl )
{
    vlc_ml_media_t* res;
    if ( vlc_ml_control( p_ml, VLC_ML_NEW_EXTERNAL_MEDIA, psz_mrl, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline vlc_ml_media_t* vlc_ml_new_stream( vlc_medialibrary_t* p_ml, const char* psz_mrl )
{
    vlc_ml_media_t* res;
    if ( vlc_ml_control( p_ml, VLC_ML_NEW_STREAM, psz_mrl, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline int vlc_ml_media_increase_playcount( vlc_medialibrary_t* p_ml, int64_t i_media_id )
{
    return vlc_ml_control( p_ml, VLC_ML_MEDIA_INCREASE_PLAY_COUNT, i_media_id );
}

static inline int vlc_ml_media_get_playback_state( vlc_medialibrary_t* p_ml, int64_t i_media_id, int i_state, char** ppsz_result )
{
    return vlc_ml_control( p_ml, VLC_ML_MEDIA_GET_MEDIA_PLAYBACK_STATE, i_media_id, i_state, ppsz_result );
}

static inline int vlc_ml_media_set_playback_state( vlc_medialibrary_t* p_ml, int64_t i_media_id, int i_state, const char* psz_value )
{
    return vlc_ml_control( p_ml, VLC_ML_MEDIA_SET_MEDIA_PLAYBACK_STATE, i_media_id, i_state, psz_value );
}

static inline int vlc_ml_media_get_all_playback_pref( vlc_medialibrary_t* p_ml,
                                                      int64_t i_media_id,
                                                      vlc_ml_playback_states_all* prefs )
{
    return vlc_ml_control( p_ml,VLC_ML_MEDIA_GET_ALL_MEDIA_PLAYBACK_STATES, i_media_id, prefs );
}

static inline int vlc_ml_media_set_all_playback_states( vlc_medialibrary_t* p_ml,
                                                        int64_t i_media_id,
                                                        const vlc_ml_playback_states_all* prefs )
{
    return vlc_ml_control( p_ml, VLC_ML_MEDIA_SET_ALL_MEDIA_PLAYBACK_STATES, i_media_id, prefs );
}

static inline int vlc_ml_media_set_thumbnail( vlc_medialibrary_t* p_ml, int64_t i_media_id,
                                              const char* psz_mrl, vlc_ml_thumbnail_size_t sizeType )
{
    return vlc_ml_control( p_ml, VLC_ML_MEDIA_SET_THUMBNAIL, i_media_id, psz_mrl, sizeType );
}

static inline int vlc_ml_media_generate_thumbnail( vlc_medialibrary_t* p_ml, int64_t i_media_id,
                                                   vlc_ml_thumbnail_size_t size_type,
                                                   uint32_t i_desired_width,
                                                   uint32_t i_desired_height,
                                                   float position )
{
    return vlc_ml_control( p_ml, VLC_ML_MEDIA_GENERATE_THUMBNAIL, i_media_id,
                           size_type, i_desired_width, i_desired_height, position );
}

static inline int vlc_ml_media_add_external_mrl( vlc_medialibrary_t* p_ml, int64_t i_media_id,
                                                 const char* psz_mrl, int i_type )
{
    return vlc_ml_control( p_ml, VLC_ML_MEDIA_ADD_EXTERNAL_MRL, i_media_id, psz_mrl, i_type );
}

static inline int vlc_ml_media_set_type( vlc_medialibrary_t* p_ml, int64_t i_media_id,
                                         vlc_ml_media_type_t i_type )
{
    return vlc_ml_control( p_ml, VLC_ML_MEDIA_SET_TYPE, i_media_id, (int)i_type );
}

static inline vlc_ml_bookmark_list_t*
vlc_ml_list_media_bookmarks( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params,
                             int64_t i_media_id )
{
    assert( p_ml != NULL );
    vlc_ml_bookmark_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_MEDIA_BOOKMARKS, params, i_media_id,
                      &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline int
vlc_ml_media_add_bookmark( vlc_medialibrary_t* p_ml, int64_t i_media_id, int64_t i_time )
{
    assert( p_ml != NULL );
    return vlc_ml_control( p_ml, VLC_ML_MEDIA_ADD_BOOKMARK, i_media_id, i_time );
}

static inline int
vlc_ml_media_remove_bookmark( vlc_medialibrary_t* p_ml, int64_t i_media_id, int64_t i_time )
{
    assert( p_ml != NULL );
    return vlc_ml_control( p_ml, VLC_ML_MEDIA_REMOVE_BOOKMARK, i_media_id, i_time );
}

static inline int
vlc_ml_media_update_bookmark( vlc_medialibrary_t* p_ml, int64_t i_media_id,
                              int64_t i_time, const char* psz_name,
                              const char* psz_desc )
{
    assert( p_ml != NULL );
    return vlc_ml_control( p_ml, VLC_ML_MEDIA_UPDATE_BOOKMARK, i_media_id,
                           i_time, psz_name, psz_desc );
}

static inline int
vlc_ml_media_remove_all_bookmarks( vlc_medialibrary_t* p_ml, int64_t i_media_id )
{
    assert( p_ml != NULL );
    return vlc_ml_control( p_ml, VLC_ML_MEDIA_REMOVE_ALL_BOOKMARKS, i_media_id );
}

static inline vlc_ml_media_t* vlc_ml_get_media( vlc_medialibrary_t* p_ml, int64_t i_media_id )
{
    return (vlc_ml_media_t*)vlc_ml_get( p_ml, VLC_ML_GET_MEDIA, i_media_id );
}

static inline vlc_ml_media_t* vlc_ml_get_media_by_mrl( vlc_medialibrary_t* p_ml,
                                                       const char* psz_mrl )
{
    return (vlc_ml_media_t*)vlc_ml_get( p_ml, VLC_ML_GET_MEDIA_BY_MRL, psz_mrl );
}

static inline input_item_t* vlc_ml_get_input_item( vlc_medialibrary_t* p_ml, int64_t i_media_id )
{
    return (input_item_t*)vlc_ml_get( p_ml, VLC_ML_GET_INPUT_ITEM, i_media_id );
}

static inline input_item_t* vlc_ml_get_input_item_by_mrl( vlc_medialibrary_t* p_ml,
                                                          const char* psz_mrl )
{
    return (input_item_t*)vlc_ml_get( p_ml, VLC_ML_GET_INPUT_ITEM_BY_MRL, psz_mrl );
}

static inline vlc_ml_album_t* vlc_ml_get_album( vlc_medialibrary_t* p_ml, int64_t i_album_id )
{
    return (vlc_ml_album_t*)vlc_ml_get( p_ml, VLC_ML_GET_ALBUM, i_album_id );
}

static inline vlc_ml_artist_t* vlc_ml_get_artist( vlc_medialibrary_t* p_ml, int64_t i_artist_id )
{
    return (vlc_ml_artist_t*)vlc_ml_get( p_ml, VLC_ML_GET_ARTIST, i_artist_id );
}

static inline vlc_ml_genre_t* vlc_ml_get_genre( vlc_medialibrary_t* p_ml, int64_t i_genre_id )
{
    return (vlc_ml_genre_t*)vlc_ml_get( p_ml, VLC_ML_GET_GENRE, i_genre_id );
}

static inline vlc_ml_show_t* vlc_ml_get_show( vlc_medialibrary_t* p_ml, int64_t i_show_id )
{
    return (vlc_ml_show_t*)vlc_ml_get( p_ml, VLC_ML_GET_SHOW, i_show_id );
}

static inline vlc_ml_playlist_t* vlc_ml_get_playlist( vlc_medialibrary_t* p_ml, int64_t i_playlist_id )
{
    return (vlc_ml_playlist_t*)vlc_ml_get( p_ml, VLC_ML_GET_PLAYLIST, i_playlist_id );
}

static inline vlc_ml_media_list_t* vlc_ml_list_media_of( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int i_parent_type, int64_t i_parent_id )
{
    vlc_assert( p_ml != NULL );
    vlc_ml_media_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_MEDIA_OF, params, i_parent_type, i_parent_id, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline size_t vlc_ml_count_media_of( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int i_parent_type, int64_t i_parent_id )
{
    vlc_assert( p_ml != NULL );
    size_t res;
    if ( vlc_ml_list( p_ml, VLC_ML_COUNT_MEDIA_OF, params, i_parent_type, i_parent_id, &res ) != VLC_SUCCESS )
        return 0;
    return res;
}

static inline vlc_ml_artist_list_t* vlc_ml_list_artist_of( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int i_parent_type, int64_t i_parent_id )
{
    vlc_assert( p_ml != NULL );
    vlc_ml_artist_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_ARTISTS_OF, params, i_parent_type, i_parent_id, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline size_t vlc_ml_count_artists_of( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int i_parent_type, int64_t i_parent_id )
{
    vlc_assert( p_ml != NULL );
    size_t res;
    if ( vlc_ml_list( p_ml, VLC_ML_COUNT_ARTISTS_OF, params, i_parent_type, i_parent_id, &res ) != VLC_SUCCESS )
        return 0;
    return res;
}

static inline vlc_ml_album_list_t* vlc_ml_list_albums_of( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int i_parent_type, int64_t i_parent_id )
{
    vlc_assert( p_ml != NULL );
    vlc_ml_album_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_ALBUMS_OF, params, i_parent_type, i_parent_id, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline size_t vlc_ml_count_albums_of( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int i_parent_type, int64_t i_parent_id )
{
    vlc_assert( p_ml != NULL );
    size_t res;
    if ( vlc_ml_list( p_ml, VLC_ML_COUNT_ALBUMS_OF, params, i_parent_type, i_parent_id, &res ) != VLC_SUCCESS )
        return 0;
    return res;
}

static inline vlc_ml_media_list_t* vlc_ml_list_album_tracks( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int64_t i_album_id )
{
    vlc_assert( p_ml != NULL );
    vlc_ml_media_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_ALBUM_TRACKS, params, i_album_id, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline size_t vlc_ml_count_album_tracks( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int64_t i_album_id )
{
    vlc_assert( p_ml != NULL );
    size_t count;
    if ( vlc_ml_list( p_ml, VLC_ML_COUNT_ALBUM_TRACKS, params, i_album_id, &count ) != VLC_SUCCESS )
        return 0;
    return count;
}

static inline vlc_ml_media_list_t* vlc_ml_list_album_artists( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int64_t i_album_id )
{
    vlc_assert( p_ml != NULL );
    vlc_ml_media_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_ALBUM_ARTISTS, params, i_album_id, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline size_t vlc_ml_count_album_artists( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int64_t i_album_id )
{
    vlc_assert( p_ml != NULL );
    size_t count;
    if ( vlc_ml_list( p_ml, VLC_ML_COUNT_ALBUM_ARTISTS, params, i_album_id, &count ) != VLC_SUCCESS )
        return 0;
    return count;
}

static inline vlc_ml_album_list_t* vlc_ml_list_artist_albums( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int64_t i_artist_id )
{
    vlc_assert( p_ml != NULL );
    vlc_ml_album_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_ARTIST_ALBUMS, params, i_artist_id, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline size_t vlc_ml_count_artist_albums( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int64_t i_artist_id )
{
    vlc_assert( p_ml != NULL );
    size_t count;
    if ( vlc_ml_list( p_ml, VLC_ML_COUNT_ARTIST_ALBUMS, params, i_artist_id, &count ) != VLC_SUCCESS )
        return 0;
    return count;
}

static inline vlc_ml_media_list_t* vlc_ml_list_artist_tracks( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int64_t i_artist_id )
{
    vlc_assert( p_ml != NULL );
    vlc_ml_media_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_ARTIST_TRACKS, params, i_artist_id, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline size_t vlc_ml_count_artist_tracks( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int64_t i_artist_id )
{
    vlc_assert( p_ml != NULL );
    size_t count;
    if ( vlc_ml_list( p_ml, VLC_ML_COUNT_ARTIST_TRACKS, params, i_artist_id, &count ) != VLC_SUCCESS )
        return 0;
    return count;
}

static inline vlc_ml_media_list_t* vlc_ml_list_video_media( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params )
{
    vlc_assert( p_ml != NULL );
    vlc_ml_media_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_VIDEOS, params, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline size_t vlc_ml_count_video_media( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params )
{
    vlc_assert( p_ml != NULL );
    size_t count;
    if ( vlc_ml_list( p_ml, VLC_ML_COUNT_VIDEOS, params, &count ) != VLC_SUCCESS )
        return 0;
    return count;
}

static inline vlc_ml_media_list_t* vlc_ml_list_audio_media( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params )
{
    vlc_assert( p_ml != NULL );
    vlc_ml_media_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_AUDIOS, params, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline size_t vlc_ml_count_audio_media( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params )
{
    vlc_assert( p_ml != NULL );
    size_t count;
    if ( vlc_ml_list( p_ml, VLC_ML_COUNT_AUDIOS, params, &count ) != VLC_SUCCESS )
        return 0;
    return count;
}

static inline vlc_ml_album_list_t* vlc_ml_list_albums( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params )
{
    vlc_assert( p_ml != NULL );
    vlc_ml_album_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_ALBUMS, params, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline size_t vlc_ml_count_albums( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params )
{
    vlc_assert( p_ml != NULL );
    size_t count;
    if ( vlc_ml_list( p_ml, VLC_ML_COUNT_ALBUMS, params, &count ) != VLC_SUCCESS )
        return 0;
    return count;
}

static inline vlc_ml_genre_list_t* vlc_ml_list_genres( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params )
{
    vlc_assert( p_ml != NULL );
    vlc_ml_genre_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_GENRES, params, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline size_t vlc_ml_count_genres( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params )
{
    vlc_assert( p_ml != NULL );
    size_t count;
    if ( vlc_ml_list( p_ml, VLC_ML_COUNT_GENRES, params, &count ) != VLC_SUCCESS )
        return 0;
    return count;
}

/**
 * @brief vlc_ml_list_artists
 * @param params Query parameters, or NULL for the default
 * @param b_include_all True if you wish to fetch artists without at least one album.
 * @return
 */
static inline vlc_ml_artist_list_t* vlc_ml_list_artists( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, bool b_include_all )
{
    vlc_assert( p_ml != NULL );
    vlc_ml_artist_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_ARTISTS, params, (int)b_include_all, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline size_t vlc_ml_count_artists( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, bool includeAll )
{
    vlc_assert( p_ml != NULL );
    size_t count;
    if ( vlc_ml_list( p_ml, VLC_ML_COUNT_ARTISTS, params, includeAll, &count ) != VLC_SUCCESS )
        return 0;
    return count;
}

static inline vlc_ml_show_list_t* vlc_ml_list_shows( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params )
{
    vlc_assert( p_ml != NULL );
    vlc_ml_show_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_SHOWS, params, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline size_t vlc_ml_count_shows( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params )
{
    vlc_assert( p_ml != NULL );
    size_t count;
    if ( vlc_ml_list( p_ml, VLC_ML_COUNT_SHOWS, params, &count ) != VLC_SUCCESS )
        return 0;
    return count;
}

static inline vlc_ml_artist_list_t* vlc_ml_list_genre_artists( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int64_t i_genre_id )
{
    vlc_assert( p_ml != NULL );
    vlc_ml_artist_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_GENRE_ARTISTS, params, i_genre_id, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline size_t vlc_ml_count_genre_artists( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int64_t i_genre_id )
{
    vlc_assert( p_ml != NULL );
    size_t count;
    if ( vlc_ml_list( p_ml, VLC_ML_COUNT_GENRE_ARTISTS, params, i_genre_id, &count ) != VLC_SUCCESS )
        return 0;
    return count;
}

static inline vlc_ml_media_list_t* vlc_ml_list_genre_tracks( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int64_t i_genre_id )
{
    vlc_assert( p_ml != NULL );
    vlc_ml_media_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_GENRE_TRACKS, params, i_genre_id, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline size_t vlc_ml_count_genre_tracks( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int64_t i_genre_id )
{
    vlc_assert( p_ml != NULL );
    size_t count;
    if ( vlc_ml_list( p_ml, VLC_ML_COUNT_GENRE_TRACKS, params, i_genre_id, &count ) != VLC_SUCCESS )
        return 0;
    return count;
}

static inline vlc_ml_album_list_t* vlc_ml_list_genre_albums( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int64_t i_genre_id )
{
    vlc_assert( p_ml != NULL );
    vlc_ml_album_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_GENRE_ALBUMS, params, i_genre_id, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline size_t vlc_ml_count_genre_albums( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int64_t i_genre_id )
{
    vlc_assert( p_ml != NULL );
    size_t count;
    if ( vlc_ml_list( p_ml, VLC_ML_COUNT_GENRE_ALBUMS, params, i_genre_id, &count ) != VLC_SUCCESS )
        return 0;
    return count;
}

static inline vlc_ml_media_list_t* vlc_ml_list_show_episodes( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int64_t i_show_id )
{
    vlc_assert( p_ml != NULL );
    vlc_ml_media_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_SHOW_EPISODES, params, i_show_id, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline size_t vlc_ml_count_show_episodes( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int64_t i_show_id )
{
    vlc_assert( p_ml != NULL );
    size_t count;
    if ( vlc_ml_list( p_ml, VLC_ML_COUNT_GENRE_ALBUMS, params, i_show_id, &count ) != VLC_SUCCESS )
        return 0;
    return count;
}

static inline vlc_ml_label_list_t* vlc_ml_list_media_labels( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int64_t i_media_id )
{
    vlc_assert( p_ml != NULL );
    vlc_ml_label_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_MEDIA_LABELS, params, i_media_id, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline size_t vlc_ml_count_media_labels( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int64_t i_media_id )
{
    vlc_assert( p_ml != NULL );
    size_t count;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_MEDIA_LABELS, params, i_media_id, &count ) != VLC_SUCCESS )
        return 0;
    return count;
}

static inline vlc_ml_media_list_t* vlc_ml_list_history( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params )
{
    vlc_assert( p_ml != NULL );
    vlc_ml_media_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_HISTORY, params, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline vlc_ml_media_list_t* vlc_ml_list_stream_history( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params )
{
    vlc_assert( p_ml != NULL );
    vlc_ml_media_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_STREAM_HISTORY, params, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline vlc_ml_playlist_list_t* vlc_ml_list_playlists( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params )
{
    vlc_assert( p_ml != NULL );
    vlc_ml_playlist_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_PLAYLISTS, params, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline size_t vlc_ml_count_playlists( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params )
{
    vlc_assert( p_ml != NULL );
    size_t count;
    if ( vlc_ml_list( p_ml, VLC_ML_COUNT_PLAYLISTS, params, &count ) != VLC_SUCCESS )
        return 0;
    return count;
}

static inline vlc_ml_media_list_t* vlc_ml_list_playlist_media( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int64_t i_playlist_id )
{
    vlc_assert( p_ml != NULL );
    vlc_ml_media_list_t* res;
    if ( vlc_ml_list( p_ml, VLC_ML_LIST_PLAYLIST_MEDIA, params, i_playlist_id, &res ) != VLC_SUCCESS )
        return NULL;
    return res;
}

static inline size_t vlc_ml_count_playlist_media( vlc_medialibrary_t* p_ml, const vlc_ml_query_params_t* params, int64_t i_playlist_id )
{
    vlc_assert( p_ml != NULL );
    size_t count;
    if ( vlc_ml_list( p_ml, VLC_ML_COUNT_PLAYLIST_MEDIA, params, i_playlist_id, &count ) != VLC_SUCCESS )
        return 0;
    return count;
}

#ifdef __cplusplus
}
#endif /* C++ */

#ifndef __cplusplus
# define vlc_ml_release( OBJ ) _Generic( ( OBJ ), \
    vlc_ml_show_t*: vlc_ml_show_release, \
    vlc_ml_artist_t*: vlc_ml_artist_release, \
    vlc_ml_album_t*: vlc_ml_album_release, \
    vlc_ml_genre_t*: vlc_ml_genre_release, \
    vlc_ml_media_t*: vlc_ml_media_release, \
    vlc_ml_playlist_t*: vlc_ml_playlist_release, \
    vlc_ml_label_list_t*: vlc_ml_label_list_release, \
    vlc_ml_file_list_t*: vlc_ml_file_list_release, \
    vlc_ml_artist_list_t*: vlc_ml_artist_list_release, \
    vlc_ml_media_list_t*: vlc_ml_media_list_release, \
    vlc_ml_album_list_t*: vlc_ml_album_list_release, \
    vlc_ml_show_list_t*: vlc_ml_show_list_release, \
    vlc_ml_genre_list_t*: vlc_ml_genre_list_release, \
    vlc_ml_playlist_list_t*: vlc_ml_playlist_list_release, \
    vlc_ml_entry_point_list_t*: vlc_ml_entry_point_list_release, \
    vlc_ml_playback_states_all*: vlc_ml_playback_states_all_release, \
    vlc_ml_bookmark_t*: vlc_ml_bookmark_release, \
    vlc_ml_bookmark_list_t*: vlc_ml_bookmark_list_release \
    )( OBJ )
#else
static inline void vlc_ml_release( vlc_ml_show_t* show ) { vlc_ml_show_release( show ); }
static inline void vlc_ml_release( vlc_ml_artist_t* artist ) { vlc_ml_artist_release( artist ); }
static inline void vlc_ml_release( vlc_ml_album_t* album ) { vlc_ml_album_release( album ); }
static inline void vlc_ml_release( vlc_ml_genre_t* genre ) { vlc_ml_genre_release( genre ); }
static inline void vlc_ml_release( vlc_ml_media_t* media ) { vlc_ml_media_release( media ); }
static inline void vlc_ml_release( vlc_ml_playlist_t* playlist ) { vlc_ml_playlist_release( playlist ); }
static inline void vlc_ml_release( vlc_ml_label_list_t* list ) { vlc_ml_label_list_release( list ); }
static inline void vlc_ml_release( vlc_ml_file_list_t* list ) { vlc_ml_file_list_release( list ); }
static inline void vlc_ml_release( vlc_ml_artist_list_t* list ) { vlc_ml_artist_list_release( list ); }
static inline void vlc_ml_release( vlc_ml_media_list_t* list ) { vlc_ml_media_list_release( list ); }
static inline void vlc_ml_release( vlc_ml_album_list_t* list ) { vlc_ml_album_list_release( list ); }
static inline void vlc_ml_release( vlc_ml_show_list_t* list ) { vlc_ml_show_list_release( list ); }
static inline void vlc_ml_release( vlc_ml_genre_list_t* list ) { vlc_ml_genre_list_release( list ); }
static inline void vlc_ml_release( vlc_ml_playlist_list_t* list ) { vlc_ml_playlist_list_release( list ); }
static inline void vlc_ml_release( vlc_ml_entry_point_list_t* list ) { vlc_ml_entry_point_list_release( list ); }
static inline void vlc_ml_release( vlc_ml_playback_states_all* prefs ) { vlc_ml_playback_states_all_release( prefs ); }
static inline void vlc_ml_release( vlc_ml_bookmark_t* bookmark ) { vlc_ml_bookmark_release( bookmark ); }
static inline void vlc_ml_release( vlc_ml_bookmark_list_t* list ) { vlc_ml_bookmark_list_release( list ); }
#endif

#endif /* VLC_MEDIA_LIBRARY_H */
