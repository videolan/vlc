/*****************************************************************************
 * strings.c
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
 * $Id$
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
#include <vlc_aout.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */

#include "../vlc.h"
#include "../libs.h"

/*****************************************************************************
 * String transformations
 *****************************************************************************/
static int vlclua_decode_uri( lua_State *L )
{
    int i_top = lua_gettop( L );
    int i;
    for( i = 1; i <= i_top; i++ )
    {
        const char *psz_cstring = luaL_checkstring( L, 1 );
        char *psz_string = strdup( psz_cstring );
        lua_remove( L, 1 ); /* remove elements to prevent being limited by
                             * the stack's size (this function will work with
                             * up to (stack size - 1) arguments */
        decode_URI( psz_string );
        lua_pushstring( L, psz_string );
        free( psz_string );
    }
    return i_top;
}

static int vlclua_encode_uri_component( lua_State *L )
{
    int i_top = lua_gettop( L );
    int i;
    for( i = 1; i <= i_top; i++ )
    {
        const char *psz_cstring = luaL_checkstring( L, 1 );
        char *psz_string = encode_URI_component( psz_cstring );
        lua_remove( L,1 );
        lua_pushstring( L, psz_string );
        free( psz_string );
    }
    return i_top;
}

static int vlclua_resolve_xml_special_chars( lua_State *L )
{
    int i_top = lua_gettop( L );
    int i;
    for( i = 1; i <= i_top; i++ )
    {
        const char *psz_cstring = luaL_checkstring( L, 1 );
        char *psz_string = strdup( psz_cstring );
        lua_remove( L, 1 ); /* remove elements to prevent being limited by
                             * the stack's size (this function will work with
                             * up to (stack size - 1) arguments */
        resolve_xml_special_chars( psz_string );
        lua_pushstring( L, psz_string );
        free( psz_string );
    }
    return i_top;
}

static int vlclua_convert_xml_special_chars( lua_State *L )
{
    int i_top = lua_gettop( L );
    int i;
    for( i = 1; i <= i_top; i++ )
    {
        char *psz_string = convert_xml_special_chars( luaL_checkstring(L,1) );
        lua_remove( L, 1 );
        lua_pushstring( L, psz_string );
        free( psz_string );
    }
    return i_top;
}

static int vlclua_iconv( lua_State *L )
{
    int i_ret;
    size_t i_out_bytes, i_in_bytes;
    char *psz_output, *psz_original;

    if( lua_gettop( L ) < 3 ) return vlclua_error( L );

    const char *psz_input = luaL_checklstring( L, 3, &i_in_bytes );

    if( i_in_bytes == 0 ) return vlclua_error( L );
    vlc_iconv_t iconv_handle = vlc_iconv_open( luaL_checkstring(L, 1),
                                             luaL_checkstring(L, 2) );

    if( iconv_handle == (vlc_iconv_t)-1 )
       return vlclua_error( L );
    i_out_bytes = 4 * i_in_bytes;
    psz_output = psz_original = malloc( i_out_bytes + 1 );
    i_ret = vlc_iconv( iconv_handle, &psz_input ,
                       &i_in_bytes, &psz_output, &i_out_bytes );
    *psz_output = '\0';

    lua_pushstring( L, psz_original );
    vlc_iconv_close( iconv_handle );
    free( psz_original );
    return 1;
}

/*****************************************************************************
 *
 *****************************************************************************/
static const luaL_Reg vlclua_strings_reg[] = {
    { "decode_uri", vlclua_decode_uri },
    { "encode_uri_component", vlclua_encode_uri_component },
    { "resolve_xml_special_chars", vlclua_resolve_xml_special_chars },
    { "convert_xml_special_chars", vlclua_convert_xml_special_chars },
    { "iconv", vlclua_iconv },
    { NULL, NULL }
};

void luaopen_strings( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_strings_reg );
    lua_setfield( L, -2, "strings" );
}
