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
#include <vlc_demux.h>
#include <vlc_art_finder.h>
#include <vlc_url.h>
#include <vlc_strings.h>
#include <vlc_stream.h>

#include "vlc.h"
#include "libs.h"

/*****************************************************************************
 * Init lua
 *****************************************************************************/
static const luaL_Reg p_reg[] = { { NULL, NULL } };

static lua_State * init( vlc_object_t *p_this, input_item_t * p_item )
{
    lua_State * L = luaL_newstate();
    if( !L )
    {
        msg_Err( p_this, "Could not create new Lua State" );
        return NULL;
    }

    /* Load Lua libraries */
    luaL_openlibs( L ); /* XXX: Don't open all the libs? */

    luaL_register( L, "vlc", p_reg );

    luaopen_msg( L );
    luaopen_stream( L );
    luaopen_strings( L );
    luaopen_variables( L );
    luaopen_object( L );
    luaopen_misc( L );
    luaopen_input_item( L, p_item );

    lua_pushlightuserdata( L, p_this );
    lua_setfield( L, -2, "private" );

    return L;
}

/*****************************************************************************
 * Run a lua entry point function
 *****************************************************************************/
static int run( vlc_object_t *p_this, const char * psz_filename,
                lua_State * L, const char *luafunction )
{
    /* Ugly hack to delete previous versions of the fetchart()
     * functions. */
    lua_pushnil( L );
    lua_setglobal( L, luafunction );

    /* Load and run the script(s) */
    if( luaL_dofile( L, psz_filename ) )
    {
        msg_Warn( p_this, "Error loading script %s: %s", psz_filename,
                 lua_tostring( L, lua_gettop( L ) ) );
        goto error;
    }

    lua_getglobal( L, luafunction );

    if( !lua_isfunction( L, lua_gettop( L ) ) )
    {
        msg_Warn( p_this, "Error while runing script %s, "
                 "function %s() not found", psz_filename, luafunction );
        goto error;
    }

    if( lua_pcall( L, 0, 1, 0 ) )
    {
        msg_Warn( p_this, "Error while runing script %s, "
                 "function %s(): %s", psz_filename, luafunction,
                 lua_tostring( L, lua_gettop( L ) ) );
        goto error;
    }
    return VLC_SUCCESS;

error:
    lua_pop( L, 1 );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Called through lua_scripts_batch_execute to call 'fetch_art' on the script
 * pointed by psz_filename.
 *****************************************************************************/
static int fetch_art( vlc_object_t *p_this, const char * psz_filename,
                      void * user_data )
{
    input_item_t * p_item = user_data;
    int s;

    lua_State *L = init( p_this, p_item );

    int i_ret = run(p_this, psz_filename, L, "fetch_art");
    if(i_ret != VLC_SUCCESS)
    {
        lua_close( L );
        return i_ret;
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
                input_item_SetArtURL ( p_item, psz_value );
                lua_close( L );
                return VLC_SUCCESS;
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

    lua_close( L );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Called through lua_scripts_batch_execute to call 'fetch_art' on the script
 * pointed by psz_filename.
 *****************************************************************************/
static int read_meta( vlc_object_t *p_this, const char * psz_filename,
                      void * user_data )
{
    input_item_t * p_item = user_data;
    lua_State *L = init( p_this, p_item );

    int i_ret = run(p_this, psz_filename, L, "read_meta");
    if(i_ret != VLC_SUCCESS)
    {
        lua_close( L );
        return i_ret;
    }

    // Continue, all "meta reader" are always run.
    lua_close( L );
    return 1;
}


/*****************************************************************************
 * Called through lua_scripts_batch_execute to call 'fetch_meta' on the script
 * pointed by psz_filename.
 *****************************************************************************/
static int fetch_meta( vlc_object_t *p_this, const char * psz_filename,
                       void * user_data )
{
    input_item_t * p_item = user_data;
    lua_State *L = init( p_this, p_item );

    int ret = run(p_this, psz_filename, L, "fetch_meta");
    lua_close( L );

    return ret;
}

/*****************************************************************************
 * Read meta.
 *****************************************************************************/

int ReadMeta( vlc_object_t *p_this )
{
    demux_meta_t *p_demux_meta = (demux_meta_t *)p_this;
    input_item_t *p_item = p_demux_meta->p_item;

    int i_ret = vlclua_scripts_batch_execute( p_this, "meta/reader", &read_meta, p_item );

    return i_ret;
}


/*****************************************************************************
 * Read meta.
 *****************************************************************************/

int FetchMeta( vlc_object_t *p_this )
{
    demux_meta_t *p_demux_meta = (demux_meta_t *)p_this;
    input_item_t *p_item = p_demux_meta->p_item;

    int i_ret = vlclua_scripts_batch_execute( p_this, "meta/fetcher", &fetch_meta, p_item );

    return i_ret;
}


/*****************************************************************************
 * Module entry point for art.
 *****************************************************************************/
int FindArt( vlc_object_t *p_this )
{
    art_finder_t *p_finder = (art_finder_t *)p_this;
    input_item_t *p_item = p_finder->p_item;

    int i_ret = vlclua_scripts_batch_execute( p_this, "meta/art", &fetch_art, p_item );

    return i_ret;
}

