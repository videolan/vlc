/*****************************************************************************
 * sd.c: Services discovery related functions
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
 * $Id$
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

#include <vlc_common.h>
#include <vlc_services_discovery.h>
#include <vlc_playlist.h>
#include <vlc_charset.h>
#include <vlc_md5.h>

#include "../vlc.h"
#include "../libs.h"
#include "playlist.h"

/*****************************************************************************
 *
 *****************************************************************************/
static int vlclua_node_add_subitem( lua_State * );
static int vlclua_node_add_subnode( lua_State * );

static const luaL_Reg vlclua_node_reg[] = {
    { "add_subitem", vlclua_node_add_subitem },
    { "add_subnode", vlclua_node_add_subnode },
    { NULL, NULL }
};

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
    { NULL, NULL }
};

static int vlclua_sd_get_services_names( lua_State *L )
{
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    char **ppsz_longnames;
    char **ppsz_names = vlc_sd_GetNames( p_playlist, &ppsz_longnames, NULL );
    if( !ppsz_names )
        return 0;

    char **ppsz_longname = ppsz_longnames;
    char **ppsz_name = ppsz_names;
    lua_settop( L, 0 );
    lua_newtable( L );
    for( ; *ppsz_name; ppsz_name++,ppsz_longname++ )
    {
        lua_pushstring( L, *ppsz_longname );
        lua_setfield( L, -2, *ppsz_name );
        free( *ppsz_name );
        free( *ppsz_longname );
    }
    free( ppsz_names );
    free( ppsz_longnames );
    return 1;
}

static int vlclua_sd_add( lua_State *L )
{
    const char *psz_sd = luaL_checkstring( L, 1 );
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    int i_ret = playlist_ServicesDiscoveryAdd( p_playlist, psz_sd );
    return vlclua_push_ret( L, i_ret );
}

static int vlclua_sd_remove( lua_State *L )
{
    const char *psz_sd = luaL_checkstring( L, 1 );
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    int i_ret = playlist_ServicesDiscoveryRemove( p_playlist, psz_sd );
    return vlclua_push_ret( L, i_ret );
}

static int vlclua_sd_is_loaded( lua_State *L )
{
    const char *psz_sd = luaL_checkstring( L, 1 );
    playlist_t *p_playlist = vlclua_get_playlist_internal( L );
    lua_pushboolean( L, playlist_IsServicesDiscoveryLoaded( p_playlist, psz_sd ));
    return 1;
}

static int vlclua_sd_add_node( lua_State *L )
{
    services_discovery_t *p_sd = (services_discovery_t *)vlclua_get_this( L );
    if( lua_istable( L, -1 ) )
    {
        lua_getfield( L, -1, "title" );
        if( lua_isstring( L, -1 ) )
        {
            const char *psz_name = lua_tostring( L, -1 );
            input_item_t *p_input = input_item_NewWithType( "vlc://nop",
                                                            psz_name, 0, NULL, 0,
                                                            -1, ITEM_TYPE_NODE );
            lua_pop( L, 1 );

            if( p_input )
            {
                lua_getfield( L, -1, "arturl" );
                if( lua_isstring( L, -1 ) && strcmp( lua_tostring( L, -1 ), "" ) )
                {
                    char *psz_value = strdup( lua_tostring( L, -1 ) );
                    EnsureUTF8( psz_value );
                    msg_Dbg( p_sd, "ArtURL: %s", psz_value );
                    /** @todo Ask for art download if not local file */
                    input_item_SetArtURL( p_input, psz_value );
                    free( psz_value );
                }
                lua_pop( L, 1 );
                lua_getfield( L, -1, "category" );
                if( lua_isstring( L, -1 ) )
                    services_discovery_AddItem( p_sd, p_input, luaL_checkstring( L, -1 ) );
                else
                    services_discovery_AddItem( p_sd, p_input, NULL );
                input_item_t **udata = (input_item_t **)
                                       lua_newuserdata( L, sizeof( input_item_t * ) );
                *udata = p_input;
                if( luaL_newmetatable( L, "node" ) )
                {
                    lua_newtable( L );
                    luaL_register( L, NULL, vlclua_node_reg );
                    lua_setfield( L, -2, "__index" );
                }
                lua_setmetatable( L, -2 );
            }
        }
        else
            msg_Err( p_sd, "vlc.sd.add_node: the \"title\" parameter can't be empty" );
    }
    else
        msg_Err( p_sd, "Error parsing add_node arguments" );
    return 1;
}

static int vlclua_sd_add_item( lua_State *L )
{
    services_discovery_t *p_sd = (services_discovery_t *)vlclua_get_this( L );
    if( lua_istable( L, -1 ) )
    {
        lua_getfield( L, -1, "path" );
        if( lua_isstring( L, -1 ) )
        {
            const char *psz_path = lua_tostring( L, -1 );

            lua_getfield( L, -2, "title" );
            const char *psz_title = luaL_checkstring( L, -1 ) ? luaL_checkstring( L, -1 ) : psz_path;

            /* The table must be at the top of the stack when calling
             * vlclua_read_options() */
            char **ppsz_options = NULL;
            int i_options = 0;
            lua_pushvalue( L, -3 );
            vlclua_read_options( p_sd, L, &i_options, &ppsz_options );

            input_item_t *p_input = input_item_NewExt( psz_path, psz_title,
                                                       i_options,
                                                       (const char **)ppsz_options,
                                                       VLC_INPUT_OPTION_TRUSTED, -1 );
            lua_pop( L, 3 );

            if( p_input )
            {
                vlclua_read_meta_data( p_sd, L, p_input );
                /* This one is to be tested... */
                vlclua_read_custom_meta_data( p_sd, L, p_input );
                /* The duration is given in seconds, convert to microseconds */
                lua_getfield( L, -1, "duration" );
                if( lua_isnumber( L, -1 ) )
                   input_item_SetDuration( p_input, (lua_tonumber( L, -1 )*1e6) );
                else if( !lua_isnil( L, -1 ) )
                    msg_Warn( p_sd, "Item duration should be a number (in seconds)." );
                lua_pop( L, 1 );
                lua_getfield( L, -1, "category" );
                if( lua_isstring( L, -1 ) )
                    services_discovery_AddItem( p_sd, p_input, luaL_checkstring( L, -1 ) );
                else
                    services_discovery_AddItem( p_sd, p_input, NULL );
                lua_pop( L, 1 );

                /* string to build the input item uid */
                lua_getfield( L, -1, "uiddata" );
                if( lua_isstring( L, -1 ) )
                {
                    char *s = strdup( luaL_checkstring( L, -1 ) );
                    if ( s )
                    {
                        struct md5_s md5;
                        InitMD5( &md5 );
                        AddMD5( &md5, s, strlen( s ) );
                        EndMD5( &md5 );
                        free( s );
                        s = psz_md5_hash( &md5 );
                        if ( s )
                            input_item_AddInfo( p_input, "uid", "md5", "%s", s );
                        free( s );
                    }
                }
                lua_pop( L, 1 );

                input_item_t **udata = (input_item_t **)
                                       lua_newuserdata( L, sizeof( input_item_t * ) );
                *udata = p_input;
                if( luaL_newmetatable( L, "input_item_t" ) )
                {
                    lua_newtable( L );
                    luaL_register( L, NULL, vlclua_item_reg );
                    lua_setfield( L, -2, "__index" );
                    lua_pushliteral( L, "none of your business" );
                    lua_setfield( L, -2, "__metatable" );
                }
                lua_setmetatable( L, -2 );
                vlc_gc_decref( p_input );
            }
            while( i_options > 0 )
                free( ppsz_options[--i_options] );
            free( ppsz_options );
        }
        else
            msg_Err( p_sd, "vlc.sd.add_item: the \"path\" parameter can't be empty" );
    }
    else
        msg_Err( p_sd, "Error parsing add_item arguments" );
    return 1;
}

static int vlclua_sd_remove_item( lua_State *L )
{
    services_discovery_t *p_sd = (services_discovery_t *)vlclua_get_this( L );
    if( !lua_isnil( L, 1 ) )
    {
        input_item_t **pp_input = luaL_checkudata( L, 1, "input_item_t" );
        if( *pp_input )
            services_discovery_RemoveItem( p_sd, *pp_input );
        /* Make sure we won't try to remove it again */
        *pp_input = NULL;
    }
    return 1;
}

static int vlclua_sd_remove_all_items_nodes( lua_State *L )
{
    services_discovery_t *p_sd = (services_discovery_t *)vlclua_get_this( L );
    services_discovery_RemoveAll( p_sd );
    return 1;
}

static int vlclua_node_add_subitem( lua_State *L )
{
    services_discovery_t *p_sd = (services_discovery_t *)vlclua_get_this( L );
    input_item_t **pp_node = (input_item_t **)luaL_checkudata( L, 1, "node" );
    if( *pp_node )
    {
        if( lua_istable( L, -1 ) )
        {
            lua_getfield( L, -1, "path" );
            if( lua_isstring( L, -1 ) )
            {
                const char *psz_path = lua_tostring( L, -1 );

                /* The table must be at the top of the stack when calling
                 * vlclua_read_options() */
                char **ppsz_options = NULL;
                int i_options = 0;
                lua_pushvalue( L, -2 );
                vlclua_read_options( p_sd, L, &i_options, &ppsz_options );

                input_item_node_t *p_input_node = input_item_node_Create( *pp_node );
                input_item_t *p_input = input_item_NewExt( psz_path,
                                                           psz_path, i_options,
                                                           (const char **)ppsz_options,
                                                           VLC_INPUT_OPTION_TRUSTED, -1 );
                lua_pop( L, 2 );

                if( p_input )
                {
                    vlclua_read_meta_data( p_sd, L, p_input );
                    /* This one is to be tested... */
                    vlclua_read_custom_meta_data( p_sd, L, p_input );
                    lua_getfield( L, -1, "duration" );
                    if( lua_isnumber( L, -1 ) )
                        input_item_SetDuration( p_input, (lua_tonumber( L, -1 )*1e6) );
                    else if( !lua_isnil( L, -1 ) )
                        msg_Warn( p_sd, "Item duration should be a number (in seconds)." );
                    lua_pop( L, 1 );
                    input_item_node_AppendItem( p_input_node, p_input );
                    input_item_node_PostAndDelete( p_input_node );
                    input_item_t **udata = (input_item_t **)
                                           lua_newuserdata( L, sizeof( input_item_t * ) );
                    *udata = p_input;
                    if( luaL_newmetatable( L, "input_item_t" ) )
                    {
                        lua_newtable( L );
                        luaL_register( L, NULL, vlclua_item_reg );
                        lua_setfield( L, -2, "__index" );
                        lua_pushliteral( L, "none of your business" );
                        lua_setfield( L, -2, "__metatable" );
                    }
                    lua_setmetatable( L, -2 );
                    vlc_gc_decref( p_input );
                }
                while( i_options > 0 )
                    free( ppsz_options[--i_options] );
                free( ppsz_options );
            }
            else
                msg_Err( p_sd, "node:add_subitem: the \"path\" parameter can't be empty" );
        }
        else
            msg_Err( p_sd, "Error parsing add_subitem arguments" );
    }
    return 1;
}

static int vlclua_node_add_subnode( lua_State *L )
{
    services_discovery_t *p_sd = (services_discovery_t *)vlclua_get_this( L );
    input_item_t **pp_node = (input_item_t **)luaL_checkudata( L, 1, "node" );
    if( *pp_node )
    {
        if( lua_istable( L, -1 ) )
        {
            lua_getfield( L, -1, "title" );
            if( lua_isstring( L, -1 ) )
            {
                const char *psz_name = lua_tostring( L, -1 );
                input_item_node_t *p_input_node = input_item_node_Create( *pp_node );
                input_item_t *p_input = input_item_NewWithType( "vlc://nop",
                                                                psz_name, 0, NULL, 0,
                                                                -1, ITEM_TYPE_NODE );
                lua_pop( L, 1 );

                if( p_input )
                {
                    lua_getfield( L, -1, "arturl" );
                    if( lua_isstring( L, -1 ) && strcmp( lua_tostring( L, -1 ), "" ) )
                    {
                        char *psz_value = strdup( lua_tostring( L, -1 ) );
                        EnsureUTF8( psz_value );
                        msg_Dbg( p_sd, "ArtURL: %s", psz_value );
                        input_item_SetArtURL( p_input, psz_value );
                        free( psz_value );
                    }
                    input_item_node_AppendItem( p_input_node, p_input );
                    input_item_node_PostAndDelete( p_input_node );
                    input_item_t **udata = (input_item_t **)
                                           lua_newuserdata( L, sizeof( input_item_t * ) );
                    *udata = p_input;
                    if( luaL_newmetatable( L, "node" ) )
                    {
                        lua_newtable( L );
                        luaL_register( L, NULL, vlclua_node_reg );
                        lua_setfield( L, -2, "__index" );
                    }
                    lua_setmetatable( L, -2 );
                }
            }
            else
                msg_Err( p_sd, "node:add_node: the \"title\" parameter can't be empty" );
        }
        else
            msg_Err( p_sd, "Error parsing add_node arguments" );
    }
    return 1;
}

/*****************************************************************************
 *
 *****************************************************************************/
static const luaL_Reg vlclua_sd_reg[] = {
    { "get_services_names", vlclua_sd_get_services_names },
    { "add", vlclua_sd_add },
    { "remove", vlclua_sd_remove },
    { "is_loaded", vlclua_sd_is_loaded },
    { "add_node", vlclua_sd_add_node },
    { "add_item", vlclua_sd_add_item },
    { "remove_item", vlclua_sd_remove_item },
    { "remove_all_items_nodes", vlclua_sd_remove_all_items_nodes },
    { NULL, NULL }
};

void luaopen_sd( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_sd_reg );
    lua_setfield( L, -2, "sd" );
}
