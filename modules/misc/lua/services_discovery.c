/*****************************************************************************
 * services_discovery.c : Services discovery using lua scripts
 *****************************************************************************
 * Copyright (C) 2009 VideoLAN and AUTHORS
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

static const char * const ppsz_sd_options[] = { "sd", "longname", NULL };

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
    lua_State *L = NULL;
    char *psz_name = NULL;

    if( !strcmp(p_sd->psz_name, "lua"))
    {
        // We want to load the module name "lua"
        // This module can be used to load lua script not registered
        // as builtin lua SD modules.
        config_ChainParse( p_sd, "lua-", ppsz_sd_options, p_sd->p_cfg );
        psz_name = var_CreateGetString( p_sd, "lua-sd" );
    }
    else
    {
        // We are loading a builtin lua sd module.
        psz_name = strdup(p_sd->psz_name);
    }

    if( !( p_sys = malloc( sizeof( services_discovery_sys_t ) ) ) )
    {
        free( psz_name );
        return VLC_ENOMEM;
    }
    p_sd->p_sys = p_sys;
    p_sys->psz_filename = vlclua_find_file( p_this, "sd", psz_name );
    if( !p_sys->psz_filename )
    {
        msg_Err( p_sd, "Couldn't find lua services discovery script \"%s\".",
                 psz_name );
        free( psz_name );
        goto error;
    }
    free( psz_name );
    L = luaL_newstate();
    if( !L )
    {
        msg_Err( p_sd, "Could not create new Lua State" );
        goto error;
    }
    vlclua_set_this( L, p_sd );
    luaL_openlibs( L );
    luaL_register( L, "vlc", p_reg );
    luaopen_input( L );
    luaopen_msg( L );
    luaopen_misc( L );
    luaopen_net( L );
    luaopen_object( L );
    luaopen_sd( L );
    luaopen_strings( L );
    luaopen_variables( L );
    luaopen_stream( L );
    luaopen_gettext( L );
    luaopen_xml( L );
    luaopen_md5( L );
    lua_pop( L, 1 );

    if( vlclua_add_modules_path( p_sd, L, p_sys->psz_filename ) )
    {
        msg_Warn( p_sd, "Error while setting the module search path for %s",
                  p_sys->psz_filename );
        goto error;
    }
    if( luaL_dofile( L, p_sys->psz_filename ) )
    {

        msg_Err( p_sd, "Error loading script %s: %s", p_sys->psz_filename,
                  lua_tostring( L, lua_gettop( L ) ) );
        lua_pop( L, 1 );
        goto error;
    }
    p_sys->L = L;
    if( vlc_clone (&p_sd->p_sys->thread, Run, p_sd, VLC_THREAD_PRIORITY_LOW) )
    {
        goto error;
    }
    return VLC_SUCCESS;
error:
    if( L )
        lua_close( L );
    free( p_sys->psz_filename );
    free( p_sys );
    return VLC_EGENERIC;
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
        msg_Err( p_sd, "Error while running script %s, "
                  "function main(): %s", p_sys->psz_filename,
                  lua_tostring( L, lua_gettop( L ) ) );
        lua_pop( L, 1 );
        return NULL;
    }
    msg_Dbg( p_sd, "LuaSD script loaded: %s", p_sys->psz_filename );

    /* Force garbage collection, because the core will keep the SD
     * open, but lua will never gc until lua_close(). */
    lua_gc( L, LUA_GCCOLLECT, 0 );

    return NULL;
}
