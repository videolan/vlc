/*****************************************************************************
 * libvlc.h:  libvlc_* new external API structures
 *****************************************************************************
 * Copyright (C) 1998-2007 the VideoLAN team
 * $Id $
 *
 * Authors: Filippo Carone <littlejohn@videolan.org>
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

#ifndef _LIBVLC_STRUCTURES_H
#define _LIBVLC_STRUCTURES_H 1

#include <vlc/vlc.h>

# ifdef __cplusplus
extern "C" {
# endif

/** This structure is opaque. It represents a libvlc instance */
typedef struct libvlc_instance_t libvlc_instance_t;

/*****************************************************************************
 * Exceptions
 *****************************************************************************/

/** defgroup libvlc_exception Exceptions
 * \ingroup libvlc
 * LibVLC Exceptions handling
 * @{
 */

typedef struct libvlc_exception_t
{
    int b_raised;
    int i_code;
    char *psz_message;
} libvlc_exception_t;

/**@} */

/*****************************************************************************
 * Tag
 *****************************************************************************/
/** defgroup libvlc_tag Tag
 * \ingroup libvlc
 * LibVLC Tag  support in media descriptor
 * @{
 */

typedef struct libvlc_tag_query_t libvlc_tag_query_t;
typedef char * libvlc_tag_t;

/**@} */

/*****************************************************************************
 * Time
 *****************************************************************************/
/** defgroup libvlc_time Time
 * \ingroup libvlc
 * LibVLC Time support in libvlc
 * @{
 */

typedef vlc_int64_t libvlc_time_t;

/**@} */

/*****************************************************************************
 * Media Descriptor
 *****************************************************************************/
/** defgroup libvlc_media_descriptor MediaDescriptor
 * \ingroup libvlc
 * LibVLC Media Descriptor handling
 * @{
 */

/* Meta Handling */
/** defgroup libvlc_meta Meta
 * \ingroup libvlc_media_descriptor
 * LibVLC Media Meta
 * @{
 */

typedef enum libvlc_meta_t {
    libvlc_meta_Title,
    libvlc_meta_Artist,
    libvlc_meta_Genre,
    libvlc_meta_Copyright,
    libvlc_meta_Album,
    libvlc_meta_TrackNumber,
    libvlc_meta_Description,
    libvlc_meta_Rating,
    libvlc_meta_Date,
    libvlc_meta_Setting,
    libvlc_meta_URL,
    libvlc_meta_Language,
    libvlc_meta_NowPlaying,
    libvlc_meta_Publisher,
    libvlc_meta_EncodedBy,
    libvlc_meta_ArtworkURL,
    libvlc_meta_TrackID
} libvlc_meta_t;

/**@} */

typedef struct libvlc_media_descriptor_t libvlc_media_descriptor_t;

/**@} */


/*****************************************************************************
 * Media Instance
 *****************************************************************************/
/** defgroup libvlc_media_instance MediaInstance
 * \ingroup libvlc
 * LibVLC Media Instance handling
 * @{
 */

typedef struct libvlc_media_instance_t libvlc_media_instance_t;

typedef enum libvlc_state_t
{
    libvlc_NothingSpecial,
    libvlc_Stopped,
    libvlc_Opening,
    libvlc_Buffering,
    libvlc_Ended,
    libvlc_Error,
    libvlc_Playing,
    libvlc_Paused
} libvlc_state_t;

/**@} */

/*****************************************************************************
 * Media List
 *****************************************************************************/
/** defgroup libvlc_media_list MediaList
 * \ingroup libvlc
 * LibVLC Media List handling
 * @{
 */

typedef struct libvlc_media_list_t libvlc_media_list_t;
typedef struct libvlc_media_list_view_t libvlc_media_list_view_t;

/**@} */

/*****************************************************************************
 * Dynamic Media List
 *****************************************************************************/
/** defgroup libvlc_media_list MediaList
 * \ingroup libvlc
 * LibVLC Dynamic Media list: Media list with content synchronized with
 * an other playlist
 * @{
 */

typedef struct libvlc_dynamic_media_list_t libvlc_dynamic_media_list_t;

/**@} */

/*****************************************************************************
 * Media List Player
 *****************************************************************************/
/** defgroup libvlc_media_list_player MediaListPlayer
 * \ingroup libvlc
 * LibVLC Media List Player handling
 * @{
 */

typedef struct libvlc_media_list_player_t libvlc_media_list_player_t;

/**@} */

/*****************************************************************************
 * Media Library
 *****************************************************************************/
/** defgroup libvlc_media_library Media Library
 * \ingroup libvlc
 * LibVLC Media Library
 * @{
 */

typedef struct libvlc_media_library_t libvlc_media_library_t;

/**@} */

/*****************************************************************************
 * Playlist
 *****************************************************************************/
/** defgroup libvlc_playlist Playlist
 * \ingroup libvlc
 * LibVLC Playlist handling
 * @{
 */

typedef struct libvlc_playlist_item_t
{
    int i_id;
    char * psz_uri;
    char * psz_name;

} libvlc_playlist_item_t;

/**@} */


/*****************************************************************************
 * Video
 *****************************************************************************/
/** defgroup libvlc_video Video
 * \ingroup libvlc
 * LibVLC Video handling
 * @{
 */
 
/**
* Downcast to this general type as placeholder for a platform specific one, such as:
*  Drawable on X11,
*  CGrafPort on MacOSX,
*  HWND on win32
*/
typedef int libvlc_drawable_t;

/**
* Rectangle type for video geometry
*/
typedef struct libvlc_rectangle_t
{
    int top, left;
    int bottom, right;
}
libvlc_rectangle_t;

/**@} */


/*****************************************************************************
 * Services/Media Discovery
 *****************************************************************************/
/** defgroup libvlc_media_discoverer Media Discoverer
 * \ingroup libvlc
 * LibVLC Media Discoverer
 * @{
 */

typedef struct libvlc_media_discoverer_t libvlc_media_discoverer_t;

/**@} */

/*****************************************************************************
 * Message log handling
 *****************************************************************************/

/** defgroup libvlc_log Log
 * \ingroup libvlc
 * LibVLC Message Logging
 * @{
 */

/** This structure is opaque. It represents a libvlc log instance */
typedef struct libvlc_log_t libvlc_log_t;

/** This structure is opaque. It represents a libvlc log iterator */
typedef struct libvlc_log_iterator_t libvlc_log_iterator_t;

typedef struct libvlc_log_message_t
{
    unsigned    sizeof_msg;   /* sizeof() of message structure, must be filled in by user */
    int         i_severity;   /* 0=INFO, 1=ERR, 2=WARN, 3=DBG */
    const char *psz_type;     /* module type */
    const char *psz_name;     /* module name */
    const char *psz_header;   /* optional header */
    const char *psz_message;  /* message */
} libvlc_log_message_t;

/**@} */

/*****************************************************************************
 * Callbacks handling
 *****************************************************************************/

/** defgroup libvlc_callbacks Callbacks
 * \ingroup libvlc
 * LibVLC Event Callbacks
 * @{
 */
 
/**
 * Available events: (XXX: being reworked)
 * - libvlc_MediaInstanceReachedEnd
 */

typedef enum libvlc_event_type_t {
    libvlc_MediaDescriptorMetaChanged,
    libvlc_MediaDescriptorSubItemAdded,
    libvlc_MediaDescriptorDurationChanged,
    libvlc_MediaDescriptorPreparsedChanged,
    libvlc_MediaDescriptorFreed,
    libvlc_MediaDescriptorStateChanged,

    libvlc_MediaInstancePlayed,
    libvlc_MediaInstancePaused,
    libvlc_MediaInstanceReachedEnd,
    libvlc_MediaInstanceEncounteredError,
    libvlc_MediaInstanceTimeChanged,
    libvlc_MediaInstancePositionChanged,
    libvlc_MediaInstanceSeekableChanged,
    libvlc_MediaInstancePausableChanged,

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
    libvlc_MediaDiscovererEnded

} libvlc_event_type_t;

/**
 * An Event
 * \param type the even type
 * \param p_obj the sender object
 * \param u Event dependent content
 */

typedef struct libvlc_event_t
{
    libvlc_event_type_t type;
    void * p_obj;
    union event_type_specific
    {
        /* media descriptor */
        struct
        {
            libvlc_meta_t meta_type;
        } media_descriptor_meta_changed;
        struct
        {
            libvlc_media_descriptor_t * new_child;
        } media_descriptor_subitem_added;
        struct
        {
            vlc_int64_t new_duration;
        } media_descriptor_duration_changed;
        struct
        {
            int new_status;
        } media_descriptor_preparsed_changed;
        struct
        {
            libvlc_media_descriptor_t * md;
        } media_descriptor_freed;
        struct
        {
            libvlc_state_t new_state;
        } media_descriptor_state_changed;
            
        /* media instance */
        struct
        {
            float new_position;
        } media_instance_position_changed;
        struct
        {
            libvlc_time_t new_time;
        } media_instance_time_changed;
        struct
        {
            libvlc_time_t new_seekable;
        } media_instance_seekable_changed;
        struct
        {
            libvlc_time_t new_pausable;
        } media_instance_pausable_changed;

        /* media list */
        struct
        {
            libvlc_media_descriptor_t * item;
            int index;
        } media_list_item_added;
        struct
        {
            libvlc_media_descriptor_t * item;
            int index;
        } media_list_will_add_item;
        struct
        {
            libvlc_media_descriptor_t * item;
            int index;
        } media_list_item_deleted;
        struct
        {
            libvlc_media_descriptor_t * item;
            int index;
        } media_list_will_delete_item;

        /* media list view */
        struct
        {
            libvlc_media_descriptor_t * item;
            int index;
        } media_list_view_item_added;
        struct
        {
            libvlc_media_descriptor_t * item;
            int index;
        } media_list_view_will_add_item;
        struct
        {
            libvlc_media_descriptor_t * item;
            int index;
        } media_list_view_item_deleted;
        struct
        {
            libvlc_media_descriptor_t * item;
            int index;
        } media_list_view_will_delete_item;

        /* media discoverer */
        struct
        {
            void * unused;
        } media_media_discoverer_started;
        struct
        {
            void * unused;
        } media_media_discoverer_ended;

    } u;
} libvlc_event_t;

/**
 * Event manager that belongs to a libvlc object, and from whom events can
 * be received.
 */

typedef struct libvlc_event_manager_t libvlc_event_manager_t;

/**
 * Callback function notification
 * \param p_event the event triggering the callback
 */

typedef void ( *libvlc_callback_t )( const libvlc_event_t *, void * );

/**@} */

# ifdef __cplusplus
}
# endif

#endif
