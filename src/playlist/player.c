/*****************************************************************************
 * playlist/player.c
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
#include <vlc_interface.h>

#include "player.h"

#include "control.h"
#include "item.h"
#include "notify.h"
#include "playlist.h"
#include "preparse.h"

static void
player_on_current_media_changed(vlc_player_t *player, input_item_t *new_media,
                                void *userdata)
{
    VLC_UNUSED(player);
    vlc_playlist_t *playlist = userdata;

    /* the playlist and the player share the lock */
    vlc_playlist_AssertLocked(playlist);

    input_item_t *media = playlist->current != -1
                        ? playlist->items.data[playlist->current]->media
                        : NULL;
    if (new_media == media)
    {
        vlc_playlist_UpdateNextMedia(playlist);
        return;
    }

    ssize_t index;
    if (new_media)
    {
        index = vlc_playlist_IndexOfMedia(playlist, new_media);
        if (index != -1)
        {
            vlc_playlist_item_t *item = playlist->items.data[index];
            if (playlist->order == VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM)
                randomizer_Select(&playlist->randomizer, item);
        }
    }
    else
        index = -1;

    struct vlc_playlist_state state;
    vlc_playlist_state_Save(playlist, &state);

    playlist->current = index;
    playlist->has_prev = vlc_playlist_ComputeHasPrev(playlist);
    playlist->has_next = vlc_playlist_ComputeHasNext(playlist);

    vlc_playlist_state_NotifyChanges(playlist, &state);

    vlc_playlist_UpdateNextMedia(playlist);
}

static void
on_player_state_changed(vlc_player_t *player,
                        enum vlc_player_state new_state, void *userdata)
{
    vlc_playlist_t *playlist = userdata;

    if (new_state == VLC_PLAYER_STATE_STOPPED
     && playlist->stopped_action == VLC_PLAYLIST_MEDIA_STOPPED_EXIT)
        libvlc_Quit(vlc_object_instance(player));
}

static void
on_player_media_meta_changed(vlc_player_t *player, input_item_t *media,
                             void *userdata)
{
    VLC_UNUSED(player);
    vlc_playlist_t *playlist = userdata;

    /* the playlist and the player share the lock */
    vlc_playlist_AssertLocked(playlist);

    vlc_playlist_NotifyMediaUpdated(playlist, media);
}

static void
on_player_media_length_changed(vlc_player_t *player, vlc_tick_t new_length,
                               void *userdata)
{
    VLC_UNUSED(player);
    VLC_UNUSED(new_length);
    vlc_playlist_t *playlist = userdata;

    /* the playlist and the player share the lock */
    vlc_playlist_AssertLocked(playlist);

    input_item_t *media = vlc_player_GetCurrentMedia(player);
    assert(media);

    vlc_playlist_NotifyMediaUpdated(playlist, media);
}

static void
on_player_media_subitems_changed(vlc_player_t *player, input_item_t *media,
                                 const input_item_node_t *subitems,
                                 void *userdata)
{
    VLC_UNUSED(player);
    VLC_UNUSED(media);
    vlc_playlist_t *playlist = userdata;
    vlc_playlist_ExpandItemFromNode(playlist, subitems);
}

static const struct vlc_player_cbs player_callbacks = {
    .on_current_media_changed = player_on_current_media_changed,
    .on_state_changed = on_player_state_changed,
    .on_media_meta_changed = on_player_media_meta_changed,
    .on_length_changed = on_player_media_length_changed,
    .on_media_subitems_changed = on_player_media_subitems_changed,
};

bool
vlc_playlist_PlayerInit(vlc_playlist_t *playlist, vlc_object_t *parent)
{
    playlist->player = vlc_player_New(parent, VLC_PLAYER_LOCK_NORMAL);
    if (unlikely(!playlist->player))
        return false;

    vlc_player_Lock(playlist->player);
    /* the playlist and the player share the lock */
    vlc_playlist_AssertLocked(playlist);
    playlist->player_listener = vlc_player_AddListener(playlist->player,
                                                       &player_callbacks,
                                                       playlist);
    vlc_player_Unlock(playlist->player);
    if (unlikely(!playlist->player_listener))
    {
        vlc_player_Delete(playlist->player);
        return false;
    }
    return true;
}

void
vlc_playlist_PlayerDestroy(vlc_playlist_t *playlist)
{
    vlc_player_Lock(playlist->player);
    vlc_player_RemoveListener(playlist->player, playlist->player_listener);
    vlc_player_Unlock(playlist->player);

    vlc_player_Delete(playlist->player);
}

vlc_player_t *
vlc_playlist_GetPlayer(vlc_playlist_t *playlist)
{
    return playlist->player;
}

int
vlc_playlist_Start(vlc_playlist_t *playlist)
{
    return vlc_player_Start(playlist->player);
}

void
vlc_playlist_Stop(vlc_playlist_t *playlist)
{
    vlc_player_Stop(playlist->player);
}

void
vlc_playlist_Pause(vlc_playlist_t *playlist)
{
    vlc_player_Pause(playlist->player);
}

void
vlc_playlist_Resume(vlc_playlist_t *playlist)
{
    vlc_player_Resume(playlist->player);
}

void
vlc_playlist_SetMediaStoppedAction(vlc_playlist_t *playlist,
                                   enum vlc_playlist_media_stopped_action action)
{
    vlc_playlist_AssertLocked(playlist);
    playlist->stopped_action = action;
    vlc_playlist_UpdateNextMedia(playlist);
    vlc_playlist_Notify(playlist, on_media_stopped_action_changed, action);
}
