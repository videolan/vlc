/*****************************************************************************
 * deprecated.h:  libvlc deprecated API
 *****************************************************************************
 * Copyright (C) 1998-2008 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Paul Saman <jpsaman@videolan.org>
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

#ifndef LIBVLC_DEPRECATED_H
#define LIBVLC_DEPRECATED_H 1

# ifdef __cplusplus
extern "C" {
# endif

/**
 * \ingroup libvlc libvlc_media_player
 * @{
 */

/**
 * Get movie fps rate
 *
 * This function is provided for backward compatibility. It cannot deal with
 * multiple video tracks. In LibVLC versions prior to 3.0, it would also fail
 * if the file format did not convey the frame rate explicitly.
 *
 * \deprecated Consider using libvlc_media_tracks_get() instead.
 *
 * \param p_mi the Media Player
 * \return frames per second (fps) for this playing movie, or 0 if unspecified
 */
LIBVLC_DEPRECATED
LIBVLC_API float libvlc_media_player_get_fps( libvlc_media_player_t *p_mi );

/** end bug */

/**
 * \deprecated Use libvlc_media_player_set_nsobject() instead
 */
LIBVLC_DEPRECATED
LIBVLC_API void libvlc_media_player_set_agl ( libvlc_media_player_t *p_mi, uint32_t drawable );

/**
 * \deprecated Use libvlc_media_player_get_nsobject() instead
 */
LIBVLC_DEPRECATED
LIBVLC_API uint32_t libvlc_media_player_get_agl ( libvlc_media_player_t *p_mi );

/**
 * \deprecated Use libvlc_track_description_list_release() instead
 */
LIBVLC_DEPRECATED LIBVLC_API
void libvlc_track_description_release( libvlc_track_description_t *p_track_description );

/** @}*/

/**
 * \ingroup libvlc libvlc_video
 * @{
 */

/**
 * Get current video height.
 * \deprecated Use libvlc_video_get_size() instead.
 *
 * \param p_mi the media player
 * \return the video pixel height or 0 if not applicable
 */
LIBVLC_DEPRECATED LIBVLC_API
int libvlc_video_get_height( libvlc_media_player_t *p_mi );

/**
 * Get current video width.
 * \deprecated Use libvlc_video_get_size() instead.
 *
 * \param p_mi the media player
 * \return the video pixel width or 0 if not applicable
 */
LIBVLC_DEPRECATED LIBVLC_API
int libvlc_video_get_width( libvlc_media_player_t *p_mi );

/**
 * Get the description of available titles.
 *
 * \param p_mi the media player
 * \return list containing description of available titles.
 * It must be freed with libvlc_track_description_list_release()
 */
LIBVLC_DEPRECATED LIBVLC_API libvlc_track_description_t *
        libvlc_video_get_title_description( libvlc_media_player_t *p_mi );

/**
 * Get the description of available chapters for specific title.
 *
 * \param p_mi the media player
 * \param i_title selected title
 * \return list containing description of available chapter for title i_title.
 * It must be freed with libvlc_track_description_list_release()
 */
LIBVLC_DEPRECATED LIBVLC_API libvlc_track_description_t *
        libvlc_video_get_chapter_description( libvlc_media_player_t *p_mi, int i_title );

/**
 * Set new video subtitle file.
 * \deprecated Use libvlc_media_player_add_slave() instead.
 *
 * \param p_mi the media player
 * \param psz_subtitle new video subtitle file
 * \return the success status (boolean)
 */
LIBVLC_DEPRECATED LIBVLC_API int
libvlc_video_set_subtitle_file( libvlc_media_player_t *p_mi, const char *psz_subtitle );

/**
 * Toggle teletext transparent status on video output.
 * \deprecated use libvlc_video_set_teletext() instead.
 *
 * \param p_mi the media player
 */
LIBVLC_DEPRECATED LIBVLC_API void
libvlc_toggle_teletext( libvlc_media_player_t *p_mi );

/** @}*/

/**
 * \ingroup libvlc libvlc_audio
 * @{
 */

/**
 * Backward compatibility stub. Do not use in new code.
 * \deprecated Use libvlc_audio_output_device_list_get() instead.
 * \return always 0.
 */
LIBVLC_DEPRECATED LIBVLC_API
int libvlc_audio_output_device_count( libvlc_instance_t *p_instance, const char *psz_audio_output );

/**
 * Backward compatibility stub. Do not use in new code.
 * \deprecated Use libvlc_audio_output_device_list_get() instead.
 * \return always NULL.
 */
LIBVLC_DEPRECATED LIBVLC_API
char *libvlc_audio_output_device_longname( libvlc_instance_t *p_instance, const char *psz_output,
                                           int i_device );

/**
 * Backward compatibility stub. Do not use in new code.
 * \deprecated Use libvlc_audio_output_device_list_get() instead.
 * \return always NULL.
 */
LIBVLC_DEPRECATED LIBVLC_API
char *libvlc_audio_output_device_id( libvlc_instance_t *p_instance, const char *psz_audio_output, int i_device );

/**
 * Stub for backward compatibility.
 * \return always -1.
 */
LIBVLC_DEPRECATED
LIBVLC_API int libvlc_audio_output_get_device_type( libvlc_media_player_t *p_mi );

/**
 * Stub for backward compatibility.
 */
LIBVLC_DEPRECATED
LIBVLC_API void libvlc_audio_output_set_device_type( libvlc_media_player_t *p_mp,
                                                     int device_type );

/** @}*/

/**
 * \ingroup libvlc libvlc_media
 * @{
 */

/**
 * Parse a media.
 *
 * This fetches (local) art, meta data and tracks information.
 * The method is synchronous.
 *
 * \deprecated This function could block indefinitely.
 *             Use libvlc_media_parse_with_options() instead
 *
 * \see libvlc_media_parse_with_options
 * \see libvlc_media_get_meta
 * \see libvlc_media_get_tracks_info
 *
 * \param p_md media descriptor object
 */
LIBVLC_DEPRECATED LIBVLC_API void
libvlc_media_parse( libvlc_media_t *p_md );

/**
 * Parse a media.
 *
 * This fetches (local) art, meta data and tracks information.
 * The method is the asynchronous of libvlc_media_parse().
 *
 * To track when this is over you can listen to libvlc_MediaParsedChanged
 * event. However if the media was already parsed you will not receive this
 * event.
 *
 * \deprecated You can't be sure to receive the libvlc_MediaParsedChanged
 *             event (you can wait indefinitely for this event).
 *             Use libvlc_media_parse_with_options() instead
 *
 * \see libvlc_media_parse
 * \see libvlc_MediaParsedChanged
 * \see libvlc_media_get_meta
 * \see libvlc_media_get_tracks_info
 *
 * \param p_md media descriptor object
 */
LIBVLC_DEPRECATED LIBVLC_API void
libvlc_media_parse_async( libvlc_media_t *p_md );

/**
 * Return true is the media descriptor object is parsed
 *
 * \deprecated This can return true in case of failure.
 *             Use libvlc_media_get_parsed_status() instead
 *
 * \see libvlc_MediaParsedChanged
 *
 * \param p_md media descriptor object
 * \return true if media object has been parsed otherwise it returns false
 *
 * \libvlc_return_bool
 */
LIBVLC_DEPRECATED LIBVLC_API int
   libvlc_media_is_parsed( libvlc_media_t *p_md );

/**
 * Get media descriptor's elementary streams description
 *
 * Note, you need to call libvlc_media_parse() or play the media at least once
 * before calling this function.
 * Not doing this will result in an empty array.
 *
 * \deprecated Use libvlc_media_tracks_get() instead
 *
 * \param p_md media descriptor object
 * \param tracks address to store an allocated array of Elementary Streams
 *        descriptions (must be freed by the caller) [OUT]
 *
 * \return the number of Elementary Streams
 */
LIBVLC_DEPRECATED LIBVLC_API
int libvlc_media_get_tracks_info( libvlc_media_t *p_md,
                                  libvlc_media_track_info_t **tracks );

/** @}*/

/**
 * \ingroup libvlc libvlc_media_list
 * @{
 */

LIBVLC_DEPRECATED int
    libvlc_media_list_add_file_content( libvlc_media_list_t * p_ml,
                                        const char * psz_uri );

/** @}*/

/**
 * \ingroup libvlc libvlc_media_discoverer
 * @{
 */

/**
 * \deprecated Use libvlc_media_discoverer_new() and libvlc_media_discoverer_start().
 */
LIBVLC_DEPRECATED LIBVLC_API libvlc_media_discoverer_t *
libvlc_media_discoverer_new_from_name( libvlc_instance_t * p_inst,
                                       const char * psz_name );

/**
 * Get media service discover object its localized name.
 *
 * \deprecated Useless, use libvlc_media_discoverer_list_get() to get the
 * longname of the service discovery.
 *
 * \param p_mdis media discover object
 * \return localized name or NULL if the media_discoverer is not started
 */
LIBVLC_DEPRECATED LIBVLC_API char *
libvlc_media_discoverer_localized_name( libvlc_media_discoverer_t * p_mdis );

/**
 * Get event manager from media service discover object.
 *
 * \deprecated Useless, media_discoverer events are only triggered when calling
 * libvlc_media_discoverer_start() and libvlc_media_discoverer_stop().
 *
 * \param p_mdis media service discover object
 * \return event manager object.
 */
LIBVLC_DEPRECATED LIBVLC_API libvlc_event_manager_t *
libvlc_media_discoverer_event_manager( libvlc_media_discoverer_t * p_mdis );

/** @}*/

/**
 * \ingroup libvlc libvlc_core
 * @{
 */

/**
 * Waits until an interface causes the instance to exit.
 * You should start at least one interface first, using libvlc_add_intf().
 *
 * \param p_instance the instance
 * \warning This function wastes one thread doing basically nothing.
 * libvlc_set_exit_handler() should be used instead.
 */
LIBVLC_DEPRECATED LIBVLC_API
void libvlc_wait( libvlc_instance_t *p_instance );


/** @}*/

/**
 * \ingroup libvlc_core
 * \defgroup libvlc_log_deprecated LibVLC logging (legacy)
 * @{
 */

/** This structure is opaque. It represents a libvlc log iterator */
typedef struct libvlc_log_iterator_t libvlc_log_iterator_t;

typedef struct libvlc_log_message_t
{
    int         i_severity;   /* 0=INFO, 1=ERR, 2=WARN, 3=DBG */
    const char *psz_type;     /* module type */
    const char *psz_name;     /* module name */
    const char *psz_header;   /* optional header */
    const char *psz_message;  /* message */
} libvlc_log_message_t;

/**
 * Always returns minus one.
 * This function is only provided for backward compatibility.
 *
 * \param p_instance ignored
 * \return always -1
 */
LIBVLC_DEPRECATED LIBVLC_API
unsigned libvlc_get_log_verbosity( const libvlc_instance_t *p_instance );

/**
 * This function does nothing.
 * It is only provided for backward compatibility.
 *
 * \param p_instance ignored
 * \param level ignored
 */
LIBVLC_DEPRECATED LIBVLC_API
void libvlc_set_log_verbosity( libvlc_instance_t *p_instance, unsigned level );

/**
 * This function does nothing useful.
 * It is only provided for backward compatibility.
 *
 * \param p_instance libvlc instance
 * \return an unique pointer or NULL on error
 */
LIBVLC_DEPRECATED LIBVLC_API
libvlc_log_t *libvlc_log_open( libvlc_instance_t *p_instance );

/**
 * Frees memory allocated by libvlc_log_open().
 *
 * \param p_log libvlc log instance or NULL
 */
LIBVLC_DEPRECATED LIBVLC_API
void libvlc_log_close( libvlc_log_t *p_log );

/**
 * Always returns zero.
 * This function is only provided for backward compatibility.
 *
 * \param p_log ignored
 * \return always zero
 */
LIBVLC_DEPRECATED LIBVLC_API
unsigned libvlc_log_count( const libvlc_log_t *p_log );

/**
 * This function does nothing.
 * It is only provided for backward compatibility.
 *
 * \param p_log ignored
 */
LIBVLC_DEPRECATED LIBVLC_API
void libvlc_log_clear( libvlc_log_t *p_log );

/**
 * This function does nothing useful.
 * It is only provided for backward compatibility.
 *
 * \param p_log ignored
 * \return an unique pointer or NULL on error or if the parameter was NULL
 */
LIBVLC_DEPRECATED LIBVLC_API
libvlc_log_iterator_t *libvlc_log_get_iterator( const libvlc_log_t *p_log );

/**
 * Frees memory allocated by libvlc_log_get_iterator().
 *
 * \param p_iter libvlc log iterator or NULL
 */
LIBVLC_DEPRECATED LIBVLC_API
void libvlc_log_iterator_free( libvlc_log_iterator_t *p_iter );

/**
 * Always returns zero.
 * This function is only provided for backward compatibility.
 *
 * \param p_iter ignored
 * \return always zero
 */
LIBVLC_DEPRECATED LIBVLC_API
int libvlc_log_iterator_has_next( const libvlc_log_iterator_t *p_iter );

/**
 * Always returns NULL.
 * This function is only provided for backward compatibility.
 *
 * \param p_iter libvlc log iterator or NULL
 * \param p_buf ignored
 * \return always NULL
 */
LIBVLC_DEPRECATED LIBVLC_API
libvlc_log_message_t *libvlc_log_iterator_next( libvlc_log_iterator_t *p_iter,
                                                libvlc_log_message_t *p_buf );

/** @}*/

/**
 * \ingroup libvlc
 * \defgroup libvlc_playlist LibVLC playlist (legacy)
 * @deprecated Use @ref libvlc_media_list instead.
 * @{
 * \file
 * LibVLC deprecated playlist API
 */

/**
 * Start playing (if there is any item in the playlist).
 *
 * Additionnal playlist item options can be specified for addition to the
 * item before it is played.
 *
 * \param p_instance the playlist instance
 * \param i_id the item to play. If this is a negative number, the next
 *        item will be selected. Otherwise, the item with the given ID will be
 *        played
 * \param i_options the number of options to add to the item
 * \param ppsz_options the options to add to the item
 */
LIBVLC_DEPRECATED LIBVLC_API
void libvlc_playlist_play( libvlc_instance_t *p_instance, int i_id,
                           int i_options, char **ppsz_options );

/** @}*/

# ifdef __cplusplus
}
# endif

#endif /* _LIBVLC_DEPRECATED_H */
