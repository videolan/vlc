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
#include <vlc_charset.h>

#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_aout.h>

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
static char *FindFile( const char *psz_name )
{
    char  *ppsz_dir_list[] = { NULL, NULL, NULL, NULL };
    char **ppsz_dir;
    vlclua_dir_list( "intf", ppsz_dir_list );
    for( ppsz_dir = ppsz_dir_list; *ppsz_dir; ppsz_dir++ )
    {
        char *psz_filename;
        FILE *fp;
        if( asprintf( &psz_filename, "%s"DIR_SEP"%s.lua", *ppsz_dir,
                      psz_name ) < 0 )
        {
            vlclua_dir_list_free( ppsz_dir_list );
            return NULL;
        }
        fp = fopen( psz_filename, "r" );
        if( fp )
        {
            fclose( fp );
            vlclua_dir_list_free( ppsz_dir_list );
            return psz_filename;
        }
        free( psz_filename );
    }
    vlclua_dir_list_free( ppsz_dir_list );
    return NULL;
}

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
    /* { "rc", "rc" }, */
    { "luahotkeys", "hotkeys" },
    /* { "hotkeys", "hotkeys" }, */
    { "luatelnet", "telnet" },
    /* { "telnet", "telnet" }, */
    { "luahttp", "http" },
    /* { "http", "http" }, */
    { NULL, NULL } };

static bool WordInList( const char *psz_list, const char *psz_word )
{
    const char *psz_str = strstr( psz_list, psz_word );
    int i_len = strlen( psz_word );
    while( psz_str )
    {
        if( (psz_str == psz_list || *(psz_str-1) == ',' )
         /* it doesn't start in middle of a word */
         /* it doest end in middle of a word */
         && ( psz_str[i_len] == '\0' || psz_str[i_len] == ',' ) )
            return true;
        psz_str = strstr( psz_str, psz_word );
    }
    return false;
}

static char *GetModuleName( intf_thread_t *p_intf )
{
    int i;
    const char *psz_intf;
    /*if( *p_intf->psz_intf == '$' )
        psz_intf = var_GetString( p_intf, p_intf->psz_intf+1 );
    else*/
        psz_intf = p_intf->psz_intf;
    for( i = 0; pp_shortcuts[i].psz_name; i++ )
    {
        if( WordInList( psz_intf, pp_shortcuts[i].psz_shortcut ) )
            return strdup( pp_shortcuts[i].psz_name );
    }

    return var_CreateGetString( p_intf, "lua-intf" );
}

static const luaL_Reg p_reg[] = { { NULL, NULL } };

int Open_LuaIntf( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    intf_sys_t *p_sys;
    lua_State *L;

    config_ChainParse( p_intf, "lua-", ppsz_intf_options, p_intf->p_cfg );
    char *psz_name = GetModuleName( p_intf );
    const char *psz_config;
    bool b_config_set = false;
    if( !psz_name ) psz_name = strdup( "dummy" );

    p_intf->p_sys = (intf_sys_t*)malloc( sizeof(intf_sys_t) );
    if( !p_intf->p_sys )
    {
        free( psz_name );
        return VLC_ENOMEM;
    }
    p_sys = p_intf->p_sys;
    p_sys->psz_filename = FindFile( psz_name );
    if( !p_sys->psz_filename )
    {
        msg_Err( p_intf, "Couldn't find lua interface script \"%s\".",
                 psz_name );
        free( psz_name );
        free( p_sys );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_intf, "Found lua interface script: %s", p_sys->psz_filename );

    L = luaL_newstate();
    if( !L )
    {
        msg_Err( p_intf, "Could not create new Lua State" );
        free( psz_name );
        free( p_sys );
        return VLC_EGENERIC;
    }

    luaL_openlibs( L );

    /* register our functions */
    luaL_register( L, "vlc", p_reg );

    /* store a pointer to p_intf (FIXME: user could overwrite this) */
    lua_pushlightuserdata( L, p_intf );
    lua_setfield( L, -2, "private" );

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

    /* clean up */
    lua_pop( L, 1 );

    /* <gruik> */
    /* Setup the module search path */
    {
    char *psz_command;
    char *psz_char = strrchr(p_sys->psz_filename,DIR_SEP_CHAR);
    *psz_char = '\0';
    /* FIXME: don't use luaL_dostring */
    if( asprintf( &psz_command,
                  "package.path = \"%s"DIR_SEP"modules"DIR_SEP"?.lua;\"..package.path",
                  p_sys->psz_filename ) < 0 )
        return VLC_EGENERIC;
    *psz_char = DIR_SEP_CHAR;
    if( luaL_dostring( L, psz_command ) )
        return VLC_EGENERIC;
    }
    /* </gruik> */

    psz_config = var_CreateGetString( p_intf, "lua-config" );
    if( psz_config && *psz_config )
    {
        char *psz_buffer;
        if( asprintf( &psz_buffer, "config={%s}", psz_config ) != -1 )
        {
            printf("%s\n", psz_buffer);
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
        p_sys->exiting = true;
        Close_LuaIntf( p_this );
        return VLC_ENOMEM;
    }

    return VLC_SUCCESS;
}

void Close_LuaIntf( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    if( !p_sys->exiting ) /* <- Read-only here and in thread: no locking */
    {
        vlc_mutex_lock( &p_sys->lock );
        p_sys->exiting = true;
        vlc_cond_signal( &p_sys->wait );
        vlc_mutex_unlock( &p_sys->lock );
        vlc_join( p_sys->thread, NULL );
    }
    vlc_cond_destroy( &p_sys->wait );
    vlc_mutex_destroy( &p_sys->lock );

    lua_close( p_sys->L );
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
