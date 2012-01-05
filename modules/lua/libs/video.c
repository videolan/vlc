/*****************************************************************************
 * video.c: Generic lua interface functions
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
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

#include <vlc_vout.h>

#include "../vlc.h"
#include "../libs.h"
#include "input.h"
#include "variables.h"

/*****************************************************************************
 * Vout control
 *****************************************************************************/
static int vlclua_fullscreen( lua_State *L )
{
    vout_thread_t *p_vout;
    int i_ret;

    input_thread_t * p_input = vlclua_get_input_internal( L );
    if( !p_input ) return vlclua_error( L );

    p_vout = input_GetVout( p_input );
    if( !p_vout )
    {
        vlc_object_release( p_input );
        return vlclua_error( L );
    }

    i_ret = vlclua_var_toggle_or_set( L, p_vout, "fullscreen" );

    vlc_object_release( p_vout );
    vlc_object_release( p_input );
    return i_ret;
}

/*****************************************************************************
 *
 *****************************************************************************/
static const luaL_Reg vlclua_video_reg[] = {
    { "fullscreen", vlclua_fullscreen },
    { NULL, NULL }
};

void luaopen_video( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_video_reg );
    lua_setfield( L, -2, "video" );
}
