/*****************************************************************************
 * playlist/control.h
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
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

#ifndef VLC_PLAYLIST_CONTROL_H
#define VLC_PLAYLIST_CONTROL_H

#include <vlc_common.h>

typedef struct vlc_playlist vlc_playlist_t;

bool
vlc_playlist_ComputeHasPrev(vlc_playlist_t *playlist);

bool
vlc_playlist_ComputeHasNext(vlc_playlist_t *playlist);

int
vlc_playlist_SetCurrentMedia(vlc_playlist_t *playlist, ssize_t index);

input_item_t *
vlc_playlist_GetNextMedia(vlc_playlist_t *playlist);

#endif
