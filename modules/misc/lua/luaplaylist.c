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
#include <vlc_charset.h>

#include <errno.h>                                                 /* ENOMEM */
#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif

#include "vlclua.h"


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
    lua_State *p_state;
    char *psz_filename;
};

/*****************************************************************************
 *
 *****************************************************************************/

static int vlclua_demux_peek( lua_State *p_state )
{
    demux_t *p_demux = (demux_t *)vlclua_get_this( p_state );
    int i = lua_gettop( p_state );
    int n;
    byte_t *p_peek;
    int i_peek;
    if( !i ) return 0;
    n = lua_tonumber( p_state, 1 );
    lua_pop( p_state, i );
    i_peek = stream_Peek( p_demux->s, &p_peek, n );
    lua_pushlstring( p_state, (const char *)p_peek, i_peek );
    return 1;
}

static int vlclua_demux_read( lua_State *p_state )
{
    demux_t *p_demux = (demux_t *)vlclua_get_this( p_state );
    int i = lua_gettop( p_state );
    int n;
    byte_t *p_read;
    int i_read;
    if( !i ) return 0;
    n = lua_tonumber( p_state, 1 );
    lua_pop( p_state, i );
    i_read = stream_Read( p_demux->s, &p_read, n );
    lua_pushlstring( p_state, (const char *)p_read, i_read );
    return 1;
}

static int vlclua_demux_readline( lua_State *p_state )
{
    demux_t *p_demux = (demux_t *)vlclua_get_this( p_state );
    char *psz_line = stream_ReadLine( p_demux->s );
    if( psz_line )
    {
        lua_pushstring( p_state, psz_line );
        free( psz_line );
    }
    else
    {
        lua_pushnil( p_state );
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
                            lua_State * p_state, void * user_data )
{
    demux_t * p_demux = (demux_t *)p_this;

    p_demux->p_sys->psz_filename = strdup(psz_filename);
 
    /* Ugly hack to delete previous versions of the probe() and parse()
    * functions. */
    lua_pushnil( p_state );
    lua_pushnil( p_state );
    lua_setglobal( p_state, "probe" );
    lua_setglobal( p_state, "parse" );
 
    /* Load and run the script(s) */
    if( luaL_dofile( p_state, psz_filename ) )
    {
        msg_Warn( p_demux, "Error loading script %s: %s", psz_filename,
                  lua_tostring( p_state, lua_gettop( p_state ) ) );
        lua_pop( p_state, 1 );
        return VLC_EGENERIC;
    }
 
    lua_getglobal( p_state, "probe" );
 
    if( !lua_isfunction( p_state, lua_gettop( p_state ) ) )
    {
        msg_Warn( p_demux, "Error while runing script %s, "
                  "function probe() not found", psz_filename );
        lua_pop( p_state, 1 );
        return VLC_EGENERIC;
    }
 
    if( lua_pcall( p_state, 0, 1, 0 ) )
    {
        msg_Warn( p_demux, "Error while runing script %s, "
                  "function probe(): %s", psz_filename,
                  lua_tostring( p_state, lua_gettop( p_state ) ) );
        lua_pop( p_state, 1 );
        return VLC_EGENERIC;
    }
 
    if( lua_gettop( p_state ) )
    {
        int i_ret = VLC_EGENERIC;
        if( lua_toboolean( p_state, 1 ) )
        {
            msg_Dbg( p_demux, "Lua playlist script %s's "
                     "probe() function was successful", psz_filename );
            i_ret = VLC_SUCCESS;
        }
        lua_pop( p_state, 1 );
 
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
    lua_State *p_state;

    p_demux->p_sys = (demux_sys_t*)malloc( sizeof( demux_sys_t ) );
    if( !p_demux->p_sys )
    {
        return VLC_ENOMEM;
    }

    p_demux->p_sys->psz_filename = NULL;

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

    luaL_register( p_state, "vlc", p_reg );
    lua_pushlightuserdata( p_state, p_demux );
    lua_setfield( p_state, lua_gettop( p_state ) - 1, "private" );
    lua_pushstring( p_state, p_demux->psz_path );
    lua_setfield( p_state, lua_gettop( p_state ) - 1, "path" );
    lua_pushstring( p_state, p_demux->psz_access );
    lua_setfield( p_state, lua_gettop( p_state ) - 1, "access" );

    lua_pop( p_state, 1 );

    return vlclua_scripts_batch_execute( p_this, "luaplaylist", &probe_luascript,
                                         p_state, NULL );
}



/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void E_(Close_LuaPlaylist)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    lua_close( p_demux->p_sys->p_state );
    free( p_demux->p_sys->psz_filename );
    free( p_demux->p_sys );
}

static inline void read_options( demux_t *p_demux, lua_State *p_state,
                                 int o, int t, int *pi_options,
                                 char ***pppsz_options )
{
    lua_getfield( p_state, o, "options" );
    if( lua_istable( p_state, t ) )
    {
        lua_pushnil( p_state );
        while( lua_next( p_state, t ) )
        {
            if( lua_isstring( p_state, t+2 ) )
            {
                char *psz_option = strdup( lua_tostring( p_state, t+2 ) );
                msg_Dbg( p_demux, "Option: %s", psz_option );
                INSERT_ELEM( *pppsz_options, *pi_options, *pi_options,
                             psz_option );
            }
            else
            {
                msg_Warn( p_demux, "Option should be a string" );
            }
            lua_pop( p_state, 1 ); /* pop option */
        }
    }
    lua_pop( p_state, 1 ); /* pop "options" */
}


static int Demux( demux_t *p_demux )
{
    input_item_t *p_input;
    lua_State *p_state = p_demux->p_sys->p_state;
    char *psz_filename = p_demux->p_sys->psz_filename;
    int t;

    playlist_t *p_playlist = pl_Yield( p_demux );
    input_thread_t *p_input_thread = (input_thread_t *)vlc_object_find( p_demux, VLC_OBJECT_INPUT, FIND_PARENT );
    input_item_t *p_current_input = input_GetItem( p_input_thread );

    luaL_register( p_state, "vlc", p_reg_parse );

    lua_getglobal( p_state, "parse" );

    if( !lua_isfunction( p_state, lua_gettop( p_state ) ) )
    {
        msg_Warn( p_demux, "Error while runing script %s, "
                  "function parse() not found", psz_filename );
        E_(Close_LuaPlaylist)( VLC_OBJECT( p_demux ) );
        return VLC_EGENERIC;
    }

    if( lua_pcall( p_state, 0, 1, 0 ) )
    {
        msg_Warn( p_demux, "Error while runing script %s, "
                  "function parse(): %s", psz_filename,
                  lua_tostring( p_state, lua_gettop( p_state ) ) );
        E_(Close_LuaPlaylist)( VLC_OBJECT( p_demux ) );
        return VLC_EGENERIC;
    }

    /* Check that the Lua stack is big enough and grow it if needed.
     * Should be ok since LUA_MINSTACK is 20 but we never know. */
    lua_checkstack( p_state, 8 );

    if( ( t = lua_gettop( p_state ) ) )
    {

        if( lua_istable( p_state, t ) )
        {
            lua_pushnil( p_state );
            while( lua_next( p_state, t ) )
            {
                if( lua_istable( p_state, t+2 ) )
                {
                    lua_getfield( p_state, t+2, "path" );
                    if( lua_isstring( p_state, t+3 ) )
                    {
                        const char  *psz_path     = NULL;
                        const char  *psz_name     = NULL;
                        char       **ppsz_options = NULL;
                        int          i_options    = 0;
                        mtime_t      i_duration   = -1;

                        /* Read path and name */
                        psz_path = lua_tostring( p_state, t+3 );
                        msg_Dbg( p_demux, "Path: %s", psz_path );
                        lua_getfield( p_state, t+2, "name" );
                        if( lua_isstring( p_state, t+4 ) )
                        {
                            psz_name = lua_tostring( p_state, t+4 );
                            msg_Dbg( p_demux, "Name: %s", psz_name );
                        }
                        else
                        {
                            psz_name = psz_path;
                        }

                        /* Read duration */
                        lua_getfield( p_state, t+2, "duration" );
                        if( lua_isnumber( p_state, t+5 ) )
                        {
                            i_duration = (mtime_t)lua_tointeger( p_state, t+5 );
                            i_duration *= 1000000;
                        }
                        lua_pop( p_state, 1 ); /* pop "duration" */

                        /* Read options */
                        read_options( p_demux, p_state, t+2, t+5,
                                      &i_options, &ppsz_options );

                        /* Create input item */
                        p_input = input_ItemNewExt( p_playlist, psz_path,
                                                    psz_name, i_options,
                                                    (const char **)ppsz_options,
                                                    i_duration );
                        lua_pop( p_state, 1 ); /* pop "name" */

                        /* Read meta data */
                        vlclua_read_meta_data( VLC_OBJECT(p_demux), p_state, t+2, t+4, p_input );

                        /* Read custom meta data */
                        vlclua_read_custom_meta_data( VLC_OBJECT(p_demux), p_state, t+2, t+4,
                                               p_input );

                        /* Append item to playlist */
                        input_ItemAddSubItem( p_current_input, p_input );

                        while( i_options > 0 )
                            free( ppsz_options[--i_options] );
                        free( ppsz_options );
                    }
                    else
                    {
                        msg_Warn( p_demux,
                                 "Playlist item's path should be a string" );
                    }
                    lua_pop( p_state, 1 ); /* pop "path" */
                }
                else
                {
                    msg_Warn( p_demux, "Playlist item should be a table" );
                }
                lua_pop( p_state, 1 ); /* pop the value, keep the key for
                                        * the next lua_next() call */
            }
        }
        else
        {
            msg_Warn( p_demux, "Script didn't return a table" );
        }
    }
    else
    {
        msg_Err( p_demux, "Script went completely foobar" );
    }

    vlc_object_release( p_input_thread );
    vlc_object_release( p_playlist );

    return -1; /* Needed for correct operation of go back */
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return VLC_EGENERIC;
}
