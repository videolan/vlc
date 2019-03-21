/*****************************************************************************
 * playlist/playlist.c
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "playlist.h"

#include <vlc_common.h>

#include "content.h"
#include "item.h"
#include "player.h"

vlc_playlist_t *
vlc_playlist_New(vlc_object_t *parent)
{
    vlc_playlist_t *playlist = malloc(sizeof(*playlist));
    if (unlikely(!playlist))
        return NULL;

    bool ok = vlc_playlist_PlayerInit(playlist, parent);
    if (unlikely(!ok))
    {
        free(playlist);
        return NULL;
    }

    vlc_vector_init(&playlist->items);
    randomizer_Init(&playlist->randomizer);
    playlist->current = -1;
    playlist->has_prev = false;
    playlist->has_next = false;
    vlc_list_init(&playlist->listeners);
    playlist->repeat = VLC_PLAYLIST_PLAYBACK_REPEAT_NONE;
    playlist->order = VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL;
    playlist->idgen = 0;
#ifdef TEST_PLAYLIST
    playlist->libvlc = NULL;
    playlist->auto_preparse = false;
#else
    assert(parent);
    playlist->libvlc = vlc_object_instance(parent);
    playlist->auto_preparse = var_InheritBool(parent, "auto-preparse");
#endif

    return playlist;
}

void
vlc_playlist_Delete(vlc_playlist_t *playlist)
{
    assert(vlc_list_is_empty(&playlist->listeners));

    vlc_playlist_PlayerDestroy(playlist);
    randomizer_Destroy(&playlist->randomizer);
    vlc_playlist_ClearItems(playlist);
    free(playlist);
}

void
vlc_playlist_Lock(vlc_playlist_t *playlist)
{
    vlc_player_Lock(playlist->player);
}

void
vlc_playlist_Unlock(vlc_playlist_t *playlist)
{
    vlc_player_Unlock(playlist->player);
}
