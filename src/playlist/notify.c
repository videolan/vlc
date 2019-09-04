/*****************************************************************************
 * playlist/notify.c
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

#include "notify.h"

#include "item.h"
#include "playlist.h"

static void
vlc_playlist_NotifyCurrentState(vlc_playlist_t *playlist,
                                vlc_playlist_listener_id *listener)
{
    vlc_playlist_NotifyListener(playlist, listener, on_items_reset,
                                playlist->items.data, playlist->items.size);
    vlc_playlist_NotifyListener(playlist, listener, on_playback_repeat_changed,
                                playlist->repeat);
    vlc_playlist_NotifyListener(playlist, listener, on_playback_order_changed,
                                playlist->order);
    vlc_playlist_NotifyListener(playlist, listener, on_current_index_changed,
                                playlist->current);
    vlc_playlist_NotifyListener(playlist, listener, on_has_prev_changed,
                                playlist->has_prev);
    vlc_playlist_NotifyListener(playlist, listener, on_has_next_changed,
                                playlist->has_next);
}

vlc_playlist_listener_id *
vlc_playlist_AddListener(vlc_playlist_t *playlist,
                         const struct vlc_playlist_callbacks *cbs,
                         void *userdata, bool notify_current_state)
{
    vlc_playlist_AssertLocked(playlist);

    vlc_playlist_listener_id *listener = malloc(sizeof(*listener));
    if (unlikely(!listener))
        return NULL;

    listener->cbs = cbs;
    listener->userdata = userdata;
    vlc_list_append(&listener->node, &playlist->listeners);

    if (notify_current_state)
        vlc_playlist_NotifyCurrentState(playlist, listener);

    return listener;
}

void
vlc_playlist_RemoveListener(vlc_playlist_t *playlist,
                            vlc_playlist_listener_id *listener)
{
    /* The playlist head is not needed to remove a node, but the list must be
     * locked. */
    vlc_playlist_AssertLocked(playlist); VLC_UNUSED(playlist);

    vlc_list_remove(&listener->node);
    free(listener);
}

void
vlc_playlist_state_Save(vlc_playlist_t *playlist,
                        struct vlc_playlist_state *state)
{
    state->current = playlist->current;
    state->has_prev = playlist->has_prev;
    state->has_next = playlist->has_next;
}

void
vlc_playlist_state_NotifyChanges(vlc_playlist_t *playlist,
                                 struct vlc_playlist_state *saved_state)
{
    if (saved_state->current != playlist->current)
        vlc_playlist_Notify(playlist, on_current_index_changed, playlist->current);
    if (saved_state->has_prev != playlist->has_prev)
        vlc_playlist_Notify(playlist, on_has_prev_changed, playlist->has_prev);
    if (saved_state->has_next != playlist->has_next)
        vlc_playlist_Notify(playlist, on_has_next_changed, playlist->has_next);
}

static inline bool
vlc_playlist_HasItemUpdatedListeners(vlc_playlist_t *playlist)
{
    vlc_playlist_listener_id *listener;
    vlc_playlist_listener_foreach(listener, playlist)
        if (listener->cbs->on_items_updated)
            return true;
    return false;
}

void
vlc_playlist_NotifyMediaUpdated(vlc_playlist_t *playlist, input_item_t *media)
{
    vlc_playlist_AssertLocked(playlist);
    if (!vlc_playlist_HasItemUpdatedListeners(playlist))
        /* no need to find the index if there are no listeners */
        return;

    ssize_t index;
    if (playlist->current != -1 &&
            playlist->items.data[playlist->current]->media == media)
        /* the player typically sends events for the current item, so we can
         * often avoid to search */
        index = playlist->current;
    else
    {
        /* linear search */
        index = vlc_playlist_IndexOfMedia(playlist, media);
        if (index == -1)
            return;
    }
    vlc_playlist_Notify(playlist, on_items_updated, index,
                        &playlist->items.data[index], 1);
}
