/*****************************************************************************
 * acl.c: Access list related functions
 *****************************************************************************
 * Copyright (C) 2007-2010 the VideoLAN team
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
#include <vlc_acl.h>

#include "../vlc.h"
#include "../libs.h"

/*****************************************************************************
 *
 *****************************************************************************/
static int vlclua_acl_create_inner( lua_State *, vlc_acl_t * );
static int vlclua_acl_delete( lua_State * );
static int vlclua_acl_check( lua_State * );
static int vlclua_acl_duplicate( lua_State * );
static int vlclua_acl_add_host( lua_State * );
static int vlclua_acl_add_net( lua_State * );
static int vlclua_acl_load_file( lua_State * );

static const luaL_Reg vlclua_acl_reg[] = {
    { "check", vlclua_acl_check },
    { "duplicate", vlclua_acl_duplicate },
    { "add_host", vlclua_acl_add_host },
    { "add_net", vlclua_acl_add_net },
    { "load_file", vlclua_acl_load_file },
    { NULL, NULL }
};

static int vlclua_acl_create( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    bool b_allow = luaL_checkboolean( L, 1 );
    vlc_acl_t *p_acl = ACL_Create( p_this, b_allow );
    return vlclua_acl_create_inner( L, p_acl );
}

static int vlclua_acl_create_inner( lua_State *L, vlc_acl_t *p_acl )
{
    if( !p_acl )
        return luaL_error( L, "ACL creation failed." );

    vlc_acl_t **pp_acl = lua_newuserdata( L, sizeof( vlc_acl_t * ) );
    *pp_acl = p_acl;

    if( luaL_newmetatable( L, "acl" ) )
    {
        lua_newtable( L );
        luaL_register( L, NULL, vlclua_acl_reg );
        lua_setfield( L, -2, "__index" );
        lua_pushcfunction( L, vlclua_acl_delete );
        lua_setfield( L, -2, "__gc" );
    }

    lua_setmetatable( L, -2 );
    return 1;
}

static int vlclua_acl_delete( lua_State *L )
{
    vlc_acl_t **pp_acl = (vlc_acl_t**)luaL_checkudata( L, 1, "acl" );
    ACL_Destroy( *pp_acl );
    return 0;
}

static int vlclua_acl_check( lua_State *L )
{
    vlc_acl_t **pp_acl = (vlc_acl_t**)luaL_checkudata( L, 1, "acl" );
    const char *psz_ip = luaL_checkstring( L, 2 );
    lua_pushinteger( L, ACL_Check( *pp_acl, psz_ip ) );
    return 1;
}

static int vlclua_acl_duplicate( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_acl_t **pp_acl = (vlc_acl_t**)luaL_checkudata( L, 1, "acl" );
    vlc_acl_t *p_acl_new = ACL_Duplicate( p_this, *pp_acl );
    return vlclua_acl_create_inner( L, p_acl_new );
}

static int vlclua_acl_add_host( lua_State *L )
{
    vlc_acl_t **pp_acl = (vlc_acl_t**)luaL_checkudata( L, 1, "acl" );
    const char *psz_ip = luaL_checkstring( L, 2 );
    bool b_allow = luaL_checkboolean( L, 3 );
    lua_pushinteger( L, ACL_AddHost( *pp_acl, psz_ip, b_allow ) );
    return 1;
}

static int vlclua_acl_add_net( lua_State *L )
{
    vlc_acl_t **pp_acl = (vlc_acl_t**)luaL_checkudata( L, 1, "acl" );
    const char *psz_ip = luaL_checkstring( L, 2 );
    int i_len = luaL_checkint( L, 3 );
    bool b_allow = luaL_checkboolean( L, 4 );
    lua_pushinteger( L, ACL_AddNet( *pp_acl, psz_ip, i_len, b_allow ) );
    return 1;
}

static int vlclua_acl_load_file( lua_State *L )
{
    vlc_acl_t **pp_acl = (vlc_acl_t**)luaL_checkudata( L, 1, "acl" );
    const char *psz_path = luaL_checkstring( L, 2 );
    lua_pushinteger( L, ACL_LoadFile( *pp_acl, psz_path ) );
    return 1;
}

void luaopen_acl( lua_State *L )
{
    lua_pushcfunction( L, vlclua_acl_create );
    lua_setfield( L, -2, "acl" );
}
