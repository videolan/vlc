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
import "qrc:///menus/" as Menus

Widgets.NavigableFocusScope{
    id: topFocusScope

    implicitHeight: topcontrolContent.implicitHeight

    property bool autoHide: player.hasVideoOutput
                            && rootPlayer.hasEmbededVideo
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
        implicitHeight: topcontrollerMouseArea.implicitHeight + topcontrollerMouseArea.anchors.topMargin

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
            implicitHeight: rowLayout.implicitHeight

            //drag and dbl click the titlebar in CSD mode
            Loader {
                anchors.fill: parent
                active: mainInterface.clientSideDecoration
                source: "qrc:///widgets/CSDTitlebarTapNDrapHandler.qml"
            }

            RowLayout {
                id: rowLayout
                anchors.fill: parent

                Column{
                    id: backAndTitleLayout
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop | Qt.AlignLeft

                    spacing: 0

                    Menus.Menubar {
                        id: menubar

                        width: parent.width
                        height: VLCStyle.icon_normal

                        Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft

                        visible: mainInterface.hasToolbarMenu
                    }

                    RowLayout {
                        anchors.left: parent.left
                        spacing: 0

                        Widgets.IconToolButton {
                            id: backBtn

                            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft

                            objectName: "IconToolButton"
                            size: VLCStyle.icon_normal
                            iconText: VLCIcons.topbar_previous
                            text: i18n.qtr("Back")
                            color: VLCStyle.colors.playerFg
                            onClicked: {
                                if (player.hasVideoOutput) {
                                    mainPlaylistController.stop()
                                }
                                history.previous()
                            }
                            KeyNavigation.right: menu_selector
                            focus: true
                        }

                        Image {
                            Layout.alignment: Qt.AlignVCenter
                            sourceSize.width: VLCStyle.icon_small
                            sourceSize.height: VLCStyle.icon_small
                            source: "qrc:///logo/cone.svg"
                            enabled: false
                        }
                    }

                    Label {
                        id: titleText

                        anchors.left: parent.left
                        anchors.leftMargin: VLCStyle.icon_normal
                        width: rowLayout.width - (windowAndGlobalButtonsLayout.width + anchors.leftMargin)

                        horizontalAlignment: Text.AlignLeft
                        color: VLCStyle.colors.playerFg
                        font.pixelSize: VLCStyle.fontSize_xxlarge
                        font.weight: Font.DemiBold
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                    }

                }

                Column{
                    id: windowAndGlobalButtonsLayout
                    Layout.alignment: Qt.AlignTop | Qt.AlignRight

                    spacing: 0

                    Loader {
                        //Layout.alignment: Qt.AlignRight | Qt.AlignTop
                        anchors.right: parent.right
                        height: VLCStyle.icon_normal
                        active: mainInterface.clientSideDecoration
                        enabled: mainInterface.clientSideDecoration
                        source: "qrc:///widgets/CSDWindowButtonSet.qml"
                        onLoaded: {
                            item.color = VLCStyle.colors.playerFg
                            item.hoverColor = VLCStyle.colors.windowCSDButtonDarkBg
                        }
                    }

                    Row {
                        //Layout.alignment: Qt.AlignRight | Qt.AlignTop
                        anchors.right: parent.right

                        Widgets.IconToolButton {
                            id: playlistBtn

                            objectName: PlayerControlBarModel.PLAYLIST_BUTTON
                            size: VLCStyle.icon_normal
                            iconText: VLCIcons.playlist
                            text: i18n.qtr("Playlist")
                            color: VLCStyle.colors.playerFg
                            onClicked: togglePlaylistVisiblity()
                            property bool acceptFocus: true

                            KeyNavigation.left: menu_selector
                        }

                        Widgets.IconToolButton {
                            id: menu_selector

                            size: VLCStyle.icon_normal
                            iconText: VLCIcons.ellipsis
                            text: i18n.qtr("Menu")
                            color: VLCStyle.colors.playerFg
                            property bool acceptFocus: true

                            onClicked: contextMenu.popup(this.mapToGlobal(0, height))

                            KeyNavigation.left: backBtn
                            KeyNavigation.right: playlistBtn

                            QmlGlobalMenu {
                                id: contextMenu
                                ctx: mainctx
                            }
                        }
                    }
                }
            }
        }
    }
}
