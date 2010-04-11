/*****************************************************************************
 * intf.c: Generic lua interface functions
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

#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_aout.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */
#include <lualib.h>     /* Lua libs */

#include "vlc.h"
#include "libs.h"

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
static void *Run( void * );

static const char * const ppsz_intf_options[] = { "intf", "config", NULL };

/*****************************************************************************
 *
 *****************************************************************************/
static inline void luaL_register_submodule( lua_State *L, const char *psz_name,
                                            const luaL_Reg *l )
{
    lua_newtable( L );
    luaL_register( L, NULL, l );
    lua_setfield( L, -2, psz_name );
}

static const struct
{
    const char *psz_shortcut;
    const char *psz_name;
} pp_shortcuts[] = {
    { "luarc", "rc" },
    { "rc", "rc" },
    { "luahotkeys", "hotkeys" },
    /* { "hotkeys", "hotkeys" }, */
    { "luatelnet", "telnet" },
    { "telnet", "telnet" },
    { "luahttp", "http" },
    { "http", "http" },
    { NULL, NULL } };

static const char *WordInList( const char *psz_list, const char *psz_word )
{
    for( ;; )
    {
        const char *end = strchr( psz_list, ',' );
        if( end == NULL )
            break;

        if( !strncmp( psz_list, psz_word, end - psz_list ) )
            return psz_list;
        psz_list = end + 1;
    }
    return !strcmp( psz_list, psz_word ) ? psz_list : NULL;
}

static char *GetModuleName( intf_thread_t *p_intf )
{
    int i;
    const char *psz_intf;
    /*if( *p_intf->psz_intf == '$' )
        psz_intf = var_GetString( p_intf, p_intf->psz_intf+1 );
    else*/
        psz_intf = p_intf->psz_intf;

    int i_candidate = -1;
    const char *psz_candidate = NULL;
    for( i = 0; pp_shortcuts[i].psz_name; i++ )
    {
        const char *psz_match;
        if( ( psz_match = WordInList( psz_intf, pp_shortcuts[i].psz_shortcut ) ) )
        {
            if( !psz_candidate || psz_match < psz_candidate )
            {
                psz_candidate = psz_match;
                i_candidate = i;
            }
        }
    }

    if( i_candidate >= 0 )
        return strdup( pp_shortcuts[i_candidate].psz_name );

    return var_CreateGetString( p_intf, "lua-intf" );
}

static const luaL_Reg p_reg[] = { { NULL, NULL } };

int Open_LuaIntf( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    intf_sys_t *p_sys;
    lua_State *L;

    config_ChainParse( p_intf, "lua-", ppsz_intf_options, p_intf->p_cfg );
    char *psz_name = NULL;

    if( !p_intf->psz_intf || !*p_intf->psz_intf )
        psz_name = strdup( "rc" );
    else
        psz_name = GetModuleName( p_intf );

    if( !psz_name ) psz_name = strdup( "dummy" );

    char *psz_config;
    bool b_config_set = false;

    p_intf->p_sys = (intf_sys_t*)malloc( sizeof(intf_sys_t) );
    if( !p_intf->p_sys )
    {
        free( psz_name );
        return VLC_ENOMEM;
    }
    p_sys = p_intf->p_sys;
    p_sys->psz_filename = vlclua_find_file( p_this, "intf", psz_name );
    if( !p_sys->psz_filename )
    {
        msg_Err( p_intf, "Couldn't find lua interface script \"%s\".",
                 psz_name );
        goto error;
    }
    msg_Dbg( p_intf, "Found lua interface script: %s", p_sys->psz_filename );

    L = luaL_newstate();
    if( !L )
    {
        msg_Err( p_intf, "Could not create new Lua State" );
        goto error;
    }

    vlclua_set_this( L, p_intf );
    vlclua_set_intf( L, p_sys );

    luaL_openlibs( L );

    /* register our functions */
    luaL_register( L, "vlc", p_reg );

    /* register submodules */
    luaopen_acl( L );
    luaopen_config( L );
    luaopen_volume( L );
    luaopen_httpd( L );
    luaopen_input( L );
    luaopen_msg( L );
    luaopen_misc( L );
    luaopen_net( L );
    luaopen_object( L );
    luaopen_osd( L );
    luaopen_playlist( L );
    luaopen_sd( L );
    luaopen_stream( L );
    luaopen_strings( L );
    luaopen_variables( L );
    luaopen_video( L );
    luaopen_vlm( L );
    luaopen_volume( L );
    luaopen_gettext( L );
    luaopen_xml( L );
    luaopen_md5( L );

    /* clean up */
    lua_pop( L, 1 );

    /* Setup the module search path */
    if( vlclua_add_modules_path( p_intf, L, p_sys->psz_filename ) )
    {
        msg_Warn( p_intf, "Error while setting the module search path for %s",
                  p_sys->psz_filename );
        lua_close( L );
        goto error;
    }

    psz_config = var_CreateGetString( p_intf, "lua-config" );
    if( psz_config && *psz_config )
    {
        char *psz_buffer;
        if( asprintf( &psz_buffer, "config={%s}", psz_config ) != -1 )
        {
            msg_Dbg( p_intf, "Setting config variable: %s", psz_buffer );
            if( luaL_dostring( L, psz_buffer ) == 1 )
                msg_Err( p_intf, "Error while parsing \"lua-config\"." );
            free( psz_buffer );
            lua_getglobal( L, "config" );
            if( lua_istable( L, -1 ) )
            {
                lua_getfield( L, -1, psz_name );
                if( lua_istable( L, -1 ) )
                {
                    lua_setglobal( L, "config" );
                    b_config_set = true;
                }
            }
        }
    }
    free( psz_config );

    if( b_config_set == false )
    {
        lua_newtable( L );
        lua_setglobal( L, "config" );
    }

    p_sys->L = L;

    p_intf->psz_header = psz_name;
    /* ^^ Do I need to clean that up myself in Close_LuaIntf? */

    vlc_mutex_init( &p_sys->lock );
    vlc_cond_init( &p_sys->wait );
    p_sys->exiting = false;

    if( vlc_clone( &p_sys->thread, Run, p_intf, VLC_THREAD_PRIORITY_LOW ) )
    {
        p_intf->psz_header = NULL;
        vlc_cond_destroy( &p_sys->wait );
        vlc_mutex_destroy( &p_sys->lock );
        lua_close( p_sys->L );
        goto error;
    }

    return VLC_SUCCESS;
error:
    free( p_sys->psz_filename );
    free( p_sys );
    free( psz_name );
    return VLC_EGENERIC;
}

void Close_LuaIntf( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    vlc_cancel( p_sys->thread );

    vlc_mutex_lock( &p_sys->lock );
    p_sys->exiting = true;
    vlc_cond_signal( &p_sys->wait );
    vlc_mutex_unlock( &p_sys->lock );
    vlc_join( p_sys->thread, NULL );
    vlc_cond_destroy( &p_sys->wait );
    vlc_mutex_destroy( &p_sys->lock );

    lua_close( p_sys->L );

    free( p_sys->psz_filename );
    free( p_sys );
}

static void *Run( void *data )
{
    intf_thread_t *p_intf = data;
    intf_sys_t *p_sys = p_intf->p_sys;
    lua_State *L = p_sys->L;

    if( luaL_dofile( L, p_sys->psz_filename ) )
    {
        msg_Err( p_intf, "Error loading script %s: %s", p_sys->psz_filename,
                 lua_tostring( L, lua_gettop( L ) ) );
        lua_pop( L, 1 );
    }
    return NULL;
}
