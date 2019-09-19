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

import "qrc:///style/"
import "qrc:///utils/" as Utils

Utils.NavigableFocusScope {
    id: root

    width: childrenRect.width
    height: childrenRect.height

    property alias model: list.model
    property string textRole


    property int popupAlignment: Qt.AlignRight | Qt.AlignBottom
    property int listWidth
    property alias currentIndex: list.currentIndex

    onFocusChanged: {
        if (!focus)
            popup.close()
    }

    onVisibleChanged: {
        if (!visible)
            popup.close()
    }

    Utils.IconToolButton {
        id: button

        size: VLCStyle.icon_normal
        text: VLCIcons.topbar_sort

        focus: true

        onClicked: {
            if (popup.opened)
                popup.close()
            else
                popup.open()
        }
    }

    Popup {
        id: popup

        y: (popupAlignment & Qt.AlignBottom) ? (root.height - 1) : 1
        x: (popupAlignment & Qt.AlignRight) ? (button.width - width) :  0
        width: root.listWidth
        implicitHeight: contentItem.implicitHeight + padding * 2
        padding: 1

        onOpened: {
            button.KeyNavigation.down = list
            button.highlighted = true
            list.forceActiveFocus()
        }

        onClosed: {
            button.KeyNavigation.down = null
            button.highlighted = false
            button.forceActiveFocus()
        }

        contentItem: ListView {
            id: list

            clip: true
            implicitHeight: contentHeight
            model: root.model
            spacing: 0

            highlight: Rectangle {
                color: VLCStyle.colors.accent
            }

            Rectangle {
                z: 10
                width: parent.width
                height: parent.height
                color: "transparent"
                border.color: VLCStyle.colors.accent
            }

            ScrollIndicator.vertical: ScrollIndicator { }

            delegate: ItemDelegate {
                id: itemDelegate

                anchors.left: parent.left
                anchors.right: parent.right
                padding: 0

                background: Item {}
                contentItem: Item {
                    implicitHeight: itemText.implicitHeight

                    Rectangle {
                        anchors.fill: parent
                        color: VLCStyle.colors.accent
                        visible: mouseArea.containsMouse
                    }

                    Text {
                        id: itemText
                        text: root.textRole ? (Array.isArray(root.model) ? modelData[root.textRole] : model[root.textRole]) : modelData
                        anchors.fill: parent
                        topPadding: VLCStyle.margin_xxsmall
                        bottomPadding: VLCStyle.margin_xxsmall
                        leftPadding: VLCStyle.margin_xsmall
                        rightPadding: VLCStyle.margin_xsmall
                        color: VLCStyle.colors.buttonText
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: itemDelegate.clicked(mouse)
                    }
                }

                onClicked: {
                    root.currentIndex = index
                    popup.close()
                }
            }
        }

        background: Rectangle {
            color: VLCStyle.colors.button
            border.color: VLCStyle.colors.buttonBorder
        }
    }
}




