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

    property bool held: false
    property bool dropVisible: false
    property var dndView: null
    anchors.verticalCenter: parent.verticalCenter
    cursorShape: dropVisible ? Qt.DragMoveCursor : Qt.OpenHandCursor
    drag.target: held ? content : undefined
    width: buttonloader.width
    height: VLCStyle.icon_medium
    hoverEnabled: true

    Rectangle {
        z: 1
        width: VLCStyle.dp(2)
        height: parent.height
        anchors {
            left: parent.left
            verticalCenter: parent.verticalCenter
        }
        antialiasing: true
        visible: dropVisible
        color: VLCStyle.colors.accent
    }
    onPressed: held = true
    onEntered: playerBtnDND.currentIndex = index

    onExited: {
        if(containsPress)
            dndView.deleteBtn = true
    }

    onReleased: {
        drag.target.Drag.drop()
        held = false
        if(dndView.deleteBtn){
            dndView.deleteBtn = false
            dndView.model.remove(
                        dragArea.DelegateModel.itemsIndex)
        }
    }

    Rectangle {
        id: content
        Drag.active: dragArea.held
        Drag.source: dragArea
        anchors {
            horizontalCenter: parent.horizontalCenter
            verticalCenter: parent.verticalCenter
        }
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
    }
    DropArea {
        anchors.fill: parent

        onEntered: {
            dropVisible = true
            dndView.deleteBtn = false
        }

        onExited: {
            dropVisible = false
            if(!dndView.addBtn)
                dndView.deleteBtn = true
        }

        onDropped: {
            if (drag.source.objectName == "buttonsList")
                dndView.model.insert(parent.DelegateModel.itemsIndex,
                                            {"id" : drag.source.mIndex,
                                                "size": PlayerControlBarModel.WIDGET_NORMAL})
            else{
                var srcIndex = drag.source.DelegateModel.itemsIndex
                var destIndex = parent.DelegateModel.itemsIndex

                if(srcIndex < destIndex)
                    destIndex -= 1
                playerBtnDND.model.move(srcIndex,destIndex)
            }
            dropVisible = false
        }
    }
}
