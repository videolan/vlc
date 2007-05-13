/*****************************************************************************
 * luaplaylist.c :  Lua playlist demux module
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
#include <vlc/vlc.h>
#include <vlc_demux.h>
#include <vlc_url.h>
#include <vlc_strings.h>

#include <errno.h>                                                 /* ENOMEM */
#include "playlist.h"

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */
#include <lualib.h>     /* Lua libs */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int E_(Import_LuaPlaylist)( vlc_object_t *p_this );
static void E_(Close_LuaPlaylist)( vlc_object_t *p_this );

static int Demux( demux_t *p_demux );
static int Control( demux_t *p_demux, int i_query, va_list args );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    add_shortcut( "lua" );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );

    set_shortname( _("Lua Playlist") );
    set_description( _("Lua Playlist Parser Interface") );
    set_capability( "demux2", 0 );
    set_callbacks( E_(Import_LuaPlaylist), E_(Close_LuaPlaylist) );
vlc_module_end();

/*****************************************************************************
 *
 *****************************************************************************/
struct demux_sys_t
{
    lua_State *p_state;
};

static demux_t *vlclua_get_demux( lua_State *p_state )
{
    demux_t *p_demux;
    lua_getglobal( p_state, "vlc" );
    lua_getfield( p_state, lua_gettop( p_state ), "private" );
    p_demux = (demux_t*)lua_topointer( p_state, lua_gettop( p_state ) );
    lua_pop( p_state, 2 );
    return p_demux;
}

static int vlclua_peek( lua_State *p_state )
{
    demux_t *p_demux = vlclua_get_demux( p_state );
    int i = lua_gettop( p_state );
    int n;
    byte_t *p_peek;
    int i_peek;
    if( !i ) return 0;
    n = lua_tonumber( p_state, 1 );
    lua_pop( p_state, i );
    i_peek = stream_Peek( p_demux->s, &p_peek, n );
    lua_pushnumber( p_state, i_peek );
    lua_pushlstring( p_state, (const char *)p_peek, i_peek );
    return 2;
}

static int vlclua_readline( lua_State *p_state )
{
    demux_t *p_demux = vlclua_get_demux( p_state );
    char *psz_line = stream_ReadLine( p_demux->s );
    lua_pushstring( p_state, psz_line );
    return 1;
}

static int vlclua_decode_uri( lua_State *p_state )
{
    int i = lua_gettop( p_state );
    if( !i ) return 0;
    const char *psz_cstring = lua_tostring( p_state, 1 );
    if( !psz_cstring ) return 0;
    char *psz_string = strdup( psz_cstring );
    lua_pop( p_state, i );
    decode_URI( psz_string );
    lua_pushstring( p_state, psz_string );
    free( psz_string );
    return 1;
}

/*****************************************************************************
 * Import_LuaPlaylist: main import function
 *****************************************************************************/
int E_(Import_LuaPlaylist)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    lua_State *p_state;

    p_demux->p_sys = (demux_sys_t*)malloc( sizeof( demux_sys_t ) );
    if( !p_demux->p_sys )
    {
        return VLC_ENOMEM;
    }

    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;

    /* Initialise Lua state structure */
    p_state = luaL_newstate();
    if( !p_state )
    {
        msg_Err( p_demux, "Could not create new Lua State" );
        free( p_demux->p_sys );
        return VLC_EGENERIC;
    }
    p_demux->p_sys->p_state = p_state;

    /* Load Lua libraries */
    luaL_openlibs( p_state ); /* FIXME: Don't open all the libs? */

    static luaL_Reg p_reg[] =
    {
        { "readline", vlclua_readline },
        { "peek", vlclua_peek },
        { "decode_uri", vlclua_decode_uri }
    };

    luaL_register( p_state, "vlc", p_reg );
    lua_pushlightuserdata( p_state, p_demux );
    lua_setfield( p_state, lua_gettop( p_state ) - 1, "private" );
    lua_pushstring( p_state, p_demux->psz_path );
    lua_setfield( p_state, lua_gettop( p_state ) - 1, "path" );
    lua_pushstring( p_state, p_demux->psz_access );
    lua_setfield( p_state, lua_gettop( p_state ) - 1, "access" );

    lua_pop( p_state, 1 );

    {
        const char *psz_filename = "share/luaplaylist/test.lua";
        int i_ret;

        /* Load and run the script(s) */
        if( luaL_dofile( p_state, psz_filename ) )
        {
            msg_Warn( p_demux, "Error while runing script %s: %s", psz_filename, lua_tostring( p_state, lua_gettop( p_state ) ) );
            return VLC_EGENERIC;
        }

        if( lua_gettop( p_state ) )
        {
            i_ret = lua_toboolean( p_state, 1 );
            printf( "Script returned %d: %d\n", 1, i_ret );

            while( lua_gettop( p_state ) != 1 )
            {
                printf( "Script returned %d: %s\n", lua_gettop( p_state ),
                        lua_tostring( p_state, lua_gettop( p_state ) ) );
                lua_pop( p_state, 1 );
            }

            lua_pop( p_state, 1 );

            if( !i_ret )
            {
                E_(Close_LuaPlaylist)( p_this );
                return VLC_EGENERIC;
            }
        }
        else
        {
            E_(Close_LuaPlaylist)( p_this );
            return VLC_EGENERIC;
        }
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void E_(Close_LuaPlaylist)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    lua_close( p_demux->p_sys->p_state );
    free( p_demux->p_sys );
}

static int Demux( demux_t *p_demux )
{
    /*input_item_t *p_input;*/

    INIT_PLAYLIST_STUFF;

#if 0
    p_input = input_ItemNewExt( p_playlist, psz_url, psz_title, 0, NULL, -1 );
    playlist_BothAddInput( p_playlist, p_input,
                           p_item_in_category,
                           PLAYLIST_APPEND | PLAYLIST_SPREPARSE,
                           PLAYLIST_END, NULL, NULL, VLC_FALSE );
#endif

    HANDLE_PLAY_AND_RELEASE;

    return -1; /* Needed for correct operation of go back */
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return VLC_EGENERIC;
}
