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
 * Lua function bridge
 *****************************************************************************/
vlc_object_t * vlclua_get_this( lua_State *p_state );
int vlclua_stream_new( lua_State *p_state );
int vlclua_stream_read( lua_State *p_state );
int vlclua_stream_readline( lua_State *p_state );
int vlclua_stream_delete( lua_State *p_state );
int vlclua_decode_uri( lua_State *p_state );
int vlclua_resolve_xml_special_chars( lua_State *p_state );
int vlclua_msg_dbg( lua_State *p_state );
int vlclua_msg_warn( lua_State *p_state );
int vlclua_msg_err( lua_State *p_state );
int vlclua_msg_info( lua_State *p_state );


/*****************************************************************************
 * Will execute func on all scripts in luadirname, and stop if func returns
 * success.
 *****************************************************************************/
int vlclua_scripts_batch_execute( vlc_object_t *p_this, const char * luadirname,
        int (*func)(vlc_object_t *, const char *, lua_State *, void *),
        lua_State * p_state, void * user_data);



/*****************************************************************************
 * Meta data setters utility.
 *****************************************************************************/
void vlclua_read_meta_data( vlc_object_t *p_this,
        lua_State *p_state, int o, int t, input_item_t *p_input );


void vlclua_read_custom_meta_data( vlc_object_t *p_this,
        lua_State *p_state, int o, int t, input_item_t *p_input );

#endif /* VLC_LUA_H */

