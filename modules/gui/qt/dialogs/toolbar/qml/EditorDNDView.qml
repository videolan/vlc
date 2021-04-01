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
import QtQml.Models 2.11

import org.videolan.vlc 0.1

import "qrc:///style/"

ListView {
    id: playerBtnDND
    spacing: VLCStyle.margin_xsmall
    orientation: Qt.Horizontal
    clip: true

    highlightFollowsCurrentItem: false

    property bool containsDrag: footerItem.dropVisible

    property alias scrollBar: scrollBar

    ScrollBar.horizontal: ScrollBar {
        id: scrollBar
        policy: playerBtnDND.contentWidth > playerBtnDND.width ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
    }

    function wheelScroll(delta) {
        if (delta > 0)
            scrollBar.decrease()
        else
            scrollBar.increase()
    }

    remove: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0; duration: 200 }
    }

    add: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1.0; duration: 200 }
    }

    displaced: Transition {
        NumberAnimation { properties: "x"; duration: 200; easing.type: Easing.OutSine }
        NumberAnimation { property: "opacity"; to: 1.0 }
    }
    
    MouseArea {
        anchors.fill: parent
        z: 1

        visible: root._held

        cursorShape: visible ? Qt.DragMoveCursor : Qt.ArrowCursor
    }

    footer: MouseArea {
        height: VLCStyle.icon_medium
        width: Math.max(height, playerBtnDND.width - x)
        anchors.verticalCenter: parent.verticalCenter
        property bool dropVisible: false

        onWheel: {
            wheelScroll(wheel.angleDelta.y)
        }

        Rectangle {
            z: 2
            width: VLCStyle.dp(2, VLCStyle.scale)
            height: parent.height
            anchors {
                left: parent.left
            }
            antialiasing: true
            visible: dropVisible
            color: VLCStyle.colors.accent
        }
        DropArea {
            anchors.fill: parent

            onEntered: {
                if (drag.source.dndView === playerBtnDND && drag.source.DelegateModel.itemsIndex === playerBtnDND.count - 1)
                    return

                dropVisible = true
            }

            onExited: {
                dropVisible = false
            }

            onDropped: {
                if (!dropVisible)
                    return

                if (drag.source.dndView === playerBtnDND) {
                    // moving from same section
                    playerBtnDND.model.move(drag.source.DelegateModel.itemsIndex, playerBtnDND.count - 1)
                }
                else if (drag.source.objectName == "buttonsList"){
                    // moving from buttonsList
                    playerBtnDND.model.insert(playerBtnDND.count, {"id" : drag.source.mIndex})
                }
                else {
                    // moving between sections
                    playerBtnDND.model.insert(playerBtnDND.count, {"id" : drag.source.controlId})
                    drag.source.dndView.model.remove(drag.source.DelegateModel.itemsIndex)
                }

                dropVisible = false
            }
        }
    }

    delegate: EditorDNDDelegate {
        dndView: playerBtnDND

        onContainsDragChanged: {
            for(var child in playerBtnDND.contentItem.children) {
                if (playerBtnDND.contentItem.children[child].containsDrag === true) {
                    playerBtnDND.containsDrag = true
                    return
                }
            }
            playerBtnDND.containsDrag = Qt.binding(function() { return footerItem.dropVisible; } )
        }
    }
}
