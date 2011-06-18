/*****************************************************************************
 * equalizer.c
 *****************************************************************************
 * Copyright (C) 2011 the VideoLAN team
 * $Id$
 *
 * Authors: Akash Mehrotra < mehrotra <dot> akash <at> gmail <dot> com >
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
#include <vlc_aout.h>
#include <vlc_input.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */

#include "input.h"
#include "../libs.h"

/*****************************************************************************
* Get the preamp level
*****************************************************************************/
static int vlclua_preamp_get( lua_State *L )
{
    input_thread_t *p_input = vlclua_get_input_internal( L );
    if( p_input )
    {
        aout_instance_t *p_aout = input_GetAout( p_input );
        float preamp = var_GetFloat( p_aout, "equalizer-preamp");
        lua_pushnumber( L, preamp );

        vlc_object_release( p_aout );
        vlc_object_release( p_input );
        return 1;
    }
    return 0;
}

/*****************************************************************************
* Set the preamp level
*****************************************************************************/
static int vlclua_preamp_set( lua_State *L )
{
    input_thread_t *p_input = vlclua_get_input_internal( L );
    if( p_input )
    {
        aout_instance_t *p_aout = input_GetAout( p_input );
        float preamp = luaL_checknumber( L, 1 );
        var_SetFloat( p_aout, "equalizer-preamp",preamp);
        lua_pushnumber( L, preamp );

        vlc_object_release( p_aout );
        vlc_object_release( p_input );
        return 1;
    }
    return 0;
}

static const luaL_Reg vlclua_equalizer_reg[] = {
    { "preampget", vlclua_preamp_get },
    { "preampset", vlclua_preamp_set },
    { NULL, NULL }
};

void luaopen_equalizer( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_equalizer_reg );
    lua_setfield( L, -2, "equalizer" );
}
