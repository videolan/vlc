/*****************************************************************************
 * luameta.c: Get meta/artwork using lua scripts
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
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

#ifndef VLCLUA_H
#define VLC_LUA_H
/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifndef  _GNU_SOURCE
#   define  _GNU_SOURCE
#endif

#include <vlc/vlc.h>
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
int E_(FindArt)( vlc_object_t * );
int E_(FindMeta)( vlc_object_t * );

int E_(Import_LuaPlaylist)( vlc_object_t * );
void E_(Close_LuaPlaylist)( vlc_object_t * );

int E_(Open_LuaIntf)( vlc_object_t * );
void E_(Close_LuaIntf)( vlc_object_t * );


/*****************************************************************************
 * Lua debug
 *****************************************************************************/
static inline void lua_Dbg( vlc_object_t * p_this, const char * ppz_fmt, ... )
{
    if( p_this->p_libvlc->i_verbose < 3 )
        return;

    va_list ap;
    va_start( ap, ppz_fmt );
    __msg_GenericVa( ( vlc_object_t *)p_this, MSG_QUEUE_NORMAL,
                      VLC_MSG_DBG, MODULE_STRING,
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

static inline const void *luaL_checklightuserdata( lua_State *L, int narg )
{
    luaL_checktype( L, narg, LUA_TLIGHTUSERDATA ); /* can raise an error */
    return lua_topointer( L, narg );
}

static inline const void *luaL_checkuserdata( lua_State *L, int narg, size_t size )
{
    luaL_checktype( L, narg, LUA_TUSERDATA ); /* can raise an error */
    if( size && size != lua_objlen( L, narg ) ) /* this isn't worth much ... but it might still prevent a few errors */
        luaL_error( L, "user data size doesn't match" );
    return lua_topointer( L, narg );
}

/*****************************************************************************
 * Lua vlc_object_t wrapper
 *****************************************************************************/
int __vlclua_push_vlc_object( lua_State *L, vlc_object_t *p_obj,
                              lua_CFunction pf_gc );
#define vlclua_push_vlc_object( a, b, c ) \
        __vlclua_push_vlc_object( a, VLC_OBJECT( b ), c )
vlc_object_t *vlclua_checkobject( lua_State *L, int narg, int i_type );
int vlclua_gc_release( lua_State *L );
int vlclua_object_find( lua_State *L );
int vlclua_object_find_name( lua_State *L );

int vlclua_add_callback( lua_State * );
int vlclua_del_callback( lua_State * );

int vlclua_url_parse( lua_State * );
int vlclua_net_listen_tcp( lua_State * );
int vlclua_net_listen_close( lua_State * );
int vlclua_net_accept( lua_State * );
int vlclua_net_close( lua_State * );
int vlclua_net_send( lua_State * );
int vlclua_net_recv( lua_State * );
int vlclua_net_select( lua_State * );

int vlclua_fd_set_new( lua_State * );
int vlclua_fd_clr( lua_State * );
int vlclua_fd_isset( lua_State * );
int vlclua_fd_set( lua_State * );
int vlclua_fd_zero( lua_State * );
int vlclua_fd_read( lua_State * );
int vlclua_fd_write( lua_State * );

int vlclua_vlm_new( lua_State * );
int vlclua_vlm_delete( lua_State * );
int vlclua_vlm_execute_command( lua_State * );

/*****************************************************************************
 * Lua function bridge
 *****************************************************************************/
vlc_object_t * vlclua_get_this( lua_State * );
#define vlclua_error( L ) luaL_error( L, "VLC lua error in file %s line %d (function %s)", __FILE__, __LINE__, __func__ )
int vlclua_push_ret( lua_State *, int i_error );

int vlclua_version( lua_State * );
int vlclua_quit( lua_State * );

int vlclua_pushvalue( lua_State *L, int i_type, vlc_value_t val ); /* internal use only */
int vlclua_var_get( lua_State * );
int vlclua_var_get_list( lua_State * );
int vlclua_var_set( lua_State * );
int vlclua_module_command( lua_State * );
int vlclua_libvlc_command( lua_State * );

int vlclua_config_get( lua_State * );
int vlclua_config_set( lua_State * );

int vlclua_volume_set( lua_State * );
int vlclua_volume_get( lua_State * );
int vlclua_volume_up( lua_State * );
int vlclua_volume_down( lua_State * );

int vlclua_stream_new( lua_State * );
int vlclua_stream_read( lua_State * );
int vlclua_stream_readline( lua_State * );
int vlclua_stream_delete( lua_State * );

int vlclua_decode_uri( lua_State * );
int vlclua_resolve_xml_special_chars( lua_State * );

int vlclua_msg_dbg( lua_State * );
int vlclua_msg_warn( lua_State * );
int vlclua_msg_err( lua_State * );
int vlclua_msg_info( lua_State * );

/*****************************************************************************
 * Will execute func on all scripts in luadirname, and stop if func returns
 * success.
 *****************************************************************************/
int vlclua_scripts_batch_execute( vlc_object_t *p_this, const char * luadirname,
        int (*func)(vlc_object_t *, const char *, lua_State *, void *),
        lua_State * L, void * user_data );
int vlclua_dir_list( vlc_object_t *p_this, const char *luadirname, char **ppsz_dir_list );

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
                                    input_item_t *, vlc_bool_t );
#define vlclua_playlist_add_internal(a,b,c,d,e) __vlclua_playlist_add_internal(VLC_OBJECT(a),b,c,d,e)


#endif /* VLC_LUA_H */

