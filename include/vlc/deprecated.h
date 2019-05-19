/*****************************************************************************
 * deprecated.h:  libvlc deprecated API
 *****************************************************************************
 * Copyright (C) 1998-2008 VLC authors and VideoLAN
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
 * \retval true media object has been parsed
 * \retval false otherwise
 */
LIBVLC_DEPRECATED LIBVLC_API bool
   libvlc_media_is_parsed( libvlc_media_t *p_md );

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
 */
LIBVLC_DEPRECATED LIBVLC_API
void libvlc_playlist_play( libvlc_instance_t *p_instance );

/** @}*/

# ifdef __cplusplus
}
# endif

#endif /* _LIBVLC_DEPRECATED_H */
