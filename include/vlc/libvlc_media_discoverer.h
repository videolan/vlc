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

/**
 * Category of a media discoverer
 * \see libvlc_media_discoverer_list_get()
 */
typedef enum libvlc_media_discoverer_category_t {
    /** devices, like portable music player */
    libvlc_media_discoverer_devices,
    /** LAN/WAN services, like Upnp, SMB, or SAP */
    libvlc_media_discoverer_lan,
    /** Podcasts */
    libvlc_media_discoverer_podcasts,
    /** Local directories, like Video, Music or Pictures directories */
    libvlc_media_discoverer_localdirs,
} libvlc_media_discoverer_category_t;

/**
 * Media discoverer description
 * \see libvlc_media_discoverer_list_get()
 */
typedef struct libvlc_media_discoverer_description_t {
    char *psz_name;
    char *psz_longname;
    libvlc_media_discoverer_category_t i_cat;
} libvlc_media_discoverer_description_t;

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
 * Create a media discoverer object by name.
 *
 * After this object is created, you should attach to media_list events in
 * order to be notified of new items discovered.
 *
 * You need to call libvlc_media_discoverer_start() in order to start the
 * discovery.
 *
 * \see libvlc_media_discoverer_media_list
 * \see libvlc_media_discoverer_event_manager
 * \see libvlc_media_discoverer_start
 *
 * \param p_inst libvlc instance
 * \param psz_name service name; use libvlc_media_discoverer_list_get() to get
 * a list of the discoverer names available in this libVLC instance
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
 * libvlc_media_discoverer_list_release() directly.
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
LIBVLC_API void
libvlc_media_discoverer_release( libvlc_media_discoverer_t * p_mdis );

/**
 * Get media service discover media list.
 *
 * \param p_mdis media service discover object
 * \return list of media items
 */
LIBVLC_API libvlc_media_list_t *
libvlc_media_discoverer_media_list( libvlc_media_discoverer_t * p_mdis );

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

/**
 * Get media discoverer services by category
 *
 * \version LibVLC 3.0.0 and later.
 *
 * \param p_inst libvlc instance
 * \param i_cat category of services to fetch
 * \param ppp_services address to store an allocated array of media discoverer
 * services (must be freed with libvlc_media_discoverer_list_release() by
 * the caller) [OUT]
 *
 * \return the number of media discoverer services (0 on error)
 */
LIBVLC_API size_t
libvlc_media_discoverer_list_get( libvlc_instance_t *p_inst,
                                  libvlc_media_discoverer_category_t i_cat,
                                  libvlc_media_discoverer_description_t ***ppp_services );

/**
 * Release an array of media discoverer services
 *
 * \version LibVLC 3.0.0 and later.
 *
 * \see libvlc_media_discoverer_list_get()
 *
 * \param pp_services array to release
 * \param i_count number of elements in the array
 */
LIBVLC_API void
libvlc_media_discoverer_list_release( libvlc_media_discoverer_description_t **pp_services,
                                      size_t i_count );

/**@} */

# ifdef __cplusplus
}
# endif

#endif /* <vlc/libvlc.h> */
