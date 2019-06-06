/*****************************************************************************
 * messages.c
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
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
 * Messaging facilities
 *****************************************************************************/
static int vlclua_msg_dbg( lua_State *L )
{
    int i_top = lua_gettop( L );
    vlc_object_t *p_this = vlclua_get_this( L );
    int i;
    for( i = 1; i <= i_top; i++ )
        msg_Dbg( p_this, "%s", luaL_checkstring( L, i ) );
    return 0;
}

static int vlclua_msg_warn( lua_State *L )
{
    int i_top = lua_gettop( L );
    vlc_object_t *p_this = vlclua_get_this( L );
    int i;
    for( i = 1; i <= i_top; i++ )
        msg_Warn( p_this, "%s", luaL_checkstring( L, i ) );
    return 0;
}

static int vlclua_msg_err( lua_State *L )
{
    int i_top = lua_gettop( L );
    vlc_object_t *p_this = vlclua_get_this( L );
    int i;
    for( i = 1; i <= i_top; i++ )
        msg_Err( p_this, "%s", luaL_checkstring( L, i ) );
    return 0;
}

static int vlclua_msg_info( lua_State *L )
{
    int i_top = lua_gettop( L );
    vlc_object_t *p_this = vlclua_get_this( L );
    int i;
    for( i = 1; i <= i_top; i++ )
        msg_Info( p_this, "%s", luaL_checkstring( L, i ) );
    return 0;
}

/*****************************************************************************
 *
 *****************************************************************************/
static const luaL_Reg vlclua_msg_reg[] = {
    { "dbg", vlclua_msg_dbg },
    { "warn", vlclua_msg_warn },
    { "err", vlclua_msg_err },
    { "info", vlclua_msg_info },
    { NULL, NULL }
};

void luaopen_msg( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_msg_reg );
    lua_setfield( L, -2, "msg" );
}
