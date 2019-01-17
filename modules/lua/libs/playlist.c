/*****************************************************************************
 * playlist.c
 *****************************************************************************
 * Copyright (C) 2007-2011 the VideoLAN team
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
#include <vlc_playlist_legacy.h>

#include "../vlc.h"
#include "../libs.h"
#include "input.h"
#include "variables.h"
#include "misc.h"

void vlclua_set_playlist_internal( lua_State *L, playlist_t *pl )
{
    vlclua_set_object( L, vlclua_set_playlist_internal, pl );
}

playlist_t *vlclua_get_playlist_internal( lua_State *L )
{
    return vlclua_get_object( L, vlclua_set_playlist_internal );
}

static int vlclua_playlist_prev( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    playlist_Prev( p_playlist );
    return 0;
}

static int vlclua_playlist_next( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    playlist_Next( p_playlist );
    return 0;
}

static int vlclua_playlist_skip( lua_State * L )
{
    int i_skip = luaL_checkinteger( L, 1 );
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    playlist_Skip( p_playlist, i_skip );
    return 0;
}

static int vlclua_playlist_play( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    playlist_Play( p_playlist );
    return 0;
}

static int vlclua_playlist_pause( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    playlist_TogglePause( p_playlist );
    return 0;
}

static int vlclua_playlist_stop( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    playlist_Stop( p_playlist );
    return 0;
}

static int vlclua_playlist_clear( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    playlist_Stop( p_playlist ); /* Isn't this already implied by Clear? */
    playlist_Clear( p_playlist, pl_Unlocked );
    return 0;
}

static int vlclua_playlist_repeat( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    int i_ret = vlclua_var_toggle_or_set( L, p_playlist, "repeat" );
    return i_ret;
}

static int vlclua_playlist_loop( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    int i_ret = vlclua_var_toggle_or_set( L, p_playlist, "loop" );
    return i_ret;
}

static int vlclua_playlist_random( lua_State * L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    int i_ret = vlclua_var_toggle_or_set( L, p_playlist, "random" );
    return i_ret;
}

static int vlclua_playlist_gotoitem( lua_State * L )
{
    int i_id = luaL_checkinteger( L, 1 );
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    PL_LOCK;
    playlist_ViewPlay( p_playlist, NULL,
                       playlist_ItemGetById( p_playlist, i_id ) );
    PL_UNLOCK;
    return vlclua_push_ret( L, VLC_SUCCESS );
}

static int vlclua_playlist_delete( lua_State * L )
{
    int i_id = luaL_checkinteger( L, 1 );
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );

    PL_LOCK;
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist, i_id );
    if( p_item != NULL )
       playlist_NodeDelete( p_playlist, p_item );
    PL_UNLOCK;

    return vlclua_push_ret( L, (p_item != NULL) ? 0 : -1 );
}

static int vlclua_playlist_move( lua_State * L )
{
    int i_item = luaL_checkinteger( L, 1 );
    int i_target = luaL_checkinteger( L, 2 );
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    PL_LOCK;
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist, i_item );
    playlist_item_t *p_target = playlist_ItemGetById( p_playlist, i_target );
    if( !p_item || !p_target )
    {
       PL_UNLOCK;
       return vlclua_push_ret( L, -1 );
    }
    int i_ret;
    if( p_target->i_children != -1 )
        i_ret = playlist_TreeMove( p_playlist, p_item, p_target, 0 );
    else
        i_ret = playlist_TreeMove( p_playlist, p_item, p_target->p_parent, p_target->i_id - p_target->p_parent->pp_children[0]->i_id + 1 );
    PL_UNLOCK;
    return vlclua_push_ret( L, i_ret );
}

static int vlclua_playlist_add_common(lua_State *L, bool play)
{
    vlc_object_t *obj = vlclua_get_this(L);
    playlist_t *playlist = vlclua_get_playlist_internal(L);
    int count = 0;

    /* playlist */
    if (!lua_istable(L, -1))
    {
        msg_Warn(obj, "Playlist should be a table.");
        return 0;
    }

    lua_pushnil(L);

    /* playlist nil */
    while (lua_next(L, -2))
    {
        input_item_t *item = vlclua_read_input_item(obj, L);
        if (item != NULL)
        {
            /* Play or Enqueue (preparse) */
            /* FIXME: playlist_AddInput() can fail */
            playlist_AddInput(playlist, item, play);
            input_item_Release(item);
            count++;
        }
        /* pop the value, keep the key for the next lua_next() call */
        lua_pop(L, 1);
    }
    /* playlist */

    lua_pushinteger(L, count);
    return 1;
}

static int vlclua_playlist_add( lua_State *L )
{
    return vlclua_playlist_add_common(L, true);
}

static int vlclua_playlist_enqueue( lua_State *L )
{
    return vlclua_playlist_add_common(L, false);
}

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
    CHECK_AND_SET_FLAG( DBL, disabled )
    CHECK_AND_SET_FLAG( RO, ro )
#undef CHECK_AND_SET_FLAG
    lua_setfield( L, -2, "flags" );
    if( p_input )
    {
        /* Apart from nb_played, these fields unfortunately duplicate
           fields already available from the input item */
        char *psz_name = input_item_GetTitleFbName( p_input );
        lua_pushstring( L, psz_name );
        free( psz_name );
        lua_setfield( L, -2, "name" );
        lua_pushstring( L, p_input->psz_uri );
        lua_setfield( L, -2, "path" );
        if( p_input->i_duration < 0 )
            lua_pushnumber( L, -1 );
        else
            lua_pushnumber( L, secf_from_vlc_tick(p_input->i_duration) );
        lua_setfield( L, -2, "duration" );
        lua_pushinteger( L, p_item->i_nb_played );
        lua_setfield( L, -2, "nb_played" );
        luaopen_input_item( L, p_input );
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
    playlist_item_t *p_item = NULL;

    if( lua_isnumber( L, 1 ) )
    {
        int i_id = lua_tointeger( L, 1 );
        p_item = playlist_ItemGetById( p_playlist, i_id );
        if( !p_item )
        {
            PL_UNLOCK;
            return 0; /* Should we return an error instead? */
        }
    }
    else if( lua_isstring( L, 1 ) )
    {
        const char *psz_what = lua_tostring( L, 1 );
        if( !strcasecmp( psz_what, "normal" )
         || !strcasecmp( psz_what, "playlist" ) )
            p_item = p_playlist->p_playing;
        else if( !strcasecmp( psz_what, "root" ) )
            p_item = &p_playlist->root;
        else
        {
            /* currently, psz_what must be SD module's longname! */
            p_item = playlist_ChildSearchName( &p_playlist->root, psz_what );

            if( !p_item )
            {
                PL_UNLOCK;
                return 0; /* Should we return an error instead? */
            }
        }
    }
    else
    {
        p_item = &p_playlist->root;
    }
    push_playlist_item( L, p_item );
    PL_UNLOCK;
    return 1;
}

static int vlclua_playlist_search( lua_State *L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    const char *psz_string = luaL_optstring( L, 1, "" );
    PL_LOCK;
    playlist_LiveSearchUpdate( p_playlist, &p_playlist->root, psz_string, true );
    push_playlist_item( L, &p_playlist->root );
    PL_UNLOCK;
    return 1;
}

static int vlclua_playlist_current( lua_State *L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    playlist_item_t *item;
    int id = -1;

    PL_LOCK;
    item = playlist_CurrentPlayingItem( p_playlist );
    if( item != NULL )
        id = item->i_id;
    PL_UNLOCK;
    lua_pushinteger( L, id );
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
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    PL_LOCK;
    int i_ret = playlist_RecursiveNodeSort( p_playlist, p_playlist->p_playing,
                                            i_mode, i_type );
    PL_UNLOCK;
    return vlclua_push_ret( L, i_ret );
}

static int vlclua_playlist_status( lua_State *L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    PL_LOCK;
    int status = playlist_Status( p_playlist );
    PL_UNLOCK;
    switch( status )
    {
        case PLAYLIST_STOPPED:
            lua_pushliteral( L, "stopped" );
            break;
        case PLAYLIST_RUNNING:
            lua_pushliteral( L, "playing" );
            break;
        case PLAYLIST_PAUSED:
            lua_pushliteral( L, "paused" );
            break;
        default:
            lua_pushliteral( L, "unknown" );
            break;
    }
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
    { "repeat", vlclua_playlist_repeat }, // repeat is a reserved lua keyword...
    { "repeat_", vlclua_playlist_repeat }, // ... provide repeat_ too.
    { "loop", vlclua_playlist_loop },
    { "random", vlclua_playlist_random },
#if LUA_VERSION_NUM < 502
    { "goto", vlclua_playlist_gotoitem },
#endif
    { "gotoitem", vlclua_playlist_gotoitem },
    { "add", vlclua_playlist_add },
    { "enqueue", vlclua_playlist_enqueue },
    { "get", vlclua_playlist_get },
    { "search", vlclua_playlist_search },
    { "current", vlclua_playlist_current },
    { "sort", vlclua_playlist_sort },
    { "status", vlclua_playlist_status },
    { "delete", vlclua_playlist_delete },
    { "move", vlclua_playlist_move },
    { NULL, NULL }
};

void luaopen_playlist( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_playlist_reg );
    lua_setfield( L, -2, "playlist" );
}
