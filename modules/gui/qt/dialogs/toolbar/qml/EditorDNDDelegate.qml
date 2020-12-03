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

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

MouseArea {
    id: dragArea

    property int controlId: model.id
    property bool held: false
    property bool dropVisible: false
    property var dndView: null
    anchors.verticalCenter: (!!parent) ? parent.verticalCenter : undefined
    cursorShape: Qt.OpenHandCursor
    drag.target: held ? content : undefined
    width: buttonloader.width
    height: VLCStyle.icon_medium
    hoverEnabled: true

    property alias containsDrag: dropArea.containsDrag

    onHeldChanged: {
        if (held) {
            removeInfoRectVisible = true
        }
        else {
            removeInfoRectVisible = false
        }
    }

    Rectangle {
        z: -1
        anchors.fill: parent

        visible: dragArea.containsMouse && !held
        color: VLCStyle.colors.bgHover
    }

    Rectangle {
        z: 1
        width: VLCStyle.dp(2, VLCStyle.scale)
        height: parent.height
        anchors {
            left: parent.left
            verticalCenter: parent.verticalCenter
            leftMargin: index === 0 ? 0 : -width
        }
        antialiasing: true
        visible: dropVisible
        color: VLCStyle.colors.accent
    }

    onPressed: {
        held = true
        root._held = true
    }

    onEntered: playerBtnDND.currentIndex = index

    onWheel: {
        playerBtnDND.wheelScroll(wheel.angleDelta.y)
    }

    onReleased: {
        drag.target.Drag.drop()
        held = false
        root._held = false
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
        Drag.active: dragArea.held
        Drag.source: dragArea
        anchors {
            horizontalCenter: parent.horizontalCenter
            verticalCenter: parent.verticalCenter
        }

        opacity: held ? 0.75 : 1.0

        Loader{
            id: buttonloader
            anchors {
                horizontalCenter: parent.horizontalCenter
                verticalCenter: parent.verticalCenter
            }
            sourceComponent: controlButtons.returnbuttondelegate(model.id)
            onLoaded: buttonloader.item.paintOnly = true

        }

        states: State {
            when: dragArea.held

            ParentChange { target: content; parent: root }
            AnchorChanges {
                target: content
                anchors { horizontalCenter: undefined; verticalCenter: undefined }
            }
        }

        onXChanged: {
            root.handleScroll(this)
        }
    }

    DropArea {
        id: dropArea
        anchors.fill: parent

        onEntered: {
            if ((drag.source === null ||
                 (drag.source.dndView === playerBtnDND &&
                  (parent.DelegateModel.itemsIndex === drag.source.DelegateModel.itemsIndex + 1))))
                return

            if (held)
                return

            dropVisible = true
        }

        onExited: {
            if (held)
                return

            dropVisible = false
        }

        onDropped: {
            if (!dropVisible)
                return

            if (held)
                return

            if (drag.source.dndView === playerBtnDND) {
                // moving from same section
                var srcIndex = drag.source.DelegateModel.itemsIndex
                var destIndex = parent.DelegateModel.itemsIndex

                if(srcIndex < destIndex)
                    destIndex -= 1
                playerBtnDND.model.move(srcIndex,destIndex)
            }
            else if (drag.source.objectName == "buttonsList"){
                // moving from buttonsList
                dndView.model.insert(parent.DelegateModel.itemsIndex, {"id" : drag.source.mIndex})
            }
            else {
                // moving between sections
                dndView.model.insert(parent.DelegateModel.itemsIndex, {"id" : drag.source.controlId})
                drag.source.dndView.model.remove(drag.source.DelegateModel.itemsIndex)
            }

            dropVisible = false
        }
    }
}
