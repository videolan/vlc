/*****************************************************************************
 * gettext.c
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
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

#include "../vlc.h"
#include "../libs.h"

/*****************************************************************************
 * Libvlc gettext support
 *****************************************************************************/
static int vlclua_gettext( lua_State *L )
{
    lua_pushstring( L, _( luaL_checkstring( L, 1 ) ) );
    return 1;
}

static int vlclua_gettext_noop( lua_State *L )
{
    lua_settop( L, 1 );
    return 1;
}

/*****************************************************************************
 *
 *****************************************************************************/
static const luaL_Reg vlclua_gettext_reg[] = {
    { "_", vlclua_gettext },
    { "N_", vlclua_gettext_noop },

    { NULL, NULL }
};

void luaopen_gettext( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_gettext_reg );
    lua_setfield( L, -2, "gettext" );
}
