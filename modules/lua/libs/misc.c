/*****************************************************************************
 * misc.c
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea at videolan tod org>
 *          Pierre d'Herbemont <pdherbemont # videolan.org>
 *          Rémi Duraffort <ivoire # videolan tod org>
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
#include <vlc_plugin.h>
#include <vlc_meta.h>
#include <vlc_interface.h>
#include <vlc_keys.h>

#include "../vlc.h"
#include "../libs.h"

/*****************************************************************************
 * Internal lua<->vlc utils
 *****************************************************************************/
static void vlclua_set_object( lua_State *L, void *id, void *value )
{
    lua_pushlightuserdata( L, id );
    lua_pushlightuserdata( L, value );
    lua_rawset( L, LUA_REGISTRYINDEX );
}

static void *vlclua_get_object( lua_State *L, void *id )
{
    lua_pushlightuserdata( L, id );
    lua_rawget( L, LUA_REGISTRYINDEX );
    const void *p = lua_topointer( L, -1 );
    lua_pop( L, 1 );
    return (void *)p;
}

#undef vlclua_set_this
void vlclua_set_this( lua_State *L, vlc_object_t *p_this )
{
    vlclua_set_object( L, vlclua_set_this, p_this );
}

vlc_object_t * vlclua_get_this( lua_State *L )
{
    return vlclua_get_object( L, vlclua_set_this );
}

void vlclua_set_intf( lua_State *L, intf_sys_t *p_intf )
{
    vlclua_set_object( L, vlclua_set_intf, p_intf );
}

static intf_sys_t * vlclua_get_intf( lua_State *L )
{
    return vlclua_get_object( L, vlclua_set_intf );
}

/*****************************************************************************
 * VLC error code translation
 *****************************************************************************/
int vlclua_push_ret( lua_State *L, int i_error )
{
    lua_pushnumber( L, i_error );
    lua_pushstring( L, vlc_error( i_error ) );
    return 2;
}

/*****************************************************************************
 * Get the VLC version string
 *****************************************************************************/
static int vlclua_version( lua_State *L )
{
    lua_pushstring( L, VERSION_MESSAGE );
    return 1;
}

/*****************************************************************************
 * Get the VLC copyright
 *****************************************************************************/
static int vlclua_copyright( lua_State *L )
{
    lua_pushliteral( L, COPYRIGHT_MESSAGE );
    return 1;
}

/*****************************************************************************
 * Get the VLC license msg/disclaimer
 *****************************************************************************/
static int vlclua_license( lua_State *L )
{
    lua_pushstring( L, LICENSE_MSG );
    return 1;
}

/*****************************************************************************
 * Quit VLC
 *****************************************************************************/
static int vlclua_quit( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    /* The rc.c code also stops the playlist ... not sure if this is needed
     * though. */
    libvlc_Quit( p_this->p_libvlc );
    return 0;
}

/*****************************************************************************
 *
 *****************************************************************************/
static int vlclua_lock_and_wait( lua_State *L )
{
    intf_sys_t *p_sys = vlclua_get_intf( L );

    vlc_mutex_lock( &p_sys->lock );
    mutex_cleanup_push( &p_sys->lock );
    while( !p_sys->exiting )
        vlc_cond_wait( &p_sys->wait, &p_sys->lock );
    vlc_cleanup_run();
    lua_pushboolean( L, 1 );
    return 1;
}

static int vlclua_mdate( lua_State *L )
{
    lua_pushnumber( L, mdate() );
    return 1;
}

static int vlclua_mwait( lua_State *L )
{
    double f = luaL_checknumber( L, 1 );
    mwait( (int64_t)f );
    return 0;
}

static int vlclua_intf_should_die( lua_State *L )
{
    intf_sys_t *p_sys = vlclua_get_intf( L );
    lua_pushboolean( L, p_sys->exiting );
    return 1;
}

static int vlclua_action_id( lua_State *L )
{
    vlc_action_t i_key = vlc_GetActionId( luaL_checkstring( L, 1 ) );
    if (i_key == 0)
        return 0;
    lua_pushnumber( L, i_key );
    return 1;
}

/*****************************************************************************
 * Timer functions
 *****************************************************************************/
static int vlclua_timer_schedule( lua_State *L );
static int vlclua_timer_getoverrun( lua_State *L);

static const luaL_Reg vlclua_timer_reg[] = {
    { "schedule",   vlclua_timer_schedule   },
    { "getoverrun", vlclua_timer_getoverrun },
    { NULL,         NULL                    }
};

typedef struct
{
    lua_State *L;
    vlc_timer_t timer;
    char *psz_callback;
} vlclua_timer_t;

static int vlclua_timer_schedule( lua_State *L )
{
    vlclua_timer_t **pp_timer = (vlclua_timer_t**)luaL_checkudata( L, 1, "timer" );
    if( !pp_timer || !*pp_timer )
        luaL_error( L, "Can't get pointer to timer" );

    bool b_relative = luaL_checkboolean( L, 2 );
    mtime_t i_value = luaL_checkinteger( L, 3 );
    mtime_t i_interval = luaL_checkinteger( L, 4 );

    vlc_timer_schedule( (*pp_timer)->timer, b_relative, i_value, i_interval );
    return 0;
}

static int vlclua_timer_getoverrun( lua_State *L )
{
    vlclua_timer_t **pp_timer = (vlclua_timer_t**)luaL_checkudata(L, 1, "timer" );
    if( !pp_timer || !*pp_timer )
        luaL_error( L, "Can't get pointer to timer" );

    lua_pushinteger( L, vlc_timer_getoverrun( (*pp_timer)->timer ) );
    return 1;
}

static void vlclua_timer_callback( void *data )
{
    vlclua_timer_t *p_timer = (vlclua_timer_t*)data;
    lua_State *L = p_timer->L;

    lua_getglobal( L, p_timer->psz_callback );
    if( lua_pcall( L, 0, 0, 0 ) )
    {
        const char *psz_err = lua_tostring( L, -1 );
        msg_Err( vlclua_get_this( L ), "Error while running the timer callback: '%s'", psz_err );
        lua_settop( L, 0 );
    }
}

static int vlclua_timer_delete( lua_State *L )
{
    vlclua_timer_t **pp_timer = (vlclua_timer_t**)luaL_checkudata( L, 1, "timer" );
    if( !pp_timer || !*pp_timer )
        luaL_error( L, "Can't get pointer to timer" );

    vlc_timer_destroy( (*pp_timer)->timer );
    free( (*pp_timer)->psz_callback );
    free( (*pp_timer) );
    return 0;
}

static int vlclua_timer_create( lua_State *L )
{
    if( !lua_isstring( L, 1 ) )
        return luaL_error( L, "timer(function_name)" );

    vlclua_timer_t *p_timer = malloc( sizeof( vlclua_timer_t ) );
    if( vlc_timer_create( &p_timer->timer, vlclua_timer_callback, p_timer ) )
    {
        free( p_timer );
        return luaL_error( L, "Cannot initialize the timer" );
    }

    p_timer->L = L;
    p_timer->psz_callback = strdup( luaL_checkstring( L, 1 ) );

    vlclua_timer_t **pp_timer = lua_newuserdata( L, sizeof( vlclua_timer_t* ) );
    *pp_timer = p_timer;

    /* Create the object */
    if( luaL_newmetatable( L, "timer" ) )
    {
        lua_newtable( L );
        luaL_register( L, NULL, vlclua_timer_reg );
        lua_setfield( L, -2, "__index" );
        lua_pushcfunction( L, vlclua_timer_delete );
        lua_setfield( L, -2, "__gc" );
    }
    lua_setmetatable( L, -2 );

    return 1;
}

/*****************************************************************************
 *
 *****************************************************************************/
static const luaL_Reg vlclua_misc_reg[] = {
    { "version", vlclua_version },
    { "copyright", vlclua_copyright },
    { "license", vlclua_license },

    { "action_id", vlclua_action_id },

    { "mdate", vlclua_mdate },
    { "mwait", vlclua_mwait },

    { "lock_and_wait", vlclua_lock_and_wait },

    { "should_die", vlclua_intf_should_die },
    { "quit", vlclua_quit },

    { "timer", vlclua_timer_create },

    { NULL, NULL }
};

void luaopen_misc( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_misc_reg );
    lua_setfield( L, -2, "misc" );
}
