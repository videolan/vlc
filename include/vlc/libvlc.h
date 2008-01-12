/*****************************************************************************
 * libvlc.h:  libvlc_* new external API
 *****************************************************************************
 * Copyright (C) 1998-2005 the VideoLAN team
 * $Id: vlc.h 13701 2005-12-12 17:58:56Z zorglub $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Paul Saman <jpsaman _at_ m2x _dot_ nl>
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
 * \defgroup libvlc Libvlc
 * This is libvlc, the base library of the VLC program.
 *
 * @{
 */


#ifndef _LIBVLC_H
#define _LIBVLC_H 1

#include <vlc/vlc.h>
#include <vlc/libvlc_structures.h>

# ifdef __cplusplus
extern "C" {
# endif

/*****************************************************************************
 * Exception handling
 *****************************************************************************/
/** defgroup libvlc_exception Exceptions
 * \ingroup libvlc
 * LibVLC Exceptions handling
 * @{
 */

/**
 * Initialize an exception structure. This can be called several times to reuse
 * an exception structure.
 * \param p_exception the exception to initialize
 */
VLC_PUBLIC_API void libvlc_exception_init( libvlc_exception_t *p_exception );

/**
 * Has an exception been raised ?
 * \param p_exception the exception to query
 * \return 0 if no exception raised, 1 else
 */
VLC_PUBLIC_API int
libvlc_exception_raised( const libvlc_exception_t *p_exception );

/**
 * Raise an exception
 * \param p_exception the exception to raise
 * \param psz_message the exception message
 */
VLC_PUBLIC_API void
libvlc_exception_raise( libvlc_exception_t *p_exception,
                        const char *psz_format, ... );

/**
 * Clear an exception object so it can be reused.
 * The exception object must be initialized
 * \param p_exception the exception to clear
 */
VLC_PUBLIC_API void libvlc_exception_clear( libvlc_exception_t * );

/**
 * Get exception message
 * \param p_exception the exception to query
 * \return the exception message or NULL if not applicable (exception not raised
 * for example)
 */
VLC_PUBLIC_API const char *
libvlc_exception_get_message( const libvlc_exception_t *p_exception );

/**@} */

/*****************************************************************************
 * Core handling
 *****************************************************************************/

/** defgroup libvlc_core Core
 * \ingroup libvlc
 * LibVLC Core
 * @{
 */

/**
 * Create an initialized libvlc instance.
 * \param argc the number of arguments
 * \param argv command-line-type arguments
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API libvlc_instance_t *
libvlc_new( int , const char *const *, libvlc_exception_t *);

/**
 * Returns a libvlc instance identifier for legacy APIs. Use of this
 * function is discouraged, you should convert your program to use the
 * new API.
 * \param p_instance the instance
 */
VLC_PUBLIC_API int libvlc_get_vlc_id( libvlc_instance_t *p_instance );

/**
 * Decrements the reference count of a libvlc instance, and destroys it
 * if it reaches zero.
 * \param p_instance the instance to destroy
 */
VLC_PUBLIC_API void libvlc_release( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Increments the reference count of a libvlc instance.
 * The reference count is initially one when libvlc_new() returns.
 */
VLC_PUBLIC_API void libvlc_retain( libvlc_instance_t * );

/** @}*/

/*****************************************************************************
 * Media descriptor
 *****************************************************************************/
/** defgroup libvlc_media_descriptor Media Descriptor
 * \ingroup libvlc
 * LibVLC Media Descriptor
 * @{
 */

/**
 * Create a media descriptor with the given mrl.
 * \param p_instance the instance
 * \param psz_mrl the mrl to read
 */
VLC_PUBLIC_API libvlc_media_descriptor_t * libvlc_media_descriptor_new(
                                   libvlc_instance_t *p_instance,
                                   const char * psz_mrl,
                                   libvlc_exception_t *p_e );

/**
 * Create a media descriptor as an empty node with the passed name.
 * \param p_instance the instance
 * \param psz_name the name of the node
 */
VLC_PUBLIC_API libvlc_media_descriptor_t * libvlc_media_descriptor_new_as_node(
                                   libvlc_instance_t *p_instance,
                                   const char * psz_name,
                                   libvlc_exception_t *p_e );

/**
 * Add an option to the media descriptor,
 * This option will be used to determine how the media_instance will
 * read the media_descriptor. This allow to use VLC advanced
 * reading/streaming options in a per-media basis.
 *
 * The options are detailled in vlc --long-help, for instance "--sout-all"
 * \param p_instance the instance
 * \param psz_mrl the mrl to read
 */
VLC_PUBLIC_API void libvlc_media_descriptor_add_option(
                                   libvlc_media_descriptor_t * p_md,
                                   const char * ppsz_options,
                                   libvlc_exception_t * p_e );

VLC_PUBLIC_API void libvlc_media_descriptor_retain(
                                   libvlc_media_descriptor_t *p_meta_desc );

VLC_PUBLIC_API void libvlc_media_descriptor_release(
                                   libvlc_media_descriptor_t *p_meta_desc );

VLC_PUBLIC_API char * libvlc_media_descriptor_get_mrl( libvlc_media_descriptor_t * p_md,
                                                       libvlc_exception_t * p_e );

VLC_PUBLIC_API libvlc_media_descriptor_t * libvlc_media_descriptor_duplicate( libvlc_media_descriptor_t * );

/**
 * Read the meta of the media descriptor.
 * \param p_meta_desc the media descriptor to read
 * \param p_meta_desc the meta to read
 */
VLC_PUBLIC_API char * libvlc_media_descriptor_get_meta(
                                   libvlc_media_descriptor_t *p_meta_desc,
                                   libvlc_meta_t e_meta,
                                   libvlc_exception_t *p_e );

VLC_PUBLIC_API libvlc_state_t libvlc_media_descriptor_get_state(
                                   libvlc_media_descriptor_t *p_meta_desc,
                                   libvlc_exception_t *p_e );

/* Tags */
VLC_PUBLIC_API void libvlc_media_descriptor_add_tag( libvlc_media_descriptor_t *p_md,
                                                     const char * key,
                                                     const libvlc_tag_t tag,
                                                     libvlc_exception_t *p_e );

VLC_PUBLIC_API void libvlc_media_descriptor_remove_tag( libvlc_media_descriptor_t *p_md,
                                                         const char * key,
                                                         const libvlc_tag_t tag,
                                                         libvlc_exception_t *p_e );

VLC_PUBLIC_API int
    libvlc_media_descriptor_tags_count_for_key( libvlc_media_descriptor_t *p_md,
                                                const char * key,
                                                libvlc_exception_t *p_e );

VLC_PUBLIC_API libvlc_tag_t
    libvlc_media_descriptor_tag_at_index_for_key( libvlc_media_descriptor_t *p_md,
                                                  int i,
                                                  const char * key,
                                                  libvlc_exception_t *p_e );

VLC_PUBLIC_API libvlc_media_list_t *
    libvlc_media_descriptor_subitems( libvlc_media_descriptor_t *p_md,
                                      libvlc_exception_t *p_e );

VLC_PUBLIC_API libvlc_event_manager_t *
    libvlc_media_descriptor_event_manager( libvlc_media_descriptor_t * p_md,
                                           libvlc_exception_t * p_e );

VLC_PUBLIC_API libvlc_time_t
   libvlc_media_descriptor_get_duration( libvlc_media_descriptor_t * p_md,
                                         libvlc_exception_t * p_e );

VLC_PUBLIC_API vlc_bool_t
   libvlc_media_descriptor_is_preparsed( libvlc_media_descriptor_t * p_md,
                                         libvlc_exception_t * p_e );

VLC_PUBLIC_API void
    libvlc_media_descriptor_set_user_data( libvlc_media_descriptor_t * p_md,
                                           void * p_new_user_data,
                                           libvlc_exception_t * p_e);
VLC_PUBLIC_API void *
    libvlc_media_descriptor_get_user_data( libvlc_media_descriptor_t * p_md,
                                           libvlc_exception_t * p_e);

/** @}*/

/*****************************************************************************
 * Playlist
 *****************************************************************************/
/** defgroup libvlc_playlist Playlist
 * \ingroup libvlc
 * LibVLC Playlist handling
 * @{
 */

/**
 * Set loop variable
 */
VLC_PUBLIC_API void libvlc_playlist_loop( libvlc_instance_t* , vlc_bool_t,
                                          libvlc_exception_t * );

/**
 * Start playing. You can give some additionnal playlist item options
 * that will be added to the item before playing it.
 * \param p_instance the instance
 * \param i_id the item to play. If this is a negative number, the next
 * item will be selected. Else, the item with the given ID will be played
 * \param i_options the number of options to add to the item
 * \param ppsz_options the options to add to the item
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_playlist_play( libvlc_instance_t*, int, int, char **,
                                          libvlc_exception_t * );

/**
 * Pause a running playlist, resume if it was stopped
 * \param p_instance the instance to pause
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_playlist_pause( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Checks if the playlist is running
 * \param p_instance the instance
 * \param p_exception an initialized exception
 * \return 0 if the playlist is stopped or paused, 1 if it is running
 */
VLC_PUBLIC_API int libvlc_playlist_isplaying( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Get the number of items in the playlist
 * \param p_instance the instance
 * \param p_exception an initialized exception
 * \return the number of items
 */
VLC_PUBLIC_API int libvlc_playlist_items_count( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Lock the playlist instance
 * \param p_instance the instance
 */
VLC_PUBLIC_API void libvlc_playlist_lock( libvlc_instance_t * );

/**
 * Unlock the playlist instance
 * \param p_instance the instance
 */
VLC_PUBLIC_API void libvlc_playlist_unlock( libvlc_instance_t * );

/**
 * Stop playing
 * \param p_instance the instance to stop
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_playlist_stop( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Go to next playlist item (starts playback if it was stopped)
 * \param p_instance the instance to use
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_playlist_next( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Go to previous playlist item (starts playback if it was stopped)
 * \param p_instance the instance to use
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_playlist_prev( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Remove all playlist items
 * \param p_instance the instance
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_playlist_clear( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Add an item at the end of the playlist
 * If you need more advanced options, \see libvlc_playlist_add_extended
 * \param p_instance the instance
 * \param psz_uri the URI to open, using VLC format
 * \param psz_name a name that you might want to give or NULL
 * \return the identifier of the new item
 */
VLC_PUBLIC_API int libvlc_playlist_add( libvlc_instance_t *, const char *, const char *,
                                        libvlc_exception_t * );

/**
 * Add an item at the end of the playlist, with additional input options
 * \param p_instance the instance
 * \param psz_uri the URI to open, using VLC format
 * \param psz_name a name that you might want to give or NULL
 * \param i_options the number of options to add
 * \param ppsz_options strings representing the options to add
 * \param p_exception an initialized exception
 * \return the identifier of the new item
 */
VLC_PUBLIC_API int libvlc_playlist_add_extended( libvlc_instance_t *, const char *,
                                                 const char *, int, const char **,
                                                 libvlc_exception_t * );

/**
 * Delete the playlist item with the given ID.
 * \param p_instance the instance
 * \param i_id the id to remove
 * \param p_exception an initialized exception
 * \return
 */
VLC_PUBLIC_API int libvlc_playlist_delete_item( libvlc_instance_t *, int,
                                                libvlc_exception_t * );

/** Get the input that is currently being played by the playlist
 * \param p_instance the instance to use
 * \param p_exception an initialized excecption
 * \return an input object
 */
VLC_PUBLIC_API libvlc_media_instance_t * libvlc_playlist_get_media_instance(
                                libvlc_instance_t *, libvlc_exception_t * );

VLC_PUBLIC_API vlc_bool_t libvlc_media_instance_is_seekable(
                                 libvlc_media_instance_t *p_mi,
                                 libvlc_exception_t *p_e );

VLC_PUBLIC_API vlc_bool_t libvlc_media_instance_can_pause(
                                 libvlc_media_instance_t *p_mi,
                                 libvlc_exception_t *p_e );

/** @}*/

/*****************************************************************************
 * Media Instance
 *****************************************************************************/
/** defgroup libvlc_media_instance Media Instance
 * \ingroup libvlc
 * LibVLC Media Instance
 * @{
 */

/** Create an empty Media Instance object
 * \param p_libvlc_instance the libvlc instance in which the Media Instance
 * should be (not used for now).
 */
VLC_PUBLIC_API libvlc_media_instance_t * libvlc_media_instance_new( libvlc_instance_t *, libvlc_exception_t * );

/** Create a Media Instance object from a Media Descriptor
 * \param p_md the media descriptor. Afterwards the p_md can safely be
 * destroyed.
 */
VLC_PUBLIC_API libvlc_media_instance_t * libvlc_media_instance_new_from_media_descriptor( libvlc_media_descriptor_t *, libvlc_exception_t * );

/** Release a media_instance after use
 * \param p_mi the Media Instance to free
 */
VLC_PUBLIC_API void libvlc_media_instance_release( libvlc_media_instance_t * );
VLC_PUBLIC_API void libvlc_media_instance_retain( libvlc_media_instance_t * );

/** Set the media descriptor that will be used by the media_instance. If any,
 * previous md will be released.
 * \param p_mi the Media Instance
 * \param p_md the Media Descriptor. Afterwards the p_md can safely be
 * destroyed.
 */
VLC_PUBLIC_API void libvlc_media_instance_set_media_descriptor( libvlc_media_instance_t *, libvlc_media_descriptor_t *, libvlc_exception_t * );

/** Get the media descriptor used by the media_instance (if any). A copy of
 * the md is returned. NULL is returned if no media instance is associated.
 * \param p_mi the Media Instance
 */
VLC_PUBLIC_API libvlc_media_descriptor_t * libvlc_media_instance_get_media_descriptor( libvlc_media_instance_t *, libvlc_exception_t * );

/** Get the Event Manager from which the media instance send event.
 * \param p_mi the Media Instance
 */
VLC_PUBLIC_API libvlc_event_manager_t * libvlc_media_instance_event_manager ( libvlc_media_instance_t *, libvlc_exception_t * );

VLC_PUBLIC_API void libvlc_media_instance_play ( libvlc_media_instance_t *, libvlc_exception_t * );
VLC_PUBLIC_API void libvlc_media_instance_pause ( libvlc_media_instance_t *, libvlc_exception_t * );
VLC_PUBLIC_API void libvlc_media_instance_stop ( libvlc_media_instance_t *, libvlc_exception_t * );

VLC_PUBLIC_API void libvlc_media_instance_set_drawable ( libvlc_media_instance_t *, libvlc_drawable_t, libvlc_exception_t * );
VLC_PUBLIC_API libvlc_drawable_t
                    libvlc_media_instance_get_drawable ( libvlc_media_instance_t *, libvlc_exception_t * );

/** \bug This might go away ... to be replaced by a broader system */
VLC_PUBLIC_API libvlc_time_t libvlc_media_instance_get_length     ( libvlc_media_instance_t *, libvlc_exception_t *);
VLC_PUBLIC_API libvlc_time_t libvlc_media_instance_get_time       ( libvlc_media_instance_t *, libvlc_exception_t *);
VLC_PUBLIC_API void          libvlc_media_instance_set_time       ( libvlc_media_instance_t *, libvlc_time_t, libvlc_exception_t *);
VLC_PUBLIC_API float         libvlc_media_instance_get_position   ( libvlc_media_instance_t *, libvlc_exception_t *);
VLC_PUBLIC_API void          libvlc_media_instance_set_position   ( libvlc_media_instance_t *, float, libvlc_exception_t *);
VLC_PUBLIC_API void          libvlc_media_instance_set_chapter    ( libvlc_media_instance_t *, int, libvlc_exception_t *);
VLC_PUBLIC_API int           libvlc_media_instance_get_chapter    (libvlc_media_instance_t *, libvlc_exception_t *);
VLC_PUBLIC_API int           libvlc_media_instance_get_chapter_count( libvlc_media_instance_t *, libvlc_exception_t *);
VLC_PUBLIC_API vlc_bool_t    libvlc_media_instance_will_play      ( libvlc_media_instance_t *, libvlc_exception_t *);
VLC_PUBLIC_API float         libvlc_media_instance_get_rate       ( libvlc_media_instance_t *, libvlc_exception_t *);
VLC_PUBLIC_API void          libvlc_media_instance_set_rate       ( libvlc_media_instance_t *, float, libvlc_exception_t *);
VLC_PUBLIC_API libvlc_state_t libvlc_media_instance_get_state   ( libvlc_media_instance_t *, libvlc_exception_t *);

/**
 * Does this input have a video output ?
 * \param p_input the input
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API vlc_bool_t  libvlc_media_instance_has_vout( libvlc_media_instance_t *, libvlc_exception_t *);
VLC_PUBLIC_API float       libvlc_media_instance_get_fps( libvlc_media_instance_t *, libvlc_exception_t *);


/** @} */

/*****************************************************************************
 * Tag Query
 *****************************************************************************/
/** defgroup libvlc_tag_query Tag Query
 * \ingroup libvlc
 * LibVLC Tag query
 * @{
 */
VLC_PUBLIC_API libvlc_tag_query_t *
    libvlc_tag_query_new( libvlc_instance_t *, libvlc_exception_t * );

VLC_PUBLIC_API void
    libvlc_tag_query_release( libvlc_tag_query_t * );

VLC_PUBLIC_API void
    libvlc_tag_query_retain( libvlc_tag_query_t * );

VLC_PUBLIC_API void
    libvlc_tag_query_set_match_tag_and_key( libvlc_tag_query_t * p_q,
                                            libvlc_tag_t tag,
                                            char * psz_tag_key,
                                            libvlc_exception_t * );

VLC_PUBLIC_API vlc_bool_t
    libvlc_tag_query_match( libvlc_tag_query_t *, libvlc_media_descriptor_t *,
                            libvlc_exception_t * );

/** @} */

/*****************************************************************************
 * Media List
 *****************************************************************************/
/** defgroup libvlc_media_list MediaList
 * \ingroup libvlc
 * LibVLC Media List
 * @{
 */
VLC_PUBLIC_API libvlc_media_list_t *
    libvlc_media_list_new( libvlc_instance_t *, libvlc_exception_t * );

VLC_PUBLIC_API void
    libvlc_media_list_release( libvlc_media_list_t * );

VLC_PUBLIC_API void
    libvlc_media_list_retain( libvlc_media_list_t * );

VLC_PUBLIC_API void
    libvlc_media_list_add_file_content( libvlc_media_list_t * p_mlist,
                                        const char * psz_uri,
                                        libvlc_exception_t * p_e );

VLC_PUBLIC_API void
    libvlc_media_list_set_media_descriptor( libvlc_media_list_t *,
                                            libvlc_media_descriptor_t *,
                                            libvlc_exception_t *);

VLC_PUBLIC_API libvlc_media_descriptor_t *
    libvlc_media_list_media_descriptor( libvlc_media_list_t *,
                                        libvlc_exception_t *);

VLC_PUBLIC_API void
    libvlc_media_list_add_media_descriptor( libvlc_media_list_t *,
                                            libvlc_media_descriptor_t *,
                                            libvlc_exception_t * );
VLC_PUBLIC_API void
    libvlc_media_list_insert_media_descriptor( libvlc_media_list_t *,
                                               libvlc_media_descriptor_t *,
                                               int,
                                               libvlc_exception_t * );
VLC_PUBLIC_API void
    libvlc_media_list_remove_index( libvlc_media_list_t *, int,
                                    libvlc_exception_t * );

VLC_PUBLIC_API int
    libvlc_media_list_count( libvlc_media_list_t * p_mlist,
                             libvlc_exception_t * p_e );

VLC_PUBLIC_API libvlc_media_descriptor_t *
    libvlc_media_list_item_at_index( libvlc_media_list_t *, int,
                                     libvlc_exception_t * );
VLC_PUBLIC_API int
    libvlc_media_list_index_of_item( libvlc_media_list_t *,
                                     libvlc_media_descriptor_t *,
                                     libvlc_exception_t * );

/* This indicates if this media list is read-only from a user point of view */
VLC_PUBLIC_API vlc_bool_t
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
/** @} */


/*****************************************************************************
 * Media List View
 *****************************************************************************/
/** defgroup libvlc_media_list_view MediaListView
 * \ingroup libvlc
 * LibVLC Media List View
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

VLC_PUBLIC_API libvlc_media_descriptor_t *
    libvlc_media_list_view_item_at_index(  libvlc_media_list_view_t * p_mlv,
                                           int index,
                                           libvlc_exception_t * p_e );

VLC_PUBLIC_API libvlc_media_list_view_t *
    libvlc_media_list_view_children_at_index(  libvlc_media_list_view_t * p_mlv,
                                           int index,
                                           libvlc_exception_t * p_e );


VLC_PUBLIC_API int
    libvlc_media_list_view_index_of_item(  libvlc_media_list_view_t * p_mlv,
                                           libvlc_media_descriptor_t * p_md,
                                           libvlc_exception_t * p_e );

VLC_PUBLIC_API void
    libvlc_media_list_view_insert_at_index(  libvlc_media_list_view_t * p_mlv,
                                             libvlc_media_descriptor_t * p_md,
                                             int index,
                                             libvlc_exception_t * p_e );

VLC_PUBLIC_API void
    libvlc_media_list_view_remove_at_index(  libvlc_media_list_view_t * p_mlv,
                                             int index,
                                             libvlc_exception_t * p_e );

VLC_PUBLIC_API void
    libvlc_media_list_view_add_item(  libvlc_media_list_view_t * p_mlv,
                                      libvlc_media_descriptor_t * p_md,
                                      libvlc_exception_t * p_e );

VLC_PUBLIC_API libvlc_media_list_t *
    libvlc_media_list_view_parent_media_list(  libvlc_media_list_view_t * p_mlv,
                                               libvlc_exception_t * p_e );

/** @} */

/*****************************************************************************
 * Dynamic Media List (Deprecated)
 *****************************************************************************/
/** defgroup libvlc_media_list MediaList
 * \ingroup libvlc
 * LibVLC Media List
 * @{ */

VLC_PUBLIC_API libvlc_dynamic_media_list_t *
    libvlc_dynamic_media_list_new(  libvlc_media_list_t * p_mlist,
                                    libvlc_tag_query_t * p_query,
                                    libvlc_tag_t tag,
                                    libvlc_exception_t * p_e );
VLC_PUBLIC_API void
    libvlc_dynamic_media_list_release( libvlc_dynamic_media_list_t * p_dmlist );

VLC_PUBLIC_API void
    libvlc_dynamic_media_list_retain( libvlc_dynamic_media_list_t * p_dmlist );

libvlc_media_list_t *
    libvlc_dynamic_media_list_media_list( libvlc_dynamic_media_list_t * p_dmlist,
                                          libvlc_exception_t * p_e );

/** @} */

/*****************************************************************************
 * Media Library
 *****************************************************************************/
/** defgroup libvlc_media_library Media Library
 * \ingroup libvlc
 * LibVLC Media Library
 * @{
 */
VLC_PUBLIC_API libvlc_media_library_t *
    libvlc_media_library_new( libvlc_instance_t * p_inst,
                              libvlc_exception_t * p_e );
VLC_PUBLIC_API void
    libvlc_media_library_release( libvlc_media_library_t * p_mlib );
VLC_PUBLIC_API void
    libvlc_media_library_retain( libvlc_media_library_t * p_mlib );


VLC_PUBLIC_API void
    libvlc_media_library_load( libvlc_media_library_t * p_mlib,
                               libvlc_exception_t * p_e );

VLC_PUBLIC_API void
    libvlc_media_library_save( libvlc_media_library_t * p_mlib,
                               libvlc_exception_t * p_e );

VLC_PUBLIC_API libvlc_media_list_t *
    libvlc_media_library_media_list( libvlc_media_library_t * p_mlib,
                                     libvlc_exception_t * p_e );


/** @} */

/*****************************************************************************
 * Media List Player
 *****************************************************************************/
/** defgroup libvlc_media_list_player MediaListPlayer
 * \ingroup libvlc
 * LibVLC Media List Player
 * @{
 */
VLC_PUBLIC_API libvlc_media_list_player_t *
    libvlc_media_list_player_new( libvlc_instance_t * p_instance,
                                  libvlc_exception_t * p_e );
VLC_PUBLIC_API void
    libvlc_media_list_player_release( libvlc_media_list_player_t * p_mlp );

VLC_PUBLIC_API void
    libvlc_media_list_player_set_media_instance(
                                     libvlc_media_list_player_t * p_mlp,
                                     libvlc_media_instance_t * p_mi,
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

VLC_PUBLIC_API vlc_bool_t
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
                                   libvlc_media_descriptor_t * p_md,
                                   libvlc_exception_t * p_e );

VLC_PUBLIC_API void
    libvlc_media_list_player_stop( libvlc_media_list_player_t * p_mlp,
                                   libvlc_exception_t * p_e );
VLC_PUBLIC_API void
    libvlc_media_list_player_next( libvlc_media_list_player_t * p_mlp,
                                   libvlc_exception_t * p_e );

/** @} */

/** defgroup libvlc_video Video
 * \ingroup libvlc
 * LibVLC Video handling
 * @{
 */

/**
 * Toggle fullscreen status on video output
 * \param p_input the input
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_toggle_fullscreen( libvlc_media_instance_t *, libvlc_exception_t * );

/**
 * Enable or disable fullscreen on a video output
 * \param p_input the input
 * \param b_fullscreen boolean for fullscreen status
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_set_fullscreen( libvlc_media_instance_t *, int, libvlc_exception_t * );

/**
 * Get current fullscreen status
 * \param p_input the input
 * \param p_exception an initialized exception
 * \return the fullscreen status (boolean)
 */
VLC_PUBLIC_API int libvlc_get_fullscreen( libvlc_media_instance_t *, libvlc_exception_t * );

/**
 * Get current video height
 * \param p_input the input
 * \param p_exception an initialized exception
 * \return the video height
 */
VLC_PUBLIC_API int libvlc_video_get_height( libvlc_media_instance_t *, libvlc_exception_t * );

/**
 * Get current video width
 * \param p_input the input
 * \param p_exception an initialized exception
 * \return the video width
 */
VLC_PUBLIC_API int libvlc_video_get_width( libvlc_media_instance_t *, libvlc_exception_t * );

/**
 * Get current video aspect ratio
 * \param p_input the input
 * \param p_exception an initialized exception
 * \return the video aspect ratio
 */
VLC_PUBLIC_API char *libvlc_video_get_aspect_ratio( libvlc_media_instance_t *, libvlc_exception_t * );

/**
 * Set new video aspect ratio
 * \param p_input the input
 * \param psz_aspect new video aspect-ratio
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_video_set_aspect_ratio( libvlc_media_instance_t *, char *, libvlc_exception_t * );

/**
 * Get current video subtitle
 * \param p_input the input
 * \param p_exception an initialized exception
 * \return the video subtitle selected
 */
VLC_PUBLIC_API int libvlc_video_get_spu( libvlc_media_instance_t *, libvlc_exception_t * );

/**
 * Set new video subtitle
 * \param p_input the input
 * \param i_spu new video subtitle to select
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_video_set_spu( libvlc_media_instance_t *, int , libvlc_exception_t * );

/**
 * Get current crop filter geometry
 * \param p_input the input
 * \param p_exception an initialized exception
 * \return the crop filter geometry
 */
VLC_PUBLIC_API char *libvlc_video_get_crop_geometry( libvlc_media_instance_t *, libvlc_exception_t * );

/**
 * Set new crop filter geometry
 * \param p_input the input
 * \param psz_geometry new crop filter geometry
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_video_set_crop_geometry( libvlc_media_instance_t *, char *, libvlc_exception_t * );

/**
 * Toggle teletext transparent status on video output
 * \param p_input the input
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_toggle_teletext( libvlc_media_instance_t *, libvlc_exception_t * );

/**
 * Get current teletext page requested.
 * \param p_input the input
 * \param p_exception an initialized exception
 * \return the current teletext page requested.
 */
VLC_PUBLIC_API int libvlc_video_get_teletext( libvlc_media_instance_t *, libvlc_exception_t * );

/**
 * Set new teletext page to retrieve
 * \param p_input the input
 * \param i_page teletex page number requested
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_video_set_teletext( libvlc_media_instance_t *, int, libvlc_exception_t * );

/**
 * Take a snapshot of the current video window
 * If i_width AND i_height is 0, original size is used
 * if i_width XOR i_height is 0, original aspect-ratio is preserved
 * \param p_input the input
 * \param psz_filepath the path where to save the screenshot to
 * \param i_width the snapshot's width
 * \param i_height the snapshot's height
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_video_take_snapshot( libvlc_media_instance_t *, char *,unsigned int, unsigned int, libvlc_exception_t * );

VLC_PUBLIC_API int libvlc_video_destroy( libvlc_media_instance_t *, libvlc_exception_t *);

/**
 * Resize the current video output window
 * \param p_instance libvlc instance
 * \param width new width for video output window
 * \param height new height for video output window
 * \param p_exception an initialized exception
 * \return the success status (boolean)
 */
VLC_PUBLIC_API void libvlc_video_resize( libvlc_media_instance_t *, int, int, libvlc_exception_t *);

/**
 * change the parent for the current the video output
 * \param p_instance libvlc instance
 * \param drawable the new parent window (Drawable on X11, CGrafPort on MacOSX, HWND on Win32)
 * \param p_exception an initialized exception
 * \return the success status (boolean)
 */
VLC_PUBLIC_API int libvlc_video_reparent( libvlc_media_instance_t *, libvlc_drawable_t, libvlc_exception_t * );

/**
 * Tell windowless video output to redraw rectangular area (MacOS X only)
 * \param p_instance libvlc instance
 * \param area coordinates within video drawable
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_video_redraw_rectangle( libvlc_media_instance_t *, const libvlc_rectangle_t *, libvlc_exception_t * );

/**
 * Set the default video output parent
 *  this settings will be used as default for all video outputs
 * \param p_instance libvlc instance
 * \param drawable the new parent window (Drawable on X11, CGrafPort on MacOSX, HWND on Win32)
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_video_set_parent( libvlc_instance_t *, libvlc_drawable_t, libvlc_exception_t * );

/**
 * Set the default video output parent
 *  this settings will be used as default for all video outputs
 * \param p_instance libvlc instance
 * \param drawable the new parent window (Drawable on X11, CGrafPort on MacOSX, HWND on Win32)
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API libvlc_drawable_t libvlc_video_get_parent( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Set the default video output size
 *  this settings will be used as default for all video outputs
 * \param p_instance libvlc instance
 * \param width new width for video drawable
 * \param height new height for video drawable
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_video_set_size( libvlc_instance_t *, int, int, libvlc_exception_t * );

/**
 * Set the default video output viewport for a windowless video output (MacOS X only)
 *  this settings will be used as default for all video outputs
 * \param p_instance libvlc instance
 * \param view coordinates within video drawable
 * \param clip coordinates within video drawable
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_video_set_viewport( libvlc_instance_t *, const libvlc_rectangle_t *, const libvlc_rectangle_t *, libvlc_exception_t * );

/** @} */

/** defgroup libvlc_audio Audio
 * \ingroup libvlc
 * LibVLC Audio handling
 * @{
 */

/**
 * Toggle mute status
 * \param p_instance libvlc instance
 * \param p_exception an initialized exception
 * \return void
 */
VLC_PUBLIC_API void libvlc_audio_toggle_mute( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Get current mute status
 * \param p_instance libvlc instance
 * \param p_exception an initialized exception
 * \return the mute status (boolean)
 */
VLC_PUBLIC_API vlc_bool_t libvlc_audio_get_mute( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Set mute status
 * \param p_instance libvlc instance
 * \param status If status is VLC_TRUE then mute, otherwise unmute
 * \param p_exception an initialized exception
 * \return void
 */
VLC_PUBLIC_API void libvlc_audio_set_mute( libvlc_instance_t *, vlc_bool_t , libvlc_exception_t * );

/**
 * Get current audio level
 * \param p_instance libvlc instance
 * \param p_exception an initialized exception
 * \return the audio level (int)
 */
VLC_PUBLIC_API int libvlc_audio_get_volume( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Set current audio level
 * \param p_instance libvlc instance
 * \param i_volume the volume (int)
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_audio_set_volume( libvlc_instance_t *, int, libvlc_exception_t *);

/**
 * Get number of available audio tracks
 * \param p_mi media instance
 * \param p_e an initialized exception
 * \return the number of available audio tracks (int)
 */
VLC_PUBLIC_API int libvlc_audio_get_track_count( libvlc_media_instance_t *,  libvlc_exception_t * );

/**
+  * Get current audio track
+  * \param p_input input instance
+  * \param p_exception an initialized exception
+  * \return the audio track (int)
+  */
VLC_PUBLIC_API int libvlc_audio_get_track( libvlc_media_instance_t *, libvlc_exception_t * );

/**
 * Set current audio track
 * \param p_input input instance
 * \param i_track the track (int)
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_audio_set_track( libvlc_media_instance_t *, int, libvlc_exception_t * );

/**
 * Get current audio channel
 * \param p_instance input instance
 * \param p_exception an initialized exception
 * \return the audio channel (int)
 */
VLC_PUBLIC_API int libvlc_audio_get_channel( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Set current audio channel
 * \param p_instance input instance
 * \param i_channel the audio channel (int)
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_audio_set_channel( libvlc_instance_t *, int, libvlc_exception_t * );

/** @} */

/*****************************************************************************
 * Services/Media Discovery
 *****************************************************************************/
/** defgroup libvlc_media_discoverer Media Discoverer
 * \ingroup libvlc
 * LibVLC Media Discoverer
 * @{
 */

VLC_PUBLIC_API libvlc_media_discoverer_t *
libvlc_media_discoverer_new_from_name( libvlc_instance_t * p_inst,
                                       const char * psz_name,
                                       libvlc_exception_t * p_e );
VLC_PUBLIC_API void   libvlc_media_discoverer_release( libvlc_media_discoverer_t * p_mdis );
VLC_PUBLIC_API char * libvlc_media_discoverer_localized_name( libvlc_media_discoverer_t * p_mdis );

VLC_PUBLIC_API libvlc_media_list_t * libvlc_media_discoverer_media_list( libvlc_media_discoverer_t * p_mdis );

VLC_PUBLIC_API libvlc_event_manager_t *
        libvlc_media_discoverer_event_manager( libvlc_media_discoverer_t * p_mdis );

VLC_PUBLIC_API vlc_bool_t
        libvlc_media_discoverer_is_running( libvlc_media_discoverer_t * p_mdis );

/**@} */

/*****************************************************************************
 * VLM
 *****************************************************************************/
/** defgroup libvlc_vlm VLM
 * \ingroup libvlc
 * LibVLC VLM
 * @{
 */

/**
 * Add a broadcast, with one input
 * \param p_instance the instance
 * \param psz_name the name of the new broadcast
 * \param psz_input the input MRL
 * \param psz_output the output MRL (the parameter to the "sout" variable)
 * \param i_options number of additional options
 * \param ppsz_options additional options
 * \param b_enabled boolean for enabling the new broadcast
 * \param b_loop Should this broadcast be played in loop ?
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_vlm_add_broadcast( libvlc_instance_t *, char *, char *, char* ,
                                              int, char **, int, int, libvlc_exception_t * );

/**
 * Delete a media (vod or broadcast)
 * \param p_instance the instance
 * \param psz_name the media to delete
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_vlm_del_media( libvlc_instance_t *, char *, libvlc_exception_t * );

/**
 * Enable or disable a media (vod or broadcast)
 * \param p_instance the instance
 * \param psz_name the media to work on
 * \param b_enabled the new status
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_vlm_set_enabled( libvlc_instance_t *, char *, int,
                                            libvlc_exception_t *);

/**
 * Set the output for a media
 * \param p_instance the instance
 * \param psz_name the media to work on
 * \param psz_output the output MRL (the parameter to the "sout" variable)
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_vlm_set_output( libvlc_instance_t *, char *, char*,
                                           libvlc_exception_t *);

/**
 * Set a media's input MRL. This will delete all existing inputs and
 * add the specified one.
 * \param p_instance the instance
 * \param psz_name the media to work on
 * \param psz_input the input MRL
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_vlm_set_input( libvlc_instance_t *, char *, char*,
                                          libvlc_exception_t *);

/**
 * Add a media's input MRL. This will add the specified one.
 * \param p_instance the instance
 * \param psz_name the media to work on
 * \param psz_input the input MRL
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_vlm_add_input( libvlc_instance_t *, char *, char *,
                                          libvlc_exception_t *p_exception );
/**
 * Set output for a media
 * \param p_instance the instance
 * \param psz_name the media to work on
 * \param b_loop the new status
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_vlm_set_loop( libvlc_instance_t *, char *, int,
                                         libvlc_exception_t *);

/**
 * Edit the parameters of a media. This will delete all existing inputs and
 * add the specified one.
 * \param p_instance the instance
 * \param psz_name the name of the new broadcast
 * \param psz_input the input MRL
 * \param psz_output the output MRL (the parameter to the "sout" variable)
 * \param i_options number of additional options
 * \param ppsz_options additional options
 * \param b_enabled boolean for enabling the new broadcast
 * \param b_loop Should this broadcast be played in loop ?
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_vlm_change_media( libvlc_instance_t *, char *, char *, char* ,
                                             int, char **, int, int, libvlc_exception_t * );

/**
 * Plays the named broadcast.
 * \param p_instance the instance
 * \param psz_name the name of the broadcast
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_vlm_play_media ( libvlc_instance_t *, char *, libvlc_exception_t * );

/**
 * Stops the named broadcast.
 * \param p_instance the instance
 * \param psz_name the name of the broadcast
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_vlm_stop_media ( libvlc_instance_t *, char *, libvlc_exception_t * );

/**
 * Pauses the named broadcast.
 * \param p_instance the instance
 * \param psz_name the name of the broadcast
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_vlm_pause_media( libvlc_instance_t *, char *, libvlc_exception_t * );

/**
 * Seeks in the named broadcast.
 * \param p_instance the instance
 * \param psz_name the name of the broadcast
 * \param f_percentage the percentage to seek to
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API void libvlc_vlm_seek_media( libvlc_instance_t *, char *,
                                           float, libvlc_exception_t * );

/**
 * Return information of the named broadcast.
 * \param p_instance the instance
 * \param psz_name the name of the broadcast
 * \param p_exception an initialized exception
 */
VLC_PUBLIC_API char* libvlc_vlm_show_media( libvlc_instance_t *, char *, libvlc_exception_t * );

#define LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( attr, returnType, getType, default)\
returnType libvlc_vlm_get_media_## attr( libvlc_instance_t *, \
                        char *, int , libvlc_exception_t * );

VLC_PUBLIC_API LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( position, float, Float, -1);
VLC_PUBLIC_API LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( time, int, Integer, -1);
VLC_PUBLIC_API LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( length, int, Integer, -1);
VLC_PUBLIC_API LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( rate, int, Integer, -1);
VLC_PUBLIC_API LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( title, int, Integer, 0);
VLC_PUBLIC_API LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( chapter, int, Integer, 0);
VLC_PUBLIC_API LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( seekable, int, Bool, 0);

#undef LIBVLC_VLM_GET_MEDIA_ATTRIBUTE

/** @} */

/*****************************************************************************
 * Message log handling
 *****************************************************************************/

/** defgroup libvlc_log Log
 * \ingroup libvlc
 * LibVLC Message Logging
 * @{
 */

/**
 * Returns the VLC messaging verbosity level
 * \param p_instance libvlc instance
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API unsigned libvlc_get_log_verbosity( const libvlc_instance_t *p_instance,
                                                  libvlc_exception_t *p_e );

/**
 * Set the VLC messaging verbosity level
 * \param p_log libvlc log instance
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_set_log_verbosity( libvlc_instance_t *p_instance, unsigned level,
                                              libvlc_exception_t *p_e );

/**
 * Open an instance to VLC message log
 * \param p_instance libvlc instance
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API libvlc_log_t *libvlc_log_open( libvlc_instance_t *, libvlc_exception_t *);

/**
 * Close an instance of VLC message log
 * \param p_log libvlc log instance
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_log_close( libvlc_log_t *, libvlc_exception_t *);

/**
 * Returns the number of messages in log
 * \param p_log libvlc log instance
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API unsigned libvlc_log_count( const libvlc_log_t *, libvlc_exception_t *);

/**
 * Clear all messages in log
 *  the log should be cleared on a regular basis to avoid clogging
 * \param p_log libvlc log instance
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_log_clear( libvlc_log_t *, libvlc_exception_t *);

/**
 * Allocate and returns a new iterator to messages in log
 * \param p_log libvlc log instance
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API libvlc_log_iterator_t *libvlc_log_get_iterator( const libvlc_log_t *, libvlc_exception_t *);

/**
 * Releases a previoulsy allocated iterator
 * \param p_log libvlc log iterator
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_log_iterator_free( libvlc_log_iterator_t *p_iter, libvlc_exception_t *p_e );

/**
 * Returns whether log iterator has more messages
 * \param p_log libvlc log iterator
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API int libvlc_log_iterator_has_next( const libvlc_log_iterator_t *p_iter, libvlc_exception_t *p_e );

/**
 * Returns next log message
 *   the content of message must not be freed
 * \param p_log libvlc log iterator
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API libvlc_log_message_t *libvlc_log_iterator_next( libvlc_log_iterator_t *p_iter,
                                                               struct libvlc_log_message_t *buffer,
                                                               libvlc_exception_t *p_e );

/** @} */

/*****************************************************************************
 * Event handling
 *****************************************************************************/

/** defgroup libvlc_callbacks Callbacks
 * \ingroup libvlc
 * LibVLC Events
 * @{
 */

/**
 * Register for an event notification
 * \param p_event_manager the event manager to which you want to attach to
 * Generally it is obtained by vlc_my_object_event_manager() where my_object
 * Is the object you want to listen to.
 * \param i_event_type the desired event to which we want to listen
 * \param f_callback the function to call when i_event_type occurs
 * \param user_data user provided data to carry with the event
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_event_attach( libvlc_event_manager_t *p_event_manager,
                                         libvlc_event_type_t i_event_type,
                                         libvlc_callback_t f_callback,
                                         void *user_data,
                                         libvlc_exception_t *p_e );

/**
 * Unregister an event notification
 * \param p_event_manager the event manager
 * \param i_event_type the desired event to which we want to unregister
 * \param f_callback the function to call when i_event_type occurs
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_event_detach( libvlc_event_manager_t *p_event_manager,
                                         libvlc_event_type_t i_event_type,
                                         libvlc_callback_t f_callback,
                                         void *p_user_data,
                                         libvlc_exception_t *p_e );

/**
 * Get an event type name
 * \param i_event_type the desired event
 */
VLC_PUBLIC_API const char * libvlc_event_type_name( libvlc_event_type_t event_type );

/** @} */


# ifdef __cplusplus
}
# endif

#endif /* <vlc/libvlc.h> */
