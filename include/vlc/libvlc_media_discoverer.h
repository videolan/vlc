/*****************************************************************************
 * libvlc.h:  libvlc external API
 *****************************************************************************
 * Copyright (C) 1998-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Paul Saman <jpsaman@videolan.org>
 *          Pierre d'Herbemont <pdherbemont@videolan.org>
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

/**
 * \file
 * This file defines libvlc_media_discoverer external API
 */

#ifndef VLC_LIBVLC_MEDIA_DISCOVERER_H
#define VLC_LIBVLC_MEDIA_DISCOVERER_H 1

# ifdef __cplusplus
extern "C" {
# endif

/*****************************************************************************
 * Services/Media Discovery
 *****************************************************************************/
/** \defgroup libvlc_media_discoverer libvlc_media_discoverer
 * \ingroup libvlc
 * LibVLC Media Discoverer
 * @{
 */

typedef struct libvlc_media_discoverer_t libvlc_media_discoverer_t;

/**
 * Discover media service by name.
 *
 * \param p_inst libvlc instance
 * \param psz_name service name
 * \return media discover object or NULL in case of error
 */
VLC_PUBLIC_API libvlc_media_discoverer_t *
libvlc_media_discoverer_new_from_name( libvlc_instance_t * p_inst,
                                       const char * psz_name );

/**
 * Release media discover object. If the reference count reaches 0, then
 * the object will be released.
 *
 * \param p_mdis media service discover object
 */
VLC_PUBLIC_API void   libvlc_media_discoverer_release( libvlc_media_discoverer_t * p_mdis );

/**
 * Get media service discover object its localized name.
 *
 * \param media discover object
 * \return localized name
 */
VLC_PUBLIC_API char * libvlc_media_discoverer_localized_name( libvlc_media_discoverer_t * p_mdis );

/**
 * Get media service discover media list.
 *
 * \param p_mdis media service discover object
 * \return list of media items
 */
VLC_PUBLIC_API libvlc_media_list_t * libvlc_media_discoverer_media_list( libvlc_media_discoverer_t * p_mdis );

/**
 * Get event manager from media service discover object.
 *
 * \param p_mdis media service discover object
 * \return event manager object.
 */
VLC_PUBLIC_API libvlc_event_manager_t *
        libvlc_media_discoverer_event_manager( libvlc_media_discoverer_t * p_mdis );

/**
 * Query if media service discover object is running.
 *
 * \param p_mdis media service discover object
 * \return true if running, false if not
 */
VLC_PUBLIC_API int
        libvlc_media_discoverer_is_running( libvlc_media_discoverer_t * p_mdis );

/**@} */

# ifdef __cplusplus
}
# endif

#endif /* <vlc/libvlc.h> */
