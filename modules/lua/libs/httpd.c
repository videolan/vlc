/*****************************************************************************
 * httpd.c: HTTPd wrapper
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
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
#include <vlc_httpd.h>

#include "../vlc.h"
#include "../libs.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static uint8_t *vlclua_todata( lua_State *L, int narg, int *i_data );

static int vlclua_httpd_host_delete( lua_State * );
static int vlclua_httpd_handler_new( lua_State * );
static int vlclua_httpd_handler_delete( lua_State * );
static int vlclua_httpd_file_new( lua_State * );
static int vlclua_httpd_file_delete( lua_State * );
static int vlclua_httpd_redirect_new( lua_State * );
static int vlclua_httpd_redirect_delete( lua_State * );

/*****************************************************************************
 * HTTPD Host
 *****************************************************************************/
static const luaL_Reg vlclua_httpd_reg[] = {
    { "handler", vlclua_httpd_handler_new },
    { "file", vlclua_httpd_file_new },
    { "redirect", vlclua_httpd_redirect_new },
    { NULL, NULL }
};

static const char no_password_fmt[] = "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n"
"<html xmlns=\"http://www.w3.org/1999/xhtml\">"
"<head>"
"<meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\" />"
"<title>%s</title>"
"</head>"
"<body>"
"%s"
"<!-- VLC_PASSWORD_NOT_SET --></body></html>";

static const char no_password_body[] = N_(
"<p>Password for Web interface has not been set.</p>"
"<p>Please use --http-password, or set a password in </p>"
"<p>Preferences &gt; All &gt; Main interfaces &gt; Lua &gt; Lua HTTP &gt; Password.</p>"
);

static const char no_password_title[] = N_("VLC media player");

static int vlclua_httpd_tls_host_new( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    httpd_host_t *p_host = vlc_http_HostNew( p_this );
    if( !p_host )
        return luaL_error( L, "Failed to create HTTP host" );

    httpd_host_t **pp_host = lua_newuserdata( L, sizeof( httpd_host_t * ) );
    *pp_host = p_host;

    if( luaL_newmetatable( L, "httpd_host" ) )
    {
        lua_newtable( L );
        luaL_register( L, NULL, vlclua_httpd_reg );
        lua_setfield( L, -2, "__index" );
        lua_pushcfunction( L, vlclua_httpd_host_delete );
        lua_setfield( L, -2, "__gc" );
    }

    lua_setmetatable( L, -2 );
    return 1;
}

static int vlclua_httpd_host_delete( lua_State *L )
{
    httpd_host_t **pp_host = (httpd_host_t **)luaL_checkudata( L, 1, "httpd_host" );
    httpd_HostDelete( *pp_host );
    return 0;
}

/*****************************************************************************
 * HTTPd Handler
 *****************************************************************************/
typedef struct
{
    lua_State *L;
    bool password;
    int ref;
} httpd_handler_lua_t;

static int vlclua_httpd_handler_callback(
     void *opaque, httpd_handler_t *p_handler, char *psz_url,
     uint8_t *psz_request, int i_type, uint8_t *p_in, int i_in,
     char *psz_remote_addr, char *psz_remote_host,
     uint8_t **pp_data, int *pi_data )
{
    VLC_UNUSED(p_handler);
    httpd_handler_lua_t *p_sys = opaque;
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
        msg_Err( p_this, "Error while running the lua HTTPd handler "
                 "callback: %s", psz_err );
        lua_settop( L, 2 );
        /* function data */
        return VLC_EGENERIC;
    }
    /* function data outdata */
    *pp_data = vlclua_todata( L, -1, pi_data );
    if (!p_sys->password)
    {
        free(*pp_data);
        char *no_password = NULL;
        if (asprintf(&no_password, no_password_fmt,
                _(no_password_title), _(no_password_body)) < 0) {
            *pi_data = 0;
        } else {
            size_t s = strlen(no_password);
            if (asprintf((char**)pp_data, "Status: 403\n"
                        "Content-Length: %zu\n"
                        "Content-Type: text/html\n\n%s", s, no_password) < 0)
                *pi_data = 0;
            else
                *pi_data = strlen((char*)*pp_data);
            free(no_password);
        }
    }
    lua_pop( L, 1 );
    /* function data */
    return VLC_SUCCESS;
}

static int vlclua_httpd_handler_new( lua_State * L )
{
    httpd_host_t **pp_host = (httpd_host_t **)luaL_checkudata( L, 1, "httpd_host" );
    const char *psz_url = luaL_checkstring( L, 2 );
    const char *psz_user = luaL_nilorcheckstring( L, 3 );
    const char *psz_password = luaL_nilorcheckstring( L, 4 );
    /* Stack item 5 is the callback function */
    luaL_argcheck( L, lua_isfunction( L, 5 ), 5, "Should be a function" );
    /* Stack item 6 is the callback data */
    lua_settop( L, 6 );
    httpd_handler_lua_t *p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return luaL_error( L, "Failed to allocate private buffer." );
    p_sys->L = lua_newthread( L );
    p_sys->ref = luaL_ref( L, LUA_REGISTRYINDEX ); /* pops the object too */
    p_sys->password = psz_password && *psz_password;
    /* use lua_xmove to move the lua callback function and data to
     * the callback's stack. */
    lua_xmove( L, p_sys->L, 2 );
    httpd_handler_t *p_handler = httpd_HandlerNew(
                            *pp_host, psz_url, psz_user, psz_password,
                            vlclua_httpd_handler_callback, p_sys );
    if( !p_handler )
    {
        free( p_sys );
        return luaL_error( L, "Failed to create HTTPd handler." );
    }

    httpd_handler_t **pp_handler = lua_newuserdata( L, sizeof( httpd_handler_t * ) );
    *pp_handler = p_handler;

    if( luaL_newmetatable( L, "httpd_handler" ) )
    {
        lua_pushcfunction( L, vlclua_httpd_handler_delete );
        lua_setfield( L, -2, "__gc" );
    }

    lua_setmetatable( L, -2 );
    return 1;
}

static int vlclua_httpd_handler_delete( lua_State *L )
{
    httpd_handler_t **pp_handler = (httpd_handler_t**)luaL_checkudata( L, 1, "httpd_handler" );
    httpd_handler_lua_t *p_sys = httpd_HandlerDelete( *pp_handler );
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
    bool password;
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
        msg_Err( p_this, "Error while running the lua HTTPd file callback: %s",
                 psz_err );
        lua_settop( L, 2 );
        /* function data */
        return VLC_EGENERIC;
    }
    /* function data outdata */
    *pp_data = vlclua_todata( L, -1, pi_data );
    if (!p_sys->password)
    {
        free(*pp_data);
        if (asprintf((char**)pp_data, no_password_fmt,
                _(no_password_title), _(no_password_body)) < 0) {
            *pi_data = 0;
        } else {
            *pi_data = strlen((char*)*pp_data);
        }
    }
    lua_pop( L, 1 );
    /* function data */
    return VLC_SUCCESS;
}

static int vlclua_httpd_file_new( lua_State *L )
{
    httpd_host_t **pp_host = (httpd_host_t **)luaL_checkudata( L, 1, "httpd_host" );
    const char *psz_url = luaL_checkstring( L, 2 );
    const char *psz_mime = luaL_nilorcheckstring( L, 3 );
    const char *psz_user = luaL_nilorcheckstring( L, 4 );
    const char *psz_password = luaL_nilorcheckstring( L, 5 );
    /* Stack item 7 is the callback function */
    luaL_argcheck( L, lua_isfunction( L, 6 ), 6, "Should be a function" );
    /* Stack item 8 is the callback data */
    httpd_file_sys_t *p_sys = (httpd_file_sys_t *)
                              malloc( sizeof( httpd_file_sys_t ) );
    if( !p_sys )
        return luaL_error( L, "Failed to allocate private buffer." );
    p_sys->L = lua_newthread( L );
    p_sys->password = psz_password && *psz_password;
    p_sys->ref = luaL_ref( L, LUA_REGISTRYINDEX ); /* pops the object too */
    lua_xmove( L, p_sys->L, 2 );
    httpd_file_t *p_file = httpd_FileNew( *pp_host, psz_url, psz_mime,
                                          psz_user, psz_password,
                                          vlclua_httpd_file_callback, p_sys );
    if( !p_file )
    {
        free( p_sys );
        return luaL_error( L, "Failed to create HTTPd file." );
    }

    httpd_file_t **pp_file = lua_newuserdata( L, sizeof( httpd_file_t * ) );
    *pp_file = p_file;

    if( luaL_newmetatable( L, "httpd_file" ) )
    {
        lua_pushcfunction( L, vlclua_httpd_file_delete );
        lua_setfield( L, -2, "__gc" );
    }

    lua_setmetatable( L, -2 );
    return 1;
}

static int vlclua_httpd_file_delete( lua_State *L )
{
    httpd_file_t **pp_file = (httpd_file_t**)luaL_checkudata( L, 1, "httpd_file" );
    httpd_file_sys_t *p_sys = httpd_FileDelete( *pp_file );
    luaL_unref( p_sys->L, LUA_REGISTRYINDEX, p_sys->ref );
    free( p_sys );
    return 0;
}

/*****************************************************************************
 * HTTPd Redirect
 *****************************************************************************/
static int vlclua_httpd_redirect_new( lua_State *L )
{
    httpd_host_t **pp_host = (httpd_host_t **)luaL_checkudata( L, 1, "httpd_host" );
    const char *psz_url_dst = luaL_checkstring( L, 2 );
    const char *psz_url_src = luaL_checkstring( L, 3 );
    httpd_redirect_t *p_redirect = httpd_RedirectNew( *pp_host,
                                                      psz_url_dst,
                                                      psz_url_src );
    if( !p_redirect )
        return luaL_error( L, "Failed to create HTTPd redirect." );

    httpd_redirect_t **pp_redirect = lua_newuserdata( L, sizeof( httpd_redirect_t * ) );
    *pp_redirect = p_redirect;

    if( luaL_newmetatable( L, "httpd_redirect" ) )
    {
        lua_pushcfunction( L, vlclua_httpd_redirect_delete );
        lua_setfield( L, -2, "__gc" );
    }

    lua_setmetatable( L, -2 );
    return 1;
}

static int vlclua_httpd_redirect_delete( lua_State *L )
{
    httpd_redirect_t **pp_redirect = (httpd_redirect_t**)luaL_checkudata( L, 1, "httpd_redirect" );
    httpd_RedirectDelete( *pp_redirect );
    return 0;
}

/*****************************************************************************
 * Utils
 *****************************************************************************/
static uint8_t *vlclua_todata( lua_State *L, int narg, int *pi_data )
{
    size_t i_data;
    const char *psz_data = lua_tolstring( L, narg, &i_data );
    uint8_t *p_data = vlc_alloc( i_data, sizeof(uint8_t) );
    *pi_data = (int)i_data;
    if( !p_data )
    {
        luaL_error( L, "Error while allocating buffer." );
        return NULL; /* To please gcc even though luaL_error longjmp-ed out of here */
    }
    memcpy( p_data, psz_data, i_data );
    return p_data;
}

/*****************************************************************************
 *
 *****************************************************************************/
void luaopen_httpd( lua_State *L )
{
    lua_pushcfunction( L, vlclua_httpd_tls_host_new );
    lua_setfield( L, -2, "httpd" );
}
