/*****************************************************************************
 * sd.c: Services discovery related functions
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
 *
 * Authors: Antoine Cellerier <dionoea at videolan tod org>
 *          Fabio Ritrovato <sephiroth87 at videolan dot org>
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

#include <assert.h>
#include <math.h>
#include <vlc_common.h>
#include <vlc_services_discovery.h>
#include <vlc_charset.h>
#include <vlc_strings.h>
#include <vlc_hash.h>

#include "../vlc.h"
#include "../libs.h"

static int vlclua_sd_delete_common( input_item_t **pp_item )
{
    assert(pp_item != NULL);

    input_item_t *p_item = *pp_item;
    if (p_item != NULL) /* item may be NULL if already removed earlier */
        input_item_Release( p_item );

    return 1;
}

static int vlclua_sd_remove_common( lua_State *L, input_item_t **pp_item )
{
    services_discovery_t *p_sd = (services_discovery_t *)vlclua_get_this( L );

    if (pp_item == NULL)
        return luaL_error( L, "expected item" );

    input_item_t *p_item = *pp_item;
    if (*pp_item == NULL)
        return luaL_error( L, "already removed item" );

    services_discovery_RemoveItem( p_sd, p_item );
    input_item_Release( p_item );
    /* Make sure we won't try to remove it again */
    *pp_item = NULL;
    return 1;
}


/*** Input item ***/

static int vlclua_sd_item_delete( lua_State *L )
{
    input_item_t **pp_item = luaL_checkudata( L, 1, "input_item_t" );

    return vlclua_sd_delete_common( pp_item );
}

#define vlclua_item_luareg( a ) \
{ "set_" # a, vlclua_item_set_ ## a },

#define vlclua_item_meta( lowercase, normal ) \
static int vlclua_item_set_ ## lowercase ( lua_State *L )\
{\
    services_discovery_t *p_sd = (services_discovery_t *)vlclua_get_this( L );\
    input_item_t **pp_node = (input_item_t **)luaL_checkudata( L, 1, "input_item_t" );\
    if( *pp_node )\
    {\
        if( lua_isstring( L, -1 ) )\
        {\
            input_item_Set ## normal ( *pp_node, lua_tostring( L, -1 ) );\
        } else\
            msg_Err( p_sd, "Error parsing set_ " # lowercase " arguments" );\
    }\
    return 1;\
}

vlclua_item_meta(title, Title)
vlclua_item_meta(artist, Artist)
vlclua_item_meta(genre, Genre)
vlclua_item_meta(copyright, Copyright)
vlclua_item_meta(album, Album)
vlclua_item_meta(tracknum, TrackNum)
vlclua_item_meta(description, Description)
vlclua_item_meta(rating, Rating)
vlclua_item_meta(date, Date)
vlclua_item_meta(setting, Setting)
vlclua_item_meta(url, URL)
vlclua_item_meta(language, Language)
vlclua_item_meta(nowplaying, NowPlaying)
vlclua_item_meta(publisher, Publisher)
vlclua_item_meta(encodedby, EncodedBy)
vlclua_item_meta(arturl, ArtworkURL)
vlclua_item_meta(trackid, TrackID)
vlclua_item_meta(tracktotal, TrackTotal)
vlclua_item_meta(director, Director)
vlclua_item_meta(season, Season)
vlclua_item_meta(episode, Episode)
vlclua_item_meta(showname, ShowName)
vlclua_item_meta(actors, Actors)

static const luaL_Reg vlclua_item_reg[] = {
    vlclua_item_luareg(title)
    vlclua_item_luareg(artist)
    vlclua_item_luareg(genre)
    vlclua_item_luareg(copyright)
    vlclua_item_luareg(album)
    vlclua_item_luareg(tracknum)
    vlclua_item_luareg(description)
    vlclua_item_luareg(rating)
    vlclua_item_luareg(date)
    vlclua_item_luareg(setting)
    vlclua_item_luareg(url)
    vlclua_item_luareg(language)
    vlclua_item_luareg(nowplaying)
    vlclua_item_luareg(publisher)
    vlclua_item_luareg(encodedby)
    vlclua_item_luareg(arturl)
    vlclua_item_luareg(trackid)
    vlclua_item_luareg(tracktotal)
    vlclua_item_luareg(director)
    vlclua_item_luareg(season)
    vlclua_item_luareg(episode)
    vlclua_item_luareg(showname)
    vlclua_item_luareg(actors)
    { NULL, NULL }
};

static input_item_t *vlclua_sd_create_item( services_discovery_t *p_sd,
                                            lua_State *L )
{
    if( !lua_istable( L, -1 ) )
    {
        msg_Err( p_sd, "Error: argument must be table" );
        return NULL;
    }

    lua_getfield( L, -1, "path" );
    if( !lua_isstring( L, -1 ) )
    {
        msg_Err( p_sd, "Error: \"%s\" parameter is required", "path" );
        return NULL;
    }

    const char *psz_path = lua_tostring( L, -1 );

    lua_getfield( L, -2, "title" );

    const char *psz_title = luaL_checkstring( L, -1 )
       ? luaL_checkstring( L, -1 )
       : psz_path;

    input_item_t *p_input = input_item_New( psz_path, psz_title );
    lua_pop( L, 2 );

    if( unlikely(p_input == NULL) )
        return NULL;

    /* The table must be at the top of the stack when calling
     * vlclua_read_options() */
    char **ppsz_options = NULL;
    int i_options = 0;

    lua_pushvalue( L, -1 );
    vlclua_read_options( p_sd, L, &i_options, &ppsz_options );
    lua_pop( L, 1 );

    input_item_AddOptions( p_input, i_options, (const char **)ppsz_options,
                           VLC_INPUT_OPTION_TRUSTED );
    while( i_options > 0 )
        free( ppsz_options[--i_options] );
    free( ppsz_options );

    vlclua_read_meta_data( p_sd, L, p_input );
    /* This one is to be tested... */
    vlclua_read_custom_meta_data( p_sd, L, p_input );
    /* The duration is given in seconds, convert to microseconds */

    lua_getfield( L, -1, "duration" );
    if( lua_isnumber( L, -1 ) )
        p_input->i_duration = llround(lua_tonumber( L, -1 ) * 1e6);
    else if( !lua_isnil( L, -1 ) )
        msg_Warn( p_sd, "Item duration should be a number (in seconds)." );
    lua_pop( L, 1 );

    /* string to build the input item uid */
    lua_getfield( L, -1, "uiddata" );
    if( lua_isstring( L, -1 ) )
    {
        char *s = strdup( luaL_checkstring( L, -1 ) );
        if ( s )
        {
            vlc_hash_md5_t md5;
            vlc_hash_md5_Init( &md5 );
            vlc_hash_md5_Update( &md5, s, strlen( s ) );
            free( s );
            char tmp[VLC_HASH_MD5_DIGEST_HEX_SIZE];
            vlc_hash_FinishHex( &md5, tmp );
            input_item_AddInfo( p_input, "uid", "md5", "%s", tmp );
        }
    }
    lua_pop( L, 1 );

    input_item_t **udata = lua_newuserdata( L, sizeof( input_item_t * ) );
    *udata = p_input;

    if( luaL_newmetatable( L, "input_item_t" ) )
    {
        lua_newtable( L );
        luaL_register( L, NULL, vlclua_item_reg );
        lua_setfield( L, -2, "__index" );
        lua_pushcfunction( L, vlclua_sd_item_delete );
        lua_setfield( L, -2, "__gc" );
        lua_pushliteral( L, "none of your business" );
        lua_setfield( L, -2, "__metatable" );
    }
    lua_setmetatable( L, -2 );

    return p_input;
}


/*** Input item tree node ***/

static int vlclua_sd_node_delete( lua_State *L )
{
    input_item_t **pp_item = luaL_checkudata( L, 1, "node" );

    return vlclua_sd_delete_common( pp_item );
}

static int vlclua_sd_add_sub_common( services_discovery_t *p_sd,
                                     input_item_t **pp_node,
                                     input_item_t *p_input )
{
    if( *pp_node != NULL && p_input != NULL )
        services_discovery_AddSubItem( p_sd, *pp_node, p_input );
    return 1;
}

static int vlclua_node_add_subitem( lua_State *L )
{
    services_discovery_t *p_sd = (services_discovery_t *)vlclua_get_this( L );
    input_item_t **pp_node = (input_item_t **)luaL_checkudata( L, 1, "node" );

    return vlclua_sd_add_sub_common( p_sd, pp_node,
                                     vlclua_sd_create_item( p_sd, L ) );
}

static const luaL_Reg vlclua_node_reg[];

static input_item_t *vlclua_sd_create_node( services_discovery_t *p_sd,
                                            lua_State *L )
{
    if( !lua_istable( L, -1 ) )
    {
        msg_Err( p_sd, "Error: argument must be table" );
        return NULL;
    }

    lua_getfield( L, -1, "title" );
    if( !lua_isstring( L, -1 ) )
    {
        msg_Err( p_sd, "Error: \"%s\" parameter is required", "title" );
        return NULL;
    }

    const char *psz_name = lua_tostring( L, -1 );
    input_item_t *p_input = input_item_NewExt( INPUT_ITEM_URI_NOP, psz_name,
                                               INPUT_DURATION_INDEFINITE,
                                               ITEM_TYPE_NODE,
                                               ITEM_NET_UNKNOWN );
    lua_pop( L, 1 );

    if( unlikely(p_input == NULL) )
        return NULL;

    lua_getfield( L, -1, "arturl" );
    if( lua_isstring( L, -1 ) && strcmp( lua_tostring( L, -1 ), "" ) )
    {
        char *psz_value = strdup( lua_tostring( L, -1 ) );
        EnsureUTF8( psz_value );
        msg_Dbg( p_sd, "ArtURL: %s", psz_value );
        /* TODO: ask for art download if not local file */
        input_item_SetArtURL( p_input, psz_value );
        free( psz_value );
    }
    lua_pop( L, 1 );

    input_item_t **udata = lua_newuserdata( L, sizeof( input_item_t * ) );
    *udata = p_input;
    if( luaL_newmetatable( L, "node" ) )
    {
        lua_newtable( L );
        luaL_register( L, NULL, vlclua_node_reg );
        lua_setfield( L, -2, "__index" );
        lua_pushcfunction( L, vlclua_sd_node_delete );
        lua_setfield( L, -2, "__gc" );
    }
    lua_setmetatable( L, -2 );

    return p_input;
}

static int vlclua_node_add_subnode( lua_State *L )
{
    services_discovery_t *p_sd = (services_discovery_t *)vlclua_get_this( L );
    input_item_t **pp_node = (input_item_t **)luaL_checkudata( L, 1, "node" );

    return vlclua_sd_add_sub_common( p_sd, pp_node,
                                     vlclua_sd_create_node( p_sd, L ) );
}

static const luaL_Reg vlclua_node_reg[] = {
    { "add_subitem", vlclua_node_add_subitem },
    { "add_subnode", vlclua_node_add_subnode },
    { NULL, NULL }
};


/*** Services discovery instance ***/

static int vlclua_sd_add_common( services_discovery_t *p_sd,
                                 input_item_t *p_input )
{
    if( p_input != NULL )
        services_discovery_AddItem( p_sd, p_input );
    return 1;
}

static int vlclua_sd_add_item( lua_State *L )
{
    services_discovery_t *p_sd = (services_discovery_t *)vlclua_get_this( L );

    return vlclua_sd_add_common( p_sd, vlclua_sd_create_item( p_sd, L ) );
}

static int vlclua_sd_add_node( lua_State *L )
{
    services_discovery_t *p_sd = (services_discovery_t *)vlclua_get_this( L );

    return vlclua_sd_add_common( p_sd, vlclua_sd_create_node( p_sd, L ) );
}

static int vlclua_sd_remove_item( lua_State *L )
{
    input_item_t **pp_input = luaL_checkudata( L, 1, "input_item_t" );

    return vlclua_sd_remove_common( L, pp_input );
}

static int vlclua_sd_remove_node( lua_State *L )
{
    input_item_t **pp_input = luaL_checkudata( L, 1, "node" );

    return vlclua_sd_remove_common( L, pp_input );
}

static const luaL_Reg vlclua_sd_sd_reg[] = {
    { "add_item", vlclua_sd_add_item },
    { "add_node", vlclua_sd_add_node },
    { "remove_item", vlclua_sd_remove_item },
    { "remove_node", vlclua_sd_remove_node },
    { NULL, NULL }
};

void luaopen_sd_sd( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_sd_sd_reg );
    lua_setfield( L, -2, "sd" );
}
