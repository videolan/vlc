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

import QtQuick
import QtQuick.Templates as T


import VLC.MainInterface
import VLC.Style
import VLC.Player
import VLC.Widgets
import VLC.Util

PopupIconToolButton {
    id: root

    popup.width: VLCStyle.dp(256, VLCStyle.scale)

    text: qsTr("Playback Speed")

    description: qsTr("change playback speed")

    popup.contentItem: PlaybackSpeed {
        colorContext.palette: root.colorContext.palette

        Navigation.parentItem: root

        // NOTE: Mapping the right direction because the down action triggers the ComboBox.
        Navigation.rightItem: root

        onRadioButtonClicked: {
            root.popup.close()
        }
    }

    contentItem: Item {
        implicitHeight: label.implicitHeight
        implicitWidth: textMetrics.width

        TextMetrics {
            id: textMetrics
            font: label.font
            // '-' is for additional space if certain digits have more width with certain fonts
            text: "0.00x-"
        }

        Text {
            id: label

            anchors.fill: parent

            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter

            font.pixelSize: VLCStyle.fontSize_normal

            text: !root.paintOnly ? qsTr("%1x").arg(+Player.rate.toFixed(2))
                                  : qsTr("1x")

            color: root.color
        }
    }

    // TODO: Qt bug 6.2: QTBUG-103604
    DoubleClickIgnoringItem {
        anchors.fill: parent

        z: -1

        WheelToVLCConverter {
            id: wheelToVLC

            function handle(steps: int) {
                let func
                if (steps > 0)
                    func = root.popup.contentItem.slider.increase
                else
                    func = root.popup.contentItem.slider.decrease

                for (let i = 0; i < Math.abs(steps); ++i)
                    func()
            }

            Component.onCompleted: {
                wheelUpDown.connect(wheelToVLC.handle)
                wheelLeftRight.connect(wheelToVLC.handle)
            }
        }

        WheelHandler {
            orientation: Qt.Vertical | Qt.Horizontal

            acceptedDevices: PointerDevice.AllDevices

            onWheel: (wheel) => {
                if (!root.popup.contentItem || !root.popup.contentItem.slider) {
                    event.accepted = false
                    return
                }

                // NOTE: Uncomment below to support (not only handle) high precision adjustment:
                // if ((Math.abs(wheel.pixelDelta.x) % 120 > 0) || (Math.abs(wheel.pixelDelta.y) % 120 > 0)) {
                //     let delta = 0
                //     if (Math.abs(wheel.pixelDelta.x) > Math.abs(wheel.pixelDelta.y))
                //         delta = wheel.pixelDelta.x
                //     else
                //         delta = wheel.pixelDelta.y

                //     if (wheel.inverted)
                //         delta = -delta

                //     root.popup.contentItem.slider.value += 0.01 * delta
                // } else
                {
                    wheelToVLC.qmlWheelEvent(wheel)
                }

                wheel.accepted = true
            }
        }

        TapHandler {
            acceptedButtons: Qt.RightButton

            enabled: root.popup.contentItem && root.popup.contentItem.slider

            gesturePolicy: TapHandler.ReleaseWithinBounds

            onTapped: (eventPoint, button) => {
                root.popup.contentItem.slider.value = 0
            }
        }
    }
}
