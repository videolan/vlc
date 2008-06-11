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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_meta.h>
#include <vlc_charset.h>
#include <vlc_aout.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */
#include <lualib.h>     /* Lua libs */

#include "../vlc.h"
#include "../libs.h"

/*****************************************************************************
 * Volume related
 *****************************************************************************/
static int vlclua_volume_set( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    int i_volume = luaL_checkint( L, 1 );
    /* Do we need to check that i_volume is in the AOUT_VOLUME_MIN->MAX range?*/
    return vlclua_push_ret( L, aout_VolumeSet( p_this, i_volume ) );
}

static int vlclua_volume_get( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    audio_volume_t i_volume;
    if( aout_VolumeGet( p_this, &i_volume ) == VLC_SUCCESS )
        lua_pushnumber( L, i_volume );
    else
        lua_pushnil( L );
    return 1;
}

static int vlclua_volume_up( lua_State *L )
{
    audio_volume_t i_volume;
    aout_VolumeUp( vlclua_get_this( L ),
                   luaL_optint( L, 1, 1 ),
                   &i_volume );
    lua_pushnumber( L, i_volume );
    return 1;
}

static int vlclua_volume_down( lua_State *L )
{
    audio_volume_t i_volume;
    aout_VolumeDown( vlclua_get_this( L ),
                     luaL_optint( L, 1, 1 ),
                     &i_volume );
    lua_pushnumber( L, i_volume );
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
