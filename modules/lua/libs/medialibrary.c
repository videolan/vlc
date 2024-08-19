/*****************************************************************************
 * medialibrary.c
 *****************************************************************************
 * Copyright (C) 2020 the VideoLAN team
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_media_library.h>

#include "../libs.h"

static void vlclua_ml_push_media( lua_State *L, const vlc_ml_media_t *media )
{
    lua_newtable( L );
    lua_pushinteger( L, media->i_id );
    lua_setfield( L, -2, "id" );
    lua_pushstring( L, media->psz_title );
    lua_setfield( L, -2, "title" );
    lua_pushstring( L, media->psz_filename );
    lua_setfield( L, -2, "filename" );
    lua_pushinteger( L, media->i_type );
    lua_setfield( L, -2, "type" );
    lua_pushinteger( L, media->i_duration );
    lua_setfield( L, -2, "duration" );
    lua_pushboolean( L, media->thumbnails[VLC_ML_THUMBNAIL_SMALL].i_status == VLC_ML_THUMBNAIL_STATUS_AVAILABLE  );
    lua_setfield( L, -2, "hasThumbnail" );
    lua_pushinteger( L, media->i_playcount );
    lua_setfield( L, -2, "playcount" );
    lua_pushnumber( L, media->f_progress );
    lua_setfield( L, -2, "progress" );
    lua_pushboolean( L, media->b_is_favorite );
    lua_setfield( L, -2, "isFavorite" );
    lua_pushinteger( L, media->i_last_played_date );
    lua_setfield( L, -2, "lastPlayedDate" );
    switch ( media->i_subtype )
    {
        case VLC_ML_MEDIA_SUBTYPE_ALBUMTRACK:
        {
            lua_newtable( L );

            lua_pushinteger( L, media->album_track.i_artist_id );
            lua_setfield( L, -2, "artistId" );
            lua_pushinteger( L, media->album_track.i_album_id );
            lua_setfield( L, -2, "albumId" );
            lua_pushinteger( L, media->album_track.i_genre_id );
            lua_setfield( L, -2, "genreId" );
            lua_pushinteger( L, media->album_track.i_track_nb );
            lua_setfield( L, -2, "trackId" );
            lua_pushinteger( L, media->album_track.i_disc_nb );
            lua_setfield( L, -2, "discId" );

            lua_setfield( L, -2, "albumTrack" );
            lua_pushstring( L, "albumTrack" );
            break;
        }
        case VLC_ML_MEDIA_SUBTYPE_MOVIE:
        {
            lua_newtable( L );

            lua_pushstring( L, media->movie.psz_summary );
            lua_setfield( L, -2, "summary" );

            lua_setfield( L, -2, "movie" );
            lua_pushstring( L, "movie" );
            break;
        }
        case VLC_ML_MEDIA_SUBTYPE_SHOW_EPISODE:
        {
            lua_newtable( L );

            lua_pushstring( L, media->show_episode.psz_summary );
            lua_setfield( L, -2, "summary" );
            lua_pushinteger( L, media->show_episode.i_episode_nb );
            lua_setfield( L, -2, "episodeId" );
            lua_pushinteger( L, media->show_episode.i_season_number );
            lua_setfield( L, -2, "seasonId" );

            lua_setfield( L, -2, "showEpisode" );
            lua_pushstring( L, "showEpisode" );
            break;
        }
        default:
            lua_pushstring( L, "unknown" );
            break;
    }
    lua_setfield( L, -2, "subType" );

    for ( size_t i = 0; i < media->p_files->i_nb_items; ++i )
    {
        if ( media->p_files->p_items[i].i_type == VLC_ML_FILE_TYPE_MAIN )
        {
            lua_pushstring( L, media->p_files->p_items[i].psz_mrl );
            lua_setfield( L, -2, "mrl" );
            break;
        }
    }

    if ( media->i_type != VLC_ML_MEDIA_TYPE_VIDEO )
        return;

    const char *quality = NULL;
    uint32_t maxChannels = 0;
    for ( size_t i = 0; i < media->p_tracks->i_nb_items; ++i )
    {
        vlc_ml_media_track_t *track = &media->p_tracks->p_items[i];
        if ( track->i_type == VLC_ML_TRACK_TYPE_VIDEO && quality == NULL )
        {
            uint32_t width = track->v.i_width > track->v.i_height ?
                        track->v.i_width : track->v.i_height;

            if ( width >= 3840 )
            {
                quality = "4K";
                break;
            }
            else if ( width >= 1920 )
            {
                quality = "1080p";
                break;
            }
            else if ( width >= 1280 )
            {
                quality = "720p";
                break;
            }
        }
        if ( track->i_type == VLC_ML_TRACK_TYPE_AUDIO )
        {
            if ( track->a.i_nbChannels > maxChannels )
                maxChannels = track->a.i_nbChannels;
        }
    }
    if ( quality == NULL )
        quality = "SD";
    lua_pushstring( L, quality );
    lua_setfield( L, -2, "quality" );
    lua_pushinteger( L, maxChannels );
    lua_setfield( L, -2, "nbChannels" );
}

static void vlclua_ml_push_folder( lua_State *L, const vlc_ml_folder_t *folder )
{
    lua_newtable( L );
    lua_pushinteger( L, folder->i_id );
    lua_setfield( L, -2, "id" );
    lua_pushstring( L, folder->psz_name );
    lua_setfield( L, -2, "title" );
    lua_pushstring( L, folder->psz_mrl );
    lua_setfield( L, -2, "mrl" );
    lua_pushinteger( L, folder->i_nb_media );
    lua_setfield( L, -2, "nbMedia" );
    lua_pushinteger( L, folder->i_nb_video );
    lua_setfield( L, -2, "nbVideos" );
    lua_pushinteger( L, folder->i_nb_audio );
    lua_setfield( L, -2, "nbAudios" );
    lua_pushinteger( L, folder->i_duration );
    lua_setfield( L, -2, "duration" );
    lua_pushboolean( L, folder->b_present );
    lua_setfield( L, -2, "isPresent" );
    lua_pushboolean( L, folder->b_banned );
    lua_setfield( L, -2, "isBanned" );
    lua_pushboolean( L, folder->b_is_favorite );
    lua_setfield( L, -2, "isFavorite" );

}

static void vlclua_ml_push_show( lua_State *L, const vlc_ml_show_t *show )
{
    lua_newtable( L );
    lua_pushinteger( L, show->i_id );
    lua_setfield( L, -2, "id" );
    lua_pushstring( L, show->psz_name );
    lua_setfield( L, -2, "name" );
    lua_pushstring( L, show->psz_summary );
    lua_setfield( L, -2, "summary" );
    lua_pushstring( L, show->psz_artwork_mrl );
    lua_setfield( L, -2, "artworkMrl" );
    lua_pushstring( L, show->psz_tvdb_id );
    lua_setfield( L, -2, "tvdbId" );
    lua_pushboolean( L, show->i_release_year );
    lua_setfield( L, -2, "releaseYear" );
    lua_pushboolean( L, show->i_nb_episodes );
    lua_setfield( L, -2, "nbEpisodes" );
    lua_pushboolean( L, show->i_nb_seasons );
    lua_setfield( L, -2, "nbSeason" );
}

static void vlclua_ml_push_album( lua_State *L, const vlc_ml_album_t *album )
{
    lua_newtable( L );
    lua_pushinteger( L, album->i_id );
    lua_setfield( L, -2, "id" );
    lua_pushstring( L, album->psz_title );
    lua_setfield( L, -2, "title" );
    lua_pushboolean( L, album->thumbnails[VLC_ML_THUMBNAIL_SMALL].i_status == VLC_ML_THUMBNAIL_STATUS_AVAILABLE  );
    lua_setfield( L, -2, "hasThumbnail" );
    lua_pushinteger( L, album->i_artist_id );
    lua_setfield( L, -2, "artistId" );
    lua_pushstring( L, album->psz_artist );
    lua_setfield( L, -2, "artist" );
    lua_pushinteger( L, album->i_nb_tracks );
    lua_setfield( L, -2, "nbTracks" );
    lua_pushinteger( L, album->i_duration );
    lua_setfield( L, -2, "duration" );
    lua_pushinteger( L, album->i_year );
    lua_setfield( L, -2, "releaseYear" );
    lua_pushboolean( L, album->b_is_favorite );
    lua_setfield( L, -2, "isFavorite" );
}

static void vlclua_ml_push_artist( lua_State *L, const vlc_ml_artist_t *artist )
{
    lua_newtable( L );
    lua_pushinteger( L, artist->i_id );
    lua_setfield( L, -2, "id" );
    lua_pushstring( L, artist->psz_name );
    lua_setfield( L, -2, "name" );
    lua_pushboolean( L, artist->thumbnails[VLC_ML_THUMBNAIL_SMALL].i_status == VLC_ML_THUMBNAIL_STATUS_AVAILABLE  );
    lua_setfield( L, -2, "hasThumbnail" );
    lua_pushinteger( L, artist->i_nb_tracks );
    lua_setfield( L, -2, "nbTracks" );
    lua_pushinteger( L, artist->i_nb_album );
    lua_setfield( L, -2, "nbAlbums" );
    lua_pushboolean( L, artist->b_is_favorite );
    lua_setfield( L, -2, "isFavorite" );
}

static void vlclua_ml_push_genre( lua_State *L, const vlc_ml_genre_t *genre )
{
    lua_newtable( L );
    lua_pushinteger( L, genre->i_id );
    lua_setfield( L, -2, "id" );
    lua_pushstring( L, genre->psz_name );
    lua_setfield( L, -2, "name" );
    lua_pushinteger( L, genre->i_nb_tracks );
    lua_setfield( L, -2, "nbTracks" );
    lua_pushboolean( L, genre->thumbnails[VLC_ML_THUMBNAIL_SMALL].i_status == VLC_ML_THUMBNAIL_STATUS_AVAILABLE );
    lua_setfield( L, -2, "hasThumbnail");
    lua_pushboolean(L, genre->b_is_favorite );
    lua_setfield( L, -2, "isFavorite" );
}

static void vlclua_ml_push_playlist( lua_State *L, const vlc_ml_playlist_t *playlist )
{
    lua_newtable( L );
    lua_pushinteger( L, playlist->i_id );
    lua_setfield( L, -2, "id" );
    lua_pushstring( L, playlist->psz_name );
    lua_setfield( L, -2, "name" );
    lua_pushstring( L, playlist->psz_mrl );
    lua_setfield( L, -2, "mrl" );
    lua_pushstring( L, playlist->psz_artwork_mrl );
    lua_setfield( L, -2, "artworkMrl" );
    lua_pushinteger( L, playlist->i_nb_media );
    lua_setfield( L, -2, "nbMedia" );
    lua_pushinteger( L, playlist->i_nb_video );
    lua_setfield( L, -2, "nbVideo" );
    lua_pushinteger( L, playlist->i_nb_audio );
    lua_setfield( L, -2, "nbAudio" );
    lua_pushinteger( L, playlist->i_nb_unknown );
    lua_setfield( L, -2, "nbUnknown" );
    lua_pushinteger( L, playlist->i_nb_present_media );
    lua_setfield( L, -2, "nbPresentMedia" );
    lua_pushinteger( L, playlist->i_nb_present_video );
    lua_setfield( L, -2, "nbPresentVideo" );
    lua_pushinteger( L, playlist->i_nb_present_audio );
    lua_setfield( L, -2, "nbPresentAudio" );
    lua_pushinteger( L, playlist->i_nb_present_unknown );
    lua_setfield( L, -2, "nbPresentUnknown" );
    lua_pushinteger( L, playlist->i_creation_date );
    lua_setfield( L, -2, "creationDate" );
    lua_pushinteger( L, playlist->i_duration );
    lua_setfield( L, -2, "duration" );
    lua_pushinteger( L, playlist->i_nb_duration_unknown );
    lua_setfield( L, -2, "nbDurationUnknown" );
    lua_pushboolean( L, playlist->b_is_read_only );
    lua_setfield( L, -2, "isReadOnly" );
    lua_pushboolean( L, playlist->b_is_favorite );
    lua_setfield( L, -2, "isFavorite" );
}

static void vlclua_ml_push_bookmark( lua_State *L, const vlc_ml_bookmark_t *bookmark )
{
    lua_newtable( L );
    lua_pushinteger( L, bookmark->i_media_id );
    lua_setfield( L, -2, "mediaid" );
    lua_pushinteger( L, bookmark->i_time );
    lua_setfield( L, -2, "time" );
    lua_pushstring( L, bookmark->psz_name );
    lua_setfield( L, -2, "name" );
    lua_pushstring( L, bookmark->psz_description );
    lua_setfield( L, -2, "description" );
}

static int vlclua_ml_get_media( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    lua_Integer mediaId = luaL_checkinteger( L, 1 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_media_t *media = vlc_ml_get_media( ml, mediaId );
    if ( media == NULL )
        return luaL_error( L, "Failed to get media" );
    vlclua_ml_push_media( L, media );
    vlc_ml_release( media );
    return 1;
}

static int vlclua_ml_get_media_by_mrl( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    const char *mediaMrl = luaL_checkstring( L, 1 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_media_t *media = vlc_ml_get_media_by_mrl( ml, mediaMrl );
    if ( media == NULL )
        return luaL_error( L, "Failed to get media" );
    vlclua_ml_push_media( L, media );
    vlc_ml_release( media );
    return 1;
}

static int vlclua_ml_list_media( lua_State *L, vlc_ml_media_list_t *list )
{
    if ( list == NULL )
        return luaL_error( L, "Failed to list media" );
    lua_createtable( L, list->i_nb_items, 0 );
    for ( size_t i = 0; i < list->i_nb_items; ++i )
    {
        vlclua_ml_push_media( L, &list->p_items[i] );
        lua_rawseti( L, -2, i + 1 );
    }
    vlc_ml_release( list );
    return 1;
}

static int vlclua_ml_list_folder( lua_State *L, vlc_ml_folder_list_t *list )
{
    if ( list == NULL )
        return luaL_error( L, "Failed to list folders" );
    lua_createtable( L, list->i_nb_items, 0 );
    for ( size_t i = 0; i < list->i_nb_items; ++i )
    {
        vlclua_ml_push_folder( L, &list->p_items[i] );
        lua_rawseti( L, -2, i + 1 );
    }
    vlc_ml_release( list );
    return 1;
}

static int vlclua_ml_list_show( lua_State *L, vlc_ml_show_list_t *list )
{
    if ( list == NULL )
        return luaL_error( L, "Failed to list show" );
    lua_createtable( L, list->i_nb_items, 0 );
    for ( size_t i = 0; i < list->i_nb_items; ++i )
    {
        vlclua_ml_push_show( L, &list->p_items[i] );
        lua_rawseti( L, -2, i + 1 );
    }
    vlc_ml_release( list );
    return 1;
}

static int vlclua_ml_list_bookmark( lua_State *L, vlc_ml_bookmark_list_t *list )
{
    if ( list == NULL )
        return luaL_error( L, "Failed to list bookmarks" );
    lua_createtable( L, list->i_nb_items, 0 );
    for ( size_t i = 0; i < list->i_nb_items; ++i )
    {
        vlclua_ml_push_bookmark( L, &list->p_items[i] );
        lua_rawseti( L, -2, i + 1 );
    }
    vlc_ml_release( list );
    return 1;
}

static void vlclua_ml_assign_params( lua_State *L, vlc_ml_query_params_t *params, uint8_t paramIndex )
{
    *params = vlc_ml_query_params_create();
    if (!lua_istable( L, paramIndex ))
        return;
    lua_getfield( L, 1, "favorite_only" );
    lua_getfield( L, 1, "limit" );
    lua_getfield( L, 1, "offset" );
    lua_getfield( L, 1, "desc" );
    lua_getfield( L, 1, "sort" );
    lua_getfield( L, 1, "pattern" );

    //impose public_only to true until something is done about permissions
    params->b_public_only = true;
    params->b_favorite_only = lua_toboolean( L, -6 );
    params->i_nbResults = lua_tointeger( L, -5 );
    params->i_offset = lua_tointeger( L, -4 );
    params->b_desc = lua_toboolean( L, -3 );
    params->i_sort = lua_tointeger( L, -2);
    params->psz_pattern = lua_tostring( L, -1);
}

static int vlclua_ml_video( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_ml_query_params_t params;
    vlclua_ml_assign_params( L, &params, 1 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_media_list_t *list = vlc_ml_list_video_media( ml, &params );
    return vlclua_ml_list_media( L, list );
}

static int vlclua_ml_folder( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_ml_query_params_t params;
    vlclua_ml_assign_params( L, &params, 1 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_folder_list_t *list = vlc_ml_list_folders( ml, &params );
    return vlclua_ml_list_folder( L, list );
}

static int vlclua_ml_folders_by_type( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_ml_query_params_t params;
    vlclua_ml_assign_params( L, &params, 1 );
    vlc_ml_media_type_t mediaType = luaL_checkinteger( L, 2 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_folder_list_t *list = vlc_ml_list_folders_by_type( ml, &params, mediaType );
    return vlclua_ml_list_folder( L, list );
}

static int vlclua_ml_list_shows( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_ml_query_params_t params;
    vlclua_ml_assign_params( L, &params, 1 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_show_list_t *list = vlc_ml_list_shows( ml, &params );
    return vlclua_ml_list_show( L, list );
}

static int vlclua_ml_audio( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_ml_query_params_t params;
    vlclua_ml_assign_params( L, &params, 1 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_media_list_t *list = vlc_ml_list_audio_media( ml, &params );
    return vlclua_ml_list_media( L, list );
}

static int vlclua_ml_list_albums( lua_State *L, vlc_ml_album_list_t *list )
{
    if ( list == NULL )
        return luaL_error( L, "Failed to list albums" );
    lua_createtable( L, list->i_nb_items, 0 );
    for ( size_t i = 0; i < list->i_nb_items; ++i )
    {
        vlclua_ml_push_album( L, &list->p_items[i] );
        lua_rawseti( L, -2, i + 1 );
    }
    vlc_ml_release( list );
    return 1;
}

static int vlclua_ml_list_all_albums( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_ml_query_params_t params;
    vlclua_ml_assign_params( L, &params, 1 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_album_list_t *list = vlc_ml_list_albums( ml, &params );
    return vlclua_ml_list_albums( L, list );
}

static int vlclua_ml_list_artist_albums( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_ml_query_params_t params;
    vlclua_ml_assign_params( L, &params, 2 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    lua_Integer artistId = luaL_checkinteger( L, 1 );
    vlc_ml_album_list_t *list = vlc_ml_list_artist_albums( ml, &params, artistId );
    return vlclua_ml_list_albums( L, list );
}

static int vlclua_ml_list_genre_albums( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_ml_query_params_t params;
    vlclua_ml_assign_params( L, &params, 2 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    lua_Integer genreId = luaL_checkinteger( L, 1 );
    vlc_ml_album_list_t *list = vlc_ml_list_genre_albums( ml, &params, genreId );
    return vlclua_ml_list_albums( L, list );
}

static int vlclua_ml_get_album( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    lua_Integer albumId = luaL_checkinteger( L, 1 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_album_t *album = vlc_ml_get_album( ml, albumId );
    if ( album == NULL )
        return luaL_error( L, "Failed to get album" );
    vlclua_ml_push_album( L, album );
    vlc_ml_release( album );
    return 1;
}

static int vlclua_ml_get_folder( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    lua_Integer folderId = luaL_checkinteger( L, 1 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_folder_t *folder = vlc_ml_get_folder( ml, folderId );
    if ( folder == NULL )
        return luaL_error( L, "Failed to get folder" );
    vlclua_ml_push_folder( L, folder );
    vlc_ml_release( folder );
    return 1;
}

static int vlclua_ml_list_artists( lua_State *L, vlc_ml_artist_list_t *list )
{
    if ( list == NULL )
        return luaL_error( L, "Failed to list artists" );
    lua_createtable( L, list->i_nb_items, 0 );
    for ( size_t i = 0; i < list->i_nb_items; ++i )
    {
        vlclua_ml_push_artist( L, &list->p_items[i] );
        lua_rawseti( L, -2, i + 1 );
    }
    vlc_ml_release( list );
    return 1;
}

static int vlclua_ml_list_all_artists( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_ml_query_params_t params;
    vlclua_ml_assign_params( L, &params, 1 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_artist_list_t *list = vlc_ml_list_artists( ml, &params, true );
    return vlclua_ml_list_artists( L, list );
}

static int vlclua_ml_list_genre_artists( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_ml_query_params_t params;
    vlclua_ml_assign_params( L, &params, 2 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    lua_Integer genreId = luaL_checkinteger( L, 1 );
    vlc_ml_artist_list_t *list = vlc_ml_list_genre_artists( ml, &params, genreId );
    return vlclua_ml_list_artists( L, list );
}

static int vlclua_ml_get_artist( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    lua_Integer artistId = luaL_checkinteger( L, 1 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_artist_t *artist = vlc_ml_get_artist( ml, artistId );
    if ( artist == NULL )
        return luaL_error( L, "Failed to get artist" );
    vlclua_ml_push_artist( L, artist );
    vlc_ml_release( artist );
    return 1;
}

static int vlclua_ml_list_genres( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_ml_query_params_t params;
    vlclua_ml_assign_params( L, &params, 1 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_genre_list_t *list = vlc_ml_list_genres( ml, &params );
    if ( list == NULL )
        return luaL_error( L, "Failed to list genres" );
    lua_createtable( L, list->i_nb_items, 0 );
    for ( size_t i = 0; i < list->i_nb_items; ++i )
    {
        vlclua_ml_push_genre( L, &list->p_items[i] );
        lua_rawseti( L, -2, i + 1 );
    }
    vlc_ml_release( list );
    return 1;
}

static int vlclua_ml_get_genre( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    lua_Integer genreId = luaL_checkinteger( L, 1 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_genre_t *genre = vlc_ml_get_genre( ml, genreId );
    if ( genre == NULL )
        return luaL_error( L, "Failed to get genre" );
    vlclua_ml_push_genre( L, genre );
    vlc_ml_release( genre );
    return 1;
}

static int vlclua_ml_list_playlists( lua_State *L, vlc_ml_playlist_list_t *list )
{
    if ( list == NULL )
        return luaL_error( L, "Failed to list playlists" );
    lua_createtable( L, list->i_nb_items, 0 );
    for ( size_t i = 0; i < list->i_nb_items; ++i )
    {
        vlclua_ml_push_playlist( L, &list->p_items[i] );
        lua_rawseti( L, -2, i + 1 );
    }
    vlc_ml_release( list );
    return 1;
}

static int vlclua_ml_list_all_playlists( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_ml_query_params_t params;
    vlclua_ml_assign_params( L, &params, 1 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_playlist_list_t *list = vlc_ml_list_playlists( ml, &params, true );
    return vlclua_ml_list_playlists( L, list );
}

static int vlclua_ml_get_playlist( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    lua_Integer playlistId = luaL_checkinteger( L, 1 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_playlist_t *playlist = vlc_ml_get_playlist( ml, playlistId );
    if ( playlist == NULL )
        return luaL_error( L, "Failed to get playlist" );
    vlclua_ml_push_playlist( L, playlist );
    vlc_ml_release( playlist );
    return 1;
}

static int vlclua_ml_get_media_thumbnail( lua_State *L )
{
    if( lua_gettop( L ) < 1 ) return vlclua_error( L );

    lua_Integer mediaId = luaL_checkinteger( L, 1 );
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_media_t *media = vlc_ml_get_media( ml, mediaId );
    if ( media == NULL ||
         media->thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl == NULL )
    {
        vlc_ml_release( media );
        return 0;
    }
    lua_pushstring( L, media->thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl );
    vlc_ml_release( media );
    return 1;
}

static int vlclua_ml_get_artist_thumbnail( lua_State *L )
{
    if( lua_gettop( L ) < 1 ) return vlclua_error( L );

    lua_Integer artistId = luaL_checkinteger( L, 1 );
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_artist_t *artist = vlc_ml_get_artist( ml, artistId );
    if ( artist == NULL ||
        artist->thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl == NULL )
    {
        vlc_ml_release( artist );
        return 0;
    }
    lua_pushstring( L, artist->thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl );
    vlc_ml_release( artist );
    return 1;
}

static int vlclua_ml_get_album_thumbnail( lua_State *L )
{
    if( lua_gettop( L ) < 1 ) return vlclua_error( L );

    lua_Integer albumId = luaL_checkinteger( L, 1 );
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_album_t *album = vlc_ml_get_album( ml, albumId );
    if ( album == NULL ||
         album->thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl == NULL )
    {
        vlc_ml_release( album );
        return 0;
    }
    lua_pushstring( L, album->thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl );
    vlc_ml_release( album );
    return 1;
}

static int vlclua_ml_get_genre_thumbnail( lua_State *L )
{
    if( lua_gettop( L ) < 1 ) return vlclua_error( L );

    lua_Integer genreId = luaL_checkinteger( L, 1 );
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_genre_t *genre = vlc_ml_get_genre( ml, genreId );
    if ( genre == NULL ||
         genre->thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl == NULL )
    {
        vlc_ml_release( genre );
        return 0;
    }
    lua_pushstring( L, genre->thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl );
    vlc_ml_release( genre );
    return 1;
}

static int vlclua_ml_list_album_tracks( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_query_params_t params;
    vlclua_ml_assign_params( L, &params, 2 );
    lua_Integer albumId = luaL_checkinteger( L, 1 );
    vlc_ml_media_list_t *list = vlc_ml_list_album_tracks( ml, &params, albumId );
    return vlclua_ml_list_media( L, list );
}

static int vlclua_ml_list_artist_tracks( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_query_params_t params;
    vlclua_ml_assign_params( L, &params, 2 );
    lua_Integer artistId = luaL_checkinteger( L, 1 );
    vlc_ml_media_list_t *list = vlc_ml_list_artist_tracks( ml, &params, artistId );
    return vlclua_ml_list_media( L, list );
}

static int vlclua_ml_list_genre_tracks( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_query_params_t params;
    vlclua_ml_assign_params( L, &params, 2 );
    lua_Integer genreId = luaL_checkinteger( L, 1 );
    vlc_ml_media_list_t *list = vlc_ml_list_genre_tracks( ml, &params, genreId );
    return vlclua_ml_list_media( L, list );
}

static int vlclua_ml_list_playlist_tracks( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_query_params_t params;
    vlclua_ml_assign_params( L, &params, 2 );
    lua_Integer playlistId = luaL_checkinteger( L, 1 );
    vlc_ml_media_list_t *list = vlc_ml_list_playlist_media( ml, &params, playlistId );
    return vlclua_ml_list_media( L, list );
}

static int vlclua_ml_reload( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_reload_folder( ml, NULL );
    return 0;
}

static int vlclua_ml_history( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_ml_query_params_t params;
    vlclua_ml_assign_params( L, &params, 1 );
    vlc_ml_history_type_t historyType = luaL_checkinteger( L, 2 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_media_list_t *list = vlc_ml_list_history( ml, &params, historyType );
    return vlclua_ml_list_media( L, list );
}

static int vlclua_ml_video_history( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_ml_query_params_t params;
    vlclua_ml_assign_params( L, &params, 1 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_media_list_t *list = vlc_ml_list_video_history( ml, &params );
    return vlclua_ml_list_media( L, list );
}

static int vlclua_ml_audio_history( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_ml_query_params_t params;
    vlclua_ml_assign_params( L, &params, 1 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_media_list_t *list = vlc_ml_list_audio_history( ml, &params );
    return vlclua_ml_list_media( L, list );
}

static int vlclua_media_bookmarks( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_ml_query_params_t params;
    vlclua_ml_assign_params( L, &params, 1 );
    int64_t mediaId = luaL_checkinteger( L, 2 );
    vlc_medialibrary_t *ml = vlc_ml_instance_get( p_this );
    vlc_ml_bookmark_list_t *list = vlc_ml_list_media_bookmarks( ml, &params, mediaId );
    return vlclua_ml_list_bookmark( L, list );
}



static const luaL_Reg vlclua_ml_reg[] = {
    { "video", vlclua_ml_video },
    { "media", vlclua_ml_get_media },
    { "media_by_mrl", vlclua_ml_get_media_by_mrl },
    { "folder", vlclua_ml_folder },
    { "folders_by_type", vlclua_ml_folders_by_type },
    { "get_folder", vlclua_ml_get_folder },
    { "show_episodes", vlclua_ml_list_shows },
    { "audio", vlclua_ml_audio },
    { "media_thumbnail", vlclua_ml_get_media_thumbnail },
    { "albums", vlclua_ml_list_all_albums },
    { "album", vlclua_ml_get_album },
    { "artists", vlclua_ml_list_all_artists },
    { "artist", vlclua_ml_get_artist },
    { "genres", vlclua_ml_list_genres },
    { "genre", vlclua_ml_get_genre },
    { "playlists", vlclua_ml_list_all_playlists },
    { "playlist", vlclua_ml_get_playlist },
    { "album_tracks", vlclua_ml_list_album_tracks },
    { "artist_tracks", vlclua_ml_list_artist_tracks },
    { "genre_tracks", vlclua_ml_list_genre_tracks },
    { "playlist_tracks", vlclua_ml_list_playlist_tracks },
    { "artist_albums", vlclua_ml_list_artist_albums },
    { "genre_albums", vlclua_ml_list_genre_albums },
    { "genre_artists", vlclua_ml_list_genre_artists },
    { "artist_thumbnail", vlclua_ml_get_artist_thumbnail },
    { "album_thumbnail", vlclua_ml_get_album_thumbnail },
    { "genre_thumbnail", vlclua_ml_get_genre_thumbnail },
    { "reload", vlclua_ml_reload },
    { "history", vlclua_ml_history },
    { "video_history",vlclua_ml_video_history },
    { "audio_history",vlclua_ml_audio_history },
    { "media_bookmarks", vlclua_media_bookmarks },
    { NULL, NULL }
};

void luaopen_ml( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_ml_reg );
    lua_setfield( L, -2, "ml" );
}
