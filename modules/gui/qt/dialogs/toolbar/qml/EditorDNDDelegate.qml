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

import QtQuick
import QtQuick.Templates as T
import QtQuick.Layouts
import QtQml.Models


import VLC.Player
import VLC.Widgets as Widgets
import VLC.Style

T.Control {
    id: control

    padding: background.border.width

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    readonly property int controlId: model.id
    property ListView dndView: null

    readonly property bool dragActive: contentItem.target.Drag.active
    property alias dropArea: dropArea

    property alias containsMouse: mouseArea.containsMouse
    property alias pressed: mouseArea.pressed

    ListView.delayRemove: dragActive
    
    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Item
    }

    MouseArea {
        id: mouseArea

        anchors.fill: parent

        cursorShape: (pressed || root.dragActive) ? Qt.DragMoveCursor
                                                  : Qt.OpenHandCursor

        drag.target: loader

        drag.smoothed: false

        hoverEnabled: true

        drag.onActiveChanged: {
            if (drag.active) {
                dragAutoScrollHandler.dragItem = loader
                root.dragStarted(controlId)
                removeInfoRectVisible = true
                drag.target.Drag.start()

            } else {
                dragAutoScrollHandler.dragItem = null
                control.visible = false
                const action = drag.target.Drag.drop()
                if (action === Qt.IgnoreAction)
                    control.visible = true
                removeInfoRectVisible = false
                root.dragStopped(controlId)
            }
        }

        onPressed: (mouse) => {
            const pos = mapToItem(control.contentItem.target.parent, mouseX, mouseY)
            control.contentItem.target.y = pos.y + VLCStyle.dragDelta
            control.contentItem.target.x = pos.x + VLCStyle.dragDelta
        }
    }

    DropArea {
        id: dropArea
        anchors.fill: parent

        onEntered: (drag) => {
            if ((drag.source === null ||
                 (drag.source.dndView === dndView &&
                  (parent.DelegateModel.itemsIndex === drag.source.DelegateModel.itemsIndex + 1))) ||
                    pressed)
                drag.accepted = false
        }

        onDropped: (drop) => {
            let destIndex = parent.DelegateModel.itemsIndex

            if((drop.source.dndView === dndView)
                    && (drop.source.DelegateModel.itemsIndex < destIndex))
                --destIndex

            dropEvent(drop, destIndex)
        }
    }

    Binding {
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
        color: theme.accent
    }

    background: Rectangle {
        color: "transparent"

        border.width: VLCStyle.dp(1, VLCStyle.scale)
        border.color: theme.border
    }

    contentItem: Item {
        implicitHeight: loader.implicitHeight
        implicitWidth: loader.implicitWidth

        readonly property Item target: loader

        Loader {
            id: loader

            parent: Drag.active ? root : control.contentItem

            anchors.fill: (parent === control.contentItem) ? parent : undefined

            source: PlayerControlbarControls.control(model.id).source

            Drag.source: control

            onLoaded: {
                item.paintOnly = true
                item.enabled = false
            }
        }
    }
}
