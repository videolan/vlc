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
import QtGraphicalEffects 1.0

import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///util/KeyHelper.js" as KeyHelper

NavigableFocusScope {
    id: root
    signal playClicked
    signal addToPlaylistClicked
    signal itemClicked(int key, int modifier)
    signal itemDoubleClicked(int keys, int modifier)
    signal contextMenuButtonClicked(Item menuParent)

    property alias hovered: mouse.containsMouse

    property Component cover: Item {}
    property alias line1: line1_text.text
    property alias line2: line2_text.text
    property alias imageText: cover_text.text

    property alias color: linerect.color
    property bool showContextButton: false

    property bool selected: false

    Accessible.role: Accessible.ListItem
    Accessible.name: line1

    Component {
        id: actionAdd
        IconToolButton {
            size: VLCStyle.icon_normal
            iconText: VLCIcons.add
            text: i18n.qtr("Enqueue")

            focus: true

            font.underline: activeFocus

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
            iconText: VLCIcons.play
            text: i18n.qtr("Play")

            focus: true

            font.underline: activeFocus

            onClicked: root.playClicked()
        }
    }

    property var actionButtons: [ actionAdd, actionPlay ]

    Rectangle {
        id: linerect
        anchors.fill: parent
        color: VLCStyle.colors.getBgColor(
                   root.selected, root.hovered,
                   root.activeFocus)

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: {
                if (mouse.button === Qt.RightButton)
                    contextMenuButtonClicked(root);
                else
                    root.itemClicked(mouse.button, mouse.modifiers);
            }
            onDoubleClicked: {
                root.itemDoubleClicked(mouse.buttons, mouse.modifiers);
            }


            Item {
                id: innerRect
                anchors.fill: parent
                anchors.margins: VLCStyle.margin_xxsmall
                anchors.verticalCenter: parent.verticalCenter

                RowLayout {
                    anchors.fill: parent
                    anchors.rightMargin: VLCStyle.margin_xxsmall
                    Item {
                        Layout.preferredWidth: VLCStyle.icon_normal
                        Layout.preferredHeight: VLCStyle.icon_normal
                        Loader {
                            anchors.fill: parent
                            sourceComponent: root.cover
                        }
                        Text {
                            id: cover_text
                            anchors.centerIn: parent
                            color: VLCStyle.colors.textInactive
                            font.pixelSize: VLCStyle.fontSize_xsmall
                        }
                    }
                    FocusScope {
                        id: presentation
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        focus: true

                        Column {
                            anchors {
                                left: parent.left
                                right: parent.right
                                verticalCenter: parent.verticalCenter
                            }

                            Text {
                                id: line1_text
                                width: parent.width

                                elide: Text.ElideRight
                                color: VLCStyle.colors.text
                                font.pixelSize: VLCStyle.fontSize_normal
                                enabled: text !== ""
                            }
                            Text {
                                id: line2_text
                                width: parent.width
                                elide: Text.ElideRight
                                color: VLCStyle.colors.textInactive
                                font.pixelSize: VLCStyle.fontSize_small
                                visible: text !== ""
                                enabled: text !== ""
                            }
                        }

                        Keys.onRightPressed: {
                            if (actionButtons.length === 0 && !root.showContextButton)
                                root.navigationRight(0)
                            else
                                toolButtons.focus = true
                        }
                        Keys.onLeftPressed: {
                            root.navigationLeft(0)
                        }

                        Keys.onReleased: {
                            if (KeyHelper.matchOk(event)) {
                                itemDoubleClicked(event.key, event.modifiers)
                            }
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
                            IconToolButton {
                                id: contextButton
                                size: VLCStyle.icon_normal
                                iconText: VLCIcons.ellipsis
                                text: i18n.qtr("More")

                                visible: root.showContextButton
                                focus: actionButtons.length == toolButtons.focusIndex

                                font.underline: activeFocus

                                onClicked: root.contextMenuButtonClicked(this)
                            }
                        }
                    }
                    Keys.onLeftPressed: {
                        if (toolButtons.focusIndex === 0)
                            presentation.focus = true
                        else {
                            toolButtons.focusIndex -= 1
                        }
                    }
                    Keys.onRightPressed: {
                        if (toolButtons.focusIndex === (actionButtons.length - (!root.showContextButton ? 1 : 0) ) )
                            root.navigationRight(0)
                        else {
                            toolButtons.focusIndex += 1
                        }
                    }
                }
            }
        }
    }
}
