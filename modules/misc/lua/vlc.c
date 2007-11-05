/*****************************************************************************
 * vlc.c: Generic lua inteface functions
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
#include <vlc_aout.h>

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */
#include <lualib.h>     /* Lua libs */

#include "vlc.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define INTF_TEXT N_("Lua interface")
#define INTF_LONGTEXT N_("Lua interface module to load")

#define CONFIG_TEXT N_("Lua inteface configuration")
#define CONFIG_LONGTEXT N_("Lua interface configuration string. Format is: '[\"<interface module name>\"] = { <option> = <value>, ...}, ...'.")

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
    add_submodule();
        add_shortcut( "luaintf" );
        add_shortcut( "luarc" );
        /* add_shortcut( "rc" ); */
        add_shortcut( "luahotkeys" );
        /* add_shortcut( "hotkeys" ); */
        add_shortcut( "luatelnet" );
        /* add_shortcut( "telnet" ); */
        set_description( _("Lua Interface Module") );
        set_capability( "interface", 0 );
        add_string( "lua-intf", "dummy", NULL,
                    INTF_TEXT, INTF_LONGTEXT, VLC_FALSE );
        add_string( "lua-config", "", NULL,
                    CONFIG_TEXT, CONFIG_LONGTEXT, VLC_FALSE );
        set_callbacks( E_(Open_LuaIntf), E_(Close_LuaIntf) );
vlc_module_end();

/*****************************************************************************
 * Internal lua<->vlc utils
 *****************************************************************************/
vlc_object_t * vlclua_get_this( lua_State *L )
{
    vlc_object_t * p_this;
    lua_getglobal( L, "vlc" );
    lua_getfield( L, -1, "private" );
    p_this = (vlc_object_t*)lua_topointer( L, lua_gettop( L ) );
    lua_pop( L, 2 );
    return p_this;
}

/*****************************************************************************
 * VLC error code translation
 *****************************************************************************/
int vlclua_push_ret( lua_State *L, int i_error )
{
    lua_pushnumber( L, i_error );
    lua_pushstring( L, vlc_error( i_error ) );
    return 2;
}

/*****************************************************************************
 * Get the VLC version string
 *****************************************************************************/
int vlclua_version( lua_State *L )
{
    lua_pushstring( L, VLC_Version() );
    return 1;
}

/*****************************************************************************
 * Get the VLC license msg/disclaimer
 *****************************************************************************/
int vlclua_license( lua_State *L )
{
    lua_pushstring( L, LICENSE_MSG );
    return 1;
}

/*****************************************************************************
 * Quit VLC
 *****************************************************************************/
int vlclua_quit( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    /* The rc.c code also stops the playlist ... not sure if this is needed
     * though. */
    vlc_object_kill( p_this->p_libvlc );
    return 0;
}

/*****************************************************************************
 * Volume related
 *****************************************************************************/
int vlclua_volume_set( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    int i_volume = luaL_checkint( L, 1 );
    /* Do we need to check that i_volume is in the AOUT_VOLUME_MIN->MAX range?*/
    return vlclua_push_ret( L, aout_VolumeSet( p_this, i_volume ) );
}

int vlclua_volume_get( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    audio_volume_t i_volume;
    if( aout_VolumeGet( p_this, &i_volume ) == VLC_SUCCESS )
        lua_pushnumber( L, i_volume );
    else
        lua_pushnil( L );
    return 1;
}

int vlclua_volume_up( lua_State *L )
{
    audio_volume_t i_volume;
    aout_VolumeUp( vlclua_get_this( L ),
                   luaL_optint( L, 1, 1 ),
                   &i_volume );
    lua_pushnumber( L, i_volume );
    return 1;
}

int vlclua_volume_down( lua_State *L )
{
    audio_volume_t i_volume;
    aout_VolumeDown( vlclua_get_this( L ),
                     luaL_optint( L, 1, 1 ),
                     &i_volume );
    lua_pushnumber( L, i_volume );
    return 1;
}

/*****************************************************************************
 * Stream handling
 *****************************************************************************/
int vlclua_stream_new( lua_State *L )
{
    vlc_object_t * p_this = vlclua_get_this( L );
    stream_t * p_stream;
    const char * psz_url;
    psz_url = luaL_checkstring( L, -1 );
    p_stream = stream_UrlNew( p_this, psz_url );
    if( !p_stream )
        return luaL_error( L, "Error when opening url: `%s'", psz_url );
    lua_pushlightuserdata( L, p_stream );
    return 1;
}

int vlclua_stream_read( lua_State *L )
{
    stream_t * p_stream;
    int n;
    byte_t *p_read;
    int i_read;
    p_stream = (stream_t *)luaL_checklightuserdata( L, 1 );
    n = luaL_checkint( L, 2 );
    p_read = malloc( n );
    if( !p_read ) return vlclua_error( L );
    i_read = stream_Read( p_stream, p_read, n );
    lua_pushlstring( L, (const char *)p_read, i_read );
    free( p_read );
    return 1;
}

int vlclua_stream_readline( lua_State *L )
{
    stream_t * p_stream;
    p_stream = (stream_t *)luaL_checklightuserdata( L, 1 );
    char *psz_line = stream_ReadLine( p_stream );
    if( psz_line )
    {
        lua_pushstring( L, psz_line );
        free( psz_line );
    }
    else
        lua_pushnil( L );
    return 1;
}

int vlclua_stream_delete( lua_State *L )
{
    stream_t * p_stream;
    p_stream = (stream_t *)luaL_checklightuserdata( L, 1 );
    stream_Delete( p_stream );
    return 0;
}

/*****************************************************************************
 * String transformations
 *****************************************************************************/
int vlclua_decode_uri( lua_State *L )
{
    int i_top = lua_gettop( L );
    int i;
    for( i = 1; i <= i_top; i++ )
    {
        const char *psz_cstring = luaL_checkstring( L, 1 );
        char *psz_string = strdup( psz_cstring );
        lua_remove( L, 1 ); /* remove elements to prevent being limited by
                             * the stack's size (this function will work with
                             * up to (stack size - 1) arguments */
        decode_URI( psz_string );
        lua_pushstring( L, psz_string );
        free( psz_string );
    }
    return i_top;
}

int vlclua_resolve_xml_special_chars( lua_State *L )
{
    int i_top = lua_gettop( L );
    int i;
    for( i = 1; i <= i_top; i++ )
    {
        const char *psz_cstring = luaL_checkstring( L, 1 );
        char *psz_string = strdup( psz_cstring );
        lua_remove( L, 1 ); /* remove elements to prevent being limited by
                             * the stack's size (this function will work with
                             * up to (stack size - 1) arguments */
        resolve_xml_special_chars( psz_string );
        lua_pushstring( L, psz_string );
        free( psz_string );
    }
    return i_top;
}

/*****************************************************************************
 * Messaging facilities
 *****************************************************************************/
int vlclua_msg_dbg( lua_State *L )
{
    int i_top = lua_gettop( L );
    vlc_object_t *p_this = vlclua_get_this( L );
    int i;
    for( i = 1; i <= i_top; i++ )
        msg_Dbg( p_this, "%s", luaL_checkstring( L, 1 ) );
    return 0;
}
int vlclua_msg_warn( lua_State *L )
{
    int i_top = lua_gettop( L );
    vlc_object_t *p_this = vlclua_get_this( L );
    int i;
    for( i = 1; i <= i_top; i++ )
        msg_Warn( p_this, "%s", luaL_checkstring( L, i ) );
    return 0;
}
int vlclua_msg_err( lua_State *L )
{
    int i_top = lua_gettop( L );
    vlc_object_t *p_this = vlclua_get_this( L );
    int i;
    for( i = 1; i <= i_top; i++ )
        msg_Err( p_this, "%s", luaL_checkstring( L, i ) );
    return 0;
}
int vlclua_msg_info( lua_State *L )
{
    int i_top = lua_gettop( L );
    vlc_object_t *p_this = vlclua_get_this( L );
    int i;
    for( i = 1; i <= i_top; i++ )
        msg_Info( p_this, "%s", luaL_checkstring( L, i ) );
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

int vlclua_dir_list( vlc_object_t *p_this, const char *luadirname,
                     char **ppsz_dir_list )
{
    if( asprintf( &ppsz_dir_list[0], "%s" DIR_SEP "%s",
                   p_this->p_libvlc->psz_datadir, luadirname ) < 0 )
        return VLC_ENOMEM;

#   if defined(__APPLE__) || defined(SYS_BEOS) || defined(WIN32)
    {
        const char *psz_vlcpath = config_GetDataDir();
        if( asprintf( &ppsz_dir_list[1], "%s" DIR_SEP "%s",
                      psz_vlcpath, luadirname )  < 0 )
            return VLC_ENOMEM;

        if( asprintf( &ppsz_dir_list[2], "%s" DIR_SEP "share" DIR_SEP "%s",
                      psz_vlcpath, luadirname )  < 0 )
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
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Will execute func on all scripts in luadirname, and stop if func returns
 * success.
 *****************************************************************************/
int vlclua_scripts_batch_execute( vlc_object_t *p_this,
                                  const char * luadirname,
                                  int (*func)(vlc_object_t *, const char *, lua_State *, void *),
                                  lua_State * L,
                                  void * user_data)
{
    int i_ret = VLC_EGENERIC;

    DIR   *dir           = NULL;
    char **ppsz_filelist = NULL;
    char **ppsz_fileend  = NULL;
    char **ppsz_file;

    char  *ppsz_dir_list[] = { NULL, NULL, NULL, NULL };
    char **ppsz_dir;

    i_ret = vlclua_dir_list( p_this, luadirname, ppsz_dir_list );
    if( i_ret != VLC_SUCCESS )
        return i_ret;
    i_ret = VLC_EGENERIC;


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
        i_files = utf8_loaddir( dir, &ppsz_filelist, file_select,
                                file_compare );
        if( i_files < 1 ) continue;
        ppsz_fileend = ppsz_filelist + i_files;

        for( ppsz_file = ppsz_filelist; ppsz_file < ppsz_fileend; ppsz_file++ )
        {
            char  *psz_filename;
            if( asprintf( &psz_filename,
                          "%s" DIR_SEP "%s", *ppsz_dir, *ppsz_file ) < 0)
                return VLC_ENOMEM;
            msg_Dbg( p_this, "Trying Lua playlist script %s", psz_filename );

            i_ret = func( p_this, psz_filename, L, user_data );

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
 * Playlist item table should be on top of the stack when these are called
 *****************************************************************************/
void __vlclua_read_meta_data( vlc_object_t *p_this, lua_State *L,
                              input_item_t *p_input )
{
#define TRY_META( a, b )                                        \
    lua_getfield( L, -1, a );                                   \
    if( lua_isstring( L, -1 ) )                                 \
    {                                                           \
        char *psz_value = strdup( lua_tostring( L, -1 ) );      \
        EnsureUTF8( psz_value );                                \
        msg_Dbg( p_this, #b ": %s", psz_value );                \
        input_item_Set ## b ( p_input, psz_value );             \
        free( psz_value );                                      \
    }                                                           \
    lua_pop( L, 1 ); /* pop a */
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

void __vlclua_read_custom_meta_data( vlc_object_t *p_this, lua_State *L,
                                     input_item_t *p_input )
{
    /* ... item */
    lua_getfield( L, -1, "meta" );
    /* ... item meta */
    if( lua_istable( L, -1 ) )
    {
        lua_pushnil( L );
        /* ... item meta nil */
        while( lua_next( L, -2 ) )
        {
            /* ... item meta key value */
            if( !lua_isstring( L, -2 ) )
            {
                msg_Warn( p_this, "Custom meta data category name must be "
                                   "a string" );
            }
            else if( !lua_istable( L, -1 ) )
            {
                msg_Warn( p_this, "Custom meta data category contents "
                                   "must be a table" );
            }
            else
            {
                const char *psz_meta_category = lua_tostring( L, -2 );
                msg_Dbg( p_this, "Found custom meta data category: %s",
                         psz_meta_category );
                lua_pushnil( L );
                /* ... item meta key value nil */
                while( lua_next( L, -2 ) )
                {
                    /* ... item meta key value key2 value2 */
                    if( !lua_isstring( L, -2 ) )
                    {
                        msg_Warn( p_this, "Custom meta category item name "
                                           "must be a string." );
                    }
                    else if( !lua_isstring( L, -1 ) )
                    {
                        msg_Warn( p_this, "Custom meta category item value "
                                           "must be a string." );
                    }
                    else
                    {
                        const char *psz_meta_name =
                            lua_tostring( L, -2 );
                        const char *psz_meta_value =
                            lua_tostring( L, -1 );
                        msg_Dbg( p_this, "Custom meta %s, %s: %s",
                                 psz_meta_category, psz_meta_name,
                                 psz_meta_value );
                        input_ItemAddInfo( p_input, psz_meta_category,
                                           psz_meta_name, psz_meta_value );
                    }
                    lua_pop( L, 1 ); /* pop item */
                    /* ... item meta key value key2 */
                }
                /* ... item meta key value */
            }
            lua_pop( L, 1 ); /* pop category */
            /* ... item meta key */
        }
        /* ... item meta */
    }
    lua_pop( L, 1 ); /* pop "meta" */
    /* ... item -> back to original stack */
}

/*****************************************************************************
 * Playlist utilities
 ****************************************************************************/
/**
 * Playlist item table should be on top of the stack when this is called
 */
void __vlclua_read_options( vlc_object_t *p_this, lua_State *L,
                            int *pi_options, char ***pppsz_options )
{
    lua_getfield( L, -1, "options" );
    if( lua_istable( L, -1 ) )
    {
        lua_pushnil( L );
        while( lua_next( L, -2 ) )
        {
            if( lua_isstring( L, -1 ) )
            {
                char *psz_option = strdup( lua_tostring( L, -1 ) );
                msg_Dbg( p_this, "Option: %s", psz_option );
                INSERT_ELEM( *pppsz_options, *pi_options, *pi_options,
                             psz_option );
            }
            else
            {
                msg_Warn( p_this, "Option should be a string" );
            }
            lua_pop( L, 1 ); /* pop option */
        }
    }
    lua_pop( L, 1 ); /* pop "options" */
}

int __vlclua_playlist_add_internal( vlc_object_t *p_this, lua_State *L,
                                    playlist_t *p_playlist,
                                    input_item_t *p_parent, vlc_bool_t b_play )
{
    int i_count = 0;

    /* playlist */
    if( lua_istable( L, -1 ) )
    {
        lua_pushnil( L );
        /* playlist nil */
        while( lua_next( L, -2 ) )
        {
            /* playlist key item */
            /* <Parse playlist item> */
            if( lua_istable( L, -1 ) )
            {
                lua_getfield( L, -1, "path" );
                /* playlist key item path */
                if( lua_isstring( L, -1 ) )
                {
                    const char   *psz_path     = NULL;
                    const char   *psz_name     = NULL;
                    char        **ppsz_options = NULL;
                    int           i_options    = 0;
                    mtime_t       i_duration   = -1;
                    input_item_t *p_input;

                    /* Read path and name */
                    psz_path = lua_tostring( L, -1 );
                    msg_Dbg( p_this, "Path: %s", psz_path );
                    lua_getfield( L, -2, "name" );
                    /* playlist key item path name */
                    if( lua_isstring( L, -1 ) )
                    {
                        psz_name = lua_tostring( L, -1 );
                        msg_Dbg( p_this, "Name: %s", psz_name );
                    }
                    else
                    {
                        if( !lua_isnil( L, -1 ) )
                            msg_Warn( p_this, "Playlist item name should be a string." );
                        psz_name = psz_path;
                    }

                    /* Read duration */
                    lua_getfield( L, -3, "duration" );
                    /* playlist key item path name duration */
                    if( lua_isnumber( L, -1 ) )
                    {
                        i_duration = (mtime_t)(lua_tonumber( L, -1 )*1e6);
                    }
                    else if( !lua_isnil( L, -1 ) )
                    {
                        msg_Warn( p_this, "Playlist item duration should be a number (in seconds)." );
                    }
                    lua_pop( L, 1 ); /* pop "duration" */

                    /* playlist key item path name */

                    /* Read options: item must be on top of stack */
                    lua_pushvalue( L, -3 );
                    /* playlist key item path name item */
                    vlclua_read_options( p_this, L, &i_options, &ppsz_options );

                    /* Create input item */
                    p_input = input_ItemNewExt( p_playlist, psz_path,
                                                psz_name, i_options,
                                                (const char **)ppsz_options,
                                                i_duration );
                    lua_pop( L, 3 ); /* pop "path name item" */
                    /* playlist key item */

                    /* Read meta data: item must be on top of stack */
                    vlclua_read_meta_data( p_this, L, p_input );

                    /* Read custom meta data: item must be on top of stack*/
                    vlclua_read_custom_meta_data( p_this, L, p_input );

                    /* Append item to playlist */
                    if( p_parent ) /* Add to node */
                        input_ItemAddSubItem( p_parent, p_input );
                    else if( b_play ) /* Play */
                        playlist_AddInput( p_playlist, p_input,
                                           PLAYLIST_APPEND | PLAYLIST_GO,
                                           PLAYLIST_END, VLC_TRUE, VLC_FALSE );
                    else /* Enqueue */
                        playlist_AddInput( p_playlist, p_input,
                                           PLAYLIST_APPEND | PLAYLIST_PREPARSE,
                                           PLAYLIST_END, VLC_TRUE, VLC_FALSE );
                    i_count ++; /* increment counter */

                    while( i_options > 0 )
                        free( ppsz_options[--i_options] );
                    free( ppsz_options );
                }
                else
                {
                    lua_pop( L, 1 ); /* pop "path" */
                    msg_Warn( p_this,
                             "Playlist item's path should be a string" );
                }
                /* playlist key item */
            }
            else
            {
                msg_Warn( p_this, "Playlist item should be a table" );
            }
            /* <Parse playlist item> */
            lua_pop( L, 1 ); /* pop the value, keep the key for
                              * the next lua_next() call */
            /* playlist key */
        }
        /* playlist */
    }
    else
    {
        msg_Warn( p_this, "Playlist should be a table." );
    }
    return i_count;
}
