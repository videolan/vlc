/*****************************************************************************
 * vlc.c: Generic lua interface functions
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <sys/stat.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_meta.h>
#include <vlc_charset.h>
#include <vlc_fs.h>
#include <vlc_services_discovery.h>
#include <vlc_stream.h>

#include "vlc.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define INTF_TEXT N_("Lua interface")
#define INTF_LONGTEXT N_("Lua interface module to load")

#define CONFIG_TEXT N_("Lua interface configuration")
#define CONFIG_LONGTEXT N_("Lua interface configuration string. Format is: '[\"<interface module name>\"] = { <option> = <value>, ...}, ...'.")
#define PASS_TEXT N_( "Password" )
#define PASS_LONGTEXT N_( "A single password restricts access " \
    "to this interface." )
#define SRC_TEXT N_( "Source directory" )
#define SRC_LONGTEXT N_( "Source directory" )
#define INDEX_TEXT N_( "Directory index" )
#define INDEX_LONGTEXT N_( "Allow to build directory index" )

#define TELNETHOST_TEXT N_( "Host" )
#define TELNETHOST_LONGTEXT N_( "This is the host on which the " \
    "interface will listen. It defaults to all network interfaces (0.0.0.0)." \
    " If you want this interface to be available only on the local " \
    "machine, enter \"127.0.0.1\"." )
#define TELNETPORT_TEXT N_( "Port" )
#define TELNETPORT_LONGTEXT N_( "This is the TCP port on which this " \
    "interface will listen. It defaults to 4212." )
#define TELNETPWD_TEXT N_( "Password" )
#define TELNETPWD_LONGTEXT N_( "A single password restricts access " \
    "to this interface." )
#define RCHOST_TEXT N_("TCP command input")
#define RCHOST_LONGTEXT N_("Accept commands over a socket rather than stdin. " \
            "You can set the address and port the interface will bind to." )
#define CLIHOST_TEXT N_("CLI input")
#define CLIHOST_LONGTEXT N_( "Accept commands from this source. " \
    "The CLI defaults to stdin (\"*console\"), but can also bind to a " \
    "plain TCP socket (\"localhost:4212\") or use the telnet protocol " \
    "(\"telnet://0.0.0.0:4212\")" )

static int vlc_sd_probe_Open( vlc_object_t * );

vlc_module_begin ()
        set_shortname( N_("Lua") )
        set_description( N_("Lua interpreter") )
        set_category( CAT_INTERFACE )
        set_subcategory( SUBCAT_INTERFACE_MAIN )

        add_string( "lua-intf", "dummy", INTF_TEXT, INTF_LONGTEXT, false )
        add_string( "lua-config", "", CONFIG_TEXT, CONFIG_LONGTEXT, false )
        set_capability( "interface", 0 )
        set_callbacks( Open_LuaIntf, Close_LuaIntf )
        add_shortcut( "luaintf" )

    add_submodule ()
        set_section( N_("Lua HTTP"), 0 )
            add_password ( "http-password", NULL, PASS_TEXT, PASS_LONGTEXT, false )
            add_string ( "http-src",  NULL, SRC_TEXT,  SRC_LONGTEXT,  true )
            add_bool   ( "http-index", false, INDEX_TEXT, INDEX_LONGTEXT, true )
        set_capability( "interface", 0 )
        set_callbacks( Open_LuaHTTP, Close_LuaIntf )
        add_shortcut( "luahttp", "http" )
        set_description( N_("Lua HTTP") )

    add_submodule ()
        set_section( N_("Lua CLI"), 0 )
            add_string( "rc-host", NULL, RCHOST_TEXT, RCHOST_LONGTEXT, true )
            add_string( "cli-host", NULL, CLIHOST_TEXT, CLIHOST_LONGTEXT, true )
        set_capability( "interface", 25 )
        set_description( N_("Command-line interface") )
        set_callbacks( Open_LuaCLI, Close_LuaIntf )
#ifndef _WIN32
        add_shortcut( "luacli", "luarc", "cli", "rc" )
#else
        add_shortcut( "luacli", "luarc" )
#endif

    add_submodule ()
        set_section( N_("Lua Telnet"), 0 )
            add_string( "telnet-host", "localhost", TELNETHOST_TEXT,
                        TELNETHOST_LONGTEXT, true )
            add_integer( "telnet-port", TELNETPORT_DEFAULT, TELNETPORT_TEXT,
                         TELNETPORT_LONGTEXT, true )
                change_integer_range( 1, 65535 )
            add_password( "telnet-password", NULL, TELNETPWD_TEXT,

                          TELNETPWD_LONGTEXT, true )
        set_capability( "interface", 0 )
        set_callbacks( Open_LuaTelnet, Close_LuaIntf )
        set_description( N_("Lua Telnet") )
        add_shortcut( "luatelnet", "telnet" )

    add_submodule ()
        set_shortname( N_( "Lua Meta Fetcher" ) )
        set_description( N_("Fetch meta data using lua scripts") )
        set_capability( "meta fetcher", 10 )
        set_callbacks( FetchMeta, NULL )

    add_submodule ()
        set_shortname( N_( "Lua Meta Reader" ) )
        set_description( N_("Read meta data using lua scripts") )
        set_capability( "meta reader", 10 )
        set_callbacks( ReadMeta, NULL )

    add_submodule ()
        add_shortcut( "luaplaylist" )
        set_shortname( N_("Lua Playlist") )
        set_description( N_("Lua Playlist Parser Interface") )
        set_capability( "demux", 2 )
        set_callbacks( Import_LuaPlaylist, Close_LuaPlaylist )

    add_submodule ()
        set_shortname( N_( "Lua Art" ) )
        set_description( N_("Fetch artwork using lua scripts") )
        set_capability( "art finder", 10 )
        set_callbacks( FindArt, NULL )

    add_submodule ()
        set_shortname( N_("Lua Extension") )
        set_description( N_("Lua Extension") )
        add_shortcut( "luaextension" )
        set_capability( "extension", 1 )
        set_callbacks( Open_Extension, Close_Extension )

    add_submodule ()
        set_description( N_("Lua SD Module") )
        add_shortcut( "luasd" )
        set_capability( "services_discovery", 0 )
        add_string( "lua-sd", "", NULL, NULL, false )
            change_volatile()
        add_string( "lua-longname", "", NULL, NULL, false )
            change_volatile()
        set_callbacks( Open_LuaSD, Close_LuaSD )

    VLC_SD_PROBE_SUBMODULE

vlc_module_end ()

/*****************************************************************************
 *
 *****************************************************************************/
static const char *ppsz_lua_exts[] = { ".luac", ".lua", ".vle", NULL };
static int file_select( const char *file )
{
    int i = strlen( file );
    int j;
    for( j = 0; ppsz_lua_exts[j]; j++ )
    {
        int l = strlen( ppsz_lua_exts[j] );
        if( i >= l && !strcmp( file+i-l, ppsz_lua_exts[j] ) )
            return 1;
    }
    return 0;
}

static int file_compare( const char **a, const char **b )
{
    return strcmp( *a, *b );
}

int vlclua_dir_list( const char *luadirname, char ***pppsz_dir_list )
{
#define MAX_DIR_LIST_SIZE 5
    *pppsz_dir_list = malloc(MAX_DIR_LIST_SIZE*sizeof(char *));
    if (!*pppsz_dir_list)
        return VLC_EGENERIC;
    char **ppsz_dir_list = *pppsz_dir_list;

    int i = 0;
    char *datadir = config_GetUserDir( VLC_DATA_DIR );

    if( likely(datadir != NULL)
     && likely(asprintf( &ppsz_dir_list[i], "%s"DIR_SEP"lua"DIR_SEP"%s",
                         datadir, luadirname ) != -1) )
        i++;
    free( datadir );

#if !(defined(__APPLE__) || defined(_WIN32) || defined(__OS2__))
    char *psz_libpath = config_GetLibDir();
    if( likely(psz_libpath != NULL) )
    {
        if( likely(asprintf( &ppsz_dir_list[i], "%s"DIR_SEP"lua"DIR_SEP"%s",
                             psz_libpath, luadirname ) != -1) )
            i++;
        free( psz_libpath );
    }
#endif

    char *psz_datapath = config_GetDataDir();
    if( likely(psz_datapath != NULL) )
    {
        if( likely(asprintf( &ppsz_dir_list[i], "%s"DIR_SEP"lua"DIR_SEP"%s",
                              psz_datapath, luadirname ) != -1) )
            i++;

#if defined(__APPLE__)
        if( likely(asprintf( &ppsz_dir_list[i],
                             "%s"DIR_SEP"share"DIR_SEP"lua"DIR_SEP"%s",
                             psz_datapath, luadirname ) != -1) )
            i++;
#endif
        free( psz_datapath );
    }

    ppsz_dir_list[i] = NULL;

    assert( i < MAX_DIR_LIST_SIZE);

    return VLC_SUCCESS;
}

void vlclua_dir_list_free( char **ppsz_dir_list )
{
    for( char **ppsz_dir = ppsz_dir_list; *ppsz_dir; ppsz_dir++ )
        free( *ppsz_dir );
    free( ppsz_dir_list );
}

/*****************************************************************************
 * Will execute func on all scripts in luadirname, and stop if func returns
 * success.
 *****************************************************************************/
int vlclua_scripts_batch_execute( vlc_object_t *p_this,
                                  const char * luadirname,
                                  int (*func)(vlc_object_t *, const char *, void *),
                                  void * user_data)
{
    char **ppsz_dir_list = NULL;
    int i_ret;

    if( (i_ret = vlclua_dir_list( luadirname, &ppsz_dir_list )) != VLC_SUCCESS)
        return i_ret;

    i_ret = VLC_EGENERIC;
    for( char **ppsz_dir = ppsz_dir_list; *ppsz_dir; ppsz_dir++ )
    {
        char **ppsz_filelist;

        msg_Dbg( p_this, "Trying Lua scripts in %s", *ppsz_dir );
        int i_files = vlc_scandir( *ppsz_dir, &ppsz_filelist, file_select,
                                   file_compare );
        if( i_files < 0 )
            continue;

        char **ppsz_file = ppsz_filelist;
        char **ppsz_fileend = ppsz_filelist + i_files;

        while( ppsz_file < ppsz_fileend )
        {
            char *psz_filename;

            if( asprintf( &psz_filename,
                          "%s" DIR_SEP "%s", *ppsz_dir, *ppsz_file ) == -1 )
                psz_filename = NULL;
            free( *(ppsz_file++) );

            if( likely(psz_filename != NULL) )
            {
                msg_Dbg( p_this, "Trying Lua playlist script %s",
                         psz_filename );
                i_ret = func( p_this, psz_filename, user_data );
                free( psz_filename );
                if( i_ret == VLC_SUCCESS )
                    break;
            }
        }

        while( ppsz_file < ppsz_fileend )
            free( *(ppsz_file++) );
        free( ppsz_filelist );

        if( i_ret == VLC_SUCCESS )
            break;
    }
    vlclua_dir_list_free( ppsz_dir_list );
    return i_ret;
}

char *vlclua_find_file( const char *psz_luadirname, const char *psz_name )
{
    char **ppsz_dir_list = NULL;
    vlclua_dir_list( psz_luadirname, &ppsz_dir_list );

    for( char **ppsz_dir = ppsz_dir_list; *ppsz_dir; ppsz_dir++ )
    {
        for( const char **ppsz_ext = ppsz_lua_exts; *ppsz_ext; ppsz_ext++ )
        {
            char *psz_filename;
            struct stat st;

            if( asprintf( &psz_filename, "%s"DIR_SEP"%s%s", *ppsz_dir,
                          psz_name, *ppsz_ext ) < 0 )
            {
                vlclua_dir_list_free( ppsz_dir_list );
                return NULL;
            }

            if( vlc_stat( psz_filename, &st ) == 0
                && S_ISREG( st.st_mode ) )
            {
                vlclua_dir_list_free( ppsz_dir_list );
                return psz_filename;
            }
            free( psz_filename );
        }
    }
    vlclua_dir_list_free( ppsz_dir_list );
    return NULL;
}

/*****************************************************************************
 * Meta data setters utility.
 * Playlist item table should be on top of the stack when these are called
 *****************************************************************************/
#undef vlclua_read_meta_data
void vlclua_read_meta_data( vlc_object_t *p_this, lua_State *L,
                            input_item_t *p_input )
{
#define TRY_META( a, b )                                        \
    lua_getfield( L, -1, a );                                   \
    if( lua_isstring( L, -1 ) &&                                \
        strcmp( lua_tostring( L, -1 ), "" ) )                   \
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

#undef vlclua_read_custom_meta_data
void vlclua_read_custom_meta_data( vlc_object_t *p_this, lua_State *L,
                                     input_item_t *p_input )
{
    /* Lock the input item and create the meta table if needed */
    vlc_mutex_lock( &p_input->lock );

    if( !p_input->p_meta )
        p_input->p_meta = vlc_meta_New();

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
            if( !lua_isstring( L, -2 ) || !lua_isstring( L, -1 ) )
            {
                msg_Err( p_this, "'meta' keys and values must be strings");
                lua_pop( L, 1 ); /* pop "value" */
                continue;
            }
            const char *psz_key = lua_tostring( L, -2 );
            const char *psz_value = lua_tostring( L, -1 );

            vlc_meta_AddExtra( p_input->p_meta, psz_key, psz_value );

            lua_pop( L, 1 ); /* pop "value" */
        }
    }
    lua_pop( L, 1 ); /* pop "meta" */
    /* ... item -> back to original stack */

    vlc_mutex_unlock( &p_input->lock );
}

/*****************************************************************************
 * Playlist utilities
 ****************************************************************************/
/**
 * Playlist item table should be on top of the stack when this is called
 */
#undef vlclua_read_options
void vlclua_read_options( vlc_object_t *p_this, lua_State *L,
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

#undef vlclua_playlist_add_internal
int vlclua_playlist_add_internal( vlc_object_t *p_this, lua_State *L,
                                    playlist_t *p_playlist,
                                    input_item_t *p_parent, bool b_play )
{
    int i_count = 0;
    input_item_node_t *p_parent_node = NULL;

    assert( p_parent || p_playlist );

    /* playlist */
    if( lua_istable( L, -1 ) )
    {
        if( p_parent ) p_parent_node = input_item_node_Create( p_parent );
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
                    char         *psz_oldurl   = NULL;
                    const char   *psz_path     = NULL;
                    char         *psz_u8path   = NULL;
                    const char   *psz_name     = NULL;
                    char        **ppsz_options = NULL;
                    int           i_options    = 0;
                    mtime_t       i_duration   = -1;
                    input_item_t *p_input;

                    /* Read path and name */
                    if (p_parent) {
                        psz_oldurl = input_item_GetURI( p_parent );
                        msg_Dbg( p_this, "old path: %s", psz_oldurl );
                    }
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
                        psz_name = NULL;
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
                    p_input = input_item_NewExt( psz_path, psz_name, i_options,
                                                (const char **)ppsz_options,
                                                VLC_INPUT_OPTION_TRUSTED,
                                                i_duration );
                    lua_pop( L, 3 ); /* pop "path name item" */
                    /* playlist key item */

                    /* Read meta data: item must be on top of stack */
                    vlclua_read_meta_data( p_this, L, p_input );

                    /* copy the original URL to the meta data, if "URL" is still empty */
                    char* url = input_item_GetURL( p_input );
                    if( url == NULL && p_parent)
                    {
                        EnsureUTF8( psz_oldurl );
                        msg_Dbg( p_this, "meta-URL: %s", psz_oldurl );
                        input_item_SetURL ( p_input, psz_oldurl );
                    }
                    free( psz_oldurl );
                    free( url );

                    /* copy the psz_name to the meta data, if "Title" is still empty */
                    char* title = input_item_GetTitle( p_input );
                    if( title == NULL )
                        input_item_SetTitle ( p_input, psz_name );
                    free( title );

                    /* Read custom meta data: item must be on top of stack*/
                    vlclua_read_custom_meta_data( p_this, L, p_input );

                    /* Append item to playlist */
                    if( p_parent ) /* Add to node */
                    {
                        input_item_CopyOptions( p_parent, p_input );
                        input_item_node_AppendItem( p_parent_node, p_input );
                    }
                    else /* Play or Enqueue (preparse) */
                        /* FIXME: playlist_AddInput() can fail */
                        playlist_AddInput( p_playlist, p_input,
                               PLAYLIST_APPEND |
                               ( b_play ? PLAYLIST_GO : PLAYLIST_PREPARSE ),
                               PLAYLIST_END, true, false );
                    i_count ++; /* increment counter */
                    vlc_gc_decref( p_input );
                    while( i_options > 0 )
                        free( ppsz_options[--i_options] );
                    free( ppsz_options );
                    free( psz_u8path );
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
        if( p_parent )
        {
            if( i_count ) input_item_node_PostAndDelete( p_parent_node );
            else input_item_node_Delete( p_parent_node );
        }
    }
    else
    {
        msg_Warn( p_this, "Playlist should be a table." );
    }
    return i_count;
}

static int vlc_sd_probe_Open( vlc_object_t *obj )
{
    vlc_probe_t *probe = (vlc_probe_t *)obj;
    char **ppsz_filelist = NULL;
    char **ppsz_fileend  = NULL;
    char **ppsz_file;
    char *psz_name;
    char **ppsz_dir_list = NULL;
    char **ppsz_dir;
    lua_State *L = NULL;
    vlclua_dir_list( "sd", &ppsz_dir_list );
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
        i_files = vlc_scandir( *ppsz_dir, &ppsz_filelist, file_select,
                                file_compare );
        if( i_files < 1 ) continue;
        ppsz_fileend = ppsz_filelist + i_files;
        for( ppsz_file = ppsz_filelist; ppsz_file < ppsz_fileend; ppsz_file++ )
        {
            char  *psz_filename;
            if( asprintf( &psz_filename,
                          "%s" DIR_SEP "%s", *ppsz_dir, *ppsz_file ) < 0 )
            {
                goto error;
            }
            L = luaL_newstate();
            if( !L )
            {
                msg_Err( probe, "Could not create new Lua State" );
                free( psz_filename );
                goto error;
            }
            luaL_openlibs( L );
            if( vlclua_add_modules_path( L, psz_filename ) )
            {
                msg_Err( probe, "Error while setting the module search path for %s",
                          psz_filename );
                free( psz_filename );
                goto error;
            }
            if( luaL_dofile( L, psz_filename ) )
            {

                msg_Err( probe, "Error loading script %s: %s", psz_filename,
                          lua_tostring( L, lua_gettop( L ) ) );
                lua_pop( L, 1 );
                free( psz_filename );
                lua_close( L );
                continue;
            }
            char *psz_longname;
            char *temp = strchr( *ppsz_file, '.' );
            if( temp )
                *temp = '\0';
            lua_getglobal( L, "descriptor" );
            if( !lua_isfunction( L, lua_gettop( L ) ) || lua_pcall( L, 0, 1, 0 ) )
            {
                msg_Warn( probe, "No 'descriptor' function in '%s'", psz_filename );
                lua_pop( L, 1 );
                if( !( psz_longname = strdup( *ppsz_file ) ) )
                {
                    free( psz_filename );
                    goto error;
                }
            }
            else
            {
                lua_getfield( L, -1, "title" );
                if( !lua_isstring( L, -1 ) ||
                    !( psz_longname = strdup( lua_tostring( L, -1 ) ) ) )
                {
                    free( psz_filename );
                    goto error;
                }
            }

            char *psz_file_esc = config_StringEscape( *ppsz_file );
            char *psz_longname_esc = config_StringEscape( psz_longname );
            if( asprintf( &psz_name, "lua{sd='%s',longname='%s'}",
                          psz_file_esc, psz_longname_esc ) < 0 )
            {
                free( psz_file_esc );
                free( psz_longname_esc );
                free( psz_filename );
                free( psz_longname );
                goto error;
            }
            free( psz_file_esc );
            free( psz_longname_esc );
            vlc_sd_probe_Add( probe, psz_name, psz_longname, SD_CAT_INTERNET );
            free( psz_name );
            free( psz_longname );
            free( psz_filename );
            lua_close( L );
        }
    }
    if( ppsz_filelist )
    {
        for( ppsz_file = ppsz_filelist; ppsz_file < ppsz_fileend;
             ppsz_file++ )
            free( *ppsz_file );
        free( ppsz_filelist );
    }
    vlclua_dir_list_free( ppsz_dir_list );
    return VLC_PROBE_CONTINUE;
error:
    if( ppsz_filelist )
    {
        for( ppsz_file = ppsz_filelist; ppsz_file < ppsz_fileend;
             ppsz_file++ )
            free( *ppsz_file );
        free( ppsz_filelist );
    }
    if( L )
        lua_close( L );
    vlclua_dir_list_free( ppsz_dir_list );
    return VLC_ENOMEM;
}

static int vlclua_add_modules_path_inner( lua_State *L, const char *psz_path )
{
    int count = 0;
    for( const char **ppsz_ext = ppsz_lua_exts; *ppsz_ext; ppsz_ext++ )
    {
        lua_pushfstring( L, "%s"DIR_SEP"modules"DIR_SEP"?%s;",
                         psz_path, *ppsz_ext );
        count ++;
    }

    return count;
}

int vlclua_add_modules_path( lua_State *L, const char *psz_filename )
{
    /* Setup the module search path:
     *   * "The script's directory"/modules
     *   * "The script's parent directory"/modules
     *   * and so on for all the next directories in the directory list
     */
    char *psz_path = strdup( psz_filename );
    if( !psz_path )
        return 1;

    char *psz_char = strrchr( psz_path, DIR_SEP_CHAR );
    if( !psz_char )
    {
        free( psz_path );
        return 1;
    }
    *psz_char = '\0';

    /* psz_path now holds the file's directory */
    psz_char = strrchr( psz_path, DIR_SEP_CHAR );
    if( !psz_char )
    {
        free( psz_path );
        return 1;
    }
    *psz_char = '\0';

    /* Push package on stack */
    int count = 0;
    lua_getglobal( L, "package" );

    /* psz_path now holds the file's parent directory */
    count += vlclua_add_modules_path_inner( L, psz_path );
    *psz_char = DIR_SEP_CHAR;

    /* psz_path now holds the file's directory */
    count += vlclua_add_modules_path_inner( L, psz_path );

    char **ppsz_dir_list = NULL;
    vlclua_dir_list( psz_char+1/* gruik? */, &ppsz_dir_list );
    char **ppsz_dir = ppsz_dir_list;

    for( ; *ppsz_dir && strcmp( *ppsz_dir, psz_path ); ppsz_dir++ );
    free( psz_path );

    for( ; *ppsz_dir; ppsz_dir++ )
    {
        psz_path = *ppsz_dir;
        /* FIXME: doesn't work well with meta/... modules due to the double
         * directory depth */
        psz_char = strrchr( psz_path, DIR_SEP_CHAR );
        if( !psz_char )
        {
            vlclua_dir_list_free( ppsz_dir_list );
            return 1;
        }

        *psz_char = '\0';
        count += vlclua_add_modules_path_inner( L, psz_path );
        *psz_char = DIR_SEP_CHAR;
        count += vlclua_add_modules_path_inner( L, psz_path );
    }

    lua_getfield( L, -(count+1), "path" ); /* Get package.path */
    lua_concat( L, count+1 ); /* Concat vlc module paths and package.path */
    lua_setfield( L, -2, "path"); /* Set package.path */
    lua_pop( L, 1 ); /* Pop the package module */

    vlclua_dir_list_free( ppsz_dir_list );
    return 0;
}

/** Replacement for luaL_dofile, using VLC's input capabilities */
int vlclua_dofile( vlc_object_t *p_this, lua_State *L, const char *uri )
{
    if( !strstr( uri, "://" ) )
        return luaL_dofile( L, uri );
    if( !strncasecmp( uri, "file://", 7 ) )
        return luaL_dofile( L, uri + 7 );
    stream_t *s = stream_UrlNew( p_this, uri );
    if( !s )
    {
        return 1;
    }
    int64_t i_size = stream_Size( s );
    char *p_buffer = ( i_size > 0 ) ? malloc( i_size ) : NULL;
    if( !p_buffer )
    {
        // FIXME: read the whole stream until we reach the end (if no size)
        stream_Delete( s );
        return 1;
    }
    int64_t i_read = stream_Read( s, p_buffer, (int) i_size );
    int i_ret = ( i_read == i_size ) ? 0 : 1;
    if( !i_ret )
        i_ret = luaL_loadbuffer( L, p_buffer, (size_t) i_size, uri );
    if( !i_ret )
        i_ret = lua_pcall( L, 0, LUA_MULTRET, 0 );
    stream_Delete( s );
    free( p_buffer );
    return i_ret;
}
