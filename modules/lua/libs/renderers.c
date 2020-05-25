/*****************************************************************************
 * renderers.c
 *****************************************************************************
 * Copyright (C) 2020 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_player.h>
#include <vlc_renderer_discovery.h>
#include <vlc_vector.h>

#include "../vlc.h"
#include "../libs.h"

#include "misc.h"

typedef struct vlclua_renderer_item
{
    uint32_t id;
    vlc_renderer_item_t* item;
} vlclua_renderer_item;

struct rd_items_vec VLC_VECTOR(vlclua_renderer_item);

typedef struct vlclua_rd_sys_t
{
    struct vlc_renderer_discovery_owner owner;
    vlc_renderer_discovery_t* rd;
    struct rd_items_vec items;
    vlc_mutex_t mutex;
    uint32_t last_renderer_id;
} vlclua_rd_sys_t;

static bool vlclua_renderer_compare( vlc_renderer_item_t* rhs,
                                     vlc_renderer_item_t* lhs )
{
    return !strcmp( vlc_renderer_item_name( rhs ), vlc_renderer_item_name( lhs ) ) &&
           !strcmp( vlc_renderer_item_type( rhs ), vlc_renderer_item_type( lhs ) );
}

static int vlclua_rd_list( lua_State* L )
{
    vlclua_rd_sys_t* sys = (vlclua_rd_sys_t*)luaL_checkudata( L, 1, "renderer_discoverer" );

    vlc_renderer_item_t *activeRenderer;
    vlc_player_t* player = vlclua_get_player_internal( L );
    vlc_player_Lock( player );
    activeRenderer = vlc_player_GetRenderer( player );
    if ( activeRenderer )
        vlc_renderer_item_hold( activeRenderer );
    vlc_player_Unlock( player );

    lua_createtable( L, sys->items.size, 0 );
    vlc_mutex_lock( &sys->mutex );
    for ( size_t i = 0; i < sys->items.size; ++i )
    {
        lua_newtable( L );

        lua_pushinteger( L, sys->items.data[i].id );
        lua_setfield( L, -2, "id" );
        lua_pushstring( L, vlc_renderer_item_name( sys->items.data[i].item ) );
        lua_setfield( L, -2, "name" );
        lua_pushstring( L, vlc_renderer_item_type( sys->items.data[i].item ) );
        lua_setfield( L, -2, "type" );

        bool selected = activeRenderer != NULL &&
                vlclua_renderer_compare( activeRenderer, sys->items.data[i].item );
        lua_pushboolean( L, selected );
        lua_setfield( L, -2, "selected" );

        lua_rawseti( L, -2, i + 1 );
    }
    vlc_mutex_unlock( &sys->mutex );

    if ( activeRenderer )
        vlc_renderer_item_release( activeRenderer );

    return 1;
}

static int vlclua_rd_select( lua_State* L )
{
    vlc_player_t* player = vlclua_get_player_internal( L );
    if ( !player )
        return 0;
    vlclua_rd_sys_t* sys = (vlclua_rd_sys_t*)luaL_checkudata( L, 1, "renderer_discoverer" );
    lua_Integer id = luaL_checkinteger( L, 2 );
    if ( id < 0 )
    {
        vlc_player_Lock( player );
        vlc_player_SetRenderer( player, NULL );
        vlc_player_Unlock( player );
    }
    vlc_mutex_lock( &sys->mutex );
    for ( size_t i = 0; i < sys->items.size; ++i )
    {
        if ( sys->items.data[i].id != id )
            continue;
        vlc_player_Lock( player );
        vlc_player_SetRenderer( player, sys->items.data[i].item );
        vlc_player_Unlock( player );
    }
    vlc_mutex_unlock( &sys->mutex );
    return 0;
}

static const luaL_Reg vlclua_rd_obj_reg[] = {
    { "list", vlclua_rd_list},
    { "select", vlclua_rd_select },
    { NULL, NULL }
};

static int vlclua_rd_obj_delete( lua_State *L )
{
    vlclua_rd_sys_t* sys = (vlclua_rd_sys_t*)luaL_checkudata( L, 1, "renderer_discoverer" );
    vlc_rd_release( sys->rd );
    for ( size_t i = 0; i < sys->items.size; ++i )
        vlc_renderer_item_release( sys->items.data[i].item );
    vlc_vector_destroy( &sys->items );
    return 0;
}

static void vlclua_rd_on_item_added( struct vlc_renderer_discovery_t* rd,
                                      struct vlc_renderer_item_t* item )
{
    vlclua_rd_sys_t* sys = rd->owner.sys;
    vlc_mutex_lock( &sys->mutex );
    vlclua_renderer_item i =
    {
        sys->last_renderer_id++,
        vlc_renderer_item_hold( item ),
    };
    vlc_vector_push( &sys->items, i );
    vlc_mutex_unlock( &sys->mutex );
}

static void vlclua_rd_on_item_removed( struct vlc_renderer_discovery_t* rd,
                                        struct vlc_renderer_item_t* item )
{
    vlclua_rd_sys_t* sys = rd->owner.sys;

    vlc_mutex_lock( &sys->mutex );
    for ( size_t i = 0; i < sys->items.size; ++i )
    {
        if ( !vlclua_renderer_compare( item, sys->items.data[i].item ) )
            continue;
        vlc_vector_remove( &sys->items, i );
        break;
    }
    vlc_mutex_unlock( &sys->mutex );
}

static int vlclua_rd_create( lua_State* L )
{
    vlclua_rd_sys_t* sys = lua_newuserdata( L, sizeof( *sys ) );
    if ( !sys )
        return 0;
    sys->owner.sys = sys;
    sys->owner.item_added = vlclua_rd_on_item_added;
    sys->owner.item_removed = vlclua_rd_on_item_removed;
    vlc_vector_init( &sys->items );
    vlc_mutex_init( &sys->mutex );
    sys->last_renderer_id = 0;

    vlc_object_t *this = vlclua_get_this( L );
    const char* name = luaL_checkstring( L, 1 );

    sys->rd = vlc_rd_new( this, name, &sys->owner );
    if ( !sys->rd )
    {
        vlc_vector_destroy( &sys->items );
        return 0;
    }

    if ( luaL_newmetatable( L, "renderer_discoverer" ) )
    {
        lua_newtable( L );
        luaL_register( L, NULL, vlclua_rd_obj_reg );
        lua_setfield( L, -2, "__index" );
        lua_pushcfunction( L, vlclua_rd_obj_delete );
        lua_setfield( L, -2, "__gc" );
    }
    lua_setmetatable( L, -2 );

    return 1;
}

static const luaL_Reg vlclua_rd_reg[] = {
    { "create", vlclua_rd_create },
    { NULL, NULL }
};

void luaopen_rd( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_rd_reg );
    lua_setfield( L, -2, "rd" );
}
