/*****************************************************************************
 * entities.cpp: medialibrary C++ -> C entities conversion & management
 *****************************************************************************
 * Copyright © 2008-2018 VLC authors, VideoLAN and VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "medialibrary.h"

#include <medialibrary/IMedia.h>
#include <medialibrary/IFile.h>
#include <medialibrary/IMovie.h>
#include <medialibrary/IShow.h>
#include <medialibrary/IShowEpisode.h>
#include <medialibrary/IArtist.h>
#include <medialibrary/IAlbum.h>
#include <medialibrary/IAlbumTrack.h>
#include <medialibrary/IGenre.h>
#include <medialibrary/ILabel.h>
#include <medialibrary/IPlaylist.h>
#include <medialibrary/IAudioTrack.h>
#include <medialibrary/IVideoTrack.h>

bool Convert( const medialibrary::IAlbumTrack* input, vlc_ml_album_track_t& output )
{
    output.i_artist_id = input->artistId();
    output.i_album_id = input->albumId();
    output.i_disc_nb = input->discNumber();
    output.i_genre_id = input->genreId();
    output.i_track_nb = input->trackNumber();
    return true;
}

bool Convert( const medialibrary::IShowEpisode* input, vlc_ml_show_episode_t& output )
{
    output.i_episode_nb = input->episodeNumber();
    output.i_season_number = input->seasonNumber();
    if ( input->shortSummary().empty() == false )
    {
        output.psz_summary = strdup( input->shortSummary().c_str() );
        if ( unlikely( output.psz_summary == nullptr ) )
            return false;
    }
    else
        output.psz_summary = nullptr;
    if ( input->tvdbId().empty() == false )
    {
        output.psz_tvdb_id = strdup( input->tvdbId().c_str() );
        if ( unlikely( output.psz_tvdb_id == nullptr ) )
            return false;
    }
    else
        output.psz_tvdb_id = nullptr;
    return true;
}

bool Convert( const medialibrary::IMovie* input, vlc_ml_movie_t& output )
{
    if ( input->imdbId().empty() == false )
    {
        output.psz_imdb_id = strdup( input->imdbId().c_str() );
        if ( unlikely( output.psz_imdb_id == nullptr ) )
            return false;
    }
    else
        output.psz_imdb_id = nullptr;
    if ( input->shortSummary().empty() == false )
    {
        output.psz_summary = strdup( input->shortSummary().c_str() );
        if ( unlikely( output.psz_summary == nullptr ) )
            return false;
    }
    else
        output.psz_summary = nullptr;
    return true;
}

static bool convertTracksCommon( vlc_ml_media_track_t* output, const std::string& codec,
                                 const std::string& language, const std::string& desc )
{
    if ( codec.empty() == false )
    {
        output->psz_codec = strdup( codec.c_str() );
        if ( unlikely( output->psz_codec == nullptr ) )
            return false;
    }
    else
        output->psz_codec = nullptr;

    if ( language.empty() == false )
    {
        output->psz_language = strdup( language.c_str() );
        if ( unlikely( output->psz_language == nullptr ) )
            return false;
    }
    else
        output->psz_language = nullptr;

    if ( desc.empty() == false )
    {
        output->psz_description = strdup( desc.c_str() );
        if ( unlikely( output->psz_description == nullptr ) )
            return false;
    }
    else
        output->psz_description = nullptr;
    return true;
}

static bool convertTracks( const medialibrary::IMedia* inputMedia, vlc_ml_media_t& outputMedia )
{
    auto videoTracks = inputMedia->videoTracks()->all();
    auto audioTracks = inputMedia->audioTracks()->all();
    auto nbItems = videoTracks.size() + audioTracks.size();
    outputMedia.p_tracks = static_cast<vlc_ml_media_track_list_t*>(
                calloc( 1, sizeof( *outputMedia.p_tracks ) +
                        nbItems * sizeof( *outputMedia.p_tracks->p_items ) ) );
    if ( unlikely( outputMedia.p_tracks == nullptr ) )
        return false;
    outputMedia.p_tracks->i_nb_items = 0;

    vlc_ml_media_track_t* items = outputMedia.p_tracks->p_items;
    for ( const auto& t : videoTracks )
    {
        vlc_ml_media_track_t* output = &items[outputMedia.p_tracks->i_nb_items++];

        if ( convertTracksCommon( output, t->codec(), t->language(), t->description() ) == false )
            return false;
        output->i_type = VLC_ML_TRACK_TYPE_VIDEO;
        output->i_bitrate = t->bitrate();
        output->v.i_fpsNum = t->fpsNum();
        output->v.i_fpsDen = t->fpsDen();
        output->v.i_sarNum = t->sarNum();
        output->v.i_sarDen = t->sarDen();
    }
    for ( const auto& t : audioTracks )
    {
        vlc_ml_media_track_t* output = &items[outputMedia.p_tracks->i_nb_items++];

        if ( convertTracksCommon( output, t->codec(), t->language(), t->description() ) == false )
            return false;
        output->i_type = VLC_ML_TRACK_TYPE_AUDIO;
        output->i_bitrate = t->bitrate();
        output->a.i_nbChannels = t->nbChannels();
        output->a.i_sampleRate = t->sampleRate();
    }
    return true;
}

bool Convert( const medialibrary::IMedia* input, vlc_ml_media_t& output )
{
    output.i_id = input->id();

    switch ( input->type() )
    {
        case medialibrary::IMedia::Type::Audio:
            output.i_type = VLC_ML_MEDIA_TYPE_AUDIO;
            switch( input->subType() )
            {
                case medialibrary::IMedia::SubType::AlbumTrack:
                {
                    output.i_subtype = VLC_ML_MEDIA_SUBTYPE_ALBUMTRACK;
                    auto albumTrack = input->albumTrack();
                    if ( albumTrack == nullptr )
                        return false;
                    if ( Convert( albumTrack.get(), output.album_track ) == false )
                        return false;
                    break;
                }
                default:
                    vlc_assert_unreachable();
            }
            break;
        case medialibrary::IMedia::Type::Video:
        {
            output.i_type = VLC_ML_MEDIA_TYPE_VIDEO;
            switch( input->subType() )
            {
                case medialibrary::IMedia::SubType::Movie:
                {
                    output.i_subtype = VLC_ML_MEDIA_SUBTYPE_MOVIE;
                    auto movie = input->movie();
                    if ( movie == nullptr )
                        return false;
                    if ( Convert( movie.get(), output.movie ) == false )
                        return false;
                    break;
                }
                case medialibrary::IMedia::SubType::ShowEpisode:
                {
                    output.i_subtype = VLC_ML_MEDIA_SUBTYPE_SHOW_EPISODE;
                    auto episode = input->showEpisode();
                    if ( episode == nullptr )
                        return false;
                    if ( Convert( episode.get(), output.show_episode ) == false )
                        return false;
                    break;
                }
                case medialibrary::IMedia::SubType::Unknown:
                    output.i_subtype = VLC_ML_MEDIA_SUBTYPE_UNKNOWN;
                    break;
                case medialibrary::IMedia::SubType::AlbumTrack:
                    vlc_assert_unreachable();
            }
            break;
        }
        case medialibrary::IMedia::Type::External:
            output.i_type = VLC_ML_MEDIA_TYPE_EXTERNAL;
            break;
        case medialibrary::IMedia::Type::Stream:
            output.i_type = VLC_ML_MEDIA_TYPE_STREAM;
            break;
        case medialibrary::IMedia::Type::Unknown:
            vlc_assert_unreachable();
    }
    output.i_year = input->releaseDate();
    output.i_duration = input->duration();
    output.b_is_favorite = input->isFavorite();
    output.i_playcount = input->playCount();
    output.i_last_played_date = input->lastPlayedDate();

    output.psz_title = strdup( input->title().c_str() );
    if ( unlikely( output.psz_title == nullptr ) )
        return false;

    auto files = input->files();
    output.p_files = ml_convert_list<vlc_ml_file_list_t>( files );
    if ( output.p_files == nullptr )
        return false;

    if ( convertTracks( input, output ) == false )
        return false;

    if ( input->isThumbnailGenerated() == true )
    {
        output.psz_artwork_mrl = strdup( input->thumbnail().c_str() );
        if ( unlikely( output.psz_artwork_mrl == nullptr ) )
            return false;
    }
    else
        output.psz_artwork_mrl = nullptr;

    return true;
}

bool Convert( const medialibrary::IFile* input, vlc_ml_file_t& output )
{
    switch ( input->type() )
    {
        case medialibrary::IFile::Type::Main:
            output.i_type = VLC_ML_FILE_TYPE_MAIN;
            break;
        case medialibrary::IFile::Type::Part:
            output.i_type = VLC_ML_FILE_TYPE_PART;
            break;
        case medialibrary::IFile::Type::Soundtrack:
            output.i_type = VLC_ML_FILE_TYPE_SOUNDTRACK;
            break;
        case medialibrary::IFile::Type::Subtitles:
            output.i_type = VLC_ML_FILE_TYPE_SUBTITLE;
            break;
        case medialibrary::IFile::Type::Playlist:
            output.i_type = VLC_ML_FILE_TYPE_PLAYLIST;
            break;
        default:
            vlc_assert_unreachable();
    }
    output.psz_mrl = strdup( input->mrl().c_str() );
    if ( unlikely( output.psz_mrl == nullptr ) )
        return false;
    output.b_external = input->isExternal();
    return true;
}

bool Convert( const medialibrary::IAlbum* input, vlc_ml_album_t& output )
{
    output.i_id = input->id();
    output.i_nb_tracks = input->nbTracks();
    output.i_duration = input->duration();
    output.i_year = input->releaseYear();
    if ( input->title().empty() == false )
    {
        output.psz_title = strdup( input->title().c_str() );
        if ( unlikely( output.psz_title == nullptr ) )
            return false;
    }
    else
        output.psz_title = nullptr;
    if ( input->shortSummary().empty() == false )
    {
        output.psz_summary = strdup( input->shortSummary().c_str() );
        if ( unlikely( output.psz_summary == nullptr ) )
            return false;
    }
    else
        output.psz_summary = nullptr;
    if ( input->artworkMrl().empty() == false )
    {
        output.psz_artwork_mrl = strdup( input->artworkMrl().c_str() );
        if ( unlikely( output.psz_artwork_mrl == nullptr ) )
            return false;
    }
    else
        output.psz_artwork_mrl = nullptr;
    auto artist = input->albumArtist();
    if ( artist != nullptr )
    {
        output.i_artist_id = artist->id();
        switch ( artist->id() )
        {
            case medialibrary::UnknownArtistID:
                output.psz_artist = strdup( _( "Unknown Artist" ) );
                break;
            case medialibrary::VariousArtistID:
                output.psz_artist = strdup( _( "Various Artist" ) );
                break;
            default:
                output.psz_artist = strdup( artist->name().c_str() );
                break;
        }
        if ( unlikely( output.psz_artist == nullptr ) )
            return false;
    }
    return true;
}

bool Convert( const medialibrary::IArtist* input, vlc_ml_artist_t& output )
{
    output.i_id = input->id();
    output.i_nb_album = input->nbAlbums();
    output.i_nb_tracks = input->nbTracks();
    switch ( input->id() )
    {
        case medialibrary::UnknownArtistID:
            output.psz_name = strdup( _( "Unknown Artist" ) );
            break;
        case medialibrary::VariousArtistID:
            output.psz_name = strdup( _( "Various Artist" ) );
            break;
        default:
            output.psz_name = strdup( input->name().c_str() );
            break;
    }
    if ( unlikely( output.psz_name == nullptr ) )
        return false;
    if ( input->shortBio().empty() == false )
    {
        output.psz_shortbio = strdup( input->shortBio().c_str() );
        if ( unlikely( output.psz_shortbio == nullptr ) )
            return false;
    }
    else
        output.psz_shortbio = nullptr;
    if ( input->artworkMrl().empty() == false )
    {
        output.psz_artwork_mrl = strdup( input->artworkMrl().c_str() );
        if ( unlikely( output.psz_artwork_mrl == nullptr ) )
            return false;
    }
    else
        output.psz_artwork_mrl = nullptr;
    if ( input->musicBrainzId().empty() == false )
    {
        output.psz_mb_id = strdup( input->musicBrainzId().c_str() );
        if ( unlikely( output.psz_mb_id == nullptr ) )
            return false;
    }
    else
        output.psz_mb_id = nullptr;
    return true;
}

void Release( vlc_ml_genre_t& genre )
{
    free( genre.psz_name );
}

bool Convert( const medialibrary::IGenre* input, vlc_ml_genre_t& output )
{
    output.i_id = input->id();
    output.i_nb_tracks = input->nbTracks();
    assert( input->name().empty() == false );
    output.psz_name = strdup( input->name().c_str() );
    if ( unlikely( output.psz_name == nullptr ) )
        return false;
    return true;
}

bool Convert( const medialibrary::IShow* input, vlc_ml_show_t& output )
{
    output.i_id = input->id();
    output.i_release_year = input->releaseDate();
    output.i_nb_episodes = input->nbEpisodes();
    output.i_nb_seasons = input->nbSeasons();
    if ( input->title().empty() == false )
    {
        output.psz_name = strdup( input->title().c_str() );
        if ( output.psz_name == nullptr )
            return false;
    }
    else
        output.psz_name = nullptr;
    if ( input->artworkMrl().empty() == false )
    {
        output.psz_artwork_mrl = strdup( input->artworkMrl().c_str() );
        if ( unlikely( output.psz_artwork_mrl == nullptr ) )
            return false;
    }
    else
        output.psz_artwork_mrl = nullptr;
    if ( input->tvdbId().empty() == false )
    {
        output.psz_tvdb_id = strdup( input->tvdbId().c_str() );
        if ( unlikely( output.psz_tvdb_id == nullptr ) )
            return false;
    }
    else
        output.psz_tvdb_id = nullptr;
    if ( input->shortSummary().empty() == false )
    {
        output.psz_summary = strdup( input->shortSummary().c_str() );
        if ( unlikely( output.psz_summary == nullptr ) )
            return false;
    }
    else
        output.psz_summary = nullptr;
    return true;
}

bool Convert( const medialibrary::ILabel* input, vlc_ml_label_t& output )
{
    assert( input->name().empty() == false );
    output.psz_name = strdup( input->name().c_str() );
    if ( unlikely( output.psz_name == nullptr ) )
        return false;
    return true;
}

bool Convert( const medialibrary::IPlaylist* input, vlc_ml_playlist_t& output )
{
    output.i_id = input->id();
    if ( input->name().empty() == false )
    {
        output.psz_name = strdup( input->name().c_str() );
        if ( unlikely( output.psz_name == nullptr ) )
            return false;
    }
    else
        output.psz_name = nullptr;
    if ( input->artworkMrl().empty() != false )
    {
        output.psz_artwork_mrl = strdup( input->artworkMrl().c_str() );
        if ( unlikely( output.psz_artwork_mrl == nullptr ) )
            return false;
    }
    else
        output.psz_artwork_mrl = nullptr;
    output.i_creation_date = input->creationDate();
    return true;
}
