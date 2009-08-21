/*****************************************************************************
 * demux.c :  Lua playlist demux module
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_url.h>
#include <vlc_strings.h>
#include <vlc_charset.h>

#include <errno.h>                                                 /* ENOMEM */
#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif

#include "vlc.h"
#include "libs.h"
#include "libs/playlist.h"


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux );
static int Control( demux_t *p_demux, int i_query, va_list args );

/*****************************************************************************
 * Demux specific functions
 *****************************************************************************/
struct demux_sys_t
{
    lua_State *L;
    char *psz_filename;
};

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
    const uint8_t *p_read;
    int n = luaL_checkint( L, 1 );
    int i_read = stream_Peek( p_demux->s, &p_read, n );
    lua_pushlstring( L, (const char *)p_read, i_read );
    int i_seek = stream_Read( p_demux->s, NULL, i_read );
    assert(i_read==i_seek);
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

/*****************************************************************************
 *
 *****************************************************************************/
/* Functions to register */
static const luaL_Reg p_reg[] =
{
    { "peek", vlclua_demux_peek },
    { NULL, NULL }
};

/* Functions to register for parse() function call only */
static const luaL_Reg p_reg_parse[] =
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
    VLC_UNUSED(user_data);
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
int Import_LuaPlaylist( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    lua_State *L;
    int ret;

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
    luaopen_msg( L );
    luaopen_strings( L );
    lua_pushlightuserdata( L, p_demux );
    lua_setfield( L, -2, "private" );
    lua_pushstring( L, p_demux->psz_path );
    lua_setfield( L, -2, "path" );
    lua_pushstring( L, p_demux->psz_access );
    lua_setfield( L, -2, "access" );

    lua_pop( L, 1 );

    ret = vlclua_scripts_batch_execute( p_this, "playlist",
                                        &probe_luascript, L, NULL );
    if( ret )
        Close_LuaPlaylist( p_this );
    return ret;
}


/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void Close_LuaPlaylist( vlc_object_t *p_this )
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

    input_thread_t *p_input_thread = demux_GetParentInput( p_demux );
    input_item_t *p_current_input = input_GetItem( p_input_thread );
    playlist_t *p_playlist = pl_Hold( p_demux );

    luaL_register( L, "vlc", p_reg_parse );

    lua_getglobal( L, "parse" );

    if( !lua_isfunction( L, -1 ) )
    {
        msg_Warn( p_demux, "Error while runing script %s, "
                  "function parse() not found", psz_filename );
        pl_Release( p_demux );
        return VLC_EGENERIC;
    }

    if( lua_pcall( L, 0, 1, 0 ) )
    {
        msg_Warn( p_demux, "Error while runing script %s, "
                  "function parse(): %s", psz_filename,
                  lua_tostring( L, lua_gettop( L ) ) );
        pl_Release( p_demux );
        return VLC_EGENERIC;
    }

    if( lua_gettop( L ) )
        vlclua_playlist_add_internal( p_demux, L, p_playlist,
                                      p_current_input, 0 );
    else
        msg_Err( p_demux, "Script went completely foobar" );

    vlc_object_release( p_input_thread );
    vlclua_release_playlist_internal( p_playlist );

    return -1; /* Needed for correct operation of go back */
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    VLC_UNUSED(p_demux); VLC_UNUSED(i_query); VLC_UNUSED(args);
    return VLC_EGENERIC;
}
