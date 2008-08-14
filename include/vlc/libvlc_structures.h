/*****************************************************************************
 * libvlc.h:  libvlc_* new external API structures
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
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

#ifndef LIBVLC_STRUCTURES_H
#define LIBVLC_STRUCTURES_H 1

/**
 * \file
 * This file defines libvlc_* new external API structures
 */

#include <stdint.h>

# ifdef __cplusplus
extern "C" {
# endif

/** This structure is opaque. It represents a libvlc instance */
typedef struct libvlc_instance_t libvlc_instance_t;

/*****************************************************************************
 * Exceptions
 *****************************************************************************/

/** \defgroup libvlc_exception libvlc_exception
 * \ingroup libvlc_core
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
 * Time
 *****************************************************************************/
/** \defgroup libvlc_time libvlc_time
 * \ingroup libvlc_core
 * LibVLC Time support in libvlc
 * @{
 */

typedef int64_t libvlc_time_t;

/**@} */

/*****************************************************************************
 * Media Descriptor
 *****************************************************************************/
/** \defgroup libvlc_media libvlc_media
 * \ingroup libvlc
 * LibVLC Media Descriptor handling
 * @{
 */

/* Meta Handling */
/** defgroup libvlc_meta libvlc_meta
 * \ingroup libvlc_media
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

typedef struct libvlc_media_t libvlc_media_t;

/**@} */


/*****************************************************************************
 * Media Instance
 *****************************************************************************/
/** \defgroup libvlc_media_player libvlc_media_player
 * \ingroup libvlc
 * LibVLC Media Instance handling
 * @{
 */

typedef struct libvlc_media_player_t libvlc_media_player_t;

/**
 * Note the order of libvlc_state_t enum must match exactly the order of
 * @see mediacontrol_PlayerStatus and @see input_state_e enums.
 *
 * Expected states by web plugins are:
 * IDLE/CLOSE=0, OPENING=1, BUFFERING=2, PLAYING=3, PAUSED=4,
 * STOPPING=5, FORWARD=6, BACKWARD=7, ENDED=8, ERROR=9
 */
typedef enum libvlc_state_t
{
    libvlc_NothingSpecial=0,
    libvlc_Opening,
    libvlc_Buffering,
    libvlc_Playing,
    libvlc_Paused,
    libvlc_Stopped,
    libvlc_Forward,
    libvlc_Backward,
    libvlc_Ended,
    libvlc_Error
} libvlc_state_t;

/**@} */

/*****************************************************************************
 * Media List
 *****************************************************************************/
/** \defgroup libvlc_media_list libvlc_media_list
 * \ingroup libvlc
 * LibVLC Media List handling
 * @{
 */

typedef struct libvlc_media_list_t libvlc_media_list_t;
typedef struct libvlc_media_list_view_t libvlc_media_list_view_t;


/*****************************************************************************
 * Media List Player
 *****************************************************************************/
/** \defgroup libvlc_media_list_player libvlc_media_list_player
 * \ingroup libvlc_media_list
 * LibVLC Media List Player handling
 * @{
 */

typedef struct libvlc_media_list_player_t libvlc_media_list_player_t;

/**@} libvlc_media_list_player */

/**@} libvlc_media_list */

/*****************************************************************************
 * Media Library
 *****************************************************************************/
/** \defgroup libvlc_media_library libvlc_media_library
 * \ingroup libvlc
 * LibVLC Media Library
 * @{
 */

typedef struct libvlc_media_library_t libvlc_media_library_t;

/**@} */

/*****************************************************************************
 * Playlist
 *****************************************************************************/
/** \defgroup libvlc_playlist libvlc_playlist (Deprecated)
 * \ingroup libvlc
 * LibVLC Playlist handling (Deprecated)
 * @deprecated Use media_list
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
/** \defgroup libvlc_video libvlc_video
 * \ingroup libvlc_media_player
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
/** \defgroup libvlc_media_discoverer libvlc_media_discoverer
 * \ingroup libvlc
 * LibVLC Media Discoverer
 * @{
 */

typedef struct libvlc_media_discoverer_t libvlc_media_discoverer_t;

/**@} */

/*****************************************************************************
 * Message log handling
 *****************************************************************************/

/** \defgroup libvlc_log libvlc_log
 * \ingroup libvlc_core
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

# ifdef __cplusplus
}
# endif

#endif
