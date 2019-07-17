/*****************************************************************************
 * vlc.c: Generic lua interface functions
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
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

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS

#include "vlc.h"

#include <vlc_plugin.h>
#include <vlc_arrays.h>
#include <vlc_charset.h>
#include <vlc_fs.h>
#include <vlc_services_discovery.h>
#include <vlc_stream.h>

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
            add_password("http-password", NULL, PASS_TEXT, PASS_LONGTEXT)
            add_string ( "http-src",  NULL, SRC_TEXT,  SRC_LONGTEXT,  true )
            add_bool   ( "http-index", false, INDEX_TEXT, INDEX_LONGTEXT, true )
        set_capability( "interface", 0 )
        set_callbacks( Open_LuaHTTP, Close_LuaIntf )
        add_shortcut( "luahttp", "http" )
        set_description( N_("Lua HTTP") )

    add_submodule ()
        set_section( N_("Lua Telnet"), 0 )
            add_string( "telnet-host", "localhost", TELNETHOST_TEXT,
                        TELNETHOST_LONGTEXT, true )
            add_integer( "telnet-port", TELNETPORT_DEFAULT, TELNETPORT_TEXT,
                         TELNETPORT_LONGTEXT, true )
                change_integer_range( 1, 65535 )
            add_password("telnet-password", NULL, TELNETPWD_TEXT,
                         TELNETPWD_LONGTEXT)
        set_capability( "interface", 0 )
        set_callbacks( Open_LuaTelnet, Close_LuaIntf )
        set_description( N_("Lua Telnet") )
        add_shortcut( "luatelnet", "telnet" )

    add_submodule ()
        set_shortname( N_( "Lua Meta Fetcher" ) )
        set_description( N_("Fetch meta data using lua scripts") )
        set_capability( "meta fetcher", 10 )
        set_callback( FetchMeta )

    add_submodule ()
        set_shortname( N_( "Lua Meta Reader" ) )
        set_description( N_("Read meta data using lua scripts") )
        set_capability( "meta reader", 10 )
        set_callback( ReadMeta )

    add_submodule ()
        add_shortcut( "luaplaylist" )
        set_shortname( N_("Lua Playlist") )
        set_description( N_("Lua Playlist Parser Interface") )
        set_capability( "stream_filter", 302 )
        set_callbacks( Import_LuaPlaylist, Close_LuaPlaylist )

    add_submodule ()
        set_shortname( N_( "Lua Art" ) )
        set_description( N_("Fetch artwork using lua scripts") )
        set_capability( "art finder", 10 )
        set_callback( FindArt )

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

static char **vlclua_dir_list_append( char **restrict list, char *basedir,
                                      const char *luadirname )
{
    if (unlikely(basedir == NULL))
        return list;

    if (likely(asprintf(list, "%s"DIR_SEP"lua"DIR_SEP"%s",
                        basedir, luadirname) != -1))
        list++;

    free(basedir);
    return list;
}

int vlclua_dir_list(const char *luadirname, char ***restrict listp)
{
    char **list = malloc(4 * sizeof(char *));
    if (unlikely(list == NULL))
        return VLC_EGENERIC;

    *listp = list;

    /* Lua scripts in user-specific data directory */
    list = vlclua_dir_list_append(list, config_GetUserDir(VLC_USERDATA_DIR),
                                  luadirname);

    char *libdir = config_GetSysPath(VLC_PKG_LIBEXEC_DIR, NULL);
    char *datadir = config_GetSysPath(VLC_PKG_DATA_DIR, NULL);
    bool both = libdir != NULL && datadir != NULL && strcmp(libdir, datadir);

    /* Tokenized Lua scripts in architecture-specific data directory */
    list = vlclua_dir_list_append(list, libdir, luadirname);

    /* Source Lua Scripts in architecture-independent data directory */
    if (both || libdir == NULL)
        list = vlclua_dir_list_append(list, datadir, luadirname);
    else
        free(datadir);

    *list = NULL;
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
                                  int (*func)(vlc_object_t *, const char *, const luabatch_context_t *),
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
                msg_Dbg( p_this, "Trying Lua playlist script %s", psz_filename );
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
    TRY_META( "language",  Language );
    TRY_META( "nowplaying", NowPlaying );
    TRY_META( "publisher",  Publisher );
    TRY_META( "encodedby",  EncodedBy );
    TRY_META( "arturl",     ArtURL );
    TRY_META( "trackid",    TrackID );
    TRY_META( "director",   Director );
    TRY_META( "season",     Season );
    TRY_META( "episode",    Episode );
    TRY_META( "show_name",  ShowName );
    TRY_META( "actors",     Actors );
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
                TAB_APPEND( *pi_options, *pppsz_options, psz_option );
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

input_item_t *vlclua_read_input_item(vlc_object_t *obj, lua_State *L)
{
    if (!lua_istable(L, -1))
    {
        msg_Warn(obj, "Playlist item should be a table" );
        return NULL;
    }

    lua_getfield(L, -1, "path");

    /* playlist key item path */
    if (!lua_isstring(L, -1))
    {
        lua_pop(L, 1); /* pop "path" */
        msg_Warn(obj, "Playlist item's path should be a string");
        return NULL;
    }

    /* Read path and name */
    const char *path = lua_tostring(L, -1);
    msg_Dbg(obj, "Path: %s", path);

    const char *name = NULL;
    lua_getfield(L, -2, "name");
    if (lua_isstring(L, -1))
    {
        name = lua_tostring(L, -1);
        msg_Dbg(obj, "Name: %s", name);
    }
    else if (!lua_isnil(L, -1))
        msg_Warn(obj, "Playlist item name should be a string" );

    /* Read duration */
    vlc_tick_t duration = INPUT_DURATION_INDEFINITE;

    lua_getfield( L, -3, "duration" );
    if (lua_isnumber(L, -1))
        duration = vlc_tick_from_sec( lua_tonumber(L, -1) );
    else if (!lua_isnil(L, -1))
        msg_Warn(obj, "Playlist item duration should be a number (seconds)");
    lua_pop( L, 1 ); /* pop "duration" */

    /* Read options: item must be on top of stack */
    char **optv = NULL;
    int optc = 0;

    lua_pushvalue(L, -3);
    vlclua_read_options(obj, L, &optc, &optv);

    /* Create input item */
    input_item_t *item = input_item_NewExt(path, name, duration,
                                           ITEM_TYPE_UNKNOWN,
                                           ITEM_NET_UNKNOWN);
    if (unlikely(item == NULL))
        goto out;

    input_item_AddOptions(item, optc, (const char **)optv,
                          VLC_INPUT_OPTION_TRUSTED);
    lua_pop(L, 3); /* pop "path name item" */
    /* playlist key item */

    /* Read meta data: item must be on top of stack */
    vlclua_read_meta_data(obj, L, item);

    /* copy the psz_name to the meta data, if "Title" is still empty */
    char* title = input_item_GetTitle(item);
    if (title == NULL)
        input_item_SetTitle(item, name);
    free(title);

    /* Read custom meta data: item must be on top of stack*/
    vlclua_read_custom_meta_data(obj, L, item);

out:
    while (optc > 0)
           free(optv[--optc]);
    free(optv);
    return item;
}

static int vlc_sd_probe_Open( vlc_object_t *obj )
{
    if( lua_Disabled( obj ) )
        return VLC_EGENERIC;

    vlc_dictionary_t name_d;

    char **ppsz_dir_list;
    if( vlclua_dir_list( "sd", &ppsz_dir_list ) )
        return VLC_ENOMEM;

    vlc_dictionary_init( &name_d, 32 );
    for( char **ppsz_dir = ppsz_dir_list; *ppsz_dir; ppsz_dir++ )
    {
        char **ppsz_filelist;
        int i_files = vlc_scandir( *ppsz_dir, &ppsz_filelist, file_select,
                                    file_compare );
        if( i_files < 1 ) continue;

        for( char **ppsz_file = ppsz_filelist;
             ppsz_file < ppsz_filelist + i_files; ppsz_file++ )
        {
            char *temp = strchr( *ppsz_file, '.' );
            if( temp )
                *temp = '\0';

            if( !vlc_dictionary_has_key( &name_d, *ppsz_file ) )
                vlc_dictionary_insert( &name_d, *ppsz_file, &name_d );
            free( *ppsz_file );
        }
        free( ppsz_filelist );
    }
    vlclua_dir_list_free( ppsz_dir_list );

    int r = VLC_PROBE_CONTINUE;
    char **names = vlc_dictionary_all_keys( &name_d );
    if( names != NULL )
    {
        for( char **name = names; *name; ++name )
        {
            r = vlclua_probe_sd( obj, *name );
            if( r != VLC_PROBE_CONTINUE )
                break;
        }

        for( char **name = names; *name; ++name )
            free( *name );

        free( names );
    }
    vlc_dictionary_clear( &name_d, NULL, NULL );

    return r;
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
int vlclua_dofile( vlc_object_t *p_this, lua_State *L, const char *curi )
{
    char *uri = ToLocaleDup( curi );
    if( !strstr( uri, "://" ) ) {
        int ret = luaL_dofile( L, uri );
        free( uri );
        return ret;
    }
    if( !strncasecmp( uri, "file://", 7 ) ) {
        int ret = luaL_dofile( L, uri + 7 );
        free( uri );
        return ret;
    }
    stream_t *s = vlc_stream_NewURL( p_this, uri );
    if( !s )
    {
        free( uri );
        return 1;
    }
    int64_t i_size = stream_Size( s );
    char *p_buffer = ( i_size > 0 ) ? malloc( i_size ) : NULL;
    if( !p_buffer )
    {
        // FIXME: read the whole stream until we reach the end (if no size)
        vlc_stream_Delete( s );
        free( uri );
        return 1;
    }
    int64_t i_read = vlc_stream_Read( s, p_buffer, (int) i_size );
    int i_ret = ( i_read == i_size ) ? 0 : 1;
    if( !i_ret )
        i_ret = luaL_loadbuffer( L, p_buffer, (size_t) i_size, uri );
    if( !i_ret )
        i_ret = lua_pcall( L, 0, LUA_MULTRET, 0 );
    vlc_stream_Delete( s );
    free( p_buffer );
    free( uri );
    return i_ret;
}
