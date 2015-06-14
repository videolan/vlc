/*****************************************************************************
 * libvlc_media_discoverer.h:  libvlc external API
 *****************************************************************************
 * Copyright (C) 1998-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Paul Saman <jpsaman@videolan.org>
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

#ifndef VLC_LIBVLC_MEDIA_DISCOVERER_H
#define VLC_LIBVLC_MEDIA_DISCOVERER_H 1

# ifdef __cplusplus
extern "C" {
# endif

/** \defgroup libvlc_media_discoverer LibVLC media discovery
 * \ingroup libvlc
 * LibVLC media discovery finds available media via various means.
 * This corresponds to the service discovery functionality in VLC media player.
 * Different plugins find potential medias locally (e.g. user media directory),
 * from peripherals (e.g. video capture device), on the local network
 * (e.g. SAP) or on the Internet (e.g. Internet radios).
 * @{
 * \file
 * LibVLC media discovery external API
 */

typedef struct libvlc_media_discoverer_t libvlc_media_discoverer_t;

/**
 * \deprecated Use libvlc_media_discoverer_new() and libvlc_media_discoverer_start().
 */
LIBVLC_DEPRECATED LIBVLC_API libvlc_media_discoverer_t *
libvlc_media_discoverer_new_from_name( libvlc_instance_t * p_inst,
                                       const char * psz_name );

/**
 * Create a media discoverer object by name.
 *
 * After this object is created, you should attach to events in order to be
 * notified of the discoverer state.
 * You should also attach to media_list events in order to be notified of new
 * items discovered.
 *
 * You need to call libvlc_media_discoverer_start() in order to start the
 * discovery.
 *
 * \see libvlc_media_discoverer_media_list
 * \see libvlc_media_discoverer_event_manager
 * \see libvlc_media_discoverer_start
 *
 * \param p_inst libvlc instance
 * \param psz_name service name
 * \return media discover object or NULL in case of error
 * \version LibVLC 3.0.0 or later
 */
LIBVLC_API libvlc_media_discoverer_t *
libvlc_media_discoverer_new( libvlc_instance_t * p_inst,
                             const char * psz_name );

/**
 * Start media discovery.
 *
 * To stop it, call libvlc_media_discoverer_stop() or
 * libvlc_media_discoverer_release() directly.
 *
 * \see libvlc_media_discoverer_stop
 *
 * \param p_mdis media discover object
 * \return -1 in case of error, 0 otherwise
 * \version LibVLC 3.0.0 or later
 */
LIBVLC_API int
libvlc_media_discoverer_start( libvlc_media_discoverer_t * p_mdis );

/**
 * Stop media discovery.
 *
 * \see libvlc_media_discoverer_start
 *
 * \param p_mdis media discover object
 * \version LibVLC 3.0.0 or later
 */
LIBVLC_API void
libvlc_media_discoverer_stop( libvlc_media_discoverer_t * p_mdis );

/**
 * Release media discover object. If the reference count reaches 0, then
 * the object will be released.
 *
 * \param p_mdis media service discover object
 */
LIBVLC_API void   libvlc_media_discoverer_release( libvlc_media_discoverer_t * p_mdis );

/**
 * Get media service discover object its localized name.
 *
 * \param p_mdis media discover object
 * \return localized name
 */
LIBVLC_API char * libvlc_media_discoverer_localized_name( libvlc_media_discoverer_t * p_mdis );

/**
 * Get media service discover media list.
 *
 * \param p_mdis media service discover object
 * \return list of media items
 */
LIBVLC_API libvlc_media_list_t * libvlc_media_discoverer_media_list( libvlc_media_discoverer_t * p_mdis );

/**
 * Get event manager from media service discover object.
 *
 * \param p_mdis media service discover object
 * \return event manager object.
 */
LIBVLC_API libvlc_event_manager_t *
        libvlc_media_discoverer_event_manager( libvlc_media_discoverer_t * p_mdis );

/**
 * Query if media service discover object is running.
 *
 * \param p_mdis media service discover object
 * \return true if running, false if not
 *
 * \libvlc_return_bool
 */
LIBVLC_API int
        libvlc_media_discoverer_is_running( libvlc_media_discoverer_t * p_mdis );

/**@} */

# ifdef __cplusplus
}
# endif

#endif /* <vlc/libvlc.h> */
