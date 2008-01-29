/*****************************************************************************
 * httpd.c: HTTPd wrapper
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
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

#include <vlc/vlc.h>
#include <vlc_httpd.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */
#include <lualib.h>     /* Lua libs */

#include "vlc.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static uint8_t *vlclua_todata( lua_State *L, int narg, int *i_data );

/*****************************************************************************
 * HTTPD Host
 *****************************************************************************/
#if 0
/* This is kind of a useless function since TLS with the 4 last args
 * unset does the same thing as far as I know. */
int vlclua_httpd_host_new( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    const char *psz_host = luaL_checkstring( L, 1 );
    int i_port = luaL_checkint( L, 2 );
    httpd_host_t *p_httpd_host = httpd_HostNew( p_this, psz_host, i_port );
    if( !p_httpd_host )
        return luaL_error( L, "Failed to create HTTP host \"%s:%d\".",
                           psz_host, i_port );
    vlclua_push_vlc_object( L, p_httpd_host, vlclua_httpd_host_delete );
    return 1;
}
#endif

int vlclua_httpd_tls_host_new( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    const char *psz_host = luaL_checkstring( L, 1 );
    int i_port = luaL_checkint( L, 2 );
    const char *psz_cert = luaL_optstring( L, 3, NULL );
    const char *psz_key = luaL_optstring( L, 4, NULL );
    const char *psz_ca = luaL_optstring( L, 5, NULL );
    const char *psz_crl = luaL_optstring( L, 6, NULL );
    httpd_host_t *p_httpd_host = httpd_TLSHostNew( p_this, psz_host, i_port,
                                                   psz_cert, psz_key,
                                                   psz_ca, psz_crl );
    if( !p_httpd_host )
        return luaL_error( L, "Failed to create HTTP TLS host \"%s:%d\" "
                           "(cert: \"%s\", key: \"%s\", ca: \"%s\", "
                           "crl: \"%s\").", psz_host, i_port,
                           psz_cert, psz_key, psz_ca, psz_crl );
    vlclua_push_vlc_object( L, (vlc_object_t*)p_httpd_host, vlclua_httpd_host_delete );
    return 1;
}

#define ARG_1_IS_HTTPD_HOST httpd_host_t *p_httpd_host = \
    (httpd_host_t*)vlclua_checkobject( L, 1, VLC_OBJECT_HTTPD_HOST );
int vlclua_httpd_host_delete( lua_State *L )
{
    ARG_1_IS_HTTPD_HOST
    httpd_HostDelete( p_httpd_host );
    return 0;
}

/*****************************************************************************
 * HTTPd Handler
 *****************************************************************************/
struct httpd_handler_sys_t
{
    lua_State *L;
    int ref;
};

static int vlclua_httpd_handler_callback(
     httpd_handler_sys_t *p_sys, httpd_handler_t *p_handler, char *psz_url,
     uint8_t *psz_request, int i_type, uint8_t *p_in, int i_in,
     char *psz_remote_addr, char *psz_remote_host,
     uint8_t **pp_data, int *pi_data )
{
    VLC_UNUSED(p_handler);
    lua_State *L = p_sys->L;

    /* function data */
    lua_pushvalue( L, 1 );
    lua_pushvalue( L, 2 );
    /* function data function data */
    lua_pushstring( L, psz_url );
    /* function data function data url */
    lua_pushstring( L, (const char *)psz_request );
    /* function data function data url request */
    lua_pushinteger( L, i_type ); /* Q: what does i_type stand for? */
    /* function data function data url request type */
    lua_pushlstring( L, (const char *)p_in, i_in ); /* Q: what do p_in contain? */
    /* function data function data url request type in */
    lua_pushstring( L, psz_remote_addr );
    /* function data function data url request type in addr */
    lua_pushstring( L, psz_remote_host );
    /* function data function data url request type in addr host */
    if( lua_pcall( L, 7, 1, 0 ) )
    {
        /* function data err */
        vlc_object_t *p_this = vlclua_get_this( L );
        const char *psz_err = lua_tostring( L, -1 );
        msg_Err( p_this, "Error while runing the lua HTTPd handler "
                 "callback: %s", psz_err );
        lua_settop( L, 2 );
        /* function data */
        return VLC_EGENERIC;
    }
    /* function data outdata */
    *pp_data = vlclua_todata( L, -1, pi_data );
    lua_pop( L, 1 );
    /* function data */
    return VLC_SUCCESS;
}

int vlclua_httpd_handler_new( lua_State * L )
{
    ARG_1_IS_HTTPD_HOST
    const char *psz_url = luaL_checkstring( L, 2 );
    const char *psz_user = luaL_nilorcheckstring( L, 3 );
    const char *psz_password = luaL_nilorcheckstring( L, 4 );
    const vlc_acl_t *p_acl = NULL; /* FIXME 5 */
    /* Stack item 6 is the callback function */
    luaL_argcheck( L, lua_isfunction( L, 6 ), 6, "Should be a function" );
    /* Stack item 7 is the callback data */
    lua_settop( L, 7 );
    httpd_handler_sys_t *p_sys = (httpd_handler_sys_t*)
                                 malloc( sizeof( httpd_handler_sys_t ) );
    if( !p_sys )
        return luaL_error( L, "Failed to allocate private buffer." );
    p_sys->L = lua_newthread( L );
    p_sys->ref = luaL_ref( L, LUA_REGISTRYINDEX ); /* pops the object too */
    /* use lua_xmove to move the lua callback function and data to
     * the callback's stack. */
    lua_xmove( L, p_sys->L, 2 );
    httpd_handler_t *p_handler = httpd_HandlerNew(
                            p_httpd_host, psz_url, psz_user, psz_password,
                            p_acl, vlclua_httpd_handler_callback, p_sys );
    if( !p_handler )
        return luaL_error( L, "Failed to create HTTPd handler." );
    lua_pushlightuserdata( L, p_handler ); /* FIXME */
    return 1;
}

int vlclua_httpd_handler_delete( lua_State *L )
{
    httpd_handler_t *p_handler = (httpd_handler_t*)luaL_checklightuserdata( L, 1 ); /* FIXME */
    httpd_handler_sys_t *p_sys = httpd_HandlerDelete( p_handler );
    luaL_unref( p_sys->L, LUA_REGISTRYINDEX, p_sys->ref );
    free( p_sys );
    return 0;
}

/*****************************************************************************
 * HTTPd File
 *****************************************************************************/
struct httpd_file_sys_t
{
    lua_State *L;
    int ref;
};

static int vlclua_httpd_file_callback(
    httpd_file_sys_t *p_sys, httpd_file_t *p_file, uint8_t *psz_request,
    uint8_t **pp_data, int *pi_data )
{
    VLC_UNUSED(p_file);
    lua_State *L = p_sys->L;

    /* function data */
    lua_pushvalue( L, 1 );
    lua_pushvalue( L, 2 );
    /* function data function data */
    lua_pushstring( L, (const char *)psz_request );
    /* function data function data request */
    if( lua_pcall( L, 2, 1, 0 ) )
    {
        /* function data err */
        vlc_object_t *p_this = vlclua_get_this( L );
        const char *psz_err = lua_tostring( L, -1 );
        msg_Err( p_this, "Error while runing the lua HTTPd file callback: %s",
                 psz_err );
        lua_settop( L, 2 );
        /* function data */
        return VLC_EGENERIC;
    }
    /* function data outdata */
    *pp_data = vlclua_todata( L, -1, pi_data );
    lua_pop( L, 1 );
    /* function data */
    return VLC_SUCCESS;
}

int vlclua_httpd_file_new( lua_State *L )
{
    ARG_1_IS_HTTPD_HOST
    const char *psz_url = luaL_checkstring( L, 2 );
    const char *psz_mime = luaL_nilorcheckstring( L, 3 );
    const char *psz_user = luaL_nilorcheckstring( L, 4 );
    const char *psz_password = luaL_nilorcheckstring( L, 5 );
    const vlc_acl_t *p_acl = lua_isnil( L, 6 ) ? NULL : luaL_checklightuserdata( L, 6 );
    /* Stack item 7 is the callback function */
    luaL_argcheck( L, lua_isfunction( L, 7 ), 7, "Should be a function" );
    /* Stack item 8 is the callback data */
    httpd_file_sys_t *p_sys = (httpd_file_sys_t *)
                              malloc( sizeof( httpd_file_sys_t ) );
    if( !p_sys )
        return luaL_error( L, "Failed to allocate private buffer." );
    p_sys->L = lua_newthread( L );
    p_sys->ref = luaL_ref( L, LUA_REGISTRYINDEX ); /* pops the object too */
    lua_xmove( L, p_sys->L, 2 );
    httpd_file_t *p_file = httpd_FileNew( p_httpd_host, psz_url, psz_mime,
                                          psz_user, psz_password, p_acl,
                                          vlclua_httpd_file_callback, p_sys );
    if( !p_file )
        return luaL_error( L, "Failed to create HTTPd file." );
    lua_pushlightuserdata( L, p_file ); /* FIXME */
    return 1;
}

int vlclua_httpd_file_delete( lua_State *L )
{
    httpd_file_t *p_file = (httpd_file_t*)luaL_checklightuserdata( L, 1 ); /* FIXME */
    /* FIXME: How do we delete p_sys ? the struct is hidden in the VLC core */
    httpd_file_sys_t *p_sys = httpd_FileDelete( p_file );
    luaL_unref( p_sys->L, LUA_REGISTRYINDEX, p_sys->ref );
    free( p_sys );
    return 0;
}

/*****************************************************************************
 * HTTPd Redirect
 *****************************************************************************/
int vlclua_httpd_redirect_new( lua_State *L )
{
    ARG_1_IS_HTTPD_HOST
    const char *psz_url_dst = luaL_checkstring( L, 2 );
    const char *psz_url_src = luaL_checkstring( L, 3 );
    httpd_redirect_t *p_redirect = httpd_RedirectNew( p_httpd_host,
                                                      psz_url_dst,
                                                      psz_url_src );
    if( !p_redirect )
        return luaL_error( L, "Failed to create HTTPd redirect." );
    lua_pushlightuserdata( L, p_redirect ); /* FIXME */
    return 1;
}

int vlclua_httpd_redirect_delete( lua_State *L )
{
    httpd_redirect_t *p_redirect = (httpd_redirect_t*)luaL_checklightuserdata( L, 1 ); /* FIXME */
    httpd_RedirectDelete( p_redirect );
    return 0;
}

/*****************************************************************************
 * Utils
 *****************************************************************************/
static uint8_t *vlclua_todata( lua_State *L, int narg, int *pi_data )
{
    size_t i_data;
    const char *psz_data = lua_tolstring( L, narg, &i_data );
    uint8_t *p_data = (uint8_t*)malloc( i_data * sizeof(uint8_t) );
    *pi_data = (int)i_data;
    if( !p_data )
    {
        luaL_error( L, "Error while allocating buffer." );
        return NULL; /* To please gcc even though luaL_error longjmp-ed out of here */
    }
    memcpy( p_data, psz_data, i_data );
    return p_data;
}
