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
    property VLCColors colors: VLCStyle.nightColors

    Keys.priority: Keys.AfterItem
    Keys.onPressed: defaultKeyAction(event, 0)

    Item {
        id : topcontrolContent

        anchors.fill: parent
        implicitHeight: topcontrollerMouseArea.implicitHeight + topcontrollerMouseArea.anchors.topMargin

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
                anchors.leftMargin: VLCStyle.margin_xxsmall

                Column{
                    id: backAndTitleLayout
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop | Qt.AlignLeft

                    spacing: VLCStyle.margin_large

                    Menus.Menubar {
                        id: menubar

                        width: parent.width
                        height: VLCStyle.icon_normal

                        Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft

                        visible: mainInterface.hasToolbarMenu
                        textColor: topFocusScope.colors.text
                        highlightedBgColor: topFocusScope.colors.bgHover
                        highlightedTextColor: topFocusScope.colors.bgHoverText
                    }

                    RowLayout {
                        anchors.left: parent.left
                        spacing: VLCStyle.margin_xxsmall

                        Widgets.IconToolButton {
                            id: backBtn

                            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft

                            objectName: "IconToolButton"
                            size: VLCStyle.banner_icon_size
                            iconText: VLCIcons.topbar_previous
                            text: i18n.qtr("Back")
                            color: topFocusScope.colors.playerFg
                            onClicked: {
                                if (mainInterface.hasEmbededVideo && !mainInterface.canShowVideoPIP) {
                                   mainPlaylistController.stop()
                                }
                                history.previous()
                            }
                            focus: true
                        }

                        Image {
                            id: logo

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
                        anchors.leftMargin: logo.x
                        width: rowLayout.width - anchors.leftMargin

                        horizontalAlignment: Text.AlignLeft
                        color: topFocusScope.colors.playerFg
                        font.pixelSize: VLCStyle.dp(18, VLCStyle.scale)
                        font.weight: Font.DemiBold
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                    }

                }
            }
        }
    }
}
