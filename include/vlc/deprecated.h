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

/**
 * This is the legacy representation of a platform-specific drawable. Because
 * it cannot accomodate a pointer on most 64-bits platforms, it should not be
 * used anymore.
 */
typedef int libvlc_drawable_t;

# ifdef __cplusplus
extern "C" {
# endif

/**
 * Set the drawable where the media player should render its video output.
 *
 * On Windows 32-bits, a window handle (HWND) is expected.
 * On Windows 64-bits, this function will always fail.
 *
 * On OSX 32-bits, a CGrafPort is expected.
 * On OSX 64-bits, this function will always fail.
 *
 * On other platforms, an existing X11 window ID is expected. See
 * libvlc_media_player_set_xid() for details.
 *
 * \param p_mi the Media Player
 * \param drawable the libvlc_drawable_t where the media player
 *        should render its video
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_media_player_set_drawable ( libvlc_media_player_t *, libvlc_drawable_t, libvlc_exception_t * );

/**
 * Get the drawable where the media player should render its video output
 *
 * \param p_mi the Media Player
 * \param p_e an initialized exception pointer
 * \return the libvlc_drawable_t where the media player
 *         should render its video
 */
VLC_DEPRECATED_API libvlc_drawable_t
                    libvlc_media_player_get_drawable ( libvlc_media_player_t *, libvlc_exception_t * );

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

/** @}*/

# ifdef __cplusplus
}
# endif

#endif /* _LIBVLC_DEPRECATED_H */
