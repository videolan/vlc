/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets
import VLC.Playlist
import VLC.Player
import VLC.Util

T.Control {
    id: root
    width: VLCStyle.dp(320, VLCStyle.scale)
    height: VLCStyle.dp(180, VLCStyle.scale)

    //VideoSurface x,y won't update
    onXChanged: videoSurface.updateSurfacePosition()
    onYChanged: videoSurface.updateSurfacePosition()

    objectName: "pip window"

    property real dragXMin: 0
    property real dragXMax: 0
    property real dragYMin: undefined
    property real dragYMax: undefined

    Accessible.role: Accessible.Graphic
    Accessible.focusable: false
    Accessible.name: qsTr("video content")

    Drag.active: dragHandler.active

    Drag.onActiveChanged: {
        root.anchors.left = undefined
        root.anchors.right = undefined
        root.anchors.top = undefined
        root.anchors.bottom = undefined
        root.anchors.verticalCenter = undefined
        root.anchors.horizontalCenter = undefined
    }

    DoubleClickIgnoringItem {
        anchors.fill: parent

        TapHandler {
            gesturePolicy: TapHandler.WithinBounds

            onDoubleTapped: History.push(["player"])
            onTapped: MainPlaylistController.togglePlayPause()
        }

        DragHandler {
            id: dragHandler

            target: root

            cursorShape: Qt.DragMoveCursor

            dragThreshold: 0

            grabPermissions: PointerHandler.CanTakeOverFromAnything

            xAxis.minimum: root.dragXMin
            xAxis.maximum: root.dragXMax
            yAxis.minimum: root.dragYMin
            yAxis.maximum: root.dragYMax
        }

        HoverHandler {
            id: hoverHandler

            grabPermissions: PointerHandler.CanTakeOverFromAnything
            cursorShape: Qt.ArrowCursor
        }
    }

    background: VideoSurface {
        id: videoSurface
        videoSurfaceProvider: MainCtx.videoSurfaceProvider
    }

    contentItem: Rectangle {
        color: "#10000000"
        visible: hoverHandler.hovered

        Widgets.IconButton {
            anchors.centerIn: parent

            font.pixelSize: VLCStyle.icon_large

            description: qsTr("play/pause")
            text: (Player.playingState !== Player.PLAYING_STATE_PAUSED
                   && Player.playingState !== Player.PLAYING_STATE_STOPPED)
                  ? VLCIcons.pause_filled
                  : VLCIcons.play_filled

            hoverEnabled: MainCtx.qtQuickControlRejectsHoverEvents()

            onClicked: MainPlaylistController.togglePlayPause()
        }

        Widgets.IconButton {
            anchors {
                top: parent.top
                topMargin: VLCStyle.margin_small
                right: parent.right
                rightMargin: VLCStyle.margin_small
            }

            font.pixelSize: VLCStyle.icon_PIP
            description: qsTr("close video")
            text: VLCIcons.close

            hoverEnabled: MainCtx.qtQuickControlRejectsHoverEvents()

            onClicked: MainPlaylistController.stop()
        }

        Widgets.IconButton {
            anchors {
                top: parent.top
                topMargin: VLCStyle.margin_small
                left: parent.left
                leftMargin: VLCStyle.margin_small
            }

            font.pixelSize: VLCStyle.icon_PIP

            description: qsTr("maximize player")
            text: VLCIcons.fullscreen

            hoverEnabled: MainCtx.qtQuickControlRejectsHoverEvents()

            onClicked: History.push(["player"])
        }
    }
}
