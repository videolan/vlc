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
import QtQuick.Layouts 1.11

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"


RowLayout {
    id: rowLayout

    readonly property ColorContext colorContext:  ColorContext {
        colorSet: ColorContext.Window
    }

    height: VLCStyle.heightBar_normal
    spacing: VLCStyle.margin_normal


    Item {
        Layout.fillWidth: true
        implicitHeight: loop.height

        Widgets.IconToolButton {
            id: loop

            anchors.centerIn: parent

            size: VLCStyle.icon_playlist
            iconText: (mainPlaylistController.repeatMode === PlaylistControllerModel.PLAYBACK_REPEAT_CURRENT)
                      ? VLCIcons.repeat_one
                      : VLCIcons.repeat_all
            checked: mainPlaylistController.repeatMode !== PlaylistControllerModel.PLAYBACK_REPEAT_NONE
            onClicked: mainPlaylistController.toggleRepeatMode()
            focusPolicy: Qt.NoFocus
        }
    }


    Item {
        Layout.fillWidth: true
        implicitHeight: shuffle.height

        Widgets.IconToolButton {
            id: shuffle

            anchors.centerIn: parent

            checked: mainPlaylistController.random
            size: VLCStyle.icon_playlist
            iconText: VLCIcons.shuffle_on
            onClicked: mainPlaylistController.toggleRandom()
            focusPolicy: Qt.NoFocus
        }
    }

    Item {
        Layout.fillWidth: true
        implicitHeight: sort.height

        Widgets.SortControl {
            id: sort

            anchors.centerIn: parent

            iconSize: VLCStyle.icon_playlist

            enabled: mainPlaylistController.count > 1

            checked: mainPlaylistController.sortKey !== PlaylistControllerModel.SORT_KEY_NONE

            popupAbove: true

            focusPolicy: Qt.NoFocus

            model: mainPlaylistController.sortKeyTitleList
            textRole: "title"
            criteriaRole: "key"

            onSortSelected: {
                mainPlaylistController.sortKey = key
            }

            onSortOrderSelected: {
                if (type === Qt.AscendingOrder)
                    mainPlaylistController.sortOrder = PlaylistControllerModel.SORT_ORDER_ASC
                else if (type === Qt.DescendingOrder)
                    mainPlaylistController.sortOrder = PlaylistControllerModel.SORT_ORDER_DESC

                mainPlaylistController.sort()
            }

            sortOrder: {
                if (mainPlaylistController.sortOrder === PlaylistControllerModel.SORT_ORDER_ASC) {
                    Qt.AscendingOrder
                }
                else if (mainPlaylistController.sortOrder === PlaylistControllerModel.SORT_ORDER_DESC) {
                    Qt.DescendingOrder
                }
            }

            sortKey: mainPlaylistController.sortKey
        }
    }

    Item {
        Layout.fillWidth: true
        implicitHeight: clear.height

        Widgets.IconToolButton {
            id: clear

            anchors.centerIn: parent

            size: VLCStyle.icon_playlist
            enabled: !mainPlaylistController.empty
            iconText: VLCIcons.playlist_clear
            onClicked: mainPlaylistController.clear()
            focusPolicy: Qt.NoFocus
        }
    }
}
