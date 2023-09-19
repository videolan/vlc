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
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"


RowLayout {
    id: rowLayout

    readonly property ColorContext colorContext:  ColorContext {
        colorSet: ColorContext.Window
    }

    spacing: VLCStyle.margin_normal

    Accessible.role: Accessible.ToolBar

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        implicitWidth: loop.implicitWidth
        implicitHeight: loop.implicitHeight

        Widgets.IconToolButton {
            id: loop

            anchors.centerIn: parent

            font.pixelSize: VLCStyle.icon_playlist
            description: qsTr("Loop")
            text: (MainPlaylistController.repeatMode === PlaylistController.PLAYBACK_REPEAT_CURRENT)
                      ? VLCIcons.repeat_one
                      : VLCIcons.repeat_all
            checked: MainPlaylistController.repeatMode !== PlaylistController.PLAYBACK_REPEAT_NONE
            onClicked: MainPlaylistController.toggleRepeatMode()
            focusPolicy: Qt.NoFocus
        }
    }


    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        implicitWidth: shuffle.implicitWidth
        implicitHeight: shuffle.implicitHeight

        Widgets.IconToolButton {
            id: shuffle

            anchors.centerIn: parent

            checked: MainPlaylistController.random
            font.pixelSize: VLCStyle.icon_playlist
            Accessible.name: qsTr("Shuffle")
            text: VLCIcons.shuffle
            onClicked: MainPlaylistController.toggleRandom()
            focusPolicy: Qt.NoFocus
        }
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        implicitWidth: sort.implicitWidth
        implicitHeight: sort.implicitHeight

        Widgets.SortControl {
            id: sort

            anchors.centerIn: parent

            font.pixelSize: VLCStyle.icon_playlist

            enabled: MainPlaylistController.count > 1

            checked: MainPlaylistController.sortKey !== PlaylistController.SORT_KEY_NONE

            popupAbove: true

            focusPolicy: Qt.NoFocus

            model: MainPlaylistController.sortKeyTitleList

            onSortSelected: {
                MainPlaylistController.sortKey = key
            }

            onSortOrderSelected: {
                if (type === Qt.AscendingOrder)
                    MainPlaylistController.sortOrder = PlaylistController.SORT_ORDER_ASC
                else if (type === Qt.DescendingOrder)
                    MainPlaylistController.sortOrder = PlaylistController.SORT_ORDER_DESC

                MainPlaylistController.sort()
            }

            sortOrder: {
                if (MainPlaylistController.sortOrder === PlaylistController.SORT_ORDER_ASC) {
                    Qt.AscendingOrder
                }
                else if (MainPlaylistController.sortOrder === PlaylistController.SORT_ORDER_DESC) {
                    Qt.DescendingOrder
                }
            }

            sortKey: MainPlaylistController.sortKey
        }
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        implicitWidth: clear.implicitWidth
        implicitHeight: clear.implicitHeight

        Widgets.IconToolButton {
            id: clear

            anchors.centerIn: parent

            font.pixelSize: VLCStyle.icon_playlist
            enabled: !MainPlaylistController.empty
            description: qsTr("Clear playqueue")
            text: VLCIcons.playlist_clear
            onClicked: MainPlaylistController.clear()
            focusPolicy: Qt.NoFocus
        }
    }
}
