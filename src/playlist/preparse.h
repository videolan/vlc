/*****************************************************************************
 * playlist/preparse.h
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

#ifndef VLC_PLAYLIST_PREPARSE_H
#define VLC_PLAYLIST_PREPARSE_H

#include <vlc_common.h>
#include <vlc_preparser.h>

#include "item.h"

typedef struct vlc_playlist vlc_playlist_t;
typedef struct input_item_node_t input_item_node_t;

/* Start an auto-preparse request for the media of a playlist item.
   On success, the request is stored in `item->preparser_req`, which owns it.
   Do nothing if the media does not need to be preparsed or if the request
   could not be started. */
void
vlc_playlist_AutoPreparse(vlc_playlist_t *playlist, vlc_playlist_item_t *item,
                          bool parse_subitems);

int
vlc_playlist_ExpandItem(vlc_playlist_t *playlist, size_t index,
                        const input_item_node_t *node);

int
vlc_playlist_ExpandItemFromNode(vlc_playlist_t *playlist,
                                const input_item_node_t *subitems);

#endif
