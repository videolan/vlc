/*****************************************************************************
 * meta.c: Get meta/artwork using lua scripts
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include <vlc_input.h>
#include <vlc_playlist.h>
#include <vlc_meta.h>
#include <vlc_url.h>
#include <vlc_strings.h>
#include <vlc_stream.h>
#include <vlc_charset.h>

#include "vlc.h"


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int fetch_meta( vlc_object_t *p_this, const char * psz_filename,
                       lua_State * L, void * user_data );
static int fetch_art( vlc_object_t *p_this, const char * psz_filename,
                      lua_State * L, void * user_data );
static lua_State *vlclua_meta_init( vlc_object_t *p_this,
                                    input_item_t * p_item );


/*****************************************************************************
 * Lua function bridge
 *****************************************************************************/
/* Functions to register */
static luaL_Reg p_reg[] =
{
    /* TODO: make an object out of the stream stuff, so we can do something
     * like:
     *
     * s = vlc.stream_new( "http://www.videolan.org" )
     * page = s:read( 2^16 )
     * s:delete()
     *
     * instead of (which could be problematic since we don't check if
     * s is really a stream object):
     *
     * s = vlc.stream_new( "http://www.videolan.org" )
     * page = vlc.stream_read( s, 2^16 )
     * vlc.stream_delete( s )
     */
    { "stream_new", vlclua_stream_new },
    { "stream_read", vlclua_stream_read },
    { "stream_readline", vlclua_stream_readline },
    { "stream_delete", vlclua_stream_delete },
    { "decode_uri", vlclua_decode_uri },
    { "resolve_xml_special_chars", vlclua_resolve_xml_special_chars },
    { "msg_dbg", vlclua_msg_dbg },
    { "msg_warn", vlclua_msg_warn },
    { "msg_err", vlclua_msg_err },
    { "msg_info", vlclua_msg_info },
    { NULL, NULL }
};

/*****************************************************************************
 * Init lua
 *****************************************************************************/
static lua_State * vlclua_meta_init( vlc_object_t *p_this, input_item_t * p_item )
{
    lua_State * L = luaL_newstate();
    if( !L )
    {
        msg_Err( p_this, "Could not create new Lua State" );
        return NULL;
    }
    char *psz_meta;

    /* Load Lua libraries */
    luaL_openlibs( L ); /* XXX: Don't open all the libs? */

    luaL_register( L, "vlc", p_reg );

    lua_pushlightuserdata( L, p_this );
    lua_setfield( L, -2, "private" );

    psz_meta = input_item_GetName( p_item );
    lua_pushstring( L, psz_meta );
    lua_setfield( L, -2, "name" );
    free( psz_meta );

    psz_meta = input_item_GetArtist( p_item );
    lua_pushstring( L, psz_meta );
    lua_setfield( L, -2, "artist" );
    free( psz_meta );

    psz_meta = input_item_GetTitle( p_item ) ;
    lua_pushstring( L, psz_meta );
    lua_setfield( L, -2, "title" );
    free( psz_meta );

    psz_meta = input_item_GetAlbum( p_item );
    lua_pushstring( L, psz_meta );
    lua_setfield( L, -2, "album" );
    free( psz_meta );

    psz_meta = input_item_GetArtURL( p_item );
    lua_pushstring( L, psz_meta );
    lua_setfield( L, -2, "arturl" );
    free( psz_meta );
    /* XXX: all should be passed ( could use macro ) */

    return L;
}

/*****************************************************************************
 * Called through lua_scripts_batch_execute to call 'fetch_art' on the script
 * pointed by psz_filename.
 *****************************************************************************/
static int fetch_art( vlc_object_t *p_this, const char * psz_filename,
                      lua_State * L, void * user_data )
{
    int i_ret = VLC_EGENERIC;
    input_item_t * p_input = user_data;
    int s;

    /* Ugly hack to delete previous versions of the fetchart()
    * functions. */
    lua_pushnil( L );
    lua_setglobal( L, "fetch_art" );

    /* Load and run the script(s) */
    if( luaL_dofile( L, psz_filename ) )
    {
        msg_Warn( p_this, "Error loading script %s: %s", psz_filename,
                  lua_tostring( L, lua_gettop( L ) ) );
        lua_pop( L, 1 );
        return VLC_EGENERIC;
    }

    lua_getglobal( L, "fetch_art" );

    if( !lua_isfunction( L, lua_gettop( L ) ) )
    {
        msg_Warn( p_this, "Error while runing script %s, "
                  "function fetch_art() not found", psz_filename );
        lua_pop( L, 1 );
        return VLC_EGENERIC;
    }

    if( lua_pcall( L, 0, 1, 0 ) )
    {
        msg_Warn( p_this, "Error while runing script %s, "
                  "function fetch_art(): %s", psz_filename,
                  lua_tostring( L, lua_gettop( L ) ) );
        lua_pop( L, 1 );
        return VLC_EGENERIC;
    }

    if((s = lua_gettop( L )))
    {
        const char * psz_value;

        if( lua_isstring( L, s ) )
        {
            psz_value = lua_tostring( L, s );
            if( psz_value && *psz_value != 0 )
            {
                lua_Dbg( p_this, "setting arturl: %s", psz_value );
                input_item_SetArtURL ( p_input, psz_value );
                i_ret = VLC_SUCCESS;
            }
        }
        else if( !lua_isnil( L, s ) )
        {
            msg_Err( p_this, "Lua art fetcher script %s: "
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
                       lua_State * L, void * user_data )
{
    input_item_t * p_input = user_data;

    /* In lua, setting a variable to nil is equivalent to deleting it */
    lua_pushnil( L );
    lua_setglobal( L, "fetch_meta" );

    /* Load and run the script(s) */
    if( luaL_dofile( L, psz_filename ) )
    {
        msg_Warn( p_this, "Error loading script %s: %s", psz_filename,
                  lua_tostring( L, lua_gettop( L ) ) );
        lua_pop( L, 1 );
        return VLC_EGENERIC;
    }

    lua_getglobal( L, "fetch_meta" );

    if( !lua_isfunction( L, lua_gettop( L ) ) )
    {
        msg_Warn( p_this, "Error while runing script %s, "
                  "function fetch_meta() not found", psz_filename );
        lua_pop( L, 1 );
        return VLC_EGENERIC;
    }

    if( lua_pcall( L, 0, 1, 0 ) )
    {
        msg_Warn( p_this, "Error while runing script %s, "
                  "function fetch_meta(): %s", psz_filename,
                  lua_tostring( L, lua_gettop( L ) ) );
        lua_pop( L, 1 );
        return VLC_EGENERIC;
    }


    if( lua_gettop( L ) )
    {
        if( lua_istable( L, -1 ) )
        {
            /* ... item */
            vlclua_read_meta_data( p_this, L, p_input );
            vlclua_read_custom_meta_data( p_this, L, p_input );
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
int E_(FindMeta)( vlc_object_t *p_this )
{
    meta_engine_t *p_me = (meta_engine_t *)p_this;
    input_item_t *p_item = p_me->p_item;
    lua_State *L = vlclua_meta_init( p_this, p_item );

    int i_ret = vlclua_scripts_batch_execute( p_this, "meta", &fetch_meta, L, p_item );
    lua_close( L );
    return i_ret;
}

/*****************************************************************************
 * Module entry point for art.
 *****************************************************************************/
int E_(FindArt)( vlc_object_t *p_this )
{
    playlist_t *p_playlist = (playlist_t *)p_this;
    input_item_t *p_item = (input_item_t *)(p_playlist->p_private);
    lua_State *L = vlclua_meta_init( p_this, p_item );

    int i_ret = vlclua_scripts_batch_execute( p_this, "meta", &fetch_art, L, p_item );
    lua_close( L );
    return i_ret;
}

