/*****************************************************************************
 * libvlc_media_list.h:  libvlc_media_list API
 *****************************************************************************
 * Copyright (C) 1998-2008 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Pierre d'Herbemont
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

#ifndef LIBVLC_MEDIA_LIST_H
#define LIBVLC_MEDIA_LIST_H 1

# ifdef __cplusplus
extern "C" {
# endif

/** \defgroup libvlc_media_list LibVLC media list
 * \ingroup libvlc
 * A LibVLC media list holds multiple @ref libvlc_media_t media descriptors.
 * @{
 * \file
 * LibVLC media list (playlist) external API
 */

typedef struct libvlc_media_list_t libvlc_media_list_t;

/**
 * Create an empty media list.
 *
 * \param p_instance libvlc instance
 * \return empty media list, or NULL on error
 */
LIBVLC_API libvlc_media_list_t *
    libvlc_media_list_new( libvlc_instance_t *p_instance );

/**
 * Release media list created with libvlc_media_list_new().
 *
 * \param p_ml a media list created with libvlc_media_list_new()
 */
LIBVLC_API void
    libvlc_media_list_release( libvlc_media_list_t *p_ml );

/**
 * Retain reference to a media list
 *
 * \param p_ml a media list created with libvlc_media_list_new()
 */
LIBVLC_API void
    libvlc_media_list_retain( libvlc_media_list_t *p_ml );

/**
 * Associate media instance with this media list instance.
 * If another media instance was present it will be released.
 * The libvlc_media_list_lock should NOT be held upon entering this function.
 *
 * \param p_ml a media list instance
 * \param p_md media instance to add
 */
LIBVLC_API void
libvlc_media_list_set_media( libvlc_media_list_t *p_ml, libvlc_media_t *p_md );

/**
 * Get media instance from this media list instance. This action will increase
 * the refcount on the media instance.
 * The libvlc_media_list_lock should NOT be held upon entering this function.
 *
 * \param p_ml a media list instance
 * \return media instance
 */
LIBVLC_API libvlc_media_t *
    libvlc_media_list_media( libvlc_media_list_t *p_ml );

/**
 * Add media instance to media list
 * The libvlc_media_list_lock should be held upon entering this function.
 *
 * \param p_ml a media list instance
 * \param p_md a media instance
 * \return 0 on success, -1 if the media list is read-only
 */
LIBVLC_API int
libvlc_media_list_add_media( libvlc_media_list_t *p_ml, libvlc_media_t *p_md );

/**
 * Insert media instance in media list on a position
 * The libvlc_media_list_lock should be held upon entering this function.
 *
 * \param p_ml a media list instance
 * \param p_md a media instance
 * \param i_pos position in array where to insert
 * \return 0 on success, -1 if the media list is read-only
 */
LIBVLC_API int
libvlc_media_list_insert_media( libvlc_media_list_t *p_ml,
                                libvlc_media_t *p_md, int i_pos );

/**
 * Remove media instance from media list on a position
 * The libvlc_media_list_lock should be held upon entering this function.
 *
 * \param p_ml a media list instance
 * \param i_pos position in array where to insert
 * \return 0 on success, -1 if the list is read-only or the item was not found
 */
LIBVLC_API int
libvlc_media_list_remove_index( libvlc_media_list_t *p_ml, int i_pos );

/**
 * Get count on media list items
 * The libvlc_media_list_lock should be held upon entering this function.
 *
 * \param p_ml a media list instance
 * \return number of items in media list
 */
LIBVLC_API int
    libvlc_media_list_count( libvlc_media_list_t *p_ml );

/**
 * List media instance in media list at a position
 * The libvlc_media_list_lock should be held upon entering this function.
 *
 * \param p_ml a media list instance
 * \param i_pos position in array where to insert
 * \return media instance at position i_pos, or NULL if not found.
 * In case of success, libvlc_media_retain() is called to increase the refcount
 * on the media.
 */
LIBVLC_API libvlc_media_t *
    libvlc_media_list_item_at_index( libvlc_media_list_t *p_ml, int i_pos );
/**
 * Find index position of List media instance in media list.
 * Warning: the function will return the first matched position.
 * The libvlc_media_list_lock should be held upon entering this function.
 *
 * \param p_ml a media list instance
 * \param p_md media instance
 * \return position of media instance or -1 if media not found
 */
LIBVLC_API int
    libvlc_media_list_index_of_item( libvlc_media_list_t *p_ml,
                                     libvlc_media_t *p_md );

/**
 * This indicates if this media list is read-only from a user point of view
 *
 * \param p_ml media list instance
 * \return 1 on readonly, 0 on readwrite
 *
 * \libvlc_return_bool
 */
LIBVLC_API int
    libvlc_media_list_is_readonly( libvlc_media_list_t * p_ml );

/**
 * Get lock on media list items
 *
 * \param p_ml a media list instance
 */
LIBVLC_API void
    libvlc_media_list_lock( libvlc_media_list_t *p_ml );

/**
 * Release lock on media list items
 * The libvlc_media_list_lock should be held upon entering this function.
 *
 * \param p_ml a media list instance
 */
LIBVLC_API void
    libvlc_media_list_unlock( libvlc_media_list_t *p_ml );

/**
 * Get libvlc_event_manager from this media list instance.
 * The p_event_manager is immutable, so you don't have to hold the lock
 *
 * \param p_ml a media list instance
 * \return libvlc_event_manager
 */
LIBVLC_API libvlc_event_manager_t *
    libvlc_media_list_event_manager( libvlc_media_list_t *p_ml );

/** @} media_list */

# ifdef __cplusplus
}
# endif

#endif /* _LIBVLC_MEDIA_LIST_H */
