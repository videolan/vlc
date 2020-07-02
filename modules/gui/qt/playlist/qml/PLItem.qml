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

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"


Rectangle {
    id: root

    property var plmodel

    signal itemClicked(int button, int modifier)
    signal itemDoubleClicked(int keys, int modifier)
    signal dragStarting()

    property alias hovered: mouse.containsMouse

    property var dragitem: null
    signal dropedMovedAt(int target, var drop)

    property int leftPadding: 0
    property int rightPadding: 0


    // Should the cover be displayed
    //property alias showCover: cover.visible

    // This item will become the parent of the dragged item during the drag operation
    //property alias draggedItemParent: draggable_item.draggedItemParent

    height: Math.max( VLCStyle.fontHeight_normal, VLCStyle.icon_normal ) + VLCStyle.margin_xxsmall

    property bool dropVisible: false


    Rectangle {
        width: parent.width
        height: 2
        anchors.top: parent.top
        antialiasing: true
        visible: dropVisible
        color: VLCStyle.colors.accent
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true

        acceptedButtons: acceptedButtons | Qt.RightButton

        onClicked:{
            root.itemClicked(mouse.button, mouse.modifiers);
        }
        onDoubleClicked: {
            if (mouse.button !== Qt.RightButton)
                root.itemDoubleClicked(mouse.buttons, mouse.modifiers);
        }

        drag.target: dragItem

        Connections {
            target: mouse.drag
            onActiveChanged: {
                if (mouse.button === Qt.RightButton)
                    return
                if (target.active) {
                    root.dragStarting()
                    dragItem.count = plmodel.getSelection().length
                    dragItem.visible = true
                } else {
                    dragItem.Drag.drop()
                    dragItem.visible = false
                }
            }
        }

        onPressed:  {
            if (mouse.button === Qt.RightButton)
                return
            var pos = this.mapToGlobal( mouseX, mouseY)
            dragItem.updatePos(pos.x, pos.y)
        }

        RowLayout {
            id: content
            anchors {
                fill: parent
                leftMargin: root.leftPadding
                rightMargin: root.rightPadding
            }

            Item {
                Layout.preferredHeight: VLCStyle.icon_normal
                Layout.preferredWidth: VLCStyle.icon_normal
                Layout.leftMargin: VLCStyle.margin_xsmall

                Image {
                    id: artwork
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectFit
                    source: (model.artwork && model.artwork.toString()) ? model.artwork : VLCStyle.noArtCover
                    visible: !statusIcon.visible
                }

                Image {
                    id: statusIcon
                    anchors.centerIn: parent
                    visible: (model.isCurrent && source !== "")
                    width: VLCStyle.play_cover_small
                    height: VLCStyle.play_cover_small
                    source: player.playingState === PlayerController.PLAYING_STATE_PLAYING ? "qrc:///toolbar/play_b.svg" :
                                                        player.playingState === PlayerController.PLAYING_STATE_PAUSED ? "qrc:///toolbar/pause_b.svg" : ""
                }
            }

            Column {
                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.margin_small

                Widgets.ListLabel {
                    id: textInfo

                    font.weight: model.isCurrent ? Font.Bold : Font.Normal
                    font.pixelSize: VLCStyle.fontSize_normal
                    elide: Text.ElideRight

                    text: model.title
                    color: VLCStyle.colors.text
                }

                Widgets.CaptionLabel {
                    id: textArtist

                    font.weight: model.isCurrent ? Font.DemiBold : Font.Normal
                    text: (model.artist ? model.artist : i18n.qtr("Unknown Artist"))
                }
            }

            Text {
                id: textDuration

                Layout.rightMargin: VLCStyle.margin_xsmall
                font.pixelSize: VLCStyle.fontSize_normal

                text: model.duration
                color: VLCStyle.colors.text
            }

        }

        DropArea {
            anchors { fill: parent }
            onEntered: {
                dropVisible = true
                return true
            }
            onExited: dropVisible = false
            onDropped: {
                root.dropedMovedAt(model.index, drop)
                dropVisible = false
            }
        }
    }
}
