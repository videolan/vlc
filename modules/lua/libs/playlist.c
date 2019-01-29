/*****************************************************************************
 * playlist.c
 *****************************************************************************
 * Copyright (C) 2007-2011 the VideoLAN team
 *
 * Authors: Antoine Cellerier <dionoea at videolan tod org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifndef  _GNU_SOURCE
#   define  _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_player.h>

#include "../vlc.h"
#include "../libs.h"
#include "input.h"
#include "variables.h"
#include "misc.h"

void vlclua_set_playlist_internal(lua_State *L, vlc_playlist_t *playlist)
{
    vlclua_set_object(L, vlclua_set_playlist_internal, playlist);
}

vlc_playlist_t *vlclua_get_playlist_internal(lua_State *L)
{
    return vlclua_get_object(L, vlclua_set_playlist_internal);
}

static int vlclua_playlist_prev(lua_State *L)
{
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);
    vlc_playlist_Lock(playlist);
    vlc_playlist_Prev(playlist);
    vlc_playlist_Unlock(playlist);
    return 0;
}

static int vlclua_playlist_next(lua_State *L)
{
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);
    vlc_playlist_Lock(playlist);
    vlc_playlist_Next(playlist);
    vlc_playlist_Unlock(playlist);
    return 0;
}

static int vlclua_playlist_skip(lua_State *L)
{
    int n = luaL_checkinteger( L, 1 );
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);
    if (n < 0) {
        for (int i = 0; i < -n; i++)
            vlc_playlist_Prev(playlist);
    } else {
        for (int i = 0; i < n; ++i)
            vlc_playlist_Next(playlist);
    }
    return 0;
}

static int vlclua_playlist_play(lua_State *L)
{
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);
    vlc_playlist_Lock(playlist);
    if (vlc_playlist_GetCurrentIndex(playlist) == -1 &&
            vlc_playlist_Count(playlist) > 0)
        vlc_playlist_GoTo(playlist, 0);
    vlc_playlist_Start(playlist);
    vlc_playlist_Unlock(playlist);
    return 0;
}

static int vlclua_playlist_pause(lua_State *L)
{
    /* this is in fact a toggle pause */
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);
    vlc_player_t *player = vlc_playlist_GetPlayer(playlist);

    vlc_player_Lock(player);
    if (vlc_player_GetState(player) != VLC_PLAYER_STATE_PAUSED)
        vlc_player_Pause(player);
    else
        vlc_player_Resume(player);
    vlc_player_Unlock(player);

    return 0;
}

static int vlclua_playlist_stop(lua_State *L)
{
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);
    vlc_playlist_Lock(playlist);
    vlc_playlist_Stop(playlist);
    vlc_playlist_Unlock(playlist);
    return 0;
}

static int vlclua_playlist_clear( lua_State *L)
{
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);
    vlc_playlist_Lock(playlist);
    vlc_playlist_Clear(playlist);
    vlc_playlist_Unlock(playlist);
    return 0;
}

static bool take_bool(lua_State *L)
{
    const char *s = luaL_checkstring(L, -1);
    lua_pop( L, 1 );
    return s && !strcmp(s, "on");
}

static int vlclua_playlist_repeat_(lua_State *L,
                               enum vlc_playlist_playback_repeat enabled_mode)
{
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);
    int top = lua_gettop(L);
    if (top > 1)
        return vlclua_error(L);

    vlc_playlist_Lock(playlist);

    bool enable;
    if (top == 0)
    {
        /* no value provided, toggle the current */
        enum vlc_playlist_playback_repeat repeat =
                vlc_playlist_GetPlaybackRepeat(playlist);
        enable = repeat != enabled_mode;
    }
    else
    {
        /* use the provided value */
        enable = take_bool(L);
    }

    enum vlc_playlist_playback_repeat new_repeat = enable
                                    ? enabled_mode
                                    : VLC_PLAYLIST_PLAYBACK_REPEAT_NONE;

    vlc_playlist_SetPlaybackRepeat(playlist, new_repeat);

    vlc_playlist_Unlock(playlist);

    lua_pushboolean(L, enable);
    return 1;
}

static int vlclua_playlist_repeat(lua_State *L)
{
    return vlclua_playlist_repeat_(L, VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT);
}

static int vlclua_playlist_loop(lua_State *L)
{
    return vlclua_playlist_repeat_(L, VLC_PLAYLIST_PLAYBACK_REPEAT_ALL);
}

static int vlclua_playlist_get_repeat(lua_State *L)
{
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);

    vlc_playlist_Lock(playlist);
    enum vlc_playlist_playback_repeat repeat =
            vlc_playlist_GetPlaybackRepeat(playlist);
    bool result = repeat != VLC_PLAYLIST_PLAYBACK_REPEAT_NONE;
    vlc_playlist_Unlock(playlist);

    lua_pushboolean(L, result);
    return 1;
}

static int vlclua_playlist_get_loop(lua_State *L)
{
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);

    vlc_playlist_Lock(playlist);
    enum vlc_playlist_playback_repeat repeat =
            vlc_playlist_GetPlaybackRepeat(playlist);
    bool result = repeat == VLC_PLAYLIST_PLAYBACK_REPEAT_ALL;
    vlc_playlist_Unlock(playlist);

    lua_pushboolean(L, result);
    return 1;
}

static int vlclua_playlist_random(lua_State *L)
{
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);
    int top = lua_gettop(L);
    if (top > 1)
        return vlclua_error(L);

    vlc_playlist_Lock(playlist);

    bool enable;
    if (top == 0)
    {
        enum vlc_playlist_playback_order order =
                vlc_playlist_GetPlaybackOrder(playlist);
        enable = order != VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM;
    }
    else
    {
        /* use the provided value */
        enable = take_bool(L);
    }

    enum vlc_playlist_playback_order new_order = enable
                                    ? VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM
                                    : VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL;

    vlc_playlist_SetPlaybackOrder(playlist, new_order);

    vlc_playlist_Unlock(playlist);

    lua_pushboolean(L, enable);
    return 1;
}

static int vlclua_playlist_get_random(lua_State *L)
{
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);

    vlc_playlist_Lock(playlist);
    enum vlc_playlist_playback_order order =
            vlc_playlist_GetPlaybackOrder(playlist);
    bool result = order == VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM;
    vlc_playlist_Unlock(playlist);

    lua_pushboolean(L, result);
    return 1;
}

static int vlclua_playlist_gotoitem(lua_State *L)
{
    uint64_t id = luaL_checkinteger(L, 1);
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);

    int ret;

    vlc_playlist_Lock(playlist);
    ssize_t index = vlc_playlist_IndexOfId(playlist, id);
    if (index == -1)
        ret = VLC_ENOITEM;
    else
    {
        vlc_playlist_GoTo(playlist, index);
        ret = VLC_SUCCESS;
    }
    vlc_playlist_Unlock(playlist);

    return vlclua_push_ret(L, ret);
}

static int vlclua_playlist_delete(lua_State *L)
{
    uint64_t id = luaL_checkinteger(L, 1);
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);

    int ret;

    vlc_playlist_Lock(playlist);
    ssize_t index = vlc_playlist_IndexOfId(playlist, id);
    if (index == -1)
        ret = -1;
    else
    {
        vlc_playlist_RemoveOne(playlist, index);
        ret = VLC_SUCCESS;
    }
    vlc_playlist_Unlock(playlist);

    return vlclua_push_ret(L, ret);
}

static int vlclua_playlist_move(lua_State *L)
{
    uint64_t item_id = luaL_checkinteger(L, 1);
    uint64_t target_id = luaL_checkinteger(L, 2);
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);

    int ret;

    vlc_playlist_Lock(playlist);
    ssize_t item_index = vlc_playlist_IndexOfId(playlist, item_id);
    ssize_t target_index = vlc_playlist_IndexOfId(playlist, target_id);
    if (item_index == -1 || target_index == -1)
        ret = -1;
    else
    {
        /* if the current item was before the target, moving it shifts the
         * target item by one */
        size_t new_index = item_index <= target_index ? target_index
                                                      : target_index + 1;
        vlc_playlist_MoveOne(playlist, item_index, new_index);
        ret = VLC_SUCCESS;
    }
    vlc_playlist_Unlock(playlist);

    return vlclua_push_ret(L, ret);
}

static int vlclua_playlist_add_common(lua_State *L, bool play)
{
    vlc_object_t *obj = vlclua_get_this(L);
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);
    int count = 0;

    /* playlist */
    if (!lua_istable(L, -1))
    {
        msg_Warn(obj, "Playlist should be a table.");
        return 0;
    }

    lua_pushnil(L);

    vlc_playlist_Lock(playlist);

    /* playlist nil */
    while (lua_next(L, -2))
    {
        input_item_t *item = vlclua_read_input_item(obj, L);
        if (item != NULL)
        {
            int ret = vlc_playlist_AppendOne(playlist, item);
            if (ret == VLC_SUCCESS)
            {
                count++;
                if (play)
                {
                    size_t last = vlc_playlist_Count(playlist) - 1;
                    vlc_playlist_PlayAt(playlist, last);
                }
            }
            input_item_Release(item);
        }
        /* pop the value, keep the key for the next lua_next() call */
        lua_pop(L, 1);
    }
    /* playlist */

    vlc_playlist_Unlock(playlist);

    lua_pushinteger(L, count);
    return 1;
}

static int vlclua_playlist_add(lua_State *L)
{
    return vlclua_playlist_add_common(L, true);
}

static int vlclua_playlist_enqueue(lua_State *L)
{
    return vlclua_playlist_add_common(L, false);
}

static void push_playlist_item(lua_State *L, vlc_playlist_item_t *item)
{
    lua_newtable(L);

    lua_pushinteger(L, vlc_playlist_item_GetId(item));
    lua_setfield(L, -2, "id");

    input_item_t *media = vlc_playlist_item_GetMedia(item);

    /* Apart from nb_played, these fields unfortunately duplicate
       fields already available from the input item */
    char *name = input_item_GetTitleFbName(media);
    lua_pushstring(L, name);
    free(name);
    lua_setfield(L, -2, "name");

    lua_pushstring(L, media->psz_uri);
    lua_setfield(L, -2, "path");

    if( media->i_duration < 0 )
        lua_pushnumber(L, -1);
    else
        lua_pushnumber(L, secf_from_vlc_tick(media->i_duration));
    lua_setfield(L, -2, "duration");

    luaopen_input_item(L, media);
}

static int vlclua_playlist_get(lua_State *L)
{
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);
    uint64_t item_id = luaL_checkinteger(L, 1);

    vlc_playlist_Lock(playlist);
    ssize_t index = vlc_playlist_IndexOfId(playlist, item_id);
    vlc_playlist_item_t *item = index != -1 ? vlc_playlist_Get(playlist, index)
                                            : NULL;
    if (item)
        push_playlist_item(L, item);
    else
        lua_pushnil(L);
    vlc_playlist_Unlock(playlist);

    return 1;
}

static int vlclua_playlist_list(lua_State *L)
{
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);

    vlc_playlist_Lock(playlist);

    size_t count = vlc_playlist_Count(playlist);
    lua_createtable(L, count, 0);

    for (size_t i = 0; i < count; ++i)
    {
        push_playlist_item(L, vlc_playlist_Get(playlist, i));
        lua_rawseti(L, -2, i + 1);
    }

    vlc_playlist_Unlock(playlist);

    return 1;
}

static int vlclua_playlist_current(lua_State *L)
{
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);

    vlc_playlist_Lock(playlist);
    ssize_t current = vlc_playlist_GetCurrentIndex(playlist);
    int id;
    if (current != -1) {
        vlc_playlist_item_t *item = vlc_playlist_Get(playlist, current);
        id = vlc_playlist_item_GetId(item);
    } else
        id = -1;
    vlc_playlist_Unlock(playlist);

    lua_pushinteger(L, id);
    return 1;
}

static int vlclua_playlist_current_item(lua_State *L)
{
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);

    vlc_playlist_Lock(playlist);
    ssize_t index = vlc_playlist_GetCurrentIndex(playlist);
    vlc_playlist_item_t *item = index != -1 ? vlc_playlist_Get(playlist, index)
                                            : NULL;
    if (item)
        push_playlist_item(L, item);
    else
        lua_pushnil(L);
    vlc_playlist_Unlock(playlist);

    return 1;
}

static bool vlc_sort_key_from_string(const char *keyname,
                                     enum vlc_playlist_sort_key *key)
{
    static const struct
    {
        const char *keyname;
        enum vlc_playlist_sort_key key;
    } map[] = {
        { "title",    VLC_PLAYLIST_SORT_KEY_TITLE },
        { "artist",   VLC_PLAYLIST_SORT_KEY_ARTIST },
        { "genre",    VLC_PLAYLIST_SORT_KEY_GENRE },
        { "duration", VLC_PLAYLIST_SORT_KEY_DURATION },
        { "album",    VLC_PLAYLIST_SORT_KEY_ALBUM },
    };
    for (size_t i = 0; i < ARRAY_SIZE(map); ++i)
    {
        if (!strcmp(keyname, map[i].keyname))
        {
            *key = map[i].key;
            return true;
        }
    }
    return false;
}

static int vlclua_playlist_sort( lua_State *L )
{
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);

    const char *keyname = luaL_checkstring(L, 1);

    int ret;
    if (!strcmp(keyname, "random"))
    {
        /* sort randomly -> shuffle */
        vlc_playlist_Lock(playlist);
        vlc_playlist_Shuffle(playlist);
        vlc_playlist_Unlock(playlist);
        ret = VLC_SUCCESS;
    }
    else
    {
        struct vlc_playlist_sort_criterion criterion;
        if (!vlc_sort_key_from_string(keyname, &criterion.key))
            return luaL_error(L, "Invalid search key.");
        criterion.order = luaL_optboolean(L, 2, 0)
                        ? VLC_PLAYLIST_SORT_ORDER_DESCENDING
                        : VLC_PLAYLIST_SORT_ORDER_ASCENDING;

        vlc_playlist_Lock(playlist);
        ret = vlc_playlist_Sort(playlist, &criterion, 1);
        vlc_playlist_Unlock(playlist);
    }
    return vlclua_push_ret(L, ret);
}

static int vlclua_playlist_status(lua_State *L)
{
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);

    vlc_player_t *player = vlc_playlist_GetPlayer(playlist);
    vlc_player_Lock(player);
    enum vlc_player_state state = vlc_player_GetState(player);
    vlc_player_Unlock(player);

    switch (state)
    {
        case VLC_PLAYER_STATE_STOPPED:
            lua_pushliteral(L, "stopped");
            break;
        case VLC_PLAYER_STATE_STARTED:
            lua_pushliteral(L, "started");
            break;
        case VLC_PLAYER_STATE_PLAYING:
            lua_pushliteral(L, "playing");
            break;
        case VLC_PLAYER_STATE_PAUSED:
            lua_pushliteral(L, "paused");
            break;
        case VLC_PLAYER_STATE_STOPPING:
            lua_pushliteral(L, "stopping");
            break;
        default:
            lua_pushliteral(L, "unknown");
    }
    return 1;
}

/*****************************************************************************
 *
 *****************************************************************************/
static const luaL_Reg vlclua_playlist_reg[] = {
    { "prev", vlclua_playlist_prev },
    { "next", vlclua_playlist_next },
    { "skip", vlclua_playlist_skip },
    { "play", vlclua_playlist_play },
    { "pause", vlclua_playlist_pause },
    { "stop", vlclua_playlist_stop },
    { "clear", vlclua_playlist_clear },
    { "repeat", vlclua_playlist_repeat }, // repeat is a reserved lua keyword...
    { "repeat_", vlclua_playlist_repeat }, // ... provide repeat_ too.
    { "loop", vlclua_playlist_loop },
    { "random", vlclua_playlist_random },
    { "get_repeat", vlclua_playlist_get_repeat },
    { "get_loop", vlclua_playlist_get_loop },
    { "get_random", vlclua_playlist_get_random },
#if LUA_VERSION_NUM < 502
    { "goto", vlclua_playlist_gotoitem },
#endif
    { "gotoitem", vlclua_playlist_gotoitem },
    { "add", vlclua_playlist_add },
    { "enqueue", vlclua_playlist_enqueue },
    { "get", vlclua_playlist_get },
    { "list", vlclua_playlist_list },
    { "current", vlclua_playlist_current },
    { "current_item", vlclua_playlist_current_item },
    { "sort", vlclua_playlist_sort },
    { "status", vlclua_playlist_status },
    { "delete", vlclua_playlist_delete },
    { "move", vlclua_playlist_move },
    { NULL, NULL }
};

void luaopen_playlist( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_playlist_reg );
    lua_setfield( L, -2, "playlist" );
}
