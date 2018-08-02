/*****************************************************************************
 * medialibrary.h: medialibrary module common declarations
 *****************************************************************************
 * Copyright © 2015-2016 VLC authors, VideoLAN and VideoLabs
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

#ifndef MEDIALIBRARY_H
#define MEDIALIBRARY_H

#include <medialibrary/IMediaLibrary.h>
#include <medialibrary/parser/IParserService.h>
#include <medialibrary/parser/IItem.h>
#include <medialibrary/parser/Parser.h>
#include <medialibrary/IMedia.h>

#include <vlc_common.h>
#include <vlc_threads.h>
#include <vlc_input_item.h>
#include <vlc_input.h>
#include <vlc_media_library.h>
#include <vlc_cxx_helpers.hpp>

#include <cstdarg>

struct vlc_event_t;
struct vlc_object_t;

class Logger;

class MetadataExtractor : public medialibrary::parser::IParserService
{
private:
    struct ParseContext
    {
        ParseContext( MetadataExtractor* mde, medialibrary::parser::IItem& item )
            : inputItem( nullptr, &input_item_Release )
            , input( nullptr, &input_Close )
            , needsProbing( false )
            , state( INIT_S )
            , mde( mde )
            , item( item )
        {
            vlc_mutex_init( &m_mutex );
            vlc_cond_init( &m_cond );
        }
        ~ParseContext()
        {
            vlc_cond_destroy( &m_cond );
            vlc_mutex_destroy( &m_mutex );
        }

        std::unique_ptr<input_item_t, decltype(&input_item_Release)> inputItem;
        std::unique_ptr<input_thread_t, decltype(&input_Close)> input;
        vlc_cond_t m_cond;
        vlc_mutex_t m_mutex;
        bool needsProbing;
        input_state_e state;
        MetadataExtractor* mde;
        medialibrary::parser::IItem& item;
    };

public:
    MetadataExtractor( vlc_object_t* parent );
    virtual ~MetadataExtractor() = default;

    // All methods are meant to be accessed through IParserService, not directly
    // hence they are all private
private:
    virtual medialibrary::parser::Status run( medialibrary::parser::IItem& item ) override;
    virtual const char*name() const override;
    virtual uint8_t nbThreads() const override;
    virtual medialibrary::parser::Step targetedStep() const override;
    virtual bool initialize( medialibrary::IMediaLibrary* ml ) override;
    virtual void onFlushing() override;
    virtual void onRestarted() override;

    void onInputEvent( const vlc_input_event* event, ParseContext& ctx );
    void onSubItemAdded( const vlc_event_t* event, ParseContext& ctx );
    void populateItem( medialibrary::parser::IItem& item, input_item_t* inputItem );

    static void onInputEvent( input_thread_t *input, void *user_data,
                               const struct vlc_input_event *event );
    static void onSubItemAdded( const vlc_event_t* event, void* data );

private:
    vlc_object_t* m_obj;
};

class MediaLibrary : public medialibrary::IMediaLibraryCb
{
public:
    MediaLibrary( vlc_medialibrary_t* ml );
    bool Start();
    int Control( int query, va_list args );
    int List( int query, const vlc_ml_query_params_t* params, va_list args );
    void* Get( int query, int64_t id );

private:
    int controlMedia( int query, va_list args );
    int getMeta( const medialibrary::IMedia& media, int meta, char** result );
    int setMeta( medialibrary::IMedia& media, int meta, const char* value );
    int filterListChildrenQuery( int query, int parentType );
    int listAlbums( int listQuery, const medialibrary::QueryParameters* paramsPtr,
                    const char* pattern, uint32_t nbItems, uint32_t offset, va_list args );
    int listArtists( int listQuery, const medialibrary::QueryParameters* paramsPtr,
                    const char* pattern, uint32_t nbItems, uint32_t offset, va_list args );
    int listGenre( int listQuery, const medialibrary::QueryParameters* paramsPtr,
                   const char* pattern, uint32_t nbItems, uint32_t offset, va_list args );
    int listPlaylist( int listQuery, const medialibrary::QueryParameters* paramsPtr,
                      const char* pattern, uint32_t nbItems, uint32_t offset, va_list args );

    static medialibrary::IMedia::MetadataType metadataType( int meta );
    static medialibrary::SortingCriteria sortingCriteria( int sort );

private:
    vlc_medialibrary_t* m_vlc_ml;
    std::unique_ptr<Logger> m_logger;
    std::unique_ptr<medialibrary::IMediaLibrary> m_ml;

    // IMediaLibraryCb interface
public:
    virtual void onMediaAdded(std::vector<medialibrary::MediaPtr> media) override;
    virtual void onMediaModified(std::vector<medialibrary::MediaPtr> media) override;
    virtual void onMediaDeleted(std::vector<int64_t> mediaIds) override;
    virtual void onArtistsAdded(std::vector<medialibrary::ArtistPtr> artists) override;
    virtual void onArtistsModified(std::vector<medialibrary::ArtistPtr> artists) override;
    virtual void onArtistsDeleted(std::vector<int64_t> artistsIds) override;
    virtual void onAlbumsAdded(std::vector<medialibrary::AlbumPtr> albums) override;
    virtual void onAlbumsModified(std::vector<medialibrary::AlbumPtr> albums) override;
    virtual void onAlbumsDeleted(std::vector<int64_t> albumsIds) override;
    virtual void onPlaylistsAdded(std::vector<medialibrary::PlaylistPtr> playlists) override;
    virtual void onPlaylistsModified(std::vector<medialibrary::PlaylistPtr> playlists) override;
    virtual void onPlaylistsDeleted(std::vector<int64_t> playlistIds) override;
    virtual void onGenresAdded(std::vector<medialibrary::GenrePtr> genres) override;
    virtual void onGenresModified(std::vector<medialibrary::GenrePtr> genres) override;
    virtual void onGenresDeleted(std::vector<int64_t> genreIds) override;
    virtual void onDiscoveryStarted(const std::string& entryPoint) override;
    virtual void onDiscoveryProgress(const std::string& entryPoint) override;
    virtual void onDiscoveryCompleted(const std::string& entryPoint, bool success) override;
    virtual void onReloadStarted(const std::string& entryPoint) override;
    virtual void onReloadCompleted(const std::string& entryPoint, bool success) override;
    virtual void onEntryPointRemoved(const std::string& entryPoint, bool success) override;
    virtual void onEntryPointBanned(const std::string& entryPoint, bool success) override;
    virtual void onEntryPointUnbanned(const std::string& entryPoint, bool success) override;
    virtual void onParsingStatsUpdated(uint32_t percent) override;
    virtual void onBackgroundTasksIdleChanged(bool isIdle) override;
    virtual void onMediaThumbnailReady(medialibrary::MediaPtr media, bool success) override;
};

bool Convert( const medialibrary::IMedia* input, vlc_ml_media_t& output );
bool Convert( const medialibrary::IFile* input, vlc_ml_file_t& output );
bool Convert( const medialibrary::IMovie* input, vlc_ml_movie_t& output );
bool Convert( const medialibrary::IShowEpisode* input, vlc_ml_show_episode_t& output );
bool Convert( const medialibrary::IAlbumTrack* input, vlc_ml_album_track_t& output );
bool Convert( const medialibrary::IAlbum* input, vlc_ml_album_t& output );
bool Convert( const medialibrary::IArtist* input, vlc_ml_artist_t& output );
bool Convert( const medialibrary::IGenre* input, vlc_ml_genre_t& output );
bool Convert( const medialibrary::IShow* input, vlc_ml_show_t& output );
bool Convert( const medialibrary::ILabel* input, vlc_ml_label_t& output );
bool Convert( const medialibrary::IPlaylist* input, vlc_ml_playlist_t& output );

template <typename To, typename ItemType, typename From>
To* ml_convert_list( const std::vector<std::shared_ptr<From>>& input )
{
    // This function uses duck typing and assumes all lists have a p_items member
    static_assert( std::is_pointer<To>::value == false,
                   "Destination type must not be a pointer" );
    // If decltype( To::p_items ) doesn't yield an array type, we can't deduce
    // the items type. however if it does, we can ensure To and ItemType are coherent
    static_assert( std::is_array<decltype(To::p_items)>::value == false ||
                    ( std::is_same<typename std::remove_extent<decltype(To::p_items)>::type,
                        ItemType>::value ), "Invalid/mismatching list/item types" );

    // Allocate the ml_*_list_t
    auto list = vlc::wrap_cptr(
        static_cast<To*>( malloc( sizeof( To ) + input.size() * sizeof( ItemType ) ) ),
        static_cast<void(*)(To*)>( &vlc_ml_release_obj ) );
    if ( unlikely( list == nullptr ) )
        return nullptr;

    list->i_nb_items = 0;

    for ( auto i = 0u; i < input.size(); ++i )
    {
         if ( Convert( input[i].get(), list->p_items[i] ) == false )
             return nullptr;
         list->i_nb_items++;
    }
    return list.release();
}

template <typename T, typename Input>
T* CreateAndConvert( const Input* input )
{
    if ( input == nullptr )
        return nullptr;
    auto res = vlc::wrap_cptr(
                static_cast<T*>( malloc( sizeof( T ) ) ),
                static_cast<void(*)(T*)>( &vlc_ml_release_obj ) );
    if ( unlikely( res == nullptr ) )
        return nullptr;
    if ( Convert( input, *res ) == false )
        return nullptr;
    // Override the pf_relase that each Convert<T> helper will assign.
    // The Convert function will use the ReleaseRef variant of the release function,
    // as it converts in place, and doesn't have to free the allocated pointer.
    // When CreateAndConvert is used, we heap-allocate an instance of T, and therefor
    // we also need to release it.
    return res.release();
}



#endif // MEDIALIBRARY_H
