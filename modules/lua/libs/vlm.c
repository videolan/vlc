/*****************************************************************************
 * vlm.c: Generic lua VLM wrapper
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
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
#include <vlc_vlm.h>

#include "../vlc.h"
#include "../libs.h"

/*****************************************************************************
 *
 *****************************************************************************/
#ifdef ENABLE_VLM
static int vlclua_vlm_delete( lua_State * );
static int vlclua_vlm_execute_command( lua_State * );

static const luaL_Reg vlclua_vlm_reg[] = {
    { "execute_command", vlclua_vlm_execute_command },
    { NULL, NULL }
};

static int vlclua_vlm_new( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlm_t *p_vlm = vlm_New( vlc_object_instance(p_this), NULL );
    if( !p_vlm )
        return luaL_error( L, "Cannot start VLM." );

    vlm_t **pp_vlm = lua_newuserdata( L, sizeof( vlm_t * ) );
    *pp_vlm = p_vlm;

    if( luaL_newmetatable( L, "vlm" ) )
    {
        lua_newtable( L );
        luaL_register( L, NULL, vlclua_vlm_reg );
        lua_setfield( L, -2, "__index" );
        lua_pushcfunction( L, vlclua_vlm_delete );
        lua_setfield( L, -2, "__gc" );
    }

    lua_setmetatable( L, -2 );
    return 1;
}

static int vlclua_vlm_delete( lua_State *L )
{
    vlm_t **pp_vlm = (vlm_t**)luaL_checkudata( L, 1, "vlm" );
    vlm_Delete( *pp_vlm );
    return 0;
}

static void push_message( lua_State *L, vlm_message_t *message )
{
    lua_createtable( L, 0, 2 );
    lua_pushstring( L, message->psz_name );
    lua_setfield( L, -2, "name" );
    if( message->i_child > 0 )
    {
        int i;
        lua_createtable( L, message->i_child, 0 );
        for( i = 0; i < message->i_child; i++ )
        {
            lua_pushinteger( L, i+1 );
            push_message( L, message->child[i] );
            lua_settable( L, -3 );
        }
        lua_setfield( L, -2, "children" );
    }
    if ( message->psz_value )
    {
        lua_pushstring( L, message->psz_value );
        lua_setfield( L, -2, "value" );
    }
}

static int vlclua_vlm_execute_command( lua_State *L )
{
    vlm_t **pp_vlm = (vlm_t**)luaL_checkudata( L, 1, "vlm" );
    const char *psz_command = luaL_checkstring( L, 2 );
    vlm_message_t *message;
    int i_ret;
    i_ret = vlm_ExecuteCommand( *pp_vlm, psz_command, &message );
    lua_settop( L, 0 );
    push_message( L, message );
    vlm_MessageDelete( message );
    return 1 + vlclua_push_ret( L, i_ret );
}

#else
static int vlclua_vlm_new( lua_State *L )
{
    return luaL_error( L, "Cannot start VLM because it was disabled when compiling VLC." );
}
#endif

/*****************************************************************************
 *
 *****************************************************************************/
void luaopen_vlm( lua_State *L )
{
    lua_pushcfunction( L, vlclua_vlm_new );
    lua_setfield( L, -2, "vlm" );
}
