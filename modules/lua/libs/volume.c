/*****************************************************************************
 * volume.c
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
 * $Id$
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

#include "../vlc.h"
#include "../libs.h"
#include "playlist.h"

/*****************************************************************************
 * Volume related
 *****************************************************************************/
static int vlclua_volume_set( lua_State *L )
{
    playlist_t *p_this = vlclua_get_playlist_internal( L );
    int i_volume = luaL_checkint( L, 1 );
    if( i_volume < 0 )
        i_volume = 0;
    int i_ret = playlist_VolumeSet( p_this, i_volume/(float)AOUT_VOLUME_DEFAULT );
    return vlclua_push_ret( L, i_ret );
}

static int vlclua_volume_get( lua_State *L )
{
    playlist_t *p_this = vlclua_get_playlist_internal( L );
    long i_volume = lroundf(playlist_VolumeGet( p_this ) * AOUT_VOLUME_DEFAULT);
    lua_pushnumber( L, i_volume );
    return 1;
}

static int vlclua_volume_up( lua_State *L )
{
    playlist_t *p_this = vlclua_get_playlist_internal( L );
    float volume;

    playlist_VolumeUp( p_this, luaL_optint( L, 1, 1 ), &volume );
    lua_pushnumber( L, lroundf(volume * AOUT_VOLUME_DEFAULT) );
    return 1;
}

static int vlclua_volume_down( lua_State *L )
{
    playlist_t *p_this = vlclua_get_playlist_internal( L );
    float volume;

    playlist_VolumeDown( p_this, luaL_optint( L, 1, 1 ), &volume );
    lua_pushnumber( L, lroundf(volume * AOUT_VOLUME_DEFAULT) );
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
