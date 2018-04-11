/*****************************************************************************
 * errno.c
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

#include "../vlc.h"
#include "../libs.h"

void luaopen_errno( lua_State *L )
{
    lua_newtable( L );

#define ADD_ERRNO_VALUE( value )  \
    lua_pushinteger( L, value );  \
    lua_setfield( L, -2, #value );

    ADD_ERRNO_VALUE( ENOENT );
    ADD_ERRNO_VALUE( EEXIST );
    ADD_ERRNO_VALUE( EACCES );
    ADD_ERRNO_VALUE( EINVAL );

#undef ADD_ERRNO_VALUE

    lua_setfield( L, -2, "errno" );
}
