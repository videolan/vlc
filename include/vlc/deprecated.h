/*****************************************************************************
 * deprecated.h:  libvlc deprecated API
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Paul Saman <jpsaman@videolan.org>
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

#ifndef LIBVLC_DEPRECATED_H
#define LIBVLC_DEPRECATED_H 1

/**
 * \file
 * This file defines libvlc depreceated API
 */

# ifdef __cplusplus
extern "C" {
# endif

/**
 * Set the default video output's parent.
 *
 * This setting will be used as default for all video outputs.
 *
 * \param p_instance libvlc instance
 * \param drawable the new parent window (Drawable on X11, CGrafPort on MacOSX, HWND on Win32)
 * \param p_e an initialized exception pointer
 * @deprecated Use libvlc_media_player_set_drawable
 */
VLC_PUBLIC_API void libvlc_video_set_parent( libvlc_instance_t *, libvlc_drawable_t, libvlc_exception_t * );

/**
 * Set the default video output parent.
 *
 * This setting will be used as default for all video outputs.
 *
 * \param p_instance libvlc instance
 * \param drawable the new parent window (Drawable on X11, CGrafPort on MacOSX, HWND on Win32)
 * \param p_e an initialized exception pointer
 * @deprecated Use libvlc_media_player_get_drawable
 */
VLC_PUBLIC_API libvlc_drawable_t libvlc_video_get_parent( libvlc_instance_t *, libvlc_exception_t * );

/*
 * This function shall not be used at all. It may lead to crash and race condition.
 */
VLC_DEPRECATED_API int libvlc_video_destroy( libvlc_media_player_t *, libvlc_exception_t *);

/*****************************************************************************
 * Playlist (Deprecated)
 *****************************************************************************/
/** \defgroup libvlc_playlist libvlc_playlist (Deprecated)
 * \ingroup libvlc
 * LibVLC Playlist handling (Deprecated)
 * @deprecated Use media_list
 * @{
 */

/**
 * Set the playlist's loop attribute. If set, the playlist runs continuously
 * and wraps around when it reaches the end.
 *
 * \param p_instance the playlist instance
 * \param loop the loop attribute. 1 sets looping, 0 disables it
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_playlist_loop( libvlc_instance_t* , int,
                                          libvlc_exception_t * );

/**
 * Start playing.
 *
 * Additionnal playlist item options can be specified for addition to the
 * item before it is played.
 *
 * \param p_instance the playlist instance
 * \param i_id the item to play. If this is a negative number, the next
 *        item will be selected. Otherwise, the item with the given ID will be
 *        played
 * \param i_options the number of options to add to the item
 * \param ppsz_options the options to add to the item
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_playlist_play( libvlc_instance_t*, int, int,
                                          char **, libvlc_exception_t * );

/**
 * Toggle the playlist's pause status.
 *
 * If the playlist was running, it is paused. If it was paused, it is resumed.
 *
 * \param p_instance the playlist instance to pause
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_playlist_pause( libvlc_instance_t *,
                                           libvlc_exception_t * );

/**
 * Checks whether the playlist is running
 *
 * \param p_instance the playlist instance
 * \param p_e an initialized exception pointer
 * \return 0 if the playlist is stopped or paused, 1 if it is running
 */
VLC_DEPRECATED_API int libvlc_playlist_isplaying( libvlc_instance_t *,
                                              libvlc_exception_t * );

/**
 * Get the number of items in the playlist
 *
 * \param p_instance the playlist instance
 * \param p_e an initialized exception pointer
 * \return the number of items
 */
VLC_DEPRECATED_API int libvlc_playlist_items_count( libvlc_instance_t *,
                                                libvlc_exception_t * );

VLC_DEPRECATED_API int libvlc_playlist_get_current_index( libvlc_instance_t *,
                                                 libvlc_exception_t *);
/**
 * Lock the playlist.
 *
 * \param p_instance the playlist instance
 */
VLC_DEPRECATED_API void libvlc_playlist_lock( libvlc_instance_t * );

/**
 * Unlock the playlist.
 *
 * \param p_instance the playlist instance
 */
VLC_DEPRECATED_API void libvlc_playlist_unlock( libvlc_instance_t * );

/**
 * Stop playing.
 *
 * \param p_instance the playlist instance to stop
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_playlist_stop( libvlc_instance_t *,
                                          libvlc_exception_t * );

/**
 * Go to the next playlist item. If the playlist was stopped, playback
 * is started.
 *
 * \param p_instance the playlist instance
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_playlist_next( libvlc_instance_t *,
                                          libvlc_exception_t * );

/**
 * Go to the previous playlist item. If the playlist was stopped, playback
 * is started.
 *
 * \param p_instance the playlist instance
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_playlist_prev( libvlc_instance_t *,
                                          libvlc_exception_t * );

/**
 * Empty a playlist. All items in the playlist are removed.
 *
 * \param p_instance the playlist instance
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_playlist_clear( libvlc_instance_t *,
                                           libvlc_exception_t * );

/**
 * Append an item to the playlist. The item is added at the end. If more
 * advanced options are required, \see libvlc_playlist_add_extended instead.
 *
 * \param p_instance the playlist instance
 * \param psz_uri the URI to open, using VLC format
 * \param psz_name a name that you might want to give or NULL
 * \param p_e an initialized exception pointer
 * \return the identifier of the new item
 */
VLC_DEPRECATED_API int libvlc_playlist_add( libvlc_instance_t *, const char *,
                                        const char *, libvlc_exception_t * );

/**
 * Append an item to the playlist. The item is added at the end, with
 * additional input options.
 *
 * \param p_instance the playlist instance
 * \param psz_uri the URI to open, using VLC format
 * \param psz_name a name that you might want to give or NULL
 * \param i_options the number of options to add
 * \param ppsz_options strings representing the options to add
 * \param p_e an initialized exception pointer
 * \return the identifier of the new item
 */
VLC_DEPRECATED_API int libvlc_playlist_add_extended( libvlc_instance_t *, const char *,
                                                 const char *, int, const char **,
                                                 libvlc_exception_t * );

/**
 * Delete the playlist item with the given ID.
 *
 * \param p_instance the playlist instance
 * \param i_id the id to remove
 * \param p_e an initialized exception pointer
 * \return 0 in case of success, a non-zero value otherwise
 */
VLC_DEPRECATED_API int libvlc_playlist_delete_item( libvlc_instance_t *, int,
                                                libvlc_exception_t * );

/** Get the input that is currently being played by the playlist.
 *
 * \param p_instance the playlist instance to use
 * \param p_e an initialized exception pointern
 * \return a media instance object
 */
VLC_DEPRECATED_API libvlc_media_player_t * libvlc_playlist_get_media_player(
                                libvlc_instance_t *, libvlc_exception_t * );

/** @}*/

# ifdef __cplusplus
}
# endif

#endif /* _LIBVLC_DEPRECATED_H */
