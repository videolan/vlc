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

namespace vlc {
namespace playlist {

class PlaylistControllerModelPrivate
{
    Q_DISABLE_COPY(PlaylistControllerModelPrivate)
public:
    Q_DECLARE_PUBLIC(PlaylistControllerModel)
    PlaylistControllerModel * const q_ptr;

public:
    PlaylistControllerModelPrivate(PlaylistControllerModel* playlistController);
    PlaylistControllerModelPrivate() = delete;
    ~PlaylistControllerModelPrivate();

    ///call function @a fun on object thread
    template <typename Fun>
    inline void callAsync(Fun&& fun)
    {
        Q_Q(PlaylistControllerModel);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
        QMetaObject::invokeMethod(q, std::forward<Fun>(fun), Qt::QueuedConnection, nullptr);
#else
        QObject src;
        QObject::connect(&src, &QObject::destroyed, q, std::forward<Fun>(fun), Qt::QueuedConnection);
#endif
    }

    //playlist
    vlc_playlist_t* m_playlist = nullptr;
    vlc_playlist_listener_id* m_listener = nullptr;

    ssize_t m_currentIndex = -1;
    PlaylistItem m_currentItem;
    bool m_hasNext= false;
    bool m_hasPrev = false;
    PlaylistControllerModel::PlaybackRepeat m_repeat = PlaylistControllerModel::PLAYBACK_REPEAT_NONE;
    bool m_random = false;
    bool m_isPlayAndExit;
    bool m_empty = true;
    size_t m_count = 0;
};

} //namespace playlist
} //namespace vlc

#endif // PLAYLIST_CONTROLLER_P_HPP
