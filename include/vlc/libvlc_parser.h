/*****************************************************************************
 * libvlc_parser.h:  libvlc parser API
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

#ifndef VLC_LIBVLC_PARSER_H
#define VLC_LIBVLC_PARSER_H 1

# ifdef __cplusplus
extern "C" {
# endif

typedef struct libvlc_media_t libvlc_media_t;

/** \defgroup libvlc_parser LibVLC parser
 * \ingroup libvlc
 * @{
 * \file
 * LibVLC parser API
 */

/**
 *
 * Parse flags used by libvlc_media_parse_request()
 */
typedef enum libvlc_media_parse_flag_t
{
    /**
     * Parse media if it's a local file
     */
    libvlc_media_parse_local    = 0x01,
    /**
     * Parse media even if it's a network file
     */
    libvlc_media_parse_network  = 0x02,
    /**
     * Force parsing the media even if it would be skipped.
     */
    libvlc_media_parse_forced   = 0x04,
    /**
     * Fetch meta and cover art using local resources
     */
    libvlc_media_fetch_local    = 0x08,
    /**
     * Fetch meta and cover art using network resources
     */
    libvlc_media_fetch_network  = 0x10,
    /**
     * Interact with the user (via libvlc_dialog_cbs) when preparsing this item
     * (and not its sub items). Set this flag in order to receive a callback
     * when the input is asking for credentials.
     */
    libvlc_media_do_interact    = 0x20,
} libvlc_media_parse_flag_t;

/**
 * Parse the media asynchronously with options.
 *
 * This fetches (local or network) art, meta data and/or tracks information.
 *
 * To track when this is over you can listen to libvlc_MediaParsedChanged
 * event. However if this functions returns an error, you will not receive any
 * events.
 *
 * It uses a flag to specify parse options (see libvlc_media_parse_flag_t). All
 * these flags can be combined. By default, media is parsed if it's a local
 * file.
 *
 * \note Parsing can be aborted with libvlc_media_parse_stop().
 *
 * \see libvlc_MediaParsedChanged
 * \see libvlc_media_get_meta
 * \see libvlc_media_get_tracklist
 * \see libvlc_media_get_parsed_status
 * \see libvlc_media_parse_flag_t
 *
 * \param inst LibVLC instance that is to parse the media
 * \param p_md media descriptor object
 * \param parse_flag parse options:
 * \param timeout maximum time allowed to preparse the media. If -1, the
 * default "preparse-timeout" option will be used as a timeout. If 0, it will
 * wait indefinitely. If > 0, the timeout will be used (in milliseconds).
 * \return -1 in case of error, 0 otherwise
 * \version LibVLC 4.0.0 or later
 */
LIBVLC_API int
libvlc_media_parse_request( libvlc_instance_t *inst, libvlc_media_t *p_md,
                            libvlc_media_parse_flag_t parse_flag,
                            int timeout );

/**
 * Stop the parsing of the media
 *
 * When the media parsing is stopped, the libvlc_MediaParsedChanged event will
 * be sent with the libvlc_media_parsed_status_timeout status.
 *
 * \see libvlc_media_parse_request()
 *
 * \param inst LibVLC instance that is to cease or give up parsing the media
 * \param p_md media descriptor object
 * \version LibVLC 3.0.0 or later
 */
LIBVLC_API void
libvlc_media_parse_stop( libvlc_instance_t *inst, libvlc_media_t *p_md );

/** @}*/

# ifdef __cplusplus
}
# endif

#endif /* VLC_LIBVLC_PARSER_H */
