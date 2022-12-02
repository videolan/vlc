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
import QtQuick.Layouts 1.11

import org.videolan.vlc 0.1

import "qrc:///player/"
import "qrc:///style/"
import "qrc:///widgets/" as Widgets
import "qrc:///util/" as Util

GridView {
    id: root

    clip: true

    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOn }
    model: PlayerControlbarControls.controlList.length

    currentIndex: -1
    highlightFollowsCurrentItem: false

    cellWidth: VLCStyle.cover_small
    cellHeight: cellWidth

    property alias removeInfoRectVisible: removeInfoRect.visible

    signal dragStarted(int id)
    signal dragStopped(int id)


    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    MouseEventFilter {
        target: root
    }

    Util.FlickableScrollHandler { }

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
        color: theme.bg.primary

        border.color: theme.border
        border.width: VLCStyle.dp(2, VLCStyle.scale)

        Text {
            anchors.centerIn: parent

            text: VLCIcons.del
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter

            font.pointSize: VLCStyle.fontHeight_xxxlarge

            font.family: VLCIcons.fontFamily
            color: theme.fg.secondary
        }

        MouseArea {
            anchors.fill: parent

            cursorShape: visible ? Qt.DragMoveCursor : Qt.ArrowCursor
        }
    }

    MouseArea {
        anchors.fill: parent
        z: 1

        visible: buttonDragItem.Drag.active

        cursorShape: visible ? Qt.DragMoveCursor : Qt.ArrowCursor
    }

    delegate: MouseArea {
        width: cellWidth
        height: cellHeight

        hoverEnabled: true
        cursorShape: Qt.OpenHandCursor

        objectName: "buttonsList"

        drag.target: buttonDragItem

        readonly property int mIndex: PlayerControlbarControls.controlList[model.index].id

        drag.onActiveChanged: {
            if (drag.active) {
                dragStarted(mIndex)

                buttonDragItem.text = PlayerControlbarControls.controlList[model.index].label
                buttonDragItem.Drag.source = this
                buttonDragItem.Drag.start()

                GridView.delayRemove = true
            } else {
                buttonDragItem.Drag.drop()

                dragStopped(mIndex)

                GridView.delayRemove = false
            }
        }

        onPositionChanged: {
            if (drag.active) {
                // FIXME: There must be a better way of this

                var pos = mapToItem(buttonDragItem.parent, mouseX, mouseY)
                // y should be set first, because the automatic scroll is
                // triggered by change on X
                buttonDragItem.y = pos.y
                buttonDragItem.x = pos.x
            }
        }

        Rectangle {
            anchors.fill: parent

            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height

            color: "transparent"

            border.width: VLCStyle.dp(1, VLCStyle.scale)
            border.color: containsMouse && !buttonDragItem.Drag.active ? theme.border
                                                                       : "transparent"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10

                EditorDummyButton {
                    Layout.preferredWidth: VLCStyle.icon_medium
                    Layout.preferredHeight: VLCStyle.icon_medium
                    Layout.alignment: Qt.AlignHCenter

                    color: theme.fg.primary
                    text: PlayerControlbarControls.controlList[model.index].label
                }

                Widgets.ListSubtitleLabel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    color: theme.fg.secondary
                    elide: Text.ElideNone
                    text: PlayerControlbarControls.controlList[model.index].text
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }
}
