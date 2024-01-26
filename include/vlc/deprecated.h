/*****************************************************************************
 * deprecated.h:  libvlc deprecated API
 *****************************************************************************
 * Copyright (C) 1998-2008 VLC authors and VideoLAN
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Paul Saman <jpsaman@videolan.org>
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

#ifndef LIBVLC_DEPRECATED_H
#define LIBVLC_DEPRECATED_H 1

# ifdef __cplusplus
extern "C" {
# endif

/**
 * \ingroup libvlc
 * \defgroup libvlc_playlist LibVLC playlist (legacy)
 * @deprecated Use @ref libvlc_media_list instead.
 * @{
 * \file
 * LibVLC deprecated playlist API
 */

/**
 * Start playing (if there is any item in the playlist).
 *
 * Additional playlist item options can be specified for addition to the
 * item before it is played.
 *
 * \param p_instance the playlist instance
 */
LIBVLC_DEPRECATED LIBVLC_API
void libvlc_playlist_play( libvlc_instance_t *p_instance );

/** @}*/

# ifdef __cplusplus
}
# endif

#endif /* _LIBVLC_DEPRECATED_H */
