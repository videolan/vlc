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
import QtQuick.Layouts 1.11
import QtQuick.Templates 2.4 as T
import QtGraphicalEffects 1.0

import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

T.Pane {
    id: root

    property VLCColors colors: VLCStyle.colors
    property color color: colors.accent
    property bool paintOnly: false
    readonly property var _player: paintOnly ? ({ muted: false, volume: .5 }) : Player

    implicitWidth: Math.max(background ? background.implicitWidth : 0, contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(background ? background.implicitHeight : 0, contentHeight + topPadding + bottomPadding)

    contentWidth: volumeWidget.implicitWidth
    contentHeight: volumeWidget.implicitHeight

    Keys.priority: Keys.AfterItem
    Keys.onPressed: Navigation.defaultKeyAction(event)

    RowLayout {
        id: volumeWidget

        anchors.fill: parent
        spacing: 0

        Widgets.IconToolButton{
            id: volumeBtn

            focus: true
            paintOnly: root.paintOnly
            size: VLCStyle.icon_normal
            iconText:
                if( _player.muted )
                    VLCIcons.volume_muted
                else if ( _player.volume === 0 )
                    VLCIcons.volume_zero
                else if ( _player.volume < .33 )
                    VLCIcons.volume_low
                else if( _player.volume <= .66 )
                    VLCIcons.volume_medium
                else
                    VLCIcons.volume_high
            text: I18n.qtr("Mute")
            color: colors.buttonText
            colorHover: colors.buttonTextHover
            colorFocus: colors.bgFocus
            onClicked: Player.muted = !Player.muted

            Navigation.parentItem: root
            Navigation.rightItem: volControl
        }

        T.Slider {
            id: volControl

            implicitWidth: VLCStyle.dp(100, VLCStyle.scale)
            implicitHeight: Math.max(background ? background.implicitHeight : 0,
                                    (handle ? handle.implicitHeight : 0) + topPadding + bottomPadding)

            Layout.fillHeight: true
            Layout.margins: VLCStyle.dp(5, VLCStyle.scale)

            property bool _inhibitPlayerVolumeUpdate: false

            from: 0
            to: maxvolpos
            stepSize: 0.05
            opacity: _player.muted ? 0.5 : 1

            Accessible.name: I18n.qtr("Volume")

            Keys.onPressed: {
                if (KeyHelper.matchOk(event)) {
                    event.accepted = true
                } else {
                    Navigation.defaultKeyAction(event)
                }
            }

            Keys.onReleased: {
                if (KeyHelper.matchOk(event)) {
                    Player.muted = !Player.muted
                }
            }

            function _syncVolumeWithPlayer() {
                volControl._inhibitPlayerVolumeUpdate = true
                volControl.value = _player.volume
                volControl._inhibitPlayerVolumeUpdate = false
            }

            function _adjustPlayerVolume() {
                Player.muted = false
                Player.volume = volControl.value
            }

            Component.onCompleted: {
                root.paintOnlyChanged.connect(_syncVolumeWithPlayer)
                volControl._syncVolumeWithPlayer()
            }

            Connections {
                target: Player
                enabled: !paintOnly

                onVolumeChanged: volControl._syncVolumeWithPlayer()
            }

            Timer {
                // useful for keyboard volume alteration
                id: tooltipShower
                running: false
                repeat: false
                interval: 1000
            }

            Navigation.leftItem: volumeBtn
            Navigation.parentItem: root

            Keys.onUpPressed: {
                volControl.increase()
                tooltipShower.restart()
            }

            Keys.onDownPressed: {
                volControl.decrease()
                tooltipShower.restart()
            }

            Keys.priority: Keys.BeforeItem

            readonly property color sliderColor: (volControl.position > fullvolpos) ? colors.volmax : root.color
            readonly property int maxvol: 125
            readonly property real fullvolpos: 100 / maxvol
            readonly property real maxvolpos: maxvol / 100

            onValueChanged: {
                if (paintOnly)
                    return

                if (!volControl._inhibitPlayerVolumeUpdate) {
                    Qt.callLater(volControl._adjustPlayerVolume)
                }
            }

            Loader {
                id: volumeTooltip
                active: !paintOnly

                sourceComponent: Widgets.PointingTooltip {
                    visible: tooltipShower.running || sliderMouseArea.pressed || sliderMouseArea.containsMouse

                    text: Math.round(volControl.value * 100) + "%"

                    pos: Qt.point(handle.x + handle.width / 2, handle.y)

                    colors: root.colors
                }
            }

            background: Rectangle {
                id: sliderBg
                x: volControl.leftPadding
                y: volControl.topPadding + volControl.availableHeight / 2 - height / 2
                implicitWidth: parent.width
                implicitHeight: VLCStyle.dp(4, VLCStyle.scale)
                height: implicitHeight
                width: volControl.availableWidth
                radius: VLCStyle.dp(4, VLCStyle.scale)
                color: colors.volsliderbg

                Rectangle {
                    id: filled
                    width: volControl.visualPosition * sliderBg.width
                    height: parent.height
                    radius: VLCStyle.dp(4, VLCStyle.scale)
                    color: root.color
                }
                Rectangle{
                    id: tickmark
                    x : parent.width * volControl.fullvolpos
                    width: VLCStyle.dp(1, VLCStyle.scale)
                    height: parent.height
                    radius: VLCStyle.dp(2, VLCStyle.scale)
                    color: root.color
                }
            }

            handle: Rectangle {
                id: handle
                x: volControl.leftPadding + volControl.visualPosition * (volControl.availableWidth - width)
                y: volControl.topPadding + volControl.availableHeight / 2 - height / 2

                implicitWidth: VLCStyle.dp(8, VLCStyle.scale)
                implicitHeight: implicitWidth
                radius: width * 0.5
                visible: (paintOnly || volControl.hovered || volControl.activeFocus)
                color: volControl.sliderColor
            }

            MouseArea {
                id: sliderMouseArea
                anchors.fill: parent

                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton

                Component.onCompleted: {
                    positionChanged.connect(adjustVolume)
                    onPressed.connect(adjustVolume)
                }

                function adjustVolume(mouse) {
                    if (pressedButtons === Qt.LeftButton) {
                        // The slider itself can handle this,
                        // but then the top&bottom margins need to be
                        // set there instead of here. Also, if handled
                        // there stepSize will be respected.
                        volControl.value = volControl.maxvolpos * (mouse.x - handle.width)
                                                                / (sliderBg.width - handle.width)

                        mouse.accepted = true
                    } else if (pressedButtons === Qt.RightButton) {
                        var pos = mouse.x * volControl.maxvolpos / width
                        if (pos < 0.25)
                            volControl.value = 0
                        else if (pos < 0.75)
                            volControl.value = 0.5
                        else if (pos < 1.125)
                            volControl.value = 1
                        else
                            volControl.value = 1.25

                        mouse.accepted = true
                    }
                }

                onPressed: {
                    if (!volControl.activeFocus)
                        volControl.forceActiveFocus(Qt.MouseFocusReason)
                }

                onWheel: {
                    var x = wheel.angleDelta.x
                    var y = wheel.angleDelta.y

                    if (x > 0 || y > 0) {
                        volControl.increase()
                        wheel.accepted = true
                    } else if (x < 0 || y < 0) {
                        volControl.decrease()
                        wheel.accepted = true
                    } else {
                        wheel.accepted = false
                    }
                }
            }
        }
    }
}
