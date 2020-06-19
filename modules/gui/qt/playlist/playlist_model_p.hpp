/*****************************************************************************
 * playlist_model_p.hpp
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
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
#ifndef PLAYLIST_MODEL_P_HPP
#define PLAYLIST_MODEL_P_HPP

#include "playlist_model.hpp"

namespace vlc {
namespace playlist {

class PlaylistListModelPrivate
{
    Q_DISABLE_COPY(PlaylistListModelPrivate)
public:
    Q_DECLARE_PUBLIC(PlaylistListModel)
    PlaylistListModel* const q_ptr;

public:
    PlaylistListModelPrivate(PlaylistListModel* playlistList);
    ~PlaylistListModelPrivate();

    ///call function @a fun on object thread
    template <typename Fun>
    inline void callAsync(Fun&& fun)
    {
        Q_Q(PlaylistListModel);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
        QMetaObject::invokeMethod(q, std::forward<Fun>(fun), Qt::QueuedConnection, nullptr);
#else
        QObject src;
        QObject::connect(&src, &QObject::destroyed, q, std::forward<Fun>(fun), Qt::QueuedConnection);
#endif
    }

    void onItemsReset(const QVector<PlaylistItem>& items);
    void onItemsAdded(const QVector<PlaylistItem>& added, size_t index);
    void onItemsMoved(size_t index, size_t count, size_t target);
    void onItemsRemoved(size_t index, size_t count);

    void notifyItemsChanged(int index, int count,
                            const QVector<int> &roles = {});

    vlc_playlist_t* m_playlist = nullptr;
    vlc_playlist_listener_id *m_listener = nullptr;

    /* access only from the UI thread */
    QVector<PlaylistItem> m_items;
    ssize_t m_current = -1;

    vlc_tick_t m_duration = 0;
};

} //namespace playlist
} //namespace vlc


#endif // PLAYLIST_MODEL_P_HPP
