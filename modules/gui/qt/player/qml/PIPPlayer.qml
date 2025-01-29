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
    width: Math.round(VLCStyle.dp(320, VLCStyle.scale))
    height: Math.round(VLCStyle.dp(180, VLCStyle.scale))

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

            onDoubleTapped: MainCtx.requestShowPlayerView()
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

            Component.onCompleted: {
                if (typeof blocking === 'boolean')
                    blocking = true // Qt 6.3 feature
            }
        }
    }

    background: VideoSurface {
        id: videoSurface
        videoSurfaceProvider: MainCtx.videoSurfaceProvider
    }

    contentItem: Rectangle {
        color: "#10000000"
        visible: hoverHandler.hovered ||
                 playButton.hovered ||
                 closeButton.hovered ||
                 fullscreenButton.hovered

        // Raise the content item so that the handlers of the control do
        // not handle events that are to be handled by the handlers/items
        // of the content item. Raising the content item should be fine
        // because content item is supposed to be the foreground item.
        z: 1

        Widgets.IconButton {
            id: playButton

            anchors.centerIn: parent

            font.pixelSize: VLCStyle.icon_large

            description: qsTr("play/pause")
            text: (Player.playingState !== Player.PLAYING_STATE_PAUSED
                   && Player.playingState !== Player.PLAYING_STATE_STOPPED)
                  ? VLCIcons.pause_filled
                  : VLCIcons.play_filled

            onClicked: MainPlaylistController.togglePlayPause()
        }

        Widgets.IconButton {
            id: closeButton

            anchors {
                top: parent.top
                topMargin: VLCStyle.margin_small
                right: parent.right
                rightMargin: VLCStyle.margin_small
            }

            font.pixelSize: VLCStyle.icon_PIP
            description: qsTr("close video")
            text: VLCIcons.close

            onClicked: MainPlaylistController.stop()
        }

        Widgets.IconButton {
            id: fullscreenButton

            anchors {
                top: parent.top
                topMargin: VLCStyle.margin_small
                left: parent.left
                leftMargin: VLCStyle.margin_small
            }

            font.pixelSize: VLCStyle.icon_PIP

            description: qsTr("maximize player")
            text: VLCIcons.fullscreen

            onClicked: MainCtx.requestShowPlayerView()
        }
    }
}
