/*****************************************************************************
 * input.c
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
#ifndef  _GNU_SOURCE
#   define  _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_meta.h>
#include <vlc_charset.h>

#include <vlc_playlist.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */

#include "input.h"
#include "playlist.h"
#include "../vlc.h"
#include "../libs.h"

input_thread_t * vlclua_get_input_internal( lua_State *L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    input_thread_t *p_input = playlist_CurrentInput( p_playlist );
    vlclua_release_playlist_internal( p_playlist );
    return p_input;
}

static int vlclua_input_info( lua_State *L )
{
    input_thread_t * p_input = vlclua_get_input_internal( L );
    int i_cat;
    int i;
    if( !p_input ) return vlclua_error( L );
    //vlc_mutex_lock( &input_GetItem(p_input)->lock );
    i_cat = input_GetItem(p_input)->i_categories;
    lua_createtable( L, 0, i_cat );
    for( i = 0; i < i_cat; i++ )
    {
        info_category_t *p_category = input_GetItem(p_input)->pp_categories[i];
        int i_infos = p_category->i_infos;
        int j;
        lua_pushstring( L, p_category->psz_name );
        lua_createtable( L, 0, i_infos );
        for( j = 0; j < i_infos; j++ )
        {
            info_t *p_info = p_category->pp_infos[j];
            lua_pushstring( L, p_info->psz_name );
            lua_pushstring( L, p_info->psz_value );
            lua_settable( L, -3 );
        }
        lua_settable( L, -3 );
    }
    vlc_object_release( p_input );
    return 1;
}

static int vlclua_is_playing( lua_State *L )
{
    input_thread_t * p_input = vlclua_get_input_internal( L );
    lua_pushboolean( L, !!p_input );
    return 1;
}

static int vlclua_get_title( lua_State *L )
{
    input_thread_t *p_input = vlclua_get_input_internal( L );
    if( !p_input )
        lua_pushnil( L );
    else
    {
        lua_pushstring( L, input_GetItem(p_input)->psz_name );
        vlc_object_release( p_input );
    }
    return 1;
}

static int vlclua_input_stats( lua_State *L )
{
    input_thread_t *p_input = vlclua_get_input_internal( L );
    input_item_t *p_item = p_input && p_input->p ? input_GetItem( p_input ) : NULL;
    lua_newtable( L );
    if( p_item )
    {
#define STATS_INT( n ) lua_pushinteger( L, p_item->p_stats->i_ ## n ); \
                       lua_setfield( L, -2, #n );
#define STATS_FLOAT( n ) lua_pushnumber( L, p_item->p_stats->f_ ## n ); \
                         lua_setfield( L, -2, #n );
        STATS_INT( read_bytes )
        STATS_FLOAT( input_bitrate )
        STATS_INT( demux_read_bytes )
        STATS_FLOAT( demux_bitrate )
        STATS_INT( decoded_video )
        STATS_INT( displayed_pictures )
        STATS_INT( lost_pictures )
        STATS_INT( decoded_audio )
        STATS_INT( played_abuffers )
        STATS_INT( lost_abuffers )
        STATS_INT( sent_packets )
        STATS_INT( sent_bytes )
        STATS_FLOAT( send_bitrate )
#undef STATS_INT
#undef STATS_FLOAT
    }
    return 1;
}

/*****************************************************************************
 *
 *****************************************************************************/
static const luaL_Reg vlclua_input_reg[] = {
    { "info", vlclua_input_info },
    { "is_playing", vlclua_is_playing },
    { "get_title", vlclua_get_title },
    { "stats", vlclua_input_stats },
    { NULL, NULL }
};

void luaopen_input( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_input_reg );
    lua_setfield( L, -2, "input" );
}
