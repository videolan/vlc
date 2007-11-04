/*****************************************************************************
 * playlist.c :  Lua playlist demux module
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
#include <vlc_charset.h>

#include <errno.h>                                                 /* ENOMEM */
#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif

#include "vlc.h"


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux );
static int Control( demux_t *p_demux, int i_query, va_list args );

/*****************************************************************************
 *
 *****************************************************************************/
struct demux_sys_t
{
    lua_State *L;
    char *psz_filename;
};

/*****************************************************************************
 *
 *****************************************************************************/

static int vlclua_demux_peek( lua_State *L )
{
    demux_t *p_demux = (demux_t *)vlclua_get_this( L );
    int n = luaL_checkint( L, 1 );
    const uint8_t *p_peek;
    int i_peek = stream_Peek( p_demux->s, &p_peek, n );
    lua_pushlstring( L, (const char *)p_peek, i_peek );
    return 1;
}

static int vlclua_demux_read( lua_State *L )
{
    demux_t *p_demux = (demux_t *)vlclua_get_this( L );
    byte_t *p_read;
    int n = luaL_checkint( L, 1 );
    int i_read = stream_Read( p_demux->s, &p_read, n );
    lua_pushlstring( L, (const char *)p_read, i_read );
    return 1;
}

static int vlclua_demux_readline( lua_State *L )
{
    demux_t *p_demux = (demux_t *)vlclua_get_this( L );
    char *psz_line = stream_ReadLine( p_demux->s );
    if( psz_line )
    {
        lua_pushstring( L, psz_line );
        free( psz_line );
    }
    else
    {
        lua_pushnil( L );
    }
    return 1;
}


/* Functions to register */
static luaL_Reg p_reg[] =
{
    { "peek", vlclua_demux_peek },
    { "decode_uri", vlclua_decode_uri },
    { "resolve_xml_special_chars", vlclua_resolve_xml_special_chars },
    { "msg_dbg", vlclua_msg_dbg },
    { "msg_warn", vlclua_msg_warn },
    { "msg_err", vlclua_msg_err },
    { "msg_info", vlclua_msg_info },
    { NULL, NULL }
};

/* Functions to register for parse() function call only */
static luaL_Reg p_reg_parse[] =
{
    { "read", vlclua_demux_read },
    { "readline", vlclua_demux_readline },
    { NULL, NULL }
};

/*****************************************************************************
 * Called through lua_scripts_batch_execute to call 'probe' on
 * the script pointed by psz_filename.
 *****************************************************************************/
static int probe_luascript( vlc_object_t *p_this, const char * psz_filename,
                            lua_State * L, void * user_data )
{
    demux_t * p_demux = (demux_t *)p_this;

    p_demux->p_sys->psz_filename = strdup(psz_filename);

    /* In lua, setting a variable's value to nil is equivalent to deleting it */
    lua_pushnil( L );
    lua_pushnil( L );
    lua_setglobal( L, "probe" );
    lua_setglobal( L, "parse" );

    /* Load and run the script(s) */
    if( luaL_dofile( L, psz_filename ) )
    {
        msg_Warn( p_demux, "Error loading script %s: %s", psz_filename,
                  lua_tostring( L, lua_gettop( L ) ) );
        lua_pop( L, 1 );
        return VLC_EGENERIC;
    }

    lua_getglobal( L, "probe" );

    if( !lua_isfunction( L, -1 ) )
    {
        msg_Warn( p_demux, "Error while runing script %s, "
                  "function probe() not found", psz_filename );
        lua_pop( L, 1 );
        return VLC_EGENERIC;
    }

    if( lua_pcall( L, 0, 1, 0 ) )
    {
        msg_Warn( p_demux, "Error while runing script %s, "
                  "function probe(): %s", psz_filename,
                  lua_tostring( L, lua_gettop( L ) ) );
        lua_pop( L, 1 );
        return VLC_EGENERIC;
    }

    if( lua_gettop( L ) )
    {
        int i_ret = VLC_EGENERIC;
        if( lua_toboolean( L, 1 ) )
        {
            msg_Dbg( p_demux, "Lua playlist script %s's "
                     "probe() function was successful", psz_filename );
            i_ret = VLC_SUCCESS;
        }
        lua_pop( L, 1 );

        return i_ret;
    }
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Import_LuaPlaylist: main import function
 *****************************************************************************/
int E_(Import_LuaPlaylist)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    lua_State *L;

    p_demux->p_sys = (demux_sys_t*)malloc( sizeof( demux_sys_t ) );
    if( !p_demux->p_sys )
    {
        return VLC_ENOMEM;
    }

    p_demux->p_sys->psz_filename = NULL;

    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;

    /* Initialise Lua state structure */
    L = luaL_newstate();
    if( !L )
    {
        msg_Err( p_demux, "Could not create new Lua State" );
        free( p_demux->p_sys );
        return VLC_EGENERIC;
    }
    p_demux->p_sys->L = L;

    /* Load Lua libraries */
    luaL_openlibs( L ); /* FIXME: Don't open all the libs? */

    luaL_register( L, "vlc", p_reg );
    lua_pushlightuserdata( L, p_demux );
    lua_setfield( L, -2, "private" );
    lua_pushstring( L, p_demux->psz_path );
    lua_setfield( L, -2, "path" );
    lua_pushstring( L, p_demux->psz_access );
    lua_setfield( L, -2, "access" );

    lua_pop( L, 1 );

    return vlclua_scripts_batch_execute( p_this, "luaplaylist", &probe_luascript,
                                         L, NULL );
}



/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void E_(Close_LuaPlaylist)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    lua_close( p_demux->p_sys->L );
    free( p_demux->p_sys->psz_filename );
    free( p_demux->p_sys );
}

static int Demux( demux_t *p_demux )
{
    lua_State *L = p_demux->p_sys->L;
    char *psz_filename = p_demux->p_sys->psz_filename;

    playlist_t *p_playlist = pl_Yield( p_demux );
    input_thread_t *p_input_thread = (input_thread_t *)
        vlc_object_find( p_demux, VLC_OBJECT_INPUT, FIND_PARENT );
    input_item_t *p_current_input = input_GetItem( p_input_thread );

    luaL_register( L, "vlc", p_reg_parse );

    lua_getglobal( L, "parse" );

    if( !lua_isfunction( L, -1 ) )
    {
        msg_Warn( p_demux, "Error while runing script %s, "
                  "function parse() not found", psz_filename );
        E_(Close_LuaPlaylist)( VLC_OBJECT( p_demux ) );
        return VLC_EGENERIC;
    }

    if( lua_pcall( L, 0, 1, 0 ) )
    {
        msg_Warn( p_demux, "Error while runing script %s, "
                  "function parse(): %s", psz_filename,
                  lua_tostring( L, lua_gettop( L ) ) );
        E_(Close_LuaPlaylist)( VLC_OBJECT( p_demux ) );
        return VLC_EGENERIC;
    }

    if( lua_gettop( L ) )
        vlclua_playlist_add_internal( p_demux, L, p_playlist,
                                      p_current_input, 0 );
    else
        msg_Err( p_demux, "Script went completely foobar" );

    vlc_object_release( p_input_thread );
    vlc_object_release( p_playlist );

    return -1; /* Needed for correct operation of go back */
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return VLC_EGENERIC;
}
