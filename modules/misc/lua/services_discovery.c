/*****************************************************************************
 * services_discovery.c : Services discovery using lua scripts
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 *
 * Authors: Fabio Ritrovato <sephiroth87 at videolan dot org>
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
#include <vlc_services_discovery.h>
#include <vlc_playlist.h>

#include "vlc.h"
#include "libs.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void *Run( void * );

static const char * const ppsz_sd_options[] = { "sd", NULL };

/*****************************************************************************
 * Local structures
 *****************************************************************************/
struct services_discovery_sys_t
{
    lua_State *L;
    char *psz_filename;
    vlc_thread_t thread;
};
static const luaL_Reg p_reg[] = { { NULL, NULL } };

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
int Open_LuaSD( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t * )p_this;
    services_discovery_sys_t *p_sys;
    lua_State *L;
    char *psz_name = NULL;

    config_ChainParse( p_sd, "lua-", ppsz_sd_options, p_sd->p_cfg );
    psz_name = var_CreateGetString( p_sd, "lua-sd" );
    if( !( p_sys = malloc( sizeof( services_discovery_sys_t ) ) ) )
        return VLC_ENOMEM;
    p_sd->p_sys = p_sys;
    p_sys->psz_filename = vlclua_find_file( p_this, "sd", psz_name );
    if( !p_sys->psz_filename )
    {
        msg_Err( p_sd, "Couldn't find lua services discovery script \"%s\".",
                 psz_name );
        free( psz_name );
        free( p_sys );
        return VLC_EGENERIC;
    }
    free( psz_name );
    L = luaL_newstate();
    if( !L )
    {
        msg_Err( p_sd, "Could not create new Lua State" );
        free( p_sys->psz_filename );
        free( p_sys );
        return VLC_EGENERIC;
    }
    luaL_openlibs( L );
    luaL_register( L, "vlc", p_reg );
    lua_pushlightuserdata( L, p_sd );
    lua_setfield( L, -2, "private" );
    luaopen_input( L );
    luaopen_msg( L );
    luaopen_misc( L );
    luaopen_net( L );
    luaopen_object( L );
    luaopen_playlist( L );
    luaopen_sd( L );
    luaopen_strings( L );
    luaopen_variables( L );
    luaopen_stream( L );
    luaopen_gettext( L );
    luaopen_xml( L );
    lua_pop( L, 1 );
    if( luaL_dofile( L, p_sys->psz_filename ) )
    {

        msg_Err( p_sd, "Error loading script %s: %s", p_sys->psz_filename,
                  lua_tostring( L, lua_gettop( L ) ) );
        lua_pop( L, 1 );
        free( p_sys->psz_filename );
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_sys->L = L;
    if( vlc_clone (&p_sd->p_sys->thread, Run, p_sd, VLC_THREAD_PRIORITY_LOW) )
    {
        free( p_sys->psz_filename );
        free( p_sys );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: cleanup
 *****************************************************************************/
void Close_LuaSD( vlc_object_t *p_this )
{
    services_discovery_t *p_sd = ( services_discovery_t * )p_this;

    vlc_join (p_sd->p_sys->thread, NULL);
    free( p_sd->p_sys->psz_filename );
    lua_close( p_sd->p_sys->L );
    free( p_sd->p_sys );
}

/*****************************************************************************
 * Run: Thread entry-point
 ****************************************************************************/
static void* Run( void *data )
{
    services_discovery_t *p_sd = ( services_discovery_t * )data;
    services_discovery_sys_t *p_sys = p_sd->p_sys;
    lua_State *L = p_sys->L;

    lua_getglobal( L, "main" );
    if( !lua_isfunction( L, lua_gettop( L ) ) || lua_pcall( L, 0, 1, 0 ) )
    {
        msg_Err( p_sd, "Error while runing script %s, "
                  "function main(): %s", p_sys->psz_filename,
                  lua_tostring( L, lua_gettop( L ) ) );
        lua_pop( L, 1 );
        return NULL;
    }
    //msg_Info( p_sd, "LuaSD script loaded: %s", p_sd->psz_name );
    return NULL;
}
