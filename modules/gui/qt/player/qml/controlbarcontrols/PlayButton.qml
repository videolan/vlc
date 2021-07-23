/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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
import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"
import "qrc:///util/Helpers.js" as Helpers


ToolButton {
    id: playBtn

    width: VLCStyle.icon_medium
    height: width

    scale: (playBtnMouseArea.pressed) ? 0.95 : 1.0

    property VLCColors colors: VLCStyle.colors

    property color color: colors.buttonPlayIcon

    property color colorDisabled: colors.textInactive

    property bool paintOnly: false

    property bool isCursorInside: false

    Keys.onPressed: {
        if (KeyHelper.matchOk(event) ) {
            if (!event.isAutoRepeat) {
                keyHoldTimer.restart()
            }
            event.accepted = true
        }
        Navigation.defaultKeyAction(event)
    }

    Keys.onReleased: {
        if (KeyHelper.matchOk(event)) {
            if (!event.isAutoRepeat) {
                keyHoldTimer.stop()
                if (player.playingState !== PlayerController.PLAYING_STATE_STOPPED)
                    mainPlaylistController.togglePlayPause()
            }
            event.accepted = true
        }
    }

    function _pressAndHoldAction() {
        mainPlaylistController.stop()
    }

    Timer {
        id: keyHoldTimer

        interval: playBtnMouseArea.pressAndHoldInterval
        repeat: false

        Component.onCompleted: {
            triggered.connect(_pressAndHoldAction)
        }
    }

    states: [
        State {
            name: "hovered"
            when: interactionIndicator

            PropertyChanges {
                target: hoverShadow
                radius: VLCStyle.dp(24, VLCStyle.scale)
            }
        },
        State {
            name: "default"
            when: !interactionIndicator

            PropertyChanges {
                target: contentLabel
                color: enabled ? playBtn.color : playBtn.colorDisabled
            }

            PropertyChanges {
                target: hoverShadow
                radius: 0
            }
        }
    ]
    readonly property bool interactionIndicator: (playBtn.activeFocus || playBtn.isCursorInside || playBtn.highlighted)

    contentItem: Label {
        id: contentLabel

        text: (player.playingState !== PlayerController.PLAYING_STATE_PAUSED
               && player.playingState !== PlayerController.PLAYING_STATE_STOPPED)
              ? VLCIcons.pause
              : VLCIcons.play

        Behavior on color {
            ColorAnimation {
                duration: VLCStyle.ms75
                easing.type: Easing.InOutSine
            }
        }

        font.pixelSize: VLCIcons.pixelSize(VLCStyle.icon_normal)
        font.family: VLCIcons.fontFamily

        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
    }

    background: Item {
        Gradient {
            id: playBtnGradient
            GradientStop { position: 0.0; color: VLCStyle.colors.buttonPlayA }
            GradientStop { position: 1.0; color: VLCStyle.colors.buttonPlayB }
        }

        MouseArea {
            id: playBtnMouseArea

            anchors.fill: parent
            anchors.margins: VLCStyle.dp(1, VLCStyle.scale)

            hoverEnabled: true

            readonly property int radius: playBtnMouseArea.width / 2

            onPositionChanged: {
                if (Helpers.pointInRadius(
                      (playBtnMouseArea.width / 2) - playBtnMouseArea.mouseX,
                      (playBtnMouseArea.height / 2) - playBtnMouseArea.mouseY,
                      radius)) {
                    // cursor is inside of the round button
                    playBtn.isCursorInside = true
                }
                else {
                    // cursor is outside
                    playBtn.isCursorInside = false
                }
            }

            onHoveredChanged: {
                if (!playBtnMouseArea.containsMouse)
                    playBtn.isCursorInside = false
            }

            onClicked: {
                if (!playBtn.isCursorInside)
                    return

                mainPlaylistController.togglePlayPause()
            }

            onPressAndHold: {
                if (!playBtn.isCursorInside)
                    return

                _pressAndHoldAction()
            }
        }

        DropShadow {
            id: hoverShadow

            anchors.fill: parent

            visible: radius > 0
            samples: (radius * 2) + 1
            // opacity: 0.29 // it looks better without this
            color: "#FF610A"
            source: opacityMask
            antialiasing: true

            Behavior on radius {
                NumberAnimation {
                    duration: VLCStyle.ms75
                    easing.type: Easing.InOutSine
                }
            }
        }

        Rectangle {
            radius: (width * 0.5)
            anchors.fill: parent
            anchors.margins: VLCStyle.dp(1, VLCStyle.scale)

            color: VLCStyle.colors.white
        }

        Rectangle {
            id: outerRect
            anchors.fill: parent

            radius: (width * 0.5)
            gradient: playBtnGradient

            visible: false
        }

        Rectangle {
            id: innerRect
            anchors.fill: parent

            radius: (width * 0.5)
            border.width: VLCStyle.dp(2, VLCStyle.scale)

            color: "transparent"
            visible: false
        }

        OpacityMask {
            id: opacityMask
            anchors.fill: parent

            source: outerRect
            maskSource: innerRect

            antialiasing: true
        }
    }
}
