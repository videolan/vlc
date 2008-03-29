/*****************************************************************************
 * objects.c: Generic lua<->vlc object wrapper
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */
#include <lualib.h>     /* Lua libs */

#include "vlc.h"

typedef struct
{
    vlc_object_t *p_obj;
} vlclua_object_t;

/*****************************************************************************
 * Generic vlc_object_t wrapper creation
 *****************************************************************************/
int __vlclua_push_vlc_object( lua_State *L, vlc_object_t *p_obj,
                              lua_CFunction pf_gc )
{
    vlclua_object_t *p_ud = (vlclua_object_t *)
        lua_newuserdata( L, sizeof( vlclua_object_t ) );
    p_ud->p_obj = p_obj;
    lua_newtable( L );
    /* Hide the metatable */
    lua_pushstring( L, "__metatable" );
    lua_pushstring( L, "none of your business" );
    lua_settable( L, -3 );
    if( pf_gc )
    {
        /* Set the garbage collector if needed */
        lua_pushstring( L, "__gc" );
        lua_pushcfunction( L, pf_gc );
        lua_settable( L, -3 );
    }
    lua_setmetatable( L, -2 );
    return 1;
}

int vlclua_gc_release( lua_State *L )
{
    vlclua_object_t *p_ud = (vlclua_object_t *)lua_touserdata( L, -1 );
    lua_pop( L, 1 );
    vlc_object_release( p_ud->p_obj );
    return 0;
}

vlc_object_t *vlclua_checkobject( lua_State *L, int narg, int i_type )
{
    vlclua_object_t *p_obj = (vlclua_object_t *)luaL_checkuserdata( L, narg, sizeof( vlclua_object_t ) );
    /* TODO: add some metatable based method to check that this isn't
     * any userdata, but really some vlc object */
    if( i_type )
    {
        if( p_obj->p_obj->i_object_type == i_type )
            return p_obj->p_obj;
        else
        {
            luaL_error( L, "VLC object type doesn't match requirements." );
            return NULL; /* luaL_error alread longjmp-ed out of here.
                          * This is to make gcc happy */
        }
    }
    else
    {
        return p_obj->p_obj;
    }
}

static int vlc_object_type_from_string( const char *psz_name )
{
    static const struct
    {
        int i_type;
        const char *psz_name;
    } pp_objects[] =
        { { VLC_OBJECT_GLOBAL, "global" },
          { VLC_OBJECT_LIBVLC, "libvlc" },
          { VLC_OBJECT_MODULE, "module" },
          { VLC_OBJECT_INTF, "intf" },
          { VLC_OBJECT_PLAYLIST, "playlist" },
          { VLC_OBJECT_INPUT, "input" },
          { VLC_OBJECT_DECODER, "decoder" },
          { VLC_OBJECT_VOUT, "vout" },
          { VLC_OBJECT_AOUT, "aout" },
          { VLC_OBJECT_SOUT, "sout" },
          { VLC_OBJECT_HTTPD, "httpd" },
          { VLC_OBJECT_PACKETIZER, "packetizer" },
          { VLC_OBJECT_ENCODER, "encoder" },
          { VLC_OBJECT_DIALOGS, "dialogs" },
          { VLC_OBJECT_VLM, "vlm" },
          { VLC_OBJECT_ANNOUNCE, "announce" },
          { VLC_OBJECT_DEMUX, "demux" },
          { VLC_OBJECT_ACCESS, "access" },
          { VLC_OBJECT_STREAM, "stream" },
          { VLC_OBJECT_OPENGL, "opengl" },
          { VLC_OBJECT_FILTER, "filter" },
          { VLC_OBJECT_XML, "xml" },
          { VLC_OBJECT_OSDMENU, "osdmenu" },
          { VLC_OBJECT_HTTPD_HOST, "httpd_host" },
          { VLC_OBJECT_META_ENGINE, "meta_engine" },
          { VLC_OBJECT_GENERIC, "generic" },
          { 0, "" } };
    int i;
    for( i = 0; pp_objects[i].i_type; i++ )
    {
        if( !strcmp( psz_name, pp_objects[i].psz_name ) )
            return pp_objects[i].i_type;
    }
    return 0;
}

static int vlc_object_search_mode_from_string( const char *psz_name )
{
    static const struct
    {
        int i_mode;
        const char *psz_name;
    } pp_modes[] =
        { { FIND_PARENT, "parent" },
          { FIND_CHILD, "child" },
          { FIND_ANYWHERE, "anywhere" },
          { 0, "" } };
    int i;
    for( i = 0; pp_modes[i].i_mode; i++ )
    {
        if( !strcmp( psz_name, pp_modes[i].psz_name ) )
            return pp_modes[i].i_mode;
    }
    return 0;
}

int vlclua_object_find( lua_State *L )
{
    const char *psz_type = luaL_checkstring( L, 2 );
    const char *psz_mode = luaL_checkstring( L, 3 );

    vlc_object_t *p_this;
    int i_type = vlc_object_type_from_string( psz_type );
    int i_mode = vlc_object_search_mode_from_string( psz_mode );
    vlc_object_t *p_result;

    if( !i_type )
        return luaL_error( L, "\"%s\" is not a valid object type.", psz_type );
    if( !i_mode )
        return luaL_error( L, "\"%s\" is not a valid search mode.", psz_mode );

    if( lua_type( L, 1 ) == LUA_TNIL )
        p_this = vlclua_get_this( L );
    else
        p_this = vlclua_checkobject( L, 1, 0 );

    p_result = vlc_object_find( p_this, i_type, i_mode );
    if( !p_result )
        lua_pushnil( L );
    else
        vlclua_push_vlc_object( L, p_result, vlclua_gc_release );
    return 1;
}

int vlclua_object_find_name( lua_State *L )
{
    const char *psz_name = luaL_checkstring( L, 2 );
    const char *psz_mode = luaL_checkstring( L, 3 );

    vlc_object_t *p_this;
    int i_mode = vlc_object_search_mode_from_string( psz_mode );
    vlc_object_t *p_result;

    if( !i_mode )
        return luaL_error( L, "\"%s\" is not a valid search mode.",
                           psz_mode );

    if( lua_type( L, 1 ) == LUA_TNIL )
        p_this = vlclua_get_this( L );
    else
        p_this = vlclua_checkobject( L, 1, 0 );

    p_result = vlc_object_find_name( p_this, psz_name, i_mode );
    if( !p_result )
        lua_pushnil( L );
    else
        vlclua_push_vlc_object( L, p_result, vlclua_gc_release );
    return 1;
}
