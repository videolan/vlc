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
#ifndef PLAYLIST_COMMON_HPP
#define PLAYLIST_COMMON_HPP

#include <QObject>

extern "C" {
    typedef struct vlc_playlist vlc_playlist_t;
}

// QObject wrapper to carry playlist ptr through QML
class PlaylistPtr
{
    Q_GADGET
public:
    PlaylistPtr();
    PlaylistPtr(vlc_playlist_t* pl);
    PlaylistPtr(const PlaylistPtr& ptr);
    PlaylistPtr& operator=(const PlaylistPtr& ptr);

    vlc_playlist_t* m_playlist = nullptr;
};

class PlaylistLocker
{
public:
    PlaylistLocker(vlc_playlist_t* playlist);

    ~PlaylistLocker();

private:
    vlc_playlist_t* m_playlist;
};

#endif // PLAYLIST_COMMON_HPP
