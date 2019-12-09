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

import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.3

import "qrc:///widgets/" as Widgets
import "qrc:///player/" as Player
import "qrc:///style/"
import "qrc:///menus/" as Menus

/**
 * lightweight standalone playlist view, used when medialibrary is not loaded
 */
Widgets.NavigableFocusScope {
    id: root
    Rectangle {
        anchors.fill: parent
        color: VLCStyle.colors.bg

        ColumnLayout {
            anchors.fill: parent

            Menus.MainMenubar {
                Layout.fillWidth: true
            }

            PlaylistListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            Player.MiniPlayer {
                Layout.fillWidth: true
            }
        }

    }
}
