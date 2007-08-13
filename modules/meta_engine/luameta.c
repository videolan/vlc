/*****************************************************************************
 * googleimage.c
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc_input.h>
#include <vlc_playlist.h>
#include <vlc_meta.h>
#include <vlc_stream.h>
#include <vlc_charset.h>

#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif

#include <lua.h>        /* Low level lua C API */
#include <lauxlib.h>    /* Higher level C API */
#include <lualib.h>     /* Lua libs */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int FindArt( vlc_object_t * );
static int FindMeta( vlc_object_t *p_this );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    set_shortname( N_( "Lua Meta" ) );
    set_description( _("Fetch Artwork using lua scripts") );
    set_capability( "meta fetcher", 10 );
    set_callbacks( FindMeta, NULL );
    add_submodule();
        set_capability( "art finder", 10 );
        set_callbacks( FindArt, NULL );
vlc_module_end();

/*****************************************************************************
 * Lua function bridge
 *****************************************************************************/
static vlc_object_t * vlclua_get_this( lua_State *p_state )
{
    vlc_object_t * p_this;
    lua_getglobal( p_state, "vlc" );
    lua_getfield( p_state, lua_gettop( p_state ), "private" );
    p_this = (vlc_object_t*)lua_topointer( p_state, lua_gettop( p_state ) );
    lua_pop( p_state, 2 );
    return p_this;
}

static int vlclua_stream_new( lua_State *p_state )
{
    vlc_object_t * p_this = vlclua_get_this( p_state );
    int i = lua_gettop( p_state );
    stream_t * p_stream;
    const char * psz_url;
    if( !i ) return 0;
    psz_url = lua_tostring( p_state, 1 );
    lua_pop( p_state, i );
    p_stream = stream_UrlNew( p_this, psz_url );
    lua_pushlightuserdata( p_state, p_stream );
    return 1;
}

static int vlclua_stream_read( lua_State *p_state )
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

static int vlclua_stream_readline( lua_State *p_state )
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

static int vlclua_stream_delete( lua_State *p_state )
{
    int i = lua_gettop( p_state );
    stream_t * p_stream;
    if( !i ) return 0;
    p_stream = (stream_t *)lua_topointer( p_state, 1 );
    lua_pop( p_state, i );
    stream_Delete( p_stream );
    return 1;
}

static int vlclua_msg_dbg( lua_State *p_state )
{
    vlc_object_t *p_this = vlclua_get_this( p_state );
    int i = lua_gettop( p_state );
    if( !i ) return 0;
    const char *psz_cstring = lua_tostring( p_state, 1 );
    if( !psz_cstring ) return 0;
    msg_Dbg( p_this, "%s", psz_cstring );
    return 0;
}
static int vlclua_msg_warn( lua_State *p_state )
{
    vlc_object_t *p_this = vlclua_get_this( p_state );
    int i = lua_gettop( p_state );
    if( !i ) return 0;
    const char *psz_cstring = lua_tostring( p_state, 1 );
    if( !psz_cstring ) return 0;
    msg_Warn( p_this, "%s", psz_cstring );
    return 0;
}
static int vlclua_msg_err( lua_State *p_state )
{
    vlc_object_t *p_this = vlclua_get_this( p_state );
    int i = lua_gettop( p_state );
    if( !i ) return 0;
    const char *psz_cstring = lua_tostring( p_state, 1 );
    if( !psz_cstring ) return 0;
    msg_Err( p_this, "%s", psz_cstring );
    return 0;
}
static int vlclua_msg_info( lua_State *p_state )
{
    vlc_object_t *p_this = vlclua_get_this( p_state );
    int i = lua_gettop( p_state );
    if( !i ) return 0;
    const char *psz_cstring = lua_tostring( p_state, 1 );
    if( !psz_cstring ) return 0;
    msg_Info( p_this, "%s", psz_cstring );
    return 0;
}

/* Functions to register */
static luaL_Reg p_reg[] =
{
    { "stream_new", vlclua_stream_new },
    { "stream_read", vlclua_stream_read },
    { "stream_readline", vlclua_stream_readline },
    { "stream_delete", vlclua_stream_delete },
    { "msg_dbg", vlclua_msg_dbg },
    { "msg_warn", vlclua_msg_warn },
    { "msg_err", vlclua_msg_err },
    { "msg_info", vlclua_msg_info },
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
 * Init lua
 *****************************************************************************/
static lua_State * vlclua_meta_init( vlc_object_t *p_this, input_item_t * p_item )
{
    lua_State * p_state = luaL_newstate();
    if( !p_state )
    {
        msg_Err( p_this, "Could not create new Lua State" );
        return NULL;
    }

    /* Load Lua libraries */
    luaL_openlibs( p_state ); /* XXX: Don't open all the libs? */
    
    luaL_register( p_state, "vlc", p_reg );
    
    lua_pushlightuserdata( p_state, p_this );
    lua_setfield( p_state, lua_gettop( p_state ) - 1, "private" );
    
    lua_pushstring( p_state, p_item->psz_name );
    lua_setfield( p_state, lua_gettop( p_state ) - 1, "name" );
    
    lua_pushstring( p_state, p_item->p_meta->psz_title );
    lua_setfield( p_state, lua_gettop( p_state ) - 1, "title" );
    
    lua_pushstring( p_state, p_item->p_meta->psz_album );
    lua_setfield( p_state, lua_gettop( p_state ) - 1, "album" );

    lua_pushstring( p_state, p_item->p_meta->psz_arturl );
    lua_setfield( p_state, lua_gettop( p_state ) - 1, "arturl" );
    /* XXX: all should be passed */

    return p_state;
}

/*****************************************************************************
 * Will execute func on all scripts in luadirname, and stop if func returns
 * success.
 *****************************************************************************/
static int vlclua_scripts_batch_execute( vlc_object_t *p_this,
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

    char  *ppsz_dir_list[] = { NULL, NULL, NULL };
    char **ppsz_dir;

    ppsz_dir_list[0] = malloc( strlen( p_this->p_libvlc->psz_homedir )
                             + strlen( "/"CONFIG_DIR"/" ) + strlen( luadirname ) + 1 );
    sprintf( ppsz_dir_list[0], "%s/"CONFIG_DIR"/%s",
             p_this->p_libvlc->psz_homedir, luadirname );

#   if defined(__APPLE__) || defined(SYS_BEOS) || defined(WIN32)
    {
        const char *psz_vlcpath = config_GetDataDir( p_this );
        ppsz_dir_list[1] = malloc( strlen( psz_vlcpath ) + strlen( luadirname ) + 1 );
        if( !ppsz_dir_list[1] ) return VLC_ENOMEM;
        sprintf( ppsz_dir_list[1], "%s/%s", psz_vlcpath, luadirname );
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

        msg_Dbg( p_this, "Trying Lua scripts in %s", *ppsz_dir );
        dir = utf8_opendir( *ppsz_dir );

        if( !dir ) continue;
        i_files = utf8_loaddir( dir, &ppsz_filelist, file_select, file_compare );
        if( i_files < 1 ) continue;
        ppsz_fileend = ppsz_filelist + i_files;

        for( ppsz_file = ppsz_filelist; ppsz_file < ppsz_fileend; ppsz_file++ )
        {
            char  *psz_filename;
            asprintf( &psz_filename, "%s/%s", *ppsz_dir, *ppsz_file );
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
static inline void read_meta_data( vlc_object_t *p_this,
                                   lua_State *p_state, int o, int t,
                                   input_item_t *p_input )
{
    const char *psz_value;
#define TRY_META( a, b )                                    \
    lua_getfield( p_state, o, a );                          \
    if( lua_isstring( p_state, t ) )                        \
    {                                                       \
        psz_value = lua_tostring( p_state, t );             \
        msg_Dbg( p_this, #b ": %s", psz_value );           \
        vlc_meta_Set ## b ( p_input->p_meta, psz_value );   \
    }                                                       \
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
}

static inline void read_custom_meta_data( vlc_object_t *p_this,
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

/*****************************************************************************
 * Called through lua_scripts_batch_execute to call 'fetch_art' on the script
 * pointed by psz_filename.
 *****************************************************************************/
static int fetch_art( vlc_object_t *p_this, const char * psz_filename,
                      lua_State * p_state, void * user_data )
{
    int i_ret = VLC_EGENERIC;
    input_item_t * p_input = user_data;
    int s;

    /* Ugly hack to delete previous versions of the fetchart()
    * functions. */
    lua_pushnil( p_state );
    lua_setglobal( p_state, "fetch_art" );
    
    /* Load and run the script(s) */
    if( luaL_dofile( p_state, psz_filename ) )
    {
        msg_Warn( p_this, "Error loading script %s: %s", psz_filename,
                  lua_tostring( p_state, lua_gettop( p_state ) ) );
        lua_pop( p_state, 1 );
        return VLC_EGENERIC;
    }

    lua_getglobal( p_state, "fetch_art" );

    if( !lua_isfunction( p_state, lua_gettop( p_state ) ) )
    {
        msg_Warn( p_this, "Error while runing script %s, "
                  "function fetch_art() not found", psz_filename );
        lua_pop( p_state, 1 );
        return VLC_EGENERIC;
    }

    if( lua_pcall( p_state, 0, 1, 0 ) )
    {
        msg_Warn( p_this, "Error while runing script %s, "
                  "function fetch_art(): %s", psz_filename,
                  lua_tostring( p_state, lua_gettop( p_state ) ) );
        lua_pop( p_state, 1 );
        return VLC_EGENERIC;
    }

    if((s = lua_gettop( p_state )))
    {
        const char * psz_value;

        if( lua_isstring( p_state, s ) )
        {
            psz_value = lua_tostring( p_state, s );
            if( psz_value && *psz_value != 0 )
            {
                msg_Dbg( p_this, "setting arturl: %s", psz_value );
                vlc_meta_SetArtURL ( p_input->p_meta, psz_value );
                i_ret = VLC_SUCCESS;
            }
        }
        else
        {
            msg_Err( p_this, "Lua playlist script %s: "
                 "didn't return a string", psz_filename );
        }
    }
    else
    {
        msg_Err( p_this, "Script went completely foobar" );
    }

    return i_ret;
}

/*****************************************************************************
 * Called through lua_scripts_batch_execute to call 'fetch_meta' on the script
 * pointed by psz_filename.
 *****************************************************************************/
static int fetch_meta( vlc_object_t *p_this, const char * psz_filename,
                       lua_State * p_state, void * user_data )
{
    input_item_t * p_input = user_data;
    int t;

    /* Ugly hack to delete previous versions of the fetchmeta()
    * functions. */
    lua_pushnil( p_state );
    lua_setglobal( p_state, "fetch_meta" );
    
    /* Load and run the script(s) */
    if( luaL_dofile( p_state, psz_filename ) )
    {
        msg_Warn( p_this, "Error loading script %s: %s", psz_filename,
                  lua_tostring( p_state, lua_gettop( p_state ) ) );
        lua_pop( p_state, 1 );
        return VLC_EGENERIC;
    }
    
    lua_getglobal( p_state, "fetch_meta" );
    
    if( !lua_isfunction( p_state, lua_gettop( p_state ) ) )
    {
        msg_Warn( p_this, "Error while runing script %s, "
                  "function fetch_meta() not found", psz_filename );
        lua_pop( p_state, 1 );
        return VLC_EGENERIC;
    }
    
    if( lua_pcall( p_state, 0, 1, 0 ) )
    {
        msg_Warn( p_this, "Error while runing script %s, "
                  "function fetch_meta(): %s", psz_filename,
                  lua_tostring( p_state, lua_gettop( p_state ) ) );
        lua_pop( p_state, 1 );
        return VLC_EGENERIC;
    }
    

    if((t = lua_gettop( p_state )))
    {
        if( lua_istable( p_state, t ) )
        {
            read_meta_data( p_this, p_state, t, t+1, p_input );
            read_custom_meta_data( p_this, p_state, t, t+1, p_input );
        }
        else
        {
            msg_Err( p_this, "Lua playlist script %s: "
                 "didn't return a table", psz_filename );
        }
    }
    else
    {
        msg_Err( p_this, "Script went completely foobar" );
    }

    /* We tell the batch thing to continue, hence all script
     * will get the change to add its meta. This behaviour could
     * be changed. */
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Module entry point for meta.
 *****************************************************************************/
static int FindMeta( vlc_object_t *p_this )
{
    meta_engine_t *p_me = (meta_engine_t *)p_this;
    input_item_t *p_item = p_me->p_item;
    lua_State *p_state = vlclua_meta_init( p_this, p_item );
    
    int i_ret = vlclua_scripts_batch_execute( p_this, "luameta", &fetch_meta, p_state, p_item );
    lua_close( p_state );
    return i_ret;
}

/*****************************************************************************
 * Module entry point for art.
 *****************************************************************************/
static int FindArt( vlc_object_t *p_this )
{
    playlist_t *p_playlist = (playlist_t *)p_this;
    input_item_t *p_item = (input_item_t *)(p_playlist->p_private);
    lua_State *p_state = vlclua_meta_init( p_this, p_item );
    
    int i_ret = vlclua_scripts_batch_execute( p_this, "luameta", &fetch_art, p_state, p_item );
    lua_close( p_state );
    return i_ret;
}

