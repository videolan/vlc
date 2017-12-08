/*****************************************************************************
 * libvlc_media_list_player.h:  libvlc_media_list API
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

#ifndef LIBVLC_MEDIA_LIST_PLAYER_H
#define LIBVLC_MEDIA_LIST_PLAYER_H 1

# ifdef __cplusplus
extern "C" {
# endif

/** \defgroup libvlc_media_list_player LibVLC media list player
 * \ingroup libvlc
 * The LibVLC media list player plays a @ref libvlc_media_list_t list of media,
 * in a certain order.
 * This is required to especially support playlist files.
 * The normal @ref libvlc_media_player_t LibVLC media player can only play a
 * single media, and does not handle playlist files properly.
 * @{
 * \file
 * LibVLC media list player external API
 */

typedef struct libvlc_media_list_player_t libvlc_media_list_player_t;

/**
 *  Defines playback modes for playlist.
 */
typedef enum libvlc_playback_mode_t
{
    libvlc_playback_mode_default,
    libvlc_playback_mode_loop,
    libvlc_playback_mode_repeat
} libvlc_playback_mode_t;

/**
 * Create new media_list_player.
 *
 * \param p_instance libvlc instance
 * \return media list player instance or NULL on error
 */
LIBVLC_API libvlc_media_list_player_t *
    libvlc_media_list_player_new( libvlc_instance_t * p_instance );

/**
 * Release a media_list_player after use
 * Decrement the reference count of a media player object. If the
 * reference count is 0, then libvlc_media_list_player_release() will
 * release the media player object. If the media player object
 * has been released, then it should not be used again.
 *
 * \param p_mlp media list player instance
 */
LIBVLC_API void
    libvlc_media_list_player_release( libvlc_media_list_player_t * p_mlp );

/**
 * Retain a reference to a media player list object. Use
 * libvlc_media_list_player_release() to decrement reference count.
 *
 * \param p_mlp media player list object
 */
LIBVLC_API void
    libvlc_media_list_player_retain( libvlc_media_list_player_t *p_mlp );

/**
 * Return the event manager of this media_list_player.
 *
 * \param p_mlp media list player instance
 * \return the event manager
 */
LIBVLC_API libvlc_event_manager_t *
    libvlc_media_list_player_event_manager(libvlc_media_list_player_t * p_mlp);

/**
 * Replace media player in media_list_player with this instance.
 *
 * \param p_mlp media list player instance
 * \param p_mi media player instance
 */
LIBVLC_API void
    libvlc_media_list_player_set_media_player(
                                     libvlc_media_list_player_t * p_mlp,
                                     libvlc_media_player_t * p_mi );

/**
 * Get media player of the media_list_player instance.
 *
 * \param p_mlp media list player instance
 * \return media player instance
 * \note the caller is responsible for releasing the returned instance
 */
LIBVLC_API libvlc_media_player_t *
    libvlc_media_list_player_get_media_player(libvlc_media_list_player_t * p_mlp);

/**
 * Set the media list associated with the player
 *
 * \param p_mlp media list player instance
 * \param p_mlist list of media
 */
LIBVLC_API void
    libvlc_media_list_player_set_media_list(
                                     libvlc_media_list_player_t * p_mlp,
                                     libvlc_media_list_t * p_mlist );

/**
 * Play media list
 *
 * \param p_mlp media list player instance
 */
LIBVLC_API
void libvlc_media_list_player_play(libvlc_media_list_player_t * p_mlp);

/**
 * Toggle pause (or resume) media list
 *
 * \param p_mlp media list player instance
 */
LIBVLC_API
void libvlc_media_list_player_pause(libvlc_media_list_player_t * p_mlp);

/**
 * Pause or resume media list
 *
 * \param p_mlp media list player instance
 * \param do_pause play/resume if zero, pause if non-zero
 * \version LibVLC 3.0.0 or later
 */
LIBVLC_API
void libvlc_media_list_player_set_pause(libvlc_media_list_player_t * p_mlp,
                                        int do_pause);

/**
 * Is media list playing?
 *
 * \param p_mlp media list player instance
 * \return true for playing and false for not playing
 *
 * \libvlc_return_bool
 */
LIBVLC_API int
    libvlc_media_list_player_is_playing( libvlc_media_list_player_t * p_mlp );

/**
 * Get current libvlc_state of media list player
 *
 * \param p_mlp media list player instance
 * \return libvlc_state_t for media list player
 */
LIBVLC_API libvlc_state_t
    libvlc_media_list_player_get_state( libvlc_media_list_player_t * p_mlp );

/**
 * Play media list item at position index
 *
 * \param p_mlp media list player instance
 * \param i_index index in media list to play
 * \return 0 upon success -1 if the item wasn't found
 */
LIBVLC_API
int libvlc_media_list_player_play_item_at_index(libvlc_media_list_player_t * p_mlp,
                                                int i_index);

/**
 * Play the given media item
 *
 * \param p_mlp media list player instance
 * \param p_md the media instance
 * \return 0 upon success, -1 if the media is not part of the media list
 */
LIBVLC_API
int libvlc_media_list_player_play_item(libvlc_media_list_player_t * p_mlp,
                                       libvlc_media_t * p_md);

/**
 * Stop playing media list
 *
 * \param p_mlp media list player instance
 */
LIBVLC_API void
    libvlc_media_list_player_stop( libvlc_media_list_player_t * p_mlp);

/**
 * Play next item from media list
 *
 * \param p_mlp media list player instance
 * \return 0 upon success -1 if there is no next item
 */
LIBVLC_API
int libvlc_media_list_player_next(libvlc_media_list_player_t * p_mlp);

/**
 * Play previous item from media list
 *
 * \param p_mlp media list player instance
 * \return 0 upon success -1 if there is no previous item
 */
LIBVLC_API
int libvlc_media_list_player_previous(libvlc_media_list_player_t * p_mlp);



/**
 * Sets the playback mode for the playlist
 *
 * \param p_mlp media list player instance
 * \param e_mode playback mode specification
 */
LIBVLC_API
void libvlc_media_list_player_set_playback_mode(libvlc_media_list_player_t * p_mlp,
                                                libvlc_playback_mode_t e_mode );

/** @} media_list_player */

# ifdef __cplusplus
}
# endif

#endif /* LIBVLC_MEDIA_LIST_PLAYER_H */
