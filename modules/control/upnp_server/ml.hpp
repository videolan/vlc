/*****************************************************************************
 * ml.hpp : C++ media library API wrapper
 *****************************************************************************
 * Copyright © 2021 VLC authors and VideoLAN
 *
 * Authors: Alaric Senat <alaric@videolabs.io>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef ML_HPP
#define ML_HPP

#include <functional>
#include <memory>
#include <optional>

#include <vlc_media_library.h>

namespace ml
{

struct MediaLibraryContext {
    vlc_medialibrary_t *handle;
    bool share_private_media;
};

static const vlc_ml_query_params_t PUBLIC_ONLY_QP = [] {
    vlc_ml_query_params_t params = vlc_ml_query_params_create();
    params.b_public_only = true;
    return params;
}();

static inline bool is_ml_object_private(const MediaLibraryContext &, vlc_ml_media_t *media)
{
    return !media->b_is_public;
}

static inline bool is_ml_object_private(const MediaLibraryContext &ml, vlc_ml_album_t *album)
{
    const size_t count = vlc_ml_count_album_tracks(ml.handle, &PUBLIC_ONLY_QP, album->i_id);
    return count == 0;
}

static inline bool is_ml_object_private(const MediaLibraryContext &ml, vlc_ml_playlist_t *playlist)
{
    const size_t count = vlc_ml_count_playlist_media(ml.handle, &PUBLIC_ONLY_QP, playlist->i_id);
    return count == 0;
}

static inline bool is_ml_object_private(const MediaLibraryContext &ml, vlc_ml_artist_t *artist)
{
    const size_t count = vlc_ml_count_artist_tracks(ml.handle, &PUBLIC_ONLY_QP, artist->i_id);
    return count == 0;
}

static inline bool is_ml_object_private(const MediaLibraryContext &ml, vlc_ml_genre_t *genre)
{
    const size_t count = vlc_ml_count_genre_tracks(ml.handle, &PUBLIC_ONLY_QP, genre->i_id);
    return count == 0;
}

static inline bool is_ml_object_private(const MediaLibraryContext &ml, vlc_ml_folder_t *folder)
{
    const size_t count = vlc_ml_count_folder_media(ml.handle, &PUBLIC_ONLY_QP, folder->i_id);
    return count == 0;
}

namespace errors {
struct ForbiddenAccess : public std::exception {
    virtual ~ForbiddenAccess() = default;
    virtual const char *what() const noexcept override {
        return "Private element";
    }
};

struct UnknownObject : public std::exception {
    virtual ~UnknownObject() = default;
    virtual const char *what() const noexcept override {
        return "Unknown element";
    }
};
}

template <typename MLObject, vlc_ml_get_queries GetQuery> struct Object
{
    using Ptr = std::unique_ptr<MLObject, std::function<void(MLObject *)>>;

    static Ptr get(const MediaLibraryContext &ml, const int64_t id)
    {
        MLObject *obj = static_cast<MLObject *>(vlc_ml_get(ml.handle, GetQuery, id));
        if (obj == nullptr)
            throw errors::UnknownObject();

        Ptr ptr{obj, static_cast<void (*)(MLObject *)>(&vlc_ml_release)};
        if (!ml.share_private_media && is_ml_object_private(ml, ptr.get()))
            throw errors::ForbiddenAccess();
        return ptr;
    }
};

using Media = Object<vlc_ml_media_t, VLC_ML_GET_MEDIA>;
using Album = Object<vlc_ml_album_t, VLC_ML_GET_ALBUM>;
using Playlist = Object<vlc_ml_playlist_t, VLC_ML_GET_PLAYLIST>;
using Artist = Object<vlc_ml_artist_t, VLC_ML_GET_ARTIST>;
using Genre = Object<vlc_ml_genre_t, VLC_ML_GET_GENRE>;
using Folder = Object<vlc_ml_folder_t, VLC_ML_GET_FOLDER>;

template <typename ListType,
          vlc_ml_list_queries ListQuery,
          vlc_ml_list_queries CountQuery,
          typename Object>
struct List : Object
{
    static size_t count(const MediaLibraryContext &ml, std::optional<int64_t> id) noexcept
    {
        size_t res;
        int status;

        vlc_ml_query_params_t params = vlc_ml_query_params_create();
        params.b_public_only = !ml.share_private_media;

        if (id.has_value())
            status = vlc_ml_list(ml.handle, CountQuery, &params, *id, &res);
        else if (CountQuery == VLC_ML_COUNT_ARTISTS)
            status = vlc_ml_list(ml.handle, CountQuery, &params, (int)false, &res);
        else if (CountQuery == VLC_ML_COUNT_ENTRY_POINTS)
            status = vlc_ml_list(ml.handle, CountQuery, &params, (int)false, &res);
        else if (CountQuery == VLC_ML_COUNT_PLAYLISTS)
            status = vlc_ml_list(ml.handle, CountQuery, &params, VLC_ML_PLAYLIST_TYPE_ALL, &res);
        else
            status = vlc_ml_list(ml.handle, CountQuery, &params, &res);
        return status == VLC_SUCCESS ? res : 0;
    }

    using Ptr = std::unique_ptr<ListType, std::function<void(ListType *)>>;

    static Ptr list(const MediaLibraryContext &ml,
                    const vlc_ml_query_params_t *params,
                    const std::optional<int64_t> id) noexcept
    {
        ListType *res;
        int status;

        vlc_ml_query_params_t extra_params = *params;
        extra_params.b_public_only = !ml.share_private_media;

        if (id.has_value())
            status = vlc_ml_list(ml.handle, ListQuery, &extra_params, *id, &res);
        else if (ListQuery == VLC_ML_LIST_ARTISTS)
            status = vlc_ml_list(ml.handle, ListQuery, &extra_params, (int)false, &res);
        else if (ListQuery == VLC_ML_LIST_ENTRY_POINTS)
            status = vlc_ml_list(ml.handle, ListQuery, &extra_params, (int)false, &res);
        else if (ListQuery == VLC_ML_LIST_PLAYLISTS)
            status = vlc_ml_list(ml.handle, ListQuery, &extra_params, VLC_ML_PLAYLIST_TYPE_ALL, &res);
        else
            status = vlc_ml_list(ml.handle, ListQuery, &extra_params, &res);
        return {status == VLC_SUCCESS ? res : nullptr,
                static_cast<void (*)(ListType *)>(&vlc_ml_release)};
    }
};

using AllAudio = List<vlc_ml_media_list_t, VLC_ML_LIST_AUDIOS, VLC_ML_COUNT_AUDIOS, Media>;
using AllVideos = List<vlc_ml_media_list_t, VLC_ML_LIST_VIDEOS, VLC_ML_COUNT_VIDEOS, Media>;
using AllAlbums = List<vlc_ml_album_list_t, VLC_ML_LIST_ALBUMS, VLC_ML_COUNT_ALBUMS, Media>;
using AllEntryPoints =
    List<vlc_ml_folder_list_t, VLC_ML_LIST_ENTRY_POINTS, VLC_ML_COUNT_ENTRY_POINTS, Folder>;

using AllArtistsList = List<vlc_ml_artist_list_t, VLC_ML_LIST_ARTISTS, VLC_ML_COUNT_ARTISTS, Media>;
using ArtistAlbumList =
    List<vlc_ml_album_list_t, VLC_ML_LIST_ARTIST_ALBUMS, VLC_ML_COUNT_ARTIST_ALBUMS, Album>;
using ArtistTracksList =
    List<vlc_ml_media_list_t, VLC_ML_LIST_ARTIST_TRACKS, VLC_ML_COUNT_ARTIST_TRACKS, Media>;
using AlbumTracksList =
    List<vlc_ml_media_list_t, VLC_ML_LIST_ALBUM_TRACKS, VLC_ML_COUNT_ALBUM_TRACKS, Album>;

using AllGenresList = List<vlc_ml_genre_list_t, VLC_ML_LIST_GENRES, VLC_ML_COUNT_GENRES, Media>;
using GenreAlbumList =
    List<vlc_ml_album_list_t, VLC_ML_LIST_GENRE_ALBUMS, VLC_ML_COUNT_GENRE_ALBUMS, Album>;
using GenreTracksList =
    List<vlc_ml_media_list_t, VLC_ML_LIST_GENRE_TRACKS, VLC_ML_COUNT_GENRE_TRACKS, Media>;

using PlaylistsList =
    List<vlc_ml_playlist_list_t, VLC_ML_LIST_PLAYLISTS, VLC_ML_COUNT_PLAYLISTS, Playlist>;
using PlaylistMediaList =
    List<vlc_ml_media_list_t, VLC_ML_LIST_PLAYLIST_MEDIA, VLC_ML_COUNT_PLAYLIST_MEDIA, Media>;

using MediaFolderList =
    List<vlc_ml_media_list_t, VLC_ML_LIST_FOLDER_MEDIA, VLC_ML_COUNT_FOLDER_MEDIA, Media>;
using SubfoldersList =
    List<vlc_ml_folder_list_t, VLC_ML_LIST_SUBFOLDERS, VLC_ML_COUNT_SUBFOLDERS, Folder>;

} // namespace ml

#endif /* ML_HPP */
