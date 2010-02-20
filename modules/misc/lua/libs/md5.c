/*****************************************************************************
 * md5.c: md5 hashing functions
 *****************************************************************************
 * Copyright (C) 2010 Antoine Cellerier
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
#include <vlc_md5.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */

#include "../vlc.h"
#include "../libs.h"

/*****************************************************************************
 *
 *****************************************************************************/
static int vlclua_md5_create( lua_State * );
static int vlclua_md5_add( lua_State * );
static int vlclua_md5_end( lua_State * );
static int vlclua_md5_hash( lua_State * );

static const luaL_Reg vlclua_md5_reg[] = {
    { "add", vlclua_md5_add },
    { "end", vlclua_md5_end },
    { "end_", vlclua_md5_end },
    { "hash", vlclua_md5_hash },
    { NULL, NULL }
};

static int vlclua_md5_create( lua_State *L )
{
    if( lua_gettop( L ) )
    {
        /* If optional first argument is given, and it's a string, just
         * return the string's hash */
        const char *psz_str = luaL_checkstring( L, 1 );
        struct md5_s md5;
        InitMD5( &md5 );
        AddMD5( &md5, psz_str, strlen( psz_str ) );
        EndMD5( &md5 );
        char *psz_hash = psz_md5_hash( &md5 );
        lua_pushstring( L, psz_hash );
        free( psz_hash );
        return 1;
    }

    struct md5_s *p_md5 = lua_newuserdata( L, sizeof( struct md5_s ) );
    InitMD5( p_md5 );

    if( luaL_newmetatable( L, "md5" ) )
    {
        lua_newtable( L );
        luaL_register( L, NULL, vlclua_md5_reg );
        lua_setfield( L, -2, "__index" );
    }

    lua_setmetatable( L, -2 );
    return 1;
}

static int vlclua_md5_add( lua_State *L )
{
    struct md5_s *p_md5 = (struct md5_s *)luaL_checkudata( L, 1, "md5" );
    if( !lua_isstring( L, 2 ) )
        luaL_error( L, "2nd argument should be a string" );
    size_t i_len = 0;
    const char *psz_str = lua_tolstring( L, 2, &i_len );
    if( !psz_str )
        vlclua_error( L );

    AddMD5( p_md5, psz_str, i_len );

    return 0;
}

static int vlclua_md5_end( lua_State *L )
{
    struct md5_s *p_md5 = (struct md5_s *)luaL_checkudata( L, 1, "md5" );
    EndMD5( p_md5 );
    return 0;
}

static int vlclua_md5_hash( lua_State *L )
{
    struct md5_s *p_md5 = (struct md5_s *)luaL_checkudata( L, 1, "md5" );
    char *psz_hash = psz_md5_hash( p_md5 );
    lua_pushstring( L, psz_hash );
    free( psz_hash );
    return 1;
}

void luaopen_md5( lua_State *L )
{
    lua_pushcfunction( L, vlclua_md5_create );
    lua_setfield( L, -2, "md5" );
}
