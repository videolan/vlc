/*****************************************************************************
 * objects.c: Generic lua VLM wrapper
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
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

#include <vlc/vlc.h>
#include <vlc_vlm.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */
#include <lualib.h>     /* Lua libs */

#include "vlc.h"

/*****************************************************************************
 *
 *****************************************************************************/
int vlclua_vlm_new( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlm_t *p_vlm = vlm_New( p_this );
    if( !p_vlm )
        return luaL_error( L, "Cannot start VLM." );
    __vlclua_push_vlc_object( L, (vlc_object_t*)p_vlm, NULL );
    return 1;
}

int vlclua_vlm_delete( lua_State *L )
{
    vlm_t *p_vlm = (vlm_t*)vlclua_checkobject( L, 1, VLC_OBJECT_VLM );
    vlm_Delete( p_vlm );
    return 0;
}

void push_message( lua_State *L, vlm_message_t *message );
void push_message( lua_State *L, vlm_message_t *message )
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
    else
    {
        lua_pushstring( L, message->psz_value );
        lua_setfield( L, -2, "value" );
    }
}

int vlclua_vlm_execute_command( lua_State *L )
{
    vlm_t *p_vlm = (vlm_t*)vlclua_checkobject( L, 1, VLC_OBJECT_VLM );
    const char *psz_command = luaL_checkstring( L, 2 );
    vlm_message_t *message;
    vlm_ExecuteCommand( p_vlm, psz_command, &message );
    lua_settop( L, 0 );
    push_message( L, message );
    vlm_MessageDelete( message );
    return 1;
}
