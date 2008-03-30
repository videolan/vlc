/*****************************************************************************
 * libvlc_media_list.h:  libvlc_media_list API
 *****************************************************************************
 * Copyright (C) 1998-2005 the VideoLAN team
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

#ifndef _LIBVLC_MEDIA_LIST_H
#define _LIBVLC_MEDIA_LIST_H 1

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
 * \param p_libvlc the event manager
 * \param i_event_type the desired event to which we want to unregister
 * \param f_callback the function to call when i_event_type occurs
 * \param p_e an initialized exception pointer
 */

VLC_PUBLIC_API libvlc_media_list_t *
    libvlc_media_list_new( libvlc_instance_t *, libvlc_exception_t * );

VLC_PUBLIC_API void
    libvlc_media_list_release( libvlc_media_list_t * );

VLC_PUBLIC_API void
    libvlc_media_list_retain( libvlc_media_list_t * );

VLC_DEPRECATED_API void
    libvlc_media_list_add_file_content( libvlc_media_list_t * p_mlist,
                                        const char * psz_uri,
                                        libvlc_exception_t * p_e );

VLC_PUBLIC_API void
    libvlc_media_list_set_media( libvlc_media_list_t *,
                                            libvlc_media_t *,
                                            libvlc_exception_t *);

VLC_PUBLIC_API libvlc_media_t *
    libvlc_media_list_media( libvlc_media_list_t *,
                                        libvlc_exception_t *);

VLC_PUBLIC_API void
    libvlc_media_list_add_media( libvlc_media_list_t *,
                                            libvlc_media_t *,
                                            libvlc_exception_t * );
VLC_PUBLIC_API void
    libvlc_media_list_insert_media( libvlc_media_list_t *,
                                               libvlc_media_t *,
                                               int,
                                               libvlc_exception_t * );
VLC_PUBLIC_API void
    libvlc_media_list_remove_index( libvlc_media_list_t *, int,
                                    libvlc_exception_t * );

VLC_PUBLIC_API int
    libvlc_media_list_count( libvlc_media_list_t * p_mlist,
                             libvlc_exception_t * p_e );

VLC_PUBLIC_API libvlc_media_t *
    libvlc_media_list_item_at_index( libvlc_media_list_t *, int,
                                     libvlc_exception_t * );
VLC_PUBLIC_API int
    libvlc_media_list_index_of_item( libvlc_media_list_t *,
                                     libvlc_media_t *,
                                     libvlc_exception_t * );

/* This indicates if this media list is read-only from a user point of view */
VLC_PUBLIC_API int
    libvlc_media_list_is_readonly( libvlc_media_list_t * p_mlist );

VLC_PUBLIC_API void
    libvlc_media_list_lock( libvlc_media_list_t * );
VLC_PUBLIC_API void
    libvlc_media_list_unlock( libvlc_media_list_t * );

VLC_PUBLIC_API libvlc_media_list_view_t *
    libvlc_media_list_flat_view( libvlc_media_list_t *,
                                 libvlc_exception_t * );

VLC_PUBLIC_API libvlc_media_list_view_t *
    libvlc_media_list_hierarchical_view( libvlc_media_list_t *,
                                         libvlc_exception_t * );

VLC_PUBLIC_API libvlc_media_list_view_t *
    libvlc_media_list_hierarchical_node_view( libvlc_media_list_t *,
                                              libvlc_exception_t * );

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

VLC_PUBLIC_API void
    libvlc_media_list_view_retain( libvlc_media_list_view_t * p_mlv );

VLC_PUBLIC_API void
    libvlc_media_list_view_release( libvlc_media_list_view_t * p_mlv );

VLC_PUBLIC_API libvlc_event_manager_t *
    libvlc_media_list_view_event_manager(  libvlc_media_list_view_t * p_mlv );

VLC_PUBLIC_API int
    libvlc_media_list_view_count(  libvlc_media_list_view_t * p_mlv,
                                   libvlc_exception_t * p_e );

VLC_PUBLIC_API libvlc_media_t *
    libvlc_media_list_view_item_at_index(  libvlc_media_list_view_t * p_mlv,
                                           int index,
                                           libvlc_exception_t * p_e );

VLC_PUBLIC_API libvlc_media_list_view_t *
    libvlc_media_list_view_children_at_index(  libvlc_media_list_view_t * p_mlv,
                                           int index,
                                           libvlc_exception_t * p_e );

VLC_PUBLIC_API libvlc_media_list_view_t *
    libvlc_media_list_view_children_for_item(  libvlc_media_list_view_t * p_mlv,
                                           libvlc_media_t * p_md,
                                           libvlc_exception_t * p_e );


VLC_PUBLIC_API int
    libvlc_media_list_view_index_of_item(  libvlc_media_list_view_t * p_mlv,
                                           libvlc_media_t * p_md,
                                           libvlc_exception_t * p_e );

VLC_PUBLIC_API void
    libvlc_media_list_view_insert_at_index(  libvlc_media_list_view_t * p_mlv,
                                             libvlc_media_t * p_md,
                                             int index,
                                             libvlc_exception_t * p_e );

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
 * \ingroup libvlc
 * LibVLC Media List Player, play a media_list. You can see that as a media
 * instance subclass
 * @{
 */
VLC_PUBLIC_API libvlc_media_list_player_t *
    libvlc_media_list_player_new( libvlc_instance_t * p_instance,
                                  libvlc_exception_t * p_e );
VLC_PUBLIC_API void
    libvlc_media_list_player_release( libvlc_media_list_player_t * p_mlp );

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

VLC_PUBLIC_API void
    libvlc_media_list_player_play( libvlc_media_list_player_t * p_mlp,
                                   libvlc_exception_t * p_e );

VLC_PUBLIC_API void
    libvlc_media_list_player_pause( libvlc_media_list_player_t * p_mlp,
                                   libvlc_exception_t * p_e );

VLC_PUBLIC_API int
    libvlc_media_list_player_is_playing( libvlc_media_list_player_t * p_mlp,
                                         libvlc_exception_t * p_e );

VLC_PUBLIC_API libvlc_state_t
    libvlc_media_list_player_get_state( libvlc_media_list_player_t * p_mlp,
                                        libvlc_exception_t * p_e );

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

VLC_PUBLIC_API void
    libvlc_media_list_player_stop( libvlc_media_list_player_t * p_mlp,
                                   libvlc_exception_t * p_e );

VLC_PUBLIC_API void
    libvlc_media_list_player_next( libvlc_media_list_player_t * p_mlp,
                                   libvlc_exception_t * p_e );

/** @} media_list_player */

/** @} media_list */

# ifdef __cplusplus
}
# endif

#endif /* _LIBVLC_MEDIA_LIST_H */
