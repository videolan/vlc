/*****************************************************************************
 * vlclua.c: Generic lua inteface functions
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea at videolan tod org>
 *          Pierre d'Herbemont <pdherbemont # videolan.org>
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
#include <vlc_meta.h>
#include <vlc_charset.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */
#include <lualib.h>     /* Lua libs */

#include "vlclua.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    add_shortcut( "luameta" );
    set_shortname( N_( "Lua Meta" ) );
    set_description( _("Fetch metadata using lua scripts") );
    set_capability( "meta fetcher", 10 );
    set_callbacks( E_(FindMeta), NULL );
    add_submodule();
        set_shortname( N_( "Lua Art" ) );
        set_description( _("Fetch artwork using lua scripts") );
        set_capability( "art finder", 10 );
        set_callbacks( E_(FindArt), NULL );
    add_submodule();
        add_shortcut( "luaplaylist" );
        set_category( CAT_INPUT );
        set_subcategory( SUBCAT_INPUT_DEMUX );
        set_shortname( _("Lua Playlist") );
        set_description( _("Lua Playlist Parser Interface") );
        set_capability( "demux2", 9 );
        set_callbacks( E_(Import_LuaPlaylist), E_(Close_LuaPlaylist) );
vlc_module_end();

/*****************************************************************************
 * Lua function bridge
 *****************************************************************************/
vlc_object_t * vlclua_get_this( lua_State *p_state )
{
    vlc_object_t * p_this;
    lua_getglobal( p_state, "vlc" );
    lua_getfield( p_state, lua_gettop( p_state ), "private" );
    p_this = (vlc_object_t*)lua_topointer( p_state, lua_gettop( p_state ) );
    lua_pop( p_state, 2 );
    return p_this;
}

int vlclua_stream_new( lua_State *p_state )
{
    vlc_object_t * p_this = vlclua_get_this( p_state );
    int i = lua_gettop( p_state );
    stream_t * p_stream;
    const char * psz_url;
    if( !i ) return 0;
    psz_url = lua_tostring( p_state, 1 );
    lua_pop( p_state, i );
    p_stream = stream_UrlNew( p_this, psz_url );
    if( !p_stream ) return 0;
    lua_pushlightuserdata( p_state, p_stream );
    return 1;
}

int vlclua_stream_read( lua_State *p_state )
{
    int i = lua_gettop( p_state );
    stream_t * p_stream;
    int n;
    byte_t *p_read;
    int i_read;
    if( !i ) return 0;
    p_stream = (stream_t *)lua_topointer( p_state, 1 );
    n = lua_tonumber( p_state, 2 );
    lua_pop( p_state, i );
    p_read = malloc( n );
    if( !p_read ) return 0;
    i_read = stream_Read( p_stream, p_read, n );
    lua_pushlstring( p_state, (const char *)p_read, i_read );
    free( p_read );
    return 1;
}

int vlclua_stream_readline( lua_State *p_state )
{
    int i = lua_gettop( p_state );
    stream_t * p_stream;
    if( !i ) return 0;
    p_stream = (stream_t *)lua_topointer( p_state, 1 );
    lua_pop( p_state, i );
    char *psz_line = stream_ReadLine( p_stream );
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

int vlclua_stream_delete( lua_State *p_state )
{
    int i = lua_gettop( p_state );
    stream_t * p_stream;
    if( !i ) return 0;
    p_stream = (stream_t *)lua_topointer( p_state, 1 );
    lua_pop( p_state, i );
    stream_Delete( p_stream );
    return 1;
}

int vlclua_decode_uri( lua_State *p_state )
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

int vlclua_resolve_xml_special_chars( lua_State *p_state )
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

int vlclua_msg_dbg( lua_State *p_state )
{
    vlc_object_t *p_this = vlclua_get_this( p_state );
    int i = lua_gettop( p_state );
    if( !i ) return 0;
    const char *psz_cstring = lua_tostring( p_state, 1 );
    if( !psz_cstring ) return 0;
    msg_Dbg( p_this, "%s", psz_cstring );
    return 0;
}
int vlclua_msg_warn( lua_State *p_state )
{
    vlc_object_t *p_this = vlclua_get_this( p_state );
    int i = lua_gettop( p_state );
    if( !i ) return 0;
    const char *psz_cstring = lua_tostring( p_state, 1 );
    if( !psz_cstring ) return 0;
    msg_Warn( p_this, "%s", psz_cstring );
    return 0;
}
int vlclua_msg_err( lua_State *p_state )
{
    vlc_object_t *p_this = vlclua_get_this( p_state );
    int i = lua_gettop( p_state );
    if( !i ) return 0;
    const char *psz_cstring = lua_tostring( p_state, 1 );
    if( !psz_cstring ) return 0;
    msg_Err( p_this, "%s", psz_cstring );
    return 0;
}
int vlclua_msg_info( lua_State *p_state )
{
    vlc_object_t *p_this = vlclua_get_this( p_state );
    int i = lua_gettop( p_state );
    if( !i ) return 0;
    const char *psz_cstring = lua_tostring( p_state, 1 );
    if( !psz_cstring ) return 0;
    msg_Info( p_this, "%s", psz_cstring );
    return 0;
}

/*****************************************************************************
 *
 *****************************************************************************/
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
 * Will execute func on all scripts in luadirname, and stop if func returns
 * success.
 *****************************************************************************/
int vlclua_scripts_batch_execute( vlc_object_t *p_this,
                                  const char * luadirname,
                                  int (*func)(vlc_object_t *, const char *, lua_State *, void *),
                                  lua_State * p_state,
                                  void * user_data)
{
    int i_ret = VLC_EGENERIC;

    DIR   *dir           = NULL;
    char **ppsz_filelist = NULL;
    char **ppsz_fileend  = NULL;
    char **ppsz_file;

    char  *ppsz_dir_list[] = { NULL, NULL, NULL, NULL };
    char **ppsz_dir;

    if( asprintf( &ppsz_dir_list[0], "%s" DIR_SEP "%s",
                   p_this->p_libvlc->psz_datadir, luadirname ) < 0 )
        return VLC_ENOMEM;

#   if defined(__APPLE__) || defined(SYS_BEOS) || defined(WIN32)
    {
        const char *psz_vlcpath = config_GetDataDir();
        if( asprintf( &ppsz_dir_list[1], "%s" DIR_SEP "%s", psz_vlcpath, luadirname )  < 0 )
            return VLC_ENOMEM;

        if( asprintf( &ppsz_dir_list[2], "%s" DIR_SEP "share" DIR_SEP "%s", psz_vlcpath, luadirname )  < 0 )
            return VLC_ENOMEM;
    }
#   else
    if( asprintf( &ppsz_dir_list[1],
                  "share" DIR_SEP "%s", luadirname ) < 0 )
        return VLC_ENOMEM;

#   ifdef HAVE_SYS_STAT_H
    {
        struct stat stat_info;
        if( ( utf8_stat( ppsz_dir_list[1], &stat_info ) == -1 )
            || !S_ISDIR( stat_info.st_mode ) )
        {
            free(ppsz_dir_list[1]);
            if( asprintf( &ppsz_dir_list[1],
                          DATA_PATH DIR_SEP "%s", luadirname ) < 0 )
                return VLC_ENOMEM;
        }
    }
#   endif
#   endif

    for( ppsz_dir = ppsz_dir_list; *ppsz_dir; ppsz_dir++ )
    {
        int i_files;

        if( ppsz_filelist )
        {
            for( ppsz_file = ppsz_filelist; ppsz_file < ppsz_fileend;
                 ppsz_file++ )
                free( *ppsz_file );
            free( ppsz_filelist );
            ppsz_filelist = NULL;
        }

        if( dir )
        {
            closedir( dir );
        }

        msg_Dbg( p_this, "Trying Lua scripts in %s", *ppsz_dir );
        dir = utf8_opendir( *ppsz_dir );

        if( !dir ) continue;
        i_files = utf8_loaddir( dir, &ppsz_filelist, file_select, file_compare );
        if( i_files < 1 ) continue;
        ppsz_fileend = ppsz_filelist + i_files;

        for( ppsz_file = ppsz_filelist; ppsz_file < ppsz_fileend; ppsz_file++ )
        {
            char  *psz_filename;
            if( asprintf( &psz_filename,
                          "%s" DIR_SEP "%s", *ppsz_dir, *ppsz_file ) < 0)
                return VLC_ENOMEM;
            msg_Dbg( p_this, "Trying Lua playlist script %s", psz_filename );
 
            i_ret = func( p_this, psz_filename, p_state, user_data );
 
            free( psz_filename );

            if( i_ret == VLC_SUCCESS ) break;
        }
        if( i_ret == VLC_SUCCESS ) break;
    }

    if( ppsz_filelist )
    {
        for( ppsz_file = ppsz_filelist; ppsz_file < ppsz_fileend;
             ppsz_file++ )
            free( *ppsz_file );
        free( ppsz_filelist );
    }
    for( ppsz_dir = ppsz_dir_list; *ppsz_dir; ppsz_dir++ )
        free( *ppsz_dir );

    if( dir ) closedir( dir );

    return i_ret;
}


/*****************************************************************************
 * Meta data setters utility.
 *****************************************************************************/
void vlclua_read_meta_data( vlc_object_t *p_this, lua_State *p_state,
                            int o, int t, input_item_t *p_input )
{
    const char *psz_value;
#define TRY_META( a, b )                                    \
    lua_getfield( p_state, o, a );                          \
    if( lua_isstring( p_state, t ) )                        \
    {                                                       \
        psz_value = lua_tostring( p_state, t );             \
        EnsureUTF8( psz_value );                            \
        msg_Dbg( p_this, #b ": %s", psz_value );            \
        input_item_Set ## b ( p_input, psz_value );         \
    }                                                       \
    lua_pop( p_state, 1 ); /* pop a */
    TRY_META( "title", Title );
    TRY_META( "artist", Artist );
    TRY_META( "genre", Genre );
    TRY_META( "copyright", Copyright );
    TRY_META( "album", Album );
    TRY_META( "tracknum", TrackNum );
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
}

void vlclua_read_custom_meta_data( vlc_object_t *p_this, lua_State *p_state,
                                   int o, int t, input_item_t *p_input )
{
    lua_getfield( p_state, o, "meta" );
    if( lua_istable( p_state, t ) )
    {
        lua_pushnil( p_state );
        while( lua_next( p_state, t ) )
        {
            if( !lua_isstring( p_state, t+1 ) )
            {
                msg_Warn( p_this, "Custom meta data category name must be "
                                   "a string" );
            }
            else if( !lua_istable( p_state, t+2 ) )
            {
                msg_Warn( p_this, "Custom meta data category contents "
                                   "must be a table" );
            }
            else
            {
                const char *psz_meta_category = lua_tostring( p_state, t+1 );
                msg_Dbg( p_this, "Found custom meta data category: %s",
                         psz_meta_category );
                lua_pushnil( p_state );
                while( lua_next( p_state, t+2 ) )
                {
                    if( !lua_isstring( p_state, t+3 ) )
                    {
                        msg_Warn( p_this, "Custom meta category item name "
                                           "must be a string." );
                    }
                    else if( !lua_isstring( p_state, t+4 ) )
                    {
                        msg_Warn( p_this, "Custom meta category item value "
                                           "must be a string." );
                    }
                    else
                    {
                        const char *psz_meta_name =
                            lua_tostring( p_state, t+3 );
                        const char *psz_meta_value =
                            lua_tostring( p_state, t+4 );
                        msg_Dbg( p_this, "Custom meta %s, %s: %s",
                                 psz_meta_category, psz_meta_name,
                                 psz_meta_value );
                        input_ItemAddInfo( p_input, psz_meta_category,
                                           psz_meta_name, psz_meta_value );
                    }
                    lua_pop( p_state, 1 ); /* pop item */
                }
            }
            lua_pop( p_state, 1 ); /* pop category */
        }
    }
    lua_pop( p_state, 1 ); /* pop "meta" */
}

