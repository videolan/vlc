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
import QtQuick.Layouts 1.3

import org.videolan.vlc 0.1

import "qrc:///style/"
import "qrc:///widgets/" as Widgets

Popup {
    id: root

    property VLCColors colors: VLCStyle.nightColors

    height: implicitHeight
    width: implicitWidth
    padding: VLCStyle.margin_small

    // Popup.CloseOnPressOutside doesn't work with non-model Popup on Qt < 5.15
    closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape
    modal: true
    Overlay.modal: Rectangle {
       color: "transparent"
    }

    background: Rectangle {
        color: colors.bg
        opacity: .85
    }

    contentItem: ColumnLayout {
        spacing: VLCStyle.margin_xsmall

        Widgets.ListLabel {
            text: i18n.qtr("Playback Speed")
            color: root.colors.text
            font.pixelSize: VLCStyle.fontSize_large

            Layout.fillWidth: true
            Layout.bottomMargin: VLCStyle.margin_xsmall
        }

        Slider {
            id: speedSlider

            property bool _inhibitUpdate: false

            from: 0.25
            to: 4
            clip: true
            implicitHeight: VLCStyle.heightBar_small
            KeyNavigation.down: slowerButton

            background: Rectangle {
                x: speedSlider.leftPadding
                y: speedSlider.topPadding + speedSlider.availableHeight / 2 - height / 2
                implicitWidth: 200
                implicitHeight: 4
                width: speedSlider.availableWidth
                height: implicitHeight
                radius: 2
                color: root.colors.bgAlt

                Rectangle {
                    width: speedSlider.visualPosition * parent.width
                    height: parent.height
                    radius: 2
                    color: (speedSlider.activeFocus || speedSlider.pressed)
                           ? root.colors.accent
                           : root.colors.text
                }
            }

            handle: Rectangle {
                x: speedSlider.leftPadding + speedSlider.visualPosition * (speedSlider.availableWidth - width)
                y: speedSlider.topPadding + speedSlider.availableHeight / 2 - height / 2
                width: speedSlider.implicitHeight
                height: speedSlider.implicitHeight
                radius: speedSlider.implicitHeight
                color: (speedSlider.activeFocus || speedSlider.pressed) ? root.colors.accent : root.colors.text
            }

            onValueChanged:  {
                if (_inhibitUpdate)
                    return
                player.rate = value
            }

            function _updateFromPlayer() {
                _inhibitUpdate = true
                value = player.rate
                _inhibitUpdate = false
            }

            Connections {
                target: player
                onRateChanged: speedSlider._updateFromPlayer()
            }

            Layout.fillWidth: true

            Component.onCompleted: speedSlider._updateFromPlayer()
        }

        RowLayout {
            spacing: 0
            KeyNavigation.up: speedSlider

            Widgets.IconControlButton {
                id: slowerButton

                iconText: VLCIcons.slower
                colors: root.colors
                KeyNavigation.right: resetButton

                onClicked: speedSlider.decrease()
            }

            Item {
                Layout.fillWidth: true
            }

            Widgets.IconControlButton {
                id: resetButton

                colors: root.colors
                KeyNavigation.left: slowerButton
                KeyNavigation.right: fasterButton

                onClicked: speedSlider.value = 1.0

                Label {
                    anchors.centerIn: parent
                    font.pixelSize: VLCStyle.fontSize_normal
                    text: i18n.qtr("1x")
                    color: resetButton.background.foregroundColor // IconToolButton.background is a BackgroundHover
                }
            }

            Item {
                Layout.fillWidth: true
            }

            Widgets.IconControlButton {
                id: fasterButton

                iconText: VLCIcons.faster
                colors: root.colors
                KeyNavigation.left: resetButton

                onClicked: speedSlider.increase()
            }
        }
    }
}
