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
import QtQuick.Layouts 1.11
import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"
import "qrc:///util/Helpers.js" as Helpers

Slider {
    id: control

    property int barHeight: VLCStyle.dp(5, VLCStyle.scale)
    property bool _isSeekPointsShown: true
    property real _tooltipPosition: timeTooltip.pos.x / sliderRectMouseArea.width

    property alias backgroundColor: sliderRect.color
    property alias progressBarColor: progressRect.color

    property VLCColors colors: VLCStyle.colors

    Keys.onRightPressed: Player.jumpFwd()
    Keys.onLeftPressed: Player.jumpBwd()

    function showChapterMarks() {
        _isSeekPointsShown = true
        seekpointTimer.restart()
    }

    Timer {
        id: seekpointTimer
        running: Player.hasChapters && !control.hovered && _isSeekPointsShown
        interval: 3000
        onTriggered: control._isSeekPointsShown = false
    }

    Widgets.PointingTooltip {
        id: timeTooltip

        visible: control.hovered

        text: Player.length.scale(pos.x / control.width).toString() +
              (Player.hasChapters ?
                   " - " + Player.chapters.getNameAtPosition(control._tooltipPosition) : "")

        pos: Qt.point(sliderRectMouseArea.mouseX, 0)

        colors: control.colors
    }

    Item {
        id: fsm

        signal playerUpdatePosition(real position)
        signal pressControl(real position, bool forcePrecise)
        signal releaseControl(real position, bool forcePrecise)
        signal moveControl(real position, bool forcePrecise)

        property var _state: fsmReleased

        function _changeState(state) {
            _state.enabled = false
            _state = state
            _state.enabled = true
        }

        function _seekToPosition(position, threshold, forcePrecise) {
            position = Helpers.clamp(position, 0., 1.)
            control.value = position
            if (!forcePrecise) {
                var chapter = Player.chapters.getClosestChapterFromPos(position, threshold)
                if (chapter !== -1) {
                    Player.chapters.selectChapter(chapter)
                    return
                }
            }
            Player.position = position
        }

        Item {
            id: fsmReleased
            enabled: true

            Connections {
                enabled: fsmReleased.enabled
                target: fsm

                onPlayerUpdatePosition: control.value = position

                onPressControl: {
                    control.forceActiveFocus()
                    fsm._seekToPosition(position, VLCStyle.dp(4) / control.width, forcePrecise)
                    fsm._changeState(fsmHeld)
                }
            }
        }

        Item {
            id: fsmHeld
            enabled: false

            Connections {
                enabled: fsmHeld.enabled
                target: fsm

                onMoveControl: fsm._seekToPosition(position, VLCStyle.dp(2) / control.width, forcePrecise)

                onReleaseControl: fsm._changeState(fsmReleased)
            }
        }
    }

    Connections {
        target: Player
        onPositionChanged: fsm.playerUpdatePosition(Player.position)
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
        color: control.colors.setColorAlpha( control.colors.playerFg, 0.2 )
        radius: implicitHeight

        MouseArea {
            id: sliderRectMouseArea

            anchors.fill: parent

            hoverEnabled: true

            onPressed: fsm.pressControl(mouse.x / control.width, mouse.modifiers === Qt.ShiftModifier)

            onReleased: fsm.releaseControl(mouse.x / control.width, mouse.modifiers === Qt.ShiftModifier)

            onPositionChanged: fsm.moveControl(mouse.x / control.width, mouse.modifiers === Qt.ShiftModifier)

            onEntered: {
                if(Player.hasChapters)
                    control._isSeekPointsShown = true
            }
            onExited: {
                if(Player.hasChapters)
                    seekpointTimer.restart()
            }
        }

        Rectangle {
            id: progressRect
            width: control.visualPosition * parent.width
            height: control.barHeight
            color: control.colors.accent
            radius: control.barHeight
        }

        Rectangle {
            id: bufferRect
            property int bufferAnimWidth: VLCStyle.dp(100, VLCStyle.scale)
            property int bufferAnimPosition: 0
            property int bufferFrames: 1000
            property alias animateLoading: loadingAnim.running

            height: control.barHeight
            opacity: 0.4
            color: control.colors.buffer
            radius: control.barHeight

            states: [
                State {
                    name: "hidden"
                    when: !control.visible
                    PropertyChanges {
                        target: bufferRect
                        width: bufferAnimWidth
                        visible: false
                        x: 0
                        animateLoading: false
                    }
                },
                State {
                    name: "buffering not started"
                    when: control.visible && Player.buffering === 0
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
                    when: control.visible && Player.buffering < 1
                    PropertyChanges {
                        target: bufferRect
                        width: Player.buffering * parent.width
                        visible: true
                        x: 0
                        animateLoading: false
                    }
                },
                State {
                    name: "playing from buffer"
                    when: control.visible && Player.buffering === 1
                    PropertyChanges {
                        target: bufferRect
                        width: Player.buffering * parent.width
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
                    duration: VLCStyle.ms2000
                    easing.type: "OutBounce"
                }
                PauseAnimation {
                    duration: VLCStyle.ms500
                }
                PropertyAnimation {
                    from: bufferRect.bufferFrames
                    to: 0.0
                    duration: VLCStyle.ms2000
                    easing.type: "OutBounce"
                }
                PauseAnimation {
                    duration: VLCStyle.ms500
                }
            }
        }

        Item {
            id: seekpointsRow

            width: parent.width
            height: control.barHeight
            visible: Player.hasChapters

            Repeater {
                id: seekpointsRptr
                model: Player.chapters
                Rectangle {
                    id: seekpointsRect
                    property real position: model.position === undefined ? 0.0 : model.position

                    color: control.colors.seekpoint
                    width: VLCStyle.dp(1, VLCStyle.scale)
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
        color: control.colors.accent

        transitions: [
            Transition {
                to: "hidden"
                SequentialAnimation {
                    NumberAnimation {
                        target: sliderHandle; properties: "implicitWidth,implicitHeight"

                        to: 0

                        duration: VLCStyle.duration_fast; easing.type: Easing.OutSine
                    }

                    PropertyAction { target: sliderHandle; property: "visible"; value: false; }
                }
            },
            Transition {
                to: "visible"
                SequentialAnimation {
                    PropertyAction { target: sliderHandle; property: "visible"; value: true; }

                    NumberAnimation {
                        target: sliderHandle; properties: "implicitWidth,implicitHeight"

                        to: VLCStyle.margin_small

                        duration: VLCStyle.duration_fast; easing.type: Easing.InSine
                    }
                }
            }
        ]

        state: (control.hovered || control.activeFocus) ? "visible" : "hidden"
    }
}
