/*****************************************************************************
 * luaplaylist.c :  Lua playlist demux module
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
#include <vlc/vlc.h>
#include <vlc_demux.h>
#include <vlc_url.h>
#include <vlc_strings.h>
#include <vlc_charset.h>

#include <errno.h>                                                 /* ENOMEM */
#include "playlist.h"

#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */
#include <lualib.h>     /* Lua libs */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int E_(Import_LuaPlaylist)( vlc_object_t *p_this );
static void E_(Close_LuaPlaylist)( vlc_object_t *p_this );

static int Demux( demux_t *p_demux );
static int Control( demux_t *p_demux, int i_query, va_list args );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    add_shortcut( "lua" );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );

    set_shortname( _("Lua Playlist") );
    set_description( _("Lua Playlist Parser Interface") );
    set_capability( "demux2", 0 );
    set_callbacks( E_(Import_LuaPlaylist), E_(Close_LuaPlaylist) );
vlc_module_end();

/*****************************************************************************
 *
 *****************************************************************************/
struct demux_sys_t
{
    lua_State *p_state;
};

static demux_t *vlclua_get_demux( lua_State *p_state )
{
    demux_t *p_demux;
    lua_getglobal( p_state, "vlc" );
    lua_getfield( p_state, lua_gettop( p_state ), "private" );
    p_demux = (demux_t*)lua_topointer( p_state, lua_gettop( p_state ) );
    lua_pop( p_state, 2 );
    return p_demux;
}

static int vlclua_peek( lua_State *p_state )
{
    demux_t *p_demux = vlclua_get_demux( p_state );
    int i = lua_gettop( p_state );
    int n;
    byte_t *p_peek;
    int i_peek;
    if( !i ) return 0;
    n = lua_tonumber( p_state, 1 );
    lua_pop( p_state, i );
    i_peek = stream_Peek( p_demux->s, &p_peek, n );
    lua_pushlstring( p_state, (const char *)p_peek, i_peek );
    return 1;
}

static int vlclua_read( lua_State *p_state )
{
    demux_t *p_demux = vlclua_get_demux( p_state );
    int i = lua_gettop( p_state );
    int n;
    byte_t *p_read;
    int i_read;
    if( !i ) return 0;
    n = lua_tonumber( p_state, 1 );
    lua_pop( p_state, i );
    i_read = stream_Read( p_demux->s, &p_read, n );
    lua_pushlstring( p_state, (const char *)p_read, i_read );
    return 1;
}

static int vlclua_readline( lua_State *p_state )
{
    demux_t *p_demux = vlclua_get_demux( p_state );
    char *psz_line = stream_ReadLine( p_demux->s );
    if( psz_line )
    {
        lua_pushstring( p_state, psz_line );
        free( psz_line );
    }
    else
    {
        lua_pushnil( p_state );
    }
    return 1;
}

static int vlclua_decode_uri( lua_State *p_state )
{
    int i = lua_gettop( p_state );
    if( !i ) return 0;
    const char *psz_cstring = lua_tostring( p_state, 1 );
    if( !psz_cstring ) return 0;
    char *psz_string = strdup( psz_cstring );
    lua_pop( p_state, i );
    decode_URI( psz_string );
    lua_pushstring( p_state, psz_string );
    free( psz_string );
    return 1;
}

static int vlclua_resolve_xml_special_chars( lua_State *p_state )
{
    int i = lua_gettop( p_state );
    if( !i ) return 0;
    const char *psz_cstring = lua_tostring( p_state, 1 );
    if( !psz_cstring ) return 0;
    char *psz_string = strdup( psz_cstring );
    lua_pop( p_state, i );
    resolve_xml_special_chars( psz_string );
    lua_pushstring( p_state, psz_string );
    free( psz_string );
    return 1;
}

static int file_select( const char *file )
{
    int i = strlen( file );
    return i > 4 && !strcmp( file+i-4, ".lua" );
}

static int file_compare( const char **a, const char **b )
{
    return strcmp( *a, *b );
}

/*****************************************************************************
 * Import_LuaPlaylist: main import function
 *****************************************************************************/
int E_(Import_LuaPlaylist)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    lua_State *p_state;
    int i_ret = VLC_EGENERIC;
    char **ppsz_filelist = NULL;
    char **ppsz_fileend = NULL;
    char **ppsz_file;
    DIR *dir;
    char *psz_filename = NULL;
    int i_files;
    char *psz_dir;

#   if defined(__APPLE__) || defined(SYS_BEOS) || defined(WIN32)
    {
        char *psz_vlcpath = p_intf->p_libvlc_global->psz_vlcpath;
        psz_dir = malloc( strlen( psz_vlcpath ) + strlen( "/share/luaplaylist" ) + 1 );
        if( !psz_src ) return VLC_ENOMEM;
#       if defined( WIN32 )
            sprintf( psz_src, "%s/luaplaylist", psz_vlcpath );
#       else
            sprintf( psz_src, "%s/share/luaplaylist", psz_vlcpath );
#       endif
    }
#   else
    {
#   ifdef HAVE_SYS_STAT_H
        struct stat stat_info;
        if( ( utf8_stat( "share/luaplaylist", &stat_info ) == -1 )
            || !S_ISDIR( stat_info.st_mode ) )
        {
            psz_dir = strdup( DATA_PATH "/luaplaylist" );
        }
        else
#   endif
        {
            psz_dir = strdup( "share/luaplaylist" );
        }
    }
#   endif

    static luaL_Reg p_reg[] =
    {
        { "peek", vlclua_peek },
        { "read", vlclua_read },
        { "readline", vlclua_readline },
        { "decode_uri", vlclua_decode_uri },
        { "resolve_xml_special_chars", vlclua_resolve_xml_special_chars }
    };

    p_demux->p_sys = (demux_sys_t*)malloc( sizeof( demux_sys_t ) );
    if( !p_demux->p_sys )
    {
        free( psz_dir );
        return VLC_ENOMEM;
    }

    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;

    /* Initialise Lua state structure */
    p_state = luaL_newstate();
    if( !p_state )
    {
        msg_Err( p_demux, "Could not create new Lua State" );
        free( psz_dir );
        free( p_demux->p_sys );
        return VLC_EGENERIC;
    }
    p_demux->p_sys->p_state = p_state;

    /* Load Lua libraries */
    luaL_openlibs( p_state ); /* FIXME: Don't open all the libs? */

    luaL_register( p_state, "vlc", p_reg );
    lua_pushlightuserdata( p_state, p_demux );
    lua_setfield( p_state, lua_gettop( p_state ) - 1, "private" );
    lua_pushstring( p_state, p_demux->psz_path );
    lua_setfield( p_state, lua_gettop( p_state ) - 1, "path" );
    lua_pushstring( p_state, p_demux->psz_access );
    lua_setfield( p_state, lua_gettop( p_state ) - 1, "access" );

    lua_pop( p_state, 1 );

    dir = utf8_opendir( psz_dir );
    if( !dir ) goto error;
    i_files = utf8_loaddir( dir, &ppsz_filelist, file_select, file_compare );
    if( i_files < 1 ) goto error;
    ppsz_fileend = ppsz_filelist + i_files;

    for( ppsz_file = ppsz_filelist; ppsz_file < ppsz_fileend; ppsz_file++ )
    {
        free( psz_filename ); psz_filename = NULL;
        asprintf( &psz_filename, "%s/%s", psz_dir, *ppsz_file );
        msg_Dbg( p_demux, "Trying Lua playlist script %s", psz_filename );

        /* Ugly hack to delete previous versions of the probe() and parse()
         * functions. */
        lua_pushnil( p_state );
        lua_pushnil( p_state );
        lua_setglobal( p_state, "probe" );
        lua_setglobal( p_state, "parse" );

        /* Load and run the script(s) */
        if( luaL_dofile( p_state, psz_filename ) )
        {
            msg_Warn( p_demux, "Error loading script %s: %s", psz_filename,
                      lua_tostring( p_state, lua_gettop( p_state ) ) );
            lua_pop( p_state, 1 );
            continue;
        }

        lua_getglobal( p_state, "probe" );

        if( !lua_isfunction( p_state, lua_gettop( p_state ) ) )
        {
            msg_Warn( p_demux, "Error while runing script %s, "
                      "function probe() not found", psz_filename );
            lua_pop( p_state, 1 );
            continue;
        }

        if( lua_pcall( p_state, 0, 1, 0 ) )
        {
            msg_Warn( p_demux, "Error while runing script %s, "
                      "function probe(): %s", psz_filename,
                      lua_tostring( p_state, lua_gettop( p_state ) ) );
            lua_pop( p_state, 1 );
            continue;
        }

        if( lua_gettop( p_state ) )
        {
            if( lua_toboolean( p_state, 1 ) )
            {
                msg_Dbg( p_demux, "Lua playlist script %s's "
                         "probe() function was successful", psz_filename );
                i_ret = VLC_SUCCESS;
            }
            lua_pop( p_state, 1 );

            if( i_ret == VLC_SUCCESS ) break;
        }
    }

    error:
        free( psz_filename );

        if( ppsz_filelist )
        {
            for( ppsz_file = ppsz_filelist; ppsz_file < ppsz_fileend;
                 ppsz_file++ )
                free( *ppsz_file );
            free( ppsz_filelist );
        }

        if( dir ) closedir( dir );
        free( psz_dir );
        if( i_ret != VLC_SUCCESS )
            E_(Close_LuaPlaylist)( p_this );
        return i_ret;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void E_(Close_LuaPlaylist)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    lua_close( p_demux->p_sys->p_state );
    free( p_demux->p_sys );
}

static int Demux( demux_t *p_demux )
{
    input_item_t *p_input;
    lua_State *p_state = p_demux->p_sys->p_state;
    char psz_filename[] = "FIXME";

    INIT_PLAYLIST_STUFF;

    lua_getglobal( p_state, "parse" );

    if( !lua_isfunction( p_state, lua_gettop( p_state ) ) )
    {
        msg_Warn( p_demux, "Error while runing script %s, "
                  "function parse() not found", psz_filename );
        E_(Close_LuaPlaylist)( VLC_OBJECT( p_demux ) );
        return VLC_EGENERIC;
    }

    if( lua_pcall( p_state, 0, 1, 0 ) )
    {
        msg_Warn( p_demux, "Error while runing script %s, "
                  "function parse(): %s", psz_filename,
                  lua_tostring( p_state, lua_gettop( p_state ) ) );
        E_(Close_LuaPlaylist)( VLC_OBJECT( p_demux ) );
        return VLC_EGENERIC;
    }

    if( lua_gettop( p_state ) )
    {
        int t = lua_gettop( p_state );
        if( lua_istable( p_state, t ) )
        {
            lua_pushnil( p_state );
            while( lua_next( p_state, t ) )
            {
                if( lua_istable( p_state, t+2 ) )
                {
                    const char *psz_url = NULL;
                    const char *psz_title = NULL;
                    lua_getfield( p_state, t+2, "path" );
                    if( lua_isstring( p_state, t+3 ) )
                    {
                        psz_url = lua_tostring( p_state, t+3 );
                        printf("Path: %s\n", psz_url );
                        lua_getfield( p_state, t+2, "name" );
                        if( lua_isstring( p_state, t+4 ) )
                        {
                            psz_title = lua_tostring( p_state, t+4 );
                            printf("Name: %s\n", psz_title );
                        }
                        else
                        {
                            psz_title = psz_url;
                        }
                        p_input = input_ItemNewExt( p_playlist, psz_url,
                                                    psz_title, 0, NULL, -1 );
                        p_input->p_meta = vlc_meta_New();

#define TRY_META( a, b ) \
                        lua_getfield( p_state, t+2, a ); \
                        if( lua_isstring( p_state, t+5 ) ) \
                        { \
                            psz_title = lua_tostring( p_state, t+5 ); \
                            printf( #b ": %s\n", psz_title ); \
                            vlc_meta_Set ## b ( p_input->p_meta, psz_title ); \
                        } \
                        lua_pop( p_state, 1 ); /* pop a */
                        TRY_META( "title", Title );
                        TRY_META( "artist", Artist );
                        TRY_META( "genre", Genre );
                        TRY_META( "copyright", Copyright );
                        TRY_META( "album", Album );
                        TRY_META( "tracknum", Tracknum );
                        TRY_META( "description", Description );
                        TRY_META( "rating", Rating );
                        TRY_META( "date", Date );
                        TRY_META( "setting", Setting );
                        TRY_META( "url", URL );
                        TRY_META( "language", Language );
                        TRY_META( "nowplaying", NowPlaying );
                        TRY_META( "publisher", Publisher );
                        TRY_META( "encodedby", EncodedBy );
                        TRY_META( "arturl", ArtURL );
                        TRY_META( "trackid", TrackID );

                        playlist_BothAddInput(
                            p_playlist, p_input,
                            p_item_in_category,
                            PLAYLIST_APPEND | PLAYLIST_SPREPARSE,
                            PLAYLIST_END, NULL, NULL, VLC_FALSE );
                        lua_pop( p_state, 1 ); /* pop "name" */
                    }
                    else
                    {
                        printf("Path isn't a string\n");
                    }
                    lua_pop( p_state, 1 ); /* pop "path" */
                }
                else
                {
                    printf("This isn't a table !!!\n" );
                }
                lua_pop( p_state, 1 ); /* pop the value, keep the key for
                                        * the next lua_next() call */
            }
            lua_pop( p_state, 1 ); /* pop the last key */
        }
        else
        {
            msg_Warn( p_demux, "Script didn't return a table" );
        }
    }
    else
    {
        msg_Err( p_demux, "Script went completely foobar" );
    }

    HANDLE_PLAY_AND_RELEASE;

    return -1; /* Needed for correct operation of go back */
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return VLC_EGENERIC;
}
