/*****************************************************************************
 * input.c
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
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
#ifndef  _GNU_SOURCE
#   define  _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_meta.h>
#include <vlc_url.h>
#include <vlc_playlist_legacy.h>
#include <vlc_playlist.h>
#include <vlc_player.h>

#include <assert.h>

#include "../vlc.h"
#include "input.h"
#include "../libs.h"
#include "../extension.h"

vlc_player_t *vlclua_get_player_internal(lua_State *L) {
    vlc_playlist_t *playlist = vlclua_get_playlist_internal(L);
    return vlc_playlist_GetPlayer(playlist);
}

vout_thread_t *vlclua_get_vout_internal(lua_State *L)
{
    vlc_player_t *player = vlclua_get_player_internal(L);
    return vlc_player_vout_Hold(player);
}

audio_output_t *vlclua_get_aout_internal(lua_State *L)
{
    vlc_player_t *player = vlclua_get_player_internal(L);
    return vlc_player_aout_Hold(player);
}

static input_item_t* vlclua_input_item_get_internal( lua_State *L );

static int vlclua_input_item_info( lua_State *L )
{
    input_item_t *p_item = vlclua_input_item_get_internal( L );
    int i_cat;
    int i;
    i_cat = p_item->i_categories;
    lua_createtable( L, 0, i_cat );
    for( i = 0; i < i_cat; i++ )
    {
        info_category_t *p_category = p_item->pp_categories[i];
        info_t *p_info;

        lua_pushstring( L, p_category->psz_name );
        lua_newtable( L );
        info_foreach(p_info, &p_category->infos)
        {
            lua_pushstring( L, p_info->psz_name );
            lua_pushstring( L, p_info->psz_value );
            lua_settable( L, -3 );
        }
        lua_settable( L, -3 );
    }
    return 1;
}

static int vlclua_input_is_playing( lua_State *L )
{
    vlc_player_t *player = vlclua_get_player_internal(L);

    vlc_player_Lock(player);
    bool started = vlc_player_IsStarted(player);
    vlc_player_Unlock(player);
    lua_pushboolean(L, started);
    return 1;
}

static int vlclua_input_metas_internal( lua_State *L, input_item_t *p_item )
{
    if( !p_item )
    {
        lua_pushnil( L );
        return 1;
    }

    lua_newtable( L );
    const char *psz_meta;

    char* psz_uri = input_item_GetURI( p_item );
    char* psz_filename = psz_uri ? strrchr( psz_uri, '/' ) : NULL;

    if( psz_filename && psz_filename[1] == '\0' )
    {
        /* trailing slash, get the preceeding data */
        psz_filename[0] = '\0';
        psz_filename = strrchr( psz_uri, '/' );
    }

    if( psz_filename )
    {
        /* url decode, without leading slash */
        psz_filename = vlc_uri_decode( psz_filename + 1 );
    }

    lua_pushstring( L, psz_filename );
    lua_setfield( L, -2, "filename" );

    free( psz_uri );

#define PUSH_META( n, m ) \
    psz_meta = vlc_meta_Get( p_item->p_meta, vlc_meta_ ## n ); \
    lua_pushstring( L, psz_meta ); \
    lua_setfield( L, -2, m )

    vlc_mutex_lock(&p_item->lock);

    if (p_item->p_meta)
    {
        PUSH_META( Title, "title" );
        PUSH_META( Artist, "artist" );
        PUSH_META( Genre, "genre" );
        PUSH_META( Copyright, "copyright" );
        PUSH_META( Album, "album" );
        PUSH_META( TrackNumber, "track_number" );
        PUSH_META( Description, "description" );
        PUSH_META( Rating, "rating" );
        PUSH_META( Date, "date" );
        PUSH_META( Setting, "setting" );
        PUSH_META( URL, "url" );
        PUSH_META( Language, "language" );
        PUSH_META( NowPlaying, "now_playing" );
        PUSH_META( Publisher, "publisher" );
        PUSH_META( EncodedBy, "encoded_by" );
        PUSH_META( ArtworkURL, "artwork_url" );
        PUSH_META( TrackID, "track_id" );
        PUSH_META( TrackTotal, "track_total" );
        PUSH_META( Director, "director" );
        PUSH_META( Season, "season" );
        PUSH_META( Episode, "episode" );
        PUSH_META( ShowName, "show_name" );
        PUSH_META( Actors, "actors" );

#undef PUSH_META

        char **names = vlc_meta_CopyExtraNames(p_item->p_meta);
        for(int i = 0; names[i]; i++)
        {
            const char *meta = vlc_meta_GetExtra(p_item->p_meta, names[i]);
            lua_pushstring( L, meta );
            lua_setfield( L, -2, names[i] );
            free(names[i]);
        }
        free(names);
    }
    vlc_mutex_unlock(&p_item->lock);

    return 1;
}

static int vlclua_input_item_stats( lua_State *L )
{
    input_item_t *p_item = vlclua_input_item_get_internal( L );
    lua_newtable( L );
    if( p_item == NULL )
        return 1;

    vlc_mutex_lock( &p_item->lock );
    input_stats_t *p_stats = p_item->p_stats;
    if( p_stats != NULL )
    {
#define STATS_INT( n ) lua_pushinteger( L, p_item->p_stats->i_ ## n ); \
                       lua_setfield( L, -2, #n );
#define STATS_FLOAT( n ) lua_pushnumber( L, p_item->p_stats->f_ ## n ); \
                         lua_setfield( L, -2, #n );
        STATS_INT( read_packets )
        STATS_INT( read_bytes )
        STATS_FLOAT( input_bitrate )
        STATS_INT( demux_read_packets )
        STATS_INT( demux_read_bytes )
        STATS_FLOAT( demux_bitrate )
        STATS_INT( demux_corrupted )
        STATS_INT( demux_discontinuity )
        STATS_INT( decoded_audio )
        STATS_INT( decoded_video )
        STATS_INT( displayed_pictures )
        STATS_INT( lost_pictures )
        STATS_INT( played_abuffers )
        STATS_INT( lost_abuffers )
#undef STATS_INT
#undef STATS_FLOAT
    }
    vlc_mutex_unlock( &p_item->lock );
    return 1;
}

static int vlclua_input_add_subtitle(lua_State *L, bool b_path)
{
    vlc_player_t *player = vlclua_get_player_internal(L);

    if (!lua_isstring(L, 1))
        return luaL_error( L, "vlc.player.add_subtitle() usage: (path)" );

    bool autoselect;
    if (lua_gettop(L) >= 2)
        autoselect = lua_toboolean(L, 2);

    const char *sub = luaL_checkstring(L, 1);
    char *mrl;
    if (b_path)
        mrl = vlc_path2uri(sub, NULL);

    const char *uri = b_path ? mrl : sub;
    vlc_player_AddAssociatedMedia(player, SPU_ES, uri, autoselect, true, false);
    if (b_path)
        free(mrl);

    return 1;
}

static int vlclua_input_add_subtitle_path( lua_State *L )
{
    return vlclua_input_add_subtitle( L, true );
}

static int vlclua_input_add_subtitle_mrl( lua_State *L )
{
    return vlclua_input_add_subtitle( L, false );
}

/*****************************************************************************
 * Input items
 *****************************************************************************/

static input_item_t* vlclua_input_item_get_internal( lua_State *L )
{
    input_item_t **pp_item = luaL_checkudata( L, 1, "input_item" );
    input_item_t *p_item = *pp_item;

    if( !p_item )
        luaL_error( L, "script went completely foobar" );

    return p_item;
}

/* Garbage collection of an input_item_t */
static int vlclua_input_item_delete( lua_State *L )
{
    input_item_t **pp_item = luaL_checkudata( L, 1, "input_item" );
    input_item_t *p_item = *pp_item;

    if( !p_item )
        return luaL_error( L, "script went completely foobar" );

    *pp_item = NULL;
    input_item_Release( p_item );

    return 1;
}

static int vlclua_input_item_get_current( lua_State *L )
{
    vlc_player_t *player = vlclua_get_player_internal(L);

    vlc_player_Lock(player);
    input_item_t *media = vlc_player_GetCurrentMedia(player);
    if (media)
        vlclua_input_item_get(L, media);
    else
        lua_pushnil(L);
    vlc_player_Unlock(player);

    return 1;
}

static int vlclua_input_item_metas( lua_State *L )
{
    vlclua_input_metas_internal( L, vlclua_input_item_get_internal( L ) );
    return 1;
}

static int vlclua_input_item_is_preparsed( lua_State *L )
{
    lua_pushboolean( L, input_item_IsPreparsed( vlclua_input_item_get_internal( L ) ) );
    return 1;
}

static int vlclua_input_item_uri( lua_State *L )
{
    char *uri = input_item_GetURI( vlclua_input_item_get_internal( L ) );
    lua_pushstring( L, uri );
    free( uri );
    return 1;
}

static int vlclua_input_item_name( lua_State *L )
{
    char *name = input_item_GetName( vlclua_input_item_get_internal( L ) );
    lua_pushstring( L, name );
    free( name );
    return 1;
}

static int vlclua_input_item_duration( lua_State *L )
{
    vlc_tick_t duration = input_item_GetDuration( vlclua_input_item_get_internal( L ) );
    lua_pushnumber( L, secf_from_vlc_tick(duration) );
    return 1;
}

static int vlclua_input_item_set_meta( lua_State *L )
{
    input_item_t *p_item = vlclua_input_item_get_internal( L );
    lua_settop( L, 1 + 2 ); // two arguments
    const char *psz_name = luaL_checkstring( L, 2 ),
               *psz_value = luaL_checkstring( L, 3 );

#define META_TYPE( n, s ) { s, vlc_meta_ ## n },
    static const struct
    {
        const char psz_name[15];
        unsigned char type;
    } pp_meta_types[] = {
        META_TYPE( Title, "title" )
        META_TYPE( Artist, "artist" )
        META_TYPE( Genre, "genre" )
        META_TYPE( Copyright, "copyright" )
        META_TYPE( Album, "album" )
        META_TYPE( TrackNumber, "track_number" )
        META_TYPE( Description, "description" )
        META_TYPE( Rating, "rating" )
        META_TYPE( Date, "date" )
        META_TYPE( Setting, "setting" )
        META_TYPE( URL, "url" )
        META_TYPE( Language, "language" )
        META_TYPE( NowPlaying, "now_playing" )
        META_TYPE( ESNowPlaying, "now_playing" )
        META_TYPE( Publisher, "publisher" )
        META_TYPE( EncodedBy, "encoded_by" )
        META_TYPE( ArtworkURL, "artwork_url" )
        META_TYPE( TrackID, "track_id" )
        META_TYPE( TrackTotal, "track_total" )
        META_TYPE( Director, "director" )
        META_TYPE( Season, "season" )
        META_TYPE( Episode, "episode" )
        META_TYPE( ShowName, "show_name" )
        META_TYPE( Actors, "actors" )
        META_TYPE( AlbumArtist, "album_artist" )
        META_TYPE( DiscNumber, "disc_number" )
        META_TYPE( DiscTotal, "disc_total" )
    };
#undef META_TYPE

    static_assert( sizeof(pp_meta_types)
                      == VLC_META_TYPE_COUNT * sizeof(pp_meta_types[0]),
                   "Inconsistent meta data types" );
    vlc_meta_type_t type = vlc_meta_Title;
    for( unsigned i = 0; i < VLC_META_TYPE_COUNT; i++ )
    {
        if( !strcasecmp( pp_meta_types[i].psz_name, psz_name ) )
        {
            type = pp_meta_types[i].type;
            input_item_SetMeta( p_item, type, psz_value );
            return 1;
        }
    }

    vlc_meta_AddExtra( p_item->p_meta, psz_name, psz_value );
    return 1;
}

/*****************************************************************************
 * Lua bindings
 *****************************************************************************/
static const luaL_Reg vlclua_input_reg[] = {
    { "is_playing", vlclua_input_is_playing },
    { "item", vlclua_input_item_get_current },
    { "add_subtitle", vlclua_input_add_subtitle_path },
    { "add_subtitle_mrl", vlclua_input_add_subtitle_mrl },
    { NULL, NULL }
};

void luaopen_input( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_input_reg );
    lua_setfield( L, -2, "player" );
}

static const luaL_Reg vlclua_input_item_reg[] = {
    { "is_preparsed", vlclua_input_item_is_preparsed },
    { "metas", vlclua_input_item_metas },
    { "set_meta", vlclua_input_item_set_meta },
    { "uri", vlclua_input_item_uri },
    { "name", vlclua_input_item_name },
    { "duration", vlclua_input_item_duration },
    { "stats", vlclua_input_item_stats },
    { "info", vlclua_input_item_info },
    { NULL, NULL }
};

int vlclua_input_item_get( lua_State *L, input_item_t *p_item )
{
    input_item_Hold( p_item );
    input_item_t **pp = lua_newuserdata( L, sizeof( input_item_t* ) );
    *pp = p_item;

    if( luaL_newmetatable( L, "input_item" ) )
    {
        lua_newtable( L );
        luaL_register( L, NULL, vlclua_input_item_reg );
        lua_setfield( L, -2, "__index" );
        lua_pushcfunction( L, vlclua_input_item_delete );
        lua_setfield( L, -2, "__gc" );
    }

    lua_setmetatable(L, -2);

    return 1;
}


void luaopen_input_item( lua_State *L, input_item_t *item )
{
    assert(item);
    vlclua_input_item_get( L, item );
    lua_setfield( L, -2, "item" );
}
