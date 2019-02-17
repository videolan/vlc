/*****************************************************************************
 * meta.c: Get meta/artwork using lua scripts
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
#ifndef  _GNU_SOURCE
#   define  _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "vlc.h"
#include "libs.h"

/*****************************************************************************
 * Init lua
 *****************************************************************************/
static const luaL_Reg p_reg[] = { { NULL, NULL } };

static lua_State * init( vlc_object_t *p_this, input_item_t * p_item, const char *psz_filename )
{
    lua_State * L = luaL_newstate();
    if( !L )
    {
        msg_Err( p_this, "Could not create new Lua State" );
        return NULL;
    }

    vlclua_set_this( L, p_this );

    /* Load Lua libraries */
    luaL_openlibs( L ); /* XXX: Don't open all the libs? */

    luaL_register_namespace( L, "vlc", p_reg );

    luaopen_msg( L );
    luaopen_stream( L );
    luaopen_strings( L );
    luaopen_variables( L );
    luaopen_object( L );
    luaopen_xml( L );
    luaopen_input_item( L, p_item );

    if( vlclua_add_modules_path( L, psz_filename ) )
    {
        msg_Warn( p_this, "Error while setting the module search path for %s",
                  psz_filename );
        lua_close( L );
        return NULL;
    }

    return L;
}

/*****************************************************************************
 * Run a lua entry point function
 *****************************************************************************/
static int run( vlc_object_t *p_this, const char * psz_filename,
                lua_State * L, const char *luafunction,
                const luabatch_context_t *p_context )
{
    /* Ugly hack to delete previous versions of the fetchart()
     * functions. */
    lua_pushnil( L );
    lua_setglobal( L, luafunction );

    /* Load and run the script(s) */
    if( vlclua_dofile( p_this, L, psz_filename ) )
    {
        msg_Warn( p_this, "Error loading script %s: %s", psz_filename,
                 lua_tostring( L, lua_gettop( L ) ) );
        goto error;
    }


    meta_fetcher_scope_t e_scope = FETCHER_SCOPE_NETWORK; /* default to restricted one */
    lua_getglobal( L, "descriptor" );
    if( lua_isfunction( L, lua_gettop( L ) ) && !lua_pcall( L, 0, 1, 0 ) )
    {
        lua_getfield( L, -1, "scope" );
        char *psz_scope = luaL_strdupornull( L, -1 );
        if ( psz_scope && !strcmp( psz_scope, "local" ) )
            e_scope = FETCHER_SCOPE_LOCAL;
        free( psz_scope );
        lua_pop( L, 1 );
    }
    lua_pop( L, 1 );

    if ( p_context && p_context->pf_validator && !p_context->pf_validator( p_context, e_scope ) )
    {
        msg_Dbg( p_this, "skipping script (unmatched scope) %s", psz_filename );
        goto error;
    }

    lua_getglobal( L, luafunction );

    if( !lua_isfunction( L, lua_gettop( L ) ) )
    {
        msg_Warn( p_this, "Error while running script %s, "
                 "function %s() not found", psz_filename, luafunction );
        goto error;
    }

    if( lua_pcall( L, 0, 1, 0 ) )
    {
        msg_Warn( p_this, "Error while running script %s, "
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
static bool validate_scope( const luabatch_context_t *p_context, meta_fetcher_scope_t e_scope )
{
    if ( p_context->e_scope == FETCHER_SCOPE_ANY )
        return true;
    else
        return ( p_context->e_scope == e_scope );
}

static int fetch_art( vlc_object_t *p_this, const char * psz_filename,
                      const luabatch_context_t *p_context )
{
    lua_State *L = init( p_this, p_context->p_item, psz_filename );
    if( !L )
        return VLC_EGENERIC;

    int i_ret = run(p_this, psz_filename, L, "fetch_art", p_context);
    if(i_ret != VLC_SUCCESS)
    {
        lua_close( L );
        return i_ret;
    }

    if(lua_gettop( L ))
    {
        const char * psz_value;

        if( lua_isstring( L, -1 ) )
        {
            psz_value = lua_tostring( L, -1 );
            if( psz_value && *psz_value != 0 )
            {
                lua_Dbg( p_this, "setting arturl: %s", psz_value );
                input_item_SetArtURL ( p_context->p_item, psz_value );
                lua_close( L );
                return VLC_SUCCESS;
            }
        }
        else if( !lua_isnoneornil( L, -1 ) )
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
 * Called through lua_scripts_batch_execute to call 'read_meta' on the script
 * pointed by psz_filename.
 *****************************************************************************/
static int read_meta( vlc_object_t *p_this, const char * psz_filename,
                      const luabatch_context_t *p_context )
{
    /* FIXME: merge with finder */
    demux_meta_t *p_demux_meta = (demux_meta_t *)p_this;
    VLC_UNUSED( p_context );

    lua_State *L = init( p_this, p_demux_meta->p_item, psz_filename );
    if( !L )
        return VLC_EGENERIC;

    int i_ret = run(p_this, psz_filename, L, "read_meta", NULL);
    lua_close( L );

    // Continue even if an error occurred: all "meta reader" are always run.
    return i_ret == VLC_SUCCESS ? VLC_EGENERIC : i_ret;
}


/*****************************************************************************
 * Called through lua_scripts_batch_execute to call 'fetch_meta' on the script
 * pointed by psz_filename.
 *****************************************************************************/
static int fetch_meta( vlc_object_t *p_this, const char * psz_filename,
                       const luabatch_context_t *p_context )
{
    lua_State *L = init( p_this, p_context->p_item, psz_filename );
    if( !L )
        return VLC_EGENERIC;

    int ret = run(p_this, psz_filename, L, "fetch_meta", p_context);
    lua_close( L );

    return ret;
}

/*****************************************************************************
 * Read meta.
 *****************************************************************************/

int ReadMeta( demux_meta_t *p_this )
{
    if( lua_Disabled( p_this ) )
        return VLC_EGENERIC;

    return vlclua_scripts_batch_execute( VLC_OBJECT(p_this), "meta"DIR_SEP"reader",
                                         (void*) &read_meta, NULL );
}


/*****************************************************************************
 * Read meta.
 *****************************************************************************/

int FetchMeta( meta_fetcher_t *p_finder )
{
    if( lua_Disabled( p_finder ) )
        return VLC_EGENERIC;

    luabatch_context_t context = { p_finder->p_item, p_finder->e_scope, validate_scope };

    return vlclua_scripts_batch_execute( VLC_OBJECT(p_finder), "meta"DIR_SEP"fetcher",
                                         &fetch_meta, (void*)&context );
}


/*****************************************************************************
 * Module entry point for art.
 *****************************************************************************/
int FindArt( meta_fetcher_t *p_finder )
{
    if( lua_Disabled( p_finder ) )
        return VLC_EGENERIC;

    luabatch_context_t context = { p_finder->p_item, p_finder->e_scope, validate_scope };

    return vlclua_scripts_batch_execute( VLC_OBJECT(p_finder), "meta"DIR_SEP"art",
                                         &fetch_art, (void*)&context );
}

