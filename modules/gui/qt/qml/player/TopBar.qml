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
import QtQuick.Layouts 1.3

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///utils/" as Utils

Utils.NavigableFocusScope{
    id: topFocusScope
    height: VLCStyle.icon_topbar
    property bool noAutoHide: false

    property bool noAutoHideInt: !player.hasVideoOutput
                              || !rootWindow.hasEmbededVideo
                              || topcontrollerMouseArea.containsMouse || lockAutoHide
    property bool lockAutoHide: false

    signal togglePlaylistVisiblity();

    Keys.priority: Keys.AfterItem
    Keys.onPressed: defaultKeyAction(event, 0)

    Rectangle{
        id : topcontrolContent
        color: VLCStyle.colors.setColorAlpha(VLCStyle.colors.banner, 0.8)
        anchors.fill: parent

        gradient: Gradient {
            GradientStop { position: 0.0; color: VLCStyle.colors.playerBg }
            GradientStop { position: 1.0; color: "transparent" }
        }


        MouseArea {
            id: topcontrollerMouseArea
            hoverEnabled: true
            anchors.fill: parent

            RowLayout{
                anchors.fill: parent
                anchors.leftMargin:  VLCStyle.margin_xsmall
                anchors.rightMargin: VLCStyle.margin_xsmall

                Utils.IconToolButton {
                    id: backBtn
                    objectName: "IconToolButton"
                    size: VLCStyle.icon_normal
                    text: VLCIcons.exit
                    color: VLCStyle.colors.playerFg
                    onClicked: {
                        if (player.playingState === PlayerController.PLAYING_STATE_PAUSED) {
                           mainPlaylistController.stop()
                        }
                        history.previous(History.Go)
                    }
                    KeyNavigation.right: playlistBtn
                    focus: true
                }

                Item{
                    Layout.fillWidth: true
                }

                Utils.IconToolButton {
                    id: playlistBtn
                    objectName: PlayerControlBarModel.PLAYLIST_BUTTON
                    size: VLCStyle.icon_normal
                    text: VLCIcons.playlist
                    color: VLCStyle.colors.playerFg
                    onClicked: togglePlaylistVisiblity()
                    property bool acceptFocus: true
                }
            }
        }
    }
}
