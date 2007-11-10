/*****************************************************************************
 * acl.c: Access list related functions
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
#include <vlc_acl.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */
#include <lualib.h>     /* Lua libs */

#include "vlc.h"

/*****************************************************************************
 *
 *****************************************************************************/
int vlclua_acl_create( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_bool_t b_allow = luaL_checkboolean( L, 1 ) ? VLC_TRUE : VLC_FALSE;
    vlc_acl_t *p_acl = ACL_Create( p_this, b_allow );
    if( !p_acl )
        return luaL_error( L, "ACL creation failed." );
    lua_pushlightuserdata( L, p_acl ); /* FIXME */
    return 1;
}

int vlclua_acl_delete( lua_State *L )
{
    vlc_acl_t *p_acl = (vlc_acl_t*)luaL_checklightuserdata( L, 1 );
    ACL_Destroy( p_acl );
    return 0;
}

int vlclua_acl_check( lua_State *L )
{
    vlc_acl_t *p_acl = (vlc_acl_t*)luaL_checklightuserdata( L, 1 );
    const char *psz_ip = luaL_checkstring( L, 2 );
    lua_pushinteger( L, ACL_Check( p_acl, psz_ip ) );
    return 1;
}

int vlclua_acl_duplicate( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_acl_t *p_acl = (vlc_acl_t*)luaL_checklightuserdata( L, 1 );
    vlc_acl_t *p_acl_new = ACL_Duplicate( p_this, p_acl );
    lua_pushlightuserdata( L, p_acl_new );
    return 1;
}

int vlclua_acl_add_host( lua_State *L )
{
    vlc_acl_t *p_acl = (vlc_acl_t*)luaL_checklightuserdata( L, 1 );
    const char *psz_ip = luaL_checkstring( L, 2 );
    vlc_bool_t b_allow = luaL_checkboolean( L, 3 ) ? VLC_TRUE : VLC_FALSE;
    lua_pushinteger( L, ACL_AddHost( p_acl, psz_ip, b_allow ) );
    return 1;
}

int vlclua_acl_add_net( lua_State *L )
{
    vlc_acl_t *p_acl = (vlc_acl_t*)luaL_checklightuserdata( L, 1 );
    const char *psz_ip = luaL_checkstring( L, 2 );
    int i_len = luaL_checkint( L, 3 );
    vlc_bool_t b_allow = luaL_checkboolean( L, 4 ) ? VLC_TRUE : VLC_FALSE;
    lua_pushinteger( L, ACL_AddNet( p_acl, psz_ip, i_len, b_allow ) );
    return 1;
}

int vlclua_acl_load_file( lua_State *L )
{
    vlc_acl_t *p_acl = (vlc_acl_t*)luaL_checklightuserdata( L, 1 );
    const char *psz_path = luaL_checkstring( L, 2 );
    lua_pushinteger( L, ACL_LoadFile( p_acl, psz_path ) );
    return 1;
}
