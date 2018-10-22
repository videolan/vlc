/*****************************************************************************
 * volume.c
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
 *
 * Authors: Antoine Cellerier <dionoea at videolan tod org>
 *          Pierre d'Herbemont <pdherbemont # videolan.org>
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

#include <math.h>
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_meta.h>
#include <vlc_playlist.h>
#include <vlc_player.h>
#include <vlc_aout.h>

#include "../vlc.h"
#include "../libs.h"

/*****************************************************************************
 * Volume related
 *****************************************************************************/
static int vlclua_volume_set(lua_State *L)
{
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);
    vlc_player_t *player = vlc_playlist_GetPlayer(playlist);

    int i_volume = luaL_checkinteger(L, 1);
    if (i_volume < 0)
        i_volume = 0;

    float volume = i_volume / (float) AOUT_VOLUME_DEFAULT;
    int ret = vlc_player_aout_SetVolume(player, volume);
    return vlclua_push_ret(L, ret);
}

static int vlclua_volume_get(lua_State *L)
{
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);
    vlc_player_t *player = vlc_playlist_GetPlayer(playlist);

    float volume = vlc_player_aout_GetVolume(player);

    long i_volume = lroundf(volume * AOUT_VOLUME_DEFAULT);
    lua_pushnumber(L, i_volume);
    return 1;
}

static int vlclua_volume_up(lua_State *L)
{
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);
    vlc_player_t *player = vlc_playlist_GetPlayer(playlist);

    float volume;
    int steps = luaL_optinteger(L, 1, 1);
    int res = vlc_player_aout_IncrementVolume(player, steps, &volume);

    long i_volume = res == VLC_SUCCESS ? lroundf(volume * AOUT_VOLUME_DEFAULT)
                                       : 0;
    lua_pushnumber(L, i_volume);
    return 1;
}

static int vlclua_volume_down(lua_State *L)
{
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);
    vlc_player_t *player = vlc_playlist_GetPlayer(playlist);

    float volume;
    int steps = luaL_optinteger(L, 1, 1);
    int res = vlc_player_aout_DecrementVolume(player, steps, &volume);

    long i_volume = res == VLC_SUCCESS ? lroundf(volume * AOUT_VOLUME_DEFAULT)
                                       : 0;
    lua_pushnumber(L, i_volume);
    return 1;
}

/*****************************************************************************
 *
 *****************************************************************************/
static const luaL_Reg vlclua_volume_reg[] = {
    { "get", vlclua_volume_get },
    { "set", vlclua_volume_set },
    { "up", vlclua_volume_up },
    { "down", vlclua_volume_down },
    { NULL, NULL }
};

void luaopen_volume( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_volume_reg );
    lua_setfield( L, -2, "volume" );
}
