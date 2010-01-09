/*****************************************************************************
 * net.c: Network related functions
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
#include <vlc_network.h>
#include <vlc_url.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */

#ifdef HAVE_POLL
#include <poll.h>       /* poll structures and defines */
#endif

#include "../vlc.h"
#include "../libs.h"

/*****************************************************************************
 *
 *****************************************************************************/
static int vlclua_url_parse( lua_State *L )
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

/*****************************************************************************
 * Net listen
 *****************************************************************************/
static int vlclua_net_listen_close( lua_State * );
static int vlclua_net_accept( lua_State * );
static int vlclua_net_fds( lua_State * );

static const luaL_Reg vlclua_net_listen_reg[] = {
    { "accept", vlclua_net_accept },
    { "fds", vlclua_net_fds },
    { NULL, NULL }
};

static int vlclua_net_listen_tcp( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    const char *psz_host = luaL_checkstring( L, 1 );
    int i_port = luaL_checkint( L, 2 );
    int *pi_fd = net_ListenTCP( p_this, psz_host, i_port );
    if( pi_fd == NULL )
        return luaL_error( L, "Cannot listen on %s:%d", psz_host, i_port );

    int **ppi_fd = lua_newuserdata( L, sizeof( int * ) );
    *ppi_fd = pi_fd;

    if( luaL_newmetatable( L, "net_listen" ) )
    {
        lua_newtable( L );
        luaL_register( L, NULL, vlclua_net_listen_reg );
        lua_setfield( L, -2, "__index" );
        lua_pushcfunction( L, vlclua_net_listen_close );
        lua_setfield( L, -2, "__gc" );
    }

    lua_setmetatable( L, -2 );
    return 1;
}

static int vlclua_net_listen_close( lua_State *L )
{
    int **ppi_fd = (int**)luaL_checkudata( L, 1, "net_listen" );
    net_ListenClose( *ppi_fd );
    return 0;
}

static int vlclua_net_fds( lua_State *L )
{
    int **ppi_fd = (int**)luaL_checkudata( L, 1, "net_listen" );
    int *pi_fd = *ppi_fd;

    int i_count = 0;
    while( pi_fd[i_count] != -1 )
        lua_pushinteger( L, pi_fd[i_count++] );

    return i_count;
}

static int vlclua_net_accept( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    int **ppi_fd = (int**)luaL_checkudata( L, 1, "net_listen" );
    int *pi_fd = *ppi_fd;
    int i_timeout = luaL_optint( L, 2, -1 ); /* block is default */

    /* Implement net_Accept with timeout */
    int i_fd = -1;

    unsigned int i_count = 1;
    while( pi_fd[i_count] != -1 )
        i_count++;

    struct pollfd ufd[i_count+1];
    unsigned int i;
    for( i = 0; i < i_count; i++ )
    {
        ufd[i].fd = pi_fd[i];
        ufd[i].events = POLLIN;
    }

    if( poll( ufd, i_count, i_timeout ) > 0 )
    {
        for( i = 0; i < i_count; i++ )
        {
            if( !ufd[i].revents ) continue;
            i_fd = net_AcceptSingle( p_this, ufd[i].fd );
            if( i_fd == -1 ) continue;
            memmove( pi_fd + i, pi_fd + i + 1, i_count - (i + 1) );
            pi_fd[i_count - 1] = ufd[i].fd;
        }
    }

    lua_pushinteger( L, i_fd );
    return 1;
}

/*****************************************************************************
 *
 *****************************************************************************/
static int vlclua_net_close( lua_State *L )
{
    int i_fd = luaL_checkint( L, 1 );
    net_Close( i_fd );
    return 0;
}

static int vlclua_net_send( lua_State *L )
{
    int i_fd = luaL_checkint( L, 1 );
    size_t i_len;
    const char *psz_buffer = luaL_checklstring( L, 2, &i_len );
    i_len = luaL_optint( L, 3, i_len );
    i_len = send( i_fd, psz_buffer, i_len, 0 );
    lua_pushinteger( L, i_len );
    return 1;
}

static int vlclua_net_recv( lua_State *L )
{
    int i_fd = luaL_checkint( L, 1 );
    size_t i_len = luaL_optint( L, 2, 1 );
    char psz_buffer[i_len];
    i_len = recv( i_fd, psz_buffer, i_len, 0 );
    lua_pushlstring( L, psz_buffer, i_len );
    return 1;
}

/*****************************************************************************
 *
 *****************************************************************************/
/* Takes a { fd : events } table as first arg and modifies it to { fd : revents } */
static int vlclua_net_poll( lua_State *L )
{
    luaL_checktype( L, 1, LUA_TTABLE );
    double f_timeout = luaL_optnumber( L, 2, -1. );

    int i_fds = 0;
    lua_pushnil( L );
    while( lua_next( L, 1 ) )
    {
        i_fds++;
        lua_pop( L, 1 );
    }
    struct pollfd *p_fds = malloc( i_fds * sizeof( struct pollfd ) );
    lua_pushnil( L );
    int i = 0;
    while( lua_next( L, 1 ) )
    {
        p_fds[i].fd = luaL_checkinteger( L, -2 );
        p_fds[i].events = luaL_checkinteger( L, -1 );
        p_fds[i].revents = 0;
        lua_pop( L, 1 );
        i++;
    }

    int i_ret = poll( p_fds, i_fds, f_timeout < 0. ? -1 : (int)(f_timeout*1000) );
    for( i = 0; i < i_fds; i++ )
    {
        lua_pushinteger( L, p_fds[i].fd );
        lua_pushinteger( L, p_fds[i].revents );
        lua_settable( L, 1 );
    }
    free( p_fds );
    lua_pushinteger( L, i_ret );
    return 1;
}

static int vlclua_net_select( lua_State *L )
{
    int i_ret;
    size_t i_nfds = luaL_checkint( L, 1 );
    fd_set *fds_read = (fd_set*)luaL_checkudata( L, 2, "fd_set" );
    fd_set *fds_write = (fd_set*)luaL_checkudata( L, 3, "fd_set" );
    double f_timeout = luaL_checknumber( L, 4 );
    struct timeval timeout;

#ifndef WIN32
    if( i_nfds > FD_SETSIZE )
        i_nfds = FD_SETSIZE;
#endif
    if( f_timeout >= 0. )
    {
        timeout.tv_sec = (int)f_timeout;
        timeout.tv_usec = (int)(1e6*(f_timeout-(double)((int)f_timeout)));
    }
    i_ret = select( i_nfds, fds_read, fds_write, 0, f_timeout >= 0. ? &timeout : NULL );
    lua_pushinteger( L, i_ret );
    return 1;
}

/*****************************************************************************
 *
 *****************************************************************************/
static int vlclua_fd_clr( lua_State * );
static int vlclua_fd_isset( lua_State * );
static int vlclua_fd_set( lua_State * );
static int vlclua_fd_zero( lua_State * );

static const luaL_Reg vlclua_fd_set_reg[] = {
    { "clr", vlclua_fd_clr },
    { "isset", vlclua_fd_isset },
    { "set", vlclua_fd_set },
    { "zero", vlclua_fd_zero },
    { NULL, NULL }
};

static int vlclua_fd_set_new( lua_State *L )
{
    fd_set *fds = (fd_set*)lua_newuserdata( L, sizeof( fd_set ) );
    FD_ZERO( fds );

    if( luaL_newmetatable( L, "fd_set" ) )
    {
        lua_newtable( L );
        luaL_register( L, NULL, vlclua_fd_set_reg );
        lua_setfield( L, -2, "__index" );
    }

    lua_setmetatable( L, -2 );
    return 1;
}

static int vlclua_fd_clr( lua_State *L )
{
    fd_set *fds = (fd_set*)luaL_checkudata( L, 1, "fd_set" );
    int i_fd = luaL_checkint( L, 2 );
    FD_CLR( i_fd, fds );
    return 0;
}

static int vlclua_fd_isset( lua_State *L )
{
    fd_set *fds = (fd_set*)luaL_checkudata( L, 1, "fd_set" );
    int i_fd = luaL_checkint( L, 2 );
    lua_pushboolean( L, FD_ISSET( i_fd, fds ) );
    return 1;
}

static int vlclua_fd_set( lua_State *L )
{
    fd_set *fds = (fd_set*)luaL_checkudata( L, 1, "fd_set" );
    size_t i_fd = luaL_checkint( L, 2 );
    /* FIXME: we should really use poll() instead here, but that breaks the
     * VLC/LUA API. On Windows, overflow protection is built-in FD_SET, not
     * on POSIX. In both cases, run-time behavior will however be wrong. */
#ifndef WIN32
    if( i_fd < FD_SETSIZE )
#endif
        FD_SET( i_fd, fds );
    return 0;
}

static int vlclua_fd_zero( lua_State *L )
{
    fd_set *fds = (fd_set*)luaL_checkudata( L, 1, "fd_set" );
    FD_ZERO( fds );
    return 0;
}

/*****************************************************************************
 *
 *****************************************************************************/
/*
static int vlclua_fd_open( lua_State *L )
{
}
*/

static int vlclua_fd_write( lua_State *L )
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

static int vlclua_fd_read( lua_State *L )
{
    int i_fd = luaL_checkint( L, 1 );
    size_t i_len = luaL_optint( L, 2, 1 );
    char psz_buffer[i_len];
    i_len = read( i_fd, psz_buffer, i_len );
    lua_pushlstring( L, psz_buffer, i_len );
    return 1;
}

/*****************************************************************************
 *
 *****************************************************************************/
static int vlclua_stat( lua_State *L )
{
#ifdef HAVE_SYS_STAT_H
    const char *psz_path = luaL_checkstring( L, 1 );
    struct stat s;
    if( utf8_stat( psz_path, &s ) )
        return 0;
        //return luaL_error( L, "Couldn't stat %s.", psz_path );
    lua_newtable( L );
    if( S_ISREG( s.st_mode ) )
        lua_pushstring( L, "file" );
    else if( S_ISDIR( s.st_mode ) )
        lua_pushstring( L, "dir" );
#ifdef S_ISCHR
    else if( S_ISCHR( s.st_mode ) )
        lua_pushstring( L, "character device" );
#endif
#ifdef S_ISBLK
    else if( S_ISBLK( s.st_mode ) )
        lua_pushstring( L, "block device" );
#endif
#ifdef S_ISFIFO
    else if( S_ISFIFO( s.st_mode ) )
        lua_pushstring( L, "fifo" );
#endif
#ifdef S_ISLNK
    else if( S_ISLNK( s.st_mode ) )
        lua_pushstring( L, "symbolic link" );
#endif
#ifdef S_ISSOCK
    else if( S_ISSOCK( s.st_mode ) )
        lua_pushstring( L, "socket" );
#endif
    else
        lua_pushstring( L, "unknown" );
    lua_setfield( L, -2, "type" );
    lua_pushinteger( L, s.st_mode );
    lua_setfield( L, -2, "mode" );
    lua_pushinteger( L, s.st_uid );
    lua_setfield( L, -2, "uid" );
    lua_pushinteger( L, s.st_gid );
    lua_setfield( L, -2, "gid" );
    lua_pushinteger( L, s.st_size );
    lua_setfield( L, -2, "size" );
    lua_pushinteger( L, s.st_atime );
    lua_setfield( L, -2, "access_time" );
    lua_pushinteger( L, s.st_mtime );
    lua_setfield( L, -2, "modification_time" );
    lua_pushinteger( L, s.st_ctime );
    lua_setfield( L, -2, "creation_time" );
    return 1;
#else
#   warning "Woops, looks like we don't have stat on your platform"
    return luaL_error( L, "System is missing <sys/stat.h>" );
#endif
}

static int vlclua_opendir( lua_State *L )
{
    const char *psz_dir = luaL_checkstring( L, 1 );
    DIR *p_dir;
    int i = 0;

    if( ( p_dir = utf8_opendir( psz_dir ) ) == NULL )
        return luaL_error( L, "cannot open directory `%s'.", psz_dir );

    lua_newtable( L );
    for( ;; )
    {
        char *psz_filename = utf8_readdir( p_dir );
        if( !psz_filename ) break;
        i++;
        lua_pushstring( L, psz_filename );
        lua_rawseti( L, -2, i );
        free( psz_filename );
    }
    closedir( p_dir );
    return 1;
}

/*****************************************************************************
 *
 *****************************************************************************/
static const luaL_Reg vlclua_net_reg[] = {
    { "url_parse", vlclua_url_parse },
    { "listen_tcp", vlclua_net_listen_tcp },
    { "close", vlclua_net_close },
    { "send", vlclua_net_send },
    { "recv", vlclua_net_recv },
    { "poll", vlclua_net_poll },
    { "select", vlclua_net_select },
    { "fd_set_new", vlclua_fd_set_new },
    { "read", vlclua_fd_read },
    { "write", vlclua_fd_write },
    { "stat", vlclua_stat }, /* Not really "net" */
    { "opendir", vlclua_opendir }, /* Not really "net" */
    { NULL, NULL }
};

void luaopen_net( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_net_reg );
#define ADD_CONSTANT( name, value )    \
    lua_pushinteger( L, value ); \
    lua_setfield( L, -2, name );
    ADD_CONSTANT( "POLLIN", POLLIN )
    ADD_CONSTANT( "POLLPRI", POLLPRI )
    ADD_CONSTANT( "POLLOUT", POLLOUT )
    ADD_CONSTANT( "POLLERR", POLLERR )
    ADD_CONSTANT( "POLLHUP", POLLHUP )
    ADD_CONSTANT( "POLLNVAL", POLLNVAL )
    lua_setfield( L, -2, "net" );
}
