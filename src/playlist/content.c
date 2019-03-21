/*****************************************************************************
 * playlist/content.c
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

#include "content.h"

#include "control.h"
#include "item.h"
#include "notify.h"
#include "playlist.h"
#include "preparse.h"

void
vlc_playlist_ClearItems(vlc_playlist_t *playlist)
{
    vlc_playlist_item_t *item;
    vlc_vector_foreach(item, &playlist->items)
        vlc_playlist_item_Release(item);
    vlc_vector_clear(&playlist->items);
}

static void
vlc_playlist_ItemsReset(vlc_playlist_t *playlist)
{
    if (playlist->order == VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM)
        randomizer_Clear(&playlist->randomizer);

    struct vlc_playlist_state state;
    vlc_playlist_state_Save(playlist, &state);

    playlist->current = -1;
    playlist->has_prev = vlc_playlist_ComputeHasPrev(playlist);
    playlist->has_next = vlc_playlist_ComputeHasNext(playlist);

    vlc_playlist_Notify(playlist, on_items_reset, playlist->items.data,
                   playlist->items.size);
    vlc_playlist_state_NotifyChanges(playlist, &state);
}

static void
vlc_playlist_ItemsInserted(vlc_playlist_t *playlist, size_t index, size_t count)
{
    if (playlist->order == VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM)
        randomizer_Add(&playlist->randomizer,
                       &playlist->items.data[index], count);

    struct vlc_playlist_state state;
    vlc_playlist_state_Save(playlist, &state);

    if (playlist->current >= (ssize_t) index)
        playlist->current += count;
    playlist->has_prev = vlc_playlist_ComputeHasPrev(playlist);
    playlist->has_next = vlc_playlist_ComputeHasNext(playlist);

    vlc_playlist_item_t **items = &playlist->items.data[index];
    vlc_playlist_Notify(playlist, on_items_added, index, items, count);
    vlc_playlist_state_NotifyChanges(playlist, &state);

    for (size_t i = index; i < index + count; ++i)
    {
        vlc_playlist_item_t *item = playlist->items.data[i];
        vlc_playlist_AutoPreparse(playlist, item->media);
    }
}

static void
vlc_playlist_ItemsMoved(vlc_playlist_t *playlist, size_t index, size_t count,
                        size_t target)
{
    struct vlc_playlist_state state;
    vlc_playlist_state_Save(playlist, &state);

    if (playlist->current != -1) {
        size_t current = (size_t) playlist->current;
        if (index < target)
        {
            if (current >= index && current < index + count)
                /* current item belongs to the moved block */
                playlist->current += target - index;
            else if (current >= index + count && current < target + count)
                /* current item was shifted backwards to the moved block */
                playlist->current -= count;
            /* else the current item does not move */
        }
        else
        {
            if (current >= index && current < index + count)
                /* current item belongs to the moved block */
                playlist->current -= index - target;
            else if (current >= target && current < index)
                /* current item was shifted forward to the moved block */
                playlist->current += count;
            /* else the current item does not move */
        }
    }

    playlist->has_prev = vlc_playlist_ComputeHasPrev(playlist);
    playlist->has_next = vlc_playlist_ComputeHasNext(playlist);

    vlc_playlist_Notify(playlist, on_items_moved, index, count, target);
    vlc_playlist_state_NotifyChanges(playlist, &state);
}

static void
vlc_playlist_ItemsRemoving(vlc_playlist_t *playlist, size_t index, size_t count)
{
    if (playlist->order == VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM)
        randomizer_Remove(&playlist->randomizer,
                          &playlist->items.data[index], count);
}

/* return whether the current media has changed */
static bool
vlc_playlist_ItemsRemoved(vlc_playlist_t *playlist, size_t index, size_t count)
{
    struct vlc_playlist_state state;
    vlc_playlist_state_Save(playlist, &state);

    bool current_media_changed = false;
    if (playlist->current != -1) {
        size_t current = (size_t) playlist->current;
        if (current >= index && current < index + count) {
            /* current item has been removed */
            if (index + count < playlist->items.size) {
                /* select the first item after the removed block */
                playlist->current = index;
            } else {
                /* no more items */
                playlist->current = -1;
            }
            current_media_changed = true;
        } else if (current >= index + count) {
            playlist->current -= count;
        }
    }
    playlist->has_prev = vlc_playlist_ComputeHasPrev(playlist);
    playlist->has_next = vlc_playlist_ComputeHasNext(playlist);

    vlc_playlist_Notify(playlist, on_items_removed, index, count);
    vlc_playlist_state_NotifyChanges(playlist, &state);

    return current_media_changed;
}

static void
vlc_playlist_ItemReplaced(vlc_playlist_t *playlist, size_t index)
{
    struct vlc_playlist_state state;
    vlc_playlist_state_Save(playlist, &state);

    playlist->has_prev = vlc_playlist_ComputeHasPrev(playlist);
    playlist->has_next = vlc_playlist_ComputeHasNext(playlist);

    vlc_playlist_Notify(playlist, on_items_updated, index,
                        &playlist->items.data[index], 1);
    vlc_playlist_state_NotifyChanges(playlist, &state);

    vlc_playlist_AutoPreparse(playlist, playlist->items.data[index]->media);
}

size_t
vlc_playlist_Count(vlc_playlist_t *playlist)
{
    vlc_playlist_AssertLocked(playlist);
    return playlist->items.size;
}

vlc_playlist_item_t *
vlc_playlist_Get(vlc_playlist_t *playlist, size_t index)
{
    vlc_playlist_AssertLocked(playlist);
    return playlist->items.data[index];
}

ssize_t
vlc_playlist_IndexOf(vlc_playlist_t *playlist, const vlc_playlist_item_t *item)
{
    vlc_playlist_AssertLocked(playlist);

    ssize_t index;
    vlc_vector_index_of(&playlist->items, item, &index);
    return index;
}

ssize_t
vlc_playlist_IndexOfMedia(vlc_playlist_t *playlist, const input_item_t *media)
{
    vlc_playlist_AssertLocked(playlist);

    playlist_item_vector_t *items = &playlist->items;
    for (size_t i = 0; i < items->size; ++i)
        if (items->data[i]->media == media)
            return i;
    return -1;
}

ssize_t
vlc_playlist_IndexOfId(vlc_playlist_t *playlist, uint64_t id)
{
    vlc_playlist_AssertLocked(playlist);

    playlist_item_vector_t *items = &playlist->items;
    for (size_t i = 0; i < items->size; ++i)
        if (items->data[i]->id == id)
            return i;
    return -1;
}

void
vlc_playlist_Clear(vlc_playlist_t *playlist)
{
    vlc_playlist_AssertLocked(playlist);

    int ret = vlc_player_SetCurrentMedia(playlist->player, NULL);
    VLC_UNUSED(ret); /* what could we do? */

    vlc_playlist_ClearItems(playlist);
    vlc_playlist_ItemsReset(playlist);
}

static int
vlc_playlist_MediaToItems(vlc_playlist_t *playlist, input_item_t *const media[],
                          size_t count, vlc_playlist_item_t *items[])
{
    vlc_playlist_AssertLocked(playlist);
    size_t i;
    for (i = 0; i < count; ++i)
    {
        uint64_t id = playlist->idgen++;
        items[i] = vlc_playlist_item_New(media[i], id);
        if (unlikely(!items[i]))
            break;
    }
    if (i < count)
    {
        /* allocation failure, release partial items */
        while (i)
            vlc_playlist_item_Release(items[--i]);
        return VLC_ENOMEM;
    }
    return VLC_SUCCESS;
}

int
vlc_playlist_Insert(vlc_playlist_t *playlist, size_t index,
                    input_item_t *const media[], size_t count)
{
    vlc_playlist_AssertLocked(playlist);
    assert(index <= playlist->items.size);

    /* make space in the vector */
    if (!vlc_vector_insert_hole(&playlist->items, index, count))
        return VLC_ENOMEM;

    /* create playlist items in place */
    int ret = vlc_playlist_MediaToItems(playlist, media, count,
                                        &playlist->items.data[index]);
    if (ret != VLC_SUCCESS)
    {
        /* we were optimistic, it failed, restore the vector state */
        vlc_vector_remove_slice(&playlist->items, index, count);
        return ret;
    }

    vlc_playlist_ItemsInserted(playlist, index, count);
    vlc_player_InvalidateNextMedia(playlist->player);

    return VLC_SUCCESS;
}

void
vlc_playlist_Move(vlc_playlist_t *playlist, size_t index, size_t count,
                  size_t target)
{
    vlc_playlist_AssertLocked(playlist);
    assert(index + count <= playlist->items.size);
    assert(target + count <= playlist->items.size);

    vlc_vector_move_slice(&playlist->items, index, count, target);

    vlc_playlist_ItemsMoved(playlist, index, count, target);
    vlc_player_InvalidateNextMedia(playlist->player);
}

void
vlc_playlist_Remove(vlc_playlist_t *playlist, size_t index, size_t count)
{
    vlc_playlist_AssertLocked(playlist);
    assert(index < playlist->items.size);

    vlc_playlist_ItemsRemoving(playlist, index, count);

    for (size_t i = 0; i < count; ++i)
        vlc_playlist_item_Release(playlist->items.data[index + i]);

    vlc_vector_remove_slice(&playlist->items, index, count);

    bool current_media_changed = vlc_playlist_ItemsRemoved(playlist, index,
                                                           count);
    if (current_media_changed)
        vlc_playlist_SetCurrentMedia(playlist, playlist->current);
    else
        vlc_player_InvalidateNextMedia(playlist->player);
}

static int
vlc_playlist_Replace(vlc_playlist_t *playlist, size_t index,
                     input_item_t *media)
{
    vlc_playlist_AssertLocked(playlist);
    assert(index < playlist->items.size);

    uint64_t id = playlist->idgen++;
    vlc_playlist_item_t *item = vlc_playlist_item_New(media, id);
    if (!item)
        return VLC_ENOMEM;

    if (playlist->order == VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM)
    {
        randomizer_Remove(&playlist->randomizer,
                          &playlist->items.data[index], 1);
        randomizer_Add(&playlist->randomizer, &item, 1);
    }

    vlc_playlist_item_Release(playlist->items.data[index]);
    playlist->items.data[index] = item;

    vlc_playlist_ItemReplaced(playlist, index);
    return VLC_SUCCESS;
}

int
vlc_playlist_Expand(vlc_playlist_t *playlist, size_t index,
                    input_item_t *const media[], size_t count)
{
    vlc_playlist_AssertLocked(playlist);
    assert(index < playlist->items.size);

    if (count == 0)
        vlc_playlist_RemoveOne(playlist, index);
    else
    {
        int ret = vlc_playlist_Replace(playlist, index, media[0]);
        if (ret != VLC_SUCCESS)
            return ret;

        if (count > 1)
        {
            /* make space in the vector */
            if (!vlc_vector_insert_hole(&playlist->items, index + 1, count - 1))
                return VLC_ENOMEM;

            /* create playlist items in place */
            ret = vlc_playlist_MediaToItems(playlist, &media[1], count - 1,
                                            &playlist->items.data[index + 1]);
            if (ret != VLC_SUCCESS)
            {
                /* we were optimistic, it failed, restore the vector state */
                vlc_vector_remove_slice(&playlist->items, index + 1, count - 1);
                return ret;
            }
            vlc_playlist_ItemsInserted(playlist, index + 1, count - 1);
        }

        if ((ssize_t) index == playlist->current)
            vlc_playlist_SetCurrentMedia(playlist, playlist->current);
        else
            vlc_player_InvalidateNextMedia(playlist->player);
    }

    return VLC_SUCCESS;
}
