/*****************************************************************************
 * meta.c: Get meta/artwork using lua scripts
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
#ifndef  _GNU_SOURCE
#   define  _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_input.h>
#include <vlc_playlist.h>
#include <vlc_meta.h>
#include <vlc_url.h>
#include <vlc_strings.h>
#include <vlc_stream.h>
#include <vlc_charset.h>

#include "vlc.h"
#include "libs.h"


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int fetch_art( vlc_object_t *p_this, const char * psz_filename,
                      lua_State * L, void * user_data );
static lua_State *vlclua_meta_init( vlc_object_t *p_this,
                                    input_item_t * p_item );


/*****************************************************************************
 * Init lua
 *****************************************************************************/
static const luaL_Reg p_reg[] = { { NULL, NULL } };

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

    luaopen_msg( L );
    luaopen_stream( L );
    luaopen_strings( L );
    luaopen_variables( L );
    luaopen_object( L );
    luaopen_misc( L );

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
 * Module entry point for art.
 *****************************************************************************/
int FindArt( vlc_object_t *p_this )
{
    playlist_t *p_playlist = pl_Hold( p_this );
    if( !p_playlist )
        return VLC_EGENERIC;

    input_item_t *p_item = (input_item_t *)p_this->p_private;
    lua_State *L = vlclua_meta_init( p_this, p_item );
    int i_ret = vlclua_scripts_batch_execute( p_this, "meta", &fetch_art, L, p_item );
    lua_close( L );

    pl_Release( p_this );
    return i_ret;
}

