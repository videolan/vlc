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
import "qrc:///style/"

NavigableFocusScope {
    id: root
    signal playClicked
    signal addToPlaylistClicked
    signal itemClicked(int keys, int modifier)
    signal itemDoubleClicked(int keys, int modifier)

    property alias hovered: mouse.containsMouse

    property Component cover: Item {}
    property alias line1: line1_text.text
    property alias line2: line2_text.text

    property alias color: linerect.color

    Component {
        id: actionAdd
        IconToolButton {
            size: VLCStyle.icon_normal
            text: VLCIcons.add

            focus: true

            highlightColor: activeFocus ? VLCStyle.colors.buttonText : "transparent"

            //visible: mouse.containsMouse || root.activeFocus
            onClicked: root.addToPlaylistClicked()
        }
    }


    Component {
        id: actionPlay
        IconToolButton {
            id: add_and_play_icon
            size: VLCStyle.icon_normal
            //visible: mouse.containsMouse  || root.activeFocus
            text: VLCIcons.play

            focus: true

            highlightColor: add_and_play_icon.activeFocus ? VLCStyle.colors.buttonText : "transparent"
            onClicked: root.playClicked()
        }
    }

    property var actionButtons: [ actionAdd, actionPlay ]

    Rectangle {
        id: linerect
        anchors.fill: parent
        color: "transparent"

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                root.itemClicked(mouse.buttons, mouse.modifiers);
            }
            onDoubleClicked: {
                root.itemDoubleClicked(mouse.buttons, mouse.modifiers);
            }
        }

        RowLayout {
            anchors.fill: parent

            Loader {
                Layout.preferredWidth: VLCStyle.icon_normal
                Layout.preferredHeight: VLCStyle.icon_normal
                sourceComponent: root.cover
            }
            FocusScope {
                id: presentation
                Layout.fillHeight: true
                Layout.fillWidth: true
                focus: true

                Column {
                    anchors.fill: parent

                    Text{
                        id: line1_text
                        font.bold: true
                        width: parent.width
                        elide: Text.ElideRight
                        color: VLCStyle.colors.text
                        font.pixelSize: VLCStyle.fontSize_normal
                        enabled: text !== ""
                    }
                    Text{
                        id: line2_text
                        width: parent.width
                        text: ""
                        elide: Text.ElideRight
                        color: VLCStyle.colors.text
                        font.pixelSize: VLCStyle.fontSize_xsmall
                        enabled: text !== ""
                    }
                }

                Keys.onRightPressed: {
                    if (actionButtons.length === 0)
                        root.actionRight(0)
                    else
                        toolButtons.focus = true
                }
                Keys.onLeftPressed: {
                    root.actionLeft(0)
                }
            }

            FocusScope {
                id: toolButtons
                Layout.preferredHeight: VLCStyle.icon_normal
                Layout.preferredWidth: toolButtonsRow.implicitWidth
                Layout.alignment: Qt.AlignVCenter
                visible: mouse.containsMouse || root.activeFocus
                property int focusIndex: 0
                Row {
                    id: toolButtonsRow
                    anchors.fill: parent
                    Repeater {
                        id: buttons
                        model: actionButtons
                        delegate: Loader {
                            sourceComponent: modelData
                            focus: index === toolButtons.focusIndex
                        }
                    }
                }
                Keys.onLeftPressed: {
                    if (focusIndex === 0)
                        presentation.focus = true
                    else {
                        focusIndex -= 1
                    }
                }
                Keys.onRightPressed: {
                    if (focusIndex === actionButtons.length - 1)
                        root.actionRight(0)
                    else {
                        focusIndex += 1
                    }
                }
            }
        }
    }
}
