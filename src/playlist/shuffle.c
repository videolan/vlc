/*****************************************************************************
 * playlist/shuffle.c
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

#include <vlc_common.h>
#include <vlc_rand.h>
#include "control.h"
#include "item.h"
#include "notify.h"
#include "playlist.h"

void
vlc_playlist_Shuffle(vlc_playlist_t *playlist)
{
    vlc_playlist_AssertLocked(playlist);
    if (playlist->items.size < 2)
        /* we use size_t (unsigned), so the following loop would be incorrect */
        return;

    vlc_playlist_item_t *current = playlist->current != -1
                                 ? playlist->items.data[playlist->current]
                                 : NULL;

    /* initialize separately instead of using vlc_lrand48() to avoid locking the
     * mutex once for each item */
    unsigned short xsubi[3];
    vlc_rand_bytes(xsubi, sizeof(xsubi));

    /* Fisher-Yates shuffle */
    for (size_t i = playlist->items.size - 1; i != 0; --i)
    {
        size_t selected = (size_t) (nrand48(xsubi) % (i + 1));

        /* swap items i and selected */
        vlc_playlist_item_t *tmp = playlist->items.data[i];
        playlist->items.data[i] = playlist->items.data[selected];
        playlist->items.data[selected] = tmp;
    }

    struct vlc_playlist_state state;
    if (current)
    {
        /* the current position have changed after the shuffle */
        vlc_playlist_state_Save(playlist, &state);
        playlist->current = vlc_playlist_IndexOf(playlist, current);
        playlist->has_prev = vlc_playlist_ComputeHasPrev(playlist);
        playlist->has_next = vlc_playlist_ComputeHasNext(playlist);
    }

    vlc_playlist_Notify(playlist, on_items_reset, playlist->items.data,
                        playlist->items.size);
    if (current)
        vlc_playlist_state_NotifyChanges(playlist, &state);
}
