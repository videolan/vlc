/*****************************************************************************
 * xml.c: XML related functions
 *****************************************************************************
 * Copyright (C) 2010 Antoine Cellerier
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
#include <vlc_xml.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */

#include "../vlc.h"
#include "../libs.h"

/*****************************************************************************
 * XML
 *****************************************************************************/
static int vlclua_xml_create_reader( lua_State * );
static int vlclua_xml_load_catalog( lua_State * );
static int vlclua_xml_add_catalog( lua_State * );

static const luaL_Reg vlclua_xml_reg[] = {
    { "create_reader", vlclua_xml_create_reader },
    { "load_catalog", vlclua_xml_load_catalog },
    { "add_catalog", vlclua_xml_add_catalog },
    { NULL, NULL }
};

static int vlclua_xml_delete( lua_State *L )
{
    xml_t *p_xml = *(xml_t**)luaL_checkudata( L, 1, "xml" );
    xml_Delete( p_xml );
    return 0;
}

static int vlclua_xml_create( lua_State *L )
{
    xml_t *p_xml = xml_Create( vlclua_get_this( L ) );
    if( !p_xml )
        return luaL_error( L, "XML module creation failed." );

    xml_t **pp_xml = lua_newuserdata( L, sizeof( xml_t * ) );
    *pp_xml = p_xml;

    if( luaL_newmetatable( L, "xml" ) )
    {
        lua_newtable( L );
        luaL_register( L, NULL, vlclua_xml_reg );
        lua_setfield( L, -2, "__index" );
        lua_pushcfunction( L, vlclua_xml_delete );
        lua_setfield( L, -2, "__gc" );
    }

    lua_setmetatable( L, -2 );
    return 1;
}

static int vlclua_xml_load_catalog( lua_State *L )
{
    xml_t *p_xml = *(xml_t**)luaL_checkudata( L, 1, "xml" );
    const char *psz_catalog = luaL_checkstring( L, 2 );
    xml_CatalogLoad( p_xml, psz_catalog );
    return 0;
}

static int vlclua_xml_add_catalog( lua_State *L )
{
    xml_t *p_xml = *(xml_t**)luaL_checkudata( L, 1, "xml" );
    const char *psz_str1 = luaL_checkstring( L, 2 );
    const char *psz_str2 = luaL_checkstring( L, 3 );
    const char *psz_str3 = luaL_checkstring( L, 4 );
    xml_CatalogAdd( p_xml, psz_str1, psz_str2, psz_str3 );
    return 0;
}

/*****************************************************************************
 * XML Reader
 *****************************************************************************/
static int vlclua_xml_reader_read( lua_State * );
static int vlclua_xml_reader_node_type( lua_State * );
static int vlclua_xml_reader_name( lua_State * );
static int vlclua_xml_reader_value( lua_State * );
static int vlclua_xml_reader_next_attr( lua_State * );
static int vlclua_xml_reader_use_dtd( lua_State * );

static const luaL_Reg vlclua_xml_reader_reg[] = {
    { "read", vlclua_xml_reader_read },
    { "node_type", vlclua_xml_reader_node_type },
    { "name", vlclua_xml_reader_name },
    { "value", vlclua_xml_reader_value },
    { "next_attr", vlclua_xml_reader_next_attr },
    { "use_dtd", vlclua_xml_reader_use_dtd },
    { NULL, NULL }
};

static int vlclua_xml_reader_delete( lua_State *L )
{
    xml_reader_t *p_reader = *(xml_reader_t**)luaL_checkudata( L, 1, "xml_reader" );
    xml_ReaderDelete( p_reader->p_xml, p_reader );
    return 0;
}

static int vlclua_xml_create_reader( lua_State *L )
{
    xml_t *p_xml = *(xml_t**)luaL_checkudata( L, 1, "xml" );
    stream_t *p_stream = *(stream_t **)luaL_checkudata( L, 2, "stream" );

    xml_reader_t *p_reader = xml_ReaderCreate( p_xml, p_stream );
    if( !p_reader )
        return luaL_error( L, "XML reader creation failed." );

    xml_reader_t **pp_reader = lua_newuserdata( L, sizeof( xml_reader_t * ) );
    *pp_reader = p_reader;

    if( luaL_newmetatable( L, "xml_reader" ) )
    {
        lua_newtable( L );
        luaL_register( L, NULL, vlclua_xml_reader_reg );
        lua_setfield( L, -2, "__index" );
        lua_pushcfunction( L, vlclua_xml_reader_delete );
        lua_setfield( L, -2, "__gc" );
    }

    lua_setmetatable( L, -2 );
    return 1;
}

static int vlclua_xml_reader_read( lua_State *L )
{
    xml_reader_t *p_reader = *(xml_reader_t**)luaL_checkudata( L, 1, "xml_reader" );
    lua_pushinteger( L, xml_ReaderRead( p_reader ) );
    return 1;
}

static int vlclua_xml_reader_node_type( lua_State *L )
{
    xml_reader_t *p_reader = *(xml_reader_t**)luaL_checkudata( L, 1, "xml_reader" );
    static const char *ppsz_type[] = { "none", "startelem", "endelem", "text" };
    lua_pushstring( L, ppsz_type[xml_ReaderNodeType( p_reader )] );
    return 1;
}

static int vlclua_xml_reader_name( lua_State *L )
{
    xml_reader_t *p_reader = *(xml_reader_t**)luaL_checkudata( L, 1, "xml_reader" );
    lua_pushstring( L, xml_ReaderName( p_reader ) );
    return 1;
}

static int vlclua_xml_reader_value( lua_State *L )
{
    xml_reader_t *p_reader = *(xml_reader_t**)luaL_checkudata( L, 1, "xml_reader" );
    lua_pushstring( L, xml_ReaderValue( p_reader ) );
    return 1;
}

static int vlclua_xml_reader_next_attr( lua_State *L )
{
    xml_reader_t *p_reader = *(xml_reader_t**)luaL_checkudata( L, 1, "xml_reader" );
    lua_pushinteger( L, xml_ReaderNextAttr( p_reader ) );
    return 1;
}

static int vlclua_xml_reader_use_dtd( lua_State *L )
{
    xml_reader_t *p_reader = *(xml_reader_t**)luaL_checkudata( L, 1, "xml_reader" );
    bool b_value = lua_toboolean( L, 2 );
    lua_pushinteger( L, xml_ReaderUseDTD( p_reader, b_value ) );
    return 1;
}

void luaopen_xml( lua_State *L )
{
    lua_pushcfunction( L, vlclua_xml_create );
    lua_setfield( L, -2, "xml" );
}
