/*****************************************************************************
 * playlist.c
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

#include <vlc_interface.h>
#include <vlc_playlist.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */

#include "../vlc.h"
#include "../libs.h"
#include "playlist.h"
#include "variables.h"

/*****************************************************************************
 * Internal lua<->vlc utils
 *****************************************************************************/
playlist_t *vlclua_get_playlist_internal( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    return pl_Hold( p_this );
}

void vlclua_release_playlist_internal( playlist_t *p_playlist )
{
    vlc_object_release( p_playlist );
}

static int vlclua_playlist_prev( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    playlist_Prev( p_playlist );
    vlclua_release_playlist_internal( p_playlist );
    return 0;
}

static int vlclua_playlist_next( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    playlist_Next( p_playlist );
    vlclua_release_playlist_internal( p_playlist );
    return 0;
}

static int vlclua_playlist_skip( lua_State * L )
{
    int i_skip = luaL_checkint( L, 1 );
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    playlist_Skip( p_playlist, i_skip );
    vlclua_release_playlist_internal( p_playlist );
    return 0;
}

static int vlclua_playlist_play( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    PL_LOCK;
    playlist_Play( p_playlist );
    PL_UNLOCK;
    vlclua_release_playlist_internal( p_playlist );
    return 0;
}

static int vlclua_playlist_pause( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    playlist_Pause( p_playlist );
    vlclua_release_playlist_internal( p_playlist );
    return 0;
}

static int vlclua_playlist_stop( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    playlist_Stop( p_playlist );
    vlclua_release_playlist_internal( p_playlist );
    return 0;
}

static int vlclua_playlist_clear( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    playlist_Stop( p_playlist ); /* Isn't this already implied by Clear? */
    playlist_Clear( p_playlist, pl_Unlocked );
    vlclua_release_playlist_internal( p_playlist );
    return 0;
}

static int vlclua_playlist_repeat( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    int i_ret = vlclua_var_toggle_or_set( L, p_playlist, "repeat" );
    vlclua_release_playlist_internal( p_playlist );
    return i_ret;
}

static int vlclua_playlist_loop( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    int i_ret = vlclua_var_toggle_or_set( L, p_playlist, "loop" );
    vlclua_release_playlist_internal( p_playlist );
    return i_ret;
}

static int vlclua_playlist_random( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    int i_ret = vlclua_var_toggle_or_set( L, p_playlist, "random" );
    vlclua_release_playlist_internal( p_playlist );
    return i_ret;
}

static int vlclua_playlist_goto( lua_State * L )
{
    int i_id = luaL_checkint( L, 1 );
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    PL_LOCK;
    int i_ret = playlist_Control( p_playlist, PLAYLIST_VIEWPLAY,
                                  true, NULL,
                                  playlist_ItemGetById( p_playlist, i_id ) );
    PL_UNLOCK;
    vlclua_release_playlist_internal( p_playlist );
    return vlclua_push_ret( L, i_ret );
}

static int vlclua_playlist_add( lua_State *L )
{
    int i_count;
    vlc_object_t *p_this = vlclua_get_this( L );
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    i_count = vlclua_playlist_add_internal( p_this, L, p_playlist,
                                            NULL, true );
    vlclua_release_playlist_internal( p_playlist );
    lua_pushinteger( L, i_count );
    return 1;
}

static int vlclua_playlist_enqueue( lua_State *L )
{
    int i_count;
    vlc_object_t *p_this = vlclua_get_this( L );
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    i_count = vlclua_playlist_add_internal( p_this, L, p_playlist,
                                            NULL, false );
    vlclua_release_playlist_internal( p_playlist );
    lua_pushinteger( L, i_count );
    return 1;
}

static void push_playlist_item( lua_State *L, playlist_item_t *p_item );
static void push_playlist_item( lua_State *L, playlist_item_t *p_item )
{
    input_item_t *p_input = p_item->p_input;
    int i_flags = 0;
    i_flags = p_item->i_flags;
    lua_newtable( L );
    lua_pushinteger( L, p_item->i_id );
    lua_setfield( L, -2, "id" );
    lua_newtable( L );
#define CHECK_AND_SET_FLAG( name, label ) \
    if( i_flags & PLAYLIST_ ## name ## _FLAG ) \
    { \
        lua_pushboolean( L, 1 ); \
        lua_setfield( L, -2, #label ); \
    }
    CHECK_AND_SET_FLAG( SAVE, save )
    CHECK_AND_SET_FLAG( SKIP, skip )
    CHECK_AND_SET_FLAG( DBL, disabled )
    CHECK_AND_SET_FLAG( RO, ro )
    CHECK_AND_SET_FLAG( REMOVE, remove )
    CHECK_AND_SET_FLAG( EXPANDED, expanded )
#undef CHECK_AND_SET_FLAG
    lua_setfield( L, -2, "flags" );
    if( p_input )
    {
        lua_pushstring( L, p_input->psz_name );
        lua_setfield( L, -2, "name" );
        lua_pushstring( L, p_input->psz_uri );
        lua_setfield( L, -2, "path" );
        if( p_input->i_duration < 0 )
            lua_pushnumber( L, -1 );
        else
            lua_pushnumber( L, ((double)p_input->i_duration)*1e-6 );
        lua_setfield( L, -2, "duration" );
        lua_pushinteger( L, p_input->i_nb_played );
        lua_setfield( L, -2, "nb_played" );
        /* TODO: add (optional) info categories, meta, options, es */
    }
    if( p_item->i_children >= 0 )
    {
        int i;
        lua_createtable( L, p_item->i_children, 0 );
        for( i = 0; i < p_item->i_children; i++ )
        {
            push_playlist_item( L, p_item->pp_children[i] );
            lua_rawseti( L, -2, i+1 );
        }
        lua_setfield( L, -2, "children" );
    }
}

static int vlclua_playlist_get( lua_State *L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    PL_LOCK;
    int b_category = luaL_optboolean( L, 2, 1 ); /* Default to tree playlist (discared when 1st argument is a playlist_item's id) */
    playlist_item_t *p_item = NULL;

    if( lua_isnumber( L, 1 ) )
    {
        int i_id = lua_tointeger( L, 1 );
        p_item = playlist_ItemGetById( p_playlist, i_id );
        if( !p_item )
        {
            PL_UNLOCK;
            vlclua_release_playlist_internal( p_playlist );
            return 0; /* Should we return an error instead? */
        }
    }
    else if( lua_isstring( L, 1 ) )
    {
        const char *psz_what = lua_tostring( L, 1 );
        if( !strcasecmp( psz_what, "normal" )
         || !strcasecmp( psz_what, "playlist" ) )
            p_item = b_category ? p_playlist->p_local_category
                                : p_playlist->p_local_onelevel;
        else if( !strcasecmp( psz_what, "ml" )
              || !strcasecmp( psz_what, "media library" ) )
            p_item = b_category ? p_playlist->p_ml_category
                                : p_playlist->p_ml_onelevel;
        else if( !strcasecmp( psz_what, "root" ) )
            p_item = b_category ? p_playlist->p_root_category
                                : p_playlist->p_root_onelevel;
        else
        {
#ifdef FIX_THAT_CODE_NOT_TO_MESS_WITH_PLAYLIST_INTERNALS
            for( int i = 0; i < p_playlist->i_sds; i++ )
            {
                if( !strcasecmp( psz_what,
                                 p_playlist->pp_sds[i]->p_sd->psz_module ) )
                {
                    p_item = b_category ? p_playlist->pp_sds[i]->p_cat
                                        : p_playlist->pp_sds[i]->p_one;
                    break;
                }
            }
#else
# warning "Don't access playlist iternal, broken code here."
            abort();
#endif
            if( !p_item )
            {
                PL_UNLOCK;
                vlclua_release_playlist_internal( p_playlist );
                return 0; /* Should we return an error instead? */
            }
        }
    }
    else
    {
        p_item = b_category ? p_playlist->p_root_category
                            : p_playlist->p_root_onelevel;
    }
    push_playlist_item( L, p_item );
    PL_UNLOCK;
    vlclua_release_playlist_internal( p_playlist );
    return 1;
}

static int vlclua_playlist_search( lua_State *L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    const char *psz_string = luaL_optstring( L, 1, "" );
    int b_category = luaL_optboolean( L, 2, 1 ); /* default to category */
    playlist_item_t *p_item = b_category ? p_playlist->p_root_category
                                         : p_playlist->p_root_onelevel;
    PL_LOCK;
    playlist_LiveSearchUpdate( p_playlist, p_item, psz_string );
    PL_UNLOCK;
    push_playlist_item( L, p_item );
    vlclua_release_playlist_internal( p_playlist );
    return 1;
}

static int vlclua_playlist_current( lua_State *L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    lua_pushinteger( L, var_GetInteger( p_playlist, "item-current" ) );
    vlclua_release_playlist_internal( p_playlist );
    return 1;
}

static int vlc_sort_key_from_string( const char *psz_name )
{
    static const struct
    {
        const char *psz_name;
        int i_key;
    } pp_keys[] =
        { { "id", SORT_ID },
          { "title", SORT_TITLE },
          { "title nodes first", SORT_TITLE_NODES_FIRST },
          { "artist", SORT_ARTIST },
          { "genre", SORT_GENRE },
          { "random", SORT_RANDOM },
          { "duration", SORT_DURATION },
          { "title numeric", SORT_TITLE_NUMERIC },
          { "album", SORT_ALBUM },
          { NULL, -1 } };
    int i;
    for( i = 0; pp_keys[i].psz_name; i++ )
    {
        if( !strcmp( psz_name, pp_keys[i].psz_name ) )
            return pp_keys[i].i_key;
    }
    return -1;
}

static int vlclua_playlist_sort( lua_State *L )
{
    /* allow setting the different sort keys */
    int i_mode = vlc_sort_key_from_string( luaL_checkstring( L, 1 ) );
    if( i_mode == -1 )
        return luaL_error( L, "Invalid search key." );
    int i_type = luaL_optboolean( L, 2, 0 ) ? ORDER_REVERSE : ORDER_NORMAL;
    int b_category = luaL_optboolean( L, 3, 1 ); /* default to category */
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    PL_LOCK;
    playlist_item_t *p_root = b_category ? p_playlist->p_local_category
                                         : p_playlist->p_local_onelevel;
    int i_ret = playlist_RecursiveNodeSort( p_playlist, p_root, i_mode,
                                            i_type );
    PL_UNLOCK;
    vlclua_release_playlist_internal( p_playlist );
    return vlclua_push_ret( L, i_ret );
}

/* FIXME: split this in 3 different functions? */
static int vlclua_playlist_status( lua_State *L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    /*
    int i_count = 0;
    lua_settop( L, 0 );*/
    input_thread_t * p_input = playlist_CurrentInput( p_playlist );
    if( p_input )
    {
        /*char *psz_uri =
            input_item_GetURI( input_GetItem( p_playlist->p_input ) );
        lua_pushstring( L, psz_uri );
        free( psz_uri );
        lua_pushnumber( L, config_GetInt( p_intf, "volume" ) );*/
        PL_LOCK;
        switch( playlist_Status( p_playlist ) )
        {
            case PLAYLIST_STOPPED:
                lua_pushstring( L, "stopped" );
                break;
            case PLAYLIST_RUNNING:
                lua_pushstring( L, "playing" );
                break;
            case PLAYLIST_PAUSED:
                lua_pushstring( L, "paused" );
                break;
            default:
                lua_pushstring( L, "unknown" );
                break;
        }
        PL_UNLOCK;
        /*i_count += 3;*/
        vlc_object_release( p_input );
    }
    else
    {
        lua_pushstring( L, "stopped" );
    }
    vlclua_release_playlist_internal( p_playlist );
    return 1;
}

/*****************************************************************************
 *
 *****************************************************************************/
static const luaL_Reg vlclua_playlist_reg[] = {
    { "prev", vlclua_playlist_prev },
    { "next", vlclua_playlist_next },
    { "skip", vlclua_playlist_skip },
    { "play", vlclua_playlist_play },
    { "pause", vlclua_playlist_pause },
    { "stop", vlclua_playlist_stop },
    { "clear", vlclua_playlist_clear },
    { "repeat", vlclua_playlist_repeat },
    { "loop", vlclua_playlist_loop },
    { "random", vlclua_playlist_random },
    { "goto", vlclua_playlist_goto },
    { "add", vlclua_playlist_add },
    { "enqueue", vlclua_playlist_enqueue },
    { "get", vlclua_playlist_get },
    { "search", vlclua_playlist_search },
    { "current", vlclua_playlist_current },
    { "sort", vlclua_playlist_sort },
    { "status", vlclua_playlist_status },
    { NULL, NULL }
};

void luaopen_playlist( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_playlist_reg );
    lua_setfield( L, -2, "playlist" );
}
