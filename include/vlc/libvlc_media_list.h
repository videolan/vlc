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

/**
 * Create an empty media list.
 *
 * \param p_libvlc libvlc instance
 * \param p_e an initialized exception pointer
 * \return empty media list
 */
VLC_PUBLIC_API libvlc_media_list_t *
    libvlc_media_list_new( libvlc_instance_t *, libvlc_exception_t * );

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
 * \param p_e initialized exception object
 */
VLC_PUBLIC_API void
    libvlc_media_list_set_media( libvlc_media_list_t *,
                                            libvlc_media_t *,
                                            libvlc_exception_t *);

/**
 * Get media instance from this media list instance. This action will increase
 * the refcount on the media instance.
 * The libvlc_media_list_lock should NOT be held upon entering this function.
 *
 * \param p_ml a media list instance
 * \param p_e initialized exception object
 * \return media instance
 */
VLC_PUBLIC_API libvlc_media_t *
    libvlc_media_list_media( libvlc_media_list_t *,
                                        libvlc_exception_t *);

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
 * \param p_e initialized exception object
 * \return number of items in media list
 */
VLC_PUBLIC_API int
    libvlc_media_list_count( libvlc_media_list_t * p_mlist,
                             libvlc_exception_t * p_e );

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
 * \param p_e initialized exception object
 * \return position of media instance
 */
VLC_PUBLIC_API int
    libvlc_media_list_index_of_item( libvlc_media_list_t *,
                                     libvlc_media_t *,
                                     libvlc_exception_t * );

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
 * \param p_ex an excpetion instance
 * \return hierarchical media list view instance
 */
VLC_PUBLIC_API libvlc_media_list_view_t *
    libvlc_media_list_hierarchical_view( libvlc_media_list_t *,
                                         libvlc_exception_t * );

VLC_PUBLIC_API libvlc_media_list_view_t *
    libvlc_media_list_hierarchical_node_view( libvlc_media_list_t *,
                                              libvlc_exception_t * );

/**
 * Get libvlc_event_manager from this media list instance.
 * The p_event_manager is immutable, so you don't have to hold the lock
 *
 * \param p_ml a media list instance
 * \param p_ex an excpetion instance
 * \return libvlc_event_manager
 */
VLC_PUBLIC_API libvlc_event_manager_t *
    libvlc_media_list_event_manager( libvlc_media_list_t *,
                                     libvlc_exception_t * );

/*****************************************************************************
 * Media List View
 *****************************************************************************/
/** \defgroup libvlc_media_list_view libvlc_media_list_view
 * \ingroup libvlc_media_list
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

/*****************************************************************************
 * Media List Player
 *****************************************************************************/
/** \defgroup libvlc_media_list_player libvlc_media_list_player
 * \ingroup libvlc_media_list_player
 * LibVLC Media List Player, play a media_list. You can see that as a media
 * instance subclass
 * @{
 */

/**
 * Create new media_list_player.
 *
 * \param p_instance libvlc instance
 * \param p_e initialized exception instance
 * \return media list player instance
 */
VLC_PUBLIC_API libvlc_media_list_player_t *
    libvlc_media_list_player_new( libvlc_instance_t * p_instance,
                                  libvlc_exception_t * p_e );

/**
 * Release media_list_player.
 *
 * \param p_mlp media list player instance
 */
VLC_PUBLIC_API void
    libvlc_media_list_player_release( libvlc_media_list_player_t * p_mlp );

/**
 * Replace media player in media_list_player with this instance.
 *
 * \param p_mlp media list player instance
 * \param p_mi media player instance
 * \param p_e initialized exception instance
 */
VLC_PUBLIC_API void
    libvlc_media_list_player_set_media_player(
                                     libvlc_media_list_player_t * p_mlp,
                                     libvlc_media_player_t * p_mi,
                                     libvlc_exception_t * p_e );

VLC_PUBLIC_API void
    libvlc_media_list_player_set_media_list(
                                     libvlc_media_list_player_t * p_mlp,
                                     libvlc_media_list_t * p_mlist,
                                     libvlc_exception_t * p_e );

/**
 * Play media list
 *
 * \param p_mlp media list player instance
 * \param p_e initialized exception instance
 */
VLC_PUBLIC_API void
    libvlc_media_list_player_play( libvlc_media_list_player_t * p_mlp,
                                   libvlc_exception_t * p_e );

/**
 * Pause media list
 *
 * \param p_mlp media list player instance
 * \param p_e initialized exception instance
 */
VLC_PUBLIC_API void
    libvlc_media_list_player_pause( libvlc_media_list_player_t * p_mlp,
                                   libvlc_exception_t * p_e );

/**
 * Is media list playing?
 *
 * \param p_mlp media list player instance
 * \param p_e initialized exception instance
 * \return true for playing and false for not playing
 */
VLC_PUBLIC_API int
    libvlc_media_list_player_is_playing( libvlc_media_list_player_t * p_mlp,
                                         libvlc_exception_t * p_e );

/**
 * Get current libvlc_state of media list player
 *
 * \param p_mlp media list player instance
 * \param p_e initialized exception instance
 * \return libvlc_state_t for media list player
 */
VLC_PUBLIC_API libvlc_state_t
    libvlc_media_list_player_get_state( libvlc_media_list_player_t * p_mlp,
                                        libvlc_exception_t * p_e );

/**
 * Play media list item at position index
 *
 * \param p_mlp media list player instance
 * \param i_index index in media list to play
 * \param p_e initialized exception instance
 */
VLC_PUBLIC_API void
    libvlc_media_list_player_play_item_at_index(
                                   libvlc_media_list_player_t * p_mlp,
                                   int i_index,
                                   libvlc_exception_t * p_e );

VLC_PUBLIC_API void
    libvlc_media_list_player_play_item(
                                   libvlc_media_list_player_t * p_mlp,
                                   libvlc_media_t * p_md,
                                   libvlc_exception_t * p_e );

/**
 * Stop playing media list
 *
 * \param p_mlp media list player instance
 * \param p_e initialized exception instance
 */
VLC_PUBLIC_API void
    libvlc_media_list_player_stop( libvlc_media_list_player_t * p_mlp,
                                   libvlc_exception_t * p_e );

/**
 * Play next item from media list
 *
 * \param p_mlp media list player instance
 * \param p_e initialized exception instance
 */
VLC_PUBLIC_API void
    libvlc_media_list_player_next( libvlc_media_list_player_t * p_mlp,
                                   libvlc_exception_t * p_e );

/* NOTE: shouldn't there also be a libvlc_media_list_player_prev() */

/** @} media_list_player */

/** @} media_list */

# ifdef __cplusplus
}
# endif

#endif /* _LIBVLC_MEDIA_LIST_H */
