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
import QtQml.Models 2.11

import org.videolan.vlc 0.1

import "qrc:///player/"
import "qrc:///widgets/" as Widgets
import "qrc:///style/"

MouseArea {
    id: dragArea

    anchors.verticalCenter: (!!parent) ? parent.verticalCenter : undefined

    cursorShape: (pressed || root.dragActive) ? Qt.DragMoveCursor : Qt.OpenHandCursor

    drag.target: content

    width: loader.width
    height: VLCStyle.icon_medium

    hoverEnabled: true

    readonly property int controlId: model.id
    readonly property bool dropVisible: dropArea.containsDrag
    property var dndView: null
    property alias containsDrag: dropArea.containsDrag

    Rectangle {
        anchors.fill: parent

        z: -1

        visible: dragArea.containsMouse && !pressed
        color: VLCStyle.colors.bgHover
    }

    Rectangle {
        anchors {
            left: parent.left
            verticalCenter: parent.verticalCenter
            leftMargin: index === 0 ? 0 : -width
        }

        z: 1

        width: VLCStyle.dp(2, VLCStyle.scale)
        height: parent.height

        visible: dropVisible
        color: VLCStyle.colors.accent
    }

    onEntered: playerBtnDND.currentIndex = index

    onPressed: {
        removeInfoRectVisible = true
    }

    onReleased: {
        drag.target.Drag.drop()
        removeInfoRectVisible = false
    }

    onPositionChanged: {
        var pos = this.mapToGlobal(mouseX, mouseY)
        updatePos(pos.x, pos.y)
    }

    function updatePos(x, y) {
        var pos = root.mapFromGlobal(x, y)
        content.x = pos.x
        content.y = pos.y
    }

    Rectangle {
        id: content

        anchors {
            horizontalCenter: parent.horizontalCenter
            verticalCenter: parent.verticalCenter
        }

        opacity: Drag.active ? 0.75 : 1.0

        Drag.active: pressed
        Drag.source: dragArea

        Loader {
            id: loader

            anchors {
                horizontalCenter: parent.horizontalCenter
                verticalCenter: parent.verticalCenter
            }
            source: PlayerControlbarControls.control(model.id).source
            onLoaded: {
                item.paintOnly = true
                item.enabled = false
            }
        }

        states: State {
            when: dragArea.pressed

            ParentChange {
                target: content;
                parent: root
            }

            AnchorChanges {
                target: content
                anchors { horizontalCenter: undefined; verticalCenter: undefined }
            }

            PropertyChanges {
                target: dragArea
                ListView.delayRemove: true
            }
        }

        onXChanged: {
            if (content.Drag.active)
                root.handleScroll(this)
        }
    }

    DropArea {
        id: dropArea
        anchors.fill: parent

        onEntered: {
            if ((drag.source === null ||
                 (drag.source.dndView === dndView &&
                  (parent.DelegateModel.itemsIndex === drag.source.DelegateModel.itemsIndex + 1))) ||
                    pressed)
                drag.accepted = false
        }

        onDropped: {
            var destIndex = parent.DelegateModel.itemsIndex

            if((drag.source.dndView === dndView)
                    && (drag.source.DelegateModel.itemsIndex < destIndex))
                --destIndex

            dropEvent(drag, destIndex)
        }
    }
}
