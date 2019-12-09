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

    Rectangle {
        anchors.fill: parent
        color: VLCStyle.colors.banner

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
            }

            Widgets.IconToolButton {
                id: shuffle
                Layout.alignment: Qt.AlignHCenter
                //Layout.minimumWidth: VLCStyle.icon_normal * 2
                size: VLCStyle.icon_normal
                iconText: VLCIcons.shuffle_on
                onClicked: mainPlaylistController.shuffle()
                focusPolicy: Qt.NoFocus
            }

            Label {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                horizontalAlignment: Text.AlignHCenter
                text: (view.mode === "select")
                        ? qsTr("Select tracks (%1)").arg(plmodel.selectedCount)
                    : (view.mode === "move")
                        ? qsTr("Move tracks (%1)").arg(plmodel.selectedCount)
                    : qsTr("%1 tracks").arg(plmodel.count)
                font.pixelSize: VLCStyle.fontSize_normal
                color: VLCStyle.colors.text
                elide: Text.ElideRight
            }

            Widgets.SortControl {
                id: sort
                Layout.alignment: Qt.AlignHCenter
                //Layout.minimumWidth: VLCStyle.icon_normal * 2
                popupAlignment: Qt.AlignRight | Qt.AlignTop

                model: ListModel {
                    ListElement { text: qsTr("Tile");             criteria: PlaylistControllerModel.SORT_KEY_TITLE}
                    ListElement { text: qsTr("Duration");         criteria: PlaylistControllerModel.SORT_KEY_DURATION}
                    ListElement { text: qsTr("Artist");           criteria: PlaylistControllerModel.SORT_KEY_ARTIST}
                    ListElement { text: qsTr("Album");            criteria: PlaylistControllerModel.SORT_KEY_ALBUM}
                    ListElement { text: qsTr("Genre");            criteria: PlaylistControllerModel.SORT_KEY_GENRE}
                    ListElement { text: qsTr("Date");             criteria: PlaylistControllerModel.SORT_KEY_DATE}
                    ListElement { text: qsTr("Track number");     criteria: PlaylistControllerModel.SORT_KEY_TRACK_NUMBER}
                    ListElement { text: qsTr("URL");              criteria: PlaylistControllerModel.SORT_KEY_URL}
                    ListElement { text: qsTr("Rating");           criteria: PlaylistControllerModel.SORT_KEY_RATIN}
                }
                textRole: "text"

                listWidth: VLCStyle.widthSortBox
                onSortSelected: {
                    mainPlaylistController.sort(modelData.criteria, PlaylistControllerModel.SORT_ORDER_ASC)
                }

                Keys.priority: Keys.AfterItem
                Keys.onPressed: defaultKeyAction(event, 0)
                navigationParent: playlistToolbar
            }

            Widgets.IconToolButton {
                id: clear
                Layout.alignment: Qt.AlignHCenter
                //Layout.minimumWidth: VLCStyle.icon_normal * 2
                size: VLCStyle.icon_normal
                iconText: VLCIcons.playlist_clear
                onClicked: mainPlaylistController.clear()
                focusPolicy: Qt.NoFocus
            }
        }
    }
}

