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

#ifndef LIBVLC_MEDIA_LIST_H
#define LIBVLC_MEDIA_LIST_H 1

/**
 * \file
 * This file defines libvlc_media_list API
 */

# ifdef __cplusplus
extern "C" {
# endif

/*****************************************************************************
 * Media List
 *****************************************************************************/
/** \defgroup libvlc_media_list libvlc_media_list
 * \ingroup libvlc
 * LibVLC Media List, a media list holds multiple media descriptors
 * @{
 */

typedef struct libvlc_media_list_t libvlc_media_list_t;
typedef struct libvlc_media_list_view_t libvlc_media_list_view_t;

/**
 * Create an empty media list.
 *
 * \param p_libvlc libvlc instance
 * \return empty media list, or NULL on error
 */
VLC_PUBLIC_API libvlc_media_list_t *
    libvlc_media_list_new( libvlc_instance_t * );

/**
 * Release media list created with libvlc_media_list_new().
 *
 * \param p_ml a media list created with libvlc_media_list_new()
 */
VLC_PUBLIC_API void
    libvlc_media_list_release( libvlc_media_list_t * );

/**
 * Retain reference to a media list
 *
 * \param p_ml a media list created with libvlc_media_list_new()
 */
VLC_PUBLIC_API void
    libvlc_media_list_retain( libvlc_media_list_t * );

VLC_DEPRECATED_API void
    libvlc_media_list_add_file_content( libvlc_media_list_t * p_mlist,
                                        const char * psz_uri,
                                        libvlc_exception_t * p_e );

/**
 * Associate media instance with this media list instance.
 * If another media instance was present it will be released.
 * The libvlc_media_list_lock should NOT be held upon entering this function.
 *
 * \param p_ml a media list instance
 * \param p_mi media instance to add
 */
VLC_PUBLIC_API void
libvlc_media_list_set_media( libvlc_media_list_t *p_ml, libvlc_media_t *p_md );

/**
 * Get media instance from this media list instance. This action will increase
 * the refcount on the media instance.
 * The libvlc_media_list_lock should NOT be held upon entering this function.
 *
 * \param p_ml a media list instance
 * \return media instance
 */
VLC_PUBLIC_API libvlc_media_t *
    libvlc_media_list_media( libvlc_media_list_t *p_ml );

/**
 * Add media instance to media list
 * The libvlc_media_list_lock should be held upon entering this function.
 *
 * \param p_ml a media list instance
 * \param p_mi a media instance
 * \param p_e initialized exception object
 */
VLC_PUBLIC_API void
    libvlc_media_list_add_media( libvlc_media_list_t *,
                                            libvlc_media_t *,
                                            libvlc_exception_t * );

/**
 * Insert media instance in media list on a position
 * The libvlc_media_list_lock should be held upon entering this function.
 *
 * \param p_ml a media list instance
 * \param p_mi a media instance
 * \param i_pos position in array where to insert
 * \param p_e initialized exception object
 */
VLC_PUBLIC_API void
    libvlc_media_list_insert_media( libvlc_media_list_t *,
                                               libvlc_media_t *,
                                               int,
                                               libvlc_exception_t * );
/**
 * Remove media instance from media list on a position
 * The libvlc_media_list_lock should be held upon entering this function.
 *
 * \param p_ml a media list instance
 * \param i_pos position in array where to insert
 * \param p_e initialized exception object
 */
VLC_PUBLIC_API void
    libvlc_media_list_remove_index( libvlc_media_list_t *, int,
                                    libvlc_exception_t * );

/**
 * Get count on media list items
 * The libvlc_media_list_lock should be held upon entering this function.
 *
 * \param p_ml a media list instance
 * \return number of items in media list
 */
VLC_PUBLIC_API int
    libvlc_media_list_count( libvlc_media_list_t *p_ml );

/**
 * List media instance in media list at a position
 * The libvlc_media_list_lock should be held upon entering this function.
 *
 * \param p_ml a media list instance
 * \param i_pos position in array where to insert
 * \param p_e initialized exception object
 * \return media instance at position i_pos and libvlc_media_retain() has been called to increase the refcount on this object.
 */
VLC_PUBLIC_API libvlc_media_t *
    libvlc_media_list_item_at_index( libvlc_media_list_t *, int,
                                     libvlc_exception_t * );
/**
 * Find index position of List media instance in media list.
 * Warning: the function will return the first matched position.
 * The libvlc_media_list_lock should be held upon entering this function.
 *
 * \param p_ml a media list instance
 * \param p_mi media list instance
 * \return position of media instance
 */
VLC_PUBLIC_API int
    libvlc_media_list_index_of_item( libvlc_media_list_t *p_ml,
                                     libvlc_media_t *p_mi );

/**
 * This indicates if this media list is read-only from a user point of view
 *
 * \param p_ml media list instance
 * \return 0 on readonly, 1 on readwrite
 */
VLC_PUBLIC_API int
    libvlc_media_list_is_readonly( libvlc_media_list_t * p_mlist );

/**
 * Get lock on media list items
 *
 * \param p_ml a media list instance
 */
VLC_PUBLIC_API void
    libvlc_media_list_lock( libvlc_media_list_t * );

/**
 * Release lock on media list items
 * The libvlc_media_list_lock should be held upon entering this function.
 *
 * \param p_ml a media list instance
 */
VLC_PUBLIC_API void
    libvlc_media_list_unlock( libvlc_media_list_t * );

/**
 * Get a flat media list view of media list items
 *
 * \param p_ml a media list instance
 * \param p_ex an excpetion instance
 * \return flat media list view instance
 */
VLC_PUBLIC_API libvlc_media_list_view_t *
    libvlc_media_list_flat_view( libvlc_media_list_t *,
                                 libvlc_exception_t * );

/**
 * Get a hierarchical media list view of media list items
 *
 * \param p_ml a media list instance
 * \return hierarchical media list view instance
 */
VLC_PUBLIC_API libvlc_media_list_view_t *
    libvlc_media_list_hierarchical_view( libvlc_media_list_t * );

VLC_PUBLIC_API libvlc_media_list_view_t *
    libvlc_media_list_hierarchical_node_view( libvlc_media_list_t * p_ml );

/**
 * Get libvlc_event_manager from this media list instance.
 * The p_event_manager is immutable, so you don't have to hold the lock
 *
 * \param p_ml a media list instance
 * \return libvlc_event_manager
 */
VLC_PUBLIC_API libvlc_event_manager_t *
    libvlc_media_list_event_manager( libvlc_media_list_t *p_ml );

/** @} media_list */

# ifdef __cplusplus
}
# endif

#endif /* _LIBVLC_MEDIA_LIST_H */
