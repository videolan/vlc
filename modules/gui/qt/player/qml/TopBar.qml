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

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

Widgets.NavigableFocusScope{
    id: topFocusScope

    implicitHeight: topcontrolContent.implicitHeight

    property bool autoHide: player.hasVideoOutput
                            && mainInterface.hasEmbededVideo
                            && !topcontrollerMouseArea.containsMouse
                            && !lockAutoHide

    property bool lockAutoHide: false

    property alias title: titleText.text

    signal togglePlaylistVisiblity();

    Keys.priority: Keys.AfterItem
    Keys.onPressed: defaultKeyAction(event, 0)

    Rectangle{
        id : topcontrolContent
        color: VLCStyle.colors.setColorAlpha(VLCStyle.colors.banner, 0.8)
        anchors.fill: parent
        implicitHeight: VLCStyle.icon_topbar + topcontrollerMouseArea.anchors.topMargin

        gradient: Gradient {
            GradientStop { position: 0.0; color: VLCStyle.colors.playerBg }
            GradientStop { position: 1.0; color: "transparent" }
        }


        MouseArea {
            id: topcontrollerMouseArea
            hoverEnabled: true
            anchors.fill: parent
            anchors.topMargin: VLCStyle.applicationVerticalMargin
            anchors.leftMargin: VLCStyle.applicationHorizontalMargin
            anchors.rightMargin: VLCStyle.applicationHorizontalMargin

            RowLayout{
                anchors.fill: parent
                anchors.leftMargin:  VLCStyle.margin_xsmall
                anchors.rightMargin: VLCStyle.margin_xsmall

                Widgets.IconToolButton {
                    id: backBtn

                    Layout.alignment: Qt.AlignTop

                    objectName: "IconToolButton"
                    size: VLCStyle.icon_normal
                    iconText: VLCIcons.exit
                    text: i18n.qtr("Back")
                    color: VLCStyle.colors.playerFg
                    onClicked: {
                        if (player.hasVideoOutput) {
                           mainPlaylistController.stop()
                        }
                        history.previous()
                    }
                    KeyNavigation.right: playlistBtn
                    focus: true
                }

                Label {
                    id: titleText

                    Layout.fillWidth: true
                    Layout.preferredHeight: implicitHeight
                    Layout.leftMargin:  VLCStyle.margin_large
                    Layout.rightMargin: VLCStyle.margin_large
                    Layout.alignment: Qt.AlignHCenter | Qt.AlignTop

                    horizontalAlignment: Text.AlignHCenter
                    color: VLCStyle.colors.playerFg
                    font.pixelSize: VLCStyle.fontSize_xxxlarge
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                }

                Widgets.IconToolButton {
                    id: playlistBtn

                    Layout.alignment: Qt.AlignTop

                    objectName: PlayerControlBarModel.PLAYLIST_BUTTON
                    size: VLCStyle.icon_normal
                    iconText: VLCIcons.playlist
                    text: i18n.qtr("Playlist")
                    color: VLCStyle.colors.playerFg
                    onClicked: togglePlaylistVisiblity()
                    property bool acceptFocus: true

                    KeyNavigation.left: backBtn
                }
            }
        }
    }
}
