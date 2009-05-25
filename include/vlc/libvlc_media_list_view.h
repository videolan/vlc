/*****************************************************************************
 * libvlc_media_list.h:  libvlc_media_list API
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont
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

#ifndef LIBVLC_MEDIA_LIST_VIEW_H
#define LIBVLC_MEDIA_LIST_VIEW_H 1

/**
 * \file
 * This file defines libvlc_media_list API
 */

# ifdef __cplusplus
extern "C" {
# endif

/*****************************************************************************
 * Media List View
 *****************************************************************************/
/** \defgroup libvlc_media_list_view libvlc_media_list_view
 * \ingroup libvlc
 * LibVLC Media List View, represent a media_list using a different layout
 * @{ */

/**
 * Retain reference to a media list view
 *
 * \param p_mlv a media list view created with libvlc_media_list_view_new()
 */
VLC_PUBLIC_API void
    libvlc_media_list_view_retain( libvlc_media_list_view_t * p_mlv );

/**
 * Release reference to a media list view. If the refcount reaches 0, then
 * the object will be released.
 *
 * \param p_mlv a media list view created with libvlc_media_list_view_new()
 */
VLC_PUBLIC_API void
    libvlc_media_list_view_release( libvlc_media_list_view_t * p_mlv );

/**
 * Get libvlc_event_manager from this media list view instance.
 * The p_event_manager is immutable, so you don't have to hold the lock
 *
 * \param p_mlv a media list view instance
 * \return libvlc_event_manager
 */
VLC_PUBLIC_API libvlc_event_manager_t *
    libvlc_media_list_view_event_manager( libvlc_media_list_view_t * p_mlv );

/**
 * Get count on media list view items
 *
 * \param p_mlv a media list view instance
 * \param p_e initialized exception object
 * \return number of items in media list view
 */
VLC_PUBLIC_API int
    libvlc_media_list_view_count(  libvlc_media_list_view_t * p_mlv,
                                   libvlc_exception_t * p_e );

/**
 * List media instance in media list view at an index position
 *
 * \param p_mlv a media list view instance
 * \param i_index index position in array where to insert
 * \param p_e initialized exception object
 * \return media instance at position i_pos and libvlc_media_retain() has been called to increase the refcount on this object.
 */
VLC_PUBLIC_API libvlc_media_t *
    libvlc_media_list_view_item_at_index(  libvlc_media_list_view_t * p_mlv,
                                           int i_index,
                                           libvlc_exception_t * p_e );

VLC_PUBLIC_API libvlc_media_list_view_t *
    libvlc_media_list_view_children_at_index(  libvlc_media_list_view_t * p_mlv,
                                           int index,
                                           libvlc_exception_t * p_e );

VLC_PUBLIC_API libvlc_media_list_view_t *
    libvlc_media_list_view_children_for_item(  libvlc_media_list_view_t * p_mlv,
                                           libvlc_media_t * p_md,
                                           libvlc_exception_t * p_e );

/**
 * Get index position of media instance in media list view.
 * The function will return the first occurence.
 *
 * \param p_mlv a media list view instance
 * \param p_md media instance
 * \param p_e initialized exception object
 * \return index position in array of p_md
 */
VLC_PUBLIC_API int
    libvlc_media_list_view_index_of_item(  libvlc_media_list_view_t * p_mlv,
                                           libvlc_media_t * p_md,
                                           libvlc_exception_t * p_e );

/**
 * Insert media instance in media list view at index position
 *
 * \param p_mlv a media list view instance
 * \param p_md media instance
 * \param index position in array where to insert
 * \param p_e initialized exception object
 */
VLC_PUBLIC_API void
    libvlc_media_list_view_insert_at_index(  libvlc_media_list_view_t * p_mlv,
                                             libvlc_media_t * p_md,
                                             int index,
                                             libvlc_exception_t * p_e );

/**
 * Remove media instance in media list view from index position
 *
 * \param p_mlv a media list view instance
 * \param index position in array of media instance to remove
 * \param p_e initialized exception object
 */
VLC_PUBLIC_API void
    libvlc_media_list_view_remove_at_index(  libvlc_media_list_view_t * p_mlv,
                                             int index,
                                             libvlc_exception_t * p_e );

VLC_PUBLIC_API void
    libvlc_media_list_view_add_item(  libvlc_media_list_view_t * p_mlv,
                                      libvlc_media_t * p_md,
                                      libvlc_exception_t * p_e );

VLC_PUBLIC_API libvlc_media_list_t *
    libvlc_media_list_view_parent_media_list(  libvlc_media_list_view_t * p_mlv,
                                               libvlc_exception_t * p_e );

/** @} media_list_view */

# ifdef __cplusplus
}
# endif

#endif /* LIBVLC_MEDIA_LIST_VIEW_H */
