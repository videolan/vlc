/*****************************************************************************
 * sd.c: Services discovery related functions
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
 * $Id$
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
#include <vlc_services_discovery.h>
#include <vlc_playlist.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */
#include <lualib.h>     /* Lua libs */

#include "../vlc.h"
#include "../libs.h"
#include "playlist.h"

/*****************************************************************************
 *
 *****************************************************************************/
static int vlclua_sd_get_services_names( lua_State *L )
{
    char **ppsz_longnames;
    char **ppsz_names = vlc_sd_GetNames( &ppsz_longnames );
    if( !ppsz_names )
        return 0;

    char **ppsz_longname = ppsz_longnames;
    char **ppsz_name = ppsz_names;
    lua_settop( L, 0 );
    lua_newtable( L );
    for( ; *ppsz_name; ppsz_name++,ppsz_longname++ )
    {
        lua_pushstring( L, *ppsz_longname );
        lua_setfield( L, -2, *ppsz_name );
        free( *ppsz_name );
        free( *ppsz_longname );
    }
    free( ppsz_names );
    free( ppsz_longnames );
    return 1;
}

static int vlclua_sd_add( lua_State *L )
{
    const char *psz_sd = luaL_checkstring( L, 1 );
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    int i_ret = playlist_ServicesDiscoveryAdd( p_playlist, psz_sd );
    vlclua_release_playlist_internal( p_playlist );
    return vlclua_push_ret( L, i_ret );
}

static int vlclua_sd_remove( lua_State *L )
{
    const char *psz_sd = luaL_checkstring( L, 1 );
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    int i_ret = playlist_ServicesDiscoveryRemove( p_playlist, psz_sd );
    vlclua_release_playlist_internal( p_playlist );
    return vlclua_push_ret( L, i_ret );
}

static int vlclua_sd_is_loaded( lua_State *L )
{
    const char *psz_sd = luaL_checkstring( L, 1 );
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    lua_pushboolean( L, playlist_IsServicesDiscoveryLoaded( p_playlist, psz_sd ));
    vlclua_release_playlist_internal( p_playlist );
    return 1;
}

/*****************************************************************************
 *
 *****************************************************************************/
static const luaL_Reg vlclua_sd_reg[] = {
    { "get_services_names", vlclua_sd_get_services_names },
    { "add", vlclua_sd_add },
    { "remove", vlclua_sd_remove },
    { "is_loaded", vlclua_sd_is_loaded },
    { NULL, NULL }
};

void luaopen_sd( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_sd_reg );
    lua_setfield( L, -2, "sd" );
}
