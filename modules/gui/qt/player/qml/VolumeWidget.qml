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
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.4
import QtGraphicalEffects 1.0

import "qrc:///widgets/" as Widgets
import "qrc:///style/"
import "qrc:///util/KeyHelper.js" as KeyHelper

FocusScope{
    id: widgetfscope
    x: volumeWidget.x
    y: volumeWidget.y
    width: volumeWidget.width
    height: volumeWidget.height
    property bool paintOnly: true
    enabled: !paintOnly

    property bool acceptFocus: true
    Component.onCompleted: paintOnly = false

    property color color: VLCStyle.colors.buttonText

    RowLayout{
        id: volumeWidget
        Widgets.IconToolButton{
            id: volumeBtn
            paintOnly: widgetfscope.paintOnly
            size: VLCStyle.icon_normal
            iconText:
                if( player.muted )
                    VLCIcons.volume_muted
                else if ( player.volume < .33 )
                    VLCIcons.volume_low
                else if( player.volume <= .66 )
                    VLCIcons.volume_medium
                else
                    VLCIcons.volume_high
            text: i18n.qtr("Mute")
            color: widgetfscope.color
            onClicked: player.muted = !player.muted
            KeyNavigation.right: volControl
        }

        Slider
        {
            id: volControl
            width: VLCStyle.dp(100)
            height: parent.height

            anchors.margins: VLCStyle.dp(5)
            from: 0
            to: maxvolpos
            stepSize: 0.05
            value: player.volume
            opacity: player.muted ? 0.5 : 1
            focus: true

            Accessible.name: i18n.qtr("Volume")

            Keys.onReleased: {
                if (event.accepted)
                    return;
                if (KeyHelper.matchOk(event)) {
                    player.muted = !player.muted
                    event.accepted = true
                }
            }

            Keys.onUpPressed: volControl.increase()
            Keys.onDownPressed: volControl.decrease()
            Keys.onRightPressed: {
                var right = widgetfscope.KeyNavigation.right
                while (right && (!right.enabled || !right.visible)) {
                    right = right.KeyNavigation ? right.KeyNavigation.left : undefined
                }
                if (right)
                    right.forceActiveFocus()
            }
            Keys.onLeftPressed: {
                var left = widgetfscope.KeyNavigation.left
                while (left && (!left.enabled || !left.visible)) {
                    left = left.KeyNavigation ? left.KeyNavigation.left : undefined
                }
                if (left)
                    left.forceActiveFocus()
            }

            property color sliderColor: (volControl.position > fullvolpos) ? VLCStyle.colors.volmax : widgetfscope.color
            property int maxvol: 125
            property double fullvolpos: 100 / maxvol
            property double maxvolpos: maxvol / 100

            onValueChanged: {
                if (!paintOnly && player.muted) player.muted = false
                player.volume = volControl.value
            }

            background: Rectangle {
                id: sliderBg
                x: volControl.leftPadding
                y: volControl.topPadding + volControl.availableHeight / 2 - height / 2
                implicitWidth: parent.width
                implicitHeight: VLCStyle.dp(4)
                height: implicitHeight
                width: volControl.availableWidth
                radius: VLCStyle.dp(4)
                color: VLCStyle.colors.volsliderbg

                MouseArea {
                    id: sliderMouseArea
                    property bool isEntered: false

                    width: parent.width
                    height: parent.height + VLCStyle.dp(60)
                    anchors.verticalCenter: parent.verticalCenter
                    hoverEnabled: true

                    onPressed: function (event) {
                        volControl.forceActiveFocus()
                        volControl.value = volControl.maxvolpos * event.x / (volControl.width)
                    }
                    onPositionChanged: function (event) {
                        if(pressed)
                            volControl.value = volControl.maxvolpos * event.x / (volControl.width)
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
                    radius: VLCStyle.dp(4)
                    color: widgetfscope.color
                    layer.enabled: (volControl.hovered || volControl.activeFocus)
                    layer.effect: LinearGradient {
                        start: Qt.point(0, 0)
                        end: Qt.point(sliderBg.width, 0)
                        gradient: Gradient {
                            GradientStop { position: 0.30; color: VLCStyle.colors.volbelowmid }
                            GradientStop { position: 0.80; color: VLCStyle.colors.volabovemid }
                            GradientStop { position: 0.85; color: VLCStyle.colors.volhigh }
                            GradientStop { position: 1.00; color: VLCStyle.colors.volmax }
                        }
                    }
                }
                Rectangle{
                    id: tickmark
                    x : parent.width * volControl.fullvolpos
                    width: VLCStyle.dp(1)
                    height: parent.height
                    radius: VLCStyle.dp(2)
                    color: widgetfscope.color
                }
            }

            handle: Rectangle {
                x: volControl.leftPadding + volControl.visualPosition * (volControl.availableWidth - width)
                y: volControl.topPadding + volControl.availableHeight / 2 - height / 2

                implicitWidth: VLCStyle.dp(8)
                implicitHeight: implicitWidth
                radius: width * 0.5
                visible: (volControl.hovered || volControl.activeFocus)
                color: volControl.sliderColor
            }
        }
    }
}
