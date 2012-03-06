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

#include "../vlc.h"
#include "../libs.h"

/*****************************************************************************
 * Stream handling
 *****************************************************************************/
static int vlclua_stream_read( lua_State * );
static int vlclua_stream_readline( lua_State * );
static int vlclua_stream_delete( lua_State * );
static int vlclua_stream_add_filter( lua_State *L );

static const luaL_Reg vlclua_stream_reg[] = {
    { "read", vlclua_stream_read },
    { "readline", vlclua_stream_readline },
    { "addfilter", vlclua_stream_add_filter },
    { NULL, NULL }
};

static int vlclua_stream_new_inner( lua_State *L, stream_t *p_stream )
{
    if( !p_stream )
    {
        lua_pushnil( L );
        lua_pushliteral( L, "Error when opening stream" );
        return 2;
    }

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

static int vlclua_stream_new( lua_State *L )
{
    vlc_object_t * p_this = vlclua_get_this( L );
    const char * psz_url = luaL_checkstring( L, 1 );
    stream_t *p_stream = stream_UrlNew( p_this, psz_url );
    return vlclua_stream_new_inner( L, p_stream );
}

static int vlclua_memory_stream_new( lua_State *L )
{
    vlc_object_t * p_this = vlclua_get_this( L );
    /* FIXME: duplicating the whole buffer is suboptimal. Keeping a reference to the string so that it doesn't get garbage collected would be better */
    char * psz_content = strdup( luaL_checkstring( L, 1 ) );
    stream_t *p_stream = stream_MemoryNew( p_this, (uint8_t *)psz_content, strlen( psz_content ), false );
    return vlclua_stream_new_inner( L, p_stream );
}

static int vlclua_stream_read( lua_State *L )
{
    int i_read;
    stream_t **pp_stream = (stream_t **)luaL_checkudata( L, 1, "stream" );
    int n = luaL_checkint( L, 2 );
    uint8_t *p_read = malloc( n );
    if( !p_read ) return vlclua_error( L );

    i_read = stream_Read( *pp_stream, p_read, n );
    if( i_read > 0 )
        lua_pushlstring( L, (const char *)p_read, i_read );
    else
        lua_pushnil( L );
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

static int vlclua_stream_add_filter( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );

    /* Make sure that we have 1 argument (+ 1 object) */
    lua_settop( L, 2 );

    stream_t **pp_stream = (stream_t **)luaL_checkudata( L, 1, "stream" );
    if( !*pp_stream ) return vlclua_error( L );
    const char *psz_filter = NULL;

    if( lua_isstring( L, 2 ) )
        psz_filter = lua_tostring( L, 2 );

    if( !psz_filter || !*psz_filter )
    {
        msg_Dbg( p_this, "adding all automatic stream filters" );
        while( true )
        {
            /* Add next automatic stream */
            stream_t *p_filtered = stream_FilterNew( *pp_stream, NULL );
            if( !p_filtered )
                break;
            else
            {
                msg_Dbg( p_this, "inserted an automatic stream filter" );
                *pp_stream = p_filtered;
            }
        }
        luaL_getmetatable( L, "stream" );
        lua_setmetatable( L, 1 );
    }
    else
    {
        /* Add a named filter */
        stream_t *p_filter = stream_FilterNew( *pp_stream, psz_filter );
        if( !p_filter )
            msg_Dbg( p_this, "Unable to open requested stream filter '%s'",
                     psz_filter );
        else
        {
            *pp_stream = p_filter;
            luaL_getmetatable( L, "stream" );
            lua_setmetatable( L, 1 );
        }
    }

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
    lua_pushcfunction( L, vlclua_memory_stream_new );
    lua_setfield( L, -2, "memory_stream" );
}
