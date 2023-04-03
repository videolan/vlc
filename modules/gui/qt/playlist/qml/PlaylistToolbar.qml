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
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

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

    Accessible.role: Accessible.ToolBar

    Item {
        Layout.fillWidth: true
        implicitHeight: loop.height

        Widgets.IconToolButton {
            id: loop

            anchors.centerIn: parent

            size: VLCStyle.icon_playlist
            text: I18n.qtr("Loop")
            iconText: (MainPlaylistController.repeatMode === PlaylistController.PLAYBACK_REPEAT_CURRENT)
                      ? VLCIcons.repeat_one
                      : VLCIcons.repeat_all
            checked: MainPlaylistController.repeatMode !== PlaylistController.PLAYBACK_REPEAT_NONE
            onClicked: MainPlaylistController.toggleRepeatMode()
            focusPolicy: Qt.NoFocus
        }
    }


    Item {
        Layout.fillWidth: true
        implicitHeight: shuffle.height

        Widgets.IconToolButton {
            id: shuffle

            anchors.centerIn: parent

            checked: MainPlaylistController.random
            size: VLCStyle.icon_playlist
            text: I18n.qtr("Shuffle")
            iconText: VLCIcons.shuffle
            onClicked: MainPlaylistController.toggleRandom()
            focusPolicy: Qt.NoFocus
        }
    }

    Item {
        Layout.fillWidth: true
        implicitHeight: sort.height

        Widgets.SortControl {
            id: sort

            anchors.centerIn: parent

            size: VLCStyle.icon_playlist

            enabled: MainPlaylistController.count > 1

            checked: MainPlaylistController.sortKey !== PlaylistController.SORT_KEY_NONE

            popupAbove: true

            focusPolicy: Qt.NoFocus

            model: MainPlaylistController.sortKeyTitleList
            textRole: "title"
            criteriaRole: "key"

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
        implicitHeight: clear.height

        Widgets.IconToolButton {
            id: clear

            anchors.centerIn: parent

            size: VLCStyle.icon_playlist
            enabled: !MainPlaylistController.empty
            text: I18n.qtr("Clear playqueue")
            iconText: VLCIcons.playlist_clear
            onClicked: MainPlaylistController.clear()
            focusPolicy: Qt.NoFocus
        }
    }
}
