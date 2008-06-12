/*****************************************************************************
 * stream.c: stream functions
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

#include "../vlc.h"
#include "../libs.h"

/*****************************************************************************
 * Stream handling
 *****************************************************************************/
static int vlclua_stream_read( lua_State * );
static int vlclua_stream_readline( lua_State * );
static int vlclua_stream_delete( lua_State * );

static const luaL_Reg vlclua_stream_reg[] = {
    { "read", vlclua_stream_read },
    { "readline", vlclua_stream_readline },
    { NULL, NULL }
};

static int vlclua_stream_new( lua_State *L )
{
    vlc_object_t * p_this = vlclua_get_this( L );
    stream_t * p_stream;
    const char * psz_url;
    psz_url = luaL_checkstring( L, 1 );
    p_stream = stream_UrlNew( p_this, psz_url );
    if( !p_stream )
        return luaL_error( L, "Error when opening url: `%s'", psz_url );

    stream_t **pp_stream = lua_newuserdata( L, sizeof( stream_t * ) );
    *pp_stream = p_stream;

    if( luaL_newmetatable( L, "stream" ) )
    {
        lua_newtable( L );
        luaL_register( L, NULL, vlclua_stream_reg );
        lua_setfield( L, -2, "__index" );
        lua_pushcfunction( L, vlclua_stream_delete );
        lua_setfield( L, -2, "__gc" );
    }

    lua_setmetatable( L, -2 );
    return 1;
}

static int vlclua_stream_read( lua_State *L )
{
    int i_read;
    stream_t **pp_stream = (stream_t **)luaL_checkudata( L, 1, "stream" );
    int n = luaL_checkint( L, 2 );
    uint8_t *p_read = malloc( n );
    if( !p_read ) return vlclua_error( L );
    i_read = stream_Read( *pp_stream, p_read, n );
    lua_pushlstring( L, (const char *)p_read, i_read );
    free( p_read );
    return 1;
}

static int vlclua_stream_readline( lua_State *L )
{
    stream_t **pp_stream = (stream_t **)luaL_checkudata( L, 1, "stream" );
    char *psz_line = stream_ReadLine( *pp_stream );
    if( psz_line )
    {
        lua_pushstring( L, psz_line );
        free( psz_line );
    }
    else
        lua_pushnil( L );
    return 1;
}

static int vlclua_stream_delete( lua_State *L )
{
    stream_t **pp_stream = (stream_t **)luaL_checkudata( L, 1, "stream" );
    stream_Delete( *pp_stream );
    return 0;
}

/*****************************************************************************
 *
 *****************************************************************************/
void luaopen_stream( lua_State *L )
{
    lua_pushcfunction( L, vlclua_stream_new );
    lua_setfield( L, -2, "stream" );
}
