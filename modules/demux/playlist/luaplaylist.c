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
    set_capability( "demux2", 9 );
    set_callbacks( E_(Import_LuaPlaylist), E_(Close_LuaPlaylist) );
vlc_module_end();

/*****************************************************************************
 *
 *****************************************************************************/
struct demux_sys_t
{
    lua_State *p_state;
    char *psz_filename;
};

/*****************************************************************************
 *
 *****************************************************************************/
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

static int vlclua_msg_dbg( lua_State *p_state )
{
    demux_t *p_demux = vlclua_get_demux( p_state );
    int i = lua_gettop( p_state );
    if( !i ) return 0;
    const char *psz_cstring = lua_tostring( p_state, 1 );
    if( !psz_cstring ) return 0;
    msg_Dbg( p_demux, "%s: %s", p_demux->p_sys->psz_filename, psz_cstring );
    return 0;
}
static int vlclua_msg_warn( lua_State *p_state )
{
    demux_t *p_demux = vlclua_get_demux( p_state );
    int i = lua_gettop( p_state );
    if( !i ) return 0;
    const char *psz_cstring = lua_tostring( p_state, 1 );
    if( !psz_cstring ) return 0;
    msg_Warn( p_demux, "%s: %s", p_demux->p_sys->psz_filename, psz_cstring );
    return 0;
}
static int vlclua_msg_err( lua_State *p_state )
{
    demux_t *p_demux = vlclua_get_demux( p_state );
    int i = lua_gettop( p_state );
    if( !i ) return 0;
    const char *psz_cstring = lua_tostring( p_state, 1 );
    if( !psz_cstring ) return 0;
    msg_Err( p_demux, "%s: %s", p_demux->p_sys->psz_filename, psz_cstring );
    return 0;
}
static int vlclua_msg_info( lua_State *p_state )
{
    demux_t *p_demux = vlclua_get_demux( p_state );
    int i = lua_gettop( p_state );
    if( !i ) return 0;
    const char *psz_cstring = lua_tostring( p_state, 1 );
    if( !psz_cstring ) return 0;
    msg_Info( p_demux, "%s: %s", p_demux->p_sys->psz_filename, psz_cstring );
    return 0;
}

/* Functions to register */
static luaL_Reg p_reg[] =
{
    { "peek", vlclua_peek },
    { "decode_uri", vlclua_decode_uri },
    { "resolve_xml_special_chars", vlclua_resolve_xml_special_chars },
    { "msg_dbg", vlclua_msg_dbg },
    { "msg_warn", vlclua_msg_warn },
    { "msg_err", vlclua_msg_err },
    { "msg_info", vlclua_msg_info },
    { NULL, NULL }
};

/* Functions to register for parse() function call only */
static luaL_Reg p_reg_parse[] =
{
    { "read", vlclua_read },
    { "readline", vlclua_readline },
    { NULL, NULL }
};

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
 * Import_LuaPlaylist: main import function
 *****************************************************************************/
int E_(Import_LuaPlaylist)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    lua_State *p_state;
    int i_ret = VLC_EGENERIC;

    char  *psz_filename  = NULL;

    DIR   *dir           = NULL;
    char **ppsz_filelist = NULL;
    char **ppsz_fileend  = NULL;
    char **ppsz_file;

    char  *ppsz_dir_list[] = { NULL, NULL, NULL, NULL };
    char **ppsz_dir;

    p_demux->p_sys = (demux_sys_t*)malloc( sizeof( demux_sys_t ) );
    if( !p_demux->p_sys )
    {
        return VLC_ENOMEM;
    }

    p_demux->p_sys->psz_filename = NULL;

    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;

    /* Initialise Lua state structure */
    p_state = luaL_newstate();
    if( !p_state )
    {
        msg_Err( p_demux, "Could not create new Lua State" );
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

    if( asprintf( &ppsz_dir_list[0], "%s" DIR_SEP CONFIG_DIR DIR_SEP "luaplaylist",
             p_demux->p_libvlc->psz_homedir ) < -1 )
        return VLC_ENOMEM;

#   if defined(__APPLE__) || defined(SYS_BEOS) || defined(WIN32)
    {
        const char *psz_vlcpath = NULL;//config_GetDataDir();
        if( asprintf( &ppsz_dir_list[1], "%s" DIR_SEP "luaplaylist", psz_vlcpath ) < 0 )
            return VLC_ENOMEM;
        if( asprintf( &ppsz_dir_list[2], "%s" DIR_SEP "share" DIR_SEP "luaplaylist", psz_vlcpath ) < 0 )
            return VLC_ENOMEM;
    }
#   else
    {
#   ifdef HAVE_SYS_STAT_H
        struct stat stat_info;
        if( ( utf8_stat( "share/luaplaylist", &stat_info ) == -1 )
            || !S_ISDIR( stat_info.st_mode ) )
        {
            ppsz_dir_list[1] = strdup( DATA_PATH "/luaplaylist" );
        }
        else
#   endif
        {
            ppsz_dir_list[1] = strdup( "share/luaplaylist" );
        }
    }
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

        msg_Dbg( p_demux, "Trying Lua scripts in %s", *ppsz_dir );
        dir = utf8_opendir( *ppsz_dir );

        if( !dir ) continue;
        i_files = utf8_loaddir( dir, &ppsz_filelist, file_select, file_compare );
        if( i_files < 1 ) continue;
        ppsz_fileend = ppsz_filelist + i_files;

        for( ppsz_file = ppsz_filelist; ppsz_file < ppsz_fileend; ppsz_file++ )
        {
            free( psz_filename ); psz_filename = NULL;
            asprintf( &psz_filename, "%s" DIR_SEP "%s", *ppsz_dir, *ppsz_file );
            msg_Dbg( p_demux, "Trying Lua playlist script %s", psz_filename );
            p_demux->p_sys->psz_filename = psz_filename;

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
    free( p_demux->p_sys->psz_filename );
    free( p_demux->p_sys );
}

static inline void read_options( demux_t *p_demux, lua_State *p_state,
                                 int o, int t, int *pi_options,
                                 char ***pppsz_options )
{
    lua_getfield( p_state, o, "options" );
    if( lua_istable( p_state, t ) )
    {
        lua_pushnil( p_state );
        while( lua_next( p_state, t ) )
        {
            if( lua_isstring( p_state, t+2 ) )
            {
                char *psz_option = strdup( lua_tostring( p_state, t+2 ) );
                msg_Dbg( p_demux, "Option: %s", psz_option );
                INSERT_ELEM( *pppsz_options, *pi_options, *pi_options,
                             psz_option );
            }
            else
            {
                msg_Warn( p_demux, "Option should be a string" );
            }
            lua_pop( p_state, 1 ); /* pop option */
        }
    }
    lua_pop( p_state, 1 ); /* pop "options" */
}

static inline void read_meta_data( demux_t *p_demux,
                                   lua_State *p_state, int o, int t,
                                   input_item_t *p_input )
{
    const char *psz_value;
#define TRY_META( a, b )                                    \
    lua_getfield( p_state, o, a );                          \
    if( lua_isstring( p_state, t ) )                        \
    {                                                       \
        psz_value = lua_tostring( p_state, t );             \
        msg_Dbg( p_demux, #b ": %s", psz_value );           \
        input_item_Set ## b ( p_input, psz_value );   \
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

static inline void read_custom_meta_data( demux_t *p_demux,
                                          lua_State *p_state, int o, int t,
                                          input_item_t *p_input )
{
    lua_getfield( p_state, o, "meta" );
    if( lua_istable( p_state, t ) )
    {
        lua_pushnil( p_state );
        while( lua_next( p_state, t ) )
        {
            if( !lua_isstring( p_state, t+1 ) )
            {
                msg_Warn( p_demux, "Custom meta data category name must be "
                                   "a string" );
            }
            else if( !lua_istable( p_state, t+2 ) )
            {
                msg_Warn( p_demux, "Custom meta data category contents "
                                   "must be a table" );
            }
            else
            {
                const char *psz_meta_category = lua_tostring( p_state, t+1 );
                msg_Dbg( p_demux, "Found custom meta data category: %s",
                         psz_meta_category );
                lua_pushnil( p_state );
                while( lua_next( p_state, t+2 ) )
                {
                    if( !lua_isstring( p_state, t+3 ) )
                    {
                        msg_Warn( p_demux, "Custom meta category item name "
                                           "must be a string." );
                    }
                    else if( !lua_isstring( p_state, t+4 ) )
                    {
                        msg_Warn( p_demux, "Custom meta category item value "
                                           "must be a string." );
                    }
                    else
                    {
                        const char *psz_meta_name =
                            lua_tostring( p_state, t+3 );
                        const char *psz_meta_value =
                            lua_tostring( p_state, t+4 );
                        msg_Dbg( p_demux, "Custom meta %s, %s: %s",
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

static int Demux( demux_t *p_demux )
{
    input_item_t *p_input;
    lua_State *p_state = p_demux->p_sys->p_state;
    char *psz_filename = p_demux->p_sys->psz_filename;
    int t;

    INIT_PLAYLIST_STUFF;

    luaL_register( p_state, "vlc", p_reg_parse );

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

    /* Check that the Lua stack is big enough and grow it if needed.
     * Should be ok since LUA_MINSTACK is 20 but we never know. */
    lua_checkstack( p_state, 8 );

    if( ( t = lua_gettop( p_state ) ) )
    {

        if( lua_istable( p_state, t ) )
        {
            lua_pushnil( p_state );
            while( lua_next( p_state, t ) )
            {
                if( lua_istable( p_state, t+2 ) )
                {
                    lua_getfield( p_state, t+2, "path" );
                    if( lua_isstring( p_state, t+3 ) )
                    {
                        const char  *psz_path     = NULL;
                        const char  *psz_name     = NULL;
                        char       **ppsz_options = NULL;
                        int          i_options    = 0;
                        mtime_t      i_duration   = -1;

                        /* Read path and name */
                        psz_path = lua_tostring( p_state, t+3 );
                        msg_Dbg( p_demux, "Path: %s", psz_path );
                        lua_getfield( p_state, t+2, "name" );
                        if( lua_isstring( p_state, t+4 ) )
                        {
                            psz_name = lua_tostring( p_state, t+4 );
                            msg_Dbg( p_demux, "Name: %s", psz_name );
                        }
                        else
                        {
                            psz_name = psz_path;
                        }

                        /* Read duration */
                        lua_getfield( p_state, t+2, "duration" );
                        if( lua_isnumber( p_state, t+5 ) )
                        {
                            i_duration = (mtime_t)lua_tointeger( p_state, t+5 );
                            i_duration *= 1000000;
                        }
                        lua_pop( p_state, 1 ); /* pop "duration" */

                        /* Read options */
                        read_options( p_demux, p_state, t+2, t+5,
                                      &i_options, &ppsz_options );

                        /* Create input item */
                        p_input = input_ItemNewExt( p_playlist, psz_path,
                                                    psz_name, i_options,
                                                    (const char **)ppsz_options,
                                                    i_duration );
                        lua_pop( p_state, 1 ); /* pop "name" */

                        /* Read meta data */
                        read_meta_data( p_demux, p_state, t+2, t+4, p_input );

                        /* Read custom meta data */
                        read_custom_meta_data( p_demux, p_state, t+2, t+4,
                                               p_input );

                        /* Append item to playlist */
                        input_ItemAddSubItem( p_current_input, p_input );

                        while( i_options > 0 )
                            free( ppsz_options[--i_options] );
                        free( ppsz_options );
                    }
                    else
                    {
                        msg_Warn( p_demux,
                                 "Playlist item's path should be a string" );
                    }
                    lua_pop( p_state, 1 ); /* pop "path" */
                }
                else
                {
                    msg_Warn( p_demux, "Playlist item should be a table" );
                }
                lua_pop( p_state, 1 ); /* pop the value, keep the key for
                                        * the next lua_next() call */
            }
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
