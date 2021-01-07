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

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///util/KeyHelper.js" as KeyHelper
import "qrc:///style/"

Widgets.NavigableFocusScope {
    id: playlistToolbar

    property int leftPadding: 0
    property int rightPadding: 0
    height: VLCStyle.heightBar_normal

    property VLCColors colors: VLCStyle.colors

    Rectangle {
        anchors.fill: parent
        color: colors.banner

        RowLayout {
            anchors {
                fill: parent
                leftMargin: playlistToolbar.leftPadding
                rightMargin: playlistToolbar.rightPadding
            }

            spacing: VLCStyle.margin_normal

            Widgets.IconToolButton {
                id: loop
                Layout.alignment: Qt.AlignHCenter
                //Layout.minimumWidth: VLCStyle.icon_normal * 2
                size: VLCStyle.icon_normal
                iconText: (mainPlaylistController.repeatMode === PlaylistControllerModel.PLAYBACK_REPEAT_CURRENT)
                          ? VLCIcons.repeat_one
                          : VLCIcons.repeat_all
                checked: mainPlaylistController.repeatMode !== PlaylistControllerModel.PLAYBACK_REPEAT_NONE
                onClicked: mainPlaylistController.toggleRepeatMode()
                focusPolicy: Qt.NoFocus

                color: colors.buttonText
                colorDisabled: colors.textInactive
            }

            Widgets.IconToolButton {
                id: shuffle
                Layout.alignment: Qt.AlignHCenter
                //Layout.minimumWidth: VLCStyle.icon_normal * 2
                enabled: !mainPlaylistController.empty
                size: VLCStyle.icon_normal
                iconText: VLCIcons.shuffle_on
                onClicked: mainPlaylistController.shuffle()
                focusPolicy: Qt.NoFocus

                color: colors.buttonText
                colorDisabled: colors.textInactive
            }

            Widgets.SortControl {
                id: sort
                Layout.alignment: Qt.AlignHCenter
                //Layout.minimumWidth: VLCStyle.icon_normal * 2
                enabled: !mainPlaylistController.empty
                popupAlignment: Qt.AlignRight | Qt.AlignTop

                focusPolicy: Qt.NoFocus

                model: mainPlaylistController.sortKeyTitleList
                textRole: "title"
                criteriaRole: "key"

                onSortSelected: {
                    mainPlaylistController.sortKey = type
                }

                onSortOrderSelected: {
                    if (type === Qt.AscendingOrder)
                        mainPlaylistController.sortOrder = PlaylistControllerModel.SORT_ORDER_ASC
                    else if (type === Qt.DescendingOrder)
                        mainPlaylistController.sortOrder = PlaylistControllerModel.SORT_ORDER_DESC

                    mainPlaylistController.sort()
                }

                colors: playlistToolbar.colors

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

            Widgets.IconToolButton {
                id: clear
                Layout.alignment: Qt.AlignHCenter
                //Layout.minimumWidth: VLCStyle.icon_normal * 2
                size: VLCStyle.icon_normal
                enabled: !mainPlaylistController.empty
                iconText: VLCIcons.playlist_clear
                onClicked: mainPlaylistController.clear()
                focusPolicy: Qt.NoFocus

                color: colors.buttonText
                colorDisabled: colors.textInactive
            }
        }
    }
}

