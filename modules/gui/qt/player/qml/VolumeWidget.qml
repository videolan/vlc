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
import QtQuick.Controls 2.4
import QtGraphicalEffects 1.0
import org.videolan.vlc 0.1

import "qrc:///widgets/" as Widgets
import "qrc:///style/"

FocusScope{
    id: widgetfscope
    x: volumeWidget.x
    y: volumeWidget.y
    width: volumeWidget.width
    height: volumeWidget.height
    property bool paintOnly: true
    enabled: !paintOnly

    Component.onCompleted: paintOnly = false

    property color color: colors.buttonText

    property alias parentWindow: volumeTooltip.parentWindow

    property VLCColors colors: VLCStyle.colors

    RowLayout{
        id: volumeWidget
        Widgets.IconToolButton{
            id: volumeBtn

            focus: true
            paintOnly: widgetfscope.paintOnly
            size: VLCStyle.icon_normal
            iconText:
                if( player.muted )
                    VLCIcons.volume_muted
                else if ( player.volume == 0 )
                    VLCIcons.volume_zero
                else if ( player.volume < .33 )
                    VLCIcons.volume_low
                else if( player.volume <= .66 )
                    VLCIcons.volume_medium
                else
                    VLCIcons.volume_high
            text: i18n.qtr("Mute")
            color: widgetfscope.color
            colorHover: colors.buttonTextHover
            colorFocus: colors.bgFocus
            onClicked: player.muted = !player.muted

            Navigation.parentItem: widgetfscope
            Navigation.rightItem: volControl
        }

        Slider {
            id: volControl

            property bool _inhibitPlayerVolumeUpdate: false

            width: VLCStyle.dp(100, VLCStyle.scale)
            height: parent.height

            anchors.margins: VLCStyle.dp(5, VLCStyle.scale)
            from: 0
            to: maxvolpos
            stepSize: 0.05
            opacity: player.muted ? 0.5 : 1

            Accessible.name: i18n.qtr("Volume")

            Keys.onPressed: {
                if (KeyHelper.matchOk(event)) {
                    event.accepted = true
                } else {
                    Navigation.defaultKeyAction(event)
                }
            }

            Keys.onReleased: {
                if (KeyHelper.matchOk(event)) {
                    player.muted = !player.muted
                }
            }

            function _syncVolumeWithPlayer() {
                if (paintOnly)
                    return

                volControl._inhibitPlayerVolumeUpdate = true
                volControl.value = player.volume
                volControl._inhibitPlayerVolumeUpdate = false
            }

            Component.onCompleted: volControl._syncVolumeWithPlayer()

            Connections {
                target: player

                onVolumeChanged: volControl._syncVolumeWithPlayer()
            }

            Timer {
                // useful for keyboard volume alteration
                id: tooltipShower
                running: false
                repeat: false
                interval: 1000

                onRunningChanged: {
                    if (running)
                        volumeTooltip.visible = true
                    else
                        volumeTooltip.visible = Qt.binding(function() {return sliderMouseArea.containsMouse;})
                }
            }

            Navigation.leftItem: volumeBtn
            Navigation.parentItem: widgetfscope

            Keys.onUpPressed: {
                volControl.increase()
                tooltipShower.restart()
            }

            Keys.onDownPressed: {
                volControl.decrease()
                tooltipShower.restart()
            }

            Keys.priority: Keys.BeforeItem

            property color sliderColor: (volControl.position > fullvolpos) ? colors.volmax : widgetfscope.color
            property int maxvol: 125
            property double fullvolpos: 100 / maxvol
            property double maxvolpos: maxvol / 100

            onValueChanged: {
                if (paintOnly)
                    return

                if (player.muted)
                    player.muted = false

                if (!volControl._inhibitPlayerVolumeUpdate)
                    player.volume = volControl.value
            }

            Widgets.PointingTooltip {
                id: volumeTooltip

                visible: sliderMouseArea.containsMouse

                text: Math.round(volControl.value * 100) + "%"

                mouseArea: sliderMouseArea

                xPos: (handle.x + handle.width / 2)

                colors: widgetfscope.colors
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

                MouseArea {
                    id: sliderMouseArea
                    property bool isEntered: false

                    width: parent.width
                    height: parent.height + VLCStyle.dp(60, VLCStyle.scale)
                    anchors.verticalCenter: parent.verticalCenter
                    hoverEnabled: true

                    acceptedButtons: Qt.LeftButton | Qt.RightButton

                    onPressed: function (event) {
                        volControl.forceActiveFocus()

                        positionChanged(event)
                    }

                    onPositionChanged: function (event) {
                        if (sliderMouseArea.pressedButtons === Qt.RightButton) {
                            var pos = sliderMouseArea.mouseX * volControl.maxvolpos / sliderMouseArea.width
                            if (pos < 0.25)
                                volControl.value = 0
                            else if (pos < 0.75)
                                volControl.value = 0.5
                            else if (pos < 1.125)
                                volControl.value = 1
                            else
                                volControl.value = 1.25
                            return
                        }

                        if(pressed)
                            volControl.value = volControl.maxvolpos * (event.x - handle.width / 2) / (sliderMouseArea.width - handle.width)
                    }

                    onWheel: {
                        if(wheel.angleDelta.y > 0)
                            volControl.increase()
                        else
                            volControl.decrease()
                    }
                }

                Rectangle {
                    id: filled
                    width: volControl.visualPosition * sliderBg.width
                    height: parent.height
                    radius: VLCStyle.dp(4, VLCStyle.scale)
                    color: widgetfscope.color
                    layer.enabled: (volControl.hovered || volControl.activeFocus)
                    layer.effect: LinearGradient {
                        start: Qt.point(0, 0)
                        end: Qt.point(sliderBg.width, 0)
                        gradient: Gradient {
                            GradientStop { position: 0.30; color: colors.volbelowmid }
                            GradientStop { position: 0.80; color: colors.volabovemid }
                            GradientStop { position: 0.85; color: colors.volhigh }
                            GradientStop { position: 1.00; color: colors.volmax }
                        }
                    }
                }
                Rectangle{
                    id: tickmark
                    x : parent.width * volControl.fullvolpos
                    width: VLCStyle.dp(1, VLCStyle.scale)
                    height: parent.height
                    radius: VLCStyle.dp(2, VLCStyle.scale)
                    color: widgetfscope.color
                }
            }

            handle: Rectangle {
                id: handle
                x: volControl.leftPadding + volControl.visualPosition * (volControl.availableWidth - width)
                y: volControl.topPadding + volControl.availableHeight / 2 - height / 2

                implicitWidth: VLCStyle.dp(8, VLCStyle.scale)
                implicitHeight: implicitWidth
                radius: width * 0.5
                visible: (volControl.hovered || volControl.activeFocus)
                color: volControl.sliderColor
            }
        }
    }
}
