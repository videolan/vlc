/*****************************************************************************
 * net.c: Network related functions
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

#include <vlc/vlc.h>
#include <vlc_network.h>
#include <vlc_url.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */
#include <lualib.h>     /* Lua libs */

#include "vlc.h"

/*****************************************************************************
 *
 *****************************************************************************/
int vlclua_url_parse( lua_State *L )
{
    const char *psz_url = luaL_checkstring( L, 1 );
    const char *psz_option = luaL_optstring( L, 2, NULL );
    vlc_url_t url;

    vlc_UrlParse( &url, psz_url, psz_option?*psz_option:0 );

    lua_newtable( L );
    lua_pushstring( L, url.psz_protocol );
    lua_setfield( L, -2, "protocol" );
    lua_pushstring( L, url.psz_username );
    lua_setfield( L, -2, "username" );
    lua_pushstring( L, url.psz_password );
    lua_setfield( L, -2, "password" );
    lua_pushstring( L, url.psz_host );
    lua_setfield( L, -2, "host" );
    lua_pushinteger( L, url.i_port );
    lua_setfield( L, -2, "port" );
    lua_pushstring( L, url.psz_path );
    lua_setfield( L, -2, "path" );
    lua_pushstring( L, url.psz_option );
    lua_setfield( L, -2, "option" );

    vlc_UrlClean( &url );

    return 1;
}

int vlclua_net_listen_tcp( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    const char *psz_host = luaL_checkstring( L, 1 );
    int i_port = luaL_checkint( L, 2 );
    int *pi_fd = net_ListenTCP( p_this, psz_host, i_port );
    if( pi_fd == NULL )
        return luaL_error( L, "Cannot listen on %s:%d", psz_host, i_port );
    lua_pushlightuserdata( L, pi_fd );
    return 1;
}

int vlclua_net_listen_close( lua_State *L )
{
    int *pi_fd = (int*)luaL_checklightuserdata( L, 1 );
    net_ListenClose( pi_fd );
    return 0;
}

int vlclua_net_accept( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    int *pi_fd = (int*)luaL_checklightuserdata( L, 1 );
    mtime_t i_wait = luaL_optint( L, 2, -1 ); /* default to block */
    int i_fd = net_Accept( p_this, pi_fd, i_wait );
    lua_pushinteger( L, i_fd );
    return 1;
}

int vlclua_net_close( lua_State *L )
{
    int i_fd = luaL_checkint( L, 1 );
    net_Close( i_fd );
    return 0;
}

int vlclua_net_send( lua_State *L )
{
    int i_fd = luaL_checkint( L, 1 );
    size_t i_len;
    const char *psz_buffer = luaL_checklstring( L, 2, &i_len );
    i_len = luaL_optint( L, 3, i_len );
    i_len = send( i_fd, psz_buffer, i_len, 0 );
    lua_pushinteger( L, i_len );
    return 1;
}

int vlclua_net_recv( lua_State *L )
{
    int i_fd = luaL_checkint( L, 1 );
    size_t i_len = luaL_optint( L, 2, 1 );
    char psz_buffer[i_len];
    i_len = recv( i_fd, psz_buffer, i_len, 0 );
    lua_pushlstring( L, psz_buffer, i_len );
    return 1;
}

int vlclua_net_select( lua_State *L )
{
    int i_ret;
    int i_nfds = luaL_checkint( L, 1 );
    fd_set *fds_read = (fd_set*)luaL_checkuserdata( L, 2, sizeof( fd_set ) );
    fd_set *fds_write = (fd_set*)luaL_checkuserdata( L, 3, sizeof( fd_set ) );
    double f_timeout = luaL_checknumber( L, 4 );
    struct timeval timeout;
    timeout.tv_sec = (int)f_timeout;
    timeout.tv_usec = (int)(1e6*(f_timeout-(double)((int)f_timeout)));
    i_ret = select( i_nfds, fds_read, fds_write, 0, &timeout );
    lua_pushinteger( L, i_ret );
    lua_pushinteger( L, (double)timeout.tv_sec+((double)timeout.tv_usec)/1e-6 );
    return 2;
}

int vlclua_fd_set_new( lua_State *L )
{
    fd_set *fds = (fd_set*)lua_newuserdata( L, sizeof( fd_set ) );
    FD_ZERO( fds );
    return 1;
}

int vlclua_fd_clr( lua_State *L )
{
    fd_set *fds = (fd_set*)luaL_checkuserdata( L, 1, sizeof( fd_set ) );
    int i_fd = luaL_checkint( L, 2 );
    FD_CLR( i_fd, fds );
    return 0;
}
int vlclua_fd_isset( lua_State *L )
{
    fd_set *fds = (fd_set*)luaL_checkuserdata( L, 1, sizeof( fd_set ) );
    int i_fd = luaL_checkint( L, 2 );
    lua_pushboolean( L, FD_ISSET( i_fd, fds ) );
    return 1;
}
int vlclua_fd_set( lua_State *L )
{
    fd_set *fds = (fd_set*)luaL_checkuserdata( L, 1, sizeof( fd_set ) );
    int i_fd = luaL_checkint( L, 2 );
    FD_SET( i_fd, fds );
    return 0;
}
int vlclua_fd_zero( lua_State *L )
{
    fd_set *fds = (fd_set*)luaL_checkuserdata( L, 1, sizeof( fd_set ) );
    FD_ZERO( fds );
    return 0;
}

/*
int vlclua_fd_open( lua_State *L )
{
}
*/

int vlclua_fd_write( lua_State *L )
{
    int i_fd = luaL_checkint( L, 1 );
    size_t i_len;
    ssize_t i_ret;
    const char *psz_buffer = luaL_checklstring( L, 2, &i_len );
    i_len = luaL_optint( L, 3, i_len );
    i_ret = write( i_fd, psz_buffer, i_len );
    lua_pushinteger( L, i_ret );
    return 1;
}

int vlclua_fd_read( lua_State *L )
{
    int i_fd = luaL_checkint( L, 1 );
    size_t i_len = luaL_optint( L, 2, 1 );
    char psz_buffer[i_len];
    i_len = read( i_fd, psz_buffer, i_len );
    lua_pushlstring( L, psz_buffer, i_len );
    return 1;
}
