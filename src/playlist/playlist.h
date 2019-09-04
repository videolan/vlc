/*****************************************************************************
 * playlist/playlist.h
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

#ifndef VLC_PLAYLIST_NEW_INTERNAL_H
#define VLC_PLAYLIST_NEW_INTERNAL_H

#include <vlc_common.h>
#include <vlc_playlist.h>
#include <vlc_vector.h>
#include "../player/player.h"
#include "randomizer.h"

typedef struct input_item_t input_item_t;

#ifdef TEST_PLAYLIST
/* mock the player in tests */
# define vlc_player_New(a,b,c,d) (VLC_UNUSED(a), VLC_UNUSED(b), VLC_UNUSED(c), \
                                 malloc(1))
# define vlc_player_Delete(p) free(p)
# define vlc_player_Lock(p) VLC_UNUSED(p)
# define vlc_player_Unlock(p) VLC_UNUSED(p)
# define vlc_player_AddListener(a,b,c) (VLC_UNUSED(b), malloc(1))
# define vlc_player_RemoveListener(a,b) free(b)
# define vlc_player_SetCurrentMedia(a,b) (VLC_UNUSED(b), VLC_SUCCESS)
# define vlc_player_InvalidateNextMedia(p) VLC_UNUSED(p)
# define vlc_player_osd_Message(p, fmt...) VLC_UNUSED(p)
#endif /* TEST_PLAYLIST */

typedef struct VLC_VECTOR(vlc_playlist_item_t *) playlist_item_vector_t;

struct vlc_playlist
{
    vlc_player_t *player;
    libvlc_int_t *libvlc;
    bool auto_preparse;
    /* all remaining fields are protected by the lock of the player */
    struct vlc_player_listener_id *player_listener;
    playlist_item_vector_t items;
    struct randomizer randomizer;
    ssize_t current;
    bool has_prev;
    bool has_next;
    struct vlc_list listeners; /**< list of vlc_playlist_listener_id.node */
    enum vlc_playlist_playback_repeat repeat;
    enum vlc_playlist_playback_order order;
    uint64_t idgen;
};

/* Also disable vlc_assert_locked in tests since the symbol is not exported */
#if !defined(NDEBUG) && !defined(TEST_PLAYLIST)
static inline void
vlc_playlist_AssertLocked(vlc_playlist_t *playlist)
{
    vlc_player_assert_locked(playlist->player);
}
#else
#define vlc_playlist_AssertLocked(x) ((void) (0))
#endif

#endif
