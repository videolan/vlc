/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef PLAYLIST_CONTROLLER_P_HPP
#define PLAYLIST_CONTROLLER_P_HPP

#include "playlist_controller.hpp"

#include "qt.hpp" // for qtr()

namespace vlc {
namespace playlist {

class PlaylistControllerPrivate
{
    Q_DISABLE_COPY(PlaylistControllerPrivate)
public:
    Q_DECLARE_PUBLIC(PlaylistController)
    PlaylistController * const q_ptr;

public:
    PlaylistControllerPrivate(PlaylistController* playlistController);
    PlaylistControllerPrivate() = delete;
    ~PlaylistControllerPrivate();

    ///call function @a fun on object thread
    template <typename Fun>
    inline void callAsync(Fun&& fun)
    {
        Q_Q(PlaylistController);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        QMetaObject::invokeMethod(q, [fun = std::forward<Fun>(fun)]() -> void* { fun(); return nullptr; }, Qt::QueuedConnection);
#else
        QMetaObject::invokeMethod(q, std::forward<Fun>(fun), Qt::QueuedConnection, nullptr);
#endif
    }

    //playlist
    vlc_playlist_t* m_playlist = nullptr;
    vlc_playlist_listener_id* m_listener = nullptr;

    ssize_t m_currentIndex = -1;
    PlaylistItem m_currentItem;
    bool m_hasNext= false;
    bool m_hasPrev = false;
    PlaylistController::PlaybackRepeat m_repeat = PlaylistController::PLAYBACK_REPEAT_NONE;
    bool m_random = false;
    PlaylistController::MediaStopAction m_mediaStopAction = PlaylistController::MEDIA_STOPPED_CONTINUE;
    bool m_empty = true;
    size_t m_count = 0;
    PlaylistController::SortKey m_sortKey = PlaylistController::SORT_KEY_NONE;
    PlaylistController::SortOrder m_sortOrder = PlaylistController::SORT_ORDER_ASC;

    QVariantList sortKeyTitleList;

private:
    inline void fillSortKeyTitleList()
    {
        auto filler = [this](PlaylistController::SortKey key, const QString& text) {
            QVariantMap map;
            map.insert("criteria", key);
            map.insert("text", text);
            sortKeyTitleList.push_back(map);
        };

        filler(PlaylistController::SORT_KEY_TITLE, qtr("Title"));
        filler(PlaylistController::SORT_KEY_DURATION, qtr("Duration"));
        filler(PlaylistController::SORT_KEY_ARTIST, qtr("Artist"));
        filler(PlaylistController::SORT_KEY_ALBUM, qtr("Album"));
        filler(PlaylistController::SORT_KEY_ALBUM_ARTIST, qtr( "Album Artist"));
        filler(PlaylistController::SORT_KEY_GENRE, qtr("Genre"));
        filler(PlaylistController::SORT_KEY_DATE, qtr("Date"));
        filler(PlaylistController::SORT_KEY_TRACK_NUMBER, qtr( "Track Number"));
        filler(PlaylistController::SORT_KEY_DISC_NUMBER, qtr( "Disc Number"));
        filler(PlaylistController::SORT_KEY_URL, qtr("URL"));
        filler(PlaylistController::SORT_KEY_RATING, qtr("Rating"));
        filler(PlaylistController::SORT_KEY_FILE_SIZE, qtr("File size"));
        filler(PlaylistController::SORT_KEY_FILE_MODIFIED, qtr("File modified"));
    }
};

} //namespace playlist
} //namespace vlc

#endif // PLAYLIST_CONTROLLER_P_HPP
