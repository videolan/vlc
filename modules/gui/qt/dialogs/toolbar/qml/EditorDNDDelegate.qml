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
import org.videolan.compat 0.1

import "qrc:///player/"
import "qrc:///widgets/" as Widgets
import "qrc:///style/"

Control {
    id: control

    padding: background.border.width

    readonly property int controlId: model.id
    property ListView dndView: null

    readonly property bool dragActive: loader.Drag.active
    property alias dropArea: dropArea

    property alias containsMouse: mouseArea.containsMouse
    property alias pressed: mouseArea.pressed

    ListView.delayRemove: dragActive
    
    MouseArea {
        id: mouseArea

        anchors.fill: parent

        cursorShape: (pressed || root.dragActive) ? Qt.DragMoveCursor
                                                  : Qt.OpenHandCursor

        drag.target: loader

        hoverEnabled: true

        drag.onActiveChanged: {
            if (drag.active) {
                dragAutoScrollHandler.dragItem = loader
                root.dragStarted(controlId)
                removeInfoRectVisible = true
                drag.target.Drag.start()

            } else {
                dragAutoScrollHandler.dragItem = null
                drag.target.Drag.drop()
                removeInfoRectVisible = false
                root.dragStopped(controlId)
            }
        }

        onPositionChanged: {
            if (drag.active) {
                // FIXME: There must be a better way of this
                var pos = mapToItem(loader.parent, mouseX, mouseY)
                // y should be set first, because the automatic scroll is
                // triggered by change on X
                loader.y = pos.y
                loader.x = pos.x
            }
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

    BindingCompat {
        when: dragActive
        value: true

        target: root
        property: "dragActive"
    }

    Rectangle {
        anchors {
            left: parent.left
            verticalCenter: parent.verticalCenter
            leftMargin: index === 0 ? 0 : -width
        }

        z: 1

        implicitWidth: VLCStyle.dp(2, VLCStyle.scale)
        implicitHeight: VLCStyle.icon_medium

        visible: dropArea.containsDrag
        color: VLCStyle.colors.accent
    }

    background: Rectangle {
        opacity: Drag.active ? 0.75 : 1.0

        color: "transparent"

        border.width: VLCStyle.dp(1, VLCStyle.scale)
        border.color: containsMouse && !pressed ? VLCStyle.colors.buttonBorder
                                                : "transparent"
    }

    contentItem: Item {
        implicitHeight: loader.implicitHeight
        implicitWidth: loader.implicitWidth

        Loader {
            id: loader

            parent: Drag.active ? root : control.contentItem

            anchors.horizontalCenter: Drag.active ? undefined : parent.horizontalCenter
            anchors.verticalCenter:  Drag.active ? undefined : parent.verticalCenter

            source: PlayerControlbarControls.control(model.id).source

            Drag.source: control

            onLoaded: {
                item.paintOnly = true
                item.enabled = false

                if (!extraWidthAvailable && item.minimumWidth !== undefined) {
                    item.width = item.minimumWidth
                }
            }
        }
    }
}
