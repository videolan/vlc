/*****************************************************************************
 * vlc.h: VLC specific lua library functions.
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

#ifndef VLC_LUA_H
#define VLC_LUA_H
/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <vlc_common.h>
#include <vlc_input.h>
#include <vlc_playlist.h>
#include <vlc_meta.h>
#include <vlc_url.h>
#include <vlc_strings.h>
#include <vlc_stream.h>
#include <vlc_charset.h>

#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */
#include <lualib.h>     /* Lua libs */

/*****************************************************************************
 * Module entry points
 *****************************************************************************/
int FindArt( vlc_object_t * );

int Import_LuaPlaylist( vlc_object_t * );
void Close_LuaPlaylist( vlc_object_t * );

int Open_LuaIntf( vlc_object_t * );
void Close_LuaIntf( vlc_object_t * );


/*****************************************************************************
 * Lua debug
 *****************************************************************************/
static inline void lua_Dbg( vlc_object_t * p_this, const char * ppz_fmt, ... )
{
    va_list ap;
    va_start( ap, ppz_fmt );
    __msg_GenericVa( ( vlc_object_t *)p_this, VLC_MSG_DBG, MODULE_STRING,
                      ppz_fmt, ap );
    va_end( ap );
}

/*****************************************************************************
 * Functions that should be in lua ... but aren't for some obscure reason
 *****************************************************************************/
static inline int luaL_checkboolean( lua_State *L, int narg )
{
    luaL_checktype( L, narg, LUA_TBOOLEAN ); /* can raise an error */
    return lua_toboolean( L, narg );
}

static inline int luaL_optboolean( lua_State *L, int narg, int def )
{
    return luaL_opt( L, luaL_checkboolean, narg, def );
}

static inline const char *luaL_nilorcheckstring( lua_State *L, int narg )
{
    if( lua_isnil( L, narg ) )
        return NULL;
    return luaL_checkstring( L, narg );
}

vlc_object_t * vlclua_get_this( lua_State * );

/*****************************************************************************
 * Lua function bridge
 *****************************************************************************/
#define vlclua_error( L ) luaL_error( L, "VLC lua error in file %s line %d (function %s)", __FILE__, __LINE__, __func__ )
int vlclua_push_ret( lua_State *, int i_error );

/*****************************************************************************
 * Will execute func on all scripts in luadirname, and stop if func returns
 * success.
 *****************************************************************************/
int vlclua_scripts_batch_execute( vlc_object_t *p_this, const char * luadirname,
        int (*func)(vlc_object_t *, const char *, lua_State *, void *),
        lua_State * L, void * user_data );
int vlclua_dir_list( const char *luadirname, char **ppsz_dir_list );
void vlclua_dir_list_free( char **ppsz_dir_list );

/*****************************************************************************
 * Playlist and meta data internal utilities.
 *****************************************************************************/
void __vlclua_read_options( vlc_object_t *, lua_State *, int *, char *** );
#define vlclua_read_options(a,b,c,d) __vlclua_read_options(VLC_OBJECT(a),b,c,d)
void __vlclua_read_meta_data( vlc_object_t *, lua_State *, input_item_t * );
#define vlclua_read_meta_data(a,b,c) __vlclua_read_meta_data(VLC_OBJECT(a),b,c)
void __vlclua_read_custom_meta_data( vlc_object_t *, lua_State *,
                                     input_item_t *);
#define vlclua_read_custom_meta_data(a,b,c) __vlclua_read_custom_meta_data(VLC_OBJECT(a),b,c)
int __vlclua_playlist_add_internal( vlc_object_t *, lua_State *, playlist_t *,
                                    input_item_t *, bool );
#define vlclua_playlist_add_internal(a,b,c,d,e) __vlclua_playlist_add_internal(VLC_OBJECT(a),b,c,d,e)

/**
 * Per-interface private state
 */
struct intf_sys_t
{
    char *psz_filename;
    lua_State *L;

    vlc_thread_t thread;
    vlc_mutex_t lock;
    vlc_cond_t wait;
    bool exiting;
};

#endif /* VLC_LUA_H */

