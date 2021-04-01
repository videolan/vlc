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
import QtQuick.Layouts 1.3

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

GridView{
    id: allButtonsView
    clip: true

    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOn }
    model: controlButtons.buttonL.length

    highlightFollowsCurrentItem: false

    cellWidth: VLCStyle.cover_small
    cellHeight: cellWidth

    property alias removeInfoRectVisible: removeInfoRect.visible

    DropArea {
        id: dropArea
        anchors.fill: parent

        z: 3

        function isFromList() {
            if (drag.source.objectName === "buttonsList")
                return true
            else
                return false
        }

        onDropped: {
            if (isFromList())
                return

            drag.source.dndView.model.remove(drag.source.DelegateModel.itemsIndex)
        }
    }

    Rectangle {
        id: removeInfoRect
        anchors.fill: parent
        z: 2

        visible: false

        opacity: 0.8
        color: VLCStyle.colors.bg

        border.color: VLCStyle.colors.menuCaption
        border.width: VLCStyle.dp(2, VLCStyle.scale)

        Text {
            anchors.centerIn: parent

            text: VLCIcons.del
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter

            font.pointSize: VLCStyle.fontHeight_xxxlarge

            font.family: VLCIcons.fontFamily
            color: VLCStyle.colors.menuCaption
        }

        MouseArea {
            anchors.fill: parent

            cursorShape: visible ? Qt.DragMoveCursor : Qt.ArrowCursor
        }
    }

    MouseArea {
        anchors.fill: parent
        z: 1

        visible: root._held

        cursorShape: visible ? Qt.DragMoveCursor : Qt.ArrowCursor
    }

    delegate: MouseArea {
        id:dragArea
        objectName: "buttonsList"
        hoverEnabled: true
        width: cellWidth
        height: cellHeight

        property bool held: false
        property int mIndex: controlButtons.buttonL[model.index].id
        drag.target: held ? buttonDragItem : undefined
        cursorShape: Qt.OpenHandCursor

        onPressed: {
            buttonDragItem.visible = true
            buttonDragItem.text = controlButtons.buttonL[model.index].label
            buttonDragItem.Drag.source = dragArea
            held = true
            root._held = true
        }

        onReleased: {
            drag.target.Drag.drop()
            buttonDragItem.visible = false
            held = false
            root._held = false
        }

        onPositionChanged: {
            var pos = this.mapToGlobal(mouseX, mouseY)
            buttonDragItem.updatePos(pos.x, pos.y)
        }

        onEntered: allButtonsView.currentIndex = index

        Loader {
            active: allButtonsView.currentIndex === index
            anchors.fill: parent

            sourceComponent: Rectangle {
                color: VLCStyle.colors.bgHover
            }
        }

        ColumnLayout{
            id: listelemlayout
            anchors.fill: parent
            anchors.margins: 10

            EditorDummyButton {
                Layout.preferredWidth: VLCStyle.icon_medium
                Layout.preferredHeight: VLCStyle.icon_medium
                Layout.alignment: Qt.AlignHCenter
                text: controlButtons.buttonL[model.index].label
            }

            Widgets.ListSubtitleLabel {
                id: buttonName
                Layout.fillWidth: true
                Layout.fillHeight: true

                elide: Text.ElideNone
                text: controlButtons.buttonL[model.index].text
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}


