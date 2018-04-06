/*****************************************************************************
 * misc.c
 *****************************************************************************
 * Copyright (C) 2007-2018 the VideoLAN team
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>

#include <vlc_common.h>
#include <vlc_fs.h>

#include "../vlc.h"
#include "../libs.h"

static int vlclua_mkdir( lua_State *L )
{
    if( lua_gettop( L ) < 2 ) return vlclua_error( L );

    const char* psz_dir = luaL_checkstring( L, 1 );
    const char* psz_mode = luaL_checkstring( L, 2 );
    if ( !psz_dir || !psz_mode )
        return vlclua_error( L );
    int i_res = vlc_mkdir( psz_dir, strtoul( psz_mode, NULL, 0 ) );
    lua_pushboolean( L, i_res == 0 || errno == EEXIST );
    return 1;
}

static const luaL_Reg vlclua_io_reg[] = {
    { "mkdir", vlclua_mkdir },

    { NULL, NULL }
};

void luaopen_vlcio( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_io_reg );
    lua_setfield( L, -2, "io" );
}
