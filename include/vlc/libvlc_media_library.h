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
 * This file defines libvlc_media_library external API
 */

#ifndef VLC_LIBVLC_MEDIA_LIBRARY_H
#define VLC_LIBVLC_MEDIA_LIBRARY_H 1

/*****************************************************************************
 * Media Library
 *****************************************************************************/
/** \defgroup libvlc_media_library libvlc_media_library
 * \ingroup libvlc
 * LibVLC Media Library
 * @{
 */

typedef struct libvlc_media_library_t libvlc_media_library_t;

/**
 * Create an new Media Library object
 *
 * \param p_libvlc_instance the libvlc instance
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API libvlc_media_player_t * libvlc_media_player_new( libvlc_instance_t *, libvlc_exception_t * );
VLC_PUBLIC_API libvlc_media_library_t *
    libvlc_media_library_new( libvlc_instance_t * p_inst,
                              libvlc_exception_t * p_e );

/**
 * Release media library object. This functions decrements the
 * reference count of the media library object. If it reaches 0,
 * then the object will be released.
 *
 * \param p_mlib media library object
 */
VLC_PUBLIC_API void
    libvlc_media_library_release( libvlc_media_library_t * p_mlib );

/**
 * Retain a reference to a media library object. This function will
 * increment the reference counting for this object. Use
 * libvlc_media_library_release() to decrement the reference count.
 *
 * \param p_mlib media library object
 */
VLC_PUBLIC_API void
    libvlc_media_library_retain( libvlc_media_library_t * p_mlib );

/**
 * Load media library.
 *
 * \param p_mlib media library object
 * \param p_e an initialized exception object.
 */
VLC_PUBLIC_API void
    libvlc_media_library_load( libvlc_media_library_t * p_mlib,
                               libvlc_exception_t * p_e );

/**
 * Save media library.
 *
 * \param p_mlib media library object
 * \param p_e an initialized exception object.
 */
VLC_PUBLIC_API void
    libvlc_media_library_save( libvlc_media_library_t * p_mlib,
                               libvlc_exception_t * p_e );

/**
 * Get media library subitems.
 *
 * \param p_mlib media library object
 * \param p_e an initialized exception object.
 * \return media list subitems
 */
VLC_PUBLIC_API libvlc_media_list_t *
    libvlc_media_library_media_list( libvlc_media_library_t * p_mlib,
                                     libvlc_exception_t * p_e );


/** @} */

#endif /* VLC_LIBVLC_MEDIA_LIBRARY_H */
