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
import QtGraphicalEffects 1.0

import "qrc:///style/"

Slider {
    id: control

    property int barHeight: isMiniplayer ? VLCStyle.dp(3) : VLCStyle.dp(5)
    property bool _isHold: false
    property bool _isSeekPointsShown: true
    property bool isMiniplayer: false

    anchors.margins: isMiniplayer ? 0 : VLCStyle.margin_xxsmall

    Keys.onRightPressed: player.jumpFwd()
    Keys.onLeftPressed: player.jumpBwd()

    Timer {
        id: seekpointTimer
        running: player.hasChapters && !control.hovered && _isSeekPointsShown
        interval: 3000
        onTriggered: control._isSeekPointsShown = false
    }

    Item {
        id: timeTooltip
        property real location: sliderRectMouseArea.mouseX
        property real position: location/control.width

        width: childrenRect.width
        height: childrenRect.height

        function getX() {
            var x = location - (timeTooltip.width / 2)
            var diff = (x + timeTooltip.width) - (VLCStyle.appWidth)
            var sliderRealX = mainInterfaceRect.mapFromItem(sliderRectMouseArea, sliderRectMouseArea.x, sliderRectMouseArea.y).x

            if (x < -sliderRealX) {
                arrow.diff = x
                x = -sliderRealX
            }
            else if (diff > 0) {
                arrow.diff = diff
                x -= (diff)
            }
            else {
                arrow.diff = 0
            }

            return x
        }

        y: -(childrenRect.height)
        x: getX()
        visible: control.hovered

        Item {
            height: childrenRect.height * Math.sqrt(2)
            width: timeIndicatorRect.width

            anchors.horizontalCenter: timeIndicatorRect.horizontalCenter
            anchors.verticalCenter: timeIndicatorRect.bottom
            anchors.verticalCenterOffset: height / 2

            clip: true

            Rectangle {
                id: arrow
                width: VLCStyle.dp(10)
                height: VLCStyle.dp(10)

                anchors.centerIn: parent
                anchors.verticalCenterOffset: -(parent.height / 2)
                anchors.horizontalCenterOffset: diff

                property int diff: 0

                color: VLCStyle.colors.bgAlt

                rotation: 45

                RectangularGlow {
                    anchors.fill: parent
                    glowRadius: VLCStyle.dp(2)
                    spread: 0.2
                    color: VLCStyle.colors.glowColor
                }
            }
        }

        Rectangle {
            id: timeIndicatorRect
            width: timeMetrics.width + VLCStyle.dp(10)
            height: timeMetrics.height + VLCStyle.dp(5)

            color: VLCStyle.colors.bgAlt
            radius: VLCStyle.dp(6)

            RectangularGlow {
                anchors.fill: parent

                glowRadius: VLCStyle.dp(2)
                cornerRadius: parent.radius
                spread: 0.2

                color: VLCStyle.colors.glowColor
            }

            Text {
                anchors.fill: parent
                text: timeMetrics.text
                color: VLCStyle.colors.text

                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter

                TextMetrics {
                    id: timeMetrics
                    text: player.length.scale(timeTooltip.position).toString() +
                          (player.hasChapters ?
                               " - " + player.chapters.getNameAtPosition(timeTooltip.position) : "")
                }
            }
        }
    }

    Connections {    
        /* only update the control position when the player position actually change, this avoid the slider
         * to jump around when clicking
         */
        target: player
        enabled: !_isHold
        onPositionChanged: control.value = player.position
    }

    height: control.barHeight
    implicitHeight: control.barHeight

    topPadding: 0
    leftPadding: 0
    bottomPadding: 0
    rightPadding: 0

    stepSize: 0.01

    background: Rectangle {
        id: sliderRect
        width: control.availableWidth
        implicitHeight: control.implicitHeight
        height: implicitHeight
        color:  isMiniplayer ? (VLCStyle.colors.sliderBarMiniplayerBgColor) : VLCStyle.colors.setColorAlpha( VLCStyle.colors.playerFg, 0.7 )
        radius: implicitHeight

        MouseArea {
            id: sliderRectMouseArea
            property bool isEntered: false

            anchors.fill: parent
            hoverEnabled: true

            onPressed: function (event) {
                control.forceActiveFocus()
                control._isHold = true
                control.value = event.x / control.width
                player.position = control.value
            }
            onReleased: control._isHold = false
            onPositionChanged: function (event) {
                if (pressed && (event.x <= control.width)) {
                    control.value = event.x / control.width
                    player.position = control.value
                }
            }
            onEntered: {
                if(player.hasChapters)
                    control._isSeekPointsShown = true
            }
            onExited: {
                if(player.hasChapters)
                    seekpointTimer.restart()
            }
        }

        Rectangle {
            id: progressRect
            width: control.visualPosition * parent.width
            height: control.barHeight
            color: (control.activeFocus || control.isMiniplayer) ? VLCStyle.colors.accent : VLCStyle.colors.bgHover
            radius: control.barHeight
        }

        Rectangle {
            id: bufferRect
            property int bufferAnimWidth: VLCStyle.dp(100)
            property int bufferAnimPosition: 0
            property int bufferFrames: 1000
            property alias animateLoading: loadingAnim.running

            height: control.barHeight
            opacity: 0.4
            color: VLCStyle.colors.buffer
            radius: control.barHeight

            states: [
                State {
                    name: "buffering not started"
                    when: player.buffering === 0
                    PropertyChanges {
                        target: bufferRect
                        width: bufferAnimWidth
                        visible: true
                        x: (bufferAnimPosition / bufferFrames) * (parent.width - bufferAnimWidth)
                        animateLoading: true
                    }
                },
                State {
                    name: "time to start playing known"
                    when: player.buffering < 1
                    PropertyChanges {
                        target: bufferRect
                        width: player.buffering * parent.width
                        visible: true
                        x: 0
                        animateLoading: false
                    }
                },
                State {
                    name: "playing from buffer"
                    when: player.buffering === 1
                    PropertyChanges {
                        target: bufferRect
                        width: player.buffering * parent.width
                        visible: false
                        x: 0
                        animateLoading: false
                    }
                }
            ]

            SequentialAnimation on bufferAnimPosition {
                id: loadingAnim
                running: bufferRect.animateLoading
                loops: Animation.Infinite
                PropertyAnimation {
                    from: 0.0
                    to: bufferRect.bufferFrames
                    duration: 2000
                    easing.type: "OutBounce"
                }
                PauseAnimation {
                    duration: 500
                }
                PropertyAnimation {
                    from: bufferRect.bufferFrames
                    to: 0.0
                    duration: 2000
                    easing.type: "OutBounce"
                }
                PauseAnimation {
                    duration: 500
                }
            }
        }

        RowLayout {
            id: seekpointsRow
            spacing: 0
            visible: player.hasChapters
            Repeater {
                id: seekpointsRptr
                model: player.chapters
                Rectangle {
                    id: seekpointsRect
                    property real position: model.position === undefined ? 0.0 : model.position

                    color: VLCStyle.colors.seekpoint
                    width: VLCStyle.dp(1)
                    height: control.barHeight
                    x: sliderRect.width * seekpointsRect.position
                }
            }

            OpacityAnimator on opacity {
                from: 1
                to: 0
                running: !control._isSeekPointsShown
            }
            OpacityAnimator on opacity{
                from: 0
                to: 1
                running: control._isSeekPointsShown
            }
        }
    }

    handle: Rectangle {
        id: sliderHandle

        visible: control.activeFocus
        x: (control.visualPosition * control.availableWidth) - width / 2
        y: (control.barHeight - width) / 2
        implicitWidth: VLCStyle.margin_small
        implicitHeight: VLCStyle.margin_small
        radius: VLCStyle.margin_small
        color: VLCStyle.colors.accent

        transitions: [
            Transition {
                to: "hidden"
                SequentialAnimation {
                    NumberAnimation { target: sliderHandle; properties: "implicitWidth,implicitHeight"; to: 0; duration: 150; easing.type: Easing.OutSine}
                    PropertyAction { target: sliderHandle; property: "visible"; value: false; }
                }
            },
            Transition {
                to: "visible"
                SequentialAnimation {
                    PropertyAction { target: sliderHandle; property: "visible"; value: true; }
                    NumberAnimation { target: sliderHandle; properties: "implicitWidth,implicitHeight"; to: VLCStyle.margin_small; duration: 150; easing.type: Easing.InSine}
                }
            }
        ]

        state: isMiniplayer ? (control.hovered ? "visible" : "hidden") : undefined
    }
}
