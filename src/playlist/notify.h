/*****************************************************************************
 * playlist/notify.h
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

#ifndef VLC_PLAYLIST_NOTIFY_H
#define VLC_PLAYLIST_NOTIFY_H

#include <vlc_common.h>
#include <vlc_list.h>

typedef struct vlc_playlist vlc_playlist_t;

struct vlc_playlist_listener_id
{
    const struct vlc_playlist_callbacks *cbs;
    void *userdata;
    struct vlc_list node; /**< node of vlc_playlist.listeners */
};

struct vlc_playlist_state {
    ssize_t current;
    bool has_prev;
    bool has_next;
};

#define vlc_playlist_listener_foreach(listener, playlist) \
    vlc_list_foreach(listener, &(playlist)->listeners, node)

#define vlc_playlist_NotifyListener(playlist, listener, event, ...) \
do { \
    if (listener->cbs->event) \
        listener->cbs->event(playlist, ##__VA_ARGS__, listener->userdata); \
} while (0)

#define vlc_playlist_Notify(playlist, event, ...) \
do { \
    vlc_playlist_AssertLocked(playlist); \
    vlc_playlist_listener_id *listener; \
    vlc_playlist_listener_foreach(listener, playlist) \
        vlc_playlist_NotifyListener(playlist, listener, event, ##__VA_ARGS__); \
} while(0)

void
vlc_playlist_state_Save(vlc_playlist_t *playlist,
                        struct vlc_playlist_state *state);

void
vlc_playlist_state_NotifyChanges(vlc_playlist_t *playlist,
                                 struct vlc_playlist_state *saved_state);

void
vlc_playlist_NotifyMediaUpdated(vlc_playlist_t *playlist, input_item_t *media);

#endif
