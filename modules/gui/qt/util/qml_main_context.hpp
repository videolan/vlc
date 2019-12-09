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
#ifndef QML_MAIN_CONTEXT_HPP
#define QML_MAIN_CONTEXT_HPP

#include "qt.hpp"

#include <QObject>
#include <playlist/playlist_common.hpp>

class MainInterface;
/**
 * @brief The QmlMainContext class
 */
class QmlMainContext : public QObject
{
    Q_OBJECT
    Q_PROPERTY(PlaylistPtr playlist READ getPlaylist CONSTANT)

public:
    explicit QmlMainContext(intf_thread_t *intf,  MainInterface *mainInterface, QObject* parent = nullptr);

    MainInterface* getMainInterface() const;
    intf_thread_t* getIntf() const;
    PlaylistPtr getPlaylist() const;

private:
    intf_thread_t* m_intf;
    PlaylistPtr m_playlist;
    MainInterface* m_mainInterface;
};

#endif // QML_MAIN_CONTEXT_HPP
