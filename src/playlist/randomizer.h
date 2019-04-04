/*****************************************************************************
 * randomizer.h
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

#ifndef RANDOMIZER_H
#define RANDOMIZER_H

#include <vlc_common.h>
#include <vlc_vector.h>

typedef struct vlc_playlist_item vlc_playlist_item_t;

/**
 * \defgroup playlist_randomizer Playlist randomizer helper
 * \ingroup playlist
 *  @{ */

/**
 * Playlist helper to manage random playback.
 *
 * See randomizer.c for implementation details.
 */
struct randomizer {
    struct VLC_VECTOR(vlc_playlist_item_t *) items;
    unsigned short xsubi[3]; /* random state */
    bool loop;
    size_t head;
    size_t next;
    size_t history;
};

/**
 * Initialize an empty randomizer.
 */
void
randomizer_Init(struct randomizer *randomizer);

/**
 * Destroy a randomizer.
 */
void
randomizer_Destroy(struct randomizer *randomizer);

/**
 * Enable or disable "loop" mode.
 *
 * This affects the behavior of prev/next.
 */
void
randomizer_SetLoop(struct randomizer *randomizer, bool loop);

/**
 * Return the number of items in the randomizer.
 */
bool
randomizer_Count(struct randomizer *randomizer);

/**
 * Start a new random cycle.
 *
 * The "history" is lost, and "next" can be called _n_ times if the randomizer
 * contains _n_ items (when loop is disabled).
 */
void
randomizer_Reshuffle(struct randomizer *randomizer);

/**
 * Indicate whether there is a previous item.
 */
bool
randomizer_HasPrev(struct randomizer *randomizer);

/**
 * Indicate whether there is a next item.
 */
bool
randomizer_HasNext(struct randomizer *randomizer);

/**
 * Peek the previous item (without changing the current one).
 */
vlc_playlist_item_t *
randomizer_PeekPrev(struct randomizer *randomizer);

/**
 * Peek the next item (without changing the current one).
 */
vlc_playlist_item_t *
randomizer_PeekNext(struct randomizer *randomizer);

/**
 * Go back to the previous item.
 */
vlc_playlist_item_t *
randomizer_Prev(struct randomizer *randomizer);

/**
 * Go back to the next item.
 */
vlc_playlist_item_t *
randomizer_Next(struct randomizer *randomizer);

/**
 * Force the selection of a specific item.
 *
 * This function should be called when the user requested to play a specific
 * item in the playlist.
 */
void
randomizer_Select(struct randomizer *randomizer,
                  const vlc_playlist_item_t *item);

/**
 * Add items to the randomizer.
 *
 * This function should be called when items are added to the playlist.
 */
bool
randomizer_Add(struct randomizer *randomizer, vlc_playlist_item_t *items[],
               size_t count);

/**
 * Remove items from the randomizer.
 *
 * This function should be called when items are removed from the playlist.
 */
void
randomizer_Remove(struct randomizer *randomizer,
                  vlc_playlist_item_t *const items[], size_t count);

/**
 * Clear the randomizer.
 */
void
randomizer_Clear(struct randomizer *randomizer);

/** @} */

#endif
